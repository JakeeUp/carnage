// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterWeaponHolder.h"
#include "Animation/AnimInstance.h"
#include "Engine/HitResult.h"
#include "ShooterWeapon.generated.h"

class IShooterWeaponHolder;
class AShooterProjectile;
class USkeletalMeshComponent;
class UAnimMontage;
class UAnimInstance;
class UNiagaraSystem;
class USoundBase;
class UDamageType;
class APlayerController;
class USceneComponent;

UENUM(BlueprintType)
enum class EShooterWeaponFireMode : uint8
{
	Projectile,
	Hitscan
};

UCLASS(abstract)
class FPS_MOVEMENT_API AShooterWeapon : public AActor
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMesh;

protected:

	IShooterWeaponHolder* WeaponOwner;

	UPROPERTY(EditAnywhere, Category="Ammo")
	TSubclassOf<AShooterProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category="Ammo", meta = (ClampMin = 0, ClampMax = 100))
	int32 MagazineSize = 10;

	int32 CurrentBullets = 0;

	UPROPERTY(EditAnywhere, Category="Ammo", meta = (ClampMin = 0))
	int32 ReserveAmmo = 90;

	UPROPERTY(EditAnywhere, Category="Ammo")
	bool bAutoReloadOnEmpty = true;

	UPROPERTY(EditAnywhere, Category="Ammo", meta = (ClampMin = 0.1, ClampMax = 5, Units = "s"))
	float ReloadDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* FiringMontage;

	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* ReloadMontage;

	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* WeaponReloadMontage;

	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* WeaponFiringMontage;

	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* FiringMontage_ADS;

	UPROPERTY(EditAnywhere, Category="Animation")
	TSubclassOf<UAnimInstance> FirstPersonAnimInstanceClass;

	UPROPERTY(EditAnywhere, Category="Animation")
	TSubclassOf<UAnimInstance> ThirdPersonAnimInstanceClass;

	UPROPERTY(EditAnywhere, Category="Aim", meta = (ClampMin = 0, ClampMax = 90, Units = "Degrees"))
	float AimVariance = 0.0f;

	UPROPERTY(EditAnywhere, Category="Aim")
	FWeaponRecoilParams RecoilParams;

	// ── Fire mode ───────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="FireMode")
	EShooterWeaponFireMode FireMode = EShooterWeaponFireMode::Projectile;

	UPROPERTY(EditAnywhere, Category="FireMode|Hitscan", meta = (ClampMin = 0, ClampMax = 500))
	float HitscanDamage = 20.0f;

	UPROPERTY(EditAnywhere, Category="FireMode|Hitscan", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float HitscanRange = 15000.0f;

	UPROPERTY(EditAnywhere, Category="FireMode|Hitscan", meta = (ClampMin = 0, ClampMax = 50000))
	float HitscanImpulse = 800.0f;

	UPROPERTY(EditAnywhere, Category="FireMode|Hitscan", meta = (ClampMin = 0, ClampMax = 5000, Units = "cm/s"))
	float HitscanKnockback = 650.0f;

	UPROPERTY(EditAnywhere, Category="FireMode|Hitscan")
	TSubclassOf<UDamageType> HitscanDamageType;

	UPROPERTY(EditAnywhere, Category="FireMode|Hitscan")
	bool bAllowHeadshots = true;

	UPROPERTY(EditAnywhere, Category="FireMode|Hitscan", meta = (ClampMin = 1, ClampMax = 10))
	float HeadshotMultiplier = 1.75f;

	UPROPERTY(EditAnywhere, Category="FireMode|Hitscan", meta = (ClampMin = 0.5, ClampMax = 1))
	float HeadshotHeightFraction = 0.8f;

	// ── FX ──────────────────────────────────────────────────────────────
	// Muzzle flash, tracer, impacts and shells are procedural by default
	// (UCarnageWeaponFXSubsystem, tuned in Project Settings > Game > Carnage Weapon FX).

	/** Draw the procedural muzzle flash / tracer / impact / shell FX. */
	UPROPERTY(EditAnywhere, Category="FX")
	bool bUseProceduralFX = true;

	/**
	 * When set, the Niagara systems below (where assigned) replace the procedural muzzle flash and tracer.
	 * Off by default so placeholder template systems on existing Blueprints don't override the procedural FX.
	 */
	UPROPERTY(EditAnywhere, Category="FX|Niagara Overrides")
	bool bUseNiagaraOverrides = false;

	UPROPERTY(EditAnywhere, Category="FX|Niagara Overrides", meta=(EditCondition="bUseNiagaraOverrides"))
	TObjectPtr<UNiagaraSystem> MuzzleFlashSystem;

	/** Point the Niagara muzzle flash along the actual aim direction instead of trusting the socket axes. */
	UPROPERTY(EditAnywhere, Category="FX|Niagara Overrides", meta=(EditCondition="bUseNiagaraOverrides"))
	bool bAlignMuzzleFlashToAim = true;

	/** Socket-relative rotation for the Niagara muzzle flash; only used when bAlignMuzzleFlashToAim is off. */
	UPROPERTY(EditAnywhere, Category="FX|Niagara Overrides", meta=(EditCondition="bUseNiagaraOverrides && !bAlignMuzzleFlashToAim"))
	FRotator MuzzleFlashRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category="FX|Niagara Overrides", meta=(EditCondition="bUseNiagaraOverrides"))
	TObjectPtr<UNiagaraSystem> TracerSystem;

	UPROPERTY(EditAnywhere, Category="FX|Niagara Overrides", meta=(EditCondition="bUseNiagaraOverrides"))
	FName TracerEndParamName = FName("BeamEnd");

	UPROPERTY(EditAnywhere, Category="FX|Sound")
	TObjectPtr<USoundBase> FireSound;

	/** Random per-shot pitch variation (0.06 = +/-6%). */
	UPROPERTY(EditAnywhere, Category="FX|Sound", meta=(ClampMin=0, ClampMax=0.5))
	float FireSoundPitchVariance = 0.06f;

	/** Random per-shot volume variation (0.05 = +/-5%). */
	UPROPERTY(EditAnywhere, Category="FX|Sound", meta=(ClampMin=0, ClampMax=0.5))
	float FireSoundVolumeVariance = 0.05f;

	UPROPERTY(EditAnywhere, Category="Aim")
	FName MuzzleSocketName;

	UPROPERTY(EditAnywhere, Category="Aim", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float MuzzleOffset = 10.0f;

	UPROPERTY(EditAnywhere, Category="Refire")
	bool bFullAuto = false;

	UPROPERTY(EditAnywhere, Category="Refire", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float RefireRate = 0.12f;

	UPROPERTY(EditAnywhere, Category="Refire", meta = (ClampMin = 0, ClampMax = 0.5, Units = "s"))
	float ShotBufferWindow = 0.1f;

	float TimeOfLastShot = 0.0f;

	bool bIsFiring = false;

	bool bShotBuffered = false;

	bool bIsReloading = false;

	FTimerHandle RefireTimer;

	FTimerHandle ReloadTimer;

	TWeakObjectPtr<APawn> PawnOwner;

	UPROPERTY(EditAnywhere, Category="Perception", meta = (ClampMin = 0, ClampMax = 100))
	float ShotLoudness = 1.0f;

	UPROPERTY(EditAnywhere, Category="Perception", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float ShotNoiseRange = 3000.0f;

	UPROPERTY(EditAnywhere, Category="Perception")
	FName ShotNoiseTag = FName("Shot");

public:	

	AShooterWeapon();

protected:
	
	virtual void BeginPlay() override;

	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

protected:

	UFUNCTION()
	void OnOwnerDestroyed(AActor* DestroyedActor);

public:

	void ActivateWeapon();

	void DeactivateWeapon();

	void StartFiring();

	void StopFiring();

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void StartReload();

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category="Weapon")
	float GetReloadDuration() const { return ReloadDuration; }

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool CanReload() const;

protected:

	void FinishReload();

	virtual void Fire();

	void FireCooldownExpired();

	void FireShot();

	virtual void FireProjectile(const FVector& TargetLocation);

	virtual void FireHitscan(const FVector& TargetLocation);

	FTransform CalculateProjectileSpawnTransform(const FVector& TargetLocation) const;

	// ── FX helpers ──────────────────────────────────────────────────────

	/** Owning local player controller with a camera manager, or null (NPCs, remote pawns). */
	APlayerController* GetLocalViewer() const;

	/** Damage-trace origin and unspread direction: camera for local players, muzzle for everyone else. */
	void GetShotRay(const FVector& TargetLocation, FVector& OutStart, FVector& OutDirection) const;

	/** Mesh carrying the muzzle used for FX, and whether it renders first-person. */
	USceneComponent* GetFXMuzzleComponent(bool& bOutFirstPerson) const;

	/** Where the muzzle visually appears in the world (first-person morph applied). */
	FVector GetVisibleMuzzleLocation() const;

	void PlayMuzzleFX(const FVector& AimDirection);

	void PlayTracerFX(const FVector& VisibleStart, const FVector& End);

	void PlayImpactFX(const FHitResult& Hit, const FVector& ShotDirection) const;

	void PlayFireSound() const;

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon", meta = (DisplayName = "On Shot Fired"))
	void BP_OnShotFired();

	UFUNCTION(BlueprintImplementableEvent, Category="Weapon", meta = (DisplayName = "On Hitscan Fired"))
	void BP_OnHitscanFired(const FVector& TraceStart, const FVector& TraceEnd, const FHitResult& Hit);

public:

	UFUNCTION(BlueprintPure, Category="Weapon")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; };

	UFUNCTION(BlueprintPure, Category="Weapon")
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; };

	const TSubclassOf<UAnimInstance>& GetFirstPersonAnimInstanceClass() const;

	const TSubclassOf<UAnimInstance>& GetThirdPersonAnimInstanceClass() const;

	int32 GetMagazineSize() const { return MagazineSize; };

	int32 GetBulletCount() const { return CurrentBullets; }

	UFUNCTION(BlueprintPure, Category="Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	// ════════════════════════════════════════════════════════════════════
	// ADS (aim down sights) — read by AShooterCharacter / UWeaponRecoilComponent.
	// Header-only block: the viewmodel aligns the sight frame defined here to
	// the camera axis. Sight frame convention: +X looks down the sight line
	// towards the target, +Z is up, origin sits on the sight line at the rear sight.
	// ════════════════════════════════════════════════════════════════════

protected:

	/** Socket on the first-person weapon mesh that already defines the sight frame (+X forward). Used as-is when present. */
	UPROPERTY(EditAnywhere, Category="ADS")
	FName SightSocketName = FName("Sight");

	/** Socket used when SightSocketName is missing on the mesh. None = use MuzzleSocketName. */
	UPROPERTY(EditAnywhere, Category="ADS")
	FName FallbackSightSocketName = NAME_None;

	/**
	 * Sight frame expressed in the FALLBACK socket's space (ignored when a real sight socket exists).
	 * Default assumes the pistol's "Muzzle" socket, whose -X points down the barrel: yaw 180 turns +X
	 * down the bore, and the location moves ~12 cm back along the slide and ~3 cm above the bore.
	 */
	UPROPERTY(EditAnywhere, Category="ADS")
	FTransform SightOffset = FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(12.0f, 0.0f, 3.0f));

	/** Distance from the eye to the rear sight when fully aimed. Rendered distance is this times the camera's FirstPersonScale. */
	UPROPERTY(EditAnywhere, Category="ADS", meta=(ClampMin=5, ClampMax=80, Units="cm"))
	float EyeRelief = 28.0f;

	/** First-person (viewmodel) FOV when fully aimed. This is what magnifies the gun. */
	UPROPERTY(EditAnywhere, Category="ADS", meta=(ClampMin=20, ClampMax=120, Units="Degrees"))
	float ADSFirstPersonFOV = 60.0f;

	/** World FOV when fully aimed. */
	UPROPERTY(EditAnywhere, Category="ADS", meta=(ClampMin=10, ClampMax=120, Units="Degrees"))
	float ADSWorldFOV = 65.0f;

	/** Critical-spring smoothing time when raising the sights. */
	UPROPERTY(EditAnywhere, Category="ADS", meta=(ClampMin=0.01, ClampMax=1, Units="s"))
	float ADSSmoothingTimeIn = 0.085f;

	/** Critical-spring smoothing time when lowering the sights. */
	UPROPERTY(EditAnywhere, Category="ADS", meta=(ClampMin=0.01, ClampMax=1, Units="s"))
	float ADSSmoothingTimeOut = 0.07f;

	/** Grounded move-speed multiplier when fully aimed. */
	UPROPERTY(EditAnywhere, Category="ADS", meta=(ClampMin=0.1, ClampMax=1))
	float ADSMoveSpeedScale = 0.75f;

public:

	FName GetSightSocketName() const { return SightSocketName; }
	FName GetFallbackSightSocketName() const { return FallbackSightSocketName.IsNone() ? MuzzleSocketName : FallbackSightSocketName; }
	const FTransform& GetSightOffset() const { return SightOffset; }
	float GetEyeRelief() const { return EyeRelief; }
	float GetADSFirstPersonFOV() const { return ADSFirstPersonFOV; }
	float GetADSWorldFOV() const { return ADSWorldFOV; }
	float GetADSSmoothingTimeIn() const { return ADSSmoothingTimeIn; }
	float GetADSSmoothingTimeOut() const { return ADSSmoothingTimeOut; }
	float GetADSMoveSpeedScale() const { return ADSMoveSpeedScale; }

	// ═══════════════════════════ end ADS block ═══════════════════════════
};
