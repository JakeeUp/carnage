# Animation & HUD Setup Guide

The C++ is done — these are the editor-only steps (assets/AnimBP/UMG can't be edited from code).
Work top to bottom; each section is independent.

---

## 1. Reload — make it actually fire (5 min)

The reload SYSTEM is in C++ (ammo, reserve, fire lockout, auto-reload on empty). You just give it an
input key and a montage.

### a) Reload input
1. Content Browser → `Content/Input/Actions/` → right-click → **Input → Input Action** → name it **`IA_Reload`**.
2. Open it → **Value Type = Digital (bool)** → Save.
3. Open **`Content/Variant_Shooter/Input/IMC_Weapons`** → on the **Mappings** array click **+** → set the new
   row to **IA_Reload** → click the keyboard icon → press **R**. Save.
4. Open **`BP_ShooterChar`** → **Class Defaults** → search **"Reload"** → set **Reload Action = IA_Reload**.
   Compile + Save.

### b) Reload, draw, holster and slide-lock animations
Run the wiring script once (editor closed or BP_Pistol not open). It is idempotent and prints every
property it changes:

```
UnrealEditor-Cmd.exe FPS_Movement.uproject -run=pythonscript -script="<repo>/Tools/Python/wire_pistol_animations.py" -EnablePlugins=PythonScriptPlugin -unattended -nop4 -nosplash -NullRHI
```

It sets on **`BP_Pistol`**: Weapon Reload Montage, Weapon Reload Montage Empty, Draw Montage, Holster Montage
and Slide Lock Sequence (see the audit table below). **Reload Montage** (arms) is already set.

With **Animation → Reload → Sync Reload To Montage** on (default), the reload finishes when the longest reload
montage ends (2.0 s normal, 2.083 s from empty), not after **Reload Duration**, which is only the fallback when
no montage is set.

Test: fire to empty (slide locks back, auto-reloads with the dry reload) or press **R** with rounds left
(normal reload). You can't fire mid-reload or during the 0.42 s draw; ammo pulls from the reserve.
Tunables on the weapon: **Reserve Ammo** (90), **Auto Reload On Empty** (true), **Reload Duration**,
**Draw Lockout Time** (-1 = draw montage length).

---

## 2. Wall run plays the run animation (AnimBP)

The movement component already exposes everything; the AnimBP just needs to read it. Open the
**first-person arms AnimBP** (`ABP_ShooterArms`, or `Variant_Shooter/Anims/ABP_FP_Pistol`).

### a) Read the wall-run flag
1. In the AnimGraph's **Event Graph**, find **Event Blueprint Update Animation** (or the Property Access /
   Thread-Safe update).
2. Get **Try Get Pawn Owner** → **Cast to ShooterCharacter** → **Get Character Movement** →
   **Cast to DoomMovementComponent**.
3. Off that, call **Is Wall Running** (and optionally **Get Movement State**). Promote the bool to a
   variable named **`bWallRunning`**.

### b) Use it in the state machine
In your locomotion **state machine**:
- On the transition **into the falling / in-air state**, add `AND NOT bWallRunning` — this stops it from
  playing the fall/jump pose while you're on the wall.
- Add a transition into your **Jog/Run/Sprint** state with the rule **`bWallRunning`** (true).

Result: while wall running you'll see the run cycle instead of the falling pose. If your locomotion is a
blendspace driven by speed, this works automatically *once* you stop the in-air state from grabbing the
wall-run frames — because the wall run keeps your velocity high (~1000+).

---

## 3. ADS and the HUD (no editor steps)

The crosshair fade, ammo/reserve readout, and ADS pose are now handled in C++ by `UShooterHUDWidget` and
`UWeaponRecoilComponent` (see [Systems/ADS.md](Systems/ADS.md)). `ABP_ShooterArms` no longer receives an ADS
signal (`IsAimingDownSights()` returns false unless `bAnimBPDrivesADSPose` is enabled on the character), so its
ADS state machine can be deleted.

---

## 4. Clunky / not-flowing animations

Almost always blend timing. Three usual culprits:

1. **State transitions too snappy** — in the state machine, click each transition arrow → **Details → Blend
   Settings → Duration** → set ~**0.15–0.2s** (0 = instant pop, which reads as clunky).
