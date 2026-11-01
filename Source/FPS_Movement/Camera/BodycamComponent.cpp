// FPS_Movement: bodycam view look.

#include "BodycamComponent.h"
#include "BodycamCameraModifier.h"
#include "CarnageLevelRules.h"
#include "DoomMovementComponent.h"
#include "ShooterCharacter.h"
#include "ViewSpringMath.h"

#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"

// ── Console variables (accessibility + toggle) ──────────────────────────────

static TAutoConsoleVariable<int32> CVarBodycamEnable(
	TEXT("carnage.Bodycam.Enable"),
	0,
	TEXT("1 = bodycam view look on for the local player, 0 = off. Kept in sync with the B toggle."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBodycamDistortion(
	TEXT("carnage.Bodycam.Distortion"),
	1.0f,
	TEXT("Bodycam lens distortion multiplier, 0..1."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBodycamMotion(
	TEXT("carnage.Bodycam.Motion"),
	1.0f,
	TEXT("Bodycam camera rig motion multiplier (lag, roll, footsteps, landing, rolling shutter), 0..1."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBodycamChromaticAberration(
	TEXT("carnage.Bodycam.ChromaticAberration"),
	1.0f,
	TEXT("Bodycam lens chromatic aberration multiplier, 0..1."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBodycamGrain(
	TEXT("carnage.Bodycam.Grain"),
	1.0f,
	TEXT("Bodycam film grain multiplier, 0..1."),
	ECVF_Default);

namespace BodycamLocal
{
	static const FName ParamDistortion(TEXT("Distortion"));
	static const FName ParamChromaticAberration(TEXT("ChromaticAberration"));
	static const FName ParamBorder(TEXT("Border"));
	static const FName ParamRollingShutter(TEXT("RollingShutter"));

	/** Level rules are a level-start default: apply them once per world, not on every respawn. */
	static TWeakObjectPtr<UWorld> LevelRulesAppliedWorld;

	static float Saturate(float V) { return FMath::Clamp(V, 0.0f, 1.0f); }

	/**
	 * Velocity kick that gives a peak displacement of roughly Amplitude on a spring of rate
	 * Omega with a damping ratio around 0.5..0.6 (peak ~= 0.5 * v0 / Omega).
	 */
	static double KickVelocity(float Amplitude, float Omega) { return 2.0 * Amplitude * Omega; }
}

UBodycamComponent::UBodycamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	LensMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Carnage/Bodycam/M_BodycamLens.M_BodycamLens")));

	// A deliberately obvious body-worn camera grade: heavy sensor noise (strongest in shadows),
	// crunchy over-sharpening, strong vignette, cool / desaturated / contrasty with lifted
	// blacks, slow auto exposure, moderate motion blur. Chromatic aberration comes from the lens
	// material, not SceneFringe.
	PostProcess.bOverride_FilmGrainIntensity = true;
	PostProcess.FilmGrainIntensity = 0.5f;
	PostProcess.bOverride_FilmGrainIntensityShadows = true;
	PostProcess.FilmGrainIntensityShadows = 1.0f;
	PostProcess.bOverride_FilmGrainIntensityMidtones = true;
	PostProcess.FilmGrainIntensityMidtones = 0.55f;
	PostProcess.bOverride_FilmGrainIntensityHighlights = true;
	PostProcess.FilmGrainIntensityHighlights = 0.2f;
	PostProcess.bOverride_FilmGrainTexelSize = true;
	PostProcess.FilmGrainTexelSize = 1.6f;

	PostProcess.bOverride_Sharpen = true;
	PostProcess.Sharpen = 2.2f;

	PostProcess.bOverride_VignetteIntensity = true;
	PostProcess.VignetteIntensity = 0.9f;

	PostProcess.bOverride_BloomIntensity = true;
	PostProcess.BloomIntensity = 1.5f;

	// Below the 6500 K default: the scene's white reads as blue, i.e. a cool cast.
	PostProcess.bOverride_WhiteTemp = true;
	PostProcess.WhiteTemp = 5400.0f;

	PostProcess.bOverride_ColorSaturation = true;
	PostProcess.ColorSaturation = FVector4(0.72f, 0.74f, 0.78f, 1.0f);

	PostProcess.bOverride_ColorContrast = true;
	PostProcess.ColorContrast = FVector4(1.25f, 1.25f, 1.25f, 1.0f);

	// Lifted, slightly blue blacks.
	PostProcess.bOverride_ColorOffset = true;
	PostProcess.ColorOffset = FVector4(0.012f, 0.016f, 0.024f, 0.0f);

	PostProcess.bOverride_AutoExposureSpeedUp = true;
	PostProcess.AutoExposureSpeedUp = 0.8f;
	PostProcess.bOverride_AutoExposureSpeedDown = true;
	PostProcess.AutoExposureSpeedDown = 0.5f;

	PostProcess.bOverride_MotionBlurAmount = true;
	PostProcess.MotionBlurAmount = 0.5f;
}

// ── Lifetime ────────────────────────────────────────────────────────────────

void UBodycamComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Movement = Cast<UDoomMovementComponent>(Character->GetCharacterMovement());
	}

	// The owner advances its ADS alpha in its own tick; read it after that.
	if (AActor* Owner = GetOwner())
	{
		PrimaryComponentTick.AddPrerequisite(Owner, Owner->PrimaryActorTick);
	}

	// Possession can happen after BeginPlay, so the local-player setup (level rules, console
	// variable sync) runs on the first tick that finds a local player controller.
	if (GetLocalPlayerController())
	{
		InitializeForLocalPlayer();
	}
}

void UBodycamComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveModifier();
	Super::EndPlay(EndPlayReason);
}

APlayerController* UBodycamComponent::GetLocalPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return (PC && PC->IsLocalController()) ? PC : nullptr;
}

void UBodycamComponent::InitializeForLocalPlayer()
{
	bInitialized = true;

	// The console variable carries the state across respawns (each respawn is a new pawn).
	bool bStart = CVarBodycamEnable.GetValueOnGameThread() != 0;

	UWorld* World = GetWorld();
	if (World && BodycamLocal::LevelRulesAppliedWorld.Get() != World)
	{
		BodycamLocal::LevelRulesAppliedWorld = World;
		if (const ACarnageLevelRules* Rules = ACarnageLevelRules::Find(this))
		{
			bStart |= Rules->bStartWithBodycam;
		}
	}

	LastSeenEnableCVar = CVarBodycamEnable.GetValueOnGameThread();
	if (bStart)
	{
		SetBodycamEnabled(true);
		// Spawning into the look: no fade.
		FadeLinear = 1.0f;
		BlendAlpha = 1.0f;
	}
}

// ── Toggle ──────────────────────────────────────────────────────────────────

void UBodycamComponent::SetBodycamEnabled(bool bEnable)
{
	if (GetLocalPlayerController())
	{
		const int32 Value = bEnable ? 1 : 0;
		if (CVarBodycamEnable.GetValueOnGameThread() != Value)
		{
			// SetByConsole so a later console command can still override it without a priority warning.
			CVarBodycamEnable.AsVariable()->Set(Value, ECVF_SetByConsole);
		}
		LastSeenEnableCVar = Value;
	}

	if (bEnabled == bEnable)
	{
		return;
	}

	bEnabled = bEnable;
	OnBodycamToggled.Broadcast(bEnabled);
}

// ── Modifier ────────────────────────────────────────────────────────────────

void UBodycamComponent::EnsureModifier(APlayerController* PC)
{
	APlayerCameraManager* CamMgr = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!CamMgr)
	{
		return;
	}

	if (Modifier && ModifierOwner.Get() != CamMgr)
	{
		RemoveModifier();
	}

	if (!Modifier)
	{
		Modifier = Cast<UBodycamCameraModifier>(CamMgr->AddNewCameraModifier(UBodycamCameraModifier::StaticClass()));
		if (Modifier)
		{
			Modifier->Source = this;
			ModifierOwner = CamMgr;
		}
		ResetRig();
	}

	if (!LensMID)
	{
		if (UMaterialInterface* Base = LensMaterial.LoadSynchronous())
		{
			LensMID = UMaterialInstanceDynamic::Create(Base, this);
		}
	}
}

void UBodycamComponent::RemoveModifier()
{
	if (Modifier)
	{
		Modifier->Source = nullptr;
		if (APlayerCameraManager* CamMgr = ModifierOwner.Get())
		{
			CamMgr->RemoveCameraModifier(Modifier);
		}
	}
	Modifier = nullptr;
	ModifierOwner = nullptr;
}

void UBodycamComponent::ResetRig()
{
	LagOffset = FVector2D::ZeroVector;
	LagVelocity = FVector2D::ZeroVector;
	bHasLastControlRotation = false;
	YawRate = 0.0f;
	CurrentRoll = 0.0f;
	StepOffset = FVector2D::ZeroVector;
	StepVelocity = FVector2D::ZeroVector;
	StrideAccumulator = 0.0f;
	LandOffset = FVector2D::ZeroVector;
	LandVelocity = FVector2D::ZeroVector;
}

// ── Tick ────────────────────────────────────────────────────────────────────

void UBodycamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APlayerController* PC = GetLocalPlayerController();
	if (!PC)
	{
		RemoveModifier();
		return;
	}

	if (!bInitialized)
	{
		InitializeForLocalPlayer();
	}

	// Console variable -> state (ExecCmds, console, settings menus).
	const int32 CVarValue = CVarBodycamEnable.GetValueOnGameThread();
	if (CVarValue != LastSeenEnableCVar)
	{
		LastSeenEnableCVar = CVarValue;
		SetBodycamEnabled(CVarValue != 0);
	}

	// Fade on unscaled time so kill hit-stop doesn't stall the toggle.
	const float RealDelta = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.1f);
	const float Step = ToggleFadeTime > UE_KINDA_SMALL_NUMBER ? RealDelta / ToggleFadeTime : 1.0f;
	FadeLinear = FMath::Clamp(FadeLinear + (bEnabled ? Step : -Step), 0.0f, 1.0f);
	BlendAlpha = FMath::SmoothStep(0.0f, 1.0f, FadeLinear);

	if (bEnabled || BlendAlpha > 0.0f)
	{
		EnsureModifier(PC);
		TickRigInputs(DeltaTime);
	}
	else
	{
		RemoveModifier();
	}
}

