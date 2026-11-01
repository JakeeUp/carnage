// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "DoomMovementComponent.h"
#include "DoomCameraEffectsComponent.h"
#include "BodycamComponent.h"
#include "InputAction.h"
#include "WeaponRecoilComponent.h"
#include "StyleMeterComponent.h"
#include "CarnageAbilitySystemComponent.h"
#include "CarnageAttributeSet.h"
#include "CarnageGameplayTags.h"
#include "GE_InstantDamage.h"
#include "EnhancedInputComponent.h"
#include "Components/InputComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "ShooterGameMode.h"
#include "GameplayEffect.h"
#include "ViewSpringMath.h"
#include "HAL/IConsoleManager.h"
#include "FPS_Movement.h"
#include "ShooterDamageUtils.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarCarnageForceADS(
	TEXT("carnage.Debug.ForceADS"),
	0,
	TEXT("1 = the local shooter character behaves as if the ADS button is held (for screenshots / sight tuning)."),
	ECVF_Cheat);
#endif

AShooterCharacter::AShooterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDoomMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));
	CameraEffects = CreateDefaultSubobject<UDoomCameraEffectsComponent>(TEXT("Camera Effects"));
	RecoilComponent = CreateDefaultSubobject<UWeaponRecoilComponent>(TEXT("Recoil"));
	StyleMeter = CreateDefaultSubobject<UStyleMeterComponent>(TEXT("Style Meter"));
	Bodycam = CreateDefaultSubobject<UBodycamComponent>(TEXT("Bodycam"));

	ToggleBodycamAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_ToggleBodycam.IA_ToggleBodycam")));

	AbilitySystem = CreateDefaultSubobject<UCarnageAbilitySystemComponent>(TEXT("AbilitySystem"));
	AttributeSet = CreateDefaultSubobject<UCarnageAttributeSet>(TEXT("AttributeSet"));

	DefaultDamageEffect = UGE_InstantDamage::StaticClass();

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	JumpMaxCount = 2;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

UAbilitySystemComponent* AShooterCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AShooterCharacter::PlayCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	if (!ShakeClass) return;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientStartCameraShake(ShakeClass, Scale);
	}
}

void AShooterCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	PlayCameraShake(LandingCameraShake);
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	DoomMovement = Cast<UDoomMovementComponent>(GetCharacterMovement());

	// The third-person mesh is owner-hidden, so with the default AlwaysTickPose its bones (and
	// hit bodies) would never refresh. Query-only bodies: the player has no physics flinch.
	if (bPerBoneWeaponHits && ShooterDamage::SetupPerBoneWeaponHits(this, false))
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}

	if (AttributeSet)
	{
		AttributeSet->InitMaxHealth(MaxHP);
		AttributeSet->InitHealth(MaxHP);
		AttributeSet->InitMaxArmor(MaxArmor);
		AttributeSet->InitArmor(StartingArmor);

		AttributeSet->OnHealthChanged.AddDynamic(this, &AShooterCharacter::HandleHealthChanged);
		AttributeSet->OnDied.AddDynamic(this, &AShooterCharacter::HandleDied);
	}

	OnDamaged.Broadcast(1.0f);
}

void AShooterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateADS(DeltaSeconds);

	if (!AbilitySystem || !DoomMovement) return;

	SetLooseTag(CarnageTags::State_Sprinting,   DoomMovement->IsSprinting());
	SetLooseTag(CarnageTags::State_Sliding,     DoomMovement->IsSliding());
	SetLooseTag(CarnageTags::State_Dashing,     DoomMovement->IsDashing());
	SetLooseTag(CarnageTags::State_WallRunning, DoomMovement->IsWallRunning());
	SetLooseTag(CarnageTags::State_Mantling,    DoomMovement->IsMantling());
	SetLooseTag(CarnageTags::State_InAir,       DoomMovement->IsFalling());
}

