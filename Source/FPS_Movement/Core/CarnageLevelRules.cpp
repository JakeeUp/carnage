// FPS_Movement: per-level gameplay defaults authored in the map.

#include "CarnageLevelRules.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ACarnageLevelRules::ACarnageLevelRules(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

ACarnageLevelRules* ACarnageLevelRules::Find(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	TActorIterator<ACarnageLevelRules> It(World);
	return It ? *It : nullptr;
}
