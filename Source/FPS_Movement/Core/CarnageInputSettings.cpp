#include "CarnageInputSettings.h"
#include "HAL/IConsoleManager.h"
#include "FPS_Movement.h"

namespace CarnageInput
{
	static TAutoConsoleVariable<float> CVarSensitivity(
		TEXT("carnage.Sensitivity"), -1.0f,
		TEXT("Look sensitivity in Valorant units (0.07 deg per mouse count x value). <= 0 uses Project Settings > Carnage Input."));

	static FAutoConsoleCommand CmdSensInfo(
		TEXT("carnage.SensInfo"),
		TEXT("Logs the active sensitivity, degrees per count, and cm/360 at the configured DPI."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UE_LOG(LogFPS_Movement, Display, TEXT("[Sens] sensitivity=%.4f (Valorant units)  deg/count=%.5f  dpi=%d  cm/360=%.1f"),
				UCarnageInputSettings::GetActiveSensitivity(),
				UCarnageInputSettings::GetTargetDegreesPerCount(),
				GetDefault<UCarnageInputSettings>()->MouseDPI,
				UCarnageInputSettings::GetCentimetersPer360());
		}));
}

// ============================
// Queries
// ============================

float UCarnageInputSettings::GetActiveSensitivity()
{
	const float Override = CarnageInput::CVarSensitivity.GetValueOnGameThread();
	return Override > 0.0f ? Override : GetDefault<UCarnageInputSettings>()->Sensitivity;
}

float UCarnageInputSettings::GetTargetDegreesPerCount()
{
	return DegreesPerCountPerSensitivity * GetActiveSensitivity();
}

float UCarnageInputSettings::GetCentimetersPer360()
{
	constexpr float CentimetersPerInch = 2.54f;
	const float DegreesPerCount = GetTargetDegreesPerCount();
	const int32 DPI = GetDefault<UCarnageInputSettings>()->MouseDPI;
	if (DegreesPerCount <= 0.0f || DPI <= 0)
	{
		return 0.0f;
	}
	const float CountsPer360 = 360.0f / DegreesPerCount;
	return CountsPer360 / static_cast<float>(DPI) * CentimetersPerInch;
}
