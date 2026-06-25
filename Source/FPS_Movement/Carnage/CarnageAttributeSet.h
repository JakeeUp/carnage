// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CarnageAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FCarnageDamageDealtSignature,
	AActor*, Source,
	AActor*, Target,
	float, DamageAmount,
	const FGameplayTagContainer&, DamageTags);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCarnageHealthChangedSignature, float, NewHealthPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCarnageArmorChangedSignature,  float, NewArmorPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCarnageDiedSignature);

UCLASS()
class FPS_MOVEMENT_API UCarnageAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	UCarnageAttributeSet();

	// ── Health ──────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category="Carnage|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UCarnageAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category="Carnage|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UCarnageAttributeSet, MaxHealth)

	// ── Armor ───────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category="Carnage|Vital")
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UCarnageAttributeSet, Armor)

	UPROPERTY(BlueprintReadOnly, Category="Carnage|Vital")
	FGameplayAttributeData MaxArmor;
	ATTRIBUTE_ACCESSORS(UCarnageAttributeSet, MaxArmor)

	// ── Shield (regenerating) ───────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category="Carnage|Vital")
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UCarnageAttributeSet, Shield)

	UPROPERTY(BlueprintReadOnly, Category="Carnage|Vital")
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UCarnageAttributeSet, MaxShield)

	// ── Meta attributes (transient, consumed by PostGameplayEffectExecute) ─
	UPROPERTY(BlueprintReadOnly, Category="Carnage|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UCarnageAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category="Carnage|Meta")
	FGameplayAttributeData IncomingHealing;
	ATTRIBUTE_ACCESSORS(UCarnageAttributeSet, IncomingHealing)

	// ── Tuning ──────────────────────────────────────────────────────────
	UPROPERTY(EditDefaultsOnly, Category="Carnage|Tuning", meta=(ClampMin=0, ClampMax=1))
	float ArmorAbsorptionRatio = 0.5f;

	// ── Events ──────────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable, Category="Carnage|Events")
	FCarnageDamageDealtSignature OnDamageDealt;

	UPROPERTY(BlueprintAssignable, Category="Carnage|Events")
	FCarnageHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="Carnage|Events")
	FCarnageArmorChangedSignature OnArmorChanged;

	UPROPERTY(BlueprintAssignable, Category="Carnage|Events")
	FCarnageDiedSignature OnDied;

	// ── Overrides ───────────────────────────────────────────────────────
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:

	void BroadcastHealthChanged();
	void BroadcastArmorChanged();
};
