#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CarnageFeelCapture.generated.h"

/**
 * Dev-only scripted capture for checking weapon feel without manual input.
 *
 * Run with:  -ExecCmds="carnage.Debug.FeelCapture 1"
 * The sequence takes screenshots (Saved/Screenshots/) at hip idle, mid-shot, mid-ADS blend,
 * full ADS, and an ADS shot, then quits if carnage.Debug.FeelCaptureQuit is 1.
 * Compiled out of Shipping builds.
 */
UCLASS()
class FPS_MOVEMENT_API UCarnageFeelCapture : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

private:

	struct FStep
	{
		float Time;
		TFunction<void()> Action;
	};

	void BuildSequence();
	void BuildMoveTest();
	void BuildCombatTest();

	/** Combat test helpers: aim the player's control rotation at a world point and log state. */
	void AimAt(const FVector& WorldPoint) const;
	bool AimAtNearestLivingNPC() const;
	bool AimAtTrainerComponent(const FString& NamePrefix) const;
	void LogCombatState(const TCHAR* Label) const;
	void LogMovement();
	void Shot(const TCHAR* Name);

	/** Move test: hold forward input every frame while set. */
	bool bHoldForward = false;
	float NextLogTime = 0.0f;
	FString Phase;

	TArray<FStep> Steps;
	int32 NextStep = 0;
	float Elapsed = 0.0f;
	bool bStarted = false;
};
