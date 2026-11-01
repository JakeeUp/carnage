#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CarnageWeaponFXSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** Fast glowing streak flying from the visible muzzle to the impact point. */
USTRUCT(BlueprintType)
struct FCarnageTracerFXSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Tracer")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Config, Category="Tracer", meta=(ClampMin=1000, Units="cm/s"))
	float Speed = 25000.0f;

	UPROPERTY(EditAnywhere, Config, Category="Tracer", meta=(ClampMin=1, Units="cm"))
	float MinLength = 150.0f;

	UPROPERTY(EditAnywhere, Config, Category="Tracer", meta=(ClampMin=1, Units="cm"))
	float MaxLength = 250.0f;

	UPROPERTY(EditAnywhere, Config, Category="Tracer", meta=(ClampMin=0.05, Units="cm"))
	float Width = 1.1f;

	/** Shots whose impact is closer than this get no tracer (it would just flicker). */
	UPROPERTY(EditAnywhere, Config, Category="Tracer", meta=(ClampMin=0, Units="cm"))
	float MinDistance = 250.0f;

	/** Head starts this far down the barrel line so the first frame is not buried in the flash. */
	UPROPERTY(EditAnywhere, Config, Category="Tracer", meta=(ClampMin=0, Units="cm"))
	float StartOffset = 40.0f;

	UPROPERTY(EditAnywhere, Config, Category="Tracer")
	FLinearColor Color = FLinearColor(1.0f, 0.62f, 0.25f);

	UPROPERTY(EditAnywhere, Config, Category="Tracer", meta=(ClampMin=0))
	float Intensity = 30.0f;

	/** Trail length used for projectile-mode shots (follows the projectile actor). */
	UPROPERTY(EditAnywhere, Config, Category="Tracer|Projectile", meta=(ClampMin=1, Units="cm"))
	float ProjectileTrailLength = 120.0f;

	UPROPERTY(EditAnywhere, Config, Category="Tracer|Projectile", meta=(ClampMin=0.01, Units="s"))
	float ProjectileTrailFadeTime = 0.12f;
};

/** Short additive burst at the muzzle + a warm light pulse. */
USTRUCT(BlueprintType)
struct FCarnageMuzzleFXSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Muzzle")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0.01, Units="s"))
	float Duration = 0.045f;

	/** Number of radial "petals" around the barrel axis. */
	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0, ClampMax=8))
	int32 NumPetals = 4;

	/** Forward plume length / radius (cm, before random scale). */
	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0, Units="cm"))
	float PlumeLength = 16.0f;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0, Units="cm"))
	float PlumeWidth = 4.5f;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0, Units="cm"))
	float PetalLength = 7.0f;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0, Units="cm"))
	float PetalWidth = 1.4f;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0, Units="cm"))
	float CoreSize = 3.5f;

	/** Random uniform scale variation (0.2 = +/-20%). */
	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0, ClampMax=1))
	float ScaleJitter = 0.2f;

	/** Push the flash forward along the aim direction from the socket. */
	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(Units="cm"))
	float ForwardOffset = 1.0f;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle")
	FLinearColor Color = FLinearColor(1.0f, 0.55f, 0.18f);

	UPROPERTY(EditAnywhere, Config, Category="Muzzle", meta=(ClampMin=0))
	float Intensity = 40.0f;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle|Light")
	bool bLightEnabled = true;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle|Light")
	FLinearColor LightColor = FLinearColor(1.0f, 0.62f, 0.3f);

	UPROPERTY(EditAnywhere, Config, Category="Muzzle|Light", meta=(ClampMin=0))
	float LightIntensity = 20000.0f;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle|Light", meta=(ClampMin=1, Units="cm"))
	float LightRadius = 400.0f;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle|Light", meta=(ClampMin=0.01, Units="s"))
	float LightDuration = 0.045f;
};

/** Parameters for one class of impact (surface vs. pawn). */
USTRUCT(BlueprintType)
struct FCarnageImpactFXSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0, ClampMax=64))
	int32 MinSparks = 8;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0, ClampMax=64))
	int32 MaxSparks = 16;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0, Units="cm/s"))
	float MinSpeed = 500.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0, Units="cm/s"))
	float MaxSpeed = 1700.0f;

	/** Half angle of the spray cone around the bounce direction. */
	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0, ClampMax=90, Units="Degrees"))
	float ConeHalfAngle = 38.0f;

	/** 0 = spray around the surface normal, 1 = spray along the mirrored shot direction. */
	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0, ClampMax=1))
	float ReflectBias = 0.45f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0.01, Units="s"))
	float MinLife = 0.15f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0.01, Units="s"))
	float MaxLife = 0.4f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(Units="cm/s"))
	float Gravity = 1400.0f;

	/** Linear drag (1/s). */
	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0))
	float Drag = 3.5f;

	/** Streak length = speed * this (pseudo motion blur). */
	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0, Units="s"))
	float StretchTime = 0.018f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0.05, Units="cm"))
	float SparkWidth = 0.6f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks")
	FLinearColor SparkColor = FLinearColor(1.0f, 0.55f, 0.15f);

	UPROPERTY(EditAnywhere, Config, Category="Impact|Sparks", meta=(ClampMin=0))
	float SparkIntensity = 25.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Flash", meta=(ClampMin=0, Units="cm"))
	float FlashSize = 9.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Flash", meta=(ClampMin=0.01, Units="s"))
	float FlashLife = 0.06f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Flash")
	FLinearColor FlashColor = FLinearColor(1.0f, 0.7f, 0.35f);

	UPROPERTY(EditAnywhere, Config, Category="Impact|Flash", meta=(ClampMin=0))
	float FlashIntensity = 20.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Dust", meta=(ClampMin=0, ClampMax=8))
	int32 DustPuffs = 2;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Dust", meta=(ClampMin=0, Units="cm"))
	float DustStartSize = 6.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Dust", meta=(ClampMin=0, Units="cm"))
	float DustEndSize = 30.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Dust", meta=(ClampMin=0.01, Units="s"))
	float DustLife = 0.45f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Dust", meta=(ClampMin=0, Units="cm/s"))
	float DustSpeed = 120.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Dust")
	FLinearColor DustColor = FLinearColor(0.32f, 0.29f, 0.25f);

	UPROPERTY(EditAnywhere, Config, Category="Impact|Dust", meta=(ClampMin=0, ClampMax=1))
	float DustOpacity = 0.35f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Decal")
	bool bSpawnDecal = true;
};

