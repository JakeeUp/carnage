// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponRecoilComponent.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "DoomMovementComponent.h"
#include "DoomCameraEffectsComponent.h"
#include "DoomCameraModifier.h"
#include "ViewSpringMath.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraComponent.h"
#include "AnimationRuntime.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarCarnageDrawSight(
	TEXT("carnage.Debug.DrawSight"),
	0,
	TEXT("1 = draw the camera axis (green) and the weapon's computed sight line (red), and print the alignment error."),
	ECVF_Cheat);
#endif

namespace
{
	FORCEINLINE FRotator AnglesToRotator(const FVector& PitchYawRoll)
	{
		return FRotator(PitchYawRoll.X, PitchYawRoll.Y, PitchYawRoll.Z);
	}

	/** Lerp location/scale and slerp rotation. */
	FTransform BlendTransforms(const FTransform& A, const FTransform& B, float Alpha)
	{
		if (Alpha <= 0.0f) return A;
		if (Alpha >= 1.0f) return B;

		return FTransform(
			FQuat::Slerp(A.GetRotation(), B.GetRotation(), Alpha),
			FMath::Lerp(A.GetLocation(), B.GetLocation(), static_cast<double>(Alpha)),
			FMath::Lerp(A.GetScale3D(), B.GetScale3D(), static_cast<double>(Alpha)));
	}
}

UWeaponRecoilComponent::UWeaponRecoilComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Run after animation and attachment updates so the sight sampled from the weapon is this
	// frame's, and before the camera manager builds the view.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
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
			HipRelative = FirstPersonMesh->GetRelativeTransform();
			AddTickPrerequisiteComponent(FirstPersonMesh);
		}
	}
}

// ─── Owner queries ──────────────────────────────────────────────────────────

float UWeaponRecoilComponent::GetADSAlpha() const
{
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
	return Shooter ? Shooter->GetADSAlpha() : 0.0f;
}

float UWeaponRecoilComponent::GetADSGameplayAlpha() const
{
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
	return Shooter ? Shooter->GetADSGameplayAlpha() : 0.0f;
}

AShooterWeapon* UWeaponRecoilComponent::GetCurrentWeapon() const
{
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
	return Shooter ? Shooter->GetCurrentWeapon() : nullptr;
}

// ─── Recoil input ───────────────────────────────────────────────────────────

void UWeaponRecoilComponent::AddRecoil(const FWeaponRecoilParams& Params)
{
	const float VisualAlpha = GetADSAlpha();
	const float AimScale = FMath::Lerp(1.0f, ADSAimRecoilScale, GetADSGameplayAlpha());
	const float KickScale = FMath::Lerp(1.0f, ADSVisualKickScale, VisualAlpha) * VisualScale;

	// (a) Aim recoil: drained into the control rotation over the next frames.
	PendingPitch += Params.PitchKick * AimScale;
	PendingYaw += FMath::FRandRange(-Params.YawKickRandom, Params.YawKickRandom) * AimScale;

	// (b) View punch: velocity impulse on the camera-only spring. Scaling by omega makes the
	// peak roughly proportional to the authored value regardless of stiffness.
	if (bEnableViewPunch)
	{
		const float PunchOmega = FMath::Sqrt(ViewPunchStiffness);
		PunchAnglesVelocity += FVector(
			Params.ViewPunchPitch,
			FMath::FRandRange(-Params.ViewPunchYawRandom, Params.ViewPunchYawRandom),
			FMath::FRandRange(-Params.ViewPunchRollRandom, Params.ViewPunchRollRandom)) * PunchOmega * AimScale;
	}

	// (c) Viewmodel kick: back + up on the stiff positional spring, climb/yaw/roll on the
	// softer rotational spring.
	const float PosOmega = FMath::Sqrt(KickSpringStiffness);
	KickLocationVelocity += FVector(-Params.VisualKickBack, 0.0f, Params.VisualKickUp) * PosOmega * KickScale;

	const float RotOmega = FMath::Sqrt(KickRotationStiffness);
	KickAnglesVelocity += FVector(
		Params.VisualKickPitch,
		FMath::FRandRange(-Params.VisualKickYawRandom, Params.VisualKickYawRandom),
		FMath::FRandRange(-Params.VisualKickRollRandom, Params.VisualKickRollRandom)) * RotOmega * KickScale;

	TimeSinceLastKick = 0.0f;
}

