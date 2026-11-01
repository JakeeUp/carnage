# NPC aim

How a shooter NPC picks where each shot goes. The aim model lives in
`AShooterNPC::GetWeaponTargetLocation` (`Core/AI/ShooterNPC.*`). The tunables are under the
**AI|Aim** category on `BP_ShooterNPC`.

## Per shot

1. **Origin.** The shot leaves the visible third-person muzzle
   (`AShooterWeapon::GetMuzzleWorldLocation`). The NPC aims from the same point, so the point it
   returns lies exactly on the shot line.
2. **Aim point.** Upper chest: capsule centre plus `AimHeightFraction` (0.45) of the capsule
   half height. Crouching lowers it with the capsule.
3. **Lead.** For projectile weapons, the NPC reads the launch speed from the projectile class
   defaults (`AShooterWeapon::GetProjectileLaunchSpeed`). It estimates the flight time, refines
   it once against the predicted point, and caps it at `MaxLeadTime` (1 s). It then adds
   `velocity x flight time x LeadAccuracy`. `LeadAccuracy` is 0.75, so a target that keeps
   strafing still gets missed behind, and a target that changes direction dodges. Hitscan
   weapons get no lead.
4. **Spread.** A random direction inside a cone around the ideal direction. The half-angle is:
   - `AcquireSpreadHalfAngle` (5 deg) when the target is first acquired, blending down to
     `BaseSpreadHalfAngle` (2 deg) over `SettleTime` (0.8 s) of continuous tracking.
     Tracking resets when the target changes, when the eye-to-target line is blocked, or when
     more than `TrackingLostTime` (1 s) passes between shots.
   - plus `MovingSpreadHalfAngle` (1.5 deg) at `MovingSpreadReferenceSpeed` (1200 cm/s),
     linear in target speed and capped at `MaxMovingSpreadHalfAngle` (3 deg).

## Weapon side

- For non-local owners, `GetShotRay` and `CalculateProjectileSpawnTransform` start at the
  third-person muzzle. The NPC's first-person mesh is hidden and has no hand socket, so shots
  used to start away from the visible gun. The local player's camera ray is unchanged.
- `IShooterWeaponHolder::SuppliesAimSpread` returns true for NPCs. The weapon then skips its
  own `AimVariance`, so the spread is not applied twice. The pistol's `AimVariance` is also 0
  (`Tools/Python/set_npc_aim_defaults.py`).

## Debugging

`carnage.Debug.DrawNPCAim 1` draws, for one second per shot: the shot ray (red), the spread cone
(orange), the chest point (yellow) and the led aim point (green). It also logs the distance,
the spread, the lead offset, line of sight, and whether the ray hits the target where it stands
now.