/** Ejected brass from the "ShellEject" socket. */
USTRUCT(BlueprintType)
struct FCarnageShellFXSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Shell")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Config, Category="Shell")
	FName SocketName = FName("ShellEject");

	UPROPERTY(EditAnywhere, Config, Category="Shell", meta=(ClampMin=0.05, Units="s"))
	float Life = 0.55f;

	/** Sideways (camera right) ejection speed range. */
	UPROPERTY(EditAnywhere, Config, Category="Shell", meta=(Units="cm/s"))
	FVector2D SideSpeed = FVector2D(140.0f, 220.0f);

	UPROPERTY(EditAnywhere, Config, Category="Shell", meta=(Units="cm/s"))
	FVector2D UpSpeed = FVector2D(110.0f, 190.0f);

	UPROPERTY(EditAnywhere, Config, Category="Shell", meta=(Units="cm/s"))
	float Gravity = 980.0f;

	UPROPERTY(EditAnywhere, Config, Category="Shell", meta=(Units="cm"))
	FVector Size = FVector(0.9f, 0.9f, 2.0f);

	/** Tumble rate in degrees/s. */
	UPROPERTY(EditAnywhere, Config, Category="Shell", meta=(ClampMin=0))
	float SpinRate = 1440.0f;
};

/**
 * Project-wide tunables for the procedural gunfire FX (Project Settings > Game > Carnage Weapon FX).
 * No Niagara assets are required; everything renders through pooled instanced meshes.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Carnage Weapon FX"))
class FPS_MOVEMENT_API UCarnageWeaponFXSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UCarnageWeaponFXSettings();

	virtual FName GetCategoryName() const override { return FName("Game"); }

	// ── Materials / meshes ─────────────────────────────────────────────

	/** Additive unlit material: CustomData 0-2 = colour, 3 = intensity. */
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UMaterialInterface> AdditiveMaterial;

	/** Translucent unlit material: CustomData 0-2 = colour, 3 = opacity. */
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UMaterialInterface> TranslucentMaterial;

	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UMaterialInterface> ShellMaterial;

	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UMaterialInterface> BulletHoleDecalMaterial;

	/** Streak mesh: unit cylinder along +Z (engine BasicShapes). */
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UStaticMesh> StreakMesh;

	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UStaticMesh> PuffMesh;

	// ── Effects ────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Config, Category="Tracer")
	FCarnageTracerFXSettings Tracer;

	UPROPERTY(EditAnywhere, Config, Category="Muzzle")
	FCarnageMuzzleFXSettings Muzzle;

	UPROPERTY(EditAnywhere, Config, Category="Impact")
	FCarnageImpactFXSettings SurfaceImpact;

	UPROPERTY(EditAnywhere, Config, Category="Impact")
	FCarnageImpactFXSettings PawnImpact;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Decal", meta=(Units="cm"))
	FVector2D BulletHoleSize = FVector2D(4.0f, 6.0f);

	UPROPERTY(EditAnywhere, Config, Category="Impact|Decal", meta=(ClampMin=0, Units="s"))
	float BulletHoleLifetime = 10.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Decal", meta=(ClampMin=0, Units="s"))
	float BulletHoleFadeTime = 2.0f;

	UPROPERTY(EditAnywhere, Config, Category="Impact|Decal", meta=(ClampMin=0, ClampMax=512))
	int32 MaxBulletHoles = 64;

	UPROPERTY(EditAnywhere, Config, Category="Shell")
	FCarnageShellFXSettings Shell;

	// ── Budget ─────────────────────────────────────────────────────────

	/** Hard cap on simultaneously simulated sparks + tracers. */
	UPROPERTY(EditAnywhere, Config, Category="Budget", meta=(ClampMin=16, ClampMax=8192))
	int32 MaxStreaks = 512;

	UPROPERTY(EditAnywhere, Config, Category="Budget", meta=(ClampMin=8, ClampMax=4096))
	int32 MaxPuffs = 128;

	UPROPERTY(EditAnywhere, Config, Category="Budget", meta=(ClampMin=1, ClampMax=16))
	int32 LightPoolSize = 4;
};
