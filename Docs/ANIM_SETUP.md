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

### b) Reload montage
1. Open **`BP_Pistol`** (`FirstPerson/Blueprints/Weapons/`) → **Class Defaults**.
2. **Animation → Reload Montage** → pick a montage built for the **first-person arms** skeleton — e.g.
   **`AM_Pistol_Reload_Montage`** (`FirstPersonPistolAnimations/Animations/`).
3. *(Optional)* **Weapon Reload Montage** → `AM_Weapon_Pistol_Reload_Montage` (plays the mag-swap on the
   gun mesh itself).
4. **Ammo → Reload Duration** → set to roughly the montage length (≈1.5–2.0s) so ammo refills when the
   animation finishes. Compile + Save.

Test: fire to empty (auto-reloads) or press **R**. You can't fire mid-reload; ammo pulls from the reserve.
Tunables on the weapon: **Reserve Ammo** (90), **Auto Reload On Empty** (true), **Reload Duration**.

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

*All C++ hooks referenced above exist in the current source. If an asset or option name differs in your project,
update this guide to match.*
