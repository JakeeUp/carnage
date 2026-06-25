// Copyright Epic Games, Inc. All Rights Reserved.

#include "StyleMeterComponent.h"
#include "ShooterCharacter.h"
#include "GameFramework/Character.h"

UStyleMeterComponent::UStyleMeterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	RankThresholds = { 0.0f, 40.0f, 100.0f, 180.0f, 280.0f, 400.0f, 560.0f };
}

void UStyleMeterComponent::BeginPlay()
{
	Super::BeginPlay();

	ShooterOwner = Cast<AShooterCharacter>(GetOwner());

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Movement = Cast<UDoomMovementComponent>(Character->GetCharacterMovement());
		if (Movement)
		{
			Movement->OnMovementAction.AddDynamic(this, &UStyleMeterComponent::HandleMovementAction);
		}
	}

	if (ShooterOwner)
	{
		ShooterOwner->OnHitConfirmed.AddDynamic(this, &UStyleMeterComponent::HandleHitConfirmed);
		ShooterOwner->OnDamaged.AddDynamic(this, &UStyleMeterComponent::HandleDamaged);
	}
}

void UStyleMeterComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (Movement)
	{
		Movement->OnMovementAction.RemoveDynamic(this, &UStyleMeterComponent::HandleMovementAction);
		Movement->SetExternalSpeedScale(1.0f);
	}
	if (ShooterOwner)
	{
		ShooterOwner->OnHitConfirmed.RemoveDynamic(this, &UStyleMeterComponent::HandleHitConfirmed);
		ShooterOwner->OnDamaged.RemoveDynamic(this, &UStyleMeterComponent::HandleDamaged);
	}

	Super::EndPlay(EndPlayReason);
}

// ── Event handlers ──────────────────────────────────────────────────────────

void UStyleMeterComponent::HandleMovementAction(EDoomMovementAction Action)
{
	AddStyle(MovementActionPoints, static_cast<uint8>(Action));
}

void UStyleMeterComponent::HandleHitConfirmed(AActor* Target, float Damage, bool bKilled, bool bHeadshot)
{
	float Points = HitPoints;
	uint8 Kind = KIND_HIT;

	if (bHeadshot)
	{
		Points += HeadshotBonus;
		Kind = KIND_HEADSHOT;
	}
	if (bKilled)
	{
		Points += KillBonus;
		Kind = KIND_KILL;
	}

	AddStyle(Points, Kind);
}

void UStyleMeterComponent::HandleDamaged(float LifePercent)
{
	if (LifePercent < LastLifePercent - KINDA_SMALL_NUMBER)
	{
		StylePoints *= DamageRetainFraction;
		RecomputeRank();
	}
	LastLifePercent = LifePercent;
}

// ── Scoring ───────────────────────────────────────────────────────────────

void UStyleMeterComponent::AddStyle(float BasePoints, uint8 ActionKind)
{
	int32 RepeatCount = 0;
	for (const uint8 Kind : RecentKinds)
	{
		if (Kind == ActionKind)
		{
			++RepeatCount;
		}
	}

	float Multiplier = FMath::Pow(RepetitionFalloff, static_cast<float>(RepeatCount));
	if (RepeatCount == 0)
	{
		Multiplier *= FreshActionBonus;
	}

	StylePoints += BasePoints * Multiplier;
	TimeSinceAction = 0.0f;

	RecentKinds.Add(ActionKind);
	while (RecentKinds.Num() > VarietyMemory)
	{
		RecentKinds.RemoveAt(0);
	}

	RecomputeRank();
}

void UStyleMeterComponent::RecomputeRank()
{
	const int32 NewIndex = RankIndexForPoints(StylePoints);
	const EStyleRank NewRank = static_cast<EStyleRank>(NewIndex);

	if (NewRank != CurrentRank)
	{
		CurrentRank = NewRank;
		OnRankChanged.Broadcast(CurrentRank);
	}

	OnMeterChanged.Broadcast(CurrentRank, GetProgressInRank());
}

int32 UStyleMeterComponent::RankIndexForPoints(float Points) const
{
	int32 Index = 0;
	for (int32 i = 0; i < RankThresholds.Num(); ++i)
	{
		if (Points >= RankThresholds[i])
		{
			Index = i;
		}
		else
		{
			break;
		}
	}
	return FMath::Clamp(Index, 0, static_cast<int32>(EStyleRank::SSS));
}

// ── Tick: decay + feed the speed bonus ──────────────────────────────────────

void UStyleMeterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceAction += DeltaTime;

	if (TimeSinceAction > DecayGracePeriod && StylePoints > 0.0f)
	{
		const int32 RankIndex = static_cast<int32>(CurrentRank);
		const float Decay = (DecayPerSecond + DecayPerRank * RankIndex) * DeltaTime;
		StylePoints = FMath::Max(0.0f, StylePoints - Decay);
		RecomputeRank();
	}

	const float TargetSpeed = bAffectsGameplay ? SpeedMultiplierForRank() : 1.0f;
	CurrentSpeedBonus = FMath::FInterpTo(CurrentSpeedBonus, TargetSpeed, DeltaTime, BonusInterpSpeed);
	if (Movement)
	{
		Movement->SetExternalSpeedScale(CurrentSpeedBonus);
	}

	const float RankAlpha = static_cast<float>(CurrentRank) / static_cast<float>(EStyleRank::SSS);
	CurrentDamageMultiplier = bAffectsGameplay ? FMath::Lerp(1.0f, MaxDamageBonus, RankAlpha) : 1.0f;
}

float UStyleMeterComponent::SpeedMultiplierForRank() const
{
	const float RankAlpha = static_cast<float>(CurrentRank) / static_cast<float>(EStyleRank::SSS);
	return FMath::Lerp(1.0f, MaxSpeedBonus, RankAlpha);
}

// ── Queries ─────────────────────────────────────────────────────────────────

float UStyleMeterComponent::GetDamageMultiplier() const
{
	return CurrentDamageMultiplier;
}

FString UStyleMeterComponent::GetRankLetter() const
{
	static const TCHAR* Letters[] = { TEXT("D"), TEXT("C"), TEXT("B"), TEXT("A"), TEXT("S"), TEXT("SS"), TEXT("SSS") };
	const int32 Index = FMath::Clamp(static_cast<int32>(CurrentRank), 0, 6);
	return Letters[Index];
}

float UStyleMeterComponent::GetProgressInRank() const
{
	const int32 Index = static_cast<int32>(CurrentRank);
	if (!RankThresholds.IsValidIndex(Index))
	{
		return 0.0f;
	}

	const float Floor = RankThresholds[Index];

	if (!RankThresholds.IsValidIndex(Index + 1))
	{
		return 1.0f;
	}

	const float Ceiling = RankThresholds[Index + 1];
	return Ceiling > Floor ? FMath::Clamp((StylePoints - Floor) / (Ceiling - Floor), 0.0f, 1.0f) : 0.0f;
}
