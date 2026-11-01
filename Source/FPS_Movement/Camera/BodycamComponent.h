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

	/** Chest mount offset from the eye, in the pawn's yaw frame (X forward, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(Units="cm"))
	FVector ChestMountOffset = FVector(10.0f, 0.0f, -20.0f);

	/** Degrees added on top of the current (movement + ADS) world FOV. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bodycam|Rig", meta=(ClampMin=0, ClampMax=40))
	float FOVBonus = 12.0f;

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
};
