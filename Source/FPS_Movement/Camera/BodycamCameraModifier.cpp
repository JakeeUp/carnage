// FPS_Movement: bodycam view look.

#include "BodycamCameraModifier.h"
#include "BodycamComponent.h"

UBodycamCameraModifier::UBodycamCameraModifier()
{
	// UDoomCameraModifier uses the default 127 and overwrites the FOV, so apply after it.
	Priority = 200;
}

bool UBodycamCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	// The base implementation also calls ModifyPostProcess and queues the PP blend.
	Super::ModifyCamera(DeltaTime, InOutPOV);

	if (UBodycamComponent* Comp = Source.Get())
	{
		Comp->ApplyToView(DeltaTime, InOutPOV);
	}
	return false;
}

void UBodycamCameraModifier::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	if (const UBodycamComponent* Comp = Source.Get())
	{
		Comp->ApplyPostProcess(PostProcessBlendWeight, PostProcessSettings);
	}
}
