// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPS_MovementCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterNPC.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPawnDeathDelegate);

class AShooterWeapon;
class UJuggleComponent;

UCLASS(abstract)
class FPS_MOVEMENT_API AShooterNPC : public AFPS_MovementCharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()

public:

	AShooterNPC();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	float CurrentHP = 100.0f;

protected:

	UPROPERTY(EditAnywhere, Category="Damage")
	FName RagdollCollisionProfile = FName("Ragdoll");

	UPROPERTY(EditAnywhere, Category="Damage")
	float DeferredDestructionTime = 5.0f;

	UPROPERTY(EditAnywhere, Category="Team")
	uint8 TeamByte = 1;

	UPROPERTY(EditAnywhere, Category="Team")
	FName DeathTag = FName("Dead");

	UPROPERTY()
	TObjectPtr<AShooterWeapon> Weapon;

	UPROPERTY(EditAnywhere, Category="Weapon")
	TSubclassOf<AShooterWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	// ── Aim model ───────────────────────────────────────────────────────
	// Each shot aims at the target's upper chest, leads its velocity, then picks a random
	// direction inside a spread cone. The cone starts wide when the NPC (re)acquires the
	// target, settles to BaseSpreadHalfAngle, and widens with target speed so moving pays.
	// The weapon's own AimVariance is not applied on top (SuppliesAimSpread).

	/** Trace length for shots fired without a target. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=100, Units="cm"))
	float AimRange = 10000.0f;

	/** Aim height above the target's capsule centre, as a fraction of the capsule half height (0.45 = upper chest). */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=-1, ClampMax=1))
	float AimHeightFraction = 0.45f;

	/** Spread cone half-angle once the NPC has settled on a still target. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=0, ClampMax=30, Units="Degrees"))
	float BaseSpreadHalfAngle = 2.0f;

	/** Spread cone half-angle right after the target is acquired or re-acquired. Blends to BaseSpreadHalfAngle over SettleTime. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=0, ClampMax=45, Units="Degrees"))
	float AcquireSpreadHalfAngle = 5.0f;

	/** Seconds of continuous tracking needed to tighten from AcquireSpreadHalfAngle to BaseSpreadHalfAngle. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=0, ClampMax=5, Units="s"))
	float SettleTime = 0.8f;

	/** Tracking counts as lost (and settling restarts) after this long without a shot or with the shot line blocked. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=0, ClampMax=5, Units="s"))
	float TrackingLostTime = 1.0f;

	/** Extra spread half-angle when the target moves at MovingSpreadReferenceSpeed. Scales linearly with target speed. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=0, ClampMax=20, Units="Degrees"))
	float MovingSpreadHalfAngle = 1.5f;

	/** Target speed at which the full MovingSpreadHalfAngle is added. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=1, Units="cm/s"))
	float MovingSpreadReferenceSpeed = 1200.0f;

	/** Upper limit of the speed-based extra spread. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=0, ClampMax=20, Units="Degrees"))
	float MaxMovingSpreadHalfAngle = 3.0f;

	/** Fraction of the ideal projectile lead applied (1 = perfect lead on a constant-velocity target, 0 = aim at the current position). Below 1 so strafing still dodges. Hitscan weapons never lead. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=0, ClampMax=1.5))
	float LeadAccuracy = 0.75f;

	/** Longest projectile flight time the NPC will lead for. */
	UPROPERTY(EditAnywhere, Category="AI|Aim", meta=(ClampMin=0, ClampMax=5, Units="s"))
	float MaxLeadTime = 1.0f;

	TWeakObjectPtr<AActor> CurrentAimTarget;

	bool bIsShooting = false;

	bool bIsDead = false;

	FTimerHandle DeathTimer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Juggle")
	TObjectPtr<UJuggleComponent> JuggleComp;

	UFUNCTION()
	void HandleJuggleStateChanged(bool bJuggled);

	// ── Hit reactions (procedural, no animation assets) ─────────────────

	/** Move Weapon-channel hits from the capsule to the physics asset bodies (needs a physics asset; otherwise the capsule stays hittable). */
	UPROPERTY(EditAnywhere, Category="Hit Reaction")
	bool bPerBoneWeaponHits = true;

	/** Bodies from this bone down blend into physics when the upper body is hit. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction")
	FName FlinchRootBone = FName("spine_01");

	/** Peak physics blend weight of a flinch. Decays linearly to 0 over FlinchDuration. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, ClampMax=1))
	float FlinchBlendWeight = 0.45f;

	/** How long the flinch (or the fallback mesh shake) takes to settle. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0.05, ClampMax=2, Units="s"))
	float FlinchDuration = 0.25f;

	/** Velocity kick given to the hit body per point of damage. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0))
	float FlinchSpeedPerDamage = 14.0f;

	/** Upper limit of the flinch velocity kick. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, Units="cm/s"))
	float MaxFlinchSpeed = 700.0f;

	/** Flinch kick multiplier on headshots. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=1))
	float HeadshotFlinchMultiplier = 1.75f;

	/** Flinch strength while airborne or juggled, so the reaction stays light on top of juggle launches. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, ClampMax=1))
	float AirborneFlinchScale = 0.4f;

	/** Fallback when the mesh has no physics asset: peak mesh tilt away from the shot. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, ClampMax=30, Units="Degrees"))
	float FallbackShakeAngle = 5.0f;

	/** Fallback when the mesh has no physics asset: peak mesh offset along the shot. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, ClampMax=30, Units="cm"))
	float FallbackShakeOffset = 3.0f;

	/** Fallback shake oscillation rate. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=1, ClampMax=40, Units="Hz"))
	float FallbackShakeFrequency = 14.0f;

	/** How long a grounded hit slows the NPC down. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, ClampMax=2, Units="s"))
	float StaggerDuration = 0.3f;

	/** MaxWalkSpeed multiplier during a stagger. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, ClampMax=1))
	float StaggerSpeedScale = 0.35f;

	/** Velocity added to every ragdoll body along the killing shot. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, Units="cm/s"))
	float DeathBodySpeed = 150.0f;

	/** Extra velocity kick on the body the killing shot hit. Scaled by the weapon's PhysicsImpulse relative to 800. */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=0, Units="cm/s"))
	float DeathBoneSpeed = 650.0f;

	/** Death kick multiplier on headshot kills (applied to the head). */
	UPROPERTY(EditAnywhere, Category="Hit Reaction", meta=(ClampMin=1))
	float HeadshotDeathMultiplier = 1.8f;

