// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "DoomCameraModifier.generated.h"

UCLASS()
class FPS_MOVEMENT_API UDoomCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:

	float DesiredFOV = 90.0f;
	float TiltRoll = 0.0f;
	float PitchOffset = 0.0f;
	float VerticalOffset = 0.0f;

	/**
	 * Camera-only recoil rotation, composed in the view's local frame. It moves the view but never
	 * the control rotation, so it springs back without changing where the player is aiming.
	 * The viewmodel applies the same rotation so the gun stays fixed on screen.
	 */
	FRotator PunchRotation = FRotator::ZeroRotator;

	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;
};
