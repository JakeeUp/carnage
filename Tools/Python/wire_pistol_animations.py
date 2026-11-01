"""
Wires the First Person Pistol Animations pack montages into BP_Pistol's class defaults.

Requires the C++ from the "Weapon draw/holster montages, empty reload variants and slide lock"
commit to be built (the properties below live on AShooterWeapon) and the paid pack to be present
under /Game/FirstPersonPistolAnimations (it is gitignored, see the README).

Run headless (editor closed, or at least BP_Pistol not open):
  UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript -script="<this file>" \
      -EnablePlugins=PythonScriptPlugin -unattended -nop4 -nosplash -NullRHI

Idempotent: only properties whose value differs are changed, and the Blueprint is saved only
when something changed. Every property is printed with its before/after value.

Sets on /Game/FirstPerson/Blueprints/Weapons/BP_Pistol:
  WeaponReloadMontage        = AM_Weapon_Pistol_Reload_Montage      (weapon mesh, was empty)
  WeaponReloadMontage_Empty  = AM_Weapon_Pistol_Reload_Dry_Montage  (weapon mesh, slide starts locked)
  DrawMontage                = AM_Pistol_Draw_Montage               (arms, on equip)
  HolsterMontage             = AM_Pistol_Holster_Montage            (arms, before a weapon switch)
  SlideLockSequence          = AS_Weapon_Pistol_Fire_Dry_Montage    (weapon mesh, last round; an
                               AnimSequence played on ABP_Pistol's DefaultSlot)

ReloadMontage_Empty (arms) is left unset: the pack has no separate arms dry reload, so the
regular arms reload (AM_Pistol_Reload_Montage, 2.0 s) is used for both.
"""

import unreal

BP_PATH = "/Game/FirstPerson/Blueprints/Weapons/BP_Pistol"
PACK = "/Game/FirstPersonPistolAnimations/Animations/"

# python property name -> asset path
WIRING = [
    ("weapon_reload_montage", PACK + "Pistol/AM_Weapon_Pistol_Reload_Montage"),
    ("weapon_reload_montage_empty", PACK + "Pistol/AM_Weapon_Pistol_Reload_Dry_Montage"),
    ("draw_montage", PACK + "AM_Pistol_Draw_Montage"),
    ("holster_montage", PACK + "AM_Pistol_Holster_Montage"),
    ("slide_lock_sequence", PACK + "Pistol/AS_Weapon_Pistol_Fire_Dry_Montage"),
]


def log(msg):
    unreal.log("[WirePistolAnims] " + msg)
    print("[WirePistolAnims] " + msg)


def path_of(obj):
    return obj.get_path_name() if obj else "None"


def main():
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_PATH)
    if not bp_class:
        unreal.log_error("[WirePistolAnims] Could not load " + BP_PATH)
        return

    cdo = unreal.get_default_object(bp_class)
    changed = False
    failures = 0

    for prop, asset_path in WIRING:
        asset = unreal.load_asset(asset_path)
        if not asset:
            unreal.log_error("[WirePistolAnims] Missing pack asset %s (is the paid pack installed?)" % asset_path)
            failures += 1
            continue

        try:
            current = cdo.get_editor_property(prop)
        except Exception as e:
            unreal.log_error("[WirePistolAnims] BP_Pistol has no property '%s' (build the C++ first): %s" % (prop, e))
            failures += 1
            continue

        if current == asset:
            log("%-28s already %s" % (prop, path_of(asset)))
            continue

        cdo.set_editor_property(prop, asset)
        changed = True
        log("%-28s %s -> %s" % (prop, path_of(current), path_of(asset)))

    if changed:
        bp = unreal.load_asset(BP_PATH)
        bp.modify()
        if unreal.EditorAssetLibrary.save_asset(BP_PATH, only_if_is_dirty=False):
            log("Saved " + BP_PATH)
        else:
            unreal.log_error("[WirePistolAnims] Failed to save " + BP_PATH)
    else:
        log("Nothing to change.")

    if failures:
        unreal.log_error("[WirePistolAnims] %d propert%s not set." % (failures, "y" if failures == 1 else "ies"))


main()
