#pragma once

#include "CoreMinimal.h"
#include "Animation/SpringMath.h"

/**
 * Small spring helpers shared by the camera and viewmodel code.
 *
 * Two flavours are used:
 *  - Critically damped "smoothing" springs (SpringMath::CriticalSpringDamper). They chase a
 *    target with no overshoot and carry velocity, so a target that reverses mid-flight
 *    decelerates and turns around instead of popping. Used for the ADS alpha and pose blends.
 *  - Stiffness/damping "impulse" springs (semi-implicit Euler, sub-stepped). They rest at zero
 *    and are excited by adding velocity. Used for kick, view punch and landing inertia.
 */
namespace ViewSpring
{
	/** Critically damped step towards Target. SmoothingTime is roughly the lag in seconds. */
	template <typename T>
	FORCEINLINE void CriticalStep(T& InOutX, T& InOutV, const T& Target, float SmoothingTime, float DeltaTime)
	{
		// SpringMath is tagged experimental in 5.7; the closed-form solver itself is stable.
		PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
		SpringMath::CriticalSpringDamper(InOutX, InOutV, Target, SmoothingTime, DeltaTime);
		PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
	}

	/** Damping coefficient that gives damping ratio Zeta for a unit-mass spring of stiffness K. */
	FORCEINLINE float DampingFromRatio(float Stiffness, float Zeta)
	{
		return 2.0f * Zeta * FMath::Sqrt(FMath::Max(Stiffness, 0.0f));
	}

	/**
	 * Advance a zero-rest spring (unit mass) by DeltaTime using fixed sub-steps so that stiff
	 * springs stay stable at low frame rates. Works for float and FVector.
	 */
	template <typename T>
	void StepImpulse(T& InOutX, T& InOutV, float Stiffness, float Damping, float DeltaTime)
	{
		constexpr float MaxStep = 1.0f / 120.0f;
		float Remaining = FMath::Min(DeltaTime, 0.1f);
		while (Remaining > UE_KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Remaining, MaxStep);
			Remaining -= Step;

			const T Accel = InOutX * -Stiffness - InOutV * Damping;
			InOutV += Accel * Step;
			InOutX += InOutV * Step;
		}
	}
}
