// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarnageAbilitySystemComponent.h"
#include "CarnageGameplayTags.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

UCarnageAbilitySystemComponent::UCarnageAbilitySystemComponent()
{
	// Single-player: attributes are not replicated.
	SetIsReplicatedByDefault(false);
}

bool UCarnageAbilitySystemComponent::ApplyDamageToTarget(
	AActor* Target,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float DamageAmount,
	const FGameplayTagContainer& DamageTags)
{
	if (!Target || !DamageEffectClass || DamageAmount <= 0.0f) return false;

	IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(Target);
	if (!TargetASI) return false;

	UAbilitySystemComponent* TargetASC = TargetASI->GetAbilitySystemComponent();
	if (!TargetASC) return false;

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetOwner());

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid()) return false;

	SpecHandle.Data->SetSetByCallerMagnitude(CarnageTags::SetByCaller_Damage, DamageAmount);
	SpecHandle.Data->AppendDynamicAssetTags(DamageTags);

	const FActiveGameplayEffectHandle Applied = ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
	return Applied.WasSuccessfullyApplied();
}

bool UCarnageAbilitySystemComponent::ApplyDamageToSelf(
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float DamageAmount,
	AActor* Source,
	const FGameplayTagContainer& DamageTags)
{
	if (!DamageEffectClass || DamageAmount <= 0.0f) return false;

	FGameplayEffectContextHandle Context = MakeEffectContext();
	if (Source)
	{
		Context.AddSourceObject(Source);
		Context.AddInstigator(Source, Source);
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid()) return false;

	SpecHandle.Data->SetSetByCallerMagnitude(CarnageTags::SetByCaller_Damage, DamageAmount);
	SpecHandle.Data->AppendDynamicAssetTags(DamageTags);

	const FActiveGameplayEffectHandle Applied = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	return Applied.WasSuccessfullyApplied();
}
