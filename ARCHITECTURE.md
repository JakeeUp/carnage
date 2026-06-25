# FPS_Movement — Project Map

Doom Eternal / Titanfall 2 style movement shooter. UE 5.7, C++ first with Blueprint configuration on top.
Last updated: 2026-06-11 (post-cleanup: Horror variant removed, source reorganized).

---

## Source layout (`Source/FPS_Movement/`)

```
FPS_Movement.h/.cpp   Module boilerplate (lives at the module root, next to Build.cs)
Template/             UE5 first-person template DEFAULTS — base Character/Controller/GameMode/CameraManager. Rarely touched; this is the "what is this?" stuff isolated out of the way.
Movement/             CUSTOM: the Doom/Titanfall movement brain + momentum combo
Camera/               CUSTOM: movement-driven camera juice
Weapons/              CUSTOM: recoil/sway feel system + shared recoil types
Carnage/              CUSTOM: GAS layer — health, armor, damage tags (player only)
Variant_Shooter/      The actual game: character, weapons, AI, UI, game mode (template variant, heavily extended)
```

Quick orientation: **`Template/` = UE5 defaults** you can mostly ignore. **`Movement/ Camera/ Weapons/ Carnage/` = the custom systems built for this project.** **`Variant_Shooter/` = the game itself.**

### (module root) + Template/
| File | What it is | Used by |
|---|---|---|
| `FPS_Movement.h/.cpp` (root) | Module boilerplate + `LogFPS_Movement` log category | everything |
| `Template/FPS_MovementCharacter` | UE5-template base first-person character: camera, FP arms mesh, move/look/jump input plumbing | parent of `ShooterCharacter` and `ShooterNPC` |
| `Template/FPS_MovementPlayerController` | UE5-template base PC: loads input mapping contexts, touch controls | parent of `ShooterPlayerController` |
| `Template/FPS_MovementGameMode` | UE5-template empty base game mode | parent of `ShooterGameMode` |
| `Template/FPS_MovementCameraManager` | UE5-template camera manager (pitch limits) | set by `FPS_MovementPlayerController` |

