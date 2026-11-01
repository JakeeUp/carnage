#include "CarnageWeaponFXSubsystem.h"
#include "CarnageWeaponFXSettings.h"
#include "FPS_Movement.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace CarnageFX
{
	static const TCHAR* FallbackMeshMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	static const TCHAR* FallbackDecalMaterial = TEXT("/Engine/EngineMaterials/DefaultDeferredDecalMaterial.DefaultDeferredDecalMaterial");
	static const TCHAR* FallbackCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	static const TCHAR* FallbackSphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	/** Engine BasicShapes are 100 cm across. */
	static constexpr float MeshUnit = 100.0f;

	template<typename T>
	static T* LoadOr(const TSoftObjectPtr<T>& Soft, const TCHAR* FallbackPath, const TCHAR* What)
	{
		T* Loaded = Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
		if (!Loaded)
		{
			UE_LOG(LogFPS_Movement, Warning, TEXT("Carnage weapon FX: %s '%s' could not be loaded, falling back to '%s'."),
				What, *Soft.ToString(), FallbackPath);
			Loaded = LoadObject<T>(nullptr, FallbackPath);
		}
		return Loaded;
	}

	/** Swap-remove elements matching Pred (order is irrelevant for particles). */
	template<typename T, typename P>
	static void RemoveDead(TArray<T>& Arr, P Pred)
	{
		for (int32 i = Arr.Num() - 1; i >= 0; --i)
		{
			if (Pred(Arr[i]))
			{
				Arr.RemoveAtSwap(i, EAllowShrinking::No);
			}
		}
	}

	static FQuat AxisZTo(const FVector& Dir)
	{
		return FRotationMatrix::MakeFromZ(Dir).ToQuat();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifetime

UCarnageWeaponFXSubsystem* UCarnageWeaponFXSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UCarnageWeaponFXSubsystem>() : nullptr;
}

bool UCarnageWeaponFXSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCarnageWeaponFXSubsystem::Deinitialize()
{
	Tracers.Empty();
	Sparks.Empty();
	Puffs.Empty();
	Flashes.Empty();
	Shells.Empty();
	BulletHoles.Empty();
	Layers.Empty();
	Lights.Empty();
	Host = nullptr;
	bActive = false;

	Super::Deinitialize();
}

TStatId UCarnageWeaponFXSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCarnageWeaponFXSubsystem, STATGROUP_Tickables);
}

bool UCarnageWeaponFXSubsystem::IsTickable() const
{
	return bActive && !IsTemplate();
}

UInstancedStaticMeshComponent* UCarnageWeaponFXSubsystem::CreateLayerISM(UStaticMesh* Mesh, UMaterialInterface* Material, int32 NumCustomData, bool bFirstPerson, int32 TranslucencySortPriority)
{
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(Host, NAME_None, RF_Transient);
	ISM->SetupAttachment(Host->GetRootComponent());
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetStaticMesh(Mesh);
	ISM->SetMaterial(0, Material);
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCanEverAffectNavigation(false);
	ISM->SetGenerateOverlapEvents(false);
	ISM->SetCastShadow(false);
	ISM->bAffectDistanceFieldLighting = false;
	ISM->bAffectDynamicIndirectLighting = false;
	ISM->bUseAsOccluder = false;
	ISM->bReceivesDecals = false;
	ISM->SetTranslucentSortPriority(TranslucencySortPriority);
	ISM->SetNumCustomDataFloats(NumCustomData);
	if (bFirstPerson)
	{
		ISM->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	}
	ISM->RegisterComponent();
	return ISM;
}

