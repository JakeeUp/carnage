// Copyright Epic Games, Inc. All Rights Reserved.

#include "DoomMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "FPS_Movement.h"

DECLARE_CYCLE_STAT(TEXT("DoomMovement Tick"), STAT_DoomMovementTick, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("DoomMovement WallRun"), STAT_DoomWallRun, STATGROUP_Game);

UDoomMovementComponent::UDoomMovementComponent()
{
	MaxWalkSpeed = 800.0f;
	MaxWalkSpeedCrouched = 400.0f;
	JumpZVelocity = 620.0f;
	AirControl = 0.85f;
	BrakingDecelerationFalling = 200.0f;
	BrakingDecelerationWalking = 1400.0f;
	GroundFriction = 6.0f;
	GravityScale = 1.4f;
	MaxAcceleration = 2800.0f;
	BrakingFrictionFactor = 1.0f;
	bUseSeparateBrakingFriction = false;

	SetCrouchedHalfHeight(40.0f);
	NavAgentProps.bCanCrouch = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void UDoomMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentDashCharges = MaxDashCharges;

	if (const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent())
	{
		SavedCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		SavedCapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
	}
}

void UDoomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_DoomMovementTick);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateDash(DeltaTime);
	UpdateMantle(DeltaTime);
	UpdateSlide(DeltaTime);

	if (IsFalling() && !bIsWallRunning && !bIsMantling && !bIsDashing)
	{
		WallRunCheckTimer -= DeltaTime;
		if (WallRunCheckTimer > 0.0f)
		{
			return;
		}
		WallRunCheckTimer = WallRunCheckInterval;

		if (bWallRunDebug && GEngine)
		{
			FHitResult DbgHit;
			bool bDbgRight = false;
			const bool bWallFound = CheckWallRun(DbgHit, bDbgRight);
			GEngine->AddOnScreenDebugMessage(101, 0.0f, bWallFound ? FColor::Green : FColor::Yellow,
				FString::Printf(TEXT("[WallRun] airborne  spd2D=%.0f (min %.0f)  wallFound=%s"),
					Velocity.Size2D(), WallRunMinSpeed, bWallFound ? TEXT("YES") : TEXT("no")));
		}

		TryStartWallRun();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

float UDoomMovementComponent::GetMaxSpeed() const
{
	if (bIsDashing) return DashSpeed;
	if (bIsMantling) return 0.0f;
	if (bIsWallRunning) return WallRunMaxSpeed * ExternalSpeedScale;

	if (bIsSliding)
	{
		return SlideMaxSpeed * ExternalSpeedScale;
	}

	if (IsMovingOnGround() && bWantsToSprint && !IsCrouching())
	{
		return SprintSpeed * ExternalSpeedScale * ADSSpeedScale;
	}

	// Aiming down sights only slows grounded walking; slides, dashes, wall runs and air
	// strafing keep their full speed so ADS stays usable mid-flow.
	if (IsMovingOnGround())
	{
		return Super::GetMaxSpeed() * ADSSpeedScale;
	}

	return Super::GetMaxSpeed();
}

float UDoomMovementComponent::GetMaxBrakingDeceleration() const
{
	if (bIsSliding) return SlideBraking;
	if (bIsDashing) return 0.0f;

	return Super::GetMaxBrakingDeceleration();
}

float UDoomMovementComponent::GetMaxAcceleration() const
{
	if (bIsSliding) return SlideSteeringAcceleration;
	if (bIsDashing) return 0.0f;

	return Super::GetMaxAcceleration();
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void UDoomMovementComponent::StartSprint()
{
	bWantsToSprint = true;

	if (IsCrouching() && !bIsSliding)
	{
		bWantsToCrouch = false;
	}
}

void UDoomMovementComponent::StopSprint()
{
	bWantsToSprint = false;
}

bool UDoomMovementComponent::IsSprinting() const
{
	return bWantsToSprint && IsMovingOnGround() && !IsCrouching() && !bIsSliding
		&& Velocity.SizeSquared2D() > FMath::Square(MaxWalkSpeed * 0.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

bool UDoomMovementComponent::CanSlide() const
{
	const bool bOffCooldown = !GetWorld() || (GetWorld()->GetTimeSeconds() - LastSlideEndTime) >= SlideCooldown;

	return IsMovingOnGround()
		&& !bIsSliding
		&& !bIsMantling
		&& bOffCooldown
		&& Velocity.Size2D() >= SlideMinSpeed;
}

void UDoomMovementComponent::StartSlide()
{
	bWantsToSlideHeld = true;

	if (!CanSlide())
	{
		if (IsFalling() && GetWorld())
		{
			LastSlidePressTime = GetWorld()->GetTimeSeconds();
		}
		return;
	}

	bIsSliding = true;
	SlideTimer = 0.0f;

	SavedGroundFriction = GroundFriction;
	GroundFriction = SlideGroundFrictionOverride;

	SlideDirection = Velocity.GetSafeNormal2D();
	if (SlideDirection.IsNearlyZero() && CharacterOwner)
	{
		SlideDirection = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	}

	const float EntryMultiplier = SlideEntrySpeedMultiplier * (IsSprinting() ? SprintSlideEntryBoost : 1.0f);
	const float CurrentSpeed2D = Velocity.Size2D();
	const float SlideSpeed = FMath::Min(CurrentSpeed2D * EntryMultiplier, SlideMaxSpeed);
	Velocity = SlideDirection * SlideSpeed + FVector(0, 0, Velocity.Z);

	OnMovementAction.Broadcast(EDoomMovementAction::Slide);
}

void UDoomMovementComponent::StopSlide(bool bForce)
{
	if (!bForce)
	{
		bWantsToSlideHeld = false;
	}

	if (!bIsSliding) return;

	bIsSliding = false;
	GroundFriction = SavedGroundFriction;

	if (GetWorld())
	{
		LastSlideEndTime = GetWorld()->GetTimeSeconds();
	}
}

bool UDoomMovementComponent::CanStandUp() const
{
	return true;
}

void UDoomMovementComponent::UpdateSlide(float DeltaTime)
{
	if (!bIsSliding) return;

	SlideTimer += DeltaTime;

	// ── Slope acceleration ─────────────────────────────────────────────
	if (CurrentFloor.IsWalkableFloor())
	{
		const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;
		const FVector Down(0.0f, 0.0f, -1.0f);

		FVector DownhillDir = Down - FloorNormal * FVector::DotProduct(Down, FloorNormal);

		if (!DownhillDir.IsNearlyZero())
		{
			DownhillDir.Normalize();

			const float Steepness = FMath::Clamp(1.0f - FloorNormal.Z, 0.0f, 1.0f);

			const FVector SlopeForce = DownhillDir * SlideSlopeAcceleration * Steepness;
			Velocity += SlopeForce * DeltaTime;

			const float Speed2D = Velocity.Size2D();
			if (Speed2D > SlideMaxSpeed)
			{
				const FVector Capped = Velocity.GetSafeNormal2D() * SlideMaxSpeed;
				Velocity.X = Capped.X;
				Velocity.Y = Capped.Y;
			}
		}
	}

	// ── Friction deceleration ───────────────────────────────────────────
	{
		const float CurrentSpeed2D = Velocity.Size2D();
		if (CurrentSpeed2D > 0.0f)
		{
			const float NewSpeed = FMath::Max(0.0f, CurrentSpeed2D - SlideDeceleration * DeltaTime);
			const FVector Dir2D = Velocity.GetSafeNormal2D();
			Velocity.X = Dir2D.X * NewSpeed;
			Velocity.Y = Dir2D.Y * NewSpeed;
		}
	}

	// ── Exit conditions ────────────────────────────────────────────────
	const float Speed2D = Velocity.Size2D();
	if (Speed2D < SlideExitSpeed || SlideTimer > SlideMaxDuration || !IsMovingOnGround())
	{
		StopSlide();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

bool UDoomMovementComponent::CanDash() const
{
	return CurrentDashCharges > 0 && !bIsDashing && !bIsMantling;
}

void UDoomMovementComponent::PerformDash(const FVector& WorldDirection)
{
	if (!CanDash()) return;

	bIsDashing = true;
	DashActiveTimer = DashDuration;

	--CurrentDashCharges;
	OnDashChargesChanged.Broadcast(CurrentDashCharges);

	if (DashCooldownTimer <= 0.0f)
	{
		DashCooldownTimer = DashCooldown;
	}

	FVector Dir = WorldDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = CharacterOwner ? CharacterOwner->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	}

	DashVelocity = Dir * DashSpeed;

	if (IsMovingOnGround())
	{
		DashVelocity.Z = 100.0f;
		SetMovementMode(MOVE_Falling);
	}
	else
	{
		DashVelocity.Z = FMath::Max(Velocity.Z, 0.0f);
	}

	Velocity = DashVelocity;

	if (bIsSliding)
	{
		StopSlide(true);
	}

	OnMovementAction.Broadcast(EDoomMovementAction::Dash);
}

void UDoomMovementComponent::UpdateDash(float DeltaTime)
{
	if (bIsDashing)
	{
		DashActiveTimer -= DeltaTime;
		if (DashActiveTimer <= 0.0f)
		{
			bIsDashing = false;
			Velocity = Velocity.GetSafeNormal() * FMath::Min(Velocity.Size(), SprintSpeed);
		}
		else
		{
			Velocity.X = DashVelocity.X;
			Velocity.Y = DashVelocity.Y;
		}
	}

	if (CurrentDashCharges < MaxDashCharges)
	{
		DashCooldownTimer -= DeltaTime;
		if (DashCooldownTimer <= 0.0f)
		{
			++CurrentDashCharges;
			OnDashChargesChanged.Broadcast(CurrentDashCharges);
			DashCooldownTimer = (CurrentDashCharges < MaxDashCharges) ? DashCooldown : 0.0f;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void UDoomMovementComponent::SetWantsToHoldJump(bool bHold)
{
	bHoldingJump = bHold;
}

void UDoomMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bBufferedJump = GetWorld() && (Now - LastJumpPressTime) <= JumpBufferTime;

	const bool bWantsSlide = bWantsToSlideHeld || (GetWorld() && (Now - LastSlidePressTime) <= SlideBufferTime);

	if (bWantsSlide && !bIsMantling && Velocity.Size2D() >= SlideMinSpeed)
	{
		LastSlidePressTime = -1000.0f;
		ConsecutiveHops = 0;
		Super::ProcessLanded(Hit, remainingTime, Iterations);
		StartSlide();
		return;
	}

	if ((bHoldingJump || bBufferedJump) && !bIsMantling)
	{
		LastJumpPressTime = -1000.0f;

		const FVector HorizontalVel = FVector(Velocity.X, Velocity.Y, 0.0f);
		const float CurrentSpeed = HorizontalVel.Size();
		const float BoostedSpeed = FMath::Min(CurrentSpeed + BunnyHopSpeedBoost, BunnyHopMaxSpeed);

		Super::ProcessLanded(Hit, remainingTime, Iterations);

		if (!HorizontalVel.IsNearlyZero())
		{
			Velocity = HorizontalVel.GetSafeNormal() * BoostedSpeed;
		}

		++ConsecutiveHops;

		OnMovementAction.Broadcast(EDoomMovementAction::BunnyHop);

		CharacterOwner->Jump();
	}
	else
	{
		ConsecutiveHops = 0;
		Super::ProcessLanded(Hit, remainingTime, Iterations);
	}
}

bool UDoomMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	LastJumpPressTime = -1000.0f;

	if (bIsSliding)
	{
		const FVector PreSlideVel = Velocity;
		StopSlide(true);
		Velocity = PreSlideVel;
		Velocity.Z = 0.0f;

		if (Super::DoJump(bReplayingMoves, DeltaTime))
		{
			Velocity.Z += SlideJumpBoost;
			OnMovementAction.Broadcast(EDoomMovementAction::Jump);
			return true;
		}
		return false;
	}

	if (bIsWallRunning)
	{
		WallJump();
		return true;
	}

	if (IsFalling() && !bIsWallRunning && !bIsMantling)
	{
		if (TryMantle()) return true;
	}

	if (IsFalling() && CharacterOwner && CharacterOwner->JumpCurrentCount >= 1 && !IsWithinCoyoteTime())
	{
		const FVector InputDir = Acceleration.GetSafeNormal2D();
		if (!InputDir.IsNearlyZero() && AirJumpRedirectStrength > 0.0f)
		{
			const float Speed2D = Velocity.Size2D();
			FVector NewDir = FMath::Lerp(Velocity.GetSafeNormal2D(), InputDir, AirJumpRedirectStrength);
			if (NewDir.Normalize())
			{
				Velocity.X = NewDir.X * Speed2D;
				Velocity.Y = NewDir.Y * Speed2D;
			}
		}
	}

	const bool bJumped = Super::DoJump(bReplayingMoves, DeltaTime);
	if (bJumped)
	{
		OnMovementAction.Broadcast(EDoomMovementAction::Jump);
	}
	return bJumped;
}

bool UDoomMovementComponent::CanAttemptJump() const
{
	return Super::CanAttemptJump() || bIsWallRunning;
}

void UDoomMovementComponent::NotifyJumpInputPressed()
{
	if (const UWorld* World = GetWorld())
	{
		LastJumpPressTime = World->GetTimeSeconds();
	}
}

bool UDoomMovementComponent::IsWithinCoyoteTime() const
{
	return IsFalling()
		&& GetWorld()
		&& (GetWorld()->GetTimeSeconds() - CoyoteAnchorTime) <= CoyoteTime;
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

bool UDoomMovementComponent::CheckWallRun(FHitResult& OutHit, bool& bOutIsRight) const
{
	if (!CharacterOwner) return false;
	if (!IsFalling()) return false;
	if (Velocity.Size2D() < WallRunMinSpeed) return false;

	const FVector Start = CharacterOwner->GetActorLocation();

	FHitResult FloorHit;
	FCollisionQueryParams FloorParams;
	FloorParams.AddIgnoredActor(CharacterOwner);
	if (GetWorld()->LineTraceSingleByChannel(FloorHit, Start, Start - FVector(0, 0, WallRunMinHeight + SavedCapsuleHalfHeight), ECC_Visibility, FloorParams))
	{
		return false;
	}

	FCollisionQueryParams WallParams;
	WallParams.AddIgnoredActor(CharacterOwner);

	const float Now = GetWorld()->GetTimeSeconds();
	const FVector AccelDir = Acceleration.GetSafeNormal2D();
	const FVector Forward = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = CharacterOwner->GetActorRightVector().GetSafeNormal2D();
	const float TraceLen = SavedCapsuleRadius + WallRunTraceDistance;

	auto IsWallValid = [&](const FHitResult& Hit) -> bool
	{
		if (!Hit.bBlockingHit) return false;

		if (FMath::Abs(Hit.ImpactNormal.Z) >= 0.35f) return false;

		if ((Now - LastWallRunEndTime) < WallRunReattachCooldown
			&& FVector::DotProduct(Hit.ImpactNormal, LastWallRunNormal) > 0.85f)
		{
			return false;
		}

		if (bWallRunRequiresInput && !AccelDir.IsNearlyZero()
			&& FVector::DotProduct(AccelDir, Hit.ImpactNormal) > 0.5f)
		{
			return false;
		}

		return true;
	};

	const FVector Dirs[] = {
		Right,
		-Right,
		(Forward + Right).GetSafeNormal(),
		(Forward - Right).GetSafeNormal()
	};

	for (const FVector& Dir : Dirs)
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * TraceLen, ECC_WorldStatic, WallParams)
			&& IsWallValid(Hit))
		{
			OutHit = Hit;
			bOutIsRight = FVector::DotProduct(Hit.ImpactNormal, Right) < 0.0f;
			return true;
		}
	}

	return false;
}

void UDoomMovementComponent::TryStartWallRun()
{
	FHitResult WallHit;
	bool bIsRight = false;
	if (!CheckWallRun(WallHit, bIsRight))
	{
		return;
	}

	bIsWallRunning = true;
	bWallRunIsRight = bIsRight;
	WallRunNormal = WallHit.ImpactNormal;
	WallRunTimer = 0.0f;

	FVector Tangent = FVector::CrossProduct(WallRunNormal, FVector::UpVector).GetSafeNormal();
	const FVector LookFlat = FRotator(0.0f, CharacterOwner->GetControlRotation().Yaw, 0.0f).Vector();
	if (FVector::DotProduct(Tangent, LookFlat) < 0.0f)
	{
		Tangent = -Tangent;
	}
	WallRunDirection = Tangent;

	WallRunCurrentSpeed = FMath::Clamp(Velocity.Size2D(), WallRunSpeed, WallRunMaxSpeed);

	Velocity.Z = FMath::Max(Velocity.Z, WallRunInitialUpVelocity);

	SetMovementMode(MOVE_Custom, CMOVE_WallRun);

	OnMovementAction.Broadcast(EDoomMovementAction::WallRunStart);

	if (bWallRunDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(102, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("[WallRun] START  right=%s  speed=%.0f"),
				bWallRunIsRight ? TEXT("R") : TEXT("L"), WallRunCurrentSpeed));
	}
}

void UDoomMovementComponent::PhysWallRun(float DeltaTime, int32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_DoomWallRun);

	if (!CharacterOwner || !UpdatedComponent) return;

	if (IsMovingOnGround())
	{
		EndWallRun();
		return;
	}

	WallRunTimer += DeltaTime;

	FHitResult WallHit;
	const FVector Start = CharacterOwner->GetActorLocation();
	const FVector TraceDir = -WallRunNormal;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	const bool bWallStillThere = GetWorld()->LineTraceSingleByChannel(
		WallHit, Start, Start + TraceDir * (WallRunTraceDistance + SavedCapsuleRadius), ECC_WorldStatic, Params);

	if (!bWallStillThere || WallRunTimer > WallRunMaxDuration)
	{
		EndWallRun();
		return;
	}

	const FVector AccelDir = Acceleration.GetSafeNormal2D();
	if (bWallRunRequiresInput && !AccelDir.IsNearlyZero()
		&& FVector::DotProduct(AccelDir, WallHit.ImpactNormal) > 0.5f)
	{
		EndWallRun();
		return;
	}

	WallRunNormal = WallHit.ImpactNormal;

	FVector Tangent = FVector::CrossProduct(WallRunNormal, FVector::UpVector).GetSafeNormal();
	if (FVector::DotProduct(Tangent, WallRunDirection) < 0.0f)
	{
		Tangent = -Tangent;
	}
	WallRunDirection = Tangent;

	const float PreviousZVelocity = Velocity.Z;
	WallRunCurrentSpeed = FMath::Min(WallRunCurrentSpeed + WallRunAcceleration * DeltaTime, WallRunMaxSpeed);
	Velocity = WallRunDirection * WallRunCurrentSpeed;
	Velocity.Z = FMath::FInterpTo(PreviousZVelocity, GetGravityZ() * WallRunGravityScale, DeltaTime, 2.5f);

	const FVector WallAttract = -WallRunNormal * 200.0f * DeltaTime;

	const FVector PreLoc = UpdatedComponent->GetComponentLocation();
	FHitResult MoveHit;
	const FVector Delta = (Velocity * DeltaTime) + WallAttract;
	SafeMoveUpdatedComponent(Delta, CharacterOwner->GetActorRotation(), true, MoveHit);

	if (MoveHit.bBlockingHit)
	{
		if (FVector::DotProduct(MoveHit.ImpactNormal, WallRunNormal) >= 0.5f)
		{
			SlideAlongSurface(Delta, 1.0f - MoveHit.Time, MoveHit.Normal, MoveHit, true);
		}
		else
		{
			EndWallRun();
			return;
		}
	}

	if (bWallRunDebug && GEngine)
	{
		const float Moved = (UpdatedComponent->GetComponentLocation() - PreLoc).Size();
		GEngine->AddOnScreenDebugMessage(103, 0.0f, FColor::White,
			FString::Printf(TEXT("[WallRun] moved=%.1fcm  vel2D=%.0f"), Moved, Velocity.Size2D()));
	}
}

void UDoomMovementComponent::EndWallRun()
{
	if (!bIsWallRunning) return;

	bIsWallRunning = false;
	WallRunTimer = 0.0f;

	LastWallRunNormal = WallRunNormal;
	if (const UWorld* World = GetWorld())
	{
		LastWallRunEndTime = World->GetTimeSeconds();
	}

	if (bWallRunDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(102, 2.0f, FColor::Red, TEXT("[WallRun] END"));
	}

	SetMovementMode(MOVE_Falling);
}

void UDoomMovementComponent::WallJump()
{
	if (!bIsWallRunning) return;

	const FVector JumpVelocity = WallRunNormal * WallJumpOffForce + FVector(0, 0, WallJumpUpForce);

	EndWallRun();

	Velocity = FVector(Velocity.X, Velocity.Y, 0.0f) + JumpVelocity;

	OnMovementAction.Broadcast(EDoomMovementAction::WallJump);
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

bool UDoomMovementComponent::TryMantle()
{
	if (!CharacterOwner || bIsMantling) return false;

	const FVector CapsuleLocation = CharacterOwner->GetActorLocation();
	const FVector Forward = CharacterOwner->GetActorForwardVector();
	const float CapsuleHH = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float CapsuleR = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	const FVector WallTraceStart = CapsuleLocation + FVector(0, 0, CapsuleHH * 0.3f);
	const FVector WallTraceEnd = WallTraceStart + Forward * MantleForwardDistance;

	FHitResult WallHit;
	if (!GetWorld()->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_WorldStatic, Params))
	{
		return false;
	}

	const FVector LedgeTraceStart = WallHit.ImpactPoint + Forward * (CapsuleR + 5.0f) + FVector(0, 0, MantleReachHeight);
	const FVector LedgeTraceEnd = LedgeTraceStart - FVector(0, 0, MantleReachHeight * 2.0f);

	FHitResult LedgeHit;
	if (!GetWorld()->LineTraceSingleByChannel(LedgeHit, LedgeTraceStart, LedgeTraceEnd, ECC_WorldStatic, Params))
	{
		return false;
	}

	const float FeetZ = CapsuleLocation.Z - CapsuleHH;
	const float LedgeZ = LedgeHit.ImpactPoint.Z;
	const float HeadZ = CapsuleLocation.Z + CapsuleHH;

	if (LedgeZ < FeetZ || LedgeZ > HeadZ + MantleReachHeight)
	{
		return false;
	}

	const FVector StandLocation = FVector(LedgeHit.ImpactPoint.X, LedgeHit.ImpactPoint.Y, LedgeZ + CapsuleHH + 2.0f);
	FCollisionShape StandShape = FCollisionShape::MakeCapsule(CapsuleR, CapsuleHH);

	if (GetWorld()->OverlapBlockingTestByChannel(StandLocation, FQuat::Identity, ECC_Pawn, StandShape, Params))
	{
		return false;
	}

	bIsMantling = true;
	MantleAlpha = 0.0f;
	MantleStartLocation = CapsuleLocation;
	MantleTargetLocation = StandLocation;

	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Flying);

	OnMovementAction.Broadcast(EDoomMovementAction::Mantle);

	return true;
}

void UDoomMovementComponent::UpdateMantle(float DeltaTime)
{
	if (!bIsMantling) return;

	MantleAlpha += DeltaTime / MantleDuration;

	if (MantleAlpha >= 1.0f)
	{
		bIsMantling = false;
		CharacterOwner->SetActorLocation(MantleTargetLocation);
		Velocity = CharacterOwner->GetActorForwardVector() * MaxWalkSpeed * 0.5f;
		SetMovementMode(MOVE_Walking);
		return;
	}

	const float SmoothedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, MantleAlpha, 2.0f);
	const FVector NewLocation = FMath::Lerp(MantleStartLocation, MantleTargetLocation, SmoothedAlpha);

	FHitResult MoveHit;
	SafeMoveUpdatedComponent(NewLocation - CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation(), false, MoveHit);
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void UDoomMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	switch (CustomMovementMode)
	{
	case CMOVE_WallRun:
		PhysWallRun(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogFPS_Movement, Warning, TEXT("Unknown custom movement mode: %d"), CustomMovementMode);
		SetMovementMode(MOVE_Falling);
		break;
	}
}

void UDoomMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (PreviousMovementMode == MOVE_Walking && MovementMode == MOVE_Falling)
	{
		if (const UWorld* World = GetWorld())
		{
			CoyoteAnchorTime = World->GetTimeSeconds();
		}
	}

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_WallRun)
	{
		bIsWallRunning = false;
	}

	if (MovementMode == MOVE_Walking)
	{
		bIsWallRunning = false;
		WallRunTimer = 0.0f;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

float UDoomMovementComponent::GetSpeedRatio() const
{
	const float MaxPossible = BunnyHopMaxSpeed;
	return MaxPossible > 0.0f ? FMath::Clamp(GetHorizontalSpeed() / MaxPossible, 0.0f, 1.0f) : 0.0f;
}

float UDoomMovementComponent::GetHorizontalSpeed() const
{
	return Velocity.Size2D();
}

void UDoomMovementComponent::SetExternalSpeedScale(float NewScale)
{
	ExternalSpeedScale = FMath::Clamp(NewScale, 1.0f, 2.0f);
}

void UDoomMovementComponent::SetADSSpeedScale(float NewScale)
{
	ADSSpeedScale = FMath::Clamp(NewScale, 0.1f, 1.0f);
}

EDoomMovementState UDoomMovementComponent::GetMovementState() const
{
	if (bIsMantling)    return EDoomMovementState::Mantling;
	if (bIsDashing)     return EDoomMovementState::Dashing;
	if (bIsWallRunning) return EDoomMovementState::WallRunning;
	if (bIsSliding)     return EDoomMovementState::Sliding;
	if (IsFalling())    return EDoomMovementState::Falling;
	if (IsSprinting())  return EDoomMovementState::Sprinting;
	if (IsMovingOnGround() && Velocity.SizeSquared2D() > FMath::Square(10.0f))
	{
		return EDoomMovementState::Walking;
	}
	return EDoomMovementState::Idle;
}
