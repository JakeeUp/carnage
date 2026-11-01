// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPS_Movement.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, FPS_Movement, "FPS_Movement" );

DEFINE_LOG_CATEGORY(LogFPS_Movement)

#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarCarnageAILog(
	TEXT("carnage.Debug.AILog"), 0,
	TEXT("1 = log AI perception, target acceptance, and weapon fire decisions."));

bool CarnageAILogEnabled()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return CVarCarnageAILog.GetValueOnGameThread() != 0;
#endif
}
