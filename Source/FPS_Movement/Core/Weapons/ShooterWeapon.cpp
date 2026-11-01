// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterWeapon.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "ShooterCharacter.h"
#include "ShooterDamageUtils.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "FPS_Movement.h"
#include "CarnageWeaponFXSubsystem.h"
#include "CarnageWeaponFXSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AShooterWeapon::AShooterWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);

	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;

	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);

	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;

	HitscanDamageType = UDamageType::StaticClass();
}

void AShooterWeapon::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		UE_LOG(LogFPS_Movement, Error, TEXT("ShooterWeapon [%s] spawned without an owner — deactivating."), *GetName());
		return;
	}

	WeaponOwner = Cast<IShooterWeaponHolder>(OwnerActor);
	if (!WeaponOwner)
	{
		UE_LOG(LogFPS_Movement, Error, TEXT("ShooterWeapon [%s] owner [%s] does not implement IShooterWeaponHolder."), *GetName(), *OwnerActor->GetName());
		return;
	}

	OwnerActor->OnDestroyed.AddDynamic(this, &AShooterWeapon::OnOwnerDestroyed);

	PawnOwner = Cast<APawn>(OwnerActor);

	CurrentBullets = MagazineSize;

	WeaponOwner->AttachWeaponMeshes(this);
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefireTimer);
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnDestroyed.RemoveDynamic(this, &AShooterWeapon::OnOwnerDestroyed);
	}
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void AShooterWeapon::ActivateWeapon()
{
	bShotBuffered = false;

	SetActorHiddenInGame(false);

	if (WeaponOwner)
	{
		WeaponOwner->OnWeaponActivated(this);
		WeaponOwner->UpdateReserveHUD(ReserveAmmo);
	}
}

void AShooterWeapon::DeactivateWeapon()
{
	StopFiring();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefireTimer);
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}
	bShotBuffered = false;
	bIsReloading = false;

	SetActorHiddenInGame(true);

	if (WeaponOwner)
	{
		WeaponOwner->OnWeaponDeactivated(this);
	}
}

void AShooterWeapon::StartFiring()
{
	if (bIsReloading)
	{
		return;
	}

	if (CurrentBullets <= 0)
	{
		if (bAutoReloadOnEmpty)
		{
			StartReload();
		}
		return;
	}

	bIsFiring = true;

	const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;

	if (TimeSinceLastShot >= RefireRate)
	{
		Fire();
	}
	else
	{
		const float RemainingCooldown = RefireRate - TimeSinceLastShot;

		if (bFullAuto)
		{
			bShotBuffered = true;
			GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RemainingCooldown, false);
		}
		else if (RemainingCooldown <= ShotBufferWindow)
		{
			bShotBuffered = true;
		}
	}
}

void AShooterWeapon::StopFiring()
{
	bIsFiring = false;

	if (bFullAuto)
	{
		GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	}
}

bool AShooterWeapon::CanReload() const
{
	return !bIsReloading && CurrentBullets < MagazineSize && ReserveAmmo > 0;
}

void AShooterWeapon::StartReload()
{
	if (!CanReload() || !WeaponOwner)
	{
		return;
	}

	bIsReloading = true;
	StopFiring();
	bShotBuffered = false;

	WeaponOwner->PlayReloadMontage(ReloadMontage);

	if (WeaponReloadMontage && FirstPersonMesh)
	{
		if (UAnimInstance* WeaponAnim = FirstPersonMesh->GetAnimInstance())
		{
			WeaponAnim->Montage_Play(WeaponReloadMontage, 1.0f);
		}
	}

	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &AShooterWeapon::FinishReload, ReloadDuration, false);
}

void AShooterWeapon::FinishReload()
{
	bIsReloading = false;

	const int32 Needed = MagazineSize - CurrentBullets;
	const int32 Taken = FMath::Min(Needed, ReserveAmmo);
	CurrentBullets += Taken;
	ReserveAmmo -= Taken;

	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
		WeaponOwner->UpdateReserveHUD(ReserveAmmo);
	}
}

void AShooterWeapon::Fire()
{
	if (!bIsFiring && !bShotBuffered)
	{
		return;
	}
	bShotBuffered = false;

	FireShot();
}

void AShooterWeapon::FireCooldownExpired()
{
	if (bShotBuffered)
	{
		bShotBuffered = false;
		FireShot();
		return;
	}

	if (WeaponOwner)
	{
		WeaponOwner->OnSemiWeaponRefire();
	}
}

