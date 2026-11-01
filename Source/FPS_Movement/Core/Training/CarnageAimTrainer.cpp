// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarnageAimTrainer.h"

#include "CarnageWeaponFXSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "UObject/ConstructorHelpers.h"
#include "ShooterWeapon.h"

namespace AimTrainer
{
	/** /Engine/BasicShapes meshes are 100 cm across. */
	constexpr float BasicShapeSize = 100.0f;

	const FName ColorParam(TEXT("Color"));
	const FName IntensityParam(TEXT("Intensity"));

	/** Text faces +X by default; the trainer faces -X. */
	const FRotator FacePlayer(0.0f, 180.0f, 0.0f);

	FString FormatClock(float Seconds)
	{
		Seconds = FMath::Max(Seconds, 0.0f);
		const int32 Minutes = FMath::FloorToInt(Seconds / 60.0f);
		return FString::Printf(TEXT("%d:%04.1f"), Minutes, Seconds - Minutes * 60.0f);
	}

	UTextRenderComponent* MakeText(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UTextRenderComponent* Text = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
		Text->SetupAttachment(Parent);
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetRelativeRotation(FacePlayer);
		Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Text->SetCastShadow(false);
		return Text;
	}

	UStaticMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, FName Profile)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetCollisionProfileName(Profile);
		Mesh->SetCastShadow(false);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->CanCharacterStepUpOn = ECB_No;
		return Mesh;
	}
}

ACarnageAimTrainer::ACarnageAimTrainer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Game/Carnage/Training/M_AimTarget.M_AimTarget"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	SurfaceMaterial = MaterialFinder.Object;
	SphereMesh = SphereFinder.Object;
	CubeMesh = CubeFinder.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Static scenery blocks every channel so missed shots land on it like any wall.
	Backdrop = AimTrainer::MakeMesh(this, Root, TEXT("Backdrop"), UCollisionProfile::BlockAll_ProfileName);
	FiringLine = AimTrainer::MakeMesh(this, Root, TEXT("FiringLine"), UCollisionProfile::NoCollision_ProfileName);

	ConsoleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ConsoleRoot"));
	ConsoleRoot->SetupAttachment(Root);
	ConsolePedestal = AimTrainer::MakeMesh(this, ConsoleRoot, TEXT("ConsolePedestal"), UCollisionProfile::BlockAll_ProfileName);

	BoardHeader = AimTrainer::MakeText(this, Root, TEXT("BoardHeader"));
	BoardStats = AimTrainer::MakeText(this, Root, TEXT("BoardStats"));
	BoardBests = AimTrainer::MakeText(this, Root, TEXT("BoardBests"));
	CenterText = AimTrainer::MakeText(this, Root, TEXT("CenterText"));

	Targets.SetNum(TargetPoolSize);
	for (int32 i = 0; i < TargetPoolSize; ++i)
	{
		UStaticMeshComponent* Mesh = AimTrainer::MakeMesh(this, Root, *FString::Printf(TEXT("Target%d"), i), UCollisionProfile::BlockAllDynamic_ProfileName);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Targets[i].Mesh = Mesh;
	}

	// The fifth entry is the INF AMMO toggle (its Mode is unused).
	static const ECarnageAimMode ButtonModes[] = { ECarnageAimMode::Gridshot, ECarnageAimMode::Microshot, ECarnageAimMode::Tracking, ECarnageAimMode::Count, ECarnageAimMode::Count };
	static const TCHAR* ButtonLabels[] = { TEXT("GRIDSHOT"), TEXT("MICROSHOT"), TEXT("TRACKING"), TEXT("START"), TEXT("INF AMMO") };
	Buttons.SetNum(UE_ARRAY_COUNT(ButtonModes));
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		FCarnageAimButton& Button = Buttons[i];
		Button.Mode = ButtonModes[i];
		Button.Mesh = AimTrainer::MakeMesh(this, ConsoleRoot, *FString::Printf(TEXT("Button%d"), i), UCollisionProfile::BlockAllDynamic_ProfileName);
		Button.Label = AimTrainer::MakeText(this, ConsoleRoot, *FString::Printf(TEXT("ButtonLabel%d"), i));
		Button.Label->SetText(FText::FromString(ButtonLabels[i]));
		Button.bInfiniteAmmoToggle = (i == UE_ARRAY_COUNT(ButtonModes) - 1);
	}
}

