// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponRecoilTypes.h"
#include "WeaponRecoilComponent.generated.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class UCameraComponent;
class AShooterWeapon;

/**
 * Weapon feel + viewmodel composer. This is the ONLY place that writes the first-person arms
 * mesh's relative transform. Every frame (after animation) it:
 *  1. drains aim recoil into the control rotation and recovers it,
 *  2. steps the kick / view-punch / landing springs,
 *  3. builds a procedural offset (look lag, figure-8 bob, move sway, kick, landing, wall pull-back),
 *  4. builds the hip transform (authored relative transform * offset) and the ADS transform
 *     (sight^-1 * offset * eye target) and blends them with the owner's ADS alpha.
 * See Docs/Systems/ADS.md.
 */
UCLASS(ClassGroup="Weapon", meta=(BlueprintSpawnableComponent))
class FPS_MOVEMENT_API UWeaponRecoilComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UWeaponRecoilComponent();

	// ── Aim recoil tuning ───────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=1, ClampMax=100))
	float KickApplySpeed = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=0, ClampMax=1, Units="s"))
	float RecoveryDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=0, ClampMax=90))
	float RecoverySpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=0, ClampMax=45))
	float MaxAccumulatedPitch = 10.0f;

	/** Aim (and view punch) recoil multiplier at full ADS, blended by the gameplay ADS alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=0, ClampMax=2))
	float ADSAimRecoilScale = 0.75f;

	// ── Visual kick tuning ──────────────────────────────────────────────

	/** Positional kick spring stiffness (unit mass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|VisualKick", meta=(ClampMin=10, ClampMax=2000))
	float KickSpringStiffness = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|VisualKick", meta=(ClampMin=1, ClampMax=200))
	float KickSpringDamping = 26.0f;

	/** Rotational kick spring stiffness; softer than the positional one so the rotation lingers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|VisualKick", meta=(ClampMin=10, ClampMax=2000))
	float KickRotationStiffness = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|VisualKick", meta=(ClampMin=1, ClampMax=200))
	float KickRotationDamping = 19.0f;

	/** Viewmodel kick multiplier at full ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|VisualKick", meta=(ClampMin=0, ClampMax=1))
	float ADSVisualKickScale = 0.35f;

	// ── View punch (camera-only) ────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|ViewPunch")
	bool bEnableViewPunch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|ViewPunch", meta=(ClampMin=1, ClampMax=1000))
	float ViewPunchStiffness = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|ViewPunch", meta=(ClampMin=0, ClampMax=100))
	float ViewPunchDamping = 9.0f;

	/**
	 * How much the viewmodel rides the punched view at the hip (1 = gun fixed on screen while the
	 * world kicks) versus while aimed. At 0 the gun stays on the true aim line, so the sight keeps
	 * telling the truth while the view punches around it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|ViewPunch", meta=(ClampMin=0, ClampMax=1))
	float ViewPunchViewmodelFollowHip = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|ViewPunch", meta=(ClampMin=0, ClampMax=1))
	float ViewPunchViewmodelFollowADS = 0.0f;

	// ── Sway tuning ─────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway")
	bool bEnableSway = true;

	/** Look lag: degrees of weapon rotation per degree the lagging facing trails the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=3))
	float LookSwayScale = 0.55f;

	/** Look lag: cm of weapon translation per degree of lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=2))
	float LookSwayPositionScale = 0.15f;

	/** Look lag: the furthest (degrees) the lagging facing may trail the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=15))
	float MaxLookSway = 4.0f;

	/** Look lag chase rate (1/s) and the move-sway smoothing rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=1, ClampMax=30))
	float SwayInterpSpeed = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=10))
	float MoveSwayLateral = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=0.01))
	float MoveSwayVertical = 0.0018f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=10))
	float MaxMoveSwayVertical = 2.0f;

	/** Look-lag / bob multiplier at full ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=1))
	float ADSSwayScale = 0.15f;

	/** Velocity-sway / landing multiplier at full ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=1))
	float ADSMoveSwayScale = 0.4f;

	// ── Viewmodel bob (separate from the camera head bob) ───────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bob")
	bool bEnableBob = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bob", meta=(ClampMin=0.05, ClampMax=3, Units="s"))
	float BobVerticalPeriod = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bob", meta=(ClampMin=0.05, ClampMax=3, Units="s"))
	float BobLateralPeriod = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bob", meta=(ClampMin=0, ClampMax=5, Units="cm"))
	float BobVerticalAmplitude = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bob", meta=(ClampMin=0, ClampMax=5, Units="cm"))
	float BobLateralAmplitude = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bob", meta=(ClampMin=0, ClampMax=10, Units="Degrees"))
	float BobRollAmplitude = 0.6f;

	/** Ground speed that gives a bob weight of 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bob", meta=(ClampMin=1, Units="cm/s"))
	float BobReferenceSpeed = 800.0f;

	// ── Landing inertia ─────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Landing")
	bool bEnableLandingInertia = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Landing", meta=(ClampMin=1, ClampMax=2000))
	float LandingStiffness = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Landing", meta=(ClampMin=0.05, ClampMax=2))
	float LandingDampingRatio = 0.5f;

	/** Downward velocity impulse (cm/s) per cm/s of impact speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Landing", meta=(ClampMin=0, ClampMax=0.1))
	float LandingImpulseScale = 0.008f;

	/** Impact speeds below this produce no landing dip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Landing", meta=(ClampMin=0, Units="cm/s"))
	float LandingMinImpactSpeed = 200.0f;

	// ── Sprint / slide viewmodel pose ───────────────────────────────────

	/** Critical-spring smoothing time for blending into / out of the sprint and slide poses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Pose", meta=(ClampMin=0.01, ClampMax=1, Units="s"))
	float PoseSmoothingTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Pose")
	FRotator SprintPoseRotation = FRotator(0.0f, -10.0f, 15.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Pose")
	FVector SprintPoseLocation = FVector(-4.0f, 2.0f, -3.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Pose")
	FRotator SlidePoseRotation = FRotator(-3.0f, -4.0f, -18.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Pose")
	FVector SlidePoseLocation = FVector(-3.0f, -1.5f, -5.0f);

	// ── Wall avoidance (anti-clip) ──────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|WallAvoid")
	bool bEnableWallAvoidance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|WallAvoid", meta=(ClampMin=0, ClampMax=200, Units="cm"))
	float WallAvoidanceTraceDistance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|WallAvoid", meta=(ClampMin=0, ClampMax=100, Units="cm"))
	float MaxWallPullback = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|WallAvoid", meta=(ClampMin=1, ClampMax=40))
	float WallAvoidanceInterpSpeed = 18.0f;

	// ── API ─────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category="Recoil")
	void AddRecoil(const FWeaponRecoilParams& Params);

	/** Look input after sensitivity scaling; used to let the player pull recoil down. */
	void NotifyAimInput(float YawInput, float PitchInput);

	/** Extra user multiplier on all visual (viewmodel) motion. ADS scaling is automatic. */
	UFUNCTION(BlueprintCallable, Category="Recoil")
	void SetVisualScale(float Scale) { VisualScale = FMath::Clamp(Scale, 0.0f, 1.0f); }

	/** Current camera-only punch rotation. */
	UFUNCTION(BlueprintPure, Category="Recoil")
	FRotator GetViewPunch() const { return FRotator(PunchAngles.X, PunchAngles.Y, PunchAngles.Z); }

