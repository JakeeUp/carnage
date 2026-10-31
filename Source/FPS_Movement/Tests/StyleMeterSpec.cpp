// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "StyleMeterComponent.h"
#include "SpecTestListener.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

BEGIN_DEFINE_SPEC(FStyleMeterSpec, "FPS_Movement.StyleMeter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	UStyleMeterComponent* Meter = nullptr;
	UWorld* World = nullptr;
	USpecTestListener* Listener = nullptr;

	void Hit(bool bKilled, bool bHeadshot)
	{
		struct { AActor* Target; float Damage; bool bKilled; bool bHeadshot; } Params{ nullptr, 10.0f, bKilled, bHeadshot };
		UFunction* Fn = Meter->FindFunction(TEXT("HandleHitConfirmed"));
		check(Fn);
		Meter->ProcessEvent(Fn, &Params);
	}

	void Damaged(float LifePercent)
	{
		struct { float Life; } Params{ LifePercent };
		UFunction* Fn = Meter->FindFunction(TEXT("HandleDamaged"));
		check(Fn);
		Meter->ProcessEvent(Fn, &Params);
	}

	void Tick(float Dt)
	{
		static_cast<UActorComponent*>(Meter)->TickComponent(Dt, LEVELTICK_All, nullptr);
	}
END_DEFINE_SPEC(FStyleMeterSpec)

void FStyleMeterSpec::Define()
{
	BeforeEach([this]()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		AActor* Owner = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Owner);
		Owner->SetRootComponent(Root);
		Root->RegisterComponent();
		Meter = NewObject<UStyleMeterComponent>(Owner);
		Meter->RegisterComponent();
		Listener = NewObject<USpecTestListener>(GetTransientPackage());
		Meter->OnRankChanged.AddDynamic(Listener, &USpecTestListener::OnRankChanged);
	});

	AfterEach([this]()
	{
		Meter->OnRankChanged.RemoveAll(Listener);
		Meter = nullptr;
		Listener = nullptr;
		if (World)
		{
			World->DestroyWorld(false);
			World = nullptr;
		}
	});

	It("starts at rank D with zero points", [this]()
	{
		TestEqual("rank", Meter->GetRank(), EStyleRank::D);
		TestEqual("points", Meter->GetStylePoints(), 0.0f);
	});

	It("promotes to rank C on a fresh kill (54 points)", [this]()
	{
		Hit(true, false);
		TestEqual("points", Meter->GetStylePoints(), 54.0f);
		TestEqual("rank", Meter->GetRank(), EStyleRank::C);
		TestEqual("rank change broadcast count", Listener->RankChangedCount, 1);
		TestTrue("progress in [0,1]", Meter->GetProgressInRank() >= 0.0f && Meter->GetProgressInRank() <= 1.0f);
	});

	It("applies repetition falloff to repeated actions", [this]()
	{
		Hit(true, false);
		Hit(true, false);
		// 54 + 36 * 0.55
		TestEqual("points", Meter->GetStylePoints(), 54.0f + 36.0f * 0.55f, 0.01f);
	});

	It("drops points to the retain fraction when life falls", [this]()
	{
		Hit(true, false);
		Damaged(0.5f);
		TestEqual("points", Meter->GetStylePoints(), 54.0f * 0.4f, 0.01f);
		TestEqual("rank", Meter->GetRank(), EStyleRank::D);
	});

	It("does not punish healing", [this]()
	{
		Hit(true, false);
		Damaged(1.0f);
		TestEqual("points", Meter->GetStylePoints(), 54.0f);
	});

	It("does not decay inside the grace period", [this]()
	{
		Hit(true, false);
		Tick(0.5f);
		TestEqual("points", Meter->GetStylePoints(), 54.0f);
	});

	It("decays after the grace period and never goes negative", [this]()
	{
		Hit(true, false);
		Tick(1.0f);
		TestTrue("points decreased", Meter->GetStylePoints() < 54.0f);
		for (int32 i = 0; i < 100; ++i)
		{
			Tick(1.0f);
		}
		TestEqual("floored at zero", Meter->GetStylePoints(), 0.0f);
		TestEqual("rank back to D", Meter->GetRank(), EStyleRank::D);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
