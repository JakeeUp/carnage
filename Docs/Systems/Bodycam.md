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
| HUD overlay | `UShooterHUDWidget::PaintBodycamOverlay` | REC dot, clock, device line, battery; dims panels, hides the crosshair |
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

Parameters are set each frame on a `UMaterialInstanceDynamic`. It is added to
`WeightedBlendables` with weight 1, and the whole PP blend carries the fade alpha. The base
material's defaults for `Distortion`, `ChromaticAberration`, `Border` and `RollingShutter` are
0. For blend weights below 1, the engine lerps instance parameters from the base material, so
neutral defaults are what make the fade smooth.

## HUD overlay

The overlay is drawn in Slate on top of the scene, so the lens never distorts it.

- **REC:** top-left, a red dot blinking at 1 Hz plus the word "REC".
- **Clock:** top-right, real-time `YYYY-MM-DD HH:MM:SS`.
- **Device line:** bottom centre, `BodycamDeviceId` ("CARNAGE // UNIT 07 // CAM-02", fictional)
  and a battery glyph that drains slowly from 87% at 1% per minute. It is centred so it clears
  the vitals and ammo panels.
- **Font:** monospace (`Mono`) with a 1 px dark outline, inset 4% from each edge.
- **Panels:** the crosshair hides (set `bBodycamCenterDot` to keep a dot), hitmarkers stay, and
  the vitals, ammo and rank panels dim to `BodycamPanelOpacity` (0.65).

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

## Credits and licences

- **Barrel distortion:** the standard Brown-Conrady radial model,
  `r' = r (1 + k1 r^2 + k2 r^4)` (D. C. Brown, 1966). The formulation follows the description in
  [ayamflow/glsl-barrel-pincushion](https://github.com/ayamflow/glsl-barrel-pincushion) (MIT
  licence). The HLSL in `make_bodycam_assets.py` was written from the formula; no code was copied.
- **Other code:** the chromatic aberration, rolling shutter, rounded-rect border, springs and
  overlay are original to this project.
- **Engine features:** film grain, sharpen, vignette, bloom and grading are Unreal Engine
  post-process features.
- **Fonts:** the overlay uses the engine's bundled default `Mono` typeface (Droid Sans Mono,
  Apache 2.0, shipped with Unreal Engine).
- **No real devices:** no real-world vendor's branding, device names or UI layout is reproduced.
  The device and unit ID is invented.
