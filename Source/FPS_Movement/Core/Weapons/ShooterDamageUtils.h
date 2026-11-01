// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"

class AActor;
class APawn;
class ACharacter;
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

/**
 * Point damage event sent by ShooterDamage::ApplyHit. Carries the resolved headshot flag and the
 * weapon's physics impulse on top of the engine's bone / location / direction, so hit reactions
 * can use them. Receivers that only know FPointDamageEvent still read it as one (IsOfType).
 */
struct FShooterPointDamageEvent : public FPointDamageEvent
{
	/** True when ApplyHit resolved the shot as a headshot (bone name or height fallback). */
	bool bHeadshot = false;

	/** The weapon's physics impulse for this shot (same units as FShooterHitInfo::PhysicsImpulse). */
	float PhysicsImpulse = 0.0f;

	FShooterPointDamageEvent() = default;
	FShooterPointDamageEvent(float InDamage, const FHitResult& InHitInfo, const FVector& InShotDirection, TSubclassOf<UDamageType> InDamageTypeClass)
		: FPointDamageEvent(InDamage, InHitInfo, InShotDirection, InDamageTypeClass)
	{
	}

	/** Unique damage event ID ('SHPD'). */
	static const int32 ClassID = 0x53485044;

	virtual int32 GetTypeID() const override { return FShooterPointDamageEvent::ClassID; }
	virtual bool IsOfType(int32 InID) const override { return (FShooterPointDamageEvent::ClassID == InID) || FPointDamageEvent::IsOfType(InID); }
};

namespace ShooterDamage
{
	FPS_MOVEMENT_API FShooterHitOutcome ApplyHit(const FShooterHitInfo& Hit);

	/**
	 * Route Weapon-channel traces to the character's physics-asset bodies instead of its capsule,
	 * so FHitResult::BoneName is valid (headshots, per-bone reactions). Does nothing and returns
	 * false if the mesh has no physics asset; the capsule then stays the hit volume.
	 * @param bEnablePhysicsBodies Also switch the mesh to QueryAndPhysics so bodies can be blended
	 *        into simulation for hit reactions (QueryOnly bodies cannot simulate).
	 */
	FPS_MOVEMENT_API bool SetupPerBoneWeaponHits(ACharacter* Character, bool bEnablePhysicsBodies);
}