// ---------------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------------

void ACarnageAimTrainer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLayout();
	EnsureMaterials();
	RefreshButtons();
	RefreshBoard();
}

FVector ACarnageAimTrainer::GetFiringPoint() const
{
	return GetActorTransform().TransformPosition(FVector(-FiringDistance, 0.0f, 0.0f));
}

void ACarnageAimTrainer::ApplyLayout()
{
	using namespace AimTrainer;

	const float AreaTop = AreaCenterHeight + AreaExtent.Y;
	const float WallTop = AreaTop + ScoreboardHeight;
	const float WallHalfWidth = AreaExtent.X + BackdropMargin;
	constexpr float WallThickness = 20.0f;
	const float TextX = BackdropOffset - 2.0f;

	Backdrop->SetStaticMesh(CubeMesh);
	Backdrop->SetRelativeLocation(FVector(BackdropOffset + WallThickness * 0.5f, 0.0f, WallTop * 0.5f));
	Backdrop->SetRelativeScale3D(FVector(WallThickness, WallHalfWidth * 2.0f, WallTop) / BasicShapeSize);

	FiringLine->SetStaticMesh(CubeMesh);
	FiringLine->SetRelativeLocation(FVector(-FiringDistance, 0.0f, 1.0f));
	FiringLine->SetRelativeScale3D(FVector(8.0f, FiringLineWidth, 1.0f) / BasicShapeSize);

	BoardHeader->SetRelativeLocation(FVector(TextX, 0.0f, AreaTop + ScoreboardHeight * 0.76f));
	BoardHeader->SetWorldSize(ScoreboardHeight * 0.2f);
	BoardStats->SetRelativeLocation(FVector(TextX, 0.0f, AreaTop + ScoreboardHeight * 0.45f));
	BoardStats->SetWorldSize(ScoreboardHeight * 0.1f);
	BoardBests->SetRelativeLocation(FVector(TextX, 0.0f, AreaTop + ScoreboardHeight * 0.15f));
	BoardBests->SetWorldSize(ScoreboardHeight * 0.075f);
	CenterText->SetRelativeLocation(FVector(TextX, 0.0f, AreaCenterHeight));

	// Console: turn so its front (-X) faces the firing point.
	const FVector ToConsole = ConsoleOffset - FVector(-FiringDistance, 0.0f, 0.0f);
	ConsoleRoot->SetRelativeLocation(ConsoleOffset);
	ConsoleRoot->SetRelativeRotation(FRotator(0.0f, ToConsole.Rotation().Yaw, 0.0f));

	constexpr float PedestalHeight = 110.0f;
	ConsolePedestal->SetStaticMesh(CubeMesh);
	ConsolePedestal->SetRelativeLocation(FVector(0.0f, 0.0f, PedestalHeight * 0.5f));
	ConsolePedestal->SetRelativeScale3D(FVector(50.0f, 260.0f, PedestalHeight) / BasicShapeSize);

	// Two-column grid of buttons standing on the pedestal (top row first), labels on their front faces.
	// An odd button out is centred on the bottom row.
	const FVector ButtonSize(16.0f, 115.0f, 45.0f);
	const int32 NumRows = (Buttons.Num() + 1) / 2;
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		const int32 Row = (NumRows - 1) - i / 2;
		const bool bAlone = (i == Buttons.Num() - 1) && (Buttons.Num() % 2 == 1);
		const float ColOffset = bAlone ? 0.0f : ((i % 2) - 0.5f);
		const FVector Pos(-10.0f, ColOffset * (ButtonSize.Y + 10.0f), PedestalHeight + 5.0f + ButtonSize.Z * 0.5f + Row * (ButtonSize.Z + 8.0f));

		Buttons[i].Mesh->SetStaticMesh(CubeMesh);
		Buttons[i].Mesh->SetRelativeLocation(Pos);
		Buttons[i].Mesh->SetRelativeScale3D(ButtonSize / BasicShapeSize);

		Buttons[i].Label->SetRelativeLocation(Pos - FVector(ButtonSize.X * 0.5f + 0.5f, 0.0f, 0.0f));
		Buttons[i].Label->SetWorldSize(16.0f);
		Buttons[i].Label->SetTextRenderColor(TextColor);
	}

	BoardHeader->SetTextRenderColor(AccentTextColor);
	BoardStats->SetTextRenderColor(TextColor);
	BoardBests->SetTextRenderColor(TextColor);
	CenterText->SetTextRenderColor(TextColor);

	for (FCarnageAimTarget& Target : Targets)
	{
		Target.Mesh->SetStaticMesh(SphereMesh);
	}
}

