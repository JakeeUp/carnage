#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ShooterDamageUtils.h"
#include "ShooterShootable.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UShooterShootable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Lets any actor (practice targets, breakables, ...) take weapon hits through the shared damage
 * pipeline without being a character. ShooterDamage::ApplyHit checks the hit actor for this
 * interface first; if HandleShot returns true, the hit is considered consumed and the shooter
 * still receives hit feedback (hitmarker, sounds) from OutOutcome.
 */
class IShooterShootable
{
	GENERATED_BODY()

public:

	/** Handle a weapon hit. Fill OutOutcome (DamageDealt, bKilled, bHeadshot) and return true if consumed. */
	virtual bool HandleShot(const FShooterHitInfo& Hit, FShooterHitOutcome& OutOutcome) = 0;
};
