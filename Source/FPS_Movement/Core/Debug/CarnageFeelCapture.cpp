#include "CarnageFeelCapture.h"
#include "ShooterCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "Kismet/KismetSystemLibrary.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarFeelCapture(
	TEXT("carnage.Debug.FeelCapture"), 0,
	TEXT("1 = run the scripted weapon-feel screenshot sequence once the player pawn exists."));

static TAutoConsoleVariable<FString> CVarFeelCaptureTour(
	TEXT("carnage.Debug.FeelCaptureTour"), TEXT(""),
	TEXT("Optional viewpoints visited after the feel sequence: \"x y z yaw pitch|x y z yaw pitch|...\". Each gets a screenshot."));

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
		BuildSequence();
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
