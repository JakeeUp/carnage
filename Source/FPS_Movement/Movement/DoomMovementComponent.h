// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DoomMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDoomDashChargesChanged, int32, CurrentCharges);

UENUM(BlueprintType)
enum class EDoomMovementAction : uint8
{
	Jump         UMETA(DisplayName = "Jump"),
	BunnyHop     UMETA(DisplayName = "Bunny Hop"),
	Slide        UMETA(DisplayName = "Slide"),
	Dash         UMETA(DisplayName = "Dash"),
	WallRunStart UMETA(DisplayName = "Wall Run"),
	WallJump     UMETA(DisplayName = "Wall Jump"),
	Mantle       UMETA(DisplayName = "Mantle")
};

UENUM(BlueprintType)
enum class EDoomMovementState : uint8
{
	Idle       UMETA(DisplayName = "Idle"),
	Walking    UMETA(DisplayName = "Walking"),
	Sprinting  UMETA(DisplayName = "Sprinting"),
	Sliding    UMETA(DisplayName = "Sliding"),
	Dashing    UMETA(DisplayName = "Dashing"),
	WallRunning UMETA(DisplayName = "Wall Running"),
	Mantling   UMETA(DisplayName = "Mantling"),
	Falling    UMETA(DisplayName = "Falling")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDoomMovementActionSignature, EDoomMovementAction, Action);

