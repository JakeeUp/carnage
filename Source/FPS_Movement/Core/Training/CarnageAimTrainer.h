// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterShootable.h"
#include "CarnageAimTrainer.generated.h"

class AShooterCharacter;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM()
enum class ECarnageAimMode : uint8
{
	Gridshot,
	Microshot,
	Tracking,
	Count UMETA(Hidden)
};

enum class ECarnageAimTargetState : uint8 { Idle, Alive, Popping };

/** Runtime state of one pooled target sphere. */
USTRUCT()
struct FCarnageAimTarget
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	ECarnageAimTargetState State = ECarnageAimTargetState::Idle;

	/** Grid cell (Gridshot) or INDEX_NONE. */
	int32 Cell = INDEX_NONE;

	float Radius = 30.0f;

	/** Seconds since the pop started. */
	float PopAge = 0.0f;

	/** Hit flash, 1 on hit decaying to 0 (Tracking). */
	float Flash = 0.0f;
};

/** One shootable console button and its label. */
USTRUCT()
struct FCarnageAimButton
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	/** Mode this button starts, or Count for START (replay the current mode). */
	ECarnageAimMode Mode = ECarnageAimMode::Count;

	/** This button toggles infinite ammo instead of starting a run. */
	bool bInfiniteAmmoToggle = false;
};

/**
 * Self-contained Aim Labs-style trainer: a target wall with a scoreboard above it and a console of
 * shootable mode buttons near the firing line. Weapon hits arrive through IShooterShootable.
 *
 * Placement contract (all relative to the actor transform):
 *   - Origin is on the floor, centred under the target wall.
 *   - Targets face -X: the player stands at X = -FiringDistance (default 1500 cm) looking along +X.
 *   - Footprint: wall/backdrop spans Y +-(AreaExtent.Y + BackdropMargin) at X ~ 0..BackdropOffset;
 *     the console sits at ConsoleOffset (default 150 cm in front of the firing line, to the left).
 * See Docs/Systems/AimTrainer.md.
 */
UCLASS(Blueprintable)
class FPS_MOVEMENT_API ACarnageAimTrainer : public AActor, public IShooterShootable
{
	GENERATED_BODY()

public:

	ACarnageAimTrainer();

	//~Begin IShooterShootable
	virtual bool HandleShot(const FShooterHitInfo& Hit, FShooterHitOutcome& OutOutcome) override;
	//~End IShooterShootable

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Starts (or restarts) a run of the given mode with a countdown. */
	UFUNCTION(BlueprintCallable, Category="Aim Trainer")
	void StartRun(ECarnageAimMode NewMode);

	/** World location where the player is meant to stand. */
	UFUNCTION(BlueprintPure, Category="Aim Trainer")
	FVector GetFiringPoint() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- Layout -------------------------------------------------------------------------------

	/** Half size (Y = width, Z = height) of the area targets spawn in, in cm. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Layout")
	FVector2D AreaExtent = FVector2D(600.0f, 300.0f);

	/** Height of the spawn area centre above the actor origin. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Layout")
	float AreaCenterHeight = 450.0f;

	/** Distance behind the spawn plane (+X) of the backdrop's front face. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Layout", meta=(ClampMin="10"))
	float BackdropOffset = 120.0f;

	/** Extra backdrop beyond the spawn area on each side and below. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Layout")
	float BackdropMargin = 150.0f;

	/** Height of the scoreboard band above the spawn area (also backdrop). */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Layout")
	float ScoreboardHeight = 420.0f;

	/** Where the player stands, measured along -X from the actor origin. Used for the floor marker and console facing. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Layout")
	float FiringDistance = 1500.0f;

	/** Width of the floor marker at the firing line. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Layout")
	float FiringLineWidth = 500.0f;

	/** Mode console location relative to the actor; it turns to face the firing point. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Layout")
	FVector ConsoleOffset = FVector(-1350.0f, -380.0f, 0.0f);

	// ---- Run rules ----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Rules", meta=(ClampMin="1"))
	float RunLength = 30.0f;

	/** Whole seconds of 3-2-1 before targets appear. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Rules", meta=(ClampMin="0"))
	int32 CountdownSeconds = 3;

	/** Points per kill (Gridshot / Microshot). */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Rules")
	int32 PointsPerKill = 100;

	/** Points per hit (Tracking). */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Rules")
	int32 PointsPerTrackingHit = 10;

