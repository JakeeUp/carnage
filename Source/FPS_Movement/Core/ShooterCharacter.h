// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "FPS_MovementCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterHitFeedbackInterface.h"
#include "ShooterCharacter.generated.h"

class AShooterWeapon;
class UInputAction;
class UInputComponent;
class UPawnNoiseEmitterComponent;
class UDoomMovementComponent;
class UDoomCameraEffectsComponent;
class UWeaponRecoilComponent;
class UStyleMeterComponent;
class UCarnageAbilitySystemComponent;
class UCarnageAttributeSet;
class UGameplayEffect;
class UCameraShakeBase;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletCountUpdatedDelegate, int32, MagazineSize, int32, Bullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamagedDelegate, float, LifePercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FHitConfirmedDelegate, AActor*, Target, float, Damage, bool, bKilled, bool, bHeadshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReserveAmmoUpdatedDelegate, int32, ReserveAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAimStateChangedDelegate, bool, bIsAimingDownSights);

UCLASS(abstract)
class FPS_MOVEMENT_API AShooterCharacter : public AFPS_MovementCharacter, public IShooterWeaponHolder, public IAbilitySystemInterface, public IShooterHitFeedbackReceiver
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UPawnNoiseEmitterComponent* PawnNoiseEmitter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UDoomCameraEffectsComponent* CameraEffects;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWeaponRecoilComponent> RecoilComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStyleMeterComponent> StyleMeter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS", meta = (AllowPrivateAccess = "true"))
	UCarnageAbilitySystemComponent* AbilitySystem;

	UPROPERTY()
	UCarnageAttributeSet* AttributeSet;

protected:

	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* FireAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* SwitchWeaponAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SlideAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DashAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ADSAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ReloadAction;

	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	UPROPERTY(EditAnywhere, Category ="Aim", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	UPROPERTY(EditAnywhere, Category="Health")
	float MaxHP = 500.0f;

	UPROPERTY(EditAnywhere, Category="Health")
	float StartingArmor = 0.0f;

	UPROPERTY(EditAnywhere, Category="Health")
	float MaxArmor = 100.0f;

	UPROPERTY(EditAnywhere, Category="GAS")
	TSubclassOf<UGameplayEffect> DefaultDamageEffect;

	// ── Camera Shakes ────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="Camera Shakes")
	TSubclassOf<UCameraShakeBase> SprintCameraShake;

	UPROPERTY(EditAnywhere, Category="Camera Shakes")
	TSubclassOf<UCameraShakeBase> LandingCameraShake;

	UPROPERTY(EditAnywhere, Category="Camera Shakes")
	TSubclassOf<UCameraShakeBase> FireCameraShake;

	UPROPERTY(EditAnywhere, Category="Camera Shakes")
	TSubclassOf<UCameraShakeBase> DashCameraShake;

	// ── Hit Feedback ─────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="Hit Feedback")
	TObjectPtr<USoundBase> HitMarkerSound;

	UPROPERTY(EditAnywhere, Category="Hit Feedback")
	TObjectPtr<USoundBase> HeadshotSound;

	UPROPERTY(EditAnywhere, Category="Hit Feedback")
	TObjectPtr<USoundBase> KillConfirmSound;

	UPROPERTY(EditAnywhere, Category="Hit Feedback")
	bool bEnableKillHitStop = true;

	UPROPERTY(EditAnywhere, Category="Hit Feedback", meta=(ClampMin=0.01, ClampMax=0.5))
	float HitStopTimeDilation = 0.1f;

	UPROPERTY(EditAnywhere, Category="Hit Feedback", meta=(ClampMin=0.01, ClampMax=0.3, Units="s"))
	float HitStopRealDuration = 0.05f;

	UPROPERTY(EditAnywhere, Category="Team")
	uint8 TeamByte = 0;

	UPROPERTY(EditAnywhere, Category="Team")
	FName DeathTag = FName("Dead");

	UPROPERTY()
	TArray<TObjectPtr<AShooterWeapon>> OwnedWeapons;

	UPROPERTY()
	TObjectPtr<AShooterWeapon> CurrentWeapon;

	UPROPERTY(EditAnywhere, Category ="Destruction", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 5.0f;

	FTimerHandle RespawnTimer;

public:

	FBulletCountUpdatedDelegate OnBulletCountUpdated;

	FDamagedDelegate OnDamaged;

	UPROPERTY(BlueprintAssignable, Category="Hit Feedback")
	FHitConfirmedDelegate OnHitConfirmed;

	UPROPERTY(BlueprintAssignable, Category="Weapon")
	FReserveAmmoUpdatedDelegate OnReserveAmmoUpdated;

	UPROPERTY(BlueprintAssignable, Category="Aim")
	FAimStateChangedDelegate OnAimStateChanged;

public:

	AShooterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	virtual void Tick(float DeltaSeconds) override;

private:

	void SetLooseTag(const FGameplayTag& Tag, bool bShouldHave);

public:

	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	virtual void DoAim(float Yaw, float Pitch) override;

	virtual void DoMove(float Right, float Forward)  override;

	virtual void DoJumpStart()  override;

	virtual void DoJumpEnd()  override;

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStartFiring();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStopFiring();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSwitchWeapon();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoReload();

public:

	//~Begin IShooterWeaponHolder interface

	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	virtual void PlayReloadMontage(UAnimMontage* Montage) override;

	virtual void AddWeaponRecoil(const FWeaponRecoilParams& Recoil) override;

	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	virtual void UpdateReserveHUD(int32 ReserveAmmo) override;

	virtual float GetOutgoingDamageMultiplier() const override;

	virtual FVector GetWeaponTargetLocation() override;

	virtual bool AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	UFUNCTION(BlueprintCallable, Category="Weapon", meta=(DisplayName="Give Weapon"))
	void GiveWeapon(TSubclassOf<AShooterWeapon> WeaponClass) { AddWeaponClass(WeaponClass); }

	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

	//~Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End IAbilitySystemInterface

	//~Begin IShooterHitFeedbackReceiver
	virtual void NotifyDamageDealt(AActor* Target, float DamageDealt, bool bKilled, bool bHeadshot, const FVector& HitLocation) override;
	//~End IShooterHitFeedbackReceiver

	virtual void OnJumped_Implementation() override;

	UFUNCTION(BlueprintPure, Category="GAS")
	UCarnageAbilitySystemComponent* GetCarnageASC() const { return AbilitySystem; }

	UFUNCTION(BlueprintPure, Category="GAS")
	const UCarnageAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintCallable, Category="Camera Shakes")
	void PlayCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.0f);

	virtual void Landed(const FHitResult& Hit) override;

protected:

	// ── Movement input handlers ────────────────────────────────────────

	void DoStartSprint();
	void DoStopSprint();
	void DoSlide();
	void DoStopSlide();
	void DoDash();
	void DoStartADS();
	void DoStopADS();

public:

	UFUNCTION(BlueprintPure, Category="Aim")
	bool IsAimingDownSights() const { return bIsAimingDownSights; }

protected:

	bool bIsAimingDownSights = false;

	UPROPERTY()
	TObjectPtr<UDoomMovementComponent> DoomMovement;

	FVector2D LastMoveInput = FVector2D::ZeroVector;

	AShooterWeapon* FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const;

	void Die();

	UFUNCTION()
	void HandleHealthChanged(float NewHealthPercent);

	UFUNCTION()
	void HandleDied();

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Death"))
	void BP_OnDeath();

	void OnRespawn();

	void EndHitStop();

	FTimerHandle HitStopTimer;

public:

	bool IsDead() const;
};
