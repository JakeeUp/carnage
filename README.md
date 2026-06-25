# PROJECT CARNAGE

A first person movement shooter built in Unreal Engine 5.7 with C++.
Doom Eternal aggression, Titanfall 2 flow, and a Devil May Cry style rank system stacked on top.

> Working title. Built as a feel-first FPS sandbox where the whole point is movement and shooting that feel incredible to chain together.

## Overall Progress

```
[##########################------]  78%
```

Core gameplay systems are in and fully playable. Remaining work is mostly animation wiring and HUD polish.

## Features

### Movement
- [x] Sprint with momentum
- [x] Momentum slide (distance scales with entry speed, slope acceleration, anti spam cooldown)
- [x] Air dash with charges and recharge
- [x] Wall running with momentum preservation and wall jumps
- [x] Double jump with air control and input redirect
- [x] Mantling onto ledges
- [x] Forgiveness mechanics: coyote time, jump buffering, slide on land buffering
- [x] Bunny hop momentum chaining

### Combat
- [x] Hitscan and projectile fire modes
- [x] Spring based recoil with recovery, procedural weapon kick and sway
- [x] Reload system (reserve ammo, fire lockout, auto reload on empty)
- [x] Headshots, hitmarkers, kill confirmation, kill hitstop
- [x] Instant aim down sights and weapon wall avoidance (anti clip)

### Style and Juggle
- [x] Carnage Meter: a D to SSS rank that rewards varied, aggressive play
- [x] Rank buffs: higher rank grants more move speed and more damage
- [x] Juggle combat: slide to launch enemies, keep them airborne, slam them down, bounce them back up

### In Progress
- [ ] Wall run plays the run animation (Animation Blueprint wiring)
- [ ] Combo meter HUD widget
- [ ] Crosshair hides while aiming down sights
- [ ] Reload and aim down sights animation polish

### Planned
- [ ] NPC flinch and hit react animations
- [ ] Unify NPC health onto the same Ability System the player uses
- [ ] Promote slide, dash, and mantle to formal custom movement modes
- [ ] Audio pass (hit, headshot, kill, and rank up sounds)
- [ ] Additional weapons and a dedicated test arena

## Controls

Default bindings (set in the Enhanced Input mapping contexts and remappable in the editor):

| Action | Key |
| :--- | :--- |
| Move | WASD |
| Look | Mouse |
| Jump and Double Jump | Space |
| Sprint | Shift (hold) |
| Slide | Left Ctrl |
| Dash | Q |
| Fire | Left Mouse |
| Aim Down Sights | Right Mouse (hold) |
| Reload | R |
| Swap Weapon | Mouse Wheel |

## Built With

- Unreal Engine 5.7 and C++
- Enhanced Input
- Gameplay Ability System (player health, armor, and damage)
- StateTree (enemy AI)
- Niagara (muzzle flash and tracers)

## Project Structure

Source code lives under `Source/FPS_Movement/`:

| Folder | Purpose |
| :--- | :--- |
| `Template/` | Unreal first person template base classes |
| `Core/` | The game itself: player character, weapons, AI, UI, game mode |
| `Movement/` | Custom character movement and the momentum system |
| `Camera/` | Movement driven camera effects |
| `Weapons/` | Recoil, sway, and weapon feel |
| `Carnage/` | Player vitals through the Gameplay Ability System |

See `ARCHITECTURE.md` for a full class by class breakdown and how the systems connect.

## Building

1. Clone the repository.
2. Right click `FPS_Movement.uproject` and choose Generate Visual Studio project files.
3. Open the generated solution and build in Development Editor, Win64. You can also open the uproject directly and let the editor compile.
4. Requires Unreal Engine 5.7.

## Notes

This project contains assets from the Unreal first person template and marketplace packs. It is kept private in line with those asset licenses.