bool UCarnageWeaponFXSubsystem::EnsureHost()
{
	if (Host)
	{
		return true;
	}
	if (bHostFailed)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(), TEXT("CarnageWeaponFXHost"));
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Host)
	{
		bHostFailed = true;
		UE_LOG(LogFPS_Movement, Error, TEXT("Carnage weapon FX: failed to spawn host actor; procedural gunfire FX disabled."));
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("Root"), RF_Transient);
	Root->SetMobility(EComponentMobility::Static);
	Host->SetRootComponent(Root);
	Root->RegisterComponent();
	Host->SetActorEnableCollision(false);

	const UCarnageWeaponFXSettings* S = GetDefault<UCarnageWeaponFXSettings>();
	UStaticMesh* Cylinder = CarnageFX::LoadOr(S->StreakMesh, CarnageFX::FallbackCylinder, TEXT("streak mesh"));
	UStaticMesh* Sphere = CarnageFX::LoadOr(S->PuffMesh, CarnageFX::FallbackSphere, TEXT("puff mesh"));
	UMaterialInterface* Additive = CarnageFX::LoadOr(S->AdditiveMaterial, CarnageFX::FallbackMeshMaterial, TEXT("additive material"));
	UMaterialInterface* Translucent = CarnageFX::LoadOr(S->TranslucentMaterial, CarnageFX::FallbackMeshMaterial, TEXT("translucent material"));
	UMaterialInterface* Brass = CarnageFX::LoadOr(S->ShellMaterial, CarnageFX::FallbackMeshMaterial, TEXT("shell material"));
	DecalMaterial = CarnageFX::LoadOr(S->BulletHoleDecalMaterial, CarnageFX::FallbackDecalMaterial, TEXT("bullet hole decal material"));

	Layers.SetNum(Layer_Count);
	Layers[Layer_Streaks].ISM = CreateLayerISM(Cylinder, Additive, 4, false, 10);
	Layers[Layer_Glow].ISM = CreateLayerISM(Sphere, Additive, 4, false, 11);
	Layers[Layer_Dust].ISM = CreateLayerISM(Sphere, Translucent, 4, false, 5);
	Layers[Layer_FlashFP].ISM = CreateLayerISM(Sphere, Additive, 4, true, 20);
	Layers[Layer_ShellFP].ISM = CreateLayerISM(Cylinder, Brass, 0, true, 0);
	Layers[Layer_ShellWorld].ISM = CreateLayerISM(Cylinder, Brass, 0, false, 0);
	for (FCarnageFXLayer& L : Layers)
	{
		L.NumCustomData = L.ISM ? L.ISM->NumCustomDataFloats : 0;
	}

	const int32 NumLights = FMath::Clamp(S->LightPoolSize, 1, 16);
	for (int32 i = 0; i < NumLights; ++i)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Host, NAME_None, RF_Transient);
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(false);
		Light->SetAttenuationRadius(S->Muzzle.LightRadius);
		Light->SetLightColor(S->Muzzle.LightColor);
		Light->SetIntensity(0.0f);
		Light->SetVisibility(false);
		Light->RegisterComponent();
		Lights.Add(Light);
	}
	LightAge.Init(-1.0f, NumLights);

	Rand.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	return true;
}

