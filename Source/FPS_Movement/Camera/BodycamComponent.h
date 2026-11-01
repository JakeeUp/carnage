// FPS_Movement: bodycam view look.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Scene.h"
#include "BodycamComponent.generated.h"

class UBodycamCameraModifier;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UDoomMovementComponent;
class AShooterCharacter;
class APlayerCameraManager;
struct FMinimalViewInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBodycamToggledDelegate, bool, bEnabled);

/**
 * Toggleable "bodycam" look for the locally controlled shooter character.
 *
 * Owns the on/off state, the fade alpha, the lens material instance and the lifetime of a
 * UBodycamCameraModifier. Everything is view-only: the control rotation (aim), the camera
 * component (and therefore the first-person arms attached to it) and hitscan are untouched,
 * so the gun leads and the chest-mounted view follows. See Docs/Systems/Bodycam.md.
 *
 * Toggle: IA_ToggleBodycam (B), carnage.Bodycam.Enable, or SetBodycamEnabled.
 * Level default: ACarnageLevelRules::bStartWithBodycam.
 */
UCLASS(ClassGroup="Camera", meta=(BlueprintSpawnableComponent))
class FPS_MOVEMENT_API UBodycamComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBodycamComponent();

	// ── API ─────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category="Bodycam")
	void SetBodycamEnabled(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Bodycam")
	void ToggleBodycam() { SetBodycamEnabled(!bEnabled); }

	/** Target state (true as soon as it is switched on, before the fade completes). */
	UFUNCTION(BlueprintPure, Category="Bodycam")
	bool IsBodycamEnabled() const { return bEnabled; }

	/** Smoothed 0..1 fade between the normal view and the bodycam view. */
	UFUNCTION(BlueprintPure, Category="Bodycam")
	float GetBlendAlpha() const { return BlendAlpha; }

	UPROPERTY(BlueprintAssignable, Category="Bodycam")
	FBodycamToggledDelegate OnBodycamToggled;

	/** Called by UBodycamCameraModifier after the Doom modifier has run. */
	void ApplyToView(float DeltaTime, FMinimalViewInfo& InOutPOV);

	/** Called by UBodycamCameraModifier; fills the weighted PP blend. */
	void ApplyPostProcess(float& OutBlendWeight, FPostProcessSettings& OutSettings) const;

	/**
	 * Feeds a hit into the damage static as if health dropped by HealthDrop (0..1 of max health).
	 * Used by OnDamaged and by carnage.Debug.BodycamDamageTest.
	 */
	UFUNCTION(BlueprintCallable, Category="Bodycam|Damage Static")
	void AddDamageHit(float HealthDrop);

	/** Current damage static impulse, 0..1, before the fade alpha and accessibility scale. */
	UFUNCTION(BlueprintPure, Category="Bodycam|Damage Static")
	float GetDamageStaticImpulse() const { return DamageStatic; }

	// ── Accessibility (each is multiplied by its carnage.Bodycam.* console variable) ──

	/** Lens barrel distortion strength. Console: carnage.Bodycam.Distortion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Accessibility", meta=(ClampMin=0, ClampMax=1))
	float DistortionScale = 1.0f;

	/** All camera rig motion (lag, roll, footsteps, landing, rolling shutter). Console: carnage.Bodycam.Motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Accessibility", meta=(ClampMin=0, ClampMax=1))
	float MotionScale = 1.0f;

	/** Lens chromatic aberration. Console: carnage.Bodycam.ChromaticAberration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Accessibility", meta=(ClampMin=0, ClampMax=1))
	float ChromaticAberrationScale = 1.0f;

	/** Film grain. Console: carnage.Bodycam.Grain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Accessibility", meta=(ClampMin=0, ClampMax=1))
	float GrainScale = 1.0f;

	/** Red damage static and the low-health static layer (flashing content). Console: carnage.Bodycam.DamageStatic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Accessibility", meta=(ClampMin=0, ClampMax=1))
	float DamageStaticScale = 1.0f;

	// ── Toggle ──────────────────────────────────────────────────────────

	/** Fade time for switching the look on or off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam", meta=(ClampMin=0, ClampMax=3, Units="s"))
	float ToggleFadeTime = 0.35f;

	// ── Lens ────────────────────────────────────────────────────────────

	/** Post-process lens material (see Tools/Python/make_bodycam_assets.py). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Bodycam|Lens")
	TSoftObjectPtr<UMaterialInterface> LensMaterial;

	/** Rounded-rect housing border opacity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Lens", meta=(ClampMin=0, ClampMax=1))
	float BorderOpacity = 1.0f;

	/** Rolling-shutter shear (viewport UV per unit of screen height) at RollingShutterReferenceYawRate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Lens", meta=(ClampMin=0, ClampMax=0.2))
	float RollingShutterMaxSkew = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Lens", meta=(ClampMin=1, Units="DegreesPerSecond"))
	float RollingShutterReferenceYawRate = 540.0f;

	/**
	 * Engine post-process applied (weighted by the fade alpha) while the bodycam is on.
	 * Film grain intensities are additionally scaled by GrainScale.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Lens")
	FPostProcessSettings PostProcess;

	// ── Rig ─────────────────────────────────────────────────────────────

	/**
	 * View offset from the eye, in the pawn's yaw frame (X forward, Z up). Defaults to zero: the
	 * first-person arms are attached to the camera component, so moving only the view puts the
	 * lens under the forearm. The chest-cam read comes from the lens, FOV, lag and roll instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(Units="cm"))
	FVector ChestMountOffset = FVector::ZeroVector;

	/** Degrees added on top of the current (movement + ADS) world FOV. Offsets the ~1.3x centre zoom of the lens normalisation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=0, ClampMax=40))
	float FOVBonus = 16.0f;

	/** Critically damped spring rate (rad/s) of the view chasing the control rotation, hip and ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=1, ClampMax=100))
	float RotationSpringOmega = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=1, ClampMax=100))
	float ADSRotationSpringOmega = 28.0f;

	/** Largest angle the view may trail the control rotation (the gun). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=0, ClampMax=30, Units="Degrees"))
	float MaxViewDivergence = 6.0f;

	/** Roll at full strafe speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=0, ClampMax=10, Units="Degrees"))
	float StrafeRoll = 2.5f;

	/** Roll per degree/second of yaw rate, clamped to MaxTurnRoll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=0, ClampMax=0.1))
	float TurnRollPerYawRate = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=0, ClampMax=15, Units="Degrees"))
	float MaxTurnRoll = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=1, ClampMax=40))
	float RollInterpSpeed = 8.0f;

	/** Ground distance per footstep while walking / sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Footsteps", meta=(ClampMin=10, Units="cm"))
	float StrideLength = 230.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Footsteps", meta=(ClampMin=10, Units="cm"))
	float SprintStrideLength = 320.0f;

	/** Approximate peak drop and roll of one footstep jolt (alternating roll sign per foot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Footsteps", meta=(Units="cm"))
	float FootstepDrop = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Footsteps", meta=(Units="Degrees"))
	float FootstepRoll = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Footsteps", meta=(ClampMin=0, ClampMax=5))
	float SprintFootstepScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Footsteps", meta=(ClampMin=1, ClampMax=100))
	float FootstepSpringOmega = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Footsteps", meta=(ClampMin=0.05, ClampMax=2))
	float FootstepSpringDampingRatio = 0.6f;

	/** Approximate peak nod-down pitch and drop of a landing at LandingMaxFallSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Landing", meta=(Units="Degrees"))
	float LandingPitch = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Landing", meta=(Units="cm"))
	float LandingDrop = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Landing", meta=(ClampMin=0, Units="cm/s"))
	float LandingMinFallSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Landing", meta=(ClampMin=1, Units="cm/s"))
	float LandingMaxFallSpeed = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Landing", meta=(ClampMin=1, ClampMax=100))
	float LandingSpringOmega = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Landing", meta=(ClampMin=0.05, ClampMax=2))
	float LandingSpringDampingRatio = 0.5f;

	/** Rig motion multiplier at full ADS (roll, footsteps, landing, view lag clamp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=0, ClampMax=1))
	float ADSMotionScale = 0.25f;

	// ── Damage static (red analog TV static on hits, only visible in bodycam mode) ──

	/** Health drop (fraction of max health) that counts as a light hit: a quick, faint crackle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0.001, ClampMax=1))
	float LightHitHealthDrop = 0.02f;

	/** Health drop (fraction of max health) that counts as a heavy hit: a full-strength burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0.001, ClampMax=1))
	float HeavyHitHealthDrop = 0.2f;

	/** Impulse added by a light hit (a heavy hit adds 1). Smaller drops scale down linearly from this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0, ClampMax=1))
	float LightHitImpulse = 0.3f;

	/** Impulse for a hit that reports a damage source but takes no health (armour absorbed it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0, ClampMax=1))
	float ArmorHitImpulse = 0.2f;

	/** Rise time constant of the visible static towards the stacked impulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0, ClampMax=0.5, Units="s"))
	float DamageStaticAttackTime = 0.025f;

	/** Exponential release: time to fall to 10% after a light hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0.02, ClampMax=3, Units="s"))
	float LightHitReleaseTime = 0.18f;

	/** Exponential release: time to fall to 10% after a heavy hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0.02, ClampMax=3, Units="s"))
	float HeavyHitReleaseTime = 0.45f;

	/** Health fraction below which the faint low-health static layer fades in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0, ClampMax=1))
	float LowHealthStaticThreshold = 0.3f;

	/** LowHealthStatic just under the threshold; it ramps to 1 at zero health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0, ClampMax=1))
	float LowHealthStaticMin = 0.35f;

	/** Impulse at or above which a hit also jolts the camera rig (scaled by Motion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0, ClampMax=1))
	float HitJoltMinImpulse = 0.6f;

	/** Approximate peak nod and roll of the jolt from a full heavy hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0, ClampMax=10, Units="Degrees"))
	float HitJoltPitch = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Damage Static", meta=(ClampMin=0, ClampMax=10, Units="Degrees"))
	float HitJoltRoll = 1.5f;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	/** True when the owner is a pawn driven by a local player controller. */
	APlayerController* GetLocalPlayerController() const;

	/** One-time init once the owner is known to be the local player (level rules, cvar sync). */
	void InitializeForLocalPlayer();

	void EnsureModifier(APlayerController* PC);
	void RemoveModifier();
	void ResetRig();

	void TickRigInputs(float DeltaTime);

	float GetADSAlpha() const;

	/** Effective accessibility scalars (UPROPERTY x console variable), each 0..1. */
	float GetEffectiveMotion() const;
	float GetEffectiveDistortion() const;
	float GetEffectiveChromaticAberration() const;
	float GetEffectiveGrain() const;
	float GetEffectiveDamageStatic() const;

	UFUNCTION()
	void HandleOwnerDamaged(float LifePercent);

	UFUNCTION()
	void HandleOwnerDamageSource(FVector SourceLocation);

	/** Decays the damage static, eases the low-health layer and runs the debug test hit. */
	void TickDamageStatic(float RealDelta);

	UPROPERTY(Transient)
	TObjectPtr<UBodycamCameraModifier> Modifier;

	TWeakObjectPtr<APlayerCameraManager> ModifierOwner;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMID;

	UPROPERTY(Transient)
	TObjectPtr<UDoomMovementComponent> Movement;

	bool bInitialized = false;
	bool bEnabled = false;
	/** Linear fade progress; BlendAlpha is its SmoothStep. */
	float FadeLinear = 0.0f;
	float BlendAlpha = 0.0f;
	int32 LastSeenEnableCVar = 0;

	// View lag: (view - control) in pitch/yaw degrees, with velocity.
	FVector2D LagOffset = FVector2D::ZeroVector;
	FVector2D LagVelocity = FVector2D::ZeroVector;
	FRotator LastControlRotation = FRotator::ZeroRotator;
	bool bHasLastControlRotation = false;

	/** Smoothed yaw rate (deg/s) of the control rotation. */
	float YawRate = 0.0f;
	float CurrentRoll = 0.0f;

	// Footsteps: X = Z drop (cm), Y = roll (deg).
	FVector2D StepOffset = FVector2D::ZeroVector;
	FVector2D StepVelocity = FVector2D::ZeroVector;
	float StrideAccumulator = 0.0f;
	bool bLeftFoot = false;

	// Landing: X = pitch (deg, positive nods down), Y = Z drop (cm).
	FVector2D LandOffset = FVector2D::ZeroVector;
	FVector2D LandVelocity = FVector2D::ZeroVector;
	bool bWasFalling = false;
	float MaxFallSpeed = 0.0f;

	// Damage static: stacked target (decays exponentially) and the visible value chasing it.
	float DamageImpulse = 0.0f;
	float DamageStatic = 0.0f;
	float DamageReleaseTime = 0.0f;
	float LastHealthPercent = 1.0f;
	float LowHealthStatic = 0.0f;
	/** GFrameCounter of the last health-drop hit, so OnDamageSource doesn't count it twice. */
	uint64 LastHealthHitFrame = 0;

	/** carnage.Debug.BodycamDamageTest: queued test hit (health drop) and its remaining delay. */
	float PendingTestHit = 0.0f;
	float PendingTestDelay = 0.0f;
};