void AShooterCharacter::SetLooseTag(const FGameplayTag& Tag, bool bShouldHave)
{
	if (!AbilitySystem) return;

	const bool bHas = AbilitySystem->HasMatchingGameplayTag(Tag);
	if (bShouldHave && !bHas)
	{
		AbilitySystem->AddLooseGameplayTag(Tag);
	}
	else if (!bShouldHave && bHas)
	{
		AbilitySystem->RemoveLooseGameplayTag(Tag);
	}
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);

	if (GetWorld()->GetTimerManager().IsTimerActive(HitStopTimer))
	{
		GetWorld()->GetTimerManager().ClearTimer(HitStopTimer);
		EndHitStop();
	}
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopMove);
		}

		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartFiring);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopFiring);

		EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &AShooterCharacter::DoSwitchWeapon);

		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopSprint);
		}

		if (SlideAction)
		{
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &AShooterCharacter::DoSlide);
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopSlide);
		}

		if (DashAction)
		{
			EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Triggered, this, &AShooterCharacter::DoDash);
		}

		if (ADSAction)
		{
			EnhancedInputComponent->BindAction(ADSAction, ETriggerEvent::Started,   this, &AShooterCharacter::DoStartADS);
			EnhancedInputComponent->BindAction(ADSAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopADS);
		}

		if (ReloadAction)
		{
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &AShooterCharacter::DoReload);
		}

		// Soft reference with a C++ default, so the character Blueprint doesn't need editing.
		if (const UInputAction* ToggleAction = ToggleBodycamAction.LoadSynchronous())
		{
			EnhancedInputComponent->BindAction(ToggleAction, ETriggerEvent::Started, this, &AShooterCharacter::DoToggleBodycam);
		}
	}

}

float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (IsDead() || Damage <= 0.0f || !AbilitySystem || !DefaultDamageEffect)
	{
		return 0.0f;
	}

	FGameplayTagContainer DamageTags;
	DamageTags.AddTag(CarnageTags::Damage_Physical);

	AbilitySystem->ApplyDamageToSelf(DefaultDamageEffect, Damage, DamageCauser, DamageTags);

	// Tell the HUD where the hit came from (the instigating pawn if known, else the causer).
	const AActor* Source = (EventInstigator && EventInstigator->GetPawn()) ? EventInstigator->GetPawn() : DamageCauser;
	if (Source && Source != this)
	{
		OnDamageSource.Broadcast(Source->GetActorLocation());
	}

	return Damage;
}

void AShooterCharacter::DoAim(float Yaw, float Pitch)
{
	if (!IsDead())
	{
		// Zoom-ratio sensitivity: keep the on-screen speed of the world roughly constant as the
		// FOV narrows. Uses the ADS part of the FOV only, so sprint/dash FOV kicks don't change aim feel.
		float LookScale = 1.0f;
		if (ADSAlpha > 0.0f)
		{
			if (bScaleLookByADSZoom && CameraEffects)
			{
				const float BaseFOV = FMath::Clamp(CameraEffects->BaseFOV, 5.0f, 170.0f);
				const float AimFOV = CurrentWeapon ? CurrentWeapon->GetADSWorldFOV() : CameraEffects->ADSFOV;
				const float CurFOV = FMath::Clamp(FMath::Lerp(BaseFOV, AimFOV, ADSAlpha), 5.0f, 170.0f);
				LookScale = FMath::Tan(FMath::DegreesToRadians(CurFOV) * 0.5f) / FMath::Tan(FMath::DegreesToRadians(BaseFOV) * 0.5f);
			}
			LookScale *= FMath::Lerp(1.0f, ADSLookSensitivityMultiplier, ADSAlpha);
		}

		Yaw *= LookScale;
		Pitch *= LookScale;

		if (RecoilComponent)
		{
			RecoilComponent->NotifyAimInput(Yaw, Pitch);
		}

		Super::DoAim(Yaw, Pitch);
	}
}

void AShooterCharacter::DoMove(float Right, float Forward)
{
	if (!IsDead())
	{
		LastMoveInput = FVector2D(Right, Forward);
		Super::DoMove(Right, Forward);
	}
}

void AShooterCharacter::DoJumpStart()
{
	if (!IsDead())
	{
		if (DoomMovement)
		{
			DoomMovement->SetWantsToHoldJump(true);
			DoomMovement->NotifyJumpInputPressed();
		}
		Super::DoJumpStart();
	}
}

void AShooterCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	if (JumpCurrentCount == 2 && JumpCurrentCountPreJump == 0
		&& DoomMovement && DoomMovement->IsWithinCoyoteTime())
	{
		JumpCurrentCount = 1;
	}
}

