"""
Generates the assets used by UBodycamComponent.

Run headless:
  UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript -script="<this file>" \
      -EnablePlugins=PythonScriptPlugin -unattended -nop4 -nosplash -NullRHI

Creates / updates:
  /Game/Input/Actions/IA_ToggleBodycam - boolean input action (created if missing)
  /Game/Input/IMC_Default              - adds the B key -> IA_ToggleBodycam mapping (idempotent)

Creates (overwrites) under /Game/Carnage/Bodycam/:
  M_BodycamLens - post-process material (after tonemapping):
      * Brown-Conrady barrel distortion with aspect correction and an undistorted centre,
      * radial chromatic aberration (R and B sampled at slightly different radial scales),
      * rolling-shutter skew (horizontal shear by scanline, driven by yaw rate from C++),
      * dark rounded-rect border that also hides out-of-frame samples in the corners,
      * red analog "TV static" damage glitch (DamageStatic) and a faint low-health static
        layer (LowHealthStatic), both driven from C++.

Scalar parameters (driven per frame from C++ through a UMaterialInstanceDynamic). The base
material's defaults for Distortion / ChromaticAberration / Border / RollingShutter are 0, the
neutral state: when the blendable is faded in with a weight below 1, the engine lerps the
instance's parameters from the base material's values, so the effect fades in smoothly.
  Distortion           0..1  multiplier on K1/K2
  ChromaticAberration  0..1  multiplier on CAStrength
  Border               0..1  rounded-rect border opacity
  RollingShutter       signed horizontal skew in viewport-UV units per unit of (uv.y - 0.5)
  K1, K2                     radial coefficients (0.32, 0.10: a clearly visible fisheye)
  CAStrength                 radial scale difference between R and B at r^2 = 1 (0.016)
  DamageStatic         0..1  damage glitch: red static, row tearing, rolling band, channel
                             split, signal-loss dropout above ~0.55 (0 = off)
  LowHealthStatic      0..1  faint pulsing red static + scanlines, edge weighted (0 = off)
  StaticTint                 static colour (1, 0.06, 0.04)
  StaticTear                 max sideways row shift in viewport UV at DamageStatic 1 (0.07)
  StaticSplit                horizontal R/B channel split in viewport UV at DamageStatic 1 (0.012)
  StaticNoiseScale           static grain size in pixels (2)

The static hash is "Hash without Sine" (hash12) by Dave Hoskins (MIT licence).

The distortion formula is the standard Brown-Conrady radial model,
  r' = r * (1 + k1*r^2 + k2*r^4),
as also used in ayamflow/glsl-barrel-pincushion (MIT licence). The HLSL below is written
from the formula, not copied from any source.
"""

import unreal

FOLDER = "/Game/Carnage/Bodycam"
NAME = "M_BodycamLens"
mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[CarnageBodycam] " + msg)


def fresh_material(name):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    mat = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError("Failed to create " + path)
    return mat


def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def connect(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("connect failed: %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def vector(mat, name, default, x, y):
    e = expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*default))
    e.set_editor_property("group", "Bodycam")
    return e


def scalar(mat, name, default, x, y):
    e = expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", default)
    e.set_editor_property("group", "Bodycam")
    return e


def custom(mat, desc, code, inputs, output_type, x, y):
    e = expr(mat, unreal.MaterialExpressionCustom, x, y)
    e.set_editor_property("description", desc)
    e.set_editor_property("code", code)
    e.set_editor_property("output_type", output_type)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    e.set_editor_property("inputs", ins)
    return e


