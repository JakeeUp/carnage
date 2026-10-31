// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarnageAttributeSet.h"
#include "CarnageGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"

UCarnageAttributeSet::UCarnageAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitArmor(0.0f);
	InitMaxArmor(100.0f);
	InitShield(0.0f);
	InitMaxShield(100.0f);
	InitIncomingDamage(0.0f);
	InitIncomingHealing(0.0f);
}

void UCarnageAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxArmorAttribute() || Attribute == GetMaxShieldAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetArmorAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxArmor());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxShield());
	}
}

void UCarnageAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
	AActor* Source = Context.GetEffectCauser();
	AActor* Target = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		Target = Data.Target.AbilityActorInfo->AvatarActor.Get();
	}

	const FGameplayTagContainer& SpecAssetTags = Data.EffectSpec.GetDynamicAssetTags();

	// ── Incoming Damage ─────────────────────────────────────────────────
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float LocalIncoming = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (LocalIncoming <= 0.0f) return;

		float Remaining = LocalIncoming;

		if (GetShield() > 0.0f)
		{
			const float ShieldAbsorbed = FMath::Min(Remaining, GetShield());
			SetShield(GetShield() - ShieldAbsorbed);
			Remaining -= ShieldAbsorbed;
			BroadcastShieldChanged();
		}

		if (Remaining > 0.0f && GetArmor() > 0.0f && ArmorAbsorptionRatio > 0.0f)
		{
			const float ArmorAbsorbed = FMath::Min(Remaining * ArmorAbsorptionRatio, GetArmor());
			SetArmor(GetArmor() - ArmorAbsorbed);
			Remaining -= ArmorAbsorbed;
			BroadcastArmorChanged();
		}

		if (Remaining > 0.0f)
		{
			const float NewHealth = FMath::Max(0.0f, GetHealth() - Remaining);
			SetHealth(NewHealth);
			BroadcastHealthChanged();
		}

		OnDamageDealt.Broadcast(Source, Target, LocalIncoming, SpecAssetTags);

		if (GetHealth() <= 0.0f && !bDied)
		{
			bDied = true;
			OnDied.Broadcast();
		}
		return;
	}

	// ── Incoming Healing ────────────────────────────────────────────────
	if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float LocalHeal = GetIncomingHealing();
		SetIncomingHealing(0.0f);

		if (LocalHeal <= 0.0f) return;

		SetHealth(FMath::Min(GetHealth() + LocalHeal, GetMaxHealth()));
		BroadcastHealthChanged();
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		BroadcastHealthChanged();
		if (GetHealth() <= 0.0f && !bDied)
		{
			bDied = true;
			OnDied.Broadcast();
		}
	}
	else if (Data.EvaluatedData.Attribute == GetArmorAttribute())
	{
		BroadcastArmorChanged();
	}
}

void UCarnageAttributeSet::BroadcastHealthChanged()
{
	const float MaxHP = GetMaxHealth();
	OnHealthChanged.Broadcast(MaxHP > 0.0f ? GetHealth() / MaxHP : 0.0f);
}

void UCarnageAttributeSet::BroadcastShieldChanged()
{
	const float MaxShd = GetMaxShield();
	OnShieldChanged.Broadcast(MaxShd > 0.0f ? GetShield() / MaxShd : 0.0f);
}

void UCarnageAttributeSet::BroadcastArmorChanged()
{
	const float MaxArm = GetMaxArmor();
	OnArmorChanged.Broadcast(MaxArm > 0.0f ? GetArmor() / MaxArm : 0.0f);
}
