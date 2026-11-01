// FPS_Movement: bodycam view look.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "BodycamCameraModifier.generated.h"

class UBodycamComponent;

/**
 * Thin camera modifier owned by UBodycamComponent. It runs after UDoomCameraModifier
 * (higher Priority value = applied later) so the bodycam FOV bonus, rotation lag and rig motion
 * compose on top of the existing movement / ADS / punch pipeline, and it pushes the bodycam
 * post-process (engine settings + lens material) through the camera's weighted PP blend so it
 * never fights the camera component's own post-process writes.
 *
 * All state and tuning live on the component; this class only forwards the two callbacks.
 */
UCLASS()
class FPS_MOVEMENT_API UBodycamCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:

	UBodycamCameraModifier();

	TWeakObjectPtr<UBodycamComponent> Source;

	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;

	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;
};
