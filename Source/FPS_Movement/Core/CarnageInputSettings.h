#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CarnageInputSettings.generated.h"

/**
 * @brief Mouse sensitivity expressed in Valorant units, so a player's existing muscle memory carries over.
 *
 * Valorant turns 0.07 degrees per mouse count per point of in-game sensitivity. Using the same scale
 * here means "0.105 at 1600 DPI" produces the same cm/360 (about 77.8 cm) as it does in Valorant.
 * Found in Project Settings > Game > Carnage Input. carnage.Sensitivity overrides it at runtime.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Carnage Input"))
class FPS_MOVEMENT_API UCarnageInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	// ============================
	// Sensitivity
	// ============================

	/** Look sensitivity in Valorant units (degrees per mouse count = 0.07 x this value). */
	UPROPERTY(Config, EditAnywhere, Category="Sensitivity", meta=(ClampMin=0.001, ClampMax=10, UIMin=0.05, UIMax=2))
	float Sensitivity = 0.105f;

	/** Mouse DPI, used only to report cm/360 (carnage.SensInfo). Does not change the feel. */
	UPROPERTY(Config, EditAnywhere, Category="Sensitivity", meta=(ClampMin=100, ClampMax=32000))
	int32 MouseDPI = 1600;

	/**
	 * Extra multiplier while aiming down sights, applied on top of the zoom-ratio scaling that keeps
	 * on-screen speed constant as the FOV narrows. 1 = pure zoom-ratio matching.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Sensitivity", meta=(ClampMin=0.1, ClampMax=3))
	float ADSSensitivityMultiplier = 1.0f;

	// ============================
	// Queries
	// ============================

	/** Valorant constant: degrees of yaw per mouse count at sensitivity 1. */
	static constexpr float DegreesPerCountPerSensitivity = 0.07f;

	/** Active sensitivity: the carnage.Sensitivity console override when set, else Sensitivity. */
	static float GetActiveSensitivity();

	/** Target view rotation, in degrees, per raw mouse count. */
	static float GetTargetDegreesPerCount();

	/** Horizontal mouse travel for a full 360 turn at the active sensitivity and MouseDPI. */
	static float GetCentimetersPer360();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
