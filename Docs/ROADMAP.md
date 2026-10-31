# Roadmap

Open work for Project Carnage, grouped by status. Items are removed when they ship.

## In Progress

- Wall run plays the run animation (Animation Blueprint wiring; see [ANIM_SETUP.md](ANIM_SETUP.md))
- Combo meter HUD widget
- Crosshair hides while aiming down sights (see [ANIM_SETUP.md](ANIM_SETUP.md))
- Reload and aim down sights animation polish

## Planned

- NPC flinch and hit react animations
- Unify NPC health onto the same Ability System the player uses
- Promote slide, dash, and mantle to formal custom movement modes
- Route dash and slide through Gameplay Abilities, with GAS-driven cooldowns
- Audio pass: hit, headshot, kill, and rank-up sounds
- Additional weapons and a dedicated test arena

## Content Cleanup Candidates

These assets are believed to be unused. Confirm that no references remain before deleting, for example with the editor's Reference Viewer.

- `Content/Variant_Horror/`: template leftover. Only `Input/IA_Sprint` and `Input/IMC_Horror` remain, and they are referenced by the older `BP_FirstPersonCharacter`.
- `Content/FirstPerson/Blueprints/BP_FirstPersonCharacter`: old template character. Remove together with `Variant_Horror` once the level's game mode override points at `BP_ShooterGameMode`.
- `Content/FirstPersonPistolAnimations/Maps/LV_Demonstration`, `Blueprints/BP_DemoCharacter`, and `Demo/Input/`: demo scene from the asset pack. Keep `Demo/FirstPersonArms/`, which contains the arms mesh in use.
- `Content/Input/Touch/`: mobile touch template assets. Not used on PC.
- `Content/Weapons/GrenadeLauncher/` and `BP_ShooterWeapon_GrenadeLauncher`: keep only if the weapon is used in a level.
- Empty folders: `Content/Collections/` and `Content/Developers/`.

## Engineering Items

- Decide on Git LFS. `.gitattributes` marks `*.uasset` and `*.umap` as binary, but no LFS filter is configured, so these files are stored as regular Git blobs. Either configure LFS tracking or remove the attributes.
- Remove the `Template/` player controller, game mode, and camera manager. `AShooterPlayerController` and `AShooterGameMode` already derive from the engine base classes.
