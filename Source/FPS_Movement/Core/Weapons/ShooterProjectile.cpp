// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterProjectile.h"
#include "ShooterDamageUtils.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CarnageWeaponFXSubsystem.h"

AShooterProjectile::AShooterProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Component"));

	CollisionComponent->SetSphereRadius(16.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile Movement"));

	ProjectileMovement->InitialSpeed = 3000.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	ProjectileMovement->bShouldBounce = true;

	HitDamageType = UDamageType::StaticClass();
}

void AShooterProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);
}

void AShooterProjectile::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(DestructionTimer);
}

void AShooterProjectile::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHit)
	{
		return;
	}

	bHit = true;

	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MakeNoise(NoiseLoudness, GetInstigator(), GetActorLocation(), NoiseRange, NoiseTag);

	if (bExplodeOnHit)
	{

		ExplosionCheck(GetActorLocation());

	} else {

		ProcessHit(Other, OtherComp, Hit.ImpactPoint, -Hit.ImpactNormal, Hit.BoneName, bAllowHeadshots);

	}

	if (UCarnageWeaponFXSubsystem* FX = UCarnageWeaponFXSubsystem::Get(this))
	{
		FX->StopProjectileTrail(this);

		if (bSpawnImpactFX && !bExplodeOnHit)
		{
			FCarnageImpactFXParams Params;
			Params.Location = Hit.ImpactPoint;
			Params.Normal = Hit.ImpactNormal;
			Params.ShotDirection = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal(UE_SMALL_NUMBER, -Hit.ImpactNormal);
			Params.bHitPawn = Cast<APawn>(Other) != nullptr;
			Params.HitComponent = OtherComp;
			FX->PlayImpact(Params);
		}
	}

	BP_OnProjectileHit(Hit);

	if (DeferredDestructionTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(DestructionTimer, this, &AShooterProjectile::OnDeferredDestruction, DeferredDestructionTime, false);

	} else {

		Destroy();
	}
}

void AShooterProjectile::ExplosionCheck(const FVector& ExplosionCenter)
{
	TArray<FOverlapResult> Overlaps;

	FCollisionShape OverlapShape;
	OverlapShape.SetSphere(ExplosionRadius);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (!bDamageOwner)
	{
		QueryParams.AddIgnoredActor(GetInstigator());
	}

	GetWorld()->OverlapMultiByObjectType(Overlaps, ExplosionCenter, FQuat::Identity, ObjectParams, OverlapShape, QueryParams);

	TArray<AActor*> DamagedActors;

	for (const FOverlapResult& CurrentOverlap : Overlaps)
	{
		if (DamagedActors.Find(CurrentOverlap.GetActor()) == INDEX_NONE)
		{
			DamagedActors.Add(CurrentOverlap.GetActor());

			const FVector& ExplosionDir = CurrentOverlap.GetActor()->GetActorLocation() - GetActorLocation();

			ProcessHit(CurrentOverlap.GetActor(), CurrentOverlap.GetComponent(), GetActorLocation(), ExplosionDir.GetSafeNormal(), NAME_None, false);
		}

	}
}

void AShooterProjectile::ProcessHit(AActor* HitActor, UPrimitiveComponent* HitComp, const FVector& HitLocation, const FVector& HitDirection, FName HitBoneName, bool bCanHeadshot)
{
	if (HitActor == GetOwner() && !bDamageOwner)
	{
		HitActor = nullptr;
	}

	APawn* InstigatorPawn = GetInstigator();

	FShooterHitInfo HitInfo;
	HitInfo.Target = HitActor;
	HitInfo.HitComponent = HitComp;
	HitInfo.HitLocation = HitLocation;
	HitInfo.HitDirection = HitDirection;
	HitInfo.BoneName = HitBoneName;
	HitInfo.BaseDamage = HitDamage;
	HitInfo.PhysicsImpulse = PhysicsForce;
	HitInfo.KnockbackForce = KnockbackForce;
	HitInfo.InstigatorController = InstigatorPawn ? InstigatorPawn->GetController() : nullptr;
	HitInfo.InstigatorPawn = InstigatorPawn;
	HitInfo.DamageCauser = this;
	HitInfo.DamageType = HitDamageType;
	HitInfo.bAllowHeadshot = bCanHeadshot;
	HitInfo.HeadshotMultiplier = HeadshotMultiplier;
	HitInfo.HeadshotHeightFraction = HeadshotHeightFraction;

	ShooterDamage::ApplyHit(HitInfo);
}

void AShooterProjectile::OnDeferredDestruction()
{
	Destroy();
}
