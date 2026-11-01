// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterDamageUtils.h"
#include "ShooterHitFeedbackInterface.h"
#include "ShooterWeaponHolder.h"
#include "Core/AI/JuggleComponent.h"
#include "DoomMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/DamageType.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ShooterShootable.h"
#include "FPS_Movement.h"

namespace
{
	const FName DeadActorTag(TEXT("Dead"));

	bool ResolveHeadshot(const ACharacter& Character, const FShooterHitInfo& Hit)
	{
		if (!Hit.bAllowHeadshot)
		{
			return false;
		}

		if (!Hit.BoneName.IsNone())
		{
			return Hit.BoneName.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase);
		}

		if (const UCapsuleComponent* Capsule = Character.GetCapsuleComponent())
		{
			const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			const float FeetZ = Character.GetActorLocation().Z - HalfHeight;
			const float HeadThresholdZ = FeetZ + (2.0f * HalfHeight * Hit.HeadshotHeightFraction);
			return Hit.HitLocation.Z >= HeadThresholdZ;
		}

		return false;
	}

	void NotifyShooter(const FShooterHitInfo& Hit, const FShooterHitOutcome& Outcome)
	{
		if (Hit.InstigatorPawn && Hit.InstigatorPawn != Hit.Target)
		{
			if (IShooterHitFeedbackReceiver* Receiver = Cast<IShooterHitFeedbackReceiver>(Hit.InstigatorPawn))
			{
				Receiver->NotifyDamageDealt(Hit.Target, Outcome.DamageDealt, Outcome.bKilled, Outcome.bHeadshot, Hit.HitLocation);
			}
		}
	}
}

FShooterHitOutcome ShooterDamage::ApplyHit(const FShooterHitInfo& Hit)
{
	FShooterHitOutcome Outcome;

	if (!Hit.Target)
	{
		return Outcome;
	}

	// ── Shootable actors (practice targets, breakables) ────────────────
	if (IShooterShootable* Shootable = Cast<IShooterShootable>(Hit.Target))
	{
		if (Shootable->HandleShot(Hit, Outcome))
		{
			NotifyShooter(Hit, Outcome);
			return Outcome;
		}
	}

	// ── Character damage ────────────────────────────────────────────────
	if (ACharacter* HitCharacter = Cast<ACharacter>(Hit.Target))
	{
		const bool bWasDead = HitCharacter->ActorHasTag(DeadActorTag);

		// AI rounds don't hurt other AI: with bullets now reaching pawns, a squad would gun itself down.
		const bool bAIFriendlyFire = Hit.InstigatorPawn && Hit.InstigatorPawn != HitCharacter
			&& !Hit.InstigatorPawn->IsPlayerControlled() && !HitCharacter->IsPlayerControlled();
		if (bAIFriendlyFire)
		{
			return Outcome;
		}

		// Corpses take no damage and give no hit feedback, but still get the physics push below.
		if (!bWasDead)
		{
			float StyleMultiplier = 1.0f;
			if (IShooterWeaponHolder* Holder = Cast<IShooterWeaponHolder>(Hit.InstigatorPawn))
			{
				StyleMultiplier = Holder->GetOutgoingDamageMultiplier();
			}

			Outcome.bHeadshot = ResolveHeadshot(*HitCharacter, Hit);
			Outcome.DamageDealt = Hit.BaseDamage * (Outcome.bHeadshot ? Hit.HeadshotMultiplier : 1.0f) * StyleMultiplier;

			if (Outcome.DamageDealt != 0.0f)
			{
				// Point damage, so the receiver knows the bone, location and direction for hit reactions.
				FHitResult PointHit(HitCharacter, Hit.HitComponent, Hit.HitLocation, -Hit.HitDirection);
				PointHit.BoneName = Hit.BoneName;

				FShooterPointDamageEvent DamageEvent(
					Outcome.DamageDealt,
					PointHit,
					Hit.HitDirection,
					Hit.DamageType ? Hit.DamageType : TSubclassOf<UDamageType>(UDamageType::StaticClass()));
				DamageEvent.bHeadshot = Outcome.bHeadshot;
				DamageEvent.PhysicsImpulse = Hit.PhysicsImpulse;

				HitCharacter->TakeDamage(Outcome.DamageDealt, DamageEvent, Hit.InstigatorController, Hit.DamageCauser);
			}

			Outcome.bDamagedCharacter = true;

			Outcome.bKilled = HitCharacter->ActorHasTag(DeadActorTag);

			// ── Hit feedback to the shooter ─────────────────────────────
			NotifyShooter(Hit, Outcome);

			// ── Juggle knockback ────────────────────────────────────────
			if (Hit.KnockbackForce > 0.0f && !Outcome.bKilled)
			{
				if (UJuggleComponent* Juggle = HitCharacter->FindComponentByClass<UJuggleComponent>())
				{
					bool bLauncher = false;
					if (const ACharacter* InstChar = Cast<ACharacter>(Hit.InstigatorPawn))
					{
						if (const UDoomMovementComponent* InstMove = Cast<UDoomMovementComponent>(InstChar->GetCharacterMovement()))
						{
							bLauncher = InstMove->IsSliding();
						}
					}
					Juggle->ApplyKnockback(Hit.HitDirection, Hit.KnockbackForce, bLauncher);
				}
			}
		}
	}

	// ── Physics push ────────────────────────────────────────────────────
	// Living characters drive their own reaction (flinch / death impulse) from the damage event.
	if (!Outcome.bDamagedCharacter && Hit.HitComponent && Hit.HitComponent->IsSimulatingPhysics(Hit.BoneName))
	{
		Hit.HitComponent->AddImpulseAtLocation(Hit.HitDirection * Hit.PhysicsImpulse, Hit.HitLocation, Hit.BoneName);
	}

	return Outcome;
}

bool ShooterDamage::SetupPerBoneWeaponHits(ACharacter* Character, bool bEnablePhysicsBodies)
{
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Mesh || !Capsule || !Mesh->GetPhysicsAsset())
	{
		return false;
	}

	if (bEnablePhysicsBodies)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else if (!Mesh->IsQueryCollisionEnabled())
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	Mesh->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	return true;
}
