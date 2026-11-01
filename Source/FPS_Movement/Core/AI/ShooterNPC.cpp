// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/AI/ShooterNPC.h"
#include "Core/AI/JuggleComponent.h"
#include "ShooterWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterGameMode.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "FPS_Movement.h"
#include "ShooterDamageUtils.h"
#include "Engine/DamageEvents.h"
#include "PhysicsEngine/BodyInstance.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarDrawNPCAim(
	TEXT("carnage.Debug.DrawNPCAim"),
	0,
	TEXT("1 = per NPC shot, draw the shot ray (red), the spread cone around the aim direction, the target's chest point (yellow) and the led aim point (green)."),
	ECVF_Cheat);
#endif

AShooterNPC::AShooterNPC()
{
	JuggleComp = CreateDefaultSubobject<UJuggleComponent>(TEXT("Juggle"));
}

void AShooterNPC::BeginPlay()
{
	Super::BeginPlay();

	if (JuggleComp)
	{
		JuggleComp->OnJuggleStateChanged.AddDynamic(this, &AShooterNPC::HandleJuggleStateChanged);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		BaseMeshLocation = MeshComp->GetRelativeLocation();
		BaseMeshRotation = MeshComp->GetRelativeRotation();
	}

	// Physics bodies on (QueryAndPhysics) so they can blend into simulation for flinches.
	bHasPhysicsHitBodies = bPerBoneWeaponHits && ShooterDamage::SetupPerBoneWeaponHits(this, true);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (!WeaponClass)
	{
		UE_LOG(LogFPS_Movement, Warning, TEXT("ShooterNPC [%s] has no WeaponClass set."), *GetName());
		return;
	}

	Weapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
}

void AShooterNPC::HandleJuggleStateChanged(bool bJuggled)
{
	if (bJuggled)
	{
		StopShooting();
	}
}

void AShooterNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(DeathTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReactionTimer);
	GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
}

float AShooterNPC::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.0f;
	}

	CurrentHP -= Damage;

	LastHit = FLastHitData();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		LastHit.bValid = true;
		LastHit.BoneName = PointEvent.HitInfo.BoneName;
		LastHit.Location = PointEvent.HitInfo.ImpactPoint;
		LastHit.Damage = Damage;

		FVector Dir = PointEvent.ShotDirection;
		if (Dir.IsNearlyZero() && DamageCauser)
		{
			Dir = GetActorLocation() - DamageCauser->GetActorLocation();
		}
		LastHit.Direction = Dir.GetSafeNormal(UE_SMALL_NUMBER, -GetActorForwardVector());

		if (DamageEvent.IsOfType(FShooterPointDamageEvent::ClassID))
		{
			const FShooterPointDamageEvent& ShooterEvent = static_cast<const FShooterPointDamageEvent&>(DamageEvent);
			LastHit.bHeadshot = ShooterEvent.bHeadshot;
			LastHit.ImpulseScale = FMath::Clamp(ShooterEvent.PhysicsImpulse / 800.0f, 0.25f, 3.0f);
		}
		else
		{
			LastHit.bHeadshot = LastHit.BoneName.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase);
		}
	}

	if (CurrentHP <= 0.0f)
	{
		Die();
	}
	else if (LastHit.bValid)
	{
		PlayHitReaction(LastHit);
	}

	return Damage;
}

