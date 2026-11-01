"""
Scales BP_ShooterAIController's perception to arena-sized levels.

The template shipped with a 15 m sight radius, sized for its small map; in the 60 x 45 m showcase
arena most enemies never saw the player. Re-runnable: it only saves when a value changes.

Run headless from the repo root:
  UnrealEditor-Cmd.exe FPS_Movement.uproject -run=pythonscript -script="Tools/Python/set_ai_perception_ranges.py"
      -EnablePlugins=PythonScriptPlugin -unattended -nop4 -nosplash -NullRHI
"""

import unreal

CONTROLLER = "/Game/Variant_Shooter/Blueprints/AI/BP_ShooterAIController"

SIGHT = {"sight_radius": 4000.0, "lose_sight_radius": 5000.0, "peripheral_vision_angle_degrees": 120.0}
HEARING = {"hearing_range": 3000.0}

bp = unreal.load_asset(CONTROLLER)
cdo = unreal.get_default_object(bp.generated_class())
perception = cdo.get_editor_property("AIPerception")

changed = False
for config in perception.get_editor_property("senses_config"):
    wanted = SIGHT if isinstance(config, unreal.AISenseConfig_Sight) else HEARING if isinstance(config, unreal.AISenseConfig_Hearing) else {}
    for name, value in wanted.items():
        old = config.get_editor_property(name)
        if abs(old - value) > 0.01:
            config.set_editor_property(name, value)
            changed = True
        unreal.log("[AIPerception] %s.%s: %s -> %s" % (config.get_class().get_name(), name, old, value))

if changed:
    unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    unreal.log("[AIPerception] saved %s" % CONTROLLER)
else:
    unreal.log("[AIPerception] already up to date")
