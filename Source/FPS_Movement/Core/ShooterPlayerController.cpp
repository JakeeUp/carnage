// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ShooterPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterCharacter.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterBulletCounterUI.h"
#include "ShooterHUDWidget.h"
#include "FPS_Movement.h"
#include "Widgets/Input/SVirtualJoystick.h"

AShooterPlayerController::AShooterPlayerController()
{
	HUDClass = UShooterHUDWidget::StaticClass();
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				MobileControlsWidget->AddToPlayerScreen(0);

			} else {

				UE_LOG(LogFPS_Movement, Error, TEXT("Could not spawn mobile controls widget."));

			}
		}

		// The native HUD replaces the template bullet counter (which also hosted the crosshair).
		if (bUseLegacyBulletCounterUI && BulletCounterUIClass)
		{
			BulletCounterUI = CreateWidget<UShooterBulletCounterUI>(this, BulletCounterUIClass);

			if (BulletCounterUI)
			{
				BulletCounterUI->AddToPlayerScreen(0);

			} else {

				UE_LOG(LogFPS_Movement, Error, TEXT("Could not spawn bullet counter widget."));

			}
		}

		EnsureHUD();
	}
}

void AShooterPlayerController::EnsureHUD()
{
	if (!IsLocalPlayerController() || !GetLocalPlayer())
	{
		return;
	}

	if (!HUDWidget)
	{
		TSubclassOf<UShooterHUDWidget> ClassToSpawn = HUDClass ? HUDClass : TSubclassOf<UShooterHUDWidget>(UShooterHUDWidget::StaticClass());
		HUDWidget = CreateWidget<UShooterHUDWidget>(this, ClassToSpawn);

		if (HUDWidget)
		{
			// AddToViewport (not player screen) so the HUD gets the viewport DPI scale and full-screen geometry.
			HUDWidget->AddToViewport(1);

		} else {

			UE_LOG(LogFPS_Movement, Error, TEXT("Could not spawn native HUD widget."));

		}
	}

	if (HUDWidget)
	{
		HUDWidget->BindToCharacter(Cast<AShooterCharacter>(GetPawn()));
	}
}

void AShooterPlayerController::NotifyDamageFrom(const FVector& WorldSourceLocation)
{
	if (HUDWidget)
	{
		HUDWidget->NotifyDamageFrom(WorldSourceLocation);
	}
}

void AShooterPlayerController::OnPawnDamageSource(FVector SourceLocation)
{
	NotifyDamageFrom(SourceLocation);
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	InPawn->OnDestroyed.AddDynamic(this, &AShooterPlayerController::OnPawnDestroyed);

	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn))
	{
		ShooterCharacter->Tags.Add(PlayerPawnTag);

		ShooterCharacter->OnBulletCountUpdated.AddDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
		ShooterCharacter->OnDamaged.AddDynamic(this, &AShooterPlayerController::OnPawnDamaged);
		ShooterCharacter->OnReserveAmmoUpdated.AddDynamic(this, &AShooterPlayerController::OnReserveAmmoUpdated);
		ShooterCharacter->OnAimStateChanged.AddDynamic(this, &AShooterPlayerController::OnAimStateChanged);

		// The character reports where incoming damage came from; the HUD turns it into a direction marker.
		ShooterCharacter->OnDamageSource.AddUniqueDynamic(this, &AShooterPlayerController::OnPawnDamageSource);

		// Re-bind the native HUD to the new (or respawned) pawn.
		EnsureHUD();

		ShooterCharacter->OnDamaged.Broadcast(1.0f);
	}
}

void AShooterPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_UpdateBulletCounter(0, 0);
	}

	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		AActor* RandomPlayerStart = ActorList[FMath::RandRange(0, ActorList.Num() - 1)];

		const FTransform SpawnTransform = RandomPlayerStart->GetActorTransform();

		// Respawn as the game mode's pawn so it always matches the character the level started with
		// (CharacterClass previously pointed at the template character, which has no weapon).
		TSubclassOf<AShooterCharacter> RespawnClass = CharacterClass;
		if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
		{
			UClass* DefaultPawn = GameMode->GetDefaultPawnClassForController(this);
			if (DefaultPawn && DefaultPawn->IsChildOf(AShooterCharacter::StaticClass()))
			{
				RespawnClass = DefaultPawn;
			}
		}

		if (AShooterCharacter* RespawnedCharacter = GetWorld()->SpawnActor<AShooterCharacter>(RespawnClass, SpawnTransform))
		{
			Possess(RespawnedCharacter);
		}
	}
}

void AShooterPlayerController::OnBulletCountUpdated(int32 MagazineSize, int32 Bullets)
{
	if (BulletCounterUI)
	{
		BulletCounterUI->BP_UpdateBulletCounter(MagazineSize, Bullets);
	}
}

void AShooterPlayerController::OnPawnDamaged(float LifePercent)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_Damaged(LifePercent);
	}
}

void AShooterPlayerController::OnReserveAmmoUpdated(int32 ReserveAmmo)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_UpdateReserveAmmo(ReserveAmmo);
	}
}

void AShooterPlayerController::OnAimStateChanged(bool bIsAimingDownSights)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_SetAimingDownSights(bIsAimingDownSights);
	}
}

bool AShooterPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
