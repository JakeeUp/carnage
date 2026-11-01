#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Chaos/ChaosEngineInterface.h"
#include "CarnageWeaponFXSubsystem.generated.h"

class AActor;
class APlayerController;
class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;

/** Describes one bullet impact for UCarnageWeaponFXSubsystem::PlayImpact. */
struct FCarnageImpactFXParams
{
	FVector Location = FVector::ZeroVector;

	/** Surface normal at the impact. */
	FVector Normal = FVector::UpVector;

	/** Direction the bullet was travelling. */
	FVector ShotDirection = FVector::ForwardVector;

	/** Hit a pawn: uses the pawn impact preset and never leaves a decal. */
	bool bHitPawn = false;

	/** Physical surface of the hit (from bReturnPhysicalMaterial). Reserved for per-surface presets. */
	EPhysicalSurface SurfaceType = SurfaceType_Default;

	/** Component that was hit; movable components get their decal attached. */
	TWeakObjectPtr<USceneComponent> HitComponent;
};

/** One pooled instanced-mesh render layer (internal). */
USTRUCT()
struct FCarnageFXLayer
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> ISM;

	/** Instances written this frame (world space). */
	TArray<FTransform> Transforms;

	/** 4 floats per instance (rgb, intensity/alpha) when NumCustomData == 4. */
	TArray<float> CustomData;

	int32 NumCustomData = 0;

	/** Instances that were visible after the last commit. */
	int32 NumVisible = 0;

	FVector LastLocation = FVector::ZeroVector;
};

/**
 * Procedural, asset-light gunfire FX: muzzle flashes, tracers, impact sparks/puffs, bullet holes and
 * ejected shells. Everything is simulated on the CPU and drawn through a handful of pooled
 * UInstancedStaticMeshComponents owned by one transient host actor per world; no actors are spawned
 * per shot (decals excepted). Tunables live in UCarnageWeaponFXSettings.
 */
UCLASS()
class FPS_MOVEMENT_API UCarnageWeaponFXSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	static UCarnageWeaponFXSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Converts the world location of something attached to a first-person-rendered primitive into the
	 * location where it visually appears (mirrors the GPU first-person morph). Returns the input when
	 * there is no player camera or the view does not use first-person parameters.
	 */
	static FVector GetVisibleFirstPersonLocation(const APlayerController* Viewer, const FVector& FirstPersonWorldLocation);

	/**
	 * Short burst at a muzzle socket. The flash follows the socket for its lifetime.
	 * @param AttachTo        mesh that owns the muzzle socket
	 * @param AimDirection    world-space direction the shot travels (the flash points this way, whatever the socket axes are)
	 * @param bFirstPerson    render with the first-person morph (AttachTo is a first-person primitive)
	 * @param Viewer          owning player, used to place the light where the first-person muzzle visually is
	 */
	void PlayMuzzleFlash(USceneComponent* AttachTo, FName SocketName, const FVector& AimDirection, bool bFirstPerson, const APlayerController* Viewer, float SizeScale = 1.0f);

	/** Fast streak from the visible muzzle to the impact. Skipped if the shot is shorter than Tracer.MinDistance. */
	void PlayTracer(const FVector& VisibleStart, const FVector& End);

	/** Streak that trails a projectile actor until it stops or is destroyed. */
	void PlayProjectileTrail(AActor* Projectile, const FVector& VisibleStart);

	/** Fades out any trail following this projectile (call when it hits something). */
	void StopProjectileTrail(const AActor* Projectile);

	/** Sparks, flash, dust and (surfaces only) a bullet-hole decal. */
	void PlayImpact(const FCarnageImpactFXParams& Params);

	/** Ejects a casing from a socket with a short arc. No-op if the socket does not exist. */
	void PlayShellEject(USceneComponent* AttachTo, FName SocketName, bool bFirstPerson, const FVector& ViewRight, const FVector& ViewUp, const FVector& ViewForward, const FVector& InheritVelocity);

	// USubsystem / FTickableGameObject
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:

	enum ELayer : uint8
	{
		Layer_Streaks,		// additive cylinders: tracers, sparks
		Layer_Glow,			// additive spheres: impact flashes, third-person muzzle flashes
		Layer_Dust,			// translucent spheres
		Layer_FlashFP,		// additive spheres, first-person morphed
		Layer_ShellFP,		// brass cylinders, first-person morphed
		Layer_ShellWorld,	// brass cylinders
		Layer_Count
	};

	struct FTracer
	{
		FVector Start;
		FVector Dir;
		float TotalDist = 0.0f;
		float HeadDist = 0.0f;
		float Length = 200.0f;
		// projectile trail
		TWeakObjectPtr<AActor> Follow;
		FVector Head;
		FVector Tail;
		float FadeAge = -1.0f;
		bool bTrail = false;
	};

	struct FSpark
	{
		FVector Pos;
		FVector Vel;
		float Age = 0.0f;
		float Life = 0.2f;
		float Gravity = 0.0f;
		float Drag = 0.0f;
		float Width = 0.5f;
		float StretchTime = 0.02f;
		float Intensity = 1.0f;
		FLinearColor Color;
	};

	struct FPuff
	{
		FVector Pos;
		FVector Vel;
		float Age = 0.0f;
		float Life = 0.2f;
		float StartSize = 1.0f;
		float EndSize = 2.0f;
		float Alpha = 1.0f;
		FLinearColor Color;
		uint8 Layer = Layer_Glow;
	};

	struct FFlash
	{
		TWeakObjectPtr<USceneComponent> Comp;
		TWeakObjectPtr<const APlayerController> Viewer;
		FName Socket;
		FVector LocalDir;
		FVector LastOrigin;
		FVector LastDir;
		float Age = 0.0f;
		float Scale = 1.0f;
		int32 Light = INDEX_NONE;
		bool bFirstPerson = false;
	};

	struct FShell
	{
		FVector Pos;
		FVector Vel;
		FQuat Rot;
		FVector SpinAxis;
		float Age = 0.0f;
		bool bFirstPerson = false;
	};

	bool EnsureHost();
	UInstancedStaticMeshComponent* CreateLayerISM(UStaticMesh* Mesh, UMaterialInterface* Material, int32 NumCustomData, bool bFirstPerson, int32 TranslucencySortPriority);
	int32 AcquireLight();

	void TickTracers(float DeltaTime);
	void TickSparks(float DeltaTime);
	void TickPuffs(float DeltaTime);
	void TickFlashes(float DeltaTime);
	void TickShells(float DeltaTime);

	void Emit(ELayer Layer, const FTransform& Transform, const FLinearColor& Color, float Scalar);
	void EmitStreak(ELayer Layer, const FVector& Tail, const FVector& Head, float Width, const FLinearColor& Color, float Intensity);
	void CommitLayer(FCarnageFXLayer& Layer);

	void SpawnBulletHole(const FCarnageImpactFXParams& Params);

	UPROPERTY(Transient)
	TObjectPtr<AActor> Host;

	UPROPERTY(Transient)
	TArray<FCarnageFXLayer> Layers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DecalMaterial;

	TArray<float> LightAge;

	TArray<TWeakObjectPtr<UDecalComponent>> BulletHoles;

	TArray<FTracer> Tracers;
	TArray<FSpark> Sparks;
	TArray<FPuff> Puffs;
	TArray<FFlash> Flashes;
	TArray<FShell> Shells;

	FRandomStream Rand;

	bool bHostFailed = false;
	bool bActive = false;
};
