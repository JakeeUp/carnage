# Hit reactions

NPC hit reactions are procedural. They use no animation assets, because the paid animation
packs are not in the repository. Everything lives in `AShooterNPC` (`Core/AI/ShooterNPC.*`).
The tunables are under the **Hit Reaction** category.

## Data flow

1. `AShooterWeapon::FireHitscan` traces on `ECC_Weapon`. When the NPC mesh has a physics asset,
   the capsule ignores that channel, so the trace hits a physics body and gets a bone name.
2. `ShooterDamage::ApplyHit` resolves the headshot. It then calls `TakeDamage` with an
   `FShooterPointDamageEvent` that carries the bone, impact point, shot direction, headshot flag
   and weapon impulse.
3. `AShooterNPC::TakeDamage` copies those values into `LastHit`, a struct of plain values with
   no object pointers. It then either plays a hit reaction or calls `Die()`.

## Non-lethal hits

- **Physics flinch.** Hits on the upper body blend everything below `FlinchRootBone`
  (`spine_01`) into simulation. Leg hits blend only that leg. The blend weight starts at
  `FlinchBlendWeight` (0.45) and decays to 0 over `FlinchDuration` (0.25 s). The hit body gets a
  velocity kick along the shot: `damage x FlinchSpeedPerDamage`, capped at `MaxFlinchSpeed`, and
  multiplied by `HeadshotFlinchMultiplier` on headshots. Afterwards the bodies go back to
  kinematic. A chain root always has a parent body, so the component's root body (pelvis) never
  simulates.
- **Physics bodies.** At BeginPlay the NPC mesh switches to `QueryAndPhysics`, because
  query-only bodies can't simulate. CharacterMesh still ignores the Pawn channel, so the bodies
  never touch capsules.
- **Fallback.** Without a physics asset, the mesh tilts and nudges away from the shot, with a
  damped oscillation that lasts `FlinchDuration`.
- **Stagger.** On the ground, `MaxWalkSpeed` is multiplied by `StaggerSpeedScale` for
  `StaggerDuration` (0.3 s) and then restored.
- **Juggle.** If the NPC is airborne or juggled, the flinch is scaled by `AirborneFlinchScale`
  and there is no stagger. The flinch only moves mesh bodies, so it never fights
  `LaunchCharacter`.
- The reaction runs on a 60 Hz timer while it is active. The NPC does not tick.

## Death

`Die()` cancels any running reaction and stagger, then turns the mesh into a ragdoll. Every body
keeps the capsule's velocity, so juggle kills keep flying, and also gets `DeathBodySpeed` along
the shot. The hit body then gets an extra `DeathBoneSpeed` kick, scaled by the weapon's
`PhysicsImpulse / 800`. On a headshot kill the kick goes to the head and is multiplied by
`HeadshotDeathMultiplier`.

## Player

`AShooterCharacter` also moves enemy hits to its physics bodies (`bPerBoneWeaponHits`). The
player's bodies are query-only and the player has no flinch. Its third-person mesh is hidden from
its owner, so it is set to `AlwaysTickPoseAndRefreshBones`, which keeps the bodies following the
animation.

## Not done

There is no hit flash, because no NPC material parameter is known to exist for one.
