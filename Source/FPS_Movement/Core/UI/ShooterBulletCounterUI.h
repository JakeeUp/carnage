// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterBulletCounterUI.generated.h"

UCLASS(abstract)
class FPS_MOVEMENT_API UShooterBulletCounterUI : public UUserWidget
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName = "UpdateBulletCounter"))
	void BP_UpdateBulletCounter(int32 MagazineSize, int32 BulletCount);

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName = "Damaged"))
	void BP_Damaged(float LifePercent);

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName = "UpdateReserveAmmo"))
	void BP_UpdateReserveAmmo(int32 ReserveAmmo);

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName = "Set Aiming Down Sights"))
	void BP_SetAimingDownSights(bool bIsAimingDownSights);
};
