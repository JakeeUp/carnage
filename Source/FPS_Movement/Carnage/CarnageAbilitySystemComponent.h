// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CarnageAbilitySystemComponent.generated.h"

UCLASS(ClassGroup="Carnage", meta=(BlueprintSpawnableComponent))
class FPS_MOVEMENT_API UCarnageAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	UCarnageAbilitySystemComponent();

	UFUNCTION(BlueprintCallable, Category="Carnage|Damage")
	bool ApplyDamageToTarget(
		AActor* Target,
		TSubclassOf<class UGameplayEffect> DamageEffectClass,
		float DamageAmount,
		const FGameplayTagContainer& DamageTags);

	UFUNCTION(BlueprintCallable, Category="Carnage|Damage")
	bool ApplyDamageToSelf(
		TSubclassOf<class UGameplayEffect> DamageEffectClass,
		float DamageAmount,
		AActor* Source,
		const FGameplayTagContainer& DamageTags);
};