FName AShooterNPC::ChooseFlinchBone(FName HitBone) const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return NAME_None;
	}

	// A chain root must hang off another body, so its joint keeps it attached to the animated
	// skeleton (and so we never simulate the component's root body, which would detach it).
	auto HasParentBody = [MeshComp](FName Bone)
	{
		for (FName Parent = MeshComp->GetParentBone(Bone); !Parent.IsNone(); Parent = MeshComp->GetParentBone(Parent))
		{
			if (MeshComp->GetBodyInstance(Parent))
			{
				return true;
			}
		}
		return false;
	};

	const bool bRootUsable = !FlinchRootBone.IsNone() && MeshComp->GetBodyInstance(FlinchRootBone) && HasParentBody(FlinchRootBone);

	if (!HitBone.IsNone() && MeshComp->GetBodyInstance(HitBone))
	{
		// upper body: flinch from the spine so torso, head and arms react together
		if (bRootUsable && (HitBone == FlinchRootBone || MeshComp->BoneIsChildOf(HitBone, FlinchRootBone)))
		{
			return FlinchRootBone;
		}
		// limbs outside the spine chain (legs): flinch just that limb
		if (HasParentBody(HitBone))
		{
			return HitBone;
		}
	}

	return bRootUsable ? FlinchRootBone : NAME_None;
}

void AShooterNPC::PlayHitReaction(const FLastHitData& HitData)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!MeshComp || !Move)
	{
		return;
	}

	EndHitReaction();

	// Juggle launches drive the capsule with LaunchCharacter; the physics blend only moves the
	// mesh, so it can't fight the launch, but keep it light so airborne combos stay readable.
	const bool bAirborne = Move->IsFalling() || (JuggleComp && JuggleComp->IsJuggled());
	const float Strength = (bAirborne ? AirborneFlinchScale : 1.0f) * (HitData.bHeadshot ? HeadshotFlinchMultiplier : 1.0f);

	ReactionStartTime = GetWorld()->GetTimeSeconds();

	const FName ChainBone = bHasPhysicsHitBodies ? ChooseFlinchBone(HitData.BoneName) : NAME_None;
	if (!ChainBone.IsNone())
	{
		ActiveFlinchBone = ChainBone;
		ActiveFlinchWeight = FMath::Clamp(FlinchBlendWeight * (bAirborne ? AirborneFlinchScale : 1.0f), 0.0f, 1.0f);

		MeshComp->SetAllBodiesBelowSimulatePhysics(ActiveFlinchBone, true, true);
		MeshComp->SetAllBodiesBelowPhysicsBlendWeight(ActiveFlinchBone, ActiveFlinchWeight, false, true);

		const bool bHitInChain = !HitData.BoneName.IsNone() && MeshComp->GetBodyInstance(HitData.BoneName)
			&& (HitData.BoneName == ActiveFlinchBone || MeshComp->BoneIsChildOf(HitData.BoneName, ActiveFlinchBone));
		const FName ImpulseBone = bHitInChain ? HitData.BoneName : ActiveFlinchBone;

		const float Speed = FMath::Min(HitData.Damage * FlinchSpeedPerDamage, MaxFlinchSpeed) * Strength;
		MeshComp->AddVelocityChangeImpulseAtLocation(HitData.Direction * Speed, HitData.Location, ImpulseBone);
	}
	else
	{
		// No physics asset: tilt and nudge the whole mesh away from the shot.
		bShakeActive = true;
		ShakeStrength = Strength;
		FVector LocalDir = GetCapsuleComponent()->GetComponentTransform().InverseTransformVectorNoScale(HitData.Direction);
		LocalDir.Z = 0.0f;
		ShakeLocalDir = LocalDir.GetSafeNormal(UE_SMALL_NUMBER, FVector::BackwardVector);
	}

	GetWorld()->GetTimerManager().SetTimer(ReactionTimer, this, &AShooterNPC::UpdateHitReaction, 1.0f / 60.0f, true);

	// Brief stagger: slow the NPC down while it is on the ground.
	if (!bAirborne && StaggerDuration > 0.0f)
	{
		if (!bStaggered)
		{
			PreStaggerWalkSpeed = Move->MaxWalkSpeed;
			bStaggered = true;
		}
		Move->MaxWalkSpeed = PreStaggerWalkSpeed * StaggerSpeedScale;
		GetWorld()->GetTimerManager().SetTimer(StaggerTimer, this, &AShooterNPC::EndStagger, StaggerDuration, false);
	}
}