UCLASS()
class FPS_MOVEMENT_API UDoomMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	UDoomMovementComponent();

	// ── Sprint ──────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Sprint", meta=(ClampMin=0, Units="cm/s"))
	float SprintSpeed = 1200.0f;

	void StartSprint();
	void StopSprint();

	UFUNCTION(BlueprintPure, Category="Doom|Sprint")
	bool IsSprinting() const;

	// ── Slide ───────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0.5, ClampMax=2.0))
	float SlideEntrySpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0, Units="cm/s"))
	float SlideMinSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0, Units="cm/s"))
	float SlideExitSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0, Units="cm/s"))
	float SlideMaxSpeed = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0, ClampMax=10, Units="s"))
	float SlideMaxDuration = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0))
	float SlideGroundFrictionOverride = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0))
	float SlideBraking = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0))
	float SlideDeceleration = 950.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0, ClampMax=3, Units="s"))
	float SlideCooldown = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0))
	float SlideSlopeAcceleration = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0))
	float SlideSteeringAcceleration = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0, Units="cm/s"))
	float SlideJumpBoost = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=1.0, ClampMax=1.5))
	float SprintSlideEntryBoost = 1.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Slide", meta=(ClampMin=0, ClampMax=0.5, Units="s"))
	float SlideBufferTime = 0.25f;

	void StartSlide();
	void StopSlide(bool bForce = false);

	UFUNCTION(BlueprintPure, Category="Doom|Slide")
	bool IsSliding() const { return bIsSliding; }

	// ── Dash ────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Dash", meta=(ClampMin=0, Units="cm/s"))
	float DashSpeed = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Dash", meta=(ClampMin=0, ClampMax=5, Units="s"))
	float DashDuration = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Dash", meta=(ClampMin=0, ClampMax=10, Units="s"))
	float DashCooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Dash", meta=(ClampMin=1, ClampMax=5))
	int32 MaxDashCharges = 2;

	void PerformDash(const FVector& WorldDirection);

	UFUNCTION(BlueprintPure, Category="Doom|Dash")
	bool CanDash() const;

	UFUNCTION(BlueprintPure, Category="Doom|Dash")
	bool IsDashing() const { return bIsDashing; }

	UFUNCTION(BlueprintPure, Category="Doom|Dash")
	int32 GetDashCharges() const { return CurrentDashCharges; }

	UPROPERTY(BlueprintAssignable, Category="Doom|Dash")
	FDoomDashChargesChanged OnDashChargesChanged;

	// ── Bunny Hop ───────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|BunnyHop", meta=(ClampMin=0, Units="cm/s"))
	float BunnyHopSpeedBoost = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|BunnyHop", meta=(ClampMin=0, Units="cm/s"))
	float BunnyHopMaxSpeed = 2000.0f;

	void SetWantsToHoldJump(bool bHold);

	UFUNCTION(BlueprintPure, Category="Doom|BunnyHop")
	int32 GetConsecutiveHops() const { return ConsecutiveHops; }

	// ── Wall Run ────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, Units="cm/s"))
	float WallRunSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, Units="cm/s"))
	float WallRunMaxSpeed = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0))
	float WallRunAcceleration = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, Units="cm/s"))
	float WallRunMinSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, Units="cm/s"))
	float WallRunInitialUpVelocity = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, ClampMax=2, Units="s"))
	float WallRunReattachCooldown = 0.35f;

	/** Seconds between airborne wall-run trace checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, ClampMax=0.5, Units="s"))
	float WallRunCheckInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun")
	bool bWallRunRequiresInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun")
	bool bWallRunDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, ClampMax=1))
	float WallRunGravityScale = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, ClampMax=5, Units="s"))
	float WallRunMaxDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, Units="cm"))
	float WallRunMinHeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, Units="cm"))
	float WallRunTraceDistance = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, Units="cm/s"))
	float WallJumpOffForce = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|WallRun", meta=(ClampMin=0, Units="cm/s"))
	float WallJumpUpForce = 600.0f;

	UFUNCTION(BlueprintPure, Category="Doom|WallRun")
	bool IsWallRunning() const { return bIsWallRunning; }

	UFUNCTION(BlueprintPure, Category="Doom|WallRun")
	bool IsWallRunRight() const { return bWallRunIsRight; }

	UFUNCTION(BlueprintPure, Category="Doom|WallRun")
	FVector GetWallNormal() const { return WallRunNormal; }

	void WallJump();

	// ── Mantle ──────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Mantle", meta=(ClampMin=0, Units="cm"))
	float MantleReachHeight = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Mantle", meta=(ClampMin=0, Units="cm"))
	float MantleForwardDistance = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Mantle", meta=(ClampMin=0, ClampMax=2, Units="s"))
	float MantleDuration = 0.25f;

	bool TryMantle();

	UFUNCTION(BlueprintPure, Category="Doom|Mantle")
	bool IsMantling() const { return bIsMantling; }

	// ── Jump Forgiveness ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Jump", meta=(ClampMin=0, ClampMax=0.5, Units="s"))
	float CoyoteTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Jump", meta=(ClampMin=0, ClampMax=0.5, Units="s"))
	float JumpBufferTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Doom|Jump", meta=(ClampMin=0, ClampMax=1))
	float AirJumpRedirectStrength = 0.75f;

	void NotifyJumpInputPressed();

	UFUNCTION(BlueprintPure, Category="Doom|Jump")
	bool IsWithinCoyoteTime() const;

	// ── Events ──────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category="Doom|Events")
	FDoomMovementActionSignature OnMovementAction;

	// ── Speed Queries ───────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category="Doom|Speed")
	float GetSpeedRatio() const;

	UFUNCTION(BlueprintPure, Category="Doom|Speed")
	float GetHorizontalSpeed() const;

	UFUNCTION(BlueprintPure, Category="Doom|Speed")
	EDoomMovementState GetMovementState() const;

	UFUNCTION(BlueprintPure, Category="Doom|Speed")
	bool IsInFlowState() const { return bIsSliding || bIsWallRunning; }

	void SetExternalSpeedScale(float NewScale);

	UFUNCTION(BlueprintPure, Category="Doom|Speed")
	float GetExternalSpeedScale() const { return ExternalSpeedScale; }

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual float GetMaxAcceleration() const override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

	virtual bool CanAttemptJump() const override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:

	static constexpr uint8 CMOVE_WallRun = 0;

	float ExternalSpeedScale = 1.0f;

	bool bWantsToSprint = false;

	bool bIsSliding = false;
	bool bWantsToSlideHeld = false;
	float SlideTimer = 0.0f;
	float SavedGroundFriction = 0.0f;
	float SavedCapsuleHalfHeight = 0.0f;
	float SavedCapsuleRadius = 0.0f;
	float LastSlidePressTime = -1000.0f;
	float LastSlideEndTime = -1000.0f;
	FVector SlideDirection = FVector::ZeroVector;

	void UpdateSlide(float DeltaTime);
	bool CanSlide() const;
	bool CanStandUp() const;

	int32 CurrentDashCharges = 0;
	float DashCooldownTimer = 0.0f;
	float DashActiveTimer = 0.0f;
	bool bIsDashing = false;
	FVector DashVelocity = FVector::ZeroVector;

	void UpdateDash(float DeltaTime);

	bool bHoldingJump = false;
	int32 ConsecutiveHops = 0;

	float CoyoteAnchorTime = -1000.0f;
	float LastJumpPressTime = -1000.0f;

	bool bIsWallRunning = false;
	bool bWallRunIsRight = false;
	float WallRunTimer = 0.0f;
	float WallRunCurrentSpeed = 0.0f;
	float LastWallRunEndTime = -1000.0f;
	float WallRunCheckTimer = 0.0f;
	FVector WallRunNormal = FVector::ZeroVector;
	FVector WallRunDirection = FVector::ZeroVector;
	FVector LastWallRunNormal = FVector::ZeroVector;

	bool CheckWallRun(FHitResult& OutHit, bool& bOutIsRight) const;
	void TryStartWallRun();
	void PhysWallRun(float DeltaTime, int32 Iterations);
	void EndWallRun();

	bool bIsMantling = false;
	float MantleAlpha = 0.0f;
	FVector MantleStartLocation = FVector::ZeroVector;
	FVector MantleTargetLocation = FVector::ZeroVector;

	void UpdateMantle(float DeltaTime);
};
