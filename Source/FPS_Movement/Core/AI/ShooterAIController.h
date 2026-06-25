// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShooterAIController.generated.h"

class UStateTreeAIComponent;
class UAIPerceptionComponent;
struct FAIStimulus;

DECLARE_DELEGATE_TwoParams(FShooterPerceptionUpdatedDelegate, AActor*, const FAIStimulus&);
DECLARE_DELEGATE_OneParam(FShooterPerceptionForgottenDelegate, AActor*);

UCLASS(abstract)
class FPS_MOVEMENT_API AShooterAIController : public AAIController
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStateTreeAIComponent* StateTreeAI;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UAIPerceptionComponent* AIPerception;

protected:

	UPROPERTY(EditAnywhere, Category="Shooter")
	FName TeamTag = FName("Enemy");

	TObjectPtr<AActor> TargetEnemy;

public:

	FShooterPerceptionUpdatedDelegate OnShooterPerceptionUpdated;

	FShooterPerceptionForgottenDelegate OnShooterPerceptionForgotten;

public:

	AShooterAIController();

protected:

	virtual void OnPossess(APawn* InPawn) override;

protected:

	UFUNCTION()
	void OnPawnDeath();

public:

	void SetCurrentTarget(AActor* Target);

	void ClearCurrentTarget();

	AActor* GetCurrentTarget() const { return TargetEnemy; };

protected:

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);
};