void AShooterNPC::UpdateHitReaction()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	const float Elapsed = GetWorld()->GetTimeSeconds() - ReactionStartTime;
	const float Alpha = 1.0f - FMath::Clamp(Elapsed / FMath::Max(FlinchDuration, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);

	if (!MeshComp || Alpha <= 0.0f)
	{
		EndHitReaction();
		return;
	}

	if (!ActiveFlinchBone.IsNone())
	{
		MeshComp->SetAllBodiesBelowPhysicsBlendWeight(ActiveFlinchBone, ActiveFlinchWeight * Alpha, false, true);
	}

	if (bShakeActive)
	{
		const float Osc = FMath::Cos(Elapsed * FallbackShakeFrequency * UE_TWO_PI) * Alpha * ShakeStrength;
		// rotating Up about (Up x Dir) tips the top of the mesh toward Dir, i.e. away from the shooter
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, ShakeLocalDir).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		const FQuat Tilt(Axis, FMath::DegreesToRadians(FallbackShakeAngle * Osc));
		MeshComp->SetRelativeLocationAndRotation(BaseMeshLocation + ShakeLocalDir * (FallbackShakeOffset * Osc), Tilt * BaseMeshRotation.Quaternion());
	}
}

void AShooterNPC::EndHitReaction()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReactionTimer);
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	if (!ActiveFlinchBone.IsNone())
	{
		// back to fully animated, kinematic bodies
		MeshComp->SetAllBodiesBelowPhysicsBlendWeight(ActiveFlinchBone, 0.0f, false, true);
		MeshComp->SetAllBodiesBelowSimulatePhysics(ActiveFlinchBone, false, true);
		ActiveFlinchBone = NAME_None;
		ActiveFlinchWeight = 0.0f;
	}

	if (bShakeActive)
	{
		MeshComp->SetRelativeLocationAndRotation(BaseMeshLocation, BaseMeshRotation);
		bShakeActive = false;
	}
}

void AShooterNPC::EndStagger()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaggerTimer);
	}

	if (bStaggered)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MaxWalkSpeed = PreStaggerWalkSpeed;
		}
		bStaggered = false;
	}
}

void AShooterNPC::ApplyDeathImpulse(const FVector& CarriedVelocity)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->IsSimulatingPhysics())
	{
		return;
	}

	// keep the capsule's momentum (juggle kills keep flying), plus a push along the killing shot
	FVector BodyVelocity = CarriedVelocity;
	if (LastHit.bValid)
	{
		BodyVelocity += LastHit.Direction * DeathBodySpeed;
	}
	if (!BodyVelocity.IsNearlyZero())
	{
		MeshComp->SetAllPhysicsLinearVelocity(BodyVelocity, true);
	}

	if (!LastHit.bValid)
	{
		return;
	}

	FName KickBone = LastHit.BoneName;
	if (LastHit.bHeadshot && (KickBone.IsNone() || !KickBone.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase)))
	{
		// headshot resolved by height (capsule hit): kick the head body if there is one
		static const FName HeadBone(TEXT("head"));
		if (MeshComp->GetBodyInstance(HeadBone))
		{
			KickBone = HeadBone;
		}
	}

	if (KickBone.IsNone() || !MeshComp->GetBodyInstance(KickBone))
	{
		return;
	}

	const float Speed = DeathBoneSpeed * LastHit.ImpulseScale * (LastHit.bHeadshot ? HeadshotDeathMultiplier : 1.0f);
	MeshComp->AddVelocityChangeImpulseAtLocation(LastHit.Direction * Speed, LastHit.Location, KickBone);
}

void AShooterNPC::AttachWeaponMeshes(AShooterWeapon* WeaponToAttach)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	WeaponToAttach->AttachToActor(this, AttachmentRule);

	WeaponToAttach->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	WeaponToAttach->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonWeaponSocket);
}

void AShooterNPC::PlayFiringMontage(UAnimMontage* Montage)
{
}

