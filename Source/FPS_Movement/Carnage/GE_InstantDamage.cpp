// Copyright Epic Games, Inc. All Rights Reserved.

#include "GE_InstantDamage.h"
#include "CarnageAttributeSet.h"
#include "CarnageGameplayTags.h"

UGE_InstantDamage::UGE_InstantDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo DamageMod;
	DamageMod.Attribute = UCarnageAttributeSet::GetIncomingDamageAttribute();
	DamageMod.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = CarnageTags::SetByCaller_Damage;

	FGameplayEffectModifierMagnitude Magnitude(SetByCaller);
	DamageMod.ModifierMagnitude = Magnitude;

	Modifiers.Add(DamageMod);
}
