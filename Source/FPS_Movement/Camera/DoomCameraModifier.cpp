// Copyright Epic Games, Inc. All Rights Reserved.

#include "DoomCameraModifier.h"

bool UDoomCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	InOutPOV.FOV = DesiredFOV;
	InOutPOV.Rotation.Roll += TiltRoll;
	InOutPOV.Rotation.Pitch += PitchOffset;
	InOutPOV.Location.Z += VerticalOffset;

	return false;
}