void AShooterCharacter::DoJumpEnd()
{
	if (DoomMovement)
	{
		DoomMovement->SetWantsToHoldJump(false);
	}

	if (!IsDead())
	{
		Super::DoJumpEnd();
	}
}

void AShooterCharacter::DoStartFiring()
{
	if (CurrentWeapon && !IsDead())
	{
		// Firing breaks sprint; if ADS is still held it takes over again.
		if (DoomMovement)
		{
			DoomMovement->StopSprint();
		}
		bADSSuppressedBySprint = false;
		RefreshAimState();

		CurrentWeapon->StartFiring();
	}
}

void AShooterCharacter::DoStopFiring()
{
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StopFiring();
	}
}

void AShooterCharacter::DoReload()
{
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StartReload();
	}
}

void AShooterCharacter::DoSwitchWeapon()
{
	if (OwnedWeapons.Num() > 1 && CurrentWeapon && !IsDead())
	{
		CurrentWeapon->DeactivateWeapon();

		int32 WeaponIndex = OwnedWeapons.Find(CurrentWeapon);

		if (WeaponIndex == OwnedWeapons.Num() - 1)
		{
			WeaponIndex = 0;
		}
		else {
			++WeaponIndex;
		}

		CurrentWeapon = OwnedWeapons[WeaponIndex];

		CurrentWeapon->ActivateWeapon();
	}
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	const FAttachmentTransformRules ActorAttachRule(EAttachmentRule::SnapToTarget, false);
	Weapon->AttachToActor(this, ActorAttachRule);

	const FAttachmentTransformRules MeshAttachRule(
		EAttachmentRule::KeepRelative,
		EAttachmentRule::KeepRelative,
		EAttachmentRule::KeepWorld,
		false);

	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), MeshAttachRule, FirstPersonWeaponSocket);

	if (GetMesh() && GetMesh()->GetSkeletalMeshAsset())
	{
		Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), MeshAttachRule, ThirdPersonWeaponSocket);
	}
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	PlayCameraShake(FireCameraShake);

	if (!Montage) return;

	if (USkeletalMeshComponent* FPMesh = GetFirstPersonMesh())
	{
		if (UAnimInstance* FPAnim = FPMesh->GetAnimInstance())
		{
			FPAnim->Montage_Play(Montage, 1.0f);
		}
	}

	if (USkeletalMeshComponent* TPMesh = GetMesh())
	{
		if (TPMesh->GetSkeletalMeshAsset())
		{
			if (UAnimInstance* TPAnim = TPMesh->GetAnimInstance())
			{
				TPAnim->Montage_Play(Montage, 1.0f);
			}
		}
	}
}

void AShooterCharacter::PlayReloadMontage(UAnimMontage* Montage)
{
	if (!Montage) return;

	if (USkeletalMeshComponent* FPMesh = GetFirstPersonMesh())
	{
		if (UAnimInstance* FPAnim = FPMesh->GetAnimInstance())
		{
			FPAnim->Montage_Play(Montage, 1.0f);
		}
	}

	if (USkeletalMeshComponent* TPMesh = GetMesh())
	{
		if (TPMesh->GetSkeletalMeshAsset())
		{
			if (UAnimInstance* TPAnim = TPMesh->GetAnimInstance())
			{
				TPAnim->Montage_Play(Montage, 1.0f);
			}
		}
	}
}

void AShooterCharacter::UpdateReserveHUD(int32 ReserveAmmo)
{
	OnReserveAmmoUpdated.Broadcast(ReserveAmmo);
}

float AShooterCharacter::GetOutgoingDamageMultiplier() const
{
	return StyleMeter ? StyleMeter->GetDamageMultiplier() : 1.0f;
}

void AShooterCharacter::AddWeaponRecoil(const FWeaponRecoilParams& Recoil)
{
	if (RecoilComponent)
	{
		RecoilComponent->AddRecoil(Recoil);
	}
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo);
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
	FHitResult OutHit;

	// Aim follows the control rotation (the gun), not the rendered view: camera modifiers such
	// as the bodycam lag the POV behind the control rotation. The camera component's own
	// rotation is only refreshed when the camera manager updates, so read the controller directly.
	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FRotator AimRot = Controller ? Controller->GetControlRotation() : GetFirstPersonCameraComponent()->GetComponentRotation();
	const FVector End = Start + (AimRot.Vector() * MaxAimDistance);

	// Weapon channel, so the aim point lands on characters instead of the wall behind them.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterAim), false, this);
	QueryParams.AddIgnoredActor(CurrentWeapon);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Weapon, QueryParams);

	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

