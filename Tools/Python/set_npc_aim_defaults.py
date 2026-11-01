"""
Applies the NPC aim model defaults to the shooter Blueprints.

Run headless:
  UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript -script="<this file>" \
      -EnablePlugins=PythonScriptPlugin -unattended -nop4 -nosplash -NullRHI

BP_ShooterWeapon_Pistol
  AimVariance = 0. The NPC holding it supplies its own spread (AShooterNPC "AI|Aim"), and a
  per-weapon cone on top stacked up to ~25 degrees of error.

BP_ShooterNPC
  Re-saved so the removed aim properties (AimVarianceHalfAngle 10, MinAimOffsetZ -35,
  MaxAimOffsetZ -60, which aimed at the thighs) are dropped, and the "AI|Aim" values are
  set to the C++ defaults below so the asset states them explicitly.

Idempotent: running it again changes nothing.
"""

import unreal

PISTOL = "/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Pistol"
NPC = "/Game/Variant_Shooter/Blueprints/AI/BP_ShooterNPC"

WEAPON_VALUES = {
    "aim_variance": 0.0,
}

# Keep in sync with the defaults in Source/FPS_Movement/Core/AI/ShooterNPC.h.
NPC_VALUES = {
    "aim_range": 10000.0,
    "aim_height_fraction": 0.45,
    "base_spread_half_angle": 2.0,
    "acquire_spread_half_angle": 5.0,
    "settle_time": 0.8,
    "tracking_lost_time": 1.0,
    "moving_spread_half_angle": 1.5,
    "moving_spread_reference_speed": 1200.0,
    "max_moving_spread_half_angle": 3.0,
    "lead_accuracy": 0.75,
    "max_lead_time": 1.0,
}


def log(msg):
    unreal.log("[NPCAim] " + msg)


def apply(path, values):
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not bp_class:
        unreal.log_error("[NPCAim] could not load " + path)
        return
    cdo = unreal.get_default_object(bp_class)
    for name, value in values.items():
        old = cdo.get_editor_property(name)
        cdo.set_editor_property(name, value)
        log("%s.%s: %s -> %s" % (path.rsplit("/", 1)[-1], name, old, value))
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    log("saved " + path)


apply(PISTOL, WEAPON_VALUES)
apply(NPC, NPC_VALUES)
