// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DoomMovementComponent.h"
#include "StyleMeterComponent.generated.h"

class UDoomMovementComponent;
class AShooterCharacter;

UENUM(BlueprintType)
enum class EStyleRank : uint8
{
	D   UMETA(DisplayName = "D - Drifter"),
	C   UMETA(DisplayName = "C - Carnage"),
	B   UMETA(DisplayName = "B - Brutal"),
	A   UMETA(DisplayName = "A - Apex"),
	S   UMETA(DisplayName = "S - Slaughter"),
	SS  UMETA(DisplayName = "SS - Savage"),
	SSS UMETA(DisplayName = "SSS - SHATTERED")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStyleRankChanged, EStyleRank, NewRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStyleMeterChanged, EStyleRank, Rank, float, ProgressInRank);

UCLASS(ClassGroup = "Shooter", meta = (BlueprintSpawnableComponent))
class FPS_MOVEMENT_API UStyleMeterComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UStyleMeterComponent();

	// ── Gameplay reward ─────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Reward")
	bool bAffectsGameplay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Reward", meta = (ClampMin = 1, ClampMax = 2))
	float MaxSpeedBonus = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Reward", meta = (ClampMin = 1, ClampMax = 3))
	float MaxDamageBonus = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Reward", meta = (ClampMin = 1, ClampMax = 30))
	float BonusInterpSpeed = 6.0f;

	// ── Scoring ─────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Scoring", meta = (ClampMin = 0))
	float MovementActionPoints = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Scoring", meta = (ClampMin = 0))
	float HitPoints = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Scoring", meta = (ClampMin = 0))
	float HeadshotBonus = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Scoring", meta = (ClampMin = 0))
	float KillBonus = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Scoring", meta = (ClampMin = 1, ClampMax = 3))
	float FreshActionBonus = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Scoring", meta = (ClampMin = 0, ClampMax = 1))
	float RepetitionFalloff = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Scoring", meta = (ClampMin = 1, ClampMax = 12))
	int32 VarietyMemory = 5;

	// ── Decay & punishment ──────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Decay", meta = (ClampMin = 0))
	float DecayPerSecond = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Decay", meta = (ClampMin = 0))
	float DecayPerRank = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Decay", meta = (ClampMin = 0, ClampMax = 3, Units = "s"))
	float DecayGracePeriod = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Decay", meta = (ClampMin = 0, ClampMax = 1))
	float DamageRetainFraction = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Ranks")
	TArray<float> RankThresholds;

	// ── Events (bind from UMG) ──────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "Style")
	FStyleRankChanged OnRankChanged;

	UPROPERTY(BlueprintAssignable, Category = "Style")
	FStyleMeterChanged OnMeterChanged;

	// ── Queries ─────────────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "Style")
	EStyleRank GetRank() const { return CurrentRank; }

	UFUNCTION(BlueprintPure, Category = "Style")
	float GetStylePoints() const { return StylePoints; }

	UFUNCTION(BlueprintPure, Category = "Style")
	FString GetRankLetter() const;

	UFUNCTION(BlueprintPure, Category = "Style")
	float GetProgressInRank() const;

	UFUNCTION(BlueprintPure, Category = "Style")
	float GetDamageMultiplier() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	UFUNCTION()
	void HandleMovementAction(EDoomMovementAction Action);

	UFUNCTION()
	void HandleHitConfirmed(AActor* Target, float Damage, bool bKilled, bool bHeadshot);

	UFUNCTION()
	void HandleDamaged(float LifePercent);

	void AddStyle(float BasePoints, uint8 ActionKind);

	void RecomputeRank();
	int32 RankIndexForPoints(float Points) const;
	float SpeedMultiplierForRank() const;

	UPROPERTY()
	TObjectPtr<UDoomMovementComponent> Movement;

	UPROPERTY()
	TObjectPtr<AShooterCharacter> ShooterOwner;

	float StylePoints = 0.0f;
	float TimeSinceAction = 1000.0f;
	float LastBroadcastProgress = -1.0f;
	float LastLifePercent = 1.0f;
	float CurrentDamageMultiplier = 1.0f;
	float CurrentSpeedBonus = 1.0f;
	EStyleRank CurrentRank = EStyleRank::D;

	TArray<uint8> RecentKinds;

	static constexpr uint8 KIND_HIT = 200;
	static constexpr uint8 KIND_HEADSHOT = 201;
	static constexpr uint8 KIND_KILL = 202;
};