void AShooterWeapon::FireShot()
{
	if (!WeaponOwner)
	{
		return;
	}

	const FVector TargetLocation = WeaponOwner->GetWeaponTargetLocation();

	// unspread aim direction; the flash points this way regardless of how the muzzle socket is oriented
	FVector ShotStart, ShotAimDir;
	GetShotRay(TargetLocation, ShotStart, ShotAimDir);

	switch (FireMode)
	{
	case EShooterWeaponFireMode::Hitscan:
		FireHitscan(TargetLocation);
		break;

	case EShooterWeaponFireMode::Projectile:
	default:
		FireProjectile(TargetLocation);
		break;
	}

	// ── shared per-shot feedback ────────────────────────────────────────

	UAnimMontage* ArmsMontage = FiringMontage;
	if (FiringMontage_ADS)
	{
		const AShooterCharacter* OwnerChar = Cast<AShooterCharacter>(GetOwner());
		if (OwnerChar && OwnerChar->IsAimingDownSights())
		{
			ArmsMontage = FiringMontage_ADS;
		}
	}
	WeaponOwner->PlayFiringMontage(ArmsMontage);

	if (WeaponFiringMontage && FirstPersonMesh)
	{
		if (UAnimInstance* WeaponAnim = FirstPersonMesh->GetAnimInstance())
		{
			WeaponAnim->Montage_Play(WeaponFiringMontage, 1.0f);
		}
	}

	PlayMuzzleFX(ShotAimDir);
	PlayFireSound();

	WeaponOwner->AddWeaponRecoil(RecoilParams);

	BP_OnShotFired();

	--CurrentBullets;
	WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);

	TimeOfLastShot = GetWorld()->GetTimeSeconds();

	if (PawnOwner.IsValid())
	{
		MakeNoise(ShotLoudness, PawnOwner.Get(), PawnOwner->GetActorLocation(), ShotNoiseRange, ShotNoiseTag);
	}

	if (CurrentBullets <= 0)
	{
		StopFiring();
		if (bAutoReloadOnEmpty)
		{
			StartReload();
		}
		return;
	}

	if (bFullAuto)
	{
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);
	}
}

void AShooterWeapon::FireProjectile(const FVector& TargetLocation)
{
	FTransform ProjectileTransform = CalculateProjectileSpawnTransform(TargetLocation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = PawnOwner.Get();

	AShooterProjectile* Projectile = GetWorld()->SpawnActor<AShooterProjectile>(ProjectileClass, ProjectileTransform, SpawnParams);

	if (Projectile && bUseProceduralFX)
	{
		if (UCarnageWeaponFXSubsystem* FX = UCarnageWeaponFXSubsystem::Get(this))
		{
			FX->PlayProjectileTrail(Projectile, GetVisibleMuzzleLocation());
		}
	}
}

void AShooterWeapon::FireHitscan(const FVector& TargetLocation)
{
	// Damage is traced from the camera for players (what you see is what you hit) and from the muzzle for NPCs.
	FVector TraceStart, AimDir;
	GetShotRay(TargetLocation, TraceStart, AimDir);

	if (AimVariance > 0.0f)
	{
		AimDir = FMath::VRandCone(AimDir, FMath::DegreesToRadians(AimVariance));
	}

	const FVector TraceEnd = TraceStart + AimDir * HitscanRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterHitscan), false);
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	const FVector BeamEnd = bHit ? Hit.ImpactPoint : TraceEnd;

	if (bHit && Hit.GetActor())
	{
		FShooterHitInfo HitInfo;
		HitInfo.Target = Hit.GetActor();
		HitInfo.HitComponent = Hit.GetComponent();
		HitInfo.HitLocation = Hit.ImpactPoint;
		HitInfo.HitDirection = AimDir;
		HitInfo.BoneName = Hit.BoneName;
		HitInfo.BaseDamage = HitscanDamage;
		HitInfo.PhysicsImpulse = HitscanImpulse;
		HitInfo.KnockbackForce = HitscanKnockback;
		HitInfo.InstigatorController = PawnOwner.IsValid() ? PawnOwner->GetController() : nullptr;
		HitInfo.InstigatorPawn = PawnOwner.Get();
		HitInfo.DamageCauser = this;
		HitInfo.DamageType = HitscanDamageType;
		HitInfo.bAllowHeadshot = bAllowHeadshots;
		HitInfo.HeadshotMultiplier = HeadshotMultiplier;
		HitInfo.HeadshotHeightFraction = HeadshotHeightFraction;

		ShooterDamage::ApplyHit(HitInfo);
	}

	// FX start where the barrel visibly is, not at the un-morphed first-person socket
	const FVector VisibleMuzzle = GetVisibleMuzzleLocation();
	PlayTracerFX(VisibleMuzzle, BeamEnd);
	if (bHit)
	{
		PlayImpactFX(Hit, AimDir);
	}

	BP_OnHitscanFired(VisibleMuzzle, BeamEnd, Hit);
}