2. **Montages cut hard** — open each montage (fire/reload) → **Blend In ≈ 0.05**, **Blend Out ≈ 0.15–0.2**.
3. **Firing/reload freezes the legs** — the montage should play on an **upper-body slot**, not override the
   whole body. In the AnimGraph, the montage **Slot** node should sit on a **Layered Blend Per Bone** that
   blends from `spine_01` (or your arms root) up, so locomotion keeps driving the lower body. For FP arms-only
   this is usually already fine.

---

## 5. Animation audit (First Person Pistol Animations pack)

Arms montages target `SK_Mannequin_Arms_Skeleton`; weapon montages target `SK_Pistol`. All pack montages use
`DefaultSlot`. `ABP_Pistol` (the gun's AnimBP) has a `DefaultSlot` node, so weapon montages and dynamic slot
animations play on it.

| Asset | Length | Used by | Where |
|---|---|---|---|
| `ABS_Movement` (AS_Pistol_Idle / AS_Pistol_Walk / AS_Pistol_Sprint by Speed) | — | arms locomotion | `ABP_ShooterArms` |
| `AS_Pistol_Jump_Start` / `_Loop` / `_Landed` | — | arms in-air states | `ABP_ShooterArms` |
| `AM_Pistol_Fire_Montage` | 0.50 s | arms fire | `BP_Pistol.FiringMontage` |
| `AM_Pistol_ADS_Fire_Montage` | — | arms fire while aimed | `BP_Pistol.FiringMontage_ADS` |
| `AM_Pistol_Reload_Montage` | 2.00 s | arms reload (normal and empty) | `BP_Pistol.ReloadMontage` |
| `AM_Pistol_Draw_Montage` | 0.42 s | arms, on equip; blocks fire/reload | `BP_Pistol.DrawMontage` (script) |
| `AM_Pistol_Holster_Montage` | 0.50 s | arms, before a weapon switch | `BP_Pistol.HolsterMontage` (script) |
| `AM_Weapon_Pistol_Fire_Montage` | 0.50 s | gun slide cycle | `BP_Pistol.WeaponFiringMontage` |
| `AM_Weapon_Pistol_Reload_Montage` | 2.00 s | gun mag swap, rounds left | `BP_Pistol.WeaponReloadMontage` (script; was empty) |
| `AM_Weapon_Pistol_Reload_Dry_Montage` | 2.08 s | gun mag swap from empty; starts slide-locked, releases slide | `BP_Pistol.WeaponReloadMontage_Empty` (script) |
| `AS_Weapon_Pistol_Fire_Dry_Montage` (AnimSequence) | 0.50 s | last round: slide goes back and is held on the final frame until the reload | `BP_Pistol.SlideLockSequence` (script), played on `DefaultSlot` |

Verified from the bone tracks: `glock_slide` sits at x = -1.05 at rest, ends `AS_Weapon_Pistol_Fire_Dry_Montage`
at x = -4.63 (locked back), and `AS_Weapon_Pistol_Reload_Dry` starts at -4.63 and ends at -1.05.

**Intentionally retired**
- `AS_Pistol_ADS_In` / `AS_Pistol_ADS_Out` / `AS_Pistol_ADS_Pose` and the ADS states in `ABP_ShooterArms`
  (still referenced by the AnimBP but never entered): ADS is procedural (`UWeaponRecoilComponent`, see
  [Systems/ADS.md](Systems/ADS.md)).
- The pack's camera shakes (`CS_*`) other than `CS_Sprint`, the demo character, input and `WBP_Ammo`.
- `ReloadMontage_Empty` (arms) is left unset: the pack has no separate arms dry reload.

**Direction (no directional animations in the pack)**
- Sprint is forward-only (`UDoomMovementComponent::SprintMaxAngle`, 50 deg): strafes and backpedals stay at
  walk speed, so the speed-driven `ABS_Movement` plays walk instead of sprint.
- Direction comes from `UWeaponRecoilComponent`: strafe roll/yaw/lean from lateral velocity, a backpedal
  pull-in, a sprint pose scaled by forward speed only, and a bob whose lateral sway moves to the step cadence
  while strafing (`Recoil|Directional` and `Recoil|Bob` tunables).
- `CS_Sprint` is a 1 s one-shot wave-oscillator shake (blend-out 1 s, single instance), not a loop, so it never
  needs stopping; it now plays only when the sprint press is within the forward cone.

---

*All C++ hooks referenced above exist in the current source. If an asset or option name differs in your project,
update this guide to match.*
