# Bodycam look

An optional "body-worn camera" presentation. It's a view-only effect: aim, hitscan and the
first-person arms are unchanged. The camera is chest-mounted and follows the gun on a spring.
The gun leads and the view catches up.

The filter is deliberately obvious, in the style of commercial bodycam shooters. When it is on,
the screen should read as body-worn camera footage at a glance:

- fisheye with dark rounded corners
- radial colour fringing
- heavy sensor noise
- strong vignette
- crunchy sharpening
- a cool, flat-blacked grade

The centre of the screen (about 15% of its height) is kept clean for aiming.

Toggle it in game with **B**, or with `carnage.Bodycam.Enable 1|0`. It fades in and out over
0.35 s.

## Pieces

| Piece | Path | Role |
|---|---|---|
| `UBodycamComponent` | `Source/FPS_Movement/Camera/BodycamComponent.*` | State, fade, tunables, rig simulation, lens instance, modifier lifetime |
| `UBodycamCameraModifier` | `Source/FPS_Movement/Camera/BodycamCameraModifier.*` | Thin modifier (Priority 200, after `UDoomCameraModifier`) that forwards `ModifyCamera` / `ModifyPostProcess` to the component |
| `ACarnageLevelRules` | `Source/FPS_Movement/Core/CarnageLevelRules.*` | `AInfo` placed in a level; `bStartWithBodycam` is the level default |
| HUD OSD | `UShooterHUDWidget::PaintBodycam*` | Camera-style OSD that crossfades with the game HUD (see below) |
| `M_BodycamLens` | `/Game/Carnage/Bodycam/M_BodycamLens` | Post-process lens material (after tonemapping) |
| `IA_ToggleBodycam` | `/Game/Input/Actions/IA_ToggleBodycam`, mapped to B in `/Game/Input/IMC_Default` | Toggle input |
| Generator | `Tools/Python/make_bodycam_assets.py` | Rebuilds the material and the input action and mapping (idempotent) |

`AShooterCharacter` creates the component (`GetBodycam()`). It binds `ToggleBodycamAction`, a
soft reference whose C++ default is `IA_ToggleBodycam`, so the character Blueprint doesn't need
editing. `UEnhancedInputComponent` deletes `BindKey`, and `BindDebugKey` is compiled out of
shipping builds, which is why the toggle is a real input action.

## State and the level default

- **One source of truth:** `carnage.Bodycam.Enable` holds the state. B, `SetBodycamEnabled` and
  the console all write to it. The component picks up console changes every tick, so a respawned
  pawn (a new component) starts in the same mode.
- **Level default:** the first `ACarnageLevelRules` in the world is read the first time a local
  player's component initializes (BeginPlay, or the first tick after possession). If
  `bStartWithBodycam` is set, the player spawns straight into the look with no fade. The rules
  apply once per world, so a respawn doesn't undo a player's toggle.
- **Local player only:** the component does nothing on AI-controlled characters.

## View rig (`ApplyToView`, after the Doom modifier)

The rig starts from the POV the Doom modifier has already built (movement and ADS FOV, tilt, view
punch, shakes) and composes on top of it.

1. **View lag:**
   - The control rotation's pitch and yaw changes are taken out of a (view - control) offset.
   - A critically damped spring pulls the offset back to zero: ω = 14 rad/s at the hip and 28 in
     ADS, ζ = 1.
   - The offset is clamped to 6°, or 6° × 0.25 in ADS.
   - Only the control-rotation part of the POV lags (`POV * inverse(Control) * Lagged`), so tilt,
     punch and shakes pass straight through.
   - Snaps larger than 90° in one frame (teleport, respawn) reset the lag.
2. **Roll:**
   - Strafe roll is 2.5° at full strafe speed, with the same sign as the Doom strafe tilt.
   - Turn roll is `-yawRate * 0.015`, clamped to 4°.
   - The two are smoothed at rate 8.
3. **Footsteps:**
   - Steps come from distance travelled on the ground: one step per 230 cm walking or 320 cm
     sprinting.
   - Each step kicks a spring (ω = 22, ζ = 0.6) for about 0.8 cm of drop and ±0.6° of roll,
     alternating feet. Sprinting doubles both.
4. **Landing:**
   - Severity is `(fallSpeed - 250) / (1400 - 250)`, clamped to 0..1.
   - It kicks a spring (ω = 11, ζ = 0.5) for about 3° of nod-down and 5 cm of drop.
5. **Chest mount:** the view sits 10 cm forward and 20 cm down in the yaw frame.
6. **FOV:** +16° on top of the current world FOV, so it composes with the sprint, slide, dash and
   ADS FOV. This is more than the suggested +12° because the lens normalisation zooms the centre
   by about 1.3x, and the extra 4° offsets it.

**Scaling:**

