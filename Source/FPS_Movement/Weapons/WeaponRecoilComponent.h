// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponRecoilTypes.h"
#include "WeaponRecoilComponent.generated.h"

class USkeletalMeshComponent;

UCLASS(ClassGroup="Weapon", meta=(BlueprintSpawnableComponent))
class FPS_MOVEMENT_API UWeaponRecoilComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UWeaponRecoilComponent();

	// ── Aim recoil tuning ───────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=1, ClampMax=100))
	float KickApplySpeed = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=0, ClampMax=1, Units="s"))
	float RecoveryDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=0, ClampMax=90))
	float RecoverySpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin=0, ClampMax=45))
	float MaxAccumulatedPitch = 10.0f;

	// ── Visual kick tuning ──────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|VisualKick", meta=(ClampMin=10, ClampMax=2000))
	float KickSpringStiffness = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|VisualKick", meta=(ClampMin=1, ClampMax=200))
	float KickSpringDamping = 26.0f;

	// ── Sway tuning ─────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway")
	bool bEnableSway = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=3))
	float LookSwayScale = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=15))
	float MaxLookSway = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=1, ClampMax=30))
	float SwayInterpSpeed = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=10))
	float MoveSwayLateral = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=0.01))
	float MoveSwayVertical = 0.0018f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Sway", meta=(ClampMin=0, ClampMax=10))
	float MaxMoveSwayVertical = 2.0f;

	// ── Wall avoidance (anti-clip) ──────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|WallAvoid")
	bool bEnableWallAvoidance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|WallAvoid", meta=(ClampMin=0, ClampMax=200, Units="cm"))
	float WallAvoidanceTraceDistance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|WallAvoid", meta=(ClampMin=0, ClampMax=100, Units="cm"))
	float MaxWallPullback = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|WallAvoid", meta=(ClampMin=1, ClampMax=40))
	float WallAvoidanceInterpSpeed = 18.0f;

	// ── API ─────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category="Recoil")
	void AddRecoil(const FWeaponRecoilParams& Params);

	void NotifyAimInput(float YawInput, float PitchInput);

	UFUNCTION(BlueprintCallable, Category="Recoil")
	void SetVisualScale(float Scale) { VisualScale = FMath::Clamp(Scale, 0.0f, 1.0f); }

protected:

	virtual void BeginPlay() override;

public:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	void ApplyAimRotation(float PitchDegrees, float YawDegrees);

	float AimInputToDegrees(float PitchInput) const;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY()
	TObjectPtr<class UCameraComponent> Camera;

	FVector BaseMeshLocation = FVector::ZeroVector;
	FRotator BaseMeshRotation = FRotator::ZeroRotator;

	float CurrentWallPullback = 0.0f;

	float PendingPitch = 0.0f;
	float PendingYaw = 0.0f;
	float AccumulatedPitch = 0.0f;
	float TimeSinceLastKick = 1000.0f;

	float KickBackOffset = 0.0f;
	float KickBackVelocity = 0.0f;
	float KickPitchOffset = 0.0f;
	float KickPitchVelocity = 0.0f;

	float PendingLookYaw = 0.0f;
	float PendingLookPitch = 0.0f;
	float SwayYaw = 0.0f;
	float SwayPitch = 0.0f;
	FVector SwayOffset = FVector::ZeroVector;

	float VisualScale = 1.0f;
};
