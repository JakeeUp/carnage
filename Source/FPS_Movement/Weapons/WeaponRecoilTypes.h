// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeaponRecoilTypes.generated.h"

/**
 * Per-shot recoil description. Three channels are driven from it:
 *  - Aim recoil: PitchKick / YawKickRandom rotate the control rotation (moves the crosshair, recovers).
 *  - View punch: camera-only rotation spring that never changes aim.
 *  - Viewmodel kick: position + rotation impulses on the first-person weapon springs.
 * Impulse values are injected as spring velocity; the visible peak is roughly half the value.
 */
USTRUCT(BlueprintType)
struct FWeaponRecoilParams
{
	GENERATED_BODY()

	// ── Aim recoil (control rotation) ───────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin=0, ClampMax=20))
	float PitchKick = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin=0, ClampMax=10))
	float YawKickRandom = 0.25f;

	// ── Viewmodel kick ──────────────────────────────────────────────────

	/** Backwards push of the weapon (cm impulse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin=0, ClampMax=30))
	float VisualKickBack = 3.0f;

	/** Muzzle climb of the weapon (degree impulse). Pivots about the sight while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin=0, ClampMax=30))
	float VisualKickPitch = 1.5f;

	/** Upward push of the weapon (cm impulse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin=0, ClampMax=10))
	float VisualKickUp = 0.4f;

	/** Random roll of the weapon, +/- degrees impulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin=0, ClampMax=20))
	float VisualKickRollRandom = 3.0f;

	/** Random yaw of the weapon, +/- degrees impulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin=0, ClampMax=10))
	float VisualKickYawRandom = 1.0f;

	// ── View punch (camera only) ────────────────────────────────────────

	/** Upward camera punch (degree impulse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|ViewPunch", meta=(ClampMin=0, ClampMax=20))
	float ViewPunchPitch = 2.5f;

	/** Random camera yaw punch, +/- degrees impulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|ViewPunch", meta=(ClampMin=0, ClampMax=10))
	float ViewPunchYawRandom = 0.4f;

	/** Random camera roll punch, +/- degrees impulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|ViewPunch", meta=(ClampMin=0, ClampMax=10))
	float ViewPunchRollRandom = 0.8f;
};
