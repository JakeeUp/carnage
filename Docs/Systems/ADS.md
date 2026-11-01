# Aim Down Sights (ADS) and weapon feel

ADS is procedural and C++-owned. The arms anim BP no longer poses the aim. C++ moves the
whole arms mesh so that the weapon's sight lands on the camera axis.

## 1. One alpha

`AShooterCharacter` owns the ADS state.

- `IsAiming()` is the real input state. It is true when ADS is held and sprint has not
  cancelled it. `OnAimStateChanged` broadcasts whenever it changes.
- `GetADSAlpha()` is the visual blend in [0,1]. A critically damped spring
  (`SpringMath::CriticalSpringDamper` via `ViewSpringMath.h`) drives it, carrying both position
  and velocity. Smoothing time is 0.085 s going in and 0.07 s coming out. Because the spring
  keeps its velocity, reversing mid-transition decelerates and turns around. It never pops.
- `GetADSGameplayAlpha()` is `SmoothStep(alpha)`.

Everything reads that one value:

| Consumer | Alpha | Hip -> ADS |
|---|---|---|
| World FOV (camera modifier) | visual | `lerp(MovementFOV, ADSWorldFOV=65, a)` |
| Camera `FirstPersonFieldOfView` | visual | `lerp(70, ADSFirstPersonFOV=60, a)` |
| Look lag + bob | visual | x `lerp(1, 0.15, a)` |
| Velocity sway + landing dip | visual | x `lerp(1, 0.4, a)` |
| Viewmodel kick (at fire time) | visual | x `lerp(1, 0.35, a)` |
| Aim recoil + view punch | gameplay | x `lerp(1, 0.75, g)` |
| Grounded move speed | gameplay | x `lerp(1, 0.75, g)` |
| Look sensitivity | visual | x `tan(FOV/2)/tan(BaseFOV/2)` x `lerp(1, ADSLookSensitivityMultiplier, a)` |

Notes:

- **Gun magnification comes only from the FP FOV.** The engine cancels the world FOV for
  first-person primitives.
- **Sprint, slide and dash FOV are smoothed separately.** ADS is blended on top of them, so the
  two effects mix instead of one overriding the other.
- **Sensitivity uses the ADS part of the FOV only.** Sprint and dash FOV kicks therefore don't
  change aim speed.

Input rules (most recent input wins):

- Sprint cancels ADS. ADS comes back on sprint release if the button is still held.
- ADS and firing both stop sprint.
- ADS stays usable while sliding, wall running and airborne. The speed scale applies only to
  grounded walking.
- The sights drop while reloading. `IsAiming()` stays true, so they come straight back after the
  reload.

Legacy hook: `ABP_ShooterArms` calls `IsAimingDownSights()` by name. That function now returns
`bAnimBPDrivesADSPose && IsAiming()`, and the flag defaults to false. As a result, the ABP's
broken ADS state machine never runs. `AShooterWeapon` uses the same function to pick
`FiringMontage_ADS`, so it now always uses the hip montage, which is the intended behaviour.

## 2. Sight alignment

`UWeaponRecoilComponent` is the only code that writes the arms mesh relative transform. It
ticks in `TG_PostUpdateWork`, after animation and attachment, and before the camera manager
builds the view. All transforms below are UE order: `A * B` means apply A, then B.

```
S_m      = SightInWeapon * WeaponComp.GetRelativeTransform(ArmsComp)   // sight in arms space
Target   = FTransform(Identity, (EyeRelief, 0, 0))                     // camera space
Offset   = FTransform(SwayRot + KickRot + ..., SwayLoc + KickLoc + ...)
Hip      = HipRel * Offset              // offset about the eye
ADS      = S_m^-1 * Offset * Target     // offset about the SIGHT
ArmsRel  = Blend(Hip, ADS, alpha)       // lerp location, slerp rotation
ArmsRel  = ArmsRel * Punch * follow     // see view punch below
```

When `Offset` is identity, `S_m * ADS = Target`. In other words, the sight frame sits exactly
on the camera axis. Any point on that axis projects to the screen centre whatever the FOV or
`FirstPersonScale`, so the alignment holds through every FOV blend.

