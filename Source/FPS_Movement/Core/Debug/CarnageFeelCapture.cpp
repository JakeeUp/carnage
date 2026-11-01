#include "CarnageFeelCapture.h"
#include "ShooterCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DoomMovementComponent.h"
#include "ShooterNPC.h"
#include "CarnageAimTrainer.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FPS_Movement.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarFeelCapture(
	TEXT("carnage.Debug.FeelCapture"), 0,
	TEXT("1 = run the scripted weapon-feel screenshot sequence once the player pawn exists."));

static TAutoConsoleVariable<FString> CVarFeelCaptureTour(
	TEXT("carnage.Debug.FeelCaptureTour"), TEXT(""),
	TEXT("Optional viewpoints visited after the feel sequence: \"x y z yaw pitch|x y z yaw pitch|...\". Each gets a screenshot."));

static TAutoConsoleVariable<int32> CVarMoveTest(
	TEXT("carnage.Debug.MoveTest"), 0,
	TEXT("1 = with FeelCapture, run the scripted movement test (slide hill + jump arcs) and log telemetry instead of the weapon sequence."));

static TAutoConsoleVariable<int32> CVarCombatTest(
	TEXT("carnage.Debug.CombatTest"), 0,
	TEXT("1 = with FeelCapture, shoot the nearest arena NPCs and then the aim trainer, logging hits/kills and taking screenshots."));

static TAutoConsoleVariable<int32> CVarFeelCaptureQuit(
	TEXT("carnage.Debug.FeelCaptureQuit"), 1,
	TEXT("1 = quit the game when the feel capture sequence finishes."));
#endif

bool UCarnageFeelCapture::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
#endif
}

TStatId UCarnageFeelCapture::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCarnageFeelCapture, STATGROUP_Tickables);
}

static AShooterCharacter* GetPlayerShooter(UWorld* World)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<AShooterCharacter>(PC->GetPawn()) : nullptr;
}

static void SetForceADS(int32 Value)
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("carnage.Debug.ForceADS")))
	{
		Var->Set(Value, ECVF_SetByCode);
	}
}

void UCarnageFeelCapture::Shot(const TCHAR* Name)
{
	FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Feel_%s"), Name), /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
}

void UCarnageFeelCapture::BuildSequence()
{
	UWorld* World = GetWorld();
	auto Fire = [World]() { if (AShooterCharacter* C = GetPlayerShooter(World)) { C->DoStartFiring(); } };
	auto Cease = [World]() { if (AShooterCharacter* C = GetPlayerShooter(World)) { C->DoStopFiring(); } };

	Steps = {
		{ 1.0f,  [this]() { Shot(TEXT("01_Hip")); } },
		{ 1.5f,  Fire },
		{ 1.52f, [this]() { Shot(TEXT("02_HipShot")); } },
		{ 1.56f, [this]() { Shot(TEXT("02b_HipShotTracer")); } },
		{ 1.6f,  Cease },
		{ 2.5f,  []() { SetForceADS(1); } },
		{ 2.58f, [this]() { Shot(TEXT("03_ADSBlend")); } },
		{ 3.5f,  [this]() { Shot(TEXT("04_ADS")); } },
		{ 4.0f,  Fire },
		{ 4.02f, [this]() { Shot(TEXT("05_ADSShot")); } },
		{ 4.06f, [this]() { Shot(TEXT("05b_ADSShotImpact")); } },
		{ 4.1f,  Cease },
		{ 5.0f,  []() { SetForceADS(0); } },
	};

	// Optional tour: teleport the player to each viewpoint, let it settle, then screenshot.
	float T = 5.5f;
	int32 Index = 0;
#if !UE_BUILD_SHIPPING
	TArray<FString> Points;
	CVarFeelCaptureTour.GetValueOnGameThread().ParseIntoArray(Points, TEXT("|"));
	for (const FString& Point : Points)
	{
		TArray<FString> V;
		Point.ParseIntoArrayWS(V);
		if (V.Num() < 4)
		{
			continue;
		}
		const FVector Loc(FCString::Atof(*V[0]), FCString::Atof(*V[1]), FCString::Atof(*V[2]));
		const FRotator Rot(V.Num() > 4 ? FCString::Atof(*V[4]) : 0.0f, FCString::Atof(*V[3]), 0.0f);
		const FString Name = FString::Printf(TEXT("Tour_%02d"), Index++);
		Steps.Add({ T, [World, Loc, Rot]()
			{
				if (AShooterCharacter* C = GetPlayerShooter(World))
				{
					C->TeleportTo(Loc, FRotator(0.0f, Rot.Yaw, 0.0f), false, true);
					if (AController* Ctrl = C->GetController())
					{
						Ctrl->SetControlRotation(Rot);
					}
				}
			} });
		Steps.Add({ T + 0.8f, [this, Name]() { Shot(*Name); } });
		T += 1.2f;
	}
#endif

	Steps.Append({
		{ T + 0.5f,  [World]()
			{
#if !UE_BUILD_SHIPPING
				if (CVarFeelCaptureQuit.GetValueOnGameThread() != 0)
				{
					UKismetSystemLibrary::QuitGame(World, nullptr, EQuitPreference::Quit, false);
				}
#endif
			} },
	});
}

