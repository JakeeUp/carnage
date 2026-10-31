// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "DoomMovementComponent.h"
#include "DoomCameraEffectsComponent.h"
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

AShooterCharacter::AShooterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDoomMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));
	CameraEffects = CreateDefaultSubobject<UDoomCameraEffectsComponent>(TEXT("Camera Effects"));
	RecoilComponent = CreateDefaultSubobject<UWeaponRecoilComponent>(TEXT("Recoil"));
	StyleMeter = CreateDefaultSubobject<UStyleMeterComponent>(TEXT("Style Meter"));

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

	if (CameraEffects)
	{
		CameraEffects->AimStateProvider.BindUObject(this, &AShooterCharacter::IsAimingDownSights);
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

	return Damage;
}

void AShooterCharacter::DoAim(float Yaw, float Pitch)
{
	if (!IsDead())
	{
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

	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector End = Start + (GetFirstPersonCameraComponent()->GetForwardVector() * MaxAimDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

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
	if (!IsDead())
	{
		bIsAimingDownSights = true;

		if (RecoilComponent)
		{
			RecoilComponent->SetVisualScale(0.35f);
		}

		OnAimStateChanged.Broadcast(true);
	}
}

void AShooterCharacter::DoStopADS()
{
	bIsAimingDownSights = false;

	if (RecoilComponent)
	{
		RecoilComponent->SetVisualScale(1.0f);
	}

	OnAimStateChanged.Broadcast(false);
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
			UGameplayStatics::SetGlobalTimeDilation(this, HitStopTimeDilation);

			GetWorld()->GetTimerManager().SetTimer(
				HitStopTimer, this, &AShooterCharacter::EndHitStop,
				HitStopRealDuration * HitStopTimeDilation, false);
		}
	}
}

void AShooterCharacter::EndHitStop()
{
	UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
}

void AShooterCharacter::HandleHealthChanged(float NewHealthPercent)
{
	OnDamaged.Broadcast(NewHealthPercent);
}

void AShooterCharacter::HandleDied()
{
	Die();
}