bool AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	if (!WeaponClass || IsDead())
	{
		return false;
	}

	AShooterWeapon* OwnedWeapon = FindWeaponOfType(WeaponClass);

	if (!OwnedWeapon)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

		AShooterWeapon* AddedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

		if (AddedWeapon)
		{
			OwnedWeapons.Add(AddedWeapon);

			if (CurrentWeapon)
			{
				CurrentWeapon->DeactivateWeapon();
			}

			CurrentWeapon = AddedWeapon;
			CurrentWeapon->ActivateWeapon();
			return true;
		}
	}

	return false;
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount());

	if (GetFirstPersonMesh())
	{
		GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
	}
	if (GetMesh())
	{
		GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
	}
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
}

void AShooterCharacter::OnSemiWeaponRefire()
{
}

// ─── Movement input handlers ────────────────────────────────────────────────

void AShooterCharacter::DoStartSprint()
{
	if (!IsDead() && DoomMovement)
	{
		// Most recent input wins: sprinting lowers the sights (they come back on sprint release
		// if ADS is still held).
		if (bADSInputHeld)
		{
			bADSSuppressedBySprint = true;
			RefreshAimState();
		}

		DoomMovement->StartSprint();
		PlayCameraShake(SprintCameraShake);
	}
}

void AShooterCharacter::DoStopSprint()
{
	if (DoomMovement)
	{
		DoomMovement->StopSprint();
	}

	bADSSuppressedBySprint = false;
	RefreshAimState();
}

void AShooterCharacter::DoSlide()
{
	if (!IsDead() && DoomMovement)
	{
		DoomMovement->StartSlide();
	}
}

void AShooterCharacter::DoStopSlide()
{
	if (DoomMovement)
	{
		DoomMovement->StopSlide();
	}
}

void AShooterCharacter::DoStartADS()
{
	bADSInputHeld = true;
	bADSSuppressedBySprint = false;

	// Raising the sights stops a sprint. Slides, wall runs and air time are unaffected.
	if (!IsDead() && DoomMovement)
	{
		DoomMovement->StopSprint();
	}

	RefreshAimState();
}

void AShooterCharacter::DoStopADS()
{
	bADSInputHeld = false;
	RefreshAimState();
}

void AShooterCharacter::DoToggleBodycam()
{
	if (Bodycam)
	{
		Bodycam->ToggleBodycam();
	}
}

// ─── ADS ────────────────────────────────────────────────────────────────────

bool AShooterCharacter::IsAiming() const
{
	if (IsDead())
	{
		return false;
	}

#if !UE_BUILD_SHIPPING
	if (CVarCarnageForceADS.GetValueOnGameThread() != 0)
	{
		return true;
	}
#endif

	return bADSInputHeld && !bADSSuppressedBySprint;
}

void AShooterCharacter::RefreshAimState()
{
	const bool bAimingNow = IsAiming();
	if (bAimingNow != bLastBroadcastAiming)
	{
		bLastBroadcastAiming = bAimingNow;
		OnAimStateChanged.Broadcast(bAimingNow);
	}
}