void UCarnageFeelCapture::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if !UE_BUILD_SHIPPING
	if (!bStarted)
	{
		if (CVarFeelCapture.GetValueOnGameThread() == 0 || !GetPlayerShooter(GetWorld()))
		{
			return;
		}
		bStarted = true;
		if (CVarCombatTest.GetValueOnGameThread() != 0)
		{
			BuildCombatTest();
		}
		else if (CVarMoveTest.GetValueOnGameThread() != 0)
		{
			BuildMoveTest();
		}
		else
		{
			BuildSequence();
		}
	}

	if (bHoldForward)
	{
		if (AShooterCharacter* C = GetPlayerShooter(GetWorld()))
		{
			C->AddMovementInput(C->GetActorForwardVector(), 1.0f);
		}
	}

	if (!Phase.IsEmpty() && Elapsed >= NextLogTime)
	{
		NextLogTime = Elapsed + 0.05f;
		LogMovement();
	}

	// Real time so kill hit-stop can't stall the script; clamped so the post-load hitch frame
	// doesn't fire the whole sequence at once. One step per frame so screenshots don't collide.
	Elapsed += FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 1.0f / 30.0f);
	if (Steps.IsValidIndex(NextStep) && Elapsed >= Steps[NextStep].Time)
	{
		Steps[NextStep++].Action();
	}
#endif
}

void UCarnageFeelCapture::LogMovement()
{
	const AShooterCharacter* C = GetPlayerShooter(GetWorld());
	const UDoomMovementComponent* M = C ? Cast<UDoomMovementComponent>(C->GetCharacterMovement()) : nullptr;
	if (!M)
	{
		return;
	}
	const FVector L = C->GetActorLocation();
	const TCHAR* State = M->IsSliding() ? TEXT("SLIDE") : M->IsFalling() ? TEXT("AIR") : TEXT("GROUND");
	UE_LOG(LogFPS_Movement, Display, TEXT("[MoveTest] %s t=%.2f x=%.0f z=%.1f spd2D=%.0f vz=%.0f state=%s"),
		*Phase, Elapsed, L.X, L.Z, M->Velocity.Size2D(), M->Velocity.Z, State);
}