	/** A shot with no hit within this window counts as a miss and breaks the streak. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Rules", meta=(ClampMin="0"))
	float MissGraceTime = 0.3f;

	// ---- Gridshot -----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Gridshot", meta=(ClampMin="1"))
	int32 GridColumns = 5;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Gridshot", meta=(ClampMin="1"))
	int32 GridRows = 3;

	/** Distance between grid cell centres (Y, Z). */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Gridshot")
	FVector2D GridCellSize = FVector2D(170.0f, 170.0f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Gridshot", meta=(ClampMin="1", ClampMax="6"))
	int32 GridAliveCount = 3;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Gridshot", meta=(ClampMin="1"))
	float GridshotTargetRadius = 38.0f;

	// ---- Microshot ----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Microshot", meta=(ClampMin="1"))
	float MicroshotTargetRadius = 12.0f;

	/** Minimum distance between consecutive Microshot targets. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Microshot")
	float MicroshotMinSeparation = 250.0f;

	// ---- Tracking -----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Tracking", meta=(ClampMin="1"))
	float TrackingTargetRadius = 30.0f;

	/** Peak horizontal strafe speed, cm/s. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Tracking")
	float TrackingSpeed = 380.0f;

	/** How quickly the strafe velocity changes, cm/s^2. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Tracking")
	float TrackingAcceleration = 1400.0f;

	/** Random time between strafe direction changes, seconds (min, max). */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Tracking")
	FVector2D TrackingDirectionChangeInterval = FVector2D(0.4f, 1.3f);

	/** Vertical bob amplitude (cm) and period (s). */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Tracking")
	float TrackingVerticalAmplitude = 120.0f;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Tracking", meta=(ClampMin="0.1"))
	float TrackingVerticalPeriod = 3.2f;

	// ---- Look ---------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FLinearColor TargetColor = FLinearColor(0.0f, 0.85f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FLinearColor TrackingColor = FLinearColor(1.0f, 0.35f, 0.05f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	float TargetIntensity = 4.0f;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FLinearColor BackdropColor = FLinearColor(0.010f, 0.012f, 0.018f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FLinearColor ConsoleColor = FLinearColor(0.03f, 0.03f, 0.035f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FLinearColor ButtonColor = FLinearColor(0.08f, 0.10f, 0.14f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FLinearColor ButtonSelectedColor = FLinearColor(0.0f, 0.45f, 0.6f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FLinearColor StartButtonColor = FLinearColor(0.05f, 0.5f, 0.12f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FLinearColor FiringLineColor = FLinearColor(0.0f, 0.5f, 0.7f);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FColor TextColor = FColor(235, 240, 245);

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	FColor AccentTextColor = FColor(0, 220, 255);

	/** Pop: scale multiplier reached at the end of PopDuration. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	float PopScale = 1.7f;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look", meta=(ClampMin="0.01"))
	float PopDuration = 0.08f;

	UPROPERTY(EditAnywhere, Category="Aim Trainer|Look")
	bool bPlayImpactBurst = true;

private:

	enum class ERunState : uint8 { Idle, Countdown, Running, Results };

	static constexpr int32 TargetPoolSize = 8;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Backdrop;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> FiringLine;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> ConsoleRoot;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> ConsolePedestal;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> BoardHeader;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> BoardStats;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> BoardBests;

	/** Big text in the middle of the spawn area: title, countdown, results. */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> CenterText;

	UPROPERTY()
	TArray<FCarnageAimTarget> Targets;

	UPROPERTY()
	TArray<FCarnageAimButton> Buttons;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackdropMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ConsoleMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FiringLineMID;

	TWeakObjectPtr<AShooterCharacter> BoundShooter;

	// ---- Run state ----------------------------------------------------------------------------

	ERunState RunState = ERunState::Idle;
	ECarnageAimMode Mode = ECarnageAimMode::Gridshot;

	float StateTime = 0.0f;

	int32 Score = 0;
	int32 Hits = 0;
	int32 Kills = 0;
	int32 ShotsFired = 0;
	int32 Streak = 0;
	int32 BestStreak = 0;
	float LastKillTime = 0.0f;
	float TotalKillInterval = 0.0f;
	bool bNewBest = false;

	/** Session best score per mode. */
	int32 BestScores[(int32)ECarnageAimMode::Count] = {};

	/** Times (StateTime) of shots that have not been matched with a hit yet. */
	TArray<float> PendingShots;

	/** Hits this frame that arrived before their shot was counted (hitscan applies damage before ammo drops). */
	int32 UnmatchedHits = 0;

	int32 LastMagazineSize = INDEX_NONE;
	int32 LastBullets = INDEX_NONE;

	/** Previous cell / location, so replacements never land in the same place twice in a row. */
	int32 LastVacatedCell = INDEX_NONE;
	FVector LastMicroLocation = FVector::ZeroVector;

	// Tracking motion (spawn-plane local Y/Z).
	float TrackingVelocity = 0.0f;
	float TrackingDesiredVelocity = 0.0f;
	float TrackingNextTurn = 0.0f;
	float TrackingPhase = 0.0f;

	FRandomStream Rand;

	// ---- Helpers ------------------------------------------------------------------------------

	void ApplyLayout();
	void EnsureMaterials();
	void SetColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Intensity) const;

	void BindShooter();
	void UnbindShooter();

	UFUNCTION()
	void HandleBulletCountUpdated(int32 MagazineSize, int32 Bullets);

	void BeginRunning();
	void FinishRun();
	void ResetRunStats();

	void TickCountdown();
	void TickRunning(float DeltaSeconds);
	void TickTargets(float DeltaSeconds);
	void TickTracking(float DeltaSeconds);
	void TickMissDetection();

	void RegisterHit();
	void KillTarget(int32 Index);

	/** Activates a pooled target at a fresh position for the current mode. Returns its index or INDEX_NONE. */
	int32 SpawnTarget();
	int32 AcquireIdleTarget() const;
	void ActivateTarget(int32 Index, const FVector& LocalPos, float Radius, const FLinearColor& Color, int32 Cell);
	void HideTarget(int32 Index);
	void ClearTargets();

	FVector CellToLocal(int32 Cell) const;
	FVector RandomAreaPoint(float Radius) const;
	float CurrentTargetRadius() const;

	void RefreshButtons();
	void RefreshBoard();

	/** True while any target is mid pop animation (keeps the actor ticking after a run ends). */
	bool AnyTargetAnimating() const;

	/** Scoreboard text is rebuilt at this rate rather than every frame. */
	UPROPERTY(EditAnywhere, Category="Aim Trainer|Board", meta=(ClampMin=0.02, ClampMax=1, Units="s"))
	float BoardRefreshInterval = 0.1f;

	float BoardRefreshAccumulator = 0.0f;

	static FString ModeName(ECarnageAimMode InMode);
	float Accuracy() const;
	float AverageKillMs() const;
};