protected:

	virtual void BeginPlay() override;

public:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	void ApplyAimRotation(float PitchDegrees, float YawDegrees);

	float AimInputToDegrees(float PitchInput) const;

	/** Owner's visual / gameplay ADS alpha (0 when the owner is not a shooter character). */
	float GetADSAlpha() const;
	float GetADSGameplayAlpha() const;

	AShooterWeapon* GetCurrentWeapon() const;

	/**
	 * Sight frame in the weapon mesh's component space, from the reference pose so weapon
	 * animation (slide / barrel tilt) can't shake the aligned view. Cached per mesh asset.
	 */
	bool GetSightInWeaponComponent(const AShooterWeapon* Weapon, const USkeletalMeshComponent* WeaponMesh, FTransform& OutSight);

	/** Sight frame relative to the arms mesh component (S_m in the design notes). */
	bool ComputeSightInArms(FTransform& OutSightInArms);

	/** Eye relief clamped so the sight stays outside the near clip plane after FirstPersonScale. */
	float GetSafeEyeRelief(const AShooterWeapon* Weapon) const;

	/** carnage.Debug.DrawSight: camera axis vs. the sight line as actually placed this frame. */
	void DrawSightDebug() const;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	/** Authored relative transform of the arms mesh (the hip pose). */
	FTransform HipRelative = FTransform::Identity;

	// Sight cache
	TWeakObjectPtr<const USkeletalMesh> CachedSightMesh;
	TWeakObjectPtr<const AShooterWeapon> CachedSightWeapon;
	FTransform CachedSightInWeapon = FTransform::Identity;
	bool bCachedSightValid = false;

	// Aim recoil
	float PendingPitch = 0.0f;
	float PendingYaw = 0.0f;
	float AccumulatedPitch = 0.0f;
	float TimeSinceLastKick = 1000.0f;

	// Viewmodel kick springs (location in cm; rotation as Pitch/Yaw/Roll degrees)
	FVector KickLocation = FVector::ZeroVector;
	FVector KickLocationVelocity = FVector::ZeroVector;
	FVector KickAngles = FVector::ZeroVector;
	FVector KickAnglesVelocity = FVector::ZeroVector;

	// View punch spring (Pitch/Yaw/Roll degrees)
	FVector PunchAngles = FVector::ZeroVector;
	FVector PunchAnglesVelocity = FVector::ZeroVector;

	// Look lag
	FQuat LaggedFacing = FQuat::Identity;
	bool bLaggedFacingValid = false;

	// Move sway
	FVector MoveSwayOffset = FVector::ZeroVector;

	// Bob
	float BobTime = 0.0f;
	float BobWeight = 0.0f;

	// Landing
	float LandingOffset = 0.0f;
	float LandingVelocity = 0.0f;
	float MaxFallSpeed = 0.0f;
	bool bWasFalling = false;

	// Poses
	float SprintPoseAlpha = 0.0f;
	float SprintPoseVelocity = 0.0f;
	float SlidePoseAlpha = 0.0f;
	float SlidePoseVelocity = 0.0f;

	float CurrentWallPullback = 0.0f;

	float VisualScale = 1.0f;
};