void UCarnageFeelCapture::BuildMoveTest()
{
	UWorld* World = GetWorld();
	auto Teleport = [World](const FVector& Loc)
	{
		if (AShooterCharacter* C = GetPlayerShooter(World))
		{
			C->GetCharacterMovement()->StopMovementImmediately();
			C->TeleportTo(Loc, FRotator::ZeroRotator, false, true);
			if (AController* Ctrl = C->GetController())
			{
				Ctrl->SetControlRotation(FRotator::ZeroRotator);
			}
		}
	};
	auto Move = [World]() { AShooterCharacter* C = GetPlayerShooter(World); return C ? Cast<UDoomMovementComponent>(C->GetCharacterMovement()) : nullptr; };

	// Showcase slide hill: flat crest X 2300..2700 at Z 250, 14 deg down-ramp to X 3700.
	Steps = {
		{ 1.0f,  [this, Teleport]() { Teleport(FVector(2320.0f, 0.0f, 400.0f)); } },
		{ 1.8f,  [this, Move]() { Phase = TEXT("slide"); bHoldForward = true; if (auto* M = Move()) { M->StartSprint(); } } },
		{ 2.15f, [Move]() { if (auto* M = Move()) { M->StartSlide(); } } },
		{ 5.0f,  [this, Move]() { bHoldForward = false; if (auto* M = Move()) { M->StopSlide(); M->StopSprint(); } Phase.Empty(); } },

		// Jump arcs on flat ground: held jump, then a tap.
		{ 5.5f,  [this, Teleport]() { Teleport(FVector(-300.0f, 0.0f, 120.0f)); } },
		{ 6.5f,  [this, World]() { Phase = TEXT("jump_hold"); if (AShooterCharacter* C = GetPlayerShooter(World)) { C->DoJumpStart(); } } },
		{ 7.2f,  [World]() { if (AShooterCharacter* C = GetPlayerShooter(World)) { C->DoJumpEnd(); } } },
		{ 8.0f,  [this]() { Phase.Empty(); } },
		{ 8.5f,  [this, World]() { Phase = TEXT("jump_tap"); if (AShooterCharacter* C = GetPlayerShooter(World)) { C->DoJumpStart(); } } },
		{ 8.56f, [World]() { if (AShooterCharacter* C = GetPlayerShooter(World)) { C->DoJumpEnd(); } } },
		{ 9.5f,  [this]() { Phase.Empty(); } },
		{ 10.0f, [World]()
			{
#if !UE_BUILD_SHIPPING
				if (CVarFeelCaptureQuit.GetValueOnGameThread() != 0)
				{
					UKismetSystemLibrary::QuitGame(World, nullptr, EQuitPreference::Quit, false);
				}
#endif
			} },
	};
}

void UCarnageFeelCapture::AimAt(const FVector& WorldPoint) const
{
	AShooterCharacter* C = GetPlayerShooter(GetWorld());
	if (!C || !C->GetController() || !C->GetFirstPersonCameraComponent())
	{
		return;
	}
	const FVector Eye = C->GetFirstPersonCameraComponent()->GetComponentLocation();
	C->GetController()->SetControlRotation((WorldPoint - Eye).Rotation());
}

bool UCarnageFeelCapture::AimAtNearestLivingNPC() const
{
	const AShooterCharacter* C = GetPlayerShooter(GetWorld());
	if (!C)
	{
		return false;
	}
	const AShooterNPC* Best = nullptr;
	double BestDist = TNumericLimits<double>::Max();
	for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(FName("Dead")))
		{
			continue;
		}
		// Only consider NPCs the gun can actually see (same channel as hitscan).
		FHitResult LOS;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(C);
		const FVector Eye = C->GetFirstPersonCameraComponent()->GetComponentLocation();
		const FVector Aim = It->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		if (GetWorld()->LineTraceSingleByChannel(LOS, Eye, Aim, ECC_Weapon, Params) && LOS.GetActor() != *It)
		{
			continue;
		}
		const double D = FVector::DistSquared(It->GetActorLocation(), C->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	if (!Best)
	{
		UE_LOG(LogFPS_Movement, Display, TEXT("[CombatTest] no NPC in line of sight"));
		return false;
	}
	UE_LOG(LogFPS_Movement, Display, TEXT("[CombatTest] aiming at %s"), *Best->GetName());
	AimAt(Best->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f));
	return true;
}

