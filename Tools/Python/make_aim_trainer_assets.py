"""
Generates the material used by ACarnageAimTrainer (targets, backdrop, buttons).

Run headless:
  UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript -script="<this file>" \
      -EnablePlugins=PythonScriptPlugin -unattended -nop4 -nosplash -NullRHI

Creates (overwrites) under /Game/Carnage/Training/:
  M_AimTarget - opaque unlit emissive. Emissive = Color * Intensity, darkened slightly toward the
                silhouette so spheres read as round. Parameters: Color (vector), Intensity (scalar).
"""

import unreal

FOLDER = "/Game/Carnage/Training"
mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[AimTrainer] " + msg)


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


def make_aim_target():
    mat = fresh_material("M_AimTarget")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    color = expr(mat, unreal.MaterialExpressionVectorParameter, -900, -150)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.0, 0.9, 1.0, 1.0))

    intensity = expr(mat, unreal.MaterialExpressionScalarParameter, -900, 0)
    intensity.set_editor_property("parameter_name", "Intensity")
    intensity.set_editor_property("default_value", 4.0)

    # Rim shading: 1 facing the camera, RimDarken at the silhouette.
    fres = expr(mat, unreal.MaterialExpressionFresnel, -900, 150)
    fres.set_editor_property("exponent", 2.0)
    fres.set_editor_property("base_reflect_fraction", 0.0)
    rim_lo = expr(mat, unreal.MaterialExpressionConstant, -900, 300)
    rim_lo.set_editor_property("r", 1.0)
    rim_hi = expr(mat, unreal.MaterialExpressionConstant, -900, 380)
    rim_hi.set_editor_property("r", 0.45)
    rim = expr(mat, unreal.MaterialExpressionLinearInterpolate, -650, 250)
    connect(rim_lo, "", rim, "A")
    connect(rim_hi, "", rim, "B")
    connect(fres, "", rim, "Alpha")

    m1 = expr(mat, unreal.MaterialExpressionMultiply, -450, -50)
    connect(color, "", m1, "A")
    connect(intensity, "", m1, "B")
    m2 = expr(mat, unreal.MaterialExpressionMultiply, -250, 50)
    connect(m1, "", m2, "A")
    connect(rim, "", m2, "B")

    mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    assets.save_loaded_asset(mat)
    log("saved " + mat.get_path_name())


def main():
    if not assets.does_directory_exist(FOLDER):
        assets.make_directory(FOLDER)
    make_aim_target()
    log("done")


main()