void UBodycamComponent::TickRigInputs(float DeltaTime)
{
	if (!Movement)
	{
		return;
	}

	// ── Landing jolt, scaled by fall speed ──────────────────────────────
	const bool bFalling = Movement->IsFalling();
	if (bFalling)
	{
		MaxFallSpeed = FMath::Max(MaxFallSpeed, static_cast<float>(-Movement->Velocity.Z));
	}
	else if (bWasFalling)
	{
		if (MaxFallSpeed > LandingMinFallSpeed)
		{
			const float Severity = BodycamLocal::Saturate((MaxFallSpeed - LandingMinFallSpeed) / FMath::Max(LandingMaxFallSpeed - LandingMinFallSpeed, 1.0f));
			LandVelocity.X += BodycamLocal::KickVelocity(LandingPitch * Severity, LandingSpringOmega);
			LandVelocity.Y -= BodycamLocal::KickVelocity(LandingDrop * Severity, LandingSpringOmega);
		}
		MaxFallSpeed = 0.0f;
		StrideAccumulator = 0.0f;
	}
	bWasFalling = bFalling;

	// ── Footsteps from distance travelled on the ground ─────────────────
	const bool bWalking = Movement->IsMovingOnGround() && !Movement->IsSliding() && !Movement->IsMantling();
	const float Speed = Movement->GetHorizontalSpeed();
	if (bWalking && Speed > 50.0f)
	{
		const bool bSprinting = Movement->IsSprinting();
		const float Stride = FMath::Max(bSprinting ? SprintStrideLength : StrideLength, 10.0f);
		StrideAccumulator += Speed * DeltaTime;
		if (StrideAccumulator >= Stride)
		{
			StrideAccumulator = FMath::Fmod(StrideAccumulator, Stride);
			bLeftFoot = !bLeftFoot;
			const float Scale = bSprinting ? SprintFootstepScale : 1.0f;
			StepVelocity.X -= BodycamLocal::KickVelocity(FootstepDrop * Scale, FootstepSpringOmega);
			StepVelocity.Y += BodycamLocal::KickVelocity(FootstepRoll * Scale, FootstepSpringOmega) * (bLeftFoot ? 1.0 : -1.0);
		}
	}
	else if (!bWalking)
	{
		// Restart the cadence half a stride in, so the first step after landing / sliding comes quickly.
		StrideAccumulator = 0.5f * StrideLength;
	}
}

