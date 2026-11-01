// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGameMode.generated.h"

class UShooterUI;

UCLASS(abstract)
class FPS_MOVEMENT_API AShooterGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	UPROPERTY(EditAnywhere, Category="Shooter")
	TSubclassOf<UShooterUI> ShooterUIClass;

	/** The template team-score widget. Off by default: the native HUD owns the screen. */
	UPROPERTY(EditAnywhere, Category="Shooter")
	bool bShowTemplateScoreUI = false;

	UPROPERTY()
	TObjectPtr<UShooterUI> ShooterUI;

	TMap<uint8, int32> TeamScores;

protected:

	virtual void BeginPlay() override;

public:

	void IncrementTeamScore(uint8 TeamByte);
};
