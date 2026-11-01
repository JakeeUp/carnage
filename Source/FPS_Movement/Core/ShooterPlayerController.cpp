// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ShooterPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterCharacter.h"
#include "ShooterBulletCounterUI.h"
#include "ShooterHUDWidget.h"
#include "CarnageAttributeSet.h"
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

void AShooterPlayerController::OnPawnDamageDealt(AActor* Source, AActor* Target, float DamageAmount, const FGameplayTagContainer& DamageTags)
{
	if (!bAutoDamageDirectionFromAttributes || !Source || DamageAmount <= 0.0f || Source == GetPawn())
	{
		return;
	}

	// Prefer the shooter over the projectile/weapon that actually applied the damage.
	const APawn* InstigatorPawn = Source->GetInstigator();
	const AActor* Origin = (InstigatorPawn && InstigatorPawn != GetPawn()) ? static_cast<const AActor*>(InstigatorPawn) : Source;
	NotifyDamageFrom(Origin->GetActorLocation());
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

		// Incoming damage on our own attribute set drives the HUD damage direction markers.
		if (UCarnageAttributeSet* Attributes = const_cast<UCarnageAttributeSet*>(ShooterCharacter->GetAttributeSet()))
		{
			Attributes->OnDamageDealt.AddUniqueDynamic(this, &AShooterPlayerController::OnPawnDamageDealt);
		}

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

		if (AShooterCharacter* RespawnedCharacter = GetWorld()->SpawnActor<AShooterCharacter>(CharacterClass, SpawnTransform))
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
