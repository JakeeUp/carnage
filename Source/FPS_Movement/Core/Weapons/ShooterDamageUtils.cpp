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
#include "Kismet/GameplayStatics.h"

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
}

FShooterHitOutcome ShooterDamage::ApplyHit(const FShooterHitInfo& Hit)
{
	FShooterHitOutcome Outcome;

	if (!Hit.Target)
	{
		return Outcome;
	}

	// ── Character damage ────────────────────────────────────────────────
	if (ACharacter* HitCharacter = Cast<ACharacter>(Hit.Target))
	{
		const bool bWasDead = HitCharacter->ActorHasTag(DeadActorTag);

		float StyleMultiplier = 1.0f;
		if (IShooterWeaponHolder* Holder = Cast<IShooterWeaponHolder>(Hit.InstigatorPawn))
		{
			StyleMultiplier = Holder->GetOutgoingDamageMultiplier();
		}

		Outcome.bHeadshot = ResolveHeadshot(*HitCharacter, Hit);
		Outcome.DamageDealt = Hit.BaseDamage * (Outcome.bHeadshot ? Hit.HeadshotMultiplier : 1.0f) * StyleMultiplier;

		UGameplayStatics::ApplyDamage(
			HitCharacter,
			Outcome.DamageDealt,
			Hit.InstigatorController,
			Hit.DamageCauser,
			Hit.DamageType ? Hit.DamageType : TSubclassOf<UDamageType>(UDamageType::StaticClass()));

		Outcome.bDamagedCharacter = true;

		Outcome.bKilled = !bWasDead && HitCharacter->ActorHasTag(DeadActorTag);

		// ── Hit feedback to the shooter ─────────────────────────────────
		if (Hit.InstigatorPawn && Hit.InstigatorPawn != Hit.Target)
		{
			if (IShooterHitFeedbackReceiver* Receiver = Cast<IShooterHitFeedbackReceiver>(Hit.InstigatorPawn))
			{
				Receiver->NotifyDamageDealt(Hit.Target, Outcome.DamageDealt, Outcome.bKilled, Outcome.bHeadshot, Hit.HitLocation);
			}
		}

		// ── Juggle knockback ────────────────────────────────────────────
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

	// ── Physics push ────────────────────────────────────────────────────
	if (Hit.HitComponent && Hit.HitComponent->IsSimulatingPhysics())
	{
		Hit.HitComponent->AddImpulseAtLocation(Hit.HitDirection * Hit.PhysicsImpulse, Hit.HitLocation);
	}

	return Outcome;
}
