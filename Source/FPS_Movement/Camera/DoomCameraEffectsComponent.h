// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DoomCameraEffectsComponent.generated.h"

class UDoomMovementComponent;
class UDoomCameraModifier;

DECLARE_DELEGATE_RetVal(bool, FDoomAimStateProvider);

UCLASS(ClassGroup="Camera", meta=(BlueprintSpawnableComponent))
class FPS_MOVEMENT_API UDoomCameraEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UDoomCameraEffectsComponent();

	FDoomAimStateProvider AimStateProvider;

	// ── FOV ─────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|FOV")
	float BaseFOV = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|FOV")
	float SprintFOV = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|FOV")
	float SlideFOV = 105.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|FOV")
	float DashFOV = 115.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|FOV")
	float ADSFOV = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|FOV", meta=(ClampMin=1, ClampMax=30))
	float ADSInterpSpeed = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|FOV", meta=(ClampMin=1, ClampMax=30))
	float FOVInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|FOV", meta=(ClampMin=1, ClampMax=50))
	float DashFOVReturnSpeed = 6.0f;

	// ── Tilt ────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Tilt", meta=(ClampMin=0, ClampMax=10))
	float StrafeTiltAngle = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Tilt", meta=(ClampMin=0, ClampMax=25))
	float WallRunTiltAngle = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Tilt", meta=(ClampMin=0, ClampMax=10))
	float SlideTiltAngle = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Tilt", meta=(ClampMin=1, ClampMax=30))
	float TiltInterpSpeed = 10.0f;

	// ── Slide ───────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Slide", meta=(ClampMin=0, ClampMax=100, Units="cm"))
	float SlideCameraDip = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Slide", meta=(ClampMin=0, ClampMax=15))
	float SlideForwardPitch = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Slide", meta=(ClampMin=1, ClampMax=30))
	float SlideTransitionSpeed = 8.0f;

	// ── Head Bob ────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|HeadBob")
	bool bEnableHeadBob = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|HeadBob", meta=(ClampMin=0, ClampMax=10))
	float WalkBobAmplitude = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|HeadBob", meta=(ClampMin=0, ClampMax=10))
	float SprintBobAmplitude = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|HeadBob", meta=(ClampMin=0, ClampMax=30))
	float BobFrequency = 12.0f;

	// ── Landing ─────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Landing")
	bool bEnableLandingImpact = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Landing", meta=(ClampMin=0, ClampMax=30))
	float LandingDropAmount = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Landing", meta=(ClampMin=1, ClampMax=30))
	float LandingRecoverSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|Landing", meta=(ClampMin=0))
	float LandingMinFallSpeed = 300.0f;

	// ── Motion Blur ─────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|MotionBlur")
	bool bEnableSpeedMotionBlur = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|MotionBlur", meta=(ClampMin=0, ClampMax=1))
	float BaseMotionBlur = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|MotionBlur", meta=(ClampMin=0, ClampMax=1))
	float MaxMotionBlur = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DoomCamera|MotionBlur", meta=(ClampMin=1, ClampMax=30))
	float MotionBlurInterpSpeed = 8.0f;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	UPROPERTY()
	TObjectPtr<UDoomMovementComponent> MoveComp;

	UPROPERTY()
	TObjectPtr<UDoomCameraModifier> CameraModifier;

	TWeakObjectPtr<class APlayerCameraManager> OwningCameraManager;

	UPROPERTY()
	TObjectPtr<class UCameraComponent> CameraComp;

	FVector BaseCameraRelativeLocation = FVector::ZeroVector;

	float CurrentFOV = 90.0f;
	float TargetFOV = 90.0f;
	bool bWasADS = false;
	float CurrentTiltRoll = 0.0f;
	float TargetTiltRoll = 0.0f;

	float BobTimer = 0.0f;
	float CurrentBobOffset = 0.0f;

	float LandingOffset = 0.0f;
	bool bWasFalling = false;
	float FallSpeed = 0.0f;

	float CurrentSlideOffset = 0.0f;
	float CurrentSlidePitch = 0.0f;

	float CurrentMotionBlur = 0.0f;
};