// ─────────────────────────────────────────────────────────────────────────────
// FX helpers

APlayerController* AShooterWeapon::GetLocalViewer() const
{
	APlayerController* PC = PawnOwner.IsValid() ? Cast<APlayerController>(PawnOwner->GetController()) : nullptr;
	return (PC && PC->IsLocalController() && PC->PlayerCameraManager) ? PC : nullptr;
}

void AShooterWeapon::GetShotRay(const FVector& TargetLocation, FVector& OutStart, FVector& OutDirection) const
{
	if (const APlayerController* PC = GetLocalViewer())
	{
		// Trace from the camera COMPONENT along the control rotation (where the gun points), not
		// from the camera manager's final POV. Camera modifiers (bodycam lag, chest mount, view
		// punch) move the rendered view without moving the aim.
		const FRotator AimRot = PC->GetControlRotation();
		const AShooterCharacter* OwnerChar = Cast<AShooterCharacter>(PawnOwner.Get());
		const UCameraComponent* AimCamera = OwnerChar ? OwnerChar->GetFirstPersonCameraComponent() : nullptr;
		OutStart = AimCamera ? AimCamera->GetComponentLocation() : PC->PlayerCameraManager->GetCameraLocation();
		OutDirection = (TargetLocation - OutStart).GetSafeNormal(UE_SMALL_NUMBER, AimRot.Vector());
		return;
	}

	OutStart = FirstPersonMesh->GetSocketLocation(MuzzleSocketName);
	OutDirection = (TargetLocation - OutStart).GetSafeNormal(UE_SMALL_NUMBER, GetActorForwardVector());
}

USceneComponent* AShooterWeapon::GetFXMuzzleComponent(bool& bOutFirstPerson) const
{
	if (GetLocalViewer() && FirstPersonMesh)
	{
		bOutFirstPerson = true;
		return FirstPersonMesh;
	}

	bOutFirstPerson = false;
	if (ThirdPersonMesh && ThirdPersonMesh->DoesSocketExist(MuzzleSocketName))
	{
		return ThirdPersonMesh;
	}
	return FirstPersonMesh;
}

FVector AShooterWeapon::GetVisibleMuzzleLocation() const
{
	bool bFirstPerson = false;
	const USceneComponent* Muzzle = GetFXMuzzleComponent(bFirstPerson);
	if (!Muzzle)
	{
		return GetActorLocation();
	}

	const FVector SocketLoc = Muzzle->GetSocketLocation(MuzzleSocketName);
	return bFirstPerson ? UCarnageWeaponFXSubsystem::GetVisibleFirstPersonLocation(GetLocalViewer(), SocketLoc) : SocketLoc;
}

