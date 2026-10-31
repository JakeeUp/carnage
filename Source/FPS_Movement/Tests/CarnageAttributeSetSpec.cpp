// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CarnageAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "SpecTestListener.h"

BEGIN_DEFINE_SPEC(FCarnageAttributeSetSpec, "FPS_Movement.Carnage.AttributeSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	UCarnageAttributeSet* Set = nullptr;
	UAbilitySystemComponent* ASC = nullptr;
	UWorld* World = nullptr;
	USpecTestListener* Listener = nullptr;

	void ApplyDamage(float Amount)
	{
		UGameplayEffect* GE = NewObject<UGameplayEffect>(GetTransientPackage());
		GE->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Mod;
		Mod.Attribute = UCarnageAttributeSet::GetIncomingDamageAttribute();
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Amount));
		GE->Modifiers.Add(Mod);
		ASC->ApplyGameplayEffectToSelf(GE, 1.0f, ASC->MakeEffectContext());
	}
END_DEFINE_SPEC(FCarnageAttributeSetSpec)

void FCarnageAttributeSetSpec::Define()
{
	BeforeEach([this]()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		AActor* Owner = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Owner);
		Owner->SetRootComponent(Root);
		Root->RegisterComponent();
		ASC = NewObject<UAbilitySystemComponent>(Owner);
		ASC->RegisterComponent();
		Set = NewObject<UCarnageAttributeSet>(Owner);
		ASC->InitAbilityActorInfo(Owner, Owner);
		ASC->AddSpawnedAttribute(Set);
		Listener = NewObject<USpecTestListener>(GetTransientPackage());
		Set->OnDied.AddDynamic(Listener, &USpecTestListener::OnDied);
	});

	AfterEach([this]()
	{
		Set->OnDied.RemoveAll(Listener);
		Set = nullptr;
		ASC = nullptr;
		if (World)
		{
			World->DestroyWorld(false);
			World = nullptr;
		}
		Listener = nullptr;
	});

	It("clamps Health into [0, MaxHealth]", [this]()
	{
		float V = 500.0f;
		Set->PreAttributeChange(UCarnageAttributeSet::GetHealthAttribute(), V);
		TestEqual("upper", V, Set->GetMaxHealth());
		V = -20.0f;
		Set->PreAttributeChange(UCarnageAttributeSet::GetHealthAttribute(), V);
		TestEqual("lower", V, 0.0f);
	});

	It("keeps Max attributes at or above 1", [this]()
	{
		float V = 0.0f;
		Set->PreAttributeChange(UCarnageAttributeSet::GetMaxHealthAttribute(), V);
		TestEqual("MaxHealth", V, 1.0f);
		V = -5.0f;
		Set->PreAttributeChange(UCarnageAttributeSet::GetMaxShieldAttribute(), V);
		TestEqual("MaxShield", V, 1.0f);
	});

	It("non-lethal damage reduces health and does not broadcast death", [this]()
	{
		ApplyDamage(30.0f);
		TestEqual("health", Set->GetHealth(), 70.0f);
		TestEqual("died count", Listener->DiedCount, 0);
	});

	It("broadcasts OnDied exactly once across repeated lethal damage", [this]()
	{
		ApplyDamage(150.0f);
		TestEqual("health", Set->GetHealth(), 0.0f);
		TestEqual("died after first", Listener->DiedCount, 1);
		ApplyDamage(50.0f);
		ApplyDamage(50.0f);
		TestEqual("died after repeats", Listener->DiedCount, 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
