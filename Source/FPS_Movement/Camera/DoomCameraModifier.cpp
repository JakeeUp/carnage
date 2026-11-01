// Copyright Epic Games, Inc. All Rights Reserved.

#include "DoomCameraModifier.h"

bool UDoomCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	InOutPOV.FOV = DesiredFOV;
	InOutPOV.Rotation.Roll += TiltRoll;
	InOutPOV.Rotation.Pitch += PitchOffset;
	InOutPOV.Location.Z += VerticalOffset;

	if (!PunchRotation.IsNearlyZero(1.e-4f))
	{
		// Local-space composition: the punch pitches/rolls about the view's own axes.
		InOutPOV.Rotation = (InOutPOV.Rotation.Quaternion() * PunchRotation.Quaternion()).Rotator();
	}

	return false;
}