void AShooterWeapon::PlayMuzzleFX(const FVector& AimDirection)
{
	bool bFirstPerson = false;
	USceneComponent* Muzzle = GetFXMuzzleComponent(bFirstPerson);
	if (!Muzzle)
	{
		return;
	}
	APlayerController* Viewer = GetLocalViewer();

	if (bUseNiagaraOverrides && MuzzleFlashSystem)
	{
		FRotator RelRot = MuzzleFlashRotationOffset;
		if (bAlignMuzzleFlashToAim)
		{
			// express the world aim direction in socket space so the system's +X points down range
			const FTransform SocketT = Muzzle->GetSocketTransform(MuzzleSocketName, RTS_World);
			RelRot = SocketT.InverseTransformVectorNoScale(AimDirection).Rotation();
		}

		if (UNiagaraComponent* Flash = UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlashSystem, Muzzle, MuzzleSocketName,
			FVector::ZeroVector, RelRot, EAttachLocation::SnapToTarget, true))
		{
			// first-person morph is per primitive and not inherited from the parent mesh
			Flash->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
		}
	}
	else if (bUseProceduralFX)
	{
		if (UCarnageWeaponFXSubsystem* FX = UCarnageWeaponFXSubsystem::Get(this))
		{
			// Shrink the flash while aiming so it reads as a flash past the sights, not a wall of light.
			const AShooterCharacter* OwnerChar = Cast<AShooterCharacter>(GetOwner());
			const float ADSAlpha = OwnerChar ? OwnerChar->GetADSAlpha() : 0.0f;
			FX->PlayMuzzleFlash(Muzzle, MuzzleSocketName, AimDirection, bFirstPerson, Viewer, FMath::Lerp(1.0f, ADSMuzzleFlashScale, ADSAlpha));
		}
	}

	if (bUseProceduralFX)
	{
		if (UCarnageWeaponFXSubsystem* FX = UCarnageWeaponFXSubsystem::Get(this))
		{
			FVector Forward = AimDirection;
			FVector Right, Up;
			if (Viewer)
			{
				const FRotator CamRot = Viewer->PlayerCameraManager->GetCameraRotation();
				Right = FRotationMatrix(CamRot).GetUnitAxis(EAxis::Y);
				Up = FRotationMatrix(CamRot).GetUnitAxis(EAxis::Z);
			}
			else
			{
				Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal(UE_SMALL_NUMBER, GetActorRightVector());
				Up = FVector::CrossProduct(Forward, Right);
			}
			const FVector InheritVelocity = PawnOwner.IsValid() ? PawnOwner->GetVelocity() : FVector::ZeroVector;
			FX->PlayShellEject(Muzzle, GetDefault<UCarnageWeaponFXSettings>()->Shell.SocketName, bFirstPerson, Right, Up, Forward, InheritVelocity);
		}
	}
}

void AShooterWeapon::PlayTracerFX(const FVector& VisibleStart, const FVector& End)
{
	if (bUseNiagaraOverrides && TracerSystem)
	{
		if (UNiagaraComponent* Tracer = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), TracerSystem, VisibleStart, (End - VisibleStart).Rotation()))
		{
			Tracer->SetVectorParameter(TracerEndParamName, End);
		}
		return;
	}

	if (bUseProceduralFX)
	{
		if (UCarnageWeaponFXSubsystem* FX = UCarnageWeaponFXSubsystem::Get(this))
		{
			FX->PlayTracer(VisibleStart, End);
		}
	}
}

void AShooterWeapon::PlayImpactFX(const FHitResult& Hit, const FVector& ShotDirection) const
{
	if (!bUseProceduralFX)
	{
		return;
	}

	UCarnageWeaponFXSubsystem* FX = UCarnageWeaponFXSubsystem::Get(this);
	if (!FX)
	{
		return;
	}

	FCarnageImpactFXParams Params;
	Params.Location = Hit.ImpactPoint;
	Params.Normal = Hit.ImpactNormal;
	Params.ShotDirection = ShotDirection;
	Params.bHitPawn = Cast<APawn>(Hit.GetActor()) != nullptr;
	Params.SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
	Params.HitComponent = Hit.GetComponent();
	FX->PlayImpact(Params);
}

void AShooterWeapon::PlayFireSound() const
{
	if (!FireSound)
	{
		return;
	}

	const float Pitch = 1.0f + FMath::FRandRange(-FireSoundPitchVariance, FireSoundPitchVariance);
	const float Volume = 1.0f + FMath::FRandRange(-FireSoundVolumeVariance, FireSoundVolumeVariance);
	UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation(), Volume, Pitch);
}

FTransform AShooterWeapon::CalculateProjectileSpawnTransform(const FVector& TargetLocation) const
{
	const FVector MuzzleLoc = FirstPersonMesh->GetSocketLocation(MuzzleSocketName);

	const FVector SpawnLoc = MuzzleLoc + ((TargetLocation - MuzzleLoc).GetSafeNormal() * MuzzleOffset);

	FVector AimDir = (TargetLocation - SpawnLoc).GetSafeNormal();
	if (AimVariance > 0.0f)
	{
		AimDir = FMath::VRandCone(AimDir, FMath::DegreesToRadians(AimVariance));
	}

	return FTransform(AimDir.Rotation(), SpawnLoc, FVector::OneVector);
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetFirstPersonAnimInstanceClass() const
{
	return FirstPersonAnimInstanceClass;
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetThirdPersonAnimInstanceClass() const
{
	return ThirdPersonAnimInstanceClass;
}
