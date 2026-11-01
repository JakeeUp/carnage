"""
Generates the procedural weapon-FX materials used by UCarnageWeaponFXSubsystem.

Run headless:
  UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript -script="<this file>" \
      -EnablePlugins=PythonScriptPlugin -unattended -nop4 -nosplash -NullRHI

Creates (overwrites) under /Game/Carnage/FX/:
  M_FX_AdditiveEmissive  - unlit additive glow; colour = CustomData[0..2], intensity = CustomData[3]
  M_FX_TranslucentSoft   - unlit translucent puff; colour = CustomData[0..2], opacity = CustomData[3]
  M_FX_ShellBrass        - opaque lit brass for ejected casings
  M_FX_BulletHole        - deferred decal, dark scorched hole with soft rim, fades via DecalLifetimeOpacity
"""

import unreal

FOLDER = "/Game/Carnage/FX"
mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[CarnageFX] " + msg)


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


def custom_data(mat, index, default, x, y):
    e = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, x, y)
    e.set_editor_property("data_index", index)
    e.set_editor_property("const_default_value", default)
    return e


def custom_rgb_and_scalar(mat):
    """Returns (rgb_expr, scalar_expr) built from per-instance custom data 0..3."""
    r = custom_data(mat, 0, 1.0, -900, -200)
    g = custom_data(mat, 1, 0.6, -900, -100)
    b = custom_data(mat, 2, 0.2, -900, 0)
    a = custom_data(mat, 3, 1.0, -900, 120)
    rg = expr(mat, unreal.MaterialExpressionAppendVector, -700, -150)
    connect(r, "", rg, "A")
    connect(g, "", rg, "B")
    rgb = expr(mat, unreal.MaterialExpressionAppendVector, -550, -100)
    connect(rg, "", rgb, "A")
    connect(b, "", rgb, "B")
    return rgb, a


def soft_edge(mat, exponent, x, y):
    """1 - Fresnel: bright where the surface faces the camera, fading at silhouettes."""
    fres = expr(mat, unreal.MaterialExpressionFresnel, x, y)
    fres.set_editor_property("exponent", exponent)
    fres.set_editor_property("base_reflect_fraction", 0.0)
    inv = expr(mat, unreal.MaterialExpressionOneMinus, x + 180, y)
    connect(fres, "", inv, "")
    return inv


def finish(mat):
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    assets.save_loaded_asset(mat)
    log("saved " + mat.get_path_name())


def make_additive():
    mat = fresh_material("M_FX_AdditiveEmissive")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mat.set_editor_property("used_with_static_lighting", False)

    rgb, intensity = custom_rgb_and_scalar(mat)
    edge = soft_edge(mat, 1.6, -700, 250)

    m1 = expr(mat, unreal.MaterialExpressionMultiply, -350, 0)
    connect(rgb, "", m1, "A")
    connect(intensity, "", m1, "B")
    m2 = expr(mat, unreal.MaterialExpressionMultiply, -200, 100)
    connect(m1, "", m2, "A")
    connect(edge, "", m2, "B")
    mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


def make_translucent():
    mat = fresh_material("M_FX_TranslucentSoft")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    rgb, alpha = custom_rgb_and_scalar(mat)
    edge = soft_edge(mat, 2.5, -700, 250)

    op = expr(mat, unreal.MaterialExpressionMultiply, -250, 200)
    connect(alpha, "", op, "A")
    connect(edge, "", op, "B")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    finish(mat)


def make_brass():
    mat = fresh_material("M_FX_ShellBrass")
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    col = expr(mat, unreal.MaterialExpressionConstant3Vector, -400, -100)
    col.set_editor_property("constant", unreal.LinearColor(0.78, 0.52, 0.18, 1.0))
    metal = expr(mat, unreal.MaterialExpressionConstant, -400, 50)
    metal.set_editor_property("r", 1.0)
    rough = expr(mat, unreal.MaterialExpressionConstant, -400, 150)
    rough.set_editor_property("r", 0.32)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat)


def make_bullet_hole():
    mat = fresh_material("M_FX_BulletHole")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    # radial distance from decal centre, 0 at centre .. ~1 at edge
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    centre = expr(mat, unreal.MaterialExpressionConstant2Vector, -1100, 120)
    centre.set_editor_property("r", 0.5)
    centre.set_editor_property("g", 0.5)
    dist = expr(mat, unreal.MaterialExpressionDistance, -900, 50)
    connect(uv, "", dist, "A")
    connect(centre, "", dist, "B")
    two = expr(mat, unreal.MaterialExpressionConstant, -900, 180)
    two.set_editor_property("r", 2.0)
    radial = expr(mat, unreal.MaterialExpressionMultiply, -750, 80)
    connect(dist, "", radial, "A")
    connect(two, "", radial, "B")

    # opacity: solid core with a soft scorched falloff
    inv = expr(mat, unreal.MaterialExpressionOneMinus, -600, 80)
    connect(radial, "", inv, "")
    sat = expr(mat, unreal.MaterialExpressionSaturate, -480, 80)
    connect(inv, "", sat, "")
    exp_c = expr(mat, unreal.MaterialExpressionConstant, -480, 200)
    exp_c.set_editor_property("r", 0.6)
    soft = expr(mat, unreal.MaterialExpressionPower, -350, 80)
    connect(sat, "", soft, "Base")
    connect(exp_c, "", soft, "Exp")
    life = expr(mat, unreal.MaterialExpressionDecalLifetimeOpacity, -350, 220)
    opacity = expr(mat, unreal.MaterialExpressionMultiply, -200, 120)
    connect(soft, "", opacity, "A")
    connect(life, "", opacity, "B")

    # colour: near-black hole in the middle, sooty brown towards the rim
    core = expr(mat, unreal.MaterialExpressionConstant3Vector, -600, -200)
    core.set_editor_property("constant", unreal.LinearColor(0.004, 0.004, 0.004, 1.0))
    rim = expr(mat, unreal.MaterialExpressionConstant3Vector, -600, -320)
    rim.set_editor_property("constant", unreal.LinearColor(0.045, 0.035, 0.028, 1.0))
    lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, -350, -250)
    connect(core, "", lerp, "A")
    connect(rim, "", lerp, "B")
    connect(radial, "", lerp, "Alpha")

    rough = expr(mat, unreal.MaterialExpressionConstant, -350, -60)
    rough.set_editor_property("r", 0.95)

    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    finish(mat)


def main():
    if not assets.does_directory_exist(FOLDER):
        assets.make_directory(FOLDER)
    make_additive()
    make_translucent()
    make_brass()
    make_bullet_hole()
    log("done")


main()
