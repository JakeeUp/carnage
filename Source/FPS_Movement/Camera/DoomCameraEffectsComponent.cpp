// Copyright Epic Games, Inc. All Rights Reserved.

#include "DoomCameraEffectsComponent.h"
#include "DoomMovementComponent.h"
#include "DoomCameraModifier.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UDoomCameraEffectsComponent::UDoomCameraEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UDoomCameraEffectsComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		MoveComp = Cast<UDoomMovementComponent>(Character->GetCharacterMovement());
		CameraComp = Character->FindComponentByClass<UCameraComponent>();

		if (CameraComp)
		{
			BaseCameraRelativeLocation = CameraComp->GetRelativeLocation();
			BaseFirstPersonFOV = CameraComp->FirstPersonFieldOfView;
		}
	}

	// The owner pushes the ADS alpha from its own tick; make sure that happens first.
	if (AActor* Owner = GetOwner())
	{
		PrimaryComponentTick.AddPrerequisite(Owner, Owner->PrimaryActorTick);
	}

	CurrentFOV = BaseFOV;
	CurrentMovementFOV = BaseFOV;
}

void UDoomCameraEffectsComponent::SetADSState(float Alpha, float WorldFOV, float FirstPersonFOV)
{
	ADSAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	ADSWorldFOVTarget = WorldFOV;
	ADSFirstPersonFOVTarget = FirstPersonFOV;
}

void UDoomCameraEffectsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CameraModifier && OwningCameraManager.IsValid())
	{
		OwningCameraManager->RemoveCameraModifier(CameraModifier);
	}
	CameraModifier = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UDoomCameraEffectsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!MoveComp) return;

	if (!CameraModifier)
	{
		if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
			{
				if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
				{
					CameraModifier = Cast<UDoomCameraModifier>(
						CamMgr->AddNewCameraModifier(UDoomCameraModifier::StaticClass()));
					OwningCameraManager = CamMgr;
				}
			}
		}

		if (!CameraModifier) return;
	}

	// ── Movement FOV (sprint / slide / dash) ────────────────────────────
	// Smoothed on its own; ADS is blended on top afterwards so neither overrides the other.
	float DesiredFOV = BaseFOV;
	float FOVSpeed = FOVInterpSpeed;

	if (MoveComp->IsDashing())
	{
		DesiredFOV = DashFOV;
		FOVSpeed = FOVInterpSpeed * 3.0f;
	}
	else if (MoveComp->IsSliding())
	{
		DesiredFOV = SlideFOV;
	}
	else if (MoveComp->IsSprinting())
	{
		DesiredFOV = SprintFOV;
	}
	else if (CurrentMovementFOV > BaseFOV + 1.0f)
	{
		FOVSpeed = DashFOVReturnSpeed;
	}

	const float SpeedRatio = MoveComp->GetSpeedRatio();
	DesiredFOV = FMath::Max(DesiredFOV, BaseFOV + SpeedRatio * (SprintFOV - BaseFOV));

	CurrentMovementFOV = FMath::FInterpTo(CurrentMovementFOV, DesiredFOV, DeltaTime, FOVSpeed);

	// ── ADS blend: one alpha drives both the world and the first-person FOV ──
	const float AimWorldFOV = ADSWorldFOVTarget > 0.0f ? ADSWorldFOVTarget : ADSFOV;
	const float AimFirstPersonFOV = ADSFirstPersonFOVTarget > 0.0f ? ADSFirstPersonFOVTarget : ADSFirstPersonFOV;

	CurrentFOV = FMath::Lerp(CurrentMovementFOV, AimWorldFOV, ADSAlpha);

	if (CameraComp)
	{
		// The engine renders first-person primitives with their own FOV and cancels the world
		// FOV for them, so this is the only thing that magnifies the gun while aiming.
		CameraComp->SetFirstPersonFieldOfView(FMath::Lerp(BaseFirstPersonFOV, AimFirstPersonFOV, ADSAlpha));
	}

	// ── Determine target tilt ───────────────────────────────────────────
	float DesiredTilt = 0.0f;

	if (MoveComp->IsWallRunning())
	{
		DesiredTilt = MoveComp->IsWallRunRight() ? -WallRunTiltAngle : WallRunTiltAngle;
	}
	else if (MoveComp->IsSliding())
	{
		DesiredTilt = SlideTiltAngle;
	}
	else
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		if (Character)
		{
			const FVector Vel = MoveComp->Velocity;
			const FVector Right = Character->GetActorRightVector();
			const float LateralSpeed = FVector::DotProduct(Vel, Right);
			const float MaxLateral = MoveComp->MaxWalkSpeed;

			if (MaxLateral > 0.0f)
			{
				const float Ratio = FMath::Clamp(LateralSpeed / MaxLateral, -1.0f, 1.0f);
				DesiredTilt = -Ratio * StrafeTiltAngle;
			}
		}
	}

	TargetTiltRoll = DesiredTilt;
	CurrentTiltRoll = FMath::FInterpTo(CurrentTiltRoll, TargetTiltRoll, DeltaTime, TiltInterpSpeed);

	// ── Head bob ────────────────────────────────────────────────────────
	float BobOffset = 0.0f;

	if (bEnableHeadBob && MoveComp->IsMovingOnGround() && !MoveComp->IsSliding() && !MoveComp->IsMantling())
	{
		const float Speed = MoveComp->GetHorizontalSpeed();
		if (Speed > 50.0f)
		{
			BobTimer += DeltaTime * BobFrequency;

			const float Amplitude = MoveComp->IsSprinting() ? SprintBobAmplitude : WalkBobAmplitude;
			const float SpeedFactor = FMath::Clamp(Speed / MoveComp->SprintSpeed, 0.0f, 1.0f);

			BobOffset = FMath::Sin(BobTimer) * Amplitude * SpeedFactor;
		}
		else
		{
			BobTimer = 0.0f;
		}
	}
	else
	{
		BobTimer = 0.0f;
	}

	CurrentBobOffset = FMath::FInterpTo(CurrentBobOffset, BobOffset, DeltaTime, 15.0f);

	// ── Landing impact ──────────────────────────────────────────────────
	const bool bFalling = MoveComp->IsFalling();

	if (bFalling)
	{
		const float DownSpeed = FMath::Max(0.0f, -MoveComp->Velocity.Z);
		FallSpeed = FMath::Max(FallSpeed, DownSpeed);
	}

	if (bWasFalling && !bFalling && bEnableLandingImpact)
	{
		if (FallSpeed > LandingMinFallSpeed)
		{
			const float ImpactRatio = FMath::Clamp(FallSpeed / 1200.0f, 0.0f, 1.0f);
			LandingOffset = -LandingDropAmount * ImpactRatio;
		}
		FallSpeed = 0.0f;
	}
	else if (!bFalling)
	{
		FallSpeed = 0.0f;
	}

	bWasFalling = bFalling;

	if (LandingOffset < -0.01f)
	{
		LandingOffset = FMath::FInterpTo(LandingOffset, 0.0f, DeltaTime, LandingRecoverSpeed);
	}
	else
	{
		LandingOffset = 0.0f;
	}

	// ── Slide camera dip + forward lean ─────────────────────────────────
	const float TargetSlideOffset = MoveComp->IsSliding() ? -SlideCameraDip : 0.0f;
	// The forward lean rotates the view but not the viewmodel, which would push the sight off
	// centre, so it fades out while aiming.
	const float TargetSlidePitch  = MoveComp->IsSliding() ?  SlideForwardPitch * (1.0f - ADSAlpha) : 0.0f;

	CurrentSlideOffset = FMath::FInterpTo(CurrentSlideOffset, TargetSlideOffset, DeltaTime, SlideTransitionSpeed);
	CurrentSlidePitch  = FMath::FInterpTo(CurrentSlidePitch,  TargetSlidePitch,  DeltaTime, SlideTransitionSpeed);

	// ── Apply vertical offsets to camera COMPONENT (so arms follow as children) ──
	const float TotalVerticalOffset = CurrentBobOffset + LandingOffset + CurrentSlideOffset;
	if (CameraComp)
	{
		FVector NewLoc = BaseCameraRelativeLocation;
		NewLoc.Z += TotalVerticalOffset;
		CameraComp->SetRelativeLocation(NewLoc);

		if (bEnableSpeedMotionBlur)
		{
			const float TargetBlur = FMath::Lerp(BaseMotionBlur, MaxMotionBlur, MoveComp->GetSpeedRatio());
			CurrentMotionBlur = FMath::FInterpTo(CurrentMotionBlur, TargetBlur, DeltaTime, MotionBlurInterpSpeed);
			CameraComp->PostProcessSettings.bOverride_MotionBlurAmount = true;
			CameraComp->PostProcessSettings.MotionBlurAmount = CurrentMotionBlur;
		}
	}

	// ── Push remaining values to camera modifier (FOV, roll, pitch only) ──
	CameraModifier->DesiredFOV     = CurrentFOV;
	CameraModifier->TiltRoll       = CurrentTiltRoll;
	CameraModifier->PitchOffset    = CurrentSlidePitch;
	CameraModifier->VerticalOffset = 0.0f;
}
