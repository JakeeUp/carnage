// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/AI/EnvQueryContext_Target.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "ShooterAIController.h"

void UEnvQueryContext_Target::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	if (AShooterAIController* Controller = Cast<AShooterAIController>(QueryInstance.Owner))
	{
		if (IsValid(Controller->GetCurrentTarget()))
		{
			UEnvQueryItemType_Actor::SetContextHelper(ContextData, Controller->GetCurrentTarget());

		} else {

			UEnvQueryItemType_Actor::SetContextHelper(ContextData, Controller);
		}
	}

}