bool UCarnageFeelCapture::AimAtTrainerComponent(const FString& NamePrefix) const
{
	for (TActorIterator<ACarnageAimTrainer> It(GetWorld()); It; ++It)
	{
		TArray<UStaticMeshComponent*> Meshes;
		It->GetComponents(Meshes);
		for (const UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetName().StartsWith(NamePrefix) && Mesh->IsVisible())
			{
				AimAt(Mesh->Bounds.Origin);
				return true;
			}
		}
	}
	return false;
}

void UCarnageFeelCapture::LogCombatState(const TCHAR* Label) const
{
	int32 Alive = 0;
	int32 Dead = 0;
	for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
	{
		(It->ActorHasTag(FName("Dead")) ? Dead : Alive)++;
	}
	UE_LOG(LogFPS_Movement, Display, TEXT("[CombatTest] %s npcs_alive=%d npcs_dead=%d"), Label, Alive, Dead);
}

void UCarnageFeelCapture::BuildCombatTest()
{
	UWorld* World = GetWorld();
	auto Teleport = [World](const FVector& Loc, float Yaw)
	{
		if (AShooterCharacter* C = GetPlayerShooter(World))
		{
			C->TeleportTo(Loc, FRotator(0.0f, Yaw, 0.0f), false, true);
			if (AController* Ctrl = C->GetController())
			{
				Ctrl->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
			}
		}
	};
	auto Fire = [World]() { if (AShooterCharacter* C = GetPlayerShooter(World)) { C->DoStartFiring(); } };
	auto Cease = [World]() { if (AShooterCharacter* C = GetPlayerShooter(World)) { C->DoStopFiring(); } };

	// Arena entrance, facing into the arena; NPC spawners fire between 3 and 6 s.
	Steps.Add({ 1.0f, [Teleport]() { Teleport(FVector(4000.0f, 4300.0f, 420.0f), 90.0f); } });
	Steps.Add({ 7.0f, [this]() { LogCombatState(TEXT("before")); } });

	float T = 7.2f;
	for (int32 ShotIndex = 0; ShotIndex < 14; ++ShotIndex)
	{
		Steps.Add({ T, [this]() { AimAtNearestLivingNPC(); } });
		Steps.Add({ T + 0.05f, Fire });
		if (ShotIndex == 0)
		{
			Steps.Add({ T + 0.08f, [this]() { Shot(TEXT("C1_NPCHit")); } });
		}
		Steps.Add({ T + 0.12f, Cease });
		T += 0.35f;
	}
	Steps.Add({ T, [this]() { LogCombatState(TEXT("after")); Shot(TEXT("C2_NPCAfter")); } });

	// Aim trainer: shoot GRIDSHOT, wait out the countdown, then clear targets.
	T += 0.5f;
	Steps.Add({ T, [Teleport]() { Teleport(FVector(11000.0f, 4500.0f, 100.0f), 0.0f); } });
	Steps.Add({ T + 0.6f, [this]() { AimAtTrainerComponent(TEXT("Button0")); } });
	Steps.Add({ T + 0.7f, Fire });
	Steps.Add({ T + 0.8f, Cease });
	Steps.Add({ T + 1.2f, [this]() { Shot(TEXT("C3_TrainerCountdown")); } });
	T += 4.6f;
	for (int32 ShotIndex = 0; ShotIndex < 12; ++ShotIndex)
	{
		Steps.Add({ T, [this]() { AimAtTrainerComponent(TEXT("Target")); } });
		Steps.Add({ T + 0.05f, Fire });
		if (ShotIndex == 3)
		{
			Steps.Add({ T + 0.09f, [this]() { Shot(TEXT("C4_TrainerPop")); } });
		}
		Steps.Add({ T + 0.12f, Cease });
		T += 0.3f;
	}
	Steps.Add({ T + 0.3f, [this]() { Shot(TEXT("C5_TrainerBoard")); } });
	Steps.Add({ T + 1.0f, [World]()
		{
#if !UE_BUILD_SHIPPING
			if (CVarFeelCaptureQuit.GetValueOnGameThread() != 0)
			{
				UKismetSystemLibrary::QuitGame(World, nullptr, EQuitPreference::Quit, false);
			}
#endif
		} });
}