### Movement/
| File | What it is | Used by |
|---|---|---|
| `DoomMovementComponent` | Custom CharacterMovementComponent: sprint, momentum slide (+sprint-slide boost), dash w/ charges, bunny hop, wall run (momentum-preserving, custom movement mode), mantle, double jump w/ air redirect, coyote time, jump buffer. Broadcasts `OnMovementAction` on each traversal; accepts a generic `SetExternalSpeedScale` so external systems can boost speed without it knowing they exist. Profiled via `STAT_DoomMovementTick` / `STAT_DoomWallRun` | swapped in by `ShooterCharacter`'s constructor (`SetDefaultSubobjectClass`) |
| `Variant_Shooter/StyleMeterComponent` | DMC-style **"Carnage Meter"**. Consumes `OnMovementAction` + `OnHitConfirmed` + `OnDamaged`. Style points with a **variety multiplier** (repetition falloff + freshness bonus — mixing moves pays, spamming one doesn't), D→SSS ranks, time decay (faster at higher ranks), and a damage-taken crash. When `bAffectsGameplay`, rank feeds a speed bonus (`SetExternalSpeedScale`) + an outgoing-damage bonus (read through `IShooterWeaponHolder::GetOutgoingDamageMultiplier` at the `ShooterDamage::ApplyHit` chokepoint). `OnRankChanged`/`OnMeterChanged` delegates for UMG. *(Superseded the earlier MomentumComponent.)* | component on `ShooterCharacter` |

### Camera/
| File | What it is | Used by |
|---|---|---|
| `DoomCameraEffectsComponent` | FOV state machine (sprint/slide/dash/ADS), strafe & wall-run tilt, head bob, landing dip, slide camera dip | component on `ShooterCharacter`; reads ADS via `AimStateProvider` delegate (no dependency on the character class) |
| `DoomCameraModifier` | Applies the FOV/tilt/pitch values to the camera each frame | created at runtime by `DoomCameraEffectsComponent` |

### Weapons/
| File | What it is | Used by |
|---|---|---|
| `WeaponRecoilComponent` | Aim-recoil spring w/ recovery + player-compensation tracking, procedural FP-mesh kickback, look/movement sway. Writes control rotation directly (immune to the legacy input-scale inversion) | component on `ShooterCharacter`; fed by `AShooterWeapon` via `AddWeaponRecoil` |
| `WeaponRecoilTypes.h` | `FWeaponRecoilParams` struct (per-shot recoil profile, editable per weapon BP) | `AShooterWeapon`, `IShooterWeaponHolder`, `WeaponRecoilComponent` |

### Carnage/ (GAS — player health/armor only; NPCs use a plain HP float)
| File | What it is | Used by |
|---|---|---|
| `CarnageAbilitySystemComponent` | ASC subclass + `ApplyDamageToTarget/Self` helpers | component on `ShooterCharacter` |
| `CarnageAttributeSet` | Health/MaxHealth, Armor (absorbs 50%), Shield (reserved), meta IncomingDamage/Healing; fires OnHealthChanged/OnDied | `ShooterCharacter` HUD + death flow |
| `CarnageGameplayTags` | Native tags: `State.*` (synced from movement every tick), `Damage.*`, `SetByCaller.*`, `Ability.*` (reserved) | `ShooterCharacter`, `GE_InstantDamage` |
| `GE_InstantDamage` | SetByCaller damage GameplayEffect — the bridge from `TakeDamage` into GAS | `ShooterCharacter::TakeDamage` |

### Variant_Shooter/
| File | What it is | Used by |
|---|---|---|
| `ShooterCharacter` | THE player class: weapon inventory, GAS wiring, hit feedback (hitmarker delegate, sounds, kill hitstop), double-jump config, coyote refund | `BP_ShooterChar` (your character), `BP_ShooterCharacter` (template variant one) |
| `ShooterGameMode` | Team scores | `BP_ShooterGameMode` |
| `ShooterPlayerController` | Adds `IMC_Weapons` etc., spawns bullet-counter HUD, respawn | `BP_ShooterPlayerController` |
| `Weapons/ShooterWeapon` | Weapon actor: projectile **or hitscan** fire modes, refire/full-auto, shot buffering, spread (true degrees), muzzle/tracer/sound FX slots, recoil profile | `BP_Pistol` (yours), `BP_ShooterWeapon_*` (template) |
| `Weapons/ShooterProjectile` | Projectile: impact/explosion damage through the shared hit pipeline, headshot support | `BP_ShooterProjectile_Bullet`, `_Grenade` |
| `Weapons/ShooterWeaponHolder` | Interface between weapons and whoever holds them | `ShooterCharacter`, `ShooterNPC` |
| `Weapons/ShooterDamageUtils` | **The one damage path**: headshot resolution (bone or capsule height), kill detection (`Dead` actor tag), physics impulse, instigator hit-feedback dispatch | `ShooterProjectile`, `ShooterWeapon` (hitscan) |
| `Weapons/ShooterHitFeedbackInterface` | `IShooterHitFeedbackReceiver` — "you hit something" callback | implemented by `ShooterCharacter` |
| `Weapons/ShooterPickup` | Weapon pickup actor | `BP_ShooterPickup` on `Lvl_Shooter` |
| `AI/ShooterNPC` | Enemy character (plain `CurrentHP`, ragdoll death, grants `Dead` tag) | `BP_ShooterNPC` |
| `AI/ShooterAIController`, `ShooterStateTreeUtility`, `EnvQueryContext_Target`, `ShooterNPCSpawner` | StateTree-driven AI + spawner | `BP_ShooterAIController`, `ST_Shooter`, `BP_ShooterNPCSpawner` |
| `UI/ShooterUI`, `UI/ShooterBulletCounterUI` | HUD widget bases | `UI_Shooter`, `UI_ShooterBulletCounter` |

---

## The gameplay flow (who calls whom)

```
Input (IMC_Weapons / IMC_Default, loaded by the PlayerController)
  → ShooterCharacter (DoStartFiring / DoJumpStart / DoSlide / DoDash ...)
      → DoomMovementComponent  (movement verbs, coyote/buffer)
      → CurrentWeapon->StartFiring()
          → FireShot(): projectile spawn OR hitscan trace
              → ShooterDamage::ApplyHit()
                  → ApplyDamage → NPC HP  /  player GAS (armor → health)
                  → headshot + kill detection
                  → IShooterHitFeedbackReceiver on the shooter
                      → OnHitConfirmed (BP/UI) + hit sounds + kill hitstop
          → AddWeaponRecoil → WeaponRecoilComponent (aim spring + mesh kick)
      → DoomCameraEffectsComponent reads movement state → FOV/tilt/bob
```

---

## Content map (what's live)

| Content folder | Status | Notes |
|---|---|---|
| `FirstPerson/` | **LIVE** | `BP_ShooterChar` (your character), `BP_Pistol`, `ABP_ShooterArms`, `Lvl_FirstPerson` (startup map) |
| `Input/` | **LIVE** | Movement actions + `IMC_Default`/`IMC_MouseLook` |
| `Variant_Shooter/` | **LIVE** | Shooter game mode/controller/HUD, NPCs, weapons, pickups, `IA_Shoot`, `IMC_Weapons`, `Lvl_Shooter` |
| `FirstPersonPistolAnimations/` | **LIVE — careful** | Your FP arms mesh + skeleton live in its `Demo/FirstPersonArms/` subfolder; fire/ADS montages in `Animations/`; camera shakes in `Misc/` |
| `Characters/Mannequins` | **LIVE** | Third-person NPC mannequin + anims |
| `Weapons/` (Pistol/Rifle/GrenadeLauncher meshes) | **LIVE** | Used by the Variant_Shooter weapon BPs |
| `Variant_Horror/` | **Removed** (backup) | Only `Input/IA_Sprint` + `IMC_Horror` kept — referenced by the old `BP_FirstPersonCharacter` |

### Safe-to-delete candidates (do these IN-EDITOR via right-click → Delete, which checks references)
- `FirstPersonPistolAnimations/Maps/LV_Demonstration` + `Blueprints/BP_DemoCharacter` + `Demo/Input/` — the pack's demo scene. Do **not** delete `Demo/FirstPersonArms/` (your arms live there).
- `FirstPerson/Blueprints/BP_FirstPersonCharacter` — the old template character (the thing keeping `Variant_Horror/Input` alive). Delete both together once you confirm `Lvl_FirstPerson`'s game mode override is set to `BP_ShooterGameMode`.
- `Weapons/GrenadeLauncher/` + `BP_ShooterWeapon_GrenadeLauncher` + grenade projectile — only if you don't want the weapon; it may be placed as a pickup in `Lvl_Shooter`.

---

## Backup

Everything removed lives in `_Removed_Backup/` (same folder structure). The editor doesn't scan it.
Restore = move it back. Once you've played for a while and miss nothing, delete the folder.