void UWeaponRecoilComponent::NotifyAimInput(float YawInput, float PitchInput)
{
	// Pulling down against the climb eats into the amount recovery would otherwise return.
	const float PitchDegrees = AimInputToDegrees(PitchInput);
	if (PitchDegrees < 0.0f && AccumulatedPitch > 0.0f)
	{
		AccumulatedPitch = FMath::Max(0.0f, AccumulatedPitch + PitchDegrees);
	}
}

float UWeaponRecoilComponent::AimInputToDegrees(float PitchInput) const
{
	// Project has bEnableLegacyInputScales=True: AddControllerPitchInput multiplies by the PC's legacy pitch scale (default -2.5).
	if (GetDefault<UInputSettings>()->bEnableLegacyInputScales)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return PitchInput * (PC ? PC->InputPitchScale_DEPRECATED : -2.5f);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

// ─── Sight ──────────────────────────────────────────────────────────────────

bool UWeaponRecoilComponent::GetSightInWeaponComponent(const AShooterWeapon* Weapon, const USkeletalMeshComponent* WeaponMesh, FTransform& OutSight)
{
	const USkeletalMesh* Mesh = WeaponMesh ? WeaponMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Weapon || !Mesh)
	{
		return false;
	}

	if (CachedSightMesh.Get() == Mesh && CachedSightWeapon.Get() == Weapon)
	{
		OutSight = CachedSightInWeapon;
		return bCachedSightValid;
	}

	CachedSightMesh = Mesh;
	CachedSightWeapon = Weapon;
	bCachedSightValid = false;
	CachedSightInWeapon = FTransform::Identity;

	// Socket frame in component space, evaluated on the reference pose.
	auto SocketInComponent = [Mesh](FName SocketName, FTransform& OutTransform) -> bool
	{
		FTransform SocketLocal;
		int32 BoneIndex = INDEX_NONE;
		int32 SocketIndex = INDEX_NONE;
		if (SocketName.IsNone() || !Mesh->FindSocketInfo(SocketName, SocketLocal, BoneIndex, SocketIndex) || BoneIndex == INDEX_NONE)
		{
			return false;
		}
		OutTransform = SocketLocal * FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), BoneIndex);
		return true;
	};

	FTransform SocketTransform;
	if (SocketInComponent(Weapon->GetSightSocketName(), SocketTransform))
	{
		// A dedicated sight socket is authored in sight convention already.
		CachedSightInWeapon = SocketTransform;
		bCachedSightValid = true;
	}
	else if (SocketInComponent(Weapon->GetFallbackSightSocketName(), SocketTransform))
	{
		// Fallback (e.g. the muzzle): SightOffset moves it onto the sight line, in socket space.
		CachedSightInWeapon = Weapon->GetSightOffset() * SocketTransform;
		bCachedSightValid = true;
	}

	OutSight = CachedSightInWeapon;
	return bCachedSightValid;
}

bool UWeaponRecoilComponent::ComputeSightInArms(FTransform& OutSightInArms)
{
	AShooterWeapon* Weapon = GetCurrentWeapon();
	USkeletalMeshComponent* WeaponMesh = Weapon ? Weapon->GetFirstPersonMesh() : nullptr;
	if (!FirstPersonMesh || !WeaponMesh)
	{
		return false;
	}

	FTransform SightInWeapon;
	if (!GetSightInWeaponComponent(Weapon, WeaponMesh, SightInWeapon))
	{
		return false;
	}

	// The weapon component follows the animated hand socket; this part is live every frame.
	const FTransform WeaponInArms = WeaponMesh->GetComponentTransform().GetRelativeTransform(FirstPersonMesh->GetComponentTransform());
	OutSightInArms = SightInWeapon * WeaponInArms;
	return true;
}

float UWeaponRecoilComponent::GetSafeEyeRelief(const AShooterWeapon* Weapon) const
{
	const float EyeRelief = Weapon ? Weapon->GetEyeRelief() : 28.0f;

	// First-person primitives are scaled towards the eye by FirstPersonScale, so the rendered
	// distance is EyeRelief * scale; keep that a little beyond the near clip plane.
	const float FPScale = (Camera && Camera->bEnableFirstPersonScale) ? FMath::Max(Camera->FirstPersonScale, 0.01f) : 1.0f;
	const float MinRelief = (GNearClippingPlane + 2.0f) / FPScale;
	return FMath::Max(EyeRelief, MinRelief);
}

// ─── Tick ───────────────────────────────────────────────────────────────────

void UWeaponRecoilComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const UDoomMovementComponent* DoomMovement = Cast<UDoomMovementComponent>(Movement);
	const AShooterWeapon* Weapon = GetCurrentWeapon();

	const float Alpha = GetADSAlpha();

	TimeSinceLastKick += DeltaTime;

	// ── 1. Aim recoil: drain pending kick into the control rotation ─────
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

	// Recovery returns the portion the player did not pull down themselves.
	if (TimeSinceLastKick >= RecoveryDelay && AccumulatedPitch > 0.0f)
	{
		const float Recover = FMath::Min(AccumulatedPitch, RecoverySpeed * DeltaTime);
		ApplyAimRotation(-Recover, 0.0f);
		AccumulatedPitch -= Recover;
	}

	// ── 2. Impulse springs: kick, view punch, landing ───────────────────
	ViewSpring::StepImpulse(KickLocation, KickLocationVelocity, KickSpringStiffness, KickSpringDamping, DeltaTime);
	ViewSpring::StepImpulse(KickAngles, KickAnglesVelocity, KickRotationStiffness, KickRotationDamping, DeltaTime);
	ViewSpring::StepImpulse(PunchAngles, PunchAnglesVelocity, ViewPunchStiffness, ViewPunchDamping, DeltaTime);

	if (Movement)
	{
		const bool bFalling = Movement->IsFalling();
		if (bFalling)
		{
			MaxFallSpeed = FMath::Max(MaxFallSpeed, static_cast<float>(-Movement->Velocity.Z));
		}
		else if (bWasFalling && bEnableLandingInertia && MaxFallSpeed > LandingMinImpactSpeed)
		{
			LandingVelocity -= MaxFallSpeed * LandingImpulseScale;
		}
		if (!bFalling)
		{
			MaxFallSpeed = 0.0f;
		}
		bWasFalling = bFalling;
	}
	ViewSpring::StepImpulse(LandingOffset, LandingVelocity, LandingStiffness,
		ViewSpring::DampingFromRatio(LandingStiffness, LandingDampingRatio), DeltaTime);

	// ── 3. Look lag (lagging facing chases the view) ────────────────────
	FRotator LookLag = FRotator::ZeroRotator;
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
		const FQuat ViewQuat = Controller ? Controller->GetControlRotation().Quaternion() : FQuat::Identity;

		if (!bLaggedFacingValid)
		{
			LaggedFacing = ViewQuat;
			bLaggedFacingValid = true;
		}

		const float ChaseAlpha = 1.0f - FMath::Exp(-SwayInterpSpeed * DeltaTime);
		LaggedFacing = FQuat::Slerp(LaggedFacing, ViewQuat, ChaseAlpha).GetNormalized();

		const float MaxLagRad = FMath::DegreesToRadians(MaxLookSway);
		const float LagRad = static_cast<float>(LaggedFacing.AngularDistance(ViewQuat));
		if (LagRad > MaxLagRad && LagRad > UE_KINDA_SMALL_NUMBER)
		{
			LaggedFacing = FQuat::Slerp(ViewQuat, LaggedFacing, MaxLagRad / LagRad).GetNormalized();
		}

		if (bEnableSway)
		{
			// Lag expressed in the view's frame: negative yaw = the gun trails to the left.
			LookLag = (ViewQuat.Inverse() * LaggedFacing).Rotator();
		}
	}

	// ── 4. Velocity sway ────────────────────────────────────────────────
	FVector TargetMoveSway = FVector::ZeroVector;
	if (bEnableSway && Character)
	{
		const FVector Velocity = Character->GetVelocity();
		const float LateralSpeed = FVector::DotProduct(Velocity, Character->GetActorRightVector());

		TargetMoveSway.Y = FMath::Clamp(-LateralSpeed / BobReferenceSpeed, -1.0f, 1.0f) * MoveSwayLateral;
		TargetMoveSway.Z = FMath::Clamp(-Velocity.Z * MoveSwayVertical, -MaxMoveSwayVertical, MaxMoveSwayVertical);
	}
	MoveSwayOffset = FMath::VInterpTo(MoveSwayOffset, TargetMoveSway, DeltaTime, SwayInterpSpeed);

	// ── 5. Figure-8 bob (vertical at twice the lateral frequency) ───────
	FVector BobLocation = FVector::ZeroVector;
	float BobRoll = 0.0f;
	{
		const bool bGroundedMove = Movement && Movement->IsMovingOnGround() && !(DoomMovement && DoomMovement->IsSliding());
		const float GroundSpeed = Movement ? static_cast<float>(Movement->Velocity.Size2D()) : 0.0f;
		const float TargetWeight = (bEnableBob && bGroundedMove) ? FMath::Clamp(GroundSpeed / BobReferenceSpeed, 0.0f, 1.5f) : 0.0f;

		BobWeight = FMath::FInterpTo(BobWeight, TargetWeight, DeltaTime, 8.0f);
		if (BobWeight > KINDA_SMALL_NUMBER)
		{
			BobTime += DeltaTime;
		}
		else
		{
			BobTime = 0.0f;
		}

		const float VerticalPhase = UE_TWO_PI * BobTime / BobVerticalPeriod;
		const float LateralPhase = UE_TWO_PI * BobTime / BobLateralPeriod;

		BobLocation.Y = FMath::Sin(LateralPhase) * BobLateralAmplitude * BobWeight;
		BobLocation.Z = FMath::Sin(VerticalPhase) * BobVerticalAmplitude * BobWeight;
		BobRoll = FMath::Sin(LateralPhase) * BobRollAmplitude * BobWeight;
	}

	// ── 6. Sprint / slide pose blends ───────────────────────────────────
	{
		const float SprintTarget = (DoomMovement && DoomMovement->IsSprinting()) ? 1.0f : 0.0f;
		const float SlideTarget = (DoomMovement && DoomMovement->IsSliding()) ? 1.0f : 0.0f;
		ViewSpring::CriticalStep(SprintPoseAlpha, SprintPoseVelocity, SprintTarget, PoseSmoothingTime, DeltaTime);
		ViewSpring::CriticalStep(SlidePoseAlpha, SlidePoseVelocity, SlideTarget, PoseSmoothingTime, DeltaTime);
	}

	// ── 7. Wall avoidance: retract the weapon when it would poke through geometry ──
	float TargetPullback = 0.0f;
	if (bEnableWallAvoidance && Camera && GetWorld())
	{
		const FVector Start = Camera->GetComponentLocation();
		const FVector End = Start + Camera->GetForwardVector() * WallAvoidanceTraceDistance;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponWallAvoid), false, GetOwner());

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			const float Closeness = 1.0f - FMath::Clamp(Hit.Distance / WallAvoidanceTraceDistance, 0.0f, 1.0f);
			TargetPullback = Closeness * MaxWallPullback;
		}
	}
	CurrentWallPullback = FMath::FInterpTo(CurrentWallPullback, TargetPullback, DeltaTime, WallAvoidanceInterpSpeed);

	// ── 8. Compose the procedural offset (scaled by the ADS alpha) ──────
	const float SwayScale = FMath::Lerp(1.0f, ADSSwayScale, Alpha) * VisualScale;
	const float MoveScale = FMath::Lerp(1.0f, ADSMoveSwayScale, Alpha) * VisualScale;
	const float PoseScale = 1.0f - Alpha;

	FVector OffsetLocation =
		  FVector(0.0f, LookLag.Yaw, LookLag.Pitch) * LookSwayPositionScale * SwayScale
		+ BobLocation * SwayScale
		+ (MoveSwayOffset + FVector(0.0f, 0.0f, LandingOffset)) * MoveScale
		+ KickLocation
		+ (SprintPoseLocation * SprintPoseAlpha + SlidePoseLocation * SlidePoseAlpha) * PoseScale;
	// First-person rendering already prevents wall clipping; the pullback is a hip-fire nicety only.
	// In ADS it would drag the sights into the eye, so it fades out with the ADS alpha.
	OffsetLocation.X -= CurrentWallPullback * (1.0f - Alpha);

	const FRotator OffsetRotation =
		  FRotator(LookLag.Pitch, LookLag.Yaw, LookLag.Yaw * 0.35f) * (LookSwayScale * SwayScale)
		+ FRotator(0.0f, 0.0f, BobRoll * SwayScale)
		+ AnglesToRotator(KickAngles)
		+ (SprintPoseRotation * SprintPoseAlpha + SlidePoseRotation * SlidePoseAlpha) * PoseScale;

	const FTransform Offset(OffsetRotation, OffsetLocation);

	// ── 9. Hip and ADS transforms of the arms mesh, in camera space ─────
	// UE composes A * B as "A, then B in B's parent space".
	// Hip: authored pose, then the offset applied about the eye.
	const FTransform HipTransform = HipRelative * Offset;

	// ADS: S_m^-1 puts the sight frame at the origin, the offset then acts about the SIGHT, and
	// Target places the sight on the camera axis at EyeRelief.
	FTransform Final = HipTransform;
	FTransform SightInArms;
	if (Alpha > 0.0f && ComputeSightInArms(SightInArms))
	{
		const FTransform Target(FQuat::Identity, FVector(GetSafeEyeRelief(Weapon), 0.0f, 0.0f));

		// Blend the SIGHT's camera-space transform, not the arms root. The arms root sits far
		// below the camera, so slerping it swings the gun in a huge arc (it leaves the screen
		// mid-transition). Blending about the sight makes the gun glide straight to the eye.
		// Endpoints are unchanged: Alpha 0 = HipTransform, Alpha 1 = S^-1 * Offset * Target.
		const FTransform HipSight = SightInArms * HipTransform;
		const FTransform AdsSight = Offset * Target;
		const FTransform BlendedSight = BlendTransforms(HipSight, AdsSight, Alpha);
		Final = SightInArms.Inverse() * BlendedSight;
	}

	// ── 10. View punch: camera modifier + matching viewmodel rotation ───
	const FRotator Punch = AnglesToRotator(PunchAngles);
	if (const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner()))
	{
		const UDoomCameraEffectsComponent* CameraEffects = Shooter->FindComponentByClass<UDoomCameraEffectsComponent>();
		if (UDoomCameraModifier* Modifier = CameraEffects ? CameraEffects->GetCameraModifier() : nullptr)
		{
			Modifier->PunchRotation = Punch;
		}
	}

	const float PunchFollow = FMath::Lerp(ViewPunchViewmodelFollowHip, ViewPunchViewmodelFollowADS, Alpha);
	if (PunchFollow > 0.0f && !Punch.IsNearlyZero(1.e-4f))
	{
		// The view is camera * punch; post-multiplying keeps the gun at the same place in it.
		Final = Final * FTransform((Punch * PunchFollow).Quaternion());
	}

	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetRelativeTransform(Final);
	}