# Output UV for one colour channel. UV is the viewport UV of the output pixel.
UV_CODE = r"""
float Aspect = ViewSize.x / max(ViewSize.y, 1.0);
float2 uv = UV;
// Rolling shutter: each scanline is read out slightly later, so a fast pan shears the frame.
// Centred on the middle row so the screen centre (the aim point) never moves.
uv.x += Skew * (uv.y - 0.5);

// Damage static: analog tearing. Rows are cut into bands and some bands are read with a sideways
// offset; a rolling band wobbles harder; the colour channels split apart. Every channel's node
// gets the same inputs, so the bands line up and only ChannelSign differs.
struct FBodycamHash
{
    // "Hash without Sine" hash12, Dave Hoskins (MIT).
    float H(float2 p)
    {
        float3 p3 = frac(float3(p.xyx) * 0.1031);
        p3 += dot(p3, p3.yzx + 33.33);
        return frac((p3.x + p3.y) * p3.z);
    }
};
FBodycamHash Hash;
float D = saturate(Damage);
if (D > 0.001)
{
    float tf = fmod(floor(T * 30.0), 997.0);
    // Keep the aim area readable at medium intensity; full tearing only near DamageStatic 1.
    float2 cc = (UV - 0.5) * float2(Aspect, 1.0);
    float Keep = lerp(0.3, 1.0, smoothstep(0.06, 0.4, length(cc)));
    Keep = lerp(Keep, 1.0, smoothstep(0.75, 1.0, D));
    float Bands = 14.0 + floor(Hash.H(float2(tf, 7.0)) * 34.0);
    float Band = floor(uv.y * Bands);
    float Torn = step(Hash.H(float2(Band, tf)), D * 0.6);
    float Shift = Hash.H(float2(Band + 17.0, tf + 3.0)) * 2.0 - 1.0;
    float Roll = frac(T * 0.8);
    float rd = abs(uv.y - Roll);
    rd = min(rd, 1.0 - rd);
    float RollBand = 1.0 - smoothstep(0.0, 0.045, rd);
    float Wobble = sin(uv.y * 260.0 + T * 45.0);
    uv.x += (Shift * Torn + RollBand * 0.5 * Wobble) * TearStrength * D * Keep;
    uv.x += ChannelSign * SplitStrength * D * lerp(0.5, 1.0, Keep);
}

float2 c = uv - 0.5;
c.x *= Aspect;
float r2 = dot(c, c);

// Brown-Conrady radial term, faded in away from the centre so the aim area (central ~15% of the
// screen height) stays rectilinear and fringe-free.
float w = smoothstep(0.075, 0.45, sqrt(r2));
float k1 = K1 * Amount * w;
float k2 = K2 * Amount * w;
float f = 1.0 + k1 * r2 + k2 * r2 * r2;
// Normalise so the left/right edge centres (r^2 = (Aspect/2)^2) map onto themselves. Normalising
// at the top/bottom edge instead (r^2 = 0.25) samples ~7% outside the frame at the sides on 16:9.
// Only the corners still sample out of frame; the housing border covers them.
float rn = 0.25 * Aspect * Aspect;
float n = 1.0 + K1 * Amount * rn + K2 * Amount * rn * rn;
c *= f / n;

// Radial chromatic aberration: this channel is sampled at a slightly different radius.
c *= 1.0 + ChannelScale * r2 * w;

c.x /= Aspect;
return c + 0.5;
"""

# Final colour: one channel from each sample, times the border / out-of-frame mask.
COMPOSE_CODE = r"""
float Aspect = ViewSize.x / max(ViewSize.y, 1.0);

// Samples that land outside the source frame (corners under strong barrel) fade to black.
float2 e = min(SampleUV, 1.0 - SampleUV);
float Inside = smoothstep(-0.004, 0.006, min(e.x, e.y));

// Rounded-rect housing border (signed distance in aspect-corrected units).
float2 p = (UV - 0.5) * float2(Aspect, 1.0);
float Radius = 0.16;
float2 HalfSize = float2(0.5 * Aspect, 0.5) - 0.008;
float2 q = abs(p) - (HalfSize - Radius);
float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - Radius;
float Housing = 1.0 - smoothstep(-0.06, 0.0, d);
// Lens falloff toward the corners (on top of the engine vignette), part of the housing look.
float Falloff = 1.0 - 0.45 * smoothstep(0.45, 1.05, length(p));
float Mask = Inside * lerp(1.0, Housing * Falloff, saturate(Border));

return float3(CR.r, CG.g, CB.b) * Mask;
"""

