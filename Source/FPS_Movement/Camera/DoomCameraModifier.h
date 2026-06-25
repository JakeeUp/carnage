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

	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;
};