void AShooterNPC::AddWeaponRecoil(const FWeaponRecoilParams& Recoil)
{
}

void AShooterNPC::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
}

FVector AShooterNPC::GetWeaponTargetLocation()
{
	const UCameraComponent* EyeCamera = GetFirstPersonCameraComponent();
	const FVector EyeLocation = EyeCamera->GetComponentLocation();

	const AActor* AimActor = CurrentAimTarget.Get();
	if (!AimActor)
	{
		// no target: fire roughly where the NPC is looking
		const FVector AimDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(EyeCamera->GetForwardVector(), AcquireSpreadHalfAngle);
		const FVector TraceEnd = EyeLocation + AimDir * AimRange;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterNPCAim), false, this);
		QueryParams.AddIgnoredActor(Weapon);

		FHitResult OutHit;
		GetWorld()->LineTraceSingleByChannel(OutHit, EyeLocation, TraceEnd, ECC_Weapon, QueryParams);
		return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
	}

	// Shots leave the visible muzzle, so aim from there: the returned point then lies exactly on the shot line.
	const FVector ShotOrigin = Weapon ? Weapon->GetMuzzleWorldLocation() : EyeLocation;
	const FVector AimPoint = GetPredictedAimPoint(AimActor, ShotOrigin);

	// line of sight from the eyes to the target itself (not the led point) drives the settle timer
	FCollisionQueryParams SightParams(SCENE_QUERY_STAT(ShooterNPCAimSight), false, this);
	SightParams.AddIgnoredActor(Weapon);
	SightParams.AddIgnoredActor(AimActor);
	const bool bHasLineOfSight = !GetWorld()->LineTraceTestByChannel(EyeLocation, AimActor->GetActorLocation(), ECC_Visibility, SightParams);

	const float SpreadHalfAngle = UpdateAimSpread(AimActor, bHasLineOfSight);

	const FVector ToAim = AimPoint - ShotOrigin;
	const float AimDistance = FMath::Max(ToAim.Size(), 100.0f);
	const FVector IdealDir = ToAim.GetSafeNormal(UE_SMALL_NUMBER, GetActorForwardVector());
	const FVector ShotDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(IdealDir, SpreadHalfAngle);
	const FVector ShotTarget = ShotOrigin + ShotDir * AimDistance;

#if !UE_BUILD_SHIPPING
	if (CVarDrawNPCAim.GetValueOnGameThread() != 0)
	{
		constexpr float DrawTime = 1.0f;
		const UWorld* World = GetWorld();
		const float ConeRad = FMath::DegreesToRadians(SpreadHalfAngle);
		DrawDebugLine(World, ShotOrigin, ShotOrigin + ShotDir * (AimDistance + 500.0f), FColor::Red, false, DrawTime, 0, 1.0f);
		DrawDebugCone(World, ShotOrigin, IdealDir, AimDistance, ConeRad, ConeRad, 16, FColor::Orange, false, DrawTime);
		DrawDebugSphere(World, AimPoint, 8.0f, 8, FColor::Green, false, DrawTime);

		float Radius = 0.0f, HalfHeight = 0.0f;
		AimActor->GetSimpleCollisionCylinder(Radius, HalfHeight);
		DrawDebugSphere(World, AimActor->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight * AimHeightFraction), 8.0f, 8, FColor::Yellow, false, DrawTime);
	}
#endif

	return ShotTarget;
}

