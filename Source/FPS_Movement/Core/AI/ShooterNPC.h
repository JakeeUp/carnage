// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPS_MovementCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterNPC.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPawnDeathDelegate);

class AShooterWeapon;
class UJuggleComponent;

UCLASS(abstract)
class FPS_MOVEMENT_API AShooterNPC : public AFPS_MovementCharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()

public:

	AShooterNPC();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	float CurrentHP = 100.0f;

protected:

	UPROPERTY(EditAnywhere, Category="Damage")
	FName RagdollCollisionProfile = FName("Ragdoll");

	UPROPERTY(EditAnywhere, Category="Damage")
	float DeferredDestructionTime = 5.0f;

	UPROPERTY(EditAnywhere, Category="Team")
	uint8 TeamByte = 1;

	UPROPERTY(EditAnywhere, Category="Team")
	FName DeathTag = FName("Dead");

	TObjectPtr<AShooterWeapon> Weapon;

	UPROPERTY(EditAnywhere, Category="Weapon")
	TSubclassOf<AShooterWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	UPROPERTY(EditAnywhere, Category="Aim")
	float AimRange = 10000.0f;

	UPROPERTY(EditAnywhere, Category="Aim")
	float AimVarianceHalfAngle = 10.0f;

	UPROPERTY(EditAnywhere, Category="Aim")
	float MinAimOffsetZ = -35.0f;

	UPROPERTY(EditAnywhere, Category="Aim")
	float MaxAimOffsetZ = -60.0f;

	TObjectPtr<AActor> CurrentAimTarget;

	bool bIsShooting = false;

	bool bIsDead = false;

	FTimerHandle DeathTimer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Juggle")
	TObjectPtr<UJuggleComponent> JuggleComp;

	UFUNCTION()
	void HandleJuggleStateChanged(bool bJuggled);

public:

	FPawnDeathDelegate OnPawnDeath;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	//~Begin IShooterWeaponHolder interface

	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	virtual void AddWeaponRecoil(const FWeaponRecoilParams& Recoil) override;

	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	virtual FVector GetWeaponTargetLocation() override;

	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

protected:

	void Die();

	void DeferredDestruction();

public:

	void StartShooting(AActor* ActorToShoot);

	void StopShooting();
};