#if !UE_BUILD_SHIPPING
	if (CVarCarnageDrawSight.GetValueOnGameThread() != 0)
	{
		DrawSightDebug();
	}
#endif
}

// ─── Debug ──────────────────────────────────────────────────────────────────

void UWeaponRecoilComponent::DrawSightDebug() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	const AShooterWeapon* Weapon = GetCurrentWeapon();
	const USkeletalMeshComponent* WeaponMesh = Weapon ? Weapon->GetFirstPersonMesh() : nullptr;
	if (!World || !Camera || !WeaponMesh || !bCachedSightValid || CachedSightWeapon.Get() != Weapon)
	{
		return;
	}

	// Re-read the weapon's world transform AFTER the arms were moved, so this verifies the
	// actual result rather than restating the maths.
	const FTransform SightWorld = CachedSightInWeapon * WeaponMesh->GetComponentTransform();
	const FVector CamLoc = Camera->GetComponentLocation();
	const FVector CamFwd = Camera->GetForwardVector();
	const FVector SightLoc = SightWorld.GetLocation();
	const FVector SightFwd = SightWorld.GetUnitAxis(EAxis::X);
	const FVector SightUp = SightWorld.GetUnitAxis(EAxis::Z);

	constexpr float LineLength = 300.0f;
	DrawDebugLine(World, CamLoc + CamFwd * 5.0f, CamLoc + CamFwd * LineLength, FColor::Green, false, -1.0f, SDPG_Foreground, 0.0f);
	DrawDebugLine(World, SightLoc, SightLoc + SightFwd * LineLength, FColor::Red, false, -1.0f, SDPG_Foreground, 0.0f);
	DrawDebugLine(World, SightLoc, SightLoc + SightUp * 3.0f, FColor::Blue, false, -1.0f, SDPG_Foreground, 0.0f);
	DrawDebugPoint(World, SightLoc, 6.0f, FColor::Yellow, false, -1.0f, SDPG_Foreground);

	// Position error: angle between the camera axis and the eye->sight direction.
	// Direction error: angle between the camera axis and the sight's +X.
	const FVector EyeToSight = (SightLoc - CamLoc).GetSafeNormal();
	const float PosErrorDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(EyeToSight, CamFwd), -1.0, 1.0)));
	const float DirErrorDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(SightFwd, CamFwd), -1.0, 1.0)));
	const float Distance = static_cast<float>(FVector::Dist(SightLoc, CamLoc));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.0f, FColor::Cyan,
			FString::Printf(TEXT("[Sight] alpha=%.3f  posErr=%.3f deg  dirErr=%.3f deg  dist=%.1f cm  punch=(%.2f, %.2f, %.2f)"),
				GetADSAlpha(), PosErrorDeg, DirErrorDeg, Distance, PunchAngles.X, PunchAngles.Y, PunchAngles.Z));
	}
#endif
}
