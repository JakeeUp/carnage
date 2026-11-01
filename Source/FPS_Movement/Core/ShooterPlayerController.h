// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "ShooterPlayerController.generated.h"

class UInputMappingContext;
class AShooterCharacter;
class UShooterBulletCounterUI;
class UShooterHUDWidget;

UCLASS(abstract, config="Game")
class FPS_MOVEMENT_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AShooterPlayerController();

	/**
	 * Show a damage direction marker on the HUD pointing at a world-space source (attacker or
	 * projectile origin). Safe to call on any controller; no-op without a local HUD.
	 * Called automatically from the possessed pawn's Carnage attribute set (OnDamageDealt) when
	 * bAutoDamageDirectionFromAttributes is set; duplicate reports within a few frames are merged.
	 */
	UFUNCTION(BlueprintCallable, Category="Shooter|UI")
	void NotifyDamageFrom(const FVector& WorldSourceLocation);

	UFUNCTION(BlueprintPure, Category="Shooter|UI")
	UShooterHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	UPROPERTY(EditAnywhere, Category="Shooter|Respawn")
	TSubclassOf<AShooterCharacter> CharacterClass;

	/** Legacy template UMG widget. Kept so existing Blueprint assets still load; only spawned if bUseLegacyBulletCounterUI. */
	UPROPERTY(EditAnywhere, Category="Shooter|UI")
	TSubclassOf<UShooterBulletCounterUI> BulletCounterUIClass;

	/** Spawn the old template bullet counter widget in addition to the native HUD. */
	UPROPERTY(EditAnywhere, Category="Shooter|UI")
	bool bUseLegacyBulletCounterUI = false;

	/** Native HUD class. Defaults to UShooterHUDWidget; set a Blueprint subclass to restyle it. */
	UPROPERTY(EditAnywhere, Category="Shooter|UI")
	TSubclassOf<UShooterHUDWidget> HUDClass;

	/** Feed damage direction markers from the pawn's attribute set damage event (effect causer / its instigator). */
	UPROPERTY(EditAnywhere, Category="Shooter|UI")
	bool bAutoDamageDirectionFromAttributes = true;

	UPROPERTY(Transient)
	TObjectPtr<UShooterHUDWidget> HUDWidget;

	UPROPERTY(EditAnywhere, Category="Shooter|Player")
	FName PlayerPawnTag = FName("Player");

	UPROPERTY()
	TObjectPtr<UShooterBulletCounterUI> BulletCounterUI;

protected:

	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	virtual void OnPossess(APawn* InPawn) override;

	/** Creates the native HUD for the local player if needed, and binds it to the current pawn. */
	void EnsureHUD();

	UFUNCTION()
	void OnPawnDamageDealt(AActor* Source, AActor* Target, float DamageAmount, const FGameplayTagContainer& DamageTags);

	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnBulletCountUpdated(int32 MagazineSize, int32 Bullets);

	UFUNCTION()
	void OnPawnDamaged(float LifePercent);

	UFUNCTION()
	void OnReserveAmmoUpdated(int32 ReserveAmmo);

	UFUNCTION()
	void OnAimStateChanged(bool bIsAimingDownSights);

	bool ShouldUseTouchControls() const;
};