void AShooterCharacter::UpdateADS(float DeltaSeconds)
{
	// Catches state changes that don't come from input (death, the ForceADS cvar).
	RefreshAimState();

	const AShooterWeapon* Weapon = CurrentWeapon;

	// The sights drop during a reload so the reload animation isn't pinned to the camera axis,
	// but IsAiming() keeps reporting the real input so they come straight back afterwards.
	const bool bRaiseSights = IsAiming() && !(Weapon && Weapon->IsReloading());
	const float Target = bRaiseSights ? 1.0f : 0.0f;

	const float SmoothIn = Weapon ? Weapon->GetADSSmoothingTimeIn() : DefaultADSSmoothingTimeIn;
	const float SmoothOut = Weapon ? Weapon->GetADSSmoothingTimeOut() : DefaultADSSmoothingTimeOut;

	// A critically damped spring with velocity: reversing mid-transition decelerates and turns
	// around smoothly instead of restarting from a new curve.
	if (DeltaSeconds > 0.0f)
	{
		ViewSpring::CriticalStep(ADSAlpha, ADSAlphaVelocity, Target, bRaiseSights ? SmoothIn : SmoothOut, DeltaSeconds);
	}

	if (ADSAlpha <= 0.0f)
	{
		ADSAlpha = 0.0f;
		ADSAlphaVelocity = FMath::Max(ADSAlphaVelocity, 0.0f);
	}
	else if (ADSAlpha >= 1.0f)
	{
		ADSAlpha = 1.0f;
		ADSAlphaVelocity = FMath::Min(ADSAlphaVelocity, 0.0f);
	}

	const float GameplayAlpha = GetADSGameplayAlpha();

	if (DoomMovement)
	{
		const float AimSpeedScale = Weapon ? Weapon->GetADSMoveSpeedScale() : 0.75f;
		DoomMovement->SetADSSpeedScale(FMath::Lerp(1.0f, AimSpeedScale, GameplayAlpha));
	}

	if (CameraEffects)
	{
		CameraEffects->SetADSState(ADSAlpha,
			Weapon ? Weapon->GetADSWorldFOV() : -1.0f,
			Weapon ? Weapon->GetADSFirstPersonFOV() : -1.0f);
	}

	// UWeaponRecoilComponent reads GetADSAlpha() itself when composing the viewmodel.
}

void AShooterCharacter::DoDash()
{
	if (!IsDead() && DoomMovement && DoomMovement->CanDash())
	{
		PlayCameraShake(DashCameraShake);

		FVector DashDir = GetActorForwardVector() * LastMoveInput.Y + GetActorRightVector() * LastMoveInput.X;
		if (DashDir.IsNearlyZero())
		{
			DashDir = GetActorForwardVector();
		}
		DoomMovement->PerformDash(DashDir);
	}
}

// ─── Weapon helpers ─────────────────────────────────────────────────────────

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
	for (AShooterWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}

	return nullptr;

}

void AShooterCharacter::Die()
{
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->IncrementTeamScore(TeamByte);
	}

	Tags.Add(DeathTag);

	if (AbilitySystem)
	{
		AbilitySystem->AddLooseGameplayTag(CarnageTags::State_Dead);
	}

	GetCharacterMovement()->StopMovementImmediately();

	DisableInput(nullptr);

	OnBulletCountUpdated.Broadcast(0, 0);

	BP_OnDeath();

	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterCharacter::OnRespawn, RespawnTime, false);
}

void AShooterCharacter::OnRespawn()
{
	Destroy();
}

bool AShooterCharacter::IsDead() const
{
	return AttributeSet ? AttributeSet->GetHealth() <= 0.0f : false;
}

// ─── Hit feedback ───────────────────────────────────────────────────────────

void AShooterCharacter::NotifyDamageDealt(AActor* Target, float DamageDealt, bool bKilled, bool bHeadshot, const FVector& HitLocation)
{
	OnHitConfirmed.Broadcast(Target, DamageDealt, bKilled, bHeadshot);

	if (USoundBase* HitSound = (bHeadshot && HeadshotSound) ? HeadshotSound.Get() : HitMarkerSound.Get())
	{
		UGameplayStatics::PlaySound2D(this, HitSound);
	}

	if (bKilled)
	{
		if (KillConfirmSound)
		{
			UGameplayStatics::PlaySound2D(this, KillConfirmSound);
		}

		if (bEnableKillHitStop && GetWorld())
		{
			if (!GetWorld()->GetTimerManager().IsTimerActive(HitStopTimer))
			{
				PreHitStopTimeDilation = UGameplayStatics::GetGlobalTimeDilation(this);
			}

			UGameplayStatics::SetGlobalTimeDilation(this, HitStopTimeDilation);

			GetWorld()->GetTimerManager().SetTimer(
				HitStopTimer, this, &AShooterCharacter::EndHitStop,
				HitStopRealDuration * HitStopTimeDilation, false);
		}
	}
}

void AShooterCharacter::EndHitStop()
{
	UGameplayStatics::SetGlobalTimeDilation(this, PreHitStopTimeDilation);
}

void AShooterCharacter::HandleHealthChanged(float NewHealthPercent)
{
	OnDamaged.Broadcast(NewHealthPercent);
}

void AShooterCharacter::HandleDied()
{
	Die();
}
