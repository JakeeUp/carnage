// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class APawn;
class AController;
class UPrimitiveComponent;
class UDamageType;

struct FShooterHitInfo
{
	AActor* Target = nullptr;

	UPrimitiveComponent* HitComponent = nullptr;

	FVector HitLocation = FVector::ZeroVector;

	FVector HitDirection = FVector::ForwardVector;

	FName BoneName = NAME_None;

	float BaseDamage = 0.0f;

	float PhysicsImpulse = 0.0f;

	float KnockbackForce = 0.0f;

	AController* InstigatorController = nullptr;

	APawn* InstigatorPawn = nullptr;

	AActor* DamageCauser = nullptr;

	TSubclassOf<UDamageType> DamageType;

	bool bAllowHeadshot = true;

	float HeadshotMultiplier = 1.75f;

	float HeadshotHeightFraction = 0.8f;
};

struct FShooterHitOutcome
{
	bool bDamagedCharacter = false;

	bool bKilled = false;

	bool bHeadshot = false;

	float DamageDealt = 0.0f;
};

namespace ShooterDamage
{
	FPS_MOVEMENT_API FShooterHitOutcome ApplyHit(const FShooterHitInfo& Hit);
}