// ── View ────────────────────────────────────────────────────────────────────

float UBodycamComponent::GetADSAlpha() const
{
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
	return Shooter ? Shooter->GetADSAlpha() : 0.0f;
}

float UBodycamComponent::GetEffectiveMotion() const
{
	return BodycamLocal::Saturate(MotionScale) * BodycamLocal::Saturate(CVarBodycamMotion.GetValueOnGameThread());
}

float UBodycamComponent::GetEffectiveDistortion() const
{
	return BodycamLocal::Saturate(DistortionScale) * BodycamLocal::Saturate(CVarBodycamDistortion.GetValueOnGameThread());
}

float UBodycamComponent::GetEffectiveChromaticAberration() const
{
	return BodycamLocal::Saturate(ChromaticAberrationScale) * BodycamLocal::Saturate(CVarBodycamChromaticAberration.GetValueOnGameThread());
}

float UBodycamComponent::GetEffectiveGrain() const
{
	return BodycamLocal::Saturate(GrainScale) * BodycamLocal::Saturate(CVarBodycamGrain.GetValueOnGameThread());
}

void UBodycamComponent::ApplyToView(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	DeltaTime = FMath::Clamp(DeltaTime, 0.0f, 0.1f);
	const FRotator Control = Pawn->GetControlRotation();
	const float ADS = GetADSAlpha();
	const float ADSMotion = FMath::Lerp(1.0f, ADSMotionScale, ADS);
	const float Motion = GetEffectiveMotion();

	// ── Track the control rotation (the gun) ────────────────────────────
	if (!bHasLastControlRotation)
	{
		LastControlRotation = Control;
		bHasLastControlRotation = true;
	}
	const float DeltaPitch = FRotator::NormalizeAxis(Control.Pitch - LastControlRotation.Pitch);
	const float DeltaYaw = FRotator::NormalizeAxis(Control.Yaw - LastControlRotation.Yaw);
	LastControlRotation = Control;

	if (FMath::Abs(DeltaYaw) > 90.0f || FMath::Abs(DeltaPitch) > 90.0f)
	{
		// Teleport / respawn / scripted snap: don't swing the camera across the room.
		LagOffset = FVector2D::ZeroVector;
		LagVelocity = FVector2D::ZeroVector;
	}
	else
	{
		// The view stays where it was; the spring below pulls it after the control rotation.
		LagOffset -= FVector2D(DeltaPitch, DeltaYaw);
	}

	if (DeltaTime > UE_KINDA_SMALL_NUMBER)
	{
		YawRate = FMath::Lerp(YawRate, DeltaYaw / DeltaTime, 1.0f - FMath::Exp(-12.0f * DeltaTime));
	}

	// ── Critically damped view-follow spring, tighter while aiming ──────
	const float Omega = FMath::Lerp(RotationSpringOmega, ADSRotationSpringOmega, ADS);
	ViewSpring::StepImpulse(LagOffset, LagVelocity, Omega * Omega, 2.0f * Omega, DeltaTime);

	const double MaxDivergence = MaxViewDivergence * ADSMotion;
	const double LagLength = LagOffset.Size();
	if (LagLength > MaxDivergence && LagLength > UE_KINDA_SMALL_NUMBER)
	{
		const FVector2D Dir = LagOffset / LagLength;
		LagOffset = Dir * MaxDivergence;
		const double Outward = FVector2D::DotProduct(LagVelocity, Dir);
		if (Outward > 0.0)
		{
			LagVelocity -= Dir * Outward;
		}
	}

	// ── Impulse springs (footsteps, landing) ────────────────────────────
	ViewSpring::StepImpulse(StepOffset, StepVelocity, FootstepSpringOmega * FootstepSpringOmega,
		ViewSpring::DampingFromRatio(FootstepSpringOmega * FootstepSpringOmega, FootstepSpringDampingRatio), DeltaTime);
	ViewSpring::StepImpulse(LandOffset, LandVelocity, LandingSpringOmega * LandingSpringOmega,
		ViewSpring::DampingFromRatio(LandingSpringOmega * LandingSpringOmega, LandingSpringDampingRatio), DeltaTime);

	// ── Strafe + turn roll ──────────────────────────────────────────────
	float TargetRoll = FMath::Clamp(-YawRate * TurnRollPerYawRate, -MaxTurnRoll, MaxTurnRoll);
	if (Movement && Movement->MaxWalkSpeed > 0.0f)
	{
		const float Lateral = FVector::DotProduct(Movement->Velocity, Pawn->GetActorRightVector());
		// Same sign convention as UDoomCameraEffectsComponent's strafe tilt.
		TargetRoll -= FMath::Clamp(Lateral / Movement->MaxWalkSpeed, -1.0f, 1.0f) * StrafeRoll;
	}
	CurrentRoll = FMath::FInterpTo(CurrentRoll, TargetRoll, DeltaTime, RollInterpSpeed);

	const float Alpha = BlendAlpha;
	if (Alpha <= 0.0f)
	{
		return;
	}

	// ── Compose onto the incoming POV ───────────────────────────────────
	// Only the control-rotation part of the view is lagged; tilt, punch and shakes already in the
	// POV pass through unchanged. LagDelta is the lag expressed in the control rotation's frame.
	const float LagScale = Alpha * Motion;
	const FRotator Lagged(Control.Pitch + LagOffset.X * LagScale, Control.Yaw + LagOffset.Y * LagScale, Control.Roll);
	const FQuat LagDelta = Control.Quaternion().Inverse() * Lagged.Quaternion();

	const float RigScale = Alpha * Motion * ADSMotion;
	const FRotator Rig(
		-LandOffset.X * RigScale,                          // landing nods the view down
		0.0f,
		(CurrentRoll + StepOffset.Y) * RigScale);

	InOutPOV.Rotation = (InOutPOV.Rotation.Quaternion() * LagDelta * Rig.Quaternion()).Rotator();

	// Chest mount: lower and forward of the eye in the yaw frame. Fades out while aiming so the
	// view returns to the eye and the procedural sight alignment (built on the camera component)
	// lines up with the screen centre again.
	const FRotator YawOnly(0.0f, Control.Yaw, 0.0f);
	InOutPOV.Location += YawOnly.RotateVector(ChestMountOffset) * (Alpha * (1.0f - ADS));
	InOutPOV.Location.Z += (StepOffset.X + LandOffset.Y) * RigScale;

	InOutPOV.FOV += FOVBonus * Alpha;
}

