// FPS_Movement: per-level gameplay defaults authored in the map.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "CarnageLevelRules.generated.h"

/**
 * Place one of these in a level to author level-specific defaults. Systems read the first
 * instance found in the world at BeginPlay (see Find). Pure data; it never ticks.
 */
UCLASS(Blueprintable, ClassGroup="Carnage", meta=(DisplayName="Carnage Level Rules"))
class FPS_MOVEMENT_API ACarnageLevelRules : public AInfo
{
	GENERATED_BODY()

public:

	ACarnageLevelRules(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Start the local player in the bodycam view (toggle with B, or carnage.Bodycam.Enable). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carnage|Camera")
	bool bStartWithBodycam = false;

	/** First ACarnageLevelRules in the world, or null if the level has none. */
	UFUNCTION(BlueprintPure, Category="Carnage", meta=(WorldContext="WorldContextObject"))
	static ACarnageLevelRules* Find(const UObject* WorldContextObject);
};
