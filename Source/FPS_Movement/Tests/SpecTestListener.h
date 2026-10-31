// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StyleMeterComponent.h"
#include "SpecTestListener.generated.h"

/** Tiny delegate receiver used by the automation specs to count dynamic-delegate broadcasts. */
UCLASS()
class USpecTestListener : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION()
	void OnDied() { ++DiedCount; }

	UFUNCTION()
	void OnRankChanged(EStyleRank NewRank) { ++RankChangedCount; LastRank = NewRank; }

	int32 DiedCount = 0;
	int32 RankChangedCount = 0;
	EStyleRank LastRank = EStyleRank::D;
};
