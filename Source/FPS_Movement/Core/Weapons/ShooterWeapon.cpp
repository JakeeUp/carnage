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

	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
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
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
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

	if (MuzzleFlashSystem && FirstPersonMesh)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlashSystem, FirstPersonMesh, MuzzleSocketName,
			FVector::ZeroVector, MuzzleFlashRotationOffset,
			EAttachLocation::SnapToTarget, true);
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

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

	GetWorld()->SpawnActor<AShooterProjectile>(ProjectileClass, ProjectileTransform, SpawnParams);
}

void AShooterWeapon::FireHitscan(const FVector& TargetLocation)
{
	const FVector MuzzleLoc = FirstPersonMesh->GetSocketLocation(MuzzleSocketName);

	FVector AimDir = (TargetLocation - MuzzleLoc).GetSafeNormal();
	if (AimVariance > 0.0f)
	{
		AimDir = FMath::VRandCone(AimDir, FMath::DegreesToRadians(AimVariance));
	}

	const FVector TraceEnd = MuzzleLoc + AimDir * HitscanRange;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, MuzzleLoc, TraceEnd, ECC_Visibility, QueryParams);
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

	if (TracerSystem)
	{
		if (UNiagaraComponent* Tracer = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), TracerSystem, MuzzleLoc, AimDir.Rotation()))
		{
			Tracer->SetVectorParameter(TracerEndParamName, BeamEnd);
		}
	}

	BP_OnHitscanFired(MuzzleLoc, BeamEnd, Hit);
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