void UBodycamComponent::ApplyPostProcess(float& OutBlendWeight, FPostProcessSettings& OutSettings) const
{
	if (BlendAlpha <= 0.0f)
	{
		OutBlendWeight = 0.0f;
		return;
	}

	OutSettings = PostProcess;
	OutSettings.FilmGrainIntensity *= GetEffectiveGrain();
	// Motion blur is camera motion too, so the Motion accessibility scalar turns it down.
	OutSettings.MotionBlurAmount *= GetEffectiveMotion();

	if (LensMID)
	{
		const float ADSMotion = FMath::Lerp(1.0f, ADSMotionScale, GetADSAlpha());
		const float Shear = FMath::Clamp(YawRate / FMath::Max(RollingShutterReferenceYawRate, 1.0f), -1.0f, 1.0f)
			* RollingShutterMaxSkew * GetEffectiveMotion() * ADSMotion;

		LensMID->SetScalarParameterValue(BodycamLocal::ParamDistortion, GetEffectiveDistortion());
		LensMID->SetScalarParameterValue(BodycamLocal::ParamChromaticAberration, GetEffectiveChromaticAberration());
		LensMID->SetScalarParameterValue(BodycamLocal::ParamBorder, BodycamLocal::Saturate(BorderOpacity));
		LensMID->SetScalarParameterValue(BodycamLocal::ParamRollingShutter, Shear);

		// Weight 1 here; the whole PP blend is weighted by BlendAlpha, which lerps the material's
		// parameters from the (neutral) base material defaults.
		OutSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, LensMID));
	}

	OutBlendWeight = BlendAlpha;
}
