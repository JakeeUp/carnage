// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ShooterHitFeedbackInterface.generated.h"

UINTERFACE(MinimalAPI)
class UShooterHitFeedbackReceiver : public UInterface
{
	GENERATED_BODY()
};

class FPS_MOVEMENT_API IShooterHitFeedbackReceiver
{
	GENERATED_BODY()

public:

	virtual void NotifyDamageDealt(AActor* Target, float DamageDealt, bool bKilled, bool bHeadshot, const FVector& HitLocation) = 0;
};
