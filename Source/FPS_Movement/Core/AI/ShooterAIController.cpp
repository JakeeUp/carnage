// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/AI/ShooterAIController.h"
#include "ShooterNPC.h"
#include "FPS_Movement.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"

AShooterAIController::AShooterAIController()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	StateTreeAI->SetStartLogicAutomatically(false);

	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
}

void AShooterAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	AIPerception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AShooterAIController::OnPerceptionUpdated);
	AIPerception->OnTargetPerceptionForgotten.AddUniqueDynamic(this, &AShooterAIController::OnPerceptionForgotten);
}

void AShooterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (AShooterNPC* NPC = Cast<AShooterNPC>(InPawn))
	{
		NPC->Tags.Add(TeamTag);

		NPC->OnPawnDeath.AddUniqueDynamic(this, &AShooterAIController::OnPawnDeath);

		StateTreeAI->StartLogic();
	}
}

void AShooterAIController::OnPawnDeath()
{
	// Defer teardown so the NPC finishes its Die() (ragdoll setup) first.
	GetWorldTimerManager().SetTimerForNextTick(this, &AShooterAIController::TeardownAfterDeath);
}

void AShooterAIController::TeardownAfterDeath()
{
	if (AShooterNPC* NPC = Cast<AShooterNPC>(GetPawn()))
	{
		NPC->OnPawnDeath.RemoveDynamic(this, &AShooterAIController::OnPawnDeath);
	}

	if (UPathFollowingComponent* PathComp = GetPathFollowingComponent())
	{
		PathComp->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
	}

	StateTreeAI->StopLogic(FString(""));

	UnPossess();

	Destroy();
}

void AShooterAIController::SetCurrentTarget(AActor* Target)
{
	TargetEnemy = Target;
}

void AShooterAIController::ClearCurrentTarget()
{
	TargetEnemy = nullptr;
}

void AShooterAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (CarnageAILogEnabled())
	{
		UE_LOG(LogFPS_Movement, Display, TEXT("[AILog] %s perceived %s sensed=%d bound=%d"),
			*GetNameSafe(GetPawn()), *GetNameSafe(Actor), Stimulus.WasSuccessfullySensed() ? 1 : 0, OnShooterPerceptionUpdated.IsBound() ? 1 : 0);
	}
	OnShooterPerceptionUpdated.ExecuteIfBound(Actor, Stimulus);
}

void AShooterAIController::OnPerceptionForgotten(AActor* Actor)
{
	OnShooterPerceptionForgotten.ExecuteIfBound(Actor);
}
