// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponRecoilComponent.h"
#include "FPS_MovementCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

UWeaponRecoilComponent::UWeaponRecoilComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UWeaponRecoilComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AFPS_MovementCharacter* Character = Cast<AFPS_MovementCharacter>(GetOwner()))
	{
		FirstPersonMesh = Character->GetFirstPersonMesh();
		Camera = Character->GetFirstPersonCameraComponent();
		if (FirstPersonMesh)
		{
			BaseMeshLocation = FirstPersonMesh->GetRelativeLocation();
			BaseMeshRotation = FirstPersonMesh->GetRelativeRotation();
		}
	}
}

void UWeaponRecoilComponent::AddRecoil(const FWeaponRecoilParams& Params)
{
	PendingPitch += Params.PitchKick;
	PendingYaw += FMath::FRandRange(-Params.YawKickRandom, Params.YawKickRandom);

	const float Omega = FMath::Sqrt(KickSpringStiffness);
	KickBackVelocity += Params.VisualKickBack * Omega * VisualScale;
	KickPitchVelocity += Params.VisualKickPitch * Omega * VisualScale;

	TimeSinceLastKick = 0.0f;
}

void UWeaponRecoilComponent::NotifyAimInput(float YawInput, float PitchInput)
{
	PendingLookYaw += YawInput;
	PendingLookPitch += PitchInput;

	const float PitchDegrees = AimInputToDegrees(PitchInput);
	if (PitchDegrees < 0.0f && AccumulatedPitch > 0.0f)
	{
		AccumulatedPitch = FMath::Max(0.0f, AccumulatedPitch + PitchDegrees);
	}
}

float UWeaponRecoilComponent::AimInputToDegrees(float PitchInput) const
{
	if (GetDefault<UInputSettings>()->bEnableLegacyInputScales)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		return PitchInput * (PC ? PC->InputPitchScale_DEPRECATED : -2.5f);
	}
	return PitchInput;
}

void UWeaponRecoilComponent::ApplyAimRotation(float PitchDegrees, float YawDegrees)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	FRotator ControlRot = Controller->GetControlRotation();
	ControlRot.Pitch = FMath::ClampAngle(ControlRot.Pitch + PitchDegrees, -89.0f, 89.0f);
	ControlRot.Yaw += YawDegrees;
	Controller->SetControlRotation(ControlRot);
}

void UWeaponRecoilComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	TimeSinceLastKick += DeltaTime;

	// ── 1. Drain pending kick into the aim ──────────────────────────────
	const float DrainAlpha = FMath::Min(1.0f, DeltaTime * KickApplySpeed);
	const float PitchStep = PendingPitch * DrainAlpha;
	const float YawStep = PendingYaw * DrainAlpha;

	if (!FMath::IsNearlyZero(PitchStep, 0.0001f) || !FMath::IsNearlyZero(YawStep, 0.0001f))
	{
		ApplyAimRotation(PitchStep, YawStep);
		PendingPitch -= PitchStep;
		PendingYaw -= YawStep;
		AccumulatedPitch = FMath::Min(AccumulatedPitch + PitchStep, MaxAccumulatedPitch);
	}

	// ── 2. Recovery — return the un-compensated portion after the burst ──
	if (TimeSinceLastKick >= RecoveryDelay && AccumulatedPitch > 0.0f)
	{
		const float Recover = FMath::Min(AccumulatedPitch, RecoverySpeed * DeltaTime);
		ApplyAimRotation(-Recover, 0.0f);
		AccumulatedPitch -= Recover;
	}

	// ── 3. Visual kick springs ──────────────────────────────────────────
	{
		constexpr float MaxSpringStep = 1.0f / 120.0f;
		float Remaining = FMath::Min(DeltaTime, 0.1f);
		while (Remaining > 0.0f)
		{
			const float Step = FMath::Min(Remaining, MaxSpringStep);
			Remaining -= Step;

			auto StepSpring = [this, Step](float& Offset, float& Vel)
			{
				const float Accel = (-KickSpringStiffness * Offset) - (KickSpringDamping * Vel);
				Vel += Accel * Step;
				Offset += Vel * Step;
			};
			StepSpring(KickBackOffset, KickBackVelocity);
			StepSpring(KickPitchOffset, KickPitchVelocity);
		}
	}

	// ── 4. Sway ─────────────────────────────────────────────────────────
	float TargetSwayYaw = 0.0f;
	float TargetSwayPitch = 0.0f;
	FVector TargetSwayOffset = FVector::ZeroVector;

	const AFPS_MovementCharacter* Character = Cast<AFPS_MovementCharacter>(GetOwner());

	if (bEnableSway && Character)
	{
		TargetSwayYaw = FMath::Clamp(-PendingLookYaw * 2.5f * LookSwayScale, -MaxLookSway, MaxLookSway);
		TargetSwayPitch = FMath::Clamp(AimInputToDegrees(PendingLookPitch) * -LookSwayScale, -MaxLookSway, MaxLookSway);

		const FVector Velocity = Character->GetVelocity();
		const float LateralSpeed = FVector::DotProduct(Velocity, Character->GetActorRightVector());
		const float MaxSpeedRef = 800.0f;

		TargetSwayOffset.Y = FMath::Clamp(-LateralSpeed / MaxSpeedRef, -1.0f, 1.0f) * MoveSwayLateral;
		TargetSwayOffset.Z = FMath::Clamp(-Velocity.Z * MoveSwayVertical, -MaxMoveSwayVertical, MaxMoveSwayVertical);

		TargetSwayYaw *= VisualScale;
		TargetSwayPitch *= VisualScale;
		TargetSwayOffset *= VisualScale;
	}

	SwayYaw = FMath::FInterpTo(SwayYaw, TargetSwayYaw, DeltaTime, SwayInterpSpeed);
	SwayPitch = FMath::FInterpTo(SwayPitch, TargetSwayPitch, DeltaTime, SwayInterpSpeed);
	SwayOffset = FMath::VInterpTo(SwayOffset, TargetSwayOffset, DeltaTime, SwayInterpSpeed);

	PendingLookYaw = 0.0f;
	PendingLookPitch = 0.0f;

	// ── 5. Wall avoidance — retract the weapon when it'd poke through geometry ──
	float TargetPullback = 0.0f;
	if (bEnableWallAvoidance && Camera && GetWorld())
	{
		const FVector Start = Camera->GetComponentLocation();
		const FVector End = Start + Camera->GetForwardVector() * WallAvoidanceTraceDistance;

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(GetOwner());

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			const float Closeness = 1.0f - FMath::Clamp(Hit.Distance / WallAvoidanceTraceDistance, 0.0f, 1.0f);
			TargetPullback = Closeness * MaxWallPullback;
		}
	}
	CurrentWallPullback = FMath::FInterpTo(CurrentWallPullback, TargetPullback, DeltaTime, WallAvoidanceInterpSpeed);

	// ── 6. Compose onto the first-person mesh ───────────────────────────
	if (FirstPersonMesh)
	{
		FVector NewLocation = BaseMeshLocation + SwayOffset;
		NewLocation.X -= KickBackOffset + CurrentWallPullback;

		FRotator NewRotation = BaseMeshRotation;
		NewRotation.Pitch += KickPitchOffset + SwayPitch;
		NewRotation.Yaw += SwayYaw;
		NewRotation.Roll += SwayYaw * 0.35f;

		FirstPersonMesh->SetRelativeLocationAndRotation(NewLocation, NewRotation);
	}
}
