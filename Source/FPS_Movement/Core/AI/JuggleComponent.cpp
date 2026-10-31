// Copyright Epic Games, Inc. All Rights Reserved.

#include "JuggleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UJuggleComponent::UJuggleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UJuggleComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerChar = Cast<ACharacter>(GetOwner());
	if (OwnerChar)
	{
		Move = OwnerChar->GetCharacterMovement();
	}
}

void UJuggleComponent::ApplyKnockback(const FVector& Direction, float Force, bool bLauncher)
{
	if (!OwnerChar || !Move || Force <= 0.0f)
	{
		return;
	}

	const FVector Dir = Direction.GetSafeNormal();
	FVector Vel = Move->Velocity;

	const FVector Horizontal = FVector(Dir.X, Dir.Y, 0.0f).GetSafeNormal() * Force * HorizontalKnockbackScale;
	Vel.X += Horizontal.X;
	Vel.Y += Horizontal.Y;

	const bool bAirborne = Move->IsFalling();

	if (bLauncher)
	{
		Vel.Z = LaunchUpVelocity;
	}
	else if (bAirborne)
	{
		if (Dir.Z < -0.3f)
		{
			Vel.Z = -Force * DownwardSlamScale;
		}
		else
		{
			Vel.Z = FMath::Max(Vel.Z, 0.0f) + AirHitUpVelocity;
		}
	}
	else
	{
		Vel.Z = FMath::Max(Vel.Z, 0.0f) + FMath::Max(0.0f, Dir.Z * Force);
	}

	Vel = Vel.GetClampedToMaxSize(MaxKnockbackSpeed);
	Move->Velocity = Vel;

	if (Move->MovementMode != MOVE_Falling && Vel.Z > 1.0f)
	{
		Move->SetMovementMode(MOVE_Falling);
	}

	GroundedTimer = 0.0f;
	JuggleElapsed = 0.0f;
	SetJuggled(true);
}

void UJuggleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsJuggled || !Move)
	{
		return;
	}

	JuggleElapsed += DeltaTime;

	const bool bFalling = Move->IsFalling();
	if (bFalling)
	{
		LastDownwardSpeed = FMath::Max(0.0f, -Move->Velocity.Z);
	}

	if (bWasFalling && !bFalling)
	{
		if (LastDownwardSpeed > MinBounceSpeed && JuggleElapsed < MaxJuggleDuration)
		{
			FVector V = Move->Velocity;
			V.Z = LastDownwardSpeed * GroundBounceFactor;
			Move->Velocity = V;
			Move->SetMovementMode(MOVE_Falling);
		}
	}
	bWasFalling = Move->IsFalling();

	const bool bGroundedAndSlow = Move->IsMovingOnGround()
		&& Move->Velocity.SizeSquared2D() < FMath::Square(150.0f);

	GroundedTimer = bGroundedAndSlow ? GroundedTimer + DeltaTime : 0.0f;

	if (GroundedTimer > SettleDelay || JuggleElapsed > MaxJuggleDuration)
	{
		SetJuggled(false);
	}
}

void UJuggleComponent::SetJuggled(bool bNewJuggled)
{
	if (bIsJuggled == bNewJuggled)
	{
		return;
	}

	bIsJuggled = bNewJuggled;
	SetComponentTickEnabled(bIsJuggled);
	OnJuggleStateChanged.Broadcast(bIsJuggled);
}