# Damage + low-health static over the composed frame (after the lens and the border).
DAMAGE_CODE = r"""
struct FBodycamHash
{
    // "Hash without Sine" hash12, Dave Hoskins (MIT).
    float H(float2 p)
    {
        float3 p3 = frac(float3(p.xyx) * 0.1031);
        p3 += dot(p3, p3.yzx + 33.33);
        return frac((p3.x + p3.y) * p3.z);
    }
};
FBodycamHash Hash;

float3 c = Color;
float D = saturate(Damage);
float L = saturate(LowHealth);
if (D <= 0.001 && L <= 0.001)
{
    return c;
}

float Aspect = ViewSize.x / max(ViewSize.y, 1.0);
float3 TintC = Tint.rgb;
float r = length((UV - 0.5) * float2(Aspect, 1.0));
float Edge = smoothstep(0.08, 0.5, r);

// Per-pixel static, re-rolled at 60 Hz, with per-line brightness noise.
float tn = fmod(floor(T * 60.0), 991.0);
float2 Cell = floor(UV * ViewSize / max(NoiseScale, 1.0));
float n = Hash.H(Cell + tn * float2(37.0, 113.0));
float LineN = Hash.H(float2(floor(UV.y * ViewSize.y * 0.5), tn + 5.0));
float sn = n * (0.7 + 0.6 * LineN);
float3 Static = TintC * sn + pow(sn, 6.0) * 0.35;

// Low health: faint, pulsing red static and scanlines, weighted to the edges.
if (L > 0.001)
{
    float Pulse = 0.5 + 0.5 * sin(T * 6.2832 * 1.1);
    float Scan = 0.5 + 0.5 * sin(UV.y * ViewSize.y * 1.5708);
    float EdgeW = lerp(0.35, 1.0, Edge);
    c *= 1.0 - 0.16 * L * Scan * EdgeW;
    c = lerp(c, Static, L * (0.04 + 0.08 * Pulse) * EdgeW);
}

// Damage: red grade, static over the scene, line jitter, signal-loss dropout.
if (D > 0.001)
{
    // The aim area stays readable-ish until the top of the range.
    float Keep = lerp(0.35, 1.0, smoothstep(0.06, 0.4, r));
    Keep = lerp(Keep, 1.0, smoothstep(0.75, 1.0, D));
    float Luma = dot(c, float3(0.299, 0.587, 0.114));
    c = lerp(c, Luma * TintC * 1.5 + c * 0.25, D * 0.5);
    c = lerp(c, Static, saturate(D * 0.8) * Keep * 0.8);
    c *= 1.0 + (LineN - 0.5) * 0.5 * D;
    // Above ~0.55 some frames (24 Hz steps) lose the signal and collapse to static.
    float td = fmod(floor(T * 24.0), 983.0);
    float Chance = saturate((D - 0.55) / 0.45) * 0.55;
    float Drop = step(Hash.H(float2(td, 91.0)), Chance);
    c = lerp(c, Static * 0.7, Drop * 0.85);
}
return max(c, 0.0);
"""