int32 UCarnageWeaponFXSubsystem::AcquireLight()
{
	// free slot, else steal the oldest pulse
	int32 Best = INDEX_NONE;
	float BestAge = -1.0f;
	for (int32 i = 0; i < LightAge.Num(); ++i)
	{
		if (LightAge[i] < 0.0f)
		{
			return i;
		}
		if (LightAge[i] > BestAge)
		{
			BestAge = LightAge[i];
			Best = i;
		}
	}
	if (Best != INDEX_NONE)
	{
		for (FFlash& F : Flashes)
		{
			if (F.Light == Best)
			{
				F.Light = INDEX_NONE;
			}
		}
	}
	return Best;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spawning

FVector UCarnageWeaponFXSubsystem::GetVisibleFirstPersonLocation(const APlayerController* Viewer, const FVector& FirstPersonWorldLocation)
{
	if (Viewer && Viewer->PlayerCameraManager)
	{
		return Viewer->PlayerCameraManager->GetCameraCacheView().TransformWorldToFirstPerson(FirstPersonWorldLocation, false);
	}
	return FirstPersonWorldLocation;
}

void UCarnageWeaponFXSubsystem::PlayMuzzleFlash(USceneComponent* AttachTo, FName SocketName, const FVector& AimDirection, bool bFirstPerson, const APlayerController* Viewer)
{
	const UCarnageWeaponFXSettings* S = GetDefault<UCarnageWeaponFXSettings>();
	if (!S->Muzzle.bEnabled || !AttachTo || !EnsureHost())
	{
		return;
	}

	const FTransform SocketT = AttachTo->GetSocketTransform(SocketName, RTS_World);
	const FVector Dir = AimDirection.GetSafeNormal(UE_SMALL_NUMBER, SocketT.GetUnitAxis(EAxis::X));

	FFlash& F = Flashes.AddDefaulted_GetRef();
	F.Comp = AttachTo;
	F.Viewer = Viewer;
	F.Socket = SocketName;
	F.LocalDir = SocketT.InverseTransformVectorNoScale(Dir);
	F.LastOrigin = SocketT.GetLocation();
	F.LastDir = Dir;
	F.Scale = 1.0f + Rand.FRandRange(-S->Muzzle.ScaleJitter, S->Muzzle.ScaleJitter);
	F.bFirstPerson = bFirstPerson;
	F.Light = S->Muzzle.bLightEnabled ? AcquireLight() : INDEX_NONE;
	if (F.Light != INDEX_NONE)
	{
		LightAge[F.Light] = 0.0f;
		UPointLightComponent* Light = Lights[F.Light];
		Light->SetAttenuationRadius(S->Muzzle.LightRadius);
		Light->SetLightColor(S->Muzzle.LightColor);
	}
	bActive = true;
}

void UCarnageWeaponFXSubsystem::PlayTracer(const FVector& VisibleStart, const FVector& End)
{
	const UCarnageWeaponFXSettings* S = GetDefault<UCarnageWeaponFXSettings>();
	const float Dist = FVector::Dist(VisibleStart, End);
	if (!S->Tracer.bEnabled || Dist < S->Tracer.MinDistance || Dist <= S->Tracer.StartOffset || Tracers.Num() >= S->MaxStreaks || !EnsureHost())
	{
		return;
	}

	FTracer& T = Tracers.AddDefaulted_GetRef();
	T.Start = VisibleStart;
	T.Dir = (End - VisibleStart) / Dist;
	T.TotalDist = Dist;
	T.HeadDist = S->Tracer.StartOffset;
	T.Length = Rand.FRandRange(S->Tracer.MinLength, FMath::Max(S->Tracer.MinLength, S->Tracer.MaxLength));
	bActive = true;
}

void UCarnageWeaponFXSubsystem::PlayProjectileTrail(AActor* Projectile, const FVector& VisibleStart)
{
	const UCarnageWeaponFXSettings* S = GetDefault<UCarnageWeaponFXSettings>();
	if (!S->Tracer.bEnabled || !Projectile || Tracers.Num() >= S->MaxStreaks || !EnsureHost())
	{
		return;
	}

	FTracer& T = Tracers.AddDefaulted_GetRef();
	T.bTrail = true;
	T.Follow = Projectile;
	T.Start = VisibleStart;
	T.Head = Projectile->GetActorLocation();
	T.Tail = VisibleStart;
	T.Dir = (T.Head - VisibleStart).GetSafeNormal(UE_SMALL_NUMBER, Projectile->GetActorForwardVector());
	T.Length = S->Tracer.ProjectileTrailLength;
	bActive = true;
}

void UCarnageWeaponFXSubsystem::PlayImpact(const FCarnageImpactFXParams& Params)
{
	const UCarnageWeaponFXSettings* S = GetDefault<UCarnageWeaponFXSettings>();
	const FCarnageImpactFXSettings& P = Params.bHitPawn ? S->PawnImpact : S->SurfaceImpact;
	if (!EnsureHost())
	{
		return;
	}

	const FVector N = Params.Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector D = Params.ShotDirection.GetSafeNormal(UE_SMALL_NUMBER, -N);
	const FVector Reflected = D - 2.0f * FVector::DotProduct(D, N) * N;
	const FVector SprayAxis = FMath::Lerp(N, Reflected, P.ReflectBias).GetSafeNormal(UE_SMALL_NUMBER, N);
	const float ConeRad = FMath::DegreesToRadians(P.ConeHalfAngle);

	// sparks
	const int32 NumSparks = FMath::Min(Rand.RandRange(FMath::Min(P.MinSparks, P.MaxSparks), FMath::Max(P.MinSparks, P.MaxSparks)),
		S->MaxStreaks - Sparks.Num() - Tracers.Num());
	for (int32 i = 0; i < NumSparks; ++i)
	{
		FVector Dir = Rand.VRandCone(SprayAxis, ConeRad);
		const float Into = FVector::DotProduct(Dir, N);
		if (Into < 0.05f)
		{
			Dir = (Dir - (Into - 0.05f) * N).GetSafeNormal();
		}
		FSpark& Sp = Sparks.AddDefaulted_GetRef();
		Sp.Pos = Params.Location + N * 0.5f;
		Sp.Vel = Dir * Rand.FRandRange(P.MinSpeed, FMath::Max(P.MinSpeed, P.MaxSpeed));
		Sp.Life = Rand.FRandRange(P.MinLife, FMath::Max(P.MinLife, P.MaxLife));
		Sp.Gravity = P.Gravity;
		Sp.Drag = P.Drag;
		Sp.Width = P.SparkWidth * Rand.FRandRange(0.7f, 1.2f);
		Sp.StretchTime = P.StretchTime;
		Sp.Intensity = P.SparkIntensity * Rand.FRandRange(0.6f, 1.2f);
		Sp.Color = P.SparkColor;
	}

	// hot flash
	if (P.FlashSize > 0.0f && Puffs.Num() < S->MaxPuffs)
	{
		FPuff& F = Puffs.AddDefaulted_GetRef();
		F.Pos = Params.Location + N * 1.5f;
		F.Vel = FVector::ZeroVector;
		F.Life = P.FlashLife;
		F.StartSize = P.FlashSize * Rand.FRandRange(0.8f, 1.1f);
		F.EndSize = F.StartSize * 1.6f;
		F.Alpha = P.FlashIntensity;
		F.Color = P.FlashColor;
		F.Layer = Layer_Glow;
	}

	// dust
	for (int32 i = 0; i < P.DustPuffs && Puffs.Num() < S->MaxPuffs; ++i)
	{
		FPuff& F = Puffs.AddDefaulted_GetRef();
		F.Pos = Params.Location + N * 2.0f;
		F.Vel = (N + Rand.VRand() * 0.45f).GetSafeNormal() * P.DustSpeed * Rand.FRandRange(0.5f, 1.0f);
		F.Life = P.DustLife * Rand.FRandRange(0.8f, 1.2f);
		F.StartSize = P.DustStartSize;
		F.EndSize = P.DustEndSize * Rand.FRandRange(0.8f, 1.2f);
		F.Alpha = P.DustOpacity;
		F.Color = P.DustColor;
		F.Layer = Layer_Dust;
	}

	if (P.bSpawnDecal && !Params.bHitPawn)
	{
		SpawnBulletHole(Params);
	}

	bActive = true;
}

void UCarnageWeaponFXSubsystem::SpawnBulletHole(const FCarnageImpactFXParams& Params)
{
	const UCarnageWeaponFXSettings* S = GetDefault<UCarnageWeaponFXSettings>();
	if (!DecalMaterial || S->MaxBulletHoles <= 0)
	{
		return;
	}

	const float Size = 0.5f * Rand.FRandRange(S->BulletHoleSize.X, FMath::Max(S->BulletHoleSize.X, S->BulletHoleSize.Y));
	const FVector DecalExtent(2.0f, Size, Size);
	FRotator Rot = (-Params.Normal).Rotation();
	Rot.Roll = Rand.FRandRange(-180.0f, 180.0f);

	UDecalComponent* Decal = nullptr;
	USceneComponent* HitComp = Params.HitComponent.Get();
	if (HitComp && HitComp->Mobility == EComponentMobility::Movable)
	{
		Decal = UGameplayStatics::SpawnDecalAttached(DecalMaterial, DecalExtent, HitComp, NAME_None,
			Params.Location, Rot, EAttachLocation::KeepWorldPosition, 0.0f);
	}
	else
	{
		Decal = UGameplayStatics::SpawnDecalAtLocation(this, DecalMaterial, DecalExtent, Params.Location, Rot, 0.0f);
	}
	if (!Decal)
	{
		return;
	}
	Decal->SetFadeScreenSize(0.0f);
	if (S->BulletHoleLifetime > 0.0f)
	{
		Decal->SetFadeOut(S->BulletHoleLifetime, S->BulletHoleFadeTime, false);
	}

	// cap the number of live holes: drop dead entries, then retire the oldest
	CarnageFX::RemoveDead(BulletHoles, [](const TWeakObjectPtr<UDecalComponent>& W) { return !W.IsValid(); });
	BulletHoles.Add(Decal);
	while (BulletHoles.Num() > S->MaxBulletHoles)
	{
		if (UDecalComponent* Oldest = BulletHoles[0].Get())
		{
			Oldest->DestroyComponent();
		}
		BulletHoles.RemoveAt(0, EAllowShrinking::No);
	}
}

void UCarnageWeaponFXSubsystem::PlayShellEject(USceneComponent* AttachTo, FName SocketName, bool bFirstPerson, const FVector& ViewRight, const FVector& ViewUp, const FVector& ViewForward, const FVector& InheritVelocity)
{
	const UCarnageWeaponFXSettings* S = GetDefault<UCarnageWeaponFXSettings>();
	if (!S->Shell.bEnabled || !AttachTo || !AttachTo->DoesSocketExist(SocketName) || Shells.Num() >= 64 || !EnsureHost())
	{
		return;
	}

	FShell& Sh = Shells.AddDefaulted_GetRef();
	Sh.Pos = AttachTo->GetSocketLocation(SocketName);
	Sh.Vel = ViewRight * Rand.FRandRange(S->Shell.SideSpeed.X, S->Shell.SideSpeed.Y)
		+ ViewUp * Rand.FRandRange(S->Shell.UpSpeed.X, S->Shell.UpSpeed.Y)
		- ViewForward * Rand.FRandRange(0.0f, 40.0f)
		+ InheritVelocity;
	Sh.Rot = FRotationMatrix::MakeFromZ(ViewRight).ToQuat();
	Sh.SpinAxis = (ViewForward + Rand.VRand() * 0.4f).GetSafeNormal(UE_SMALL_NUMBER, ViewForward);
	Sh.bFirstPerson = bFirstPerson;
	bActive = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Simulation + rendering

void UCarnageWeaponFXSubsystem::Emit(ELayer Layer, const FTransform& Transform, const FLinearColor& Color, float Scalar)
{
	FCarnageFXLayer& L = Layers[Layer];
	L.Transforms.Add(Transform);
	if (L.NumCustomData >= 4)
	{
		L.CustomData.Add(Color.R);
		L.CustomData.Add(Color.G);
		L.CustomData.Add(Color.B);
		L.CustomData.Add(Scalar);
		for (int32 i = 4; i < L.NumCustomData; ++i)
		{
			L.CustomData.Add(0.0f);
		}
	}
}

void UCarnageWeaponFXSubsystem::EmitStreak(ELayer Layer, const FVector& Tail, const FVector& Head, float Width, const FLinearColor& Color, float Intensity)
{
	const FVector Seg = Head - Tail;
	const float Len = Seg.Size();
	if (Len < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Scale(Width / CarnageFX::MeshUnit, Width / CarnageFX::MeshUnit, Len / CarnageFX::MeshUnit);
	Emit(Layer, FTransform(CarnageFX::AxisZTo(Seg / Len), (Head + Tail) * 0.5f, Scale), Color, Intensity);
}

void UCarnageWeaponFXSubsystem::TickTracers(float DeltaTime)
{
	const FCarnageTracerFXSettings& Cfg = GetDefault<UCarnageWeaponFXSettings>()->Tracer;

	for (FTracer& T : Tracers)
	{
		if (T.bTrail)
		{
			AActor* A = T.Follow.Get();
			const bool bAlive = A && !A->IsHidden() && !A->IsActorBeingDestroyed() && A->GetVelocity().SizeSquared() > FMath::Square(50.0f);
			if (bAlive && T.FadeAge < 0.0f)
			{
				T.Head = A->GetActorLocation();
				const FVector Back = T.Head - T.Start;
				const float BackLen = Back.Size();
				T.Dir = BackLen > UE_KINDA_SMALL_NUMBER ? Back / BackLen : T.Dir;
				T.Tail = T.Head - T.Dir * FMath::Min(T.Length, BackLen);
			}
			else
			{
				T.FadeAge = FMath::Max(T.FadeAge, 0.0f) + DeltaTime;
			}
			const float Alpha = T.FadeAge < 0.0f ? 1.0f : 1.0f - T.FadeAge / Cfg.ProjectileTrailFadeTime;
			if (Alpha > 0.0f)
			{
				EmitStreak(Layer_Streaks, T.Tail, T.Head, Cfg.Width, Cfg.Color, Cfg.Intensity * Alpha);
			}
			continue;
		}

		T.HeadDist += Cfg.Speed * DeltaTime;
		const float Head = FMath::Min(T.HeadDist, T.TotalDist);
		const float Tail = FMath::Max(0.0f, T.HeadDist - T.Length);
		if (Tail < T.TotalDist)
		{
			EmitStreak(Layer_Streaks, T.Start + T.Dir * Tail, T.Start + T.Dir * Head, Cfg.Width, Cfg.Color, Cfg.Intensity);
		}
	}

	CarnageFX::RemoveDead(Tracers, [&Cfg](const FTracer& T)
	{
		return T.bTrail ? T.FadeAge >= Cfg.ProjectileTrailFadeTime : (T.HeadDist - T.Length) >= T.TotalDist;
	});
}

void UCarnageWeaponFXSubsystem::TickSparks(float DeltaTime)
{
	for (FSpark& Sp : Sparks)
	{
		Sp.Age += DeltaTime;
		Sp.Vel *= FMath::Max(0.0f, 1.0f - Sp.Drag * DeltaTime);
		Sp.Vel.Z -= Sp.Gravity * DeltaTime;
		Sp.Pos += Sp.Vel * DeltaTime;

		const float T = FMath::Clamp(Sp.Age / Sp.Life, 0.0f, 1.0f);
		const float Speed = Sp.Vel.Size();
		const float Len = FMath::Max(Sp.Width * 2.0f, Speed * Sp.StretchTime);
		const FVector Dir = Speed > UE_KINDA_SMALL_NUMBER ? Sp.Vel / Speed : FVector::UpVector;
		// cools from hot to ember as it dies
		const FLinearColor Col = FMath::Lerp(Sp.Color, Sp.Color * FLinearColor(1.0f, 0.35f, 0.15f), T);
		EmitStreak(Layer_Streaks, Sp.Pos - Dir * Len, Sp.Pos, Sp.Width * (1.0f - 0.5f * T), Col, Sp.Intensity * FMath::Square(1.0f - T));
	}
	CarnageFX::RemoveDead(Sparks, [](const FSpark& Sp) { return Sp.Age >= Sp.Life; });
}

void UCarnageWeaponFXSubsystem::TickPuffs(float DeltaTime)
{
	for (FPuff& P : Puffs)
	{
		P.Age += DeltaTime;
		P.Vel *= FMath::Max(0.0f, 1.0f - 4.0f * DeltaTime);
		P.Pos += P.Vel * DeltaTime;

		const float T = FMath::Clamp(P.Age / P.Life, 0.0f, 1.0f);
		// ease-out growth, smooth fade
		const float Grow = 1.0f - FMath::Square(1.0f - T);
		const float Size = FMath::Lerp(P.StartSize, P.EndSize, Grow) / CarnageFX::MeshUnit;
		const float Fade = P.Layer == Layer_Dust ? FMath::Square(1.0f - T) * FMath::Min(1.0f, T * 8.0f) : FMath::Square(1.0f - T);
		Emit(static_cast<ELayer>(P.Layer), FTransform(FQuat::Identity, P.Pos, FVector(Size)), P.Color, P.Alpha * Fade);
	}
	CarnageFX::RemoveDead(Puffs, [](const FPuff& P) { return P.Age >= P.Life; });
}

void UCarnageWeaponFXSubsystem::TickFlashes(float DeltaTime)
{
	const FCarnageMuzzleFXSettings& Cfg = GetDefault<UCarnageWeaponFXSettings>()->Muzzle;

	for (FFlash& F : Flashes)
	{
		F.Age += DeltaTime;

		// follow the socket so the flash stays glued to a moving gun
		FVector SocketLoc = F.LastOrigin;
		if (USceneComponent* Comp = F.Comp.Get())
		{
			const FTransform SocketT = Comp->GetSocketTransform(F.Socket, RTS_World);
			SocketLoc = SocketT.GetLocation();
			F.LastDir = SocketT.TransformVectorNoScale(F.LocalDir).GetSafeNormal(UE_SMALL_NUMBER, F.LastDir);
			F.LastOrigin = SocketLoc;
		}
		const FVector Dir = F.LastDir;
		const FVector Origin = SocketLoc + Dir * Cfg.ForwardOffset;

		if (F.Age < Cfg.Duration)
		{
			const float T = F.Age / Cfg.Duration;
			const float Pulse = F.Scale * FMath::Lerp(1.0f, 1.3f, T);
			const float Intensity = Cfg.Intensity * (1.0f - T * T);
			const ELayer Layer = F.bFirstPerson ? Layer_FlashFP : Layer_Glow;

			// fresh random roll every frame reads as flicker
			const FQuat Basis = FQuat(Dir, Rand.FRandRange(0.0f, UE_TWO_PI)) * CarnageFX::AxisZTo(Dir);
			const FVector AxisX = Basis.GetAxisX();
			const FVector AxisY = Basis.GetAxisY();

			// forward plume
			const float PlumeLen = Cfg.PlumeLength * Pulse * Rand.FRandRange(0.85f, 1.15f);
			Emit(Layer, FTransform(Basis, Origin + Dir * PlumeLen * 0.5f,
				FVector(Cfg.PlumeWidth * Pulse, Cfg.PlumeWidth * Pulse, PlumeLen) / CarnageFX::MeshUnit), Cfg.Color, Intensity);

			// hot core
			Emit(Layer, FTransform(Basis, Origin, FVector(Cfg.CoreSize * Pulse / CarnageFX::MeshUnit)),
				FLinearColor(1.0f, 0.85f, 0.6f), Intensity * 1.5f);

			// side petals
			const int32 NumPetals = FMath::Clamp(Cfg.NumPetals, 0, 8);
			for (int32 k = 0; k < NumPetals; ++k)
			{
				const float Angle = UE_TWO_PI * k / NumPetals;
				const FVector Radial = AxisX * FMath::Cos(Angle) + AxisY * FMath::Sin(Angle);
				const FVector PetalDir = (Radial + Dir * 0.55f).GetSafeNormal();
				const float PetalLen = Cfg.PetalLength * Pulse * Rand.FRandRange(0.7f, 1.2f);
				Emit(Layer, FTransform(CarnageFX::AxisZTo(PetalDir), Origin + PetalDir * PetalLen * 0.5f,
					FVector(Cfg.PetalWidth * Pulse, Cfg.PetalWidth * Pulse, PetalLen) / CarnageFX::MeshUnit), Cfg.Color, Intensity * 0.8f);
			}
		}

		if (F.Light != INDEX_NONE && Lights.IsValidIndex(F.Light))
		{
			UPointLightComponent* Light = Lights[F.Light];
			const float T = F.Age / FMath::Max(Cfg.LightDuration, UE_KINDA_SMALL_NUMBER);
			if (T < 1.0f)
			{
				// the light is a world-space object, so put it where the first-person muzzle visually is
				const FVector LightPos = F.bFirstPerson ? GetVisibleFirstPersonLocation(F.Viewer.Get(), Origin) : Origin;
				Light->SetWorldLocation(LightPos + Dir * 8.0f);
				Light->SetIntensity(Cfg.LightIntensity * FMath::Square(1.0f - T));
				Light->SetVisibility(true);
				LightAge[F.Light] = F.Age;
			}
			else
			{
				Light->SetVisibility(false);
				LightAge[F.Light] = -1.0f;
				F.Light = INDEX_NONE;
			}
		}
	}

	const float MaxAge = FMath::Max(Cfg.Duration, Cfg.LightDuration);
	for (int32 i = Flashes.Num() - 1; i >= 0; --i)
	{
		FFlash& F = Flashes[i];
		if (F.Age >= MaxAge && F.Light == INDEX_NONE)
		{
			Flashes.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}
}

void UCarnageWeaponFXSubsystem::TickShells(float DeltaTime)
{
	const FCarnageShellFXSettings& Cfg = GetDefault<UCarnageWeaponFXSettings>()->Shell;
	const float Spin = FMath::DegreesToRadians(Cfg.SpinRate);

	for (FShell& Sh : Shells)
	{
		Sh.Age += DeltaTime;
		Sh.Vel.Z -= Cfg.Gravity * DeltaTime;
		Sh.Pos += Sh.Vel * DeltaTime;
		Sh.Rot = FQuat(Sh.SpinAxis, Spin * DeltaTime) * Sh.Rot;

		const float T = Sh.Age / FMath::Max(Cfg.Life, UE_KINDA_SMALL_NUMBER);
		const float Shrink = T > 0.85f ? FMath::Max(0.0f, 1.0f - (T - 0.85f) / 0.15f) : 1.0f;
		Emit(Sh.bFirstPerson ? Layer_ShellFP : Layer_ShellWorld,
			FTransform(Sh.Rot, Sh.Pos, Cfg.Size * (Shrink / CarnageFX::MeshUnit)), FLinearColor::White, 1.0f);
	}
	const float Life = Cfg.Life;
	CarnageFX::RemoveDead(Shells, [Life](const FShell& Sh) { return Sh.Age >= Life; });
}

void UCarnageWeaponFXSubsystem::CommitLayer(FCarnageFXLayer& L)
{
	UInstancedStaticMeshComponent* ISM = L.ISM;
	const int32 Live = L.Transforms.Num();
	if (!ISM || (Live == 0 && L.NumVisible == 0))
	{
		L.Transforms.Reset();
		L.CustomData.Reset();
		return;
	}

	if (Live > 0)
	{
		L.LastLocation = L.Transforms[0].GetLocation();
	}
	// parked instances: zero scale near the live ones so bounds stay tight
	const FTransform Parked(FQuat::Identity, L.LastLocation, FVector::ZeroVector);

	const int32 Needed = FMath::Max(Live, L.NumVisible);
	const int32 Existing = ISM->GetInstanceCount();
	if (Existing < Needed)
	{
		TArray<FTransform> Extra;
		Extra.Init(Parked, Needed - Existing);
		ISM->AddInstances(Extra, false, true, false);
	}

	for (int32 i = Live; i < L.NumVisible; ++i)
	{
		L.Transforms.Add(Parked);
	}
	ISM->BatchUpdateInstancesTransforms(0, L.Transforms, true, false, true);

	if (L.NumCustomData > 0)
	{
		for (int32 i = 0; i < Live; ++i)
		{
			ISM->SetCustomData(i, TArrayView<const float>(L.CustomData.GetData() + i * L.NumCustomData, L.NumCustomData), false);
		}
	}

	ISM->UpdateBounds();
	ISM->MarkRenderTransformDirty();

	L.NumVisible = Live;
	L.Transforms.Reset();
	L.CustomData.Reset();
}

void UCarnageWeaponFXSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!Host || Layers.Num() != Layer_Count)
	{
		bActive = false;
		return;
	}

	// clamp hitches so particles don't teleport
	const float Dt = FMath::Min(DeltaTime, 0.05f);

	TickTracers(Dt);
	TickSparks(Dt);
	TickPuffs(Dt);
	TickFlashes(Dt);
	TickShells(Dt);

	bool bAnyVisible = false;
	for (FCarnageFXLayer& L : Layers)
	{
		CommitLayer(L);
		bAnyVisible |= L.NumVisible > 0;
	}

	bActive = bAnyVisible || Tracers.Num() > 0 || Sparks.Num() > 0 || Puffs.Num() > 0 || Flashes.Num() > 0 || Shells.Num() > 0;
}
