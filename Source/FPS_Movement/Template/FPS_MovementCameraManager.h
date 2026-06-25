// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "FPS_MovementCameraManager.generated.h"

UCLASS()
class AFPS_MovementCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
	
public:

	AFPS_MovementCameraManager();
};
