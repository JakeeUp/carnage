// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JuggleComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJuggleStateChanged, bool, bIsJuggled);

UCLASS(ClassGroup = "AI", meta = (BlueprintSpawnableComponent))
class FPS_MOVEMENT_API UJuggleComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UJuggleComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0, Units = "cm/s"))
	float LaunchUpVelocity = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0, Units = "cm/s"))
	float AirHitUpVelocity = 430.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0))
	float HorizontalKnockbackScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0))
	float DownwardSlamScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0, ClampMax = 1))
	float GroundBounceFactor = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0, Units = "cm/s"))
	float MinBounceSpeed = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0, Units = "cm/s"))
	float MaxKnockbackSpeed = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0, ClampMax = 3, Units = "s"))
	float SettleDelay = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juggle", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float MaxJuggleDuration = 6.0f;

	UPROPERTY(BlueprintAssignable, Category = "Juggle")
	FJuggleStateChanged OnJuggleStateChanged;

	UFUNCTION(BlueprintCallable, Category = "Juggle")
	void ApplyKnockback(const FVector& Direction, float Force, bool bLauncher);

	UFUNCTION(BlueprintPure, Category = "Juggle")
	bool IsJuggled() const { return bIsJuggled; }

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	void SetJuggled(bool bNewJuggled);

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerChar;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> Move;

	bool bIsJuggled = false;
	bool bWasFalling = false;
	float LastDownwardSpeed = 0.0f;
	float GroundedTimer = 0.0f;
	float JuggleElapsed = 0.0f;
};
