#include "CarnageWeaponFXSettings.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

UCarnageWeaponFXSettings::UCarnageWeaponFXSettings()
{
	AdditiveMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Carnage/FX/M_FX_AdditiveEmissive.M_FX_AdditiveEmissive")));
	TranslucentMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Carnage/FX/M_FX_TranslucentSoft.M_FX_TranslucentSoft")));
	ShellMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Carnage/FX/M_FX_ShellBrass.M_FX_ShellBrass")));
	BulletHoleDecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Carnage/FX/M_FX_BulletHole.M_FX_BulletHole")));
	StreakMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	PuffMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));

	// Pawn hits: fewer, darker-red, slower sparks, a meatier puff and no decal.
	PawnImpact.MinSparks = 6;
	PawnImpact.MaxSparks = 10;
	PawnImpact.MinSpeed = 250.0f;
	PawnImpact.MaxSpeed = 900.0f;
	PawnImpact.ConeHalfAngle = 50.0f;
	PawnImpact.ReflectBias = 0.0f;
	PawnImpact.Gravity = 1800.0f;
	PawnImpact.SparkWidth = 0.9f;
	PawnImpact.SparkColor = FLinearColor(0.9f, 0.04f, 0.02f);
	PawnImpact.SparkIntensity = 6.0f;
	PawnImpact.FlashSize = 12.0f;
	PawnImpact.FlashColor = FLinearColor(1.0f, 0.15f, 0.08f);
	PawnImpact.FlashIntensity = 8.0f;
	PawnImpact.DustPuffs = 2;
	PawnImpact.DustStartSize = 8.0f;
	PawnImpact.DustEndSize = 26.0f;
	PawnImpact.DustLife = 0.3f;
	PawnImpact.DustColor = FLinearColor(0.25f, 0.015f, 0.01f);
	PawnImpact.DustOpacity = 0.55f;
	PawnImpact.bSpawnDecal = false;
}