How the sight frame is found:

- **Convention:** +X points down the sight line, +Z is up, and the origin is the rear sight.
- **Dedicated socket:** if the weapon mesh has the `SightSocketName` socket (default `Sight`),
  it is used as-is.
- **Fallback socket:** otherwise C++ uses `FallbackSightSocketName`. When that is None, it falls
  back to `MuzzleSocketName`, composed with `SightOffset`. `SightOffset` is expressed in that
  socket's space.
- **Pistol defaults:** the pistol's `Muzzle` socket points along -X. The default `SightOffset`
  is therefore yaw 180 with location `(12, 0, 3)`: 12 cm back along the slide and 3 cm above
  the bore. **This is a first guess and needs visual tuning.**
- **Reference pose:** the socket is evaluated on the weapon's reference pose and cached. The
  slide or barrel animation therefore can't shake the aligned view. The hand-to-weapon
  attachment is still sampled live every frame.
- **Eye relief:** `EyeRelief` (28 cm) is clamped so that `EyeRelief * FirstPersonScale` stays
  beyond the near clip plane.
- **Arms animation while aiming:** with the full alpha, arms animation (idle breathing, fire
  montage) is cancelled at the sight. Procedural kick and sway supply the motion instead.

## 3. Recoil channels

1. **Aim recoil:** rotates the control rotation. It drains in, then recovers the part the
   player didn't pull down.
2. **View punch:** a camera-only spring (k=65, c=9) written to
   `UDoomCameraModifier::PunchRotation`. It never changes aim.
   - At the hip, the viewmodel follows the punch, so the gun stays fixed on screen.
   - In ADS the follow drops to 0, so the sight keeps showing the true aim while the view
     kicks around it.
   - Tunables: `ViewPunchViewmodelFollowHip` / `ViewPunchViewmodelFollowADS`.
3. **Viewmodel kick:** velocity impulses on two springs.
   - A positional spring (k=350) handles back and up.
   - A softer rotational spring (k=180) handles climb, yaw and roll.
   - In ADS the kick pivots about the sight.

The recoil fields live in `FWeaponRecoilParams`. The original four fields are kept. The new
fields are `VisualKickUp`, `VisualKickRollRandom`, `VisualKickYawRandom`, `ViewPunchPitch`,
`ViewPunchYawRandom` and `ViewPunchRollRandom`.

## 4. Sway, bob, inertia

- **Look lag:** a lagging facing quaternion chases the control rotation at `SwayInterpSpeed`,
  clamped to `MaxLookSway`. Its offset in the view frame drives the weapon rotation and position.
- **Figure-8 bob:** vertical period 0.45 s, lateral period 0.9 s. Weighted by
  `speed / BobReferenceSpeed(800)` and only applied while grounded and not sliding. This bob is
  separate from the camera head bob in `UDoomCameraEffectsComponent`.
- **Landing inertia:** a Z spring (k=120, damping ratio 0.5). On landing it gets a velocity
  impulse of `-ImpactSpeed * LandingImpulseScale`.
- **Sprint and slide poses:** each blends in through its own critical spring (T=0.1) and is
  faded out by `(1 - alpha)`.

## 5. Tunables most likely to need in-game tuning

- `AShooterWeapon` ADS block: `SightOffset` (check roll and up first), `EyeRelief`,
  `ADSFirstPersonFOV`.
- `UWeaponRecoilComponent`: `SprintPoseRotation/Location`, `SlidePoseRotation/Location` (signs
  depend on the arms rig), `LandingImpulseScale`, and the bob amplitudes.

## 6. Debug

These console variables are available in non-shipping builds:

- `carnage.Debug.ForceADS 1`: acts as if ADS is held.
- `carnage.Debug.DrawSight 1`: draws the following and prints the alpha plus the position and
  direction alignment error, in degrees, on screen:
  - green line: the camera axis
  - red line: the sight +X line, read back from the weapon after the arms moved
  - blue line: the sight +Z
  - yellow point: the rear sight

At alpha 1, both errors should read about 0 and the red line should vanish into the screen
centre. If the gun looks upside down or rolled, fix `SightOffset`'s rotation.
