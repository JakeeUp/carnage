# Gameplay Ability System (GAS) — Project Carnage

Phase 1 foundation. Phase 2+ adds damage exec calcs, abilities (dash/melee), and direct GE application from weapons.

## Architecture

```
AShooterCharacter
├── UCarnageAbilitySystemComponent  (subobject "AbilitySystem")
└── UCarnageAttributeSet            (subobject "AttributeSet")
        ├── Health / MaxHealth
        ├── Armor  / MaxArmor       (absorbs ArmorAbsorptionRatio of incoming)
        ├── Shield / MaxShield      (full absorption, regenerates — reserved)
        └── IncomingDamage          (meta-attribute — drained in PostGameplayEffectExecute)
```

The character implements `IAbilitySystemInterface` so any GAS query (`UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)`) returns the ASC.

## Damage Flow

1. **Source** (weapon, projectile, environment) calls either:
   - `UCarnageAbilitySystemComponent::ApplyDamageToTarget(Target, DamageGE, Amount, Tags)`, or
   - the legacy `Actor->TakeAnyDamage(...)` path (still works — bridged in `AShooterCharacter::TakeDamage`)

2. **GE applied to target**. The default damage GE is `UGE_InstantDamage`:
   - `DurationPolicy = Instant`
   - One modifier: `IncomingDamage += SetByCaller(Carnage.SetByCaller.Damage)`

3. **AttributeSet::PostGameplayEffectExecute** drains `IncomingDamage`:
   - **Shield first** — full absorption.
   - **Armor next** — absorbs `ArmorAbsorptionRatio` (default 0.5) of remainder until depleted.
   - **Health last** — takes what's left.

4. **Broadcasts** (Phase 1 — single multicast for now; Phase 2 will split into damage-dealt / hit-confirmed / kill events on a `UCarnageDamageSubsystem`):
   - `OnDamageDealt(Source, Target, Amount, Tags)`
   - `OnHealthChanged(Percent)` → bridged to legacy `OnDamaged` UI delegate.
   - `OnDied()` → bridged to `Die()`.

## Native Gameplay Tags

Defined in `CarnageGameplayTags.h/.cpp`. Compile-time constants — no string lookups.

| Tag root | Purpose |
|---|---|
| `Carnage.State.*` | Movement/character states (Sprinting, Sliding, Dashing, WallRunning, Mantling, InAir, Dead, Stunned, Parrying) |
| `Carnage.Damage.*` | Damage classification (Physical, Headshot, Critical, Melee, Explosion, Falling) |
| `Carnage.SetByCaller.*` | GE magnitude IDs (Damage, Heal, HeadshotMult, CritMult, SurfaceMult) |
| `Carnage.Ability.*` | Ability identifiers (Dash, Slide, DoubleJump, Melee.Light, Melee.Heavy, Parry) |
| `Carnage.Event.*` | Gameplay events (DamageDealt, KillConfirmed, Headshot, Parried) |
| `Carnage.Cooldown.*` | Applied while ability is on cooldown |

**State tags are synced from `UDoomMovementComponent` in `AShooterCharacter::Tick`** using `AddLooseGameplayTag` / `RemoveLooseGameplayTag`. Loose tags bypass GE replication overhead — movement remains the source of truth.

## Tuning Knobs

All on `UCarnageAttributeSet` defaults or `AShooterCharacter` Blueprint:

| Property | Lives on | Notes |
|---|---|---|
| `MaxHP` / `StartingArmor` / `MaxArmor` | `AShooterCharacter` | Seeds the attribute set in `BeginPlay`. |
| `ArmorAbsorptionRatio` | `UCarnageAttributeSet` defaults | 0..1 fraction of damage armor absorbs before health. |
| `DefaultDamageEffect` | `AShooterCharacter` | The GE class used by the legacy `TakeDamage` bridge. Replace with a tuned BP subclass to layer in falloff curves, surface multipliers, etc. |

## Extension Points

- **Custom damage GE per weapon**: subclass `UGE_InstantDamage` in BP, add modifiers/conditions, plug into the weapon's `UWeaponDefinition` data asset (Phase 3).
- **Damage exec calc** (Phase 2): replace the SetByCaller-only flow with `UCarnageDamageExecCalc` that captures source/target attributes and rolls headshot/crit multipliers.
- **Abilities** (Phase 2-4): subclass `UGameplayAbility`, grant via `GiveAbility` in `BeginPlay`, bind to input via `AbilitySystem->BindAbilityActivationToInputComponent`.
- **Subsystem broadcaster** (Phase 5): introduce `UCarnageDamageSubsystem : UWorldSubsystem` that aggregates `OnDamageDealt` events globally — damage numbers / kill-cam / scoring subscribers register once rather than per-character.

## Tick Cost

Per-character per frame: 6 tag membership checks + at most 6 tag add/remove ops. Negligible at hundreds of actors. Move to event-driven if the profiler ever flags it.

## What Phase 1 does NOT do yet

- No abilities granted (dash/slide/melee still run on the movement component directly — Phase 2 reroutes through `UGameplayAbility`).
- No damage execution calculation — headshot/crit/falloff multipliers handled per-weapon for now.
- No GAS-driven cooldowns — `UDoomMovementComponent::DashCooldown` is still authoritative for dash.
- No animation montage replication via `UAbilityTask_PlayMontageAndWait` — Phase 4.
