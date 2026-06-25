// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WeaponRecoilTypes.h"
#include "ShooterWeaponHolder.generated.h"

class AShooterWeapon;
class UAnimMontage;

UINTERFACE(MinimalAPI)
class UShooterWeaponHolder : public UInterface
{
	GENERATED_BODY()
};

class FPS_MOVEMENT_API IShooterWeaponHolder
{
	GENERATED_BODY()

public:

	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) = 0;

	virtual void PlayFiringMontage(UAnimMontage* Montage) = 0;

	virtual void PlayReloadMontage(UAnimMontage* Montage) {}

	virtual void AddWeaponRecoil(const FWeaponRecoilParams& Recoil) = 0;

	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) = 0;

	virtual void UpdateReserveHUD(int32 ReserveAmmo) {}

	virtual float GetOutgoingDamageMultiplier() const { return 1.0f; }

	virtual FVector GetWeaponTargetLocation() = 0;

	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) = 0;

	virtual void OnWeaponActivated(AShooterWeapon* Weapon) = 0;

	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) = 0;

	virtual void OnSemiWeaponRefire() = 0;
};