float AShooterNPC::UpdateAimSpread(const AActor* AimActor, bool bHasLineOfSight)
{
	const float Now = GetWorld()->GetTimeSeconds();

	float SettleAlpha = 0.0f;
	if (!bHasLineOfSight)
	{
		// blind shot: widest spread, and the next clear shot starts settling from scratch
		SettleTarget.Reset();
		LastTrackedShotTime = -1.0f;
	}
	else
	{
		const bool bReacquired = SettleTarget.Get() != AimActor || LastTrackedShotTime < 0.0f || (Now - LastTrackedShotTime) > TrackingLostTime;
		if (bReacquired)
		{
			SettleTarget = AimActor;
			TrackingStartTime = Now;
		}
		LastTrackedShotTime = Now;

		SettleAlpha = SettleTime > 0.0f ? FMath::Clamp((Now - TrackingStartTime) / SettleTime, 0.0f, 1.0f) : 1.0f;
	}

	const float SettledSpread = FMath::Lerp(AcquireSpreadHalfAngle, BaseSpreadHalfAngle, SettleAlpha);

	const float TargetSpeed = AimActor->GetVelocity().Size();
	const float MovingSpread = FMath::Min(MovingSpreadHalfAngle * TargetSpeed / FMath::Max(MovingSpreadReferenceSpeed, 1.0f), MaxMovingSpreadHalfAngle);

	return SettledSpread + MovingSpread;
}

FVector AShooterNPC::GetPredictedAimPoint(const AActor* AimActor, const FVector& ShotOrigin) const
{
	// upper chest: capsule centre plus a fraction of the half height (crouching shrinks it with the capsule)
	float Radius = 0.0f, HalfHeight = 0.0f;
	AimActor->GetSimpleCollisionCylinder(Radius, HalfHeight);
	FVector AimPoint = AimActor->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight * AimHeightFraction);

	const float ProjectileSpeed = Weapon ? Weapon->GetProjectileLaunchSpeed() : 0.0f;
	if (ProjectileSpeed <= 0.0f || LeadAccuracy <= 0.0f)
	{
		// hitscan (or unknown projectile): no lead
		return AimPoint;
	}

	// flight time to the current point, refined once against the predicted point
	const FVector TargetVelocity = AimActor->GetVelocity();
	float FlightTime = FVector::Dist(ShotOrigin, AimPoint) / ProjectileSpeed;
	FlightTime = FVector::Dist(ShotOrigin, AimPoint + TargetVelocity * FlightTime) / ProjectileSpeed;
	FlightTime = FMath::Min(FlightTime, MaxLeadTime);

	AimPoint += TargetVelocity * (FlightTime * LeadAccuracy);
	return AimPoint;
}

bool AShooterNPC::AddWeaponClass(const TSubclassOf<AShooterWeapon>& InWeaponClass)
{
	return false;
}

void AShooterNPC::OnWeaponActivated(AShooterWeapon* InWeapon)
{
}

void AShooterNPC::OnWeaponDeactivated(AShooterWeapon* InWeapon)
{
}

void AShooterNPC::OnSemiWeaponRefire()
{
	if (bIsShooting && Weapon)
	{
		Weapon->StartFiring();
	}
}

void AShooterNPC::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	StopShooting();
	if (Weapon)
	{
		Weapon->DeactivateWeapon();
	}

	Tags.Add(DeathTag);

	OnPawnDeath.Broadcast();

	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->IncrementTeamScore(TeamByte);
	}

	// the ragdoll takes over the whole mesh; drop any running flinch / shake / stagger first
	const FVector CarriedVelocity = GetVelocity();
	EndHitReaction();
	EndStagger();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->StopActiveMovement();

	GetMesh()->SetCollisionProfileName(RagdollCollisionProfile);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetPhysicsBlendWeight(1.0f);

	ApplyDeathImpulse(CarriedVelocity);

	GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &AShooterNPC::DeferredDestruction, DeferredDestructionTime, false);
}

void AShooterNPC::DeferredDestruction()
{
	Destroy();
}

void AShooterNPC::StartShooting(AActor* ActorToShoot)
{
	if (JuggleComp && JuggleComp->IsJuggled())
	{
		return;
	}

	CurrentAimTarget = ActorToShoot;

	bIsShooting = true;

	if (Weapon)
	{
		Weapon->StartFiring();
	}
}

void AShooterNPC::StopShooting()
{
	bIsShooting = false;

	if (Weapon)
	{
		Weapon->StopFiring();
	}
}
