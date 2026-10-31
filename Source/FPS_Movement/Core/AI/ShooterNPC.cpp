// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/AI/ShooterNPC.h"
#include "Core/AI/JuggleComponent.h"
#include "ShooterWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterGameMode.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

AShooterNPC::AShooterNPC()
{
	JuggleComp = CreateDefaultSubobject<UJuggleComponent>(TEXT("Juggle"));
}

void AShooterNPC::BeginPlay()
{
	Super::BeginPlay();

	if (JuggleComp)
	{
		JuggleComp->OnJuggleStateChanged.AddDynamic(this, &AShooterNPC::HandleJuggleStateChanged);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Weapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
}

void AShooterNPC::HandleJuggleStateChanged(bool bJuggled)
{
	if (bJuggled)
	{
		StopShooting();
	}
}

void AShooterNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(DeathTimer);
}

float AShooterNPC::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.0f;
	}

	CurrentHP -= Damage;

	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	return Damage;
}

void AShooterNPC::AttachWeaponMeshes(AShooterWeapon* WeaponToAttach)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	WeaponToAttach->AttachToActor(this, AttachmentRule);

	WeaponToAttach->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	WeaponToAttach->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonWeaponSocket);
}

void AShooterNPC::PlayFiringMontage(UAnimMontage* Montage)
{
}

void AShooterNPC::AddWeaponRecoil(const FWeaponRecoilParams& Recoil)
{
}

void AShooterNPC::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
}

FVector AShooterNPC::GetWeaponTargetLocation()
{
	const FVector AimSource = GetFirstPersonCameraComponent()->GetComponentLocation();

	FVector AimDir, AimTarget = FVector::ZeroVector;

	if (const AActor* AimActor = CurrentAimTarget.Get())
	{
		AimTarget = AimActor->GetActorLocation();

		AimTarget.Z += FMath::RandRange(MinAimOffsetZ, MaxAimOffsetZ);

		AimDir = (AimTarget - AimSource).GetSafeNormal();
		AimDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDir, AimVarianceHalfAngle);

	} else {

		AimDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(GetFirstPersonCameraComponent()->GetForwardVector(), AimVarianceHalfAngle);

	}

	AimTarget = AimSource + (AimDir * AimRange);

	FHitResult OutHit;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, AimSource, AimTarget, ECC_Visibility, QueryParams);

	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterNPC::AddWeaponClass(const TSubclassOf<AShooterWeapon>& InWeaponClass)
{
}

void AShooterNPC::OnWeaponActivated(AShooterWeapon* InWeapon)
{
}

void AShooterNPC::OnWeaponDeactivated(AShooterWeapon* InWeapon)
{
}

void AShooterNPC::OnSemiWeaponRefire()
{
	if (bIsShooting)
	{
		Weapon->StartFiring();
	}
}

void AShooterNPC::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	Tags.Add(DeathTag);

	OnPawnDeath.Broadcast();

	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->IncrementTeamScore(TeamByte);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->StopActiveMovement();

	GetMesh()->SetCollisionProfileName(RagdollCollisionProfile);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetPhysicsBlendWeight(1.0f);

	GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &AShooterNPC::DeferredDestruction, DeferredDestructionTime, false);
}

void AShooterNPC::DeferredDestruction()
{
	Destroy();
}

void AShooterNPC::StartShooting(AActor* ActorToShoot)
{
	if (JuggleComp && JuggleComp->IsJuggled())
	{
		return;
	}

	CurrentAimTarget = ActorToShoot;

	bIsShooting = true;

	Weapon->StartFiring();
}

void AShooterNPC::StopShooting()
{
	bIsShooting = false;

	Weapon->StopFiring();
}