void ACarnageAimTrainer::EnsureMaterials()
{
	UMaterialInterface* Parent = SurfaceMaterial ? SurfaceMaterial.Get() : UMaterial::GetDefaultMaterial(MD_Surface);

	auto Ensure = [Parent](UStaticMeshComponent* Mesh, TObjectPtr<UMaterialInstanceDynamic>& MID)
	{
		if (!MID || MID->Parent != Parent)
		{
			MID = Mesh->CreateDynamicMaterialInstance(0, Parent);
		}
	};

	Ensure(Backdrop, BackdropMID);
	Ensure(ConsolePedestal, ConsoleMID);
	Ensure(FiringLine, FiringLineMID);
	for (FCarnageAimTarget& Target : Targets)
	{
		Ensure(Target.Mesh, Target.MID);
	}
	for (FCarnageAimButton& Button : Buttons)
	{
		Ensure(Button.Mesh, Button.MID);
	}

	SetColor(BackdropMID, BackdropColor, 1.0f);
	SetColor(ConsoleMID, ConsoleColor, 1.0f);
	SetColor(FiringLineMID, FiringLineColor, 2.0f);
}

void ACarnageAimTrainer::SetColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Intensity) const
{
	if (MID)
	{
		MID->SetVectorParameterValue(AimTrainer::ColorParam, Color);
		MID->SetScalarParameterValue(AimTrainer::IntensityParam, Intensity);
	}
}

// ---------------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------------

void ACarnageAimTrainer::BeginPlay()
{
	Super::BeginPlay();

	Rand.Initialize(FMath::Rand());
	ApplyLayout();
	EnsureMaterials();
	ClearTargets();
	RunState = ERunState::Idle;
	RefreshButtons();
	RefreshBoard();
}

void ACarnageAimTrainer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindShooter();
	Super::EndPlay(EndPlayReason);
}

void ACarnageAimTrainer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	StateTime += DeltaSeconds;

	switch (RunState)
	{
	case ERunState::Countdown:
		TickCountdown();
		break;
	case ERunState::Running:
		TickRunning(DeltaSeconds);
		break;
	default:
		break;
	}

	TickTargets(DeltaSeconds);

	// The board is text formatting, so throttle it instead of rebuilding strings every frame.
	BoardRefreshAccumulator += DeltaSeconds;
	if (BoardRefreshAccumulator >= BoardRefreshInterval)
	{
		BoardRefreshAccumulator = 0.0f;
		RefreshBoard();
	}

	// Nothing to animate between runs: sleep until a mode button starts the next one.
	if ((RunState == ERunState::Idle || RunState == ERunState::Results) && !AnyTargetAnimating())
	{
		RefreshBoard();
		SetActorTickEnabled(false);
	}
}