def make_lens():
    mat = fresh_material(NAME)
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

    screen = expr(mat, unreal.MaterialExpressionScreenPosition, -1600, 0)
    view = expr(mat, unreal.MaterialExpressionViewSize, -1600, 150)

    p_dist = scalar(mat, "Distortion", 0.0, -1600, 300)
    p_ca = scalar(mat, "ChromaticAberration", 0.0, -1600, 400)
    p_border = scalar(mat, "Border", 0.0, -1600, 500)
    p_skew = scalar(mat, "RollingShutter", 0.0, -1600, 600)
    p_k1 = scalar(mat, "K1", 0.32, -1600, 700)
    p_k2 = scalar(mat, "K2", 0.10, -1600, 800)
    p_cas = scalar(mat, "CAStrength", 0.016, -1600, 900)
    p_damage = scalar(mat, "DamageStatic", 0.0, -1600, 1000)
    p_low = scalar(mat, "LowHealthStatic", 0.0, -1600, 1100)
    p_tear = scalar(mat, "StaticTear", 0.07, -1600, 1200)
    p_split = scalar(mat, "StaticSplit", 0.012, -1600, 1300)
    p_noise = scalar(mat, "StaticNoiseScale", 2.0, -1600, 1400)
    p_tint = vector(mat, "StaticTint", (1.0, 0.06, 0.04, 1.0), -1600, 1500)
    # Real time, so the static keeps moving through kill hit-stop.
    time = expr(mat, unreal.MaterialExpressionTime, -1600, 1700)
    time.set_editor_property("ignore_pause", True)

    # ChannelScale: +CA for red, 0 for green, -CA for blue.
    ca = expr(mat, unreal.MaterialExpressionMultiply, -1350, 900)
    connect(p_ca, "", ca, "A")
    connect(p_cas, "", ca, "B")
    neg_ca = expr(mat, unreal.MaterialExpressionMultiply, -1200, 1000)
    connect(ca, "", neg_ca, "A")
    neg_ca.set_editor_property("const_b", -1.0)
    zero = expr(mat, unreal.MaterialExpressionConstant, -1200, 1100)
    zero.set_editor_property("r", 0.0)
    plus_one = expr(mat, unreal.MaterialExpressionConstant, -1200, 1200)
    plus_one.set_editor_property("r", 1.0)
    minus_one = expr(mat, unreal.MaterialExpressionConstant, -1200, 1300)
    minus_one.set_editor_property("r", -1.0)

    channel_scales = [ca, zero, neg_ca]
    channel_signs = [plus_one, zero, minus_one]
    colors = []
    uv_nodes = []
    for i, cs in enumerate(channel_scales):
        uvn = custom(mat, "BodycamUV", UV_CODE,
                     ["UV", "ViewSize", "Amount", "K1", "K2", "Skew", "ChannelScale",
                      "Damage", "T", "TearStrength", "SplitStrength", "ChannelSign"],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT2, -900, i * 250)
        connect(screen, "ViewportUV", uvn, "UV")
        connect(view, "", uvn, "ViewSize")
        connect(p_dist, "", uvn, "Amount")
        connect(p_k1, "", uvn, "K1")
        connect(p_k2, "", uvn, "K2")
        connect(p_skew, "", uvn, "Skew")
        connect(cs, "", uvn, "ChannelScale")
        connect(p_damage, "", uvn, "Damage")
        connect(time, "", uvn, "T")
        connect(p_tear, "", uvn, "TearStrength")
        connect(p_split, "", uvn, "SplitStrength")
        connect(channel_signs[i], "", uvn, "ChannelSign")
        uv_nodes.append(uvn)

        st = expr(mat, unreal.MaterialExpressionSceneTexture, -600, i * 250)
        st.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
        connect(uvn, "", st, "UVs")
        colors.append(st)

    comp = custom(mat, "BodycamCompose", COMPOSE_CODE,
                  ["CR", "CG", "CB", "UV", "SampleUV", "ViewSize", "Border"],
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3, -300, 250)
    connect(colors[0], "Color", comp, "CR")
    connect(colors[1], "Color", comp, "CG")
    connect(colors[2], "Color", comp, "CB")
    connect(screen, "ViewportUV", comp, "UV")
    connect(uv_nodes[1], "", comp, "SampleUV")
    connect(view, "", comp, "ViewSize")
    connect(p_border, "", comp, "Border")

    dmg = custom(mat, "BodycamDamageStatic", DAMAGE_CODE,
                 ["Color", "UV", "ViewSize", "T", "Damage", "LowHealth", "Tint", "NoiseScale"],
                 unreal.CustomMaterialOutputType.CMOT_FLOAT3, 0, 250)
    connect(comp, "", dmg, "Color")
    connect(screen, "ViewportUV", dmg, "UV")
    connect(view, "", dmg, "ViewSize")
    connect(time, "", dmg, "T")
    connect(p_damage, "", dmg, "Damage")
    connect(p_low, "", dmg, "LowHealth")
    connect(p_tint, "", dmg, "Tint")
    connect(p_noise, "", dmg, "NoiseScale")

    mel.connect_material_property(dmg, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    assets.save_loaded_asset(mat)
    log("saved " + mat.get_path_name())


INPUT_ACTION_FOLDER = "/Game/Input/Actions"
INPUT_ACTION_NAME = "IA_ToggleBodycam"
IMC_PATH = "/Game/Input/IMC_Default"
TOGGLE_KEY = "B"


def make_input():
    ia_path = INPUT_ACTION_FOLDER + "/" + INPUT_ACTION_NAME
    if assets.does_asset_exist(ia_path):
        action = unreal.load_asset(ia_path)
    else:
        factory_cls = getattr(unreal, "InputAction_Factory", None) or getattr(unreal, "InputActionFactory")
        action = tools.create_asset(INPUT_ACTION_NAME, INPUT_ACTION_FOLDER, unreal.InputAction, factory_cls())
        if action is None:
            raise RuntimeError("Failed to create " + ia_path)
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property("action_description", "Toggle the bodycam camera look")
    assets.save_loaded_asset(action)
    log("saved " + action.get_path_name())

    imc = unreal.load_asset(IMC_PATH)
    if imc is None:
        raise RuntimeError("Missing " + IMC_PATH)
    already = False
    for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"):
        if m.get_editor_property("action") == action:
            already = True
    if not already:
        key = unreal.Key()
        key.set_editor_property("key_name", TOGGLE_KEY)
        imc.modify()
        imc.map_key(action, key)
        # map_key does not dirty the package, so force the save.
        if not assets.save_asset(IMC_PATH, only_if_is_dirty=False):
            raise RuntimeError("Failed to save " + IMC_PATH)
        log("mapped %s -> %s in %s" % (TOGGLE_KEY, INPUT_ACTION_NAME, IMC_PATH))
    else:
        log("%s already mapped in %s" % (INPUT_ACTION_NAME, IMC_PATH))


if not assets.does_directory_exist(FOLDER):
    assets.make_directory(FOLDER)
make_lens()
make_input()