public:

	FPawnDeathDelegate OnPawnDeath;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	//~Begin IShooterWeaponHolder interface

	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	virtual void AddWeaponRecoil(const FWeaponRecoilParams& Recoil) override;

	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	virtual FVector GetWeaponTargetLocation() override;

	virtual bool SuppliesAimSpread() const override { return true; }

	virtual bool AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

protected:

	void Die();

	void DeferredDestruction();

	/** Plain data about the most recent point hit (no object pointers), used by reactions and the death impulse. */
	struct FLastHitData
	{
		bool bValid = false;
		bool bHeadshot = false;
		FName BoneName = NAME_None;
		FVector Location = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		float Damage = 0.0f;
		float ImpulseScale = 1.0f;
	};

	void PlayHitReaction(const FLastHitData& HitData);
	void UpdateHitReaction();
	void EndHitReaction();
	void ApplyDeathImpulse(const FVector& CarriedVelocity);
	void EndStagger();

	/** Picks the bone whose subtree blends into physics for a hit on HitBone, or NAME_None if none is usable. */
	FName ChooseFlinchBone(FName HitBone) const;

	/** Current spread half-angle in degrees for a shot at AimActor, and updates the settle tracking state. */
	float UpdateAimSpread(const AActor* AimActor, bool bHasLineOfSight);

	/** Upper-chest aim point of AimActor, led by its velocity for projectile weapons fired from ShotOrigin. */
	FVector GetPredictedAimPoint(const AActor* AimActor, const FVector& ShotOrigin) const;

private:

	FLastHitData LastHit;

	bool bHasPhysicsHitBodies = false;

	FName ActiveFlinchBone = NAME_None;
	float ActiveFlinchWeight = 0.0f;
	bool bShakeActive = false;
	float ShakeStrength = 0.0f;
	FVector ShakeLocalDir = FVector::ForwardVector;
	float ReactionStartTime = 0.0f;
	FTimerHandle ReactionTimer;

	FVector BaseMeshLocation = FVector::ZeroVector;
	FRotator BaseMeshRotation = FRotator::ZeroRotator;

	bool bStaggered = false;
	float PreStaggerWalkSpeed = 0.0f;
	FTimerHandle StaggerTimer;

	/** Target the settle timer is tracking (weak: the target may be destroyed between shots). */
	TWeakObjectPtr<const AActor> SettleTarget;

	/** World time continuous tracking of SettleTarget began. */
	float TrackingStartTime = 0.0f;

	/** World time of the last shot that had a clear line to SettleTarget. */
	float LastTrackedShotTime = -1.0f;

protected:

public:

	void StartShooting(AActor* ActorToShoot);

	void StopShooting();
};