- Rig motion (roll, footsteps, landing, rolling shutter) is multiplied by
  `lerp(1, ADSMotionScale = 0.25, ADSAlpha)`, by the Motion accessibility scalar and by the fade
  alpha.
- The view lag is multiplied by Motion and the fade alpha. In ADS it is limited by the stiffer
  spring and the smaller clamp instead.

### Aim, viewmodel and ADS

- **Aim follows the gun:** `AShooterWeapon::GetShotRay` and
  `AShooterCharacter::GetWeaponTargetLocation` trace from the camera **component** along the
  **control rotation**. Hitscan and projectiles therefore follow the gun, not the lagging,
  chest-mounted view. This also makes them independent of view punch.
- **The gun leads:** the arms are attached to the camera component, which uses the pawn control
  rotation and isn't moved by modifiers. With the bodycam on, the gun visibly leads the view and
  sits a little higher on screen (chest mount). That is the intended look.
- **ADS alignment:**
  - The chest offset fades out with `(1 - ADSAlpha)`, so in ADS the view goes back to the eye.
  - The lag spring settles to zero divergence.
  - The remaining roll turns about the view axis.
  - Together, the sight (aligned on the camera component's axis by `UWeaponRecoilComponent`)
    lands on the screen centre once the spring settles.
  - While turning in ADS, the sight can sit up to 1.5° off centre. That is the bodycam
    "gun leads" read, kept small.
- **Lens centre stays true:** the lens distortion fades to zero inside r ≈ 0.08, and the rolling
  shutter is centred on the middle scanline. The screen centre is always the true aim point.

## Post-process (`ApplyPostProcess`)

The modifier queues a weighted PP blend (weight = fade alpha) through the camera manager's
cached PP blends. It never writes the camera component's `PostProcessSettings`, which
`UDoomCameraEffectsComponent` owns, and it wins over them by blend order.

Defaults, editable in `UBodycamComponent::PostProcess`:

These are deliberately stronger than the first research numbers (grain 0.2, sharpen 1.0,
vignette 0.6, white temp 7000).

| Setting | Value |
|---|---|
| Film grain | 0.5 (shadows 1.0, mids 0.55, highlights 0.2), texel size 1.6, times Grain |
| Sharpen | 2.2 (crunchy) |
| Vignette | 0.9 |
| Bloom | 1.5 |
| White temp | 5400 (below 6500, so the image reads cool) |
| Saturation | (0.72, 0.74, 0.78) |
| Contrast | 1.25 |
| Colour offset | (0.012, 0.016, 0.024): lifted, slightly blue blacks |
| Auto exposure | speed up 0.8, speed down 0.5 (slow adaptation) |
| Motion blur | 0.5, times Motion |
| Scene fringe | Not used; the lens material does chromatic aberration |

### Lens material

`M_BodycamLens` is in the post-process domain and runs after tonemapping. It does four things:

- **Barrel distortion** (Brown-Conrady), with aspect correction:
  ```
  c = uv - 0.5; c.x *= Aspect; r2 = dot(c, c)
  w = smoothstep(0.075, 0.45, sqrt(r2))                      // centre ~15% stays clean
  k = K * Distortion * w
  c *= (1 + k1 r2 + k2 r2^2) / (1 + K1 rn + K2 rn^2),  rn = (Aspect/2)^2
  ```
  - K1 is 0.32 and K2 is 0.10, a clearly visible fisheye.
  - **Changed from the reference formula:** that formula normalised at r² = 0.25. On 16:9 this
    sampled about 7% outside the frame at the left and right edges, which showed as black bands.
    Normalising at the left and right edge centres fixes this. Only the corners now sample
    outside the frame, and the housing border covers them.
- **Chromatic aberration:** R and B are sampled at radial scales `1 ± CAStrength * r2 * w`, with
  CAStrength = 0.016 times ChromaticAberration. Because of `w`, the centre has no fringing.
- **Rolling shutter:** `uv.x += RollingShutter * (uv.y - 0.5)`. From C++, RollingShutter is
  `clamp(yawRate / 540) * 0.025`, times Motion and the ADS scale.
- **Housing border:** a rounded rectangle (radius 0.16), a 45% lens falloff toward the corners,
  and a fade to black where samples fall outside the frame. `Border` scales all three.

- **Damage static** (`DamageStatic`, `LowHealthStatic`): see the next section.

Parameters are set each frame on a `UMaterialInstanceDynamic`. It is added to
`WeightedBlendables` with weight 1, and the whole PP blend carries the fade alpha. The base
material's defaults for `Distortion`, `ChromaticAberration`, `Border` and `RollingShutter` are
0. For blend weights below 1, the engine lerps instance parameters from the base material, so
neutral defaults are what make the fade smooth.

### Damage static

When the player is hit in bodycam mode the feed glitches with red analog TV static. It's part of
`M_BodycamLens`, so the normal game view never shows it.

**Driving it (`UBodycamComponent`):**

- **Hits:** the component binds the owner's `OnDamaged(LifePercent)`. A drop in health is a
  hit, and its size sets the impulse:
  - a drop of `LightHitHealthDrop` (0.02) adds `LightHitImpulse` (0.3); smaller drops scale down
    linearly,
  - a drop of `HeavyHitHealthDrop` (0.2) or more adds 1,
  - between the two the impulse ramps linearly.
- **Armour hits:** `OnDamageSource` without a health drop in the same frame (armour soaked it)
  adds `ArmorHitImpulse` (0.2).
- **Envelope:** impulses stack, capped at 1. The visible value rises towards the impulse with a
  25 ms time constant (`DamageStaticAttackTime`) and the impulse releases exponentially. The
  release time (to 10%) runs from `LightHitReleaseTime` (0.18 s) to `HeavyHitReleaseTime`
  (0.45 s) by hit size; a stacked burst keeps the longest. It runs on real time, so kill
  hit-stop doesn't freeze it.
- **Output:** `DamageStatic = value * BlendAlpha * accessibility`. The PP blend weight is the
  fade alpha too, so the static is gone well before the normal view is back.
- **Jolt:** hits with an impulse of at least `HitJoltMinImpulse` (0.6) kick the landing spring
  (about 1.2° nod) and the footstep spring (about 1.5° roll, random side). These are scaled by
  Motion, ADS and the fade like every other rig impulse.
- **Low health:** below `LowHealthStaticThreshold` (30% health), `LowHealthStatic` goes from
  `LowHealthStaticMin` (0.35) at the threshold to 1 at zero health, eased at rate 4 so it doesn't
  pop. It uses the same alpha and accessibility scale.

**Material (`BodycamUV` and `BodycamDamageStatic` custom nodes):**

| Layer | What it does |
|---|---|
| Tearing | Rows are cut into 14-48 bands, re-rolled at 30 Hz. A fraction `0.6 * D` of the bands is sampled with a sideways offset of up to `StaticTear` (0.07 UV) times D. |
| Rolling band | A band rolls up the frame at 0.8 screens/s and wobbles sideways. |
| Chromatic split | R and B are sampled `±StaticSplit` (0.012 UV) times D apart, on top of the lens fringing. |
| Static | Per-pixel hash noise in `StaticNoiseScale` (2 px) cells, re-rolled at 60 Hz, with per-line brightness noise. It's tinted `StaticTint` (1, 0.06, 0.04) and blended over the scene by `D^1.5 * 0.62`. |
| Red grade | The scene is pulled towards red luminance by `D^2 * 0.6`. |
| Crackle | The brightest noise cells become red sparks. The density is `0.03 D + 0.1 D^2`, weighted towards the edges. |
| Dropout | Above D = 0.6, up to 30% of 24 Hz frames lose the signal and collapse to static. |
| Low health | Pulsing (1.1 Hz) red static at 5-15%, 2 px scanlines and a red edge tint, all weighted towards the edges. |

- **Centre readability:** tearing and static are cut to about a third inside r ≈ 0.06-0.4 of
  the screen height. Even at D = 1 the centre keeps a little of the cut.
- **Feel by hit size:**
  - a light hit (about 0.3) is sparse sparks, a few torn rows and a slight split, gone in about
    0.2 s;
  - a medium hit (about 0.55) is a red wash with heavy edge static, and the centre still reads;
  - a heavy hit (1) is near-total red static with dropouts for about 0.4 s.

The noise hash is "Hash without Sine" (`hash12`) by Dave Hoskins (MIT licence).

## HUD: camera OSD

In bodycam mode the game HUD is replaced by burned-in camera metadata. It is drawn in Slate on top
of the scene, so the lens never distorts it.

- **Crossfade:** `UShooterHUDWidget::NativePaint` paints the game layout at opacity
  `1 - alpha` and the OSD at `alpha`, where alpha is `UBodycamComponent::GetBlendAlpha()`. B
  morphs between the two over the 0.35 s fade. At alpha 0 the OSD isn't drawn and the game HUD
  is unchanged.
- **Text:** the engine's `Mono` typeface (Droid Sans Mono), off-white at 0.9 alpha with a 1 px
  dark outline, about 1.8% of screen height (`BodycamTextHeight`), inset 4% from each edge.
  There are no backplates, accents or large numerals.

| Where | Fields |
|---|---|
| Top-left | Red `REC` dot (1 Hz), `STYLE D // DRIFTER` (blinks inverted on a rank change) |
| Top-right | `YYYY-MM-DD HH:MM:SS`, `HDG 274  SPD 12 m/s` (camera yaw, ground speed) |
| Bottom-left | `HR 100  ARM 050`, `DASH [##-]` |
| Bottom-right | Blinking `RELOADING [####------]`, `RELOAD` or `NO AMMO`; `MAG 08 / 090` (red when low) |
| Bottom centre | `BodycamDeviceId` (fictional) and a battery glyph draining from 87% at 1% per minute |

- **Combat feedback:** hitmarkers become four thin white ticks (red on a kill), with no punch or
  damage numbers. An incoming hit flashes the screen edge on that side red for 0.35 s, with a
  short jittering `!` just inside the OSD. The crosshair is hidden; `bBodycamCenterDot` keeps a
  dot.
- **Low health:** below `LowHealthThreshold` the OSD text eases to `BodycamAlertColor` and the
  REC dot blinks at 3 Hz.
- **Flavour:** a very slight brightness flicker (`BodycamFlickerAmount` 0.04) and a rare
  one-frame sideways tear of one row or the whole OSD (`BodycamGlitchChancePerSecond` 0.005,
  rolled on real time).
- **Tunables:** all are under **HUD|Bodycam** on the widget.

## Accessibility and console

Each scalar is a `UPROPERTY` on the component multiplied by a console variable. Both range
over 0..1 and default to 1.

| Console variable | Effect |
|---|---|
| `carnage.Bodycam.Enable` | 0/1 state (synced with B) |
| `carnage.Bodycam.Distortion` | Lens barrel distortion |
| `carnage.Bodycam.Motion` | Lag, roll, footsteps, landing, rolling shutter, motion blur |
| `carnage.Bodycam.ChromaticAberration` | Lens chromatic aberration |
| `carnage.Bodycam.Grain` | Film grain |
| `carnage.Bodycam.DamageStatic` | Damage static and low-health static (flashing content) |

Debug console variables (not in shipping builds):

| Console variable | Effect |
|---|---|
| `carnage.Debug.BodycamDamageTest <drop>` | Simulates a hit with that health drop (0.02 light, 0.2 heavy) without dealing damage. It fires once and resets to 0. |
| `carnage.Debug.BodycamDamageTestDelay <s>` | Delay before the test hit, counted from when the local player's component picks it up. |
| `carnage.Debug.BodycamDamageHold <0..1>` | Pins the damage static value for screenshots and tuning (-1 = off). |
| `carnage.Debug.BodycamHealthOverride <0..1>` | Health fraction for the low-health layer (-1 = off). |

To capture a heavy burst without enemies:

```
-ExecCmds="carnage.Bodycam.Enable 1,carnage.Debug.BodycamDamageTestDelay 0.85,carnage.Debug.BodycamDamageTest 0.2,carnage.Debug.FeelCapture 1"
```

FeelCapture's first screenshot comes about 1 s after the pawn exists, so the hit lands just
before it. Screenshot timing jitters with the frame rate, so use `BodycamDamageHold` for a frame
at an exact intensity.

## Likely to need visual tuning

- **Chest mount and framing:** `ChestMountOffset` and how high the gun sits at the hip. This
  wasn't checked with the arms mesh.
- **Lens:** K1 and K2 against `FOVBonus`, since the barrel zooms the centre by about 1.3x.
- **Accessibility gaps:** sharpen, vignette and the colour grade have no accessibility scalar.
  They are static and fade with the toggle. Add a `carnage.Bodycam.Grade` console variable if
  they need one.
- **Highlights:** bloom 1.5 together with contrast 1.25 runs bright in sunlit levels.
- **Fringing:** `CAStrength`.
- **Footsteps:** stride lengths (they depend on the game's move speeds) and step amplitudes.
- **Grading:** white temperature and saturation, per level lighting.
- **Damage static:** checked only from held-value and test-hit screenshots on Lvl_Shooter, not
  under real enemy fire. The impulse curve, the release times and the low-health layer (faint by
  design) may need tuning in play.

## Credits and licences

- **Barrel distortion:** the standard Brown-Conrady radial model,
  `r' = r (1 + k1 r^2 + k2 r^4)` (D. C. Brown, 1966). The formulation follows the description in
  [ayamflow/glsl-barrel-pincushion](https://github.com/ayamflow/glsl-barrel-pincushion) (MIT
  licence). The HLSL in `make_bodycam_assets.py` was written from the formula; no code was copied.
- **Static noise:** "Hash without Sine" `hash12` by Dave Hoskins (MIT licence), in the damage
  static custom nodes.
- **Other code:** the chromatic aberration, rolling shutter, rounded-rect border, damage static
  layers, springs and overlay are original to this project.
- **Engine features:** film grain, sharpen, vignette, bloom and grading are Unreal Engine
  post-process features.
- **Fonts:** the overlay uses the engine's bundled default `Mono` typeface (Droid Sans Mono,
  Apache 2.0, shipped with Unreal Engine).
- **No real devices:** no real-world vendor's branding, device names or UI layout is reproduced.
  The device and unit ID is invented.
