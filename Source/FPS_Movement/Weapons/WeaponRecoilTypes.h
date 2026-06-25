// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeaponRecoilTypes.generated.h"

USTRUCT(BlueprintType)
struct FWeaponRecoilParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin=0, ClampMax=20))
	float PitchKick = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin=0, ClampMax=10))
	float YawKickRandom = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin=0, ClampMax=30))
	float VisualKickBack = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin=0, ClampMax=30))
	float VisualKickPitch = 1.5f;
};
