#include "CarnageFeelCapture.h"
#include "ShooterCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DoomMovementComponent.h"
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
		if (CVarMoveTest.GetValueOnGameThread() != 0)
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