bool ACarnageAimTrainer::AnyTargetAnimating() const
{
	for (const FCarnageAimTarget& Target : Targets)
	{
		if (Target.State == ECarnageAimTargetState::Popping)
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------
// Shots
// ---------------------------------------------------------------------------------------------------

bool ACarnageAimTrainer::HandleShot(const FShooterHitInfo& Hit, FShooterHitOutcome& OutOutcome)
{
	if (!Hit.HitComponent)
	{
		return false;
	}

	for (const FCarnageAimButton& Button : Buttons)
	{
		if (Button.Mesh == Hit.HitComponent && Button.bInfiniteAmmoToggle)
		{
			AShooterWeapon::SetGlobalInfiniteAmmo(!AShooterWeapon::IsGlobalInfiniteAmmo());
			RefreshButtons();
			OutOutcome.DamageDealt = 1.0f;
			return true;
		}
		if (Button.Mesh == Hit.HitComponent)
		{
			StartRun(Button.Mode == ECarnageAimMode::Count ? Mode : Button.Mode);
			OutOutcome.DamageDealt = 1.0f;
			return true;
		}
	}

	if (RunState != ERunState::Running)
	{
		return false;
	}

	for (int32 i = 0; i < Targets.Num(); ++i)
	{
		FCarnageAimTarget& Target = Targets[i];
		if (Target.Mesh != Hit.HitComponent || Target.State != ECarnageAimTargetState::Alive)
		{
			continue;
		}

		RegisterHit();
		OutOutcome.DamageDealt = 1.0f;

		if (Mode == ECarnageAimMode::Tracking)
		{
			// Tracking targets never die; each hit scores and flashes.
			Score += PointsPerTrackingHit;
			Target.Flash = 1.0f;
		}
		else
		{
			++Kills;
			Score += PointsPerKill;
			TotalKillInterval += StateTime - LastKillTime;
			LastKillTime = StateTime;
			KillTarget(i);
			OutOutcome.bKilled = true;
		}
		return true;
	}

	// Backdrop / pedestal: let the normal impact path handle it.
	return false;
}

void ACarnageAimTrainer::RegisterHit()
{
	++Hits;
	++Streak;
	BestStreak = FMath::Max(BestStreak, Streak);

	// Hitscan applies the hit before the ammo count drops, so the matching shot may not exist yet.
	if (PendingShots.Num() > 0)
	{
		PendingShots.RemoveAt(0);
	}
	else
	{
		++UnmatchedHits;
	}
}

void ACarnageAimTrainer::BindShooter()
{
	AShooterCharacter* Shooter = Cast<AShooterCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Shooter != BoundShooter.Get())
	{
		UnbindShooter();
		if (Shooter)
		{
			Shooter->OnBulletCountUpdated.AddUniqueDynamic(this, &ACarnageAimTrainer::HandleBulletCountUpdated);
			BoundShooter = Shooter;
		}
	}

	const AShooterWeapon* Weapon = Shooter ? Shooter->GetCurrentWeapon() : nullptr;
	LastMagazineSize = Weapon ? Weapon->GetMagazineSize() : INDEX_NONE;
	LastBullets = Weapon ? Weapon->GetBulletCount() : INDEX_NONE;
}

void ACarnageAimTrainer::UnbindShooter()
{
	if (AShooterCharacter* Shooter = BoundShooter.Get())
	{
		Shooter->OnBulletCountUpdated.RemoveDynamic(this, &ACarnageAimTrainer::HandleBulletCountUpdated);
	}
	BoundShooter.Reset();
}

void ACarnageAimTrainer::HandleBulletCountUpdated(int32 MagazineSize, int32 Bullets)
{
	// A drop in the same magazine is a shot; weapon swaps (different magazine) and reloads are not.
	const bool bSameMagazine = MagazineSize == LastMagazineSize && LastBullets != INDEX_NONE;
	if (RunState == ERunState::Running && bSameMagazine && Bullets < LastBullets)
	{
		for (int32 Shot = Bullets; Shot < LastBullets; ++Shot)
		{
			++ShotsFired;
			if (UnmatchedHits > 0)
			{
				--UnmatchedHits;
			}
			else
			{
				PendingShots.Add(StateTime);
			}
		}
	}

	LastMagazineSize = MagazineSize;
	LastBullets = Bullets;
}

// ---------------------------------------------------------------------------------------------------
// Run flow
// ---------------------------------------------------------------------------------------------------

void ACarnageAimTrainer::StartRun(ECarnageAimMode NewMode)
{
	if (NewMode == ECarnageAimMode::Count)
	{
		return;
	}

	SetActorTickEnabled(true);

	Mode = NewMode;
	ClearTargets();
	ResetRunStats();
	BindShooter();

	RunState = ERunState::Countdown;
	StateTime = 0.0f;
	if (CountdownSeconds <= 0)
	{
		BeginRunning();
	}

	RefreshButtons();
	RefreshBoard();
}

void ACarnageAimTrainer::ResetRunStats()
{
	Score = 0;
	Hits = 0;
	Kills = 0;
	ShotsFired = 0;
	Streak = 0;
	BestStreak = 0;
	LastKillTime = 0.0f;
	TotalKillInterval = 0.0f;
	bNewBest = false;
	PendingShots.Reset();
	UnmatchedHits = 0;
	LastVacatedCell = INDEX_NONE;
}

void ACarnageAimTrainer::TickCountdown()
{
	if (StateTime >= CountdownSeconds)
	{
		BeginRunning();
	}
}

void ACarnageAimTrainer::BeginRunning()
{
	RunState = ERunState::Running;
	StateTime = 0.0f;
	LastKillTime = 0.0f;

	int32 Count = 1;
	if (Mode == ECarnageAimMode::Gridshot)
	{
		// Leave at least one free cell so a replacement always has somewhere new to go.
		Count = FMath::Clamp(GridAliveCount, 1, FMath::Max(GridColumns * GridRows - 2, 1));
	}
	else if (Mode == ECarnageAimMode::Tracking)
	{
		TrackingVelocity = 0.0f;
		TrackingDesiredVelocity = 0.0f;
		TrackingNextTurn = 0.0f;
		TrackingPhase = 0.0f;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		SpawnTarget();
	}
}

void ACarnageAimTrainer::TickRunning(float DeltaSeconds)
{
	if (StateTime >= RunLength)
	{
		FinishRun();
		return;
	}

	if (Mode == ECarnageAimMode::Tracking)
	{
		TickTracking(DeltaSeconds);
	}
	TickMissDetection();
}

void ACarnageAimTrainer::TickMissDetection()
{
	while (PendingShots.Num() > 0 && StateTime - PendingShots[0] > MissGraceTime)
	{
		PendingShots.RemoveAt(0);
		Streak = 0;
	}

	// Extra hits from one shot (pellets) never get a matching shot; don't carry them over.
	UnmatchedHits = 0;
}

void ACarnageAimTrainer::FinishRun()
{
	RunState = ERunState::Results;
	StateTime = 0.0f;
	ClearTargets();
	PendingShots.Reset();

	int32& Best = BestScores[(int32)Mode];
	bNewBest = Score > Best;
	Best = FMath::Max(Best, Score);
}

// ---------------------------------------------------------------------------------------------------
// Targets
// ---------------------------------------------------------------------------------------------------

float ACarnageAimTrainer::CurrentTargetRadius() const
{
	switch (Mode)
	{
	case ECarnageAimMode::Microshot: return MicroshotTargetRadius;
	case ECarnageAimMode::Tracking:  return TrackingTargetRadius;
	default:                         return GridshotTargetRadius;
	}
}

FVector ACarnageAimTrainer::CellToLocal(int32 Cell) const
{
	const int32 Col = Cell % GridColumns;
	const int32 Row = Cell / GridColumns;
	return FVector(
		0.0f,
		(Col - (GridColumns - 1) * 0.5f) * GridCellSize.X,
		AreaCenterHeight + (Row - (GridRows - 1) * 0.5f) * GridCellSize.Y);
}

FVector ACarnageAimTrainer::RandomAreaPoint(float Radius) const
{
	const float HalfY = FMath::Max(AreaExtent.X - Radius, 0.0f);
	const float HalfZ = FMath::Max(AreaExtent.Y - Radius, 0.0f);
	return FVector(0.0f, Rand.FRandRange(-HalfY, HalfY), AreaCenterHeight + Rand.FRandRange(-HalfZ, HalfZ));
}

int32 ACarnageAimTrainer::AcquireIdleTarget() const
{
	return Targets.IndexOfByPredicate([](const FCarnageAimTarget& T) { return T.State == ECarnageAimTargetState::Idle; });
}

int32 ACarnageAimTrainer::SpawnTarget()
{
	const int32 Index = AcquireIdleTarget();
	if (Index == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const float Radius = CurrentTargetRadius();

	switch (Mode)
	{
	case ECarnageAimMode::Gridshot:
	{
		const int32 NumCells = GridColumns * GridRows;
		TArray<int32> Free;
		for (int32 Cell = 0; Cell < NumCells; ++Cell)
		{
			const bool bOccupied = Targets.ContainsByPredicate([Cell](const FCarnageAimTarget& T)
			{
				return T.State != ECarnageAimTargetState::Idle && T.Cell == Cell;
			});
			if (!bOccupied && Cell != LastVacatedCell)
			{
				Free.Add(Cell);
			}
		}
		if (Free.Num() == 0)
		{
			return INDEX_NONE;
		}
		const int32 Cell = Free[Rand.RandRange(0, Free.Num() - 1)];
		ActivateTarget(Index, CellToLocal(Cell), Radius, TargetColor, Cell);
		break;
	}
	case ECarnageAimMode::Microshot:
	{
		FVector Pos = RandomAreaPoint(Radius);
		for (int32 Try = 0; Try < 16 && FVector::DistSquared(Pos, LastMicroLocation) < FMath::Square(MicroshotMinSeparation); ++Try)
		{
			Pos = RandomAreaPoint(Radius);
		}
		LastMicroLocation = Pos;
		ActivateTarget(Index, Pos, Radius, TargetColor, INDEX_NONE);
		break;
	}
	case ECarnageAimMode::Tracking:
	default:
		ActivateTarget(Index, FVector(0.0f, 0.0f, AreaCenterHeight), Radius, TrackingColor, INDEX_NONE);
		break;
	}

	return Index;
}

void ACarnageAimTrainer::ActivateTarget(int32 Index, const FVector& LocalPos, float Radius, const FLinearColor& Color, int32 Cell)
{
	FCarnageAimTarget& Target = Targets[Index];
	Target.State = ECarnageAimTargetState::Alive;
	Target.Cell = Cell;
	Target.Radius = Radius;
	Target.PopAge = 0.0f;
	Target.Flash = 0.0f;

	Target.Mesh->SetRelativeLocation(LocalPos);
	Target.Mesh->SetRelativeScale3D(FVector(Radius * 2.0f / AimTrainer::BasicShapeSize));
	Target.Mesh->SetVisibility(true);
	Target.Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetColor(Target.MID, Color, TargetIntensity);
}

void ACarnageAimTrainer::KillTarget(int32 Index)
{
	FCarnageAimTarget& Target = Targets[Index];
	Target.State = ECarnageAimTargetState::Popping;
	Target.PopAge = 0.0f;
	Target.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetColor(Target.MID, FLinearColor::White, TargetIntensity * 1.5f);
	LastVacatedCell = Target.Cell;

	if (bPlayImpactBurst)
	{
		if (UCarnageWeaponFXSubsystem* FX = UCarnageWeaponFXSubsystem::Get(this))
		{
			FCarnageImpactFXParams Params;
			Params.Location = Target.Mesh->GetComponentLocation() - GetActorForwardVector() * Target.Radius;
			Params.Normal = -GetActorForwardVector();
			Params.ShotDirection = GetActorForwardVector();
			Params.bHitPawn = true;	// pawn preset: burst, no decal
			Params.HitComponent = Target.Mesh;
			FX->PlayImpact(Params);
		}
	}

	SpawnTarget();
}

void ACarnageAimTrainer::HideTarget(int32 Index)
{
	FCarnageAimTarget& Target = Targets[Index];
	Target.State = ECarnageAimTargetState::Idle;
	Target.Cell = INDEX_NONE;
	Target.Mesh->SetVisibility(false);
	Target.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACarnageAimTrainer::ClearTargets()
{
	for (int32 i = 0; i < Targets.Num(); ++i)
	{
		HideTarget(i);
	}
}

void ACarnageAimTrainer::TickTargets(float DeltaSeconds)
{
	for (int32 i = 0; i < Targets.Num(); ++i)
	{
		FCarnageAimTarget& Target = Targets[i];
		if (Target.State == ECarnageAimTargetState::Popping)
		{
			Target.PopAge += DeltaSeconds;
			const float Alpha = Target.PopAge / PopDuration;
			if (Alpha >= 1.0f)
			{
				HideTarget(i);
				continue;
			}
			const float Scale = FMath::Lerp(1.0f, PopScale, FMath::Sin(Alpha * HALF_PI));
			Target.Mesh->SetRelativeScale3D(FVector(Target.Radius * 2.0f * Scale / AimTrainer::BasicShapeSize));
		}
		else if (Target.State == ECarnageAimTargetState::Alive && Target.Flash > 0.0f)
		{
			Target.Flash = FMath::Max(Target.Flash - DeltaSeconds * 6.0f, 0.0f);
			SetColor(Target.MID, FLinearColor::LerpUsingHSV(TrackingColor, FLinearColor::White, Target.Flash * 0.6f), TargetIntensity * (1.0f + Target.Flash));
		}
	}
}

void ACarnageAimTrainer::TickTracking(float DeltaSeconds)
{
	const int32 Index = Targets.IndexOfByPredicate([](const FCarnageAimTarget& T) { return T.State == ECarnageAimTargetState::Alive; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	FCarnageAimTarget& Target = Targets[Index];
	FVector Pos = Target.Mesh->GetRelativeLocation();
	const float LimitY = FMath::Max(AreaExtent.X - Target.Radius, 0.0f);

	// Random strafes with constant acceleration, turning early enough to stop inside the area.
	TrackingNextTurn -= DeltaSeconds;
	if (TrackingNextTurn <= 0.0f)
	{
		const float Dir = Rand.FRand() < 0.5f ? -1.0f : 1.0f;
		TrackingDesiredVelocity = Dir * TrackingSpeed * Rand.FRandRange(0.6f, 1.0f);
		TrackingNextTurn = Rand.FRandRange(TrackingDirectionChangeInterval.X, TrackingDirectionChangeInterval.Y);
	}

	const float StopDistance = FMath::Square(TrackingVelocity) / (2.0f * FMath::Max(TrackingAcceleration, 1.0f));
	if (Pos.Y + StopDistance >= LimitY && TrackingDesiredVelocity > 0.0f)
	{
		TrackingDesiredVelocity = -TrackingDesiredVelocity;
	}
	else if (Pos.Y - StopDistance <= -LimitY && TrackingDesiredVelocity < 0.0f)
	{
		TrackingDesiredVelocity = -TrackingDesiredVelocity;
	}

	TrackingVelocity = FMath::FInterpConstantTo(TrackingVelocity, TrackingDesiredVelocity, DeltaSeconds, TrackingAcceleration);
	Pos.Y = FMath::Clamp(Pos.Y + TrackingVelocity * DeltaSeconds, -LimitY, LimitY);

	TrackingPhase += DeltaSeconds * TWO_PI / TrackingVerticalPeriod;
	const float Amplitude = FMath::Min(TrackingVerticalAmplitude, FMath::Max(AreaExtent.Y - Target.Radius, 0.0f));
	Pos.Z = AreaCenterHeight + Amplitude * FMath::Sin(TrackingPhase);

	Target.Mesh->SetRelativeLocation(Pos);
}

// ---------------------------------------------------------------------------------------------------
// Presentation
// ---------------------------------------------------------------------------------------------------

FString ACarnageAimTrainer::ModeName(ECarnageAimMode InMode)
{
	switch (InMode)
	{
	case ECarnageAimMode::Gridshot:  return TEXT("GRIDSHOT");
	case ECarnageAimMode::Microshot: return TEXT("MICROSHOT");
	case ECarnageAimMode::Tracking:  return TEXT("TRACKING-LITE");
	default:                         return TEXT("");
	}
}

float ACarnageAimTrainer::Accuracy() const
{
	return ShotsFired > 0 ? FMath::Min((float)Hits / ShotsFired, 1.0f) * 100.0f : 0.0f;
}

float ACarnageAimTrainer::AverageKillMs() const
{
	return Kills > 0 ? TotalKillInterval / Kills * 1000.0f : 0.0f;
}

void ACarnageAimTrainer::RefreshButtons()
{
	for (FCarnageAimButton& Button : Buttons)
	{
		if (Button.bInfiniteAmmoToggle)
		{
			const bool bOn = AShooterWeapon::IsGlobalInfiniteAmmo();
			// Dark when off so the state reads at a glance from the firing line, not just from the label.
			constexpr float OffBrightness = 0.12f;
			SetColor(Button.MID, bOn ? StartButtonColor : ButtonColor * OffBrightness, 1.0f);
			Button.Label->SetText(FText::FromString(bOn ? TEXT("INF AMMO: ON") : TEXT("INF AMMO: OFF")));
			continue;
		}
		const bool bStart = Button.Mode == ECarnageAimMode::Count;
		const bool bSelected = !bStart && Button.Mode == Mode && RunState != ERunState::Idle;
		SetColor(Button.MID, bStart ? StartButtonColor : (bSelected ? ButtonSelectedColor : ButtonColor), 1.0f);
	}
}

void ACarnageAimTrainer::RefreshBoard()
{
	auto SetIfChanged = [](UTextRenderComponent* Text, const FString& Value)
	{
		if (!Text->Text.ToString().Equals(Value, ESearchCase::CaseSensitive))
		{
			Text->SetText(FText::FromString(Value));
		}
	};

	const bool bTracking = Mode == ECarnageAimMode::Tracking;
	const FString Ttk = (bTracking || Kills == 0) ? TEXT("--") : FString::Printf(TEXT("%d ms"), FMath::RoundToInt(AverageKillMs()));
	const int32 AccuracyPct = FMath::RoundToInt(Accuracy());

	// Header
	FString Header;
	switch (RunState)
	{
	case ERunState::Idle:      Header = TEXT("AIM TRAINER"); break;
	case ERunState::Countdown: Header = FString::Printf(TEXT("%s    %s"), *ModeName(Mode), *AimTrainer::FormatClock(RunLength)); break;
	case ERunState::Running:   Header = FString::Printf(TEXT("%s    %s"), *ModeName(Mode), *AimTrainer::FormatClock(RunLength - StateTime)); break;
	case ERunState::Results:   Header = FString::Printf(TEXT("%s    RESULTS"), *ModeName(Mode)); break;
	}
	SetIfChanged(BoardHeader, Header);

	SetIfChanged(BoardStats, FString::Printf(
		TEXT("SCORE %d     HITS %d     SHOTS %d     ACC %d%%\nAVG TTK %s     STREAK %d     BEST STREAK %d"),
		Score, Hits, ShotsFired, AccuracyPct, *Ttk, Streak, BestStreak));

	SetIfChanged(BoardBests, FString::Printf(
		TEXT("SESSION BEST     GRIDSHOT %d     MICROSHOT %d     TRACKING %d"),
		BestScores[(int32)ECarnageAimMode::Gridshot],
		BestScores[(int32)ECarnageAimMode::Microshot],
		BestScores[(int32)ECarnageAimMode::Tracking]));

	// Centre of the target area
	FString Center;
	float CenterSize = AreaExtent.Y * 0.2f;
	switch (RunState)
	{
	case ERunState::Idle:
		Center = TEXT("SHOOT A MODE ON THE CONSOLE TO BEGIN");
		break;
	case ERunState::Countdown:
		Center = FString::FromInt(FMath::Max(CountdownSeconds - FMath::FloorToInt(StateTime), 1));
		CenterSize = AreaExtent.Y * 0.9f;
		break;
	case ERunState::Running:
		Center = StateTime < 0.4f ? TEXT("GO!") : TEXT("");
		CenterSize = AreaExtent.Y * 0.6f;
		break;
	case ERunState::Results:
		Center = FString::Printf(
			TEXT("SCORE %d%s\nACCURACY %d%%     AVG TTK %s\nHITS %d / SHOTS %d     BEST STREAK %d\n\nSHOOT START TO GO AGAIN"),
			Score, bNewBest ? TEXT("  NEW BEST!") : TEXT(""), AccuracyPct, *Ttk, Hits, ShotsFired, BestStreak);
		CenterSize = AreaExtent.Y * 0.17f;
		break;
	}
	SetIfChanged(CenterText, Center);
	if (!FMath::IsNearlyEqual(CenterText->WorldSize, CenterSize))
	{
		CenterText->SetWorldSize(CenterSize);
	}
}
