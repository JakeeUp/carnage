// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFPS_Movement, Log, All);

/**
 * "Weapon" trace channel (Config/DefaultEngine.ini). Used by hitscan rounds and aim traces.
 * Unlike Visibility, the engine's Pawn and CharacterMesh profiles block it by default, so
 * shots actually reach characters. Characters with a physics asset ignore it on the capsule
 * so the trace lands on per-bone bodies instead (see ShooterDamage::SetupPerBoneWeaponHits).
 */
#define ECC_Weapon ECC_GameTraceChannel2
