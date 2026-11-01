"""
Builds /Game/Showcase/Lvl_Showcase: a guided course that demonstrates every mechanic.

Run headless from the repo root:
  UnrealEditor-Cmd.exe FPS_Movement.uproject -run=pythonscript -script="Tools/Python/build_showcase_level.py"
      -EnablePlugins=PythonScriptPlugin -unattended -nop4 -nosplash -NullRHI

Layout (X is forward from the spawn, +Y is to the player's right, units in cm). Dimensions come
from UDoomMovementComponent: jump apex ~140, double jump ~280, mantle reach 200, wall run
<= 1.5 s at ~1000 cm/s, dash 450 cm x2.

  A  Spawn hall            X  -600 ..  1200
  B  Slide hill            X  1500 ..  4300   up-ramp, crest, steep down-ramp, low gate
  C  Mantle steps          X  4600 ..  5800   ledges at 120 / 290 / 470
  D  Wall-run gauntlet     X  5800 ..  7300   alternating walls over a drop
  E  Dash gap              X  8100 ..  9100   1000 cm gap: jump + double jump + dash
  F  Speed ramp + bhop     X  9900 .. 14500   slide down into a striped bunny-hop strip
  G  Shooting range        X 15000 .. 20600   targets at 10 / 25 / 50 m
  H  Combat arena          X  1000 ..  7000, Y 2500 .. 7000   NPC spawners, cover, jump pads
"""

import math
import unreal

LEVEL_PATH = "/Game/Showcase/Lvl_Showcase"
TEMPLATE = "/Engine/Maps/Templates/Template_Default"

CUBE = unreal.load_asset("/Game/LevelPrototyping/Meshes/SM_ChamferCube")      # 100 cm, centre pivot
CYL = unreal.load_asset("/Game/LevelPrototyping/Meshes/SM_Cylinder")          # 100 cm, base pivot
MAT_GROUND = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray")
MAT_WALL = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark")
MAT_ACCENT = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_DefaultColorway")
MAT_STEP = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02")
JUMP_PAD = unreal.load_asset("/Game/LevelPrototyping/Interactable/JumpPad/BP_JumpPad")
NPC_SPAWNER = unreal.load_asset("/Game/Variant_Shooter/Blueprints/AI/BP_ShooterNPCSpawner")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
log = []


# ── helpers ─────────────────────────────────────────────────────────────────

def _finish(actor, label, folder):
    unreal.log("[Showcase] placed %s" % label)
    # Outliner folders need editor UI services that don't exist in a headless commandlet, so the
    # zone is encoded in the label prefix instead.
    actor.set_actor_label("%s__%s" % (folder.split("/")[-1], label))
    return actor


def mesh_actor(mesh, loc, rot):
    # spawn_actor_from_object goes through viewport actor factories and crashes headless;
    # spawning the class and assigning the mesh is safe.
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
    a.static_mesh_component.set_static_mesh(mesh)
    return a


def box(label, folder, center, size, mat=MAT_WALL, rot=(0.0, 0.0, 0.0)):
    """Chamfer cube centred at `center` with full extents `size` and (pitch, yaw, roll)."""
    a = mesh_actor(CUBE, unreal.Vector(*center), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    a.static_mesh_component.set_material(0, mat)
    return _finish(a, label, folder)


def block(label, folder, x0, x1, y0, y1, z0, z1, mat=MAT_WALL):
    """Axis-aligned block from min/max corners."""
    return box(label, folder, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), mat)


def slope(label, folder, x0, z0, x1, z1, y0, y1, thickness=40.0, mat=MAT_STEP):
    """Ramp whose TOP surface runs from (x0, z0) to (x1, z1)."""
    run, rise = x1 - x0, z1 - z0
    length = math.hypot(run, rise)
    pitch = math.degrees(math.atan2(rise, run))
    # Centre the slab under the surface line.
    nx, nz = -math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    cx = (x0 + x1) / 2 - nx * thickness / 2
    cz = (z0 + z1) / 2 - nz * thickness / 2
    return box(label, folder, (cx, (y0 + y1) / 2, cz), (length, y1 - y0, thickness), mat, rot=(pitch, 0.0, 0.0))


def cylinder(label, folder, base, radius, height, mat=MAT_WALL, rot=(0.0, 0.0, 0.0)):
    a = mesh_actor(CYL, unreal.Vector(*base), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    a.set_actor_scale3d(unreal.Vector(radius / 50.0, radius / 50.0, height / 100.0))
    a.static_mesh_component.set_material(0, mat)
    return _finish(a, label, folder)


def sign(label, text, loc, yaw=180.0, size=70.0, color=(255, 255, 255)):
    a = eas.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    t = a.text_render
    t.set_text(unreal.Text(text))
    t.set_world_size(size)
    t.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    t.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
    t.set_text_render_color(unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    return _finish(a, label, "Showcase/Signs")


def spawn_bp(bp, label, folder, loc, yaw=0.0):
    a = eas.spawn_actor_from_class(bp.generated_class(), unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    return _finish(a, label, folder)


ORANGE = (255, 140, 40)


# ── level ───────────────────────────────────────────────────────────────────

if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
    unreal.EditorAssetLibrary.delete_asset(LEVEL_PATH)
if unreal.EditorAssetLibrary.does_asset_exist("/Game/Showcase/__Probe"):
    unreal.EditorAssetLibrary.delete_asset("/Game/Showcase/__Probe")

unreal.log("[Showcase] creating level")
assert les.new_level_from_template(LEVEL_PATH, TEMPLATE), "could not create level"

unreal.log("[Showcase] level created")
# Replace the template's floor; keep its sky, sun, fog and clouds.
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "Floor":
        a.destroy_actor()       # EditorActorSubsystem.destroy_actor needs selection services (crashes headless)
    elif isinstance(a, unreal.PlayerStart):
        a.set_actor_location(unreal.Vector(-300.0, 0.0, 100.0), False, False)
        a.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0), False)

block("Ground", "Showcase/Ground", -1200, 21200, -1600, 7600, -100, 0, MAT_GROUND)

# A  Spawn hall
F = "Showcase/A_Spawn"
block("Hall_BackWall", F, -700, -600, -900, 900, 0, 500)
block("Hall_LeftWall", F, -600, 1200, -1000, -900, 0, 500)
block("Hall_RightWall_Front", F, -600, 100, 900, 1000, 0, 500)      # gap to the right leads to the arena
sign("Sign_Title", "PROJECT CARNAGE  //  SHOWCASE", (1150, 0, 420), size=90, color=ORANGE)
sign("Sign_Controls", "WASD move   SPACE jump x2   SHIFT sprint   CTRL slide   Q dash\nRMB aim   LMB fire   R reload   B bodycam", (1150, 0, 300), size=38)
sign("Sign_Course", "COURSE  >>>", (1150, -450, 160), size=60)
sign("Sign_Arena", "ARENA  >>>", (300, 950, 200), yaw=270, size=60, color=ORANGE)

# B  Slide hill
F = "Showcase/B_Slide"
slope("Slide_UpRamp", F, 1500, 0, 2300, 250, -600, 600)
block("Slide_Crest", F, 2300, 2700, -600, 600, 0, 250, MAT_STEP)
slope("Slide_DownRamp", F, 2700, 250, 3700, 0, -600, 600)
block("Slide_GatePostL", F, 3950, 4050, -340, -300, 0, 160, MAT_ACCENT)
block("Slide_GatePostR", F, 3950, 4050, 300, 340, 0, 160, MAT_ACCENT)
# No overhead beam yet: the slide does not lower the capsule, so a low gate would be a wall.
sign("Sign_Slide", "SPRINT + CTRL on the downhill: slopes add speed", (1450, 0, 190), size=40)

# C  Mantle steps
F = "Showcase/C_Mantle"
block("Mantle_Step1", F, 4600, 5000, -600, 600, 0, 120, MAT_STEP)
block("Mantle_Step2", F, 5000, 5400, -600, 600, 0, 290, MAT_STEP)
block("Mantle_Step3", F, 5400, 5800, -600, 600, 0, 470, MAT_STEP)
sign("Sign_Mantle", "MANTLE: hold forward into a ledge", (4550, 0, 330), size=40)

# D  Wall-run gauntlet (alternating walls over a drop)
F = "Showcase/D_WallRun"
block("WallRun_Left", F, 5800, 6700, -560, -460, 0, 1000, MAT_ACCENT)
block("WallRun_Right", F, 6400, 7300, 460, 560, 0, 1000, MAT_ACCENT)
block("WallRun_Landing", F, 7300, 8100, -600, 600, 0, 470, MAT_STEP)
sign("Sign_WallRun", "WALL RUN: jump at a wall while moving, SPACE to kick off", (5750, 0, 700), size=40)

# E  Dash gap
F = "Showcase/E_Dash"
block("Dash_FarLedge", F, 9100, 9900, -600, 600, 0, 470, MAT_STEP)
sign("Sign_Dash", "GAP: jump, double jump, then Q to dash", (8250, 0, 640), size=40)

# F  Speed ramp into the bunny-hop strip
F = "Showcase/F_Bhop"
slope("Bhop_SpeedRamp", F, 9900, 470, 11500, 0, -600, 600)
for i in range(6):
    x = 11800 + i * 500
    block("Bhop_Stripe_%d" % i, F, x, x + 60, -600, 600, 0, 2, MAT_ACCENT)
sign("Sign_Bhop", "BUNNY HOP: jump the moment you land to keep momentum", (11500, 0, 300), size=40)

# G  Shooting range
F = "Showcase/G_Range"
block("Range_Counter", F, 14950, 15050, -700, 700, 0, 110, MAT_STEP)
block("Range_Backstop", F, 20500, 20600, -1200, 1200, 0, 800)
for dist in (10, 25, 50):
    x = 15000 + dist * 100
    for j, y in enumerate((-450, 0, 450)):
        block("Range_%dm_Post_%d" % (dist, j), F, x - 10, x + 10, y - 10, y + 10, 0, 130)
        cylinder("Range_%dm_Plate_%d" % (dist, j), F, (x - 15, y, 175), 45, 6, MAT_ACCENT, rot=(-90.0, 0.0, 0.0))
    sign("Sign_Range_%dm" % dist, "%d m" % dist, (x - 30, -700, 260), size=60, color=ORANGE)
sign("Sign_Range", "RANGE: hip fire vs aimed, watch the tracers and impacts", (14900, 0, 330), size=40)

# H  Combat arena (to the right of the spawn hall)
F = "Showcase/H_Arena"
AX0, AX1, AY0, AY1 = 1000, 7000, 2500, 7000
block("Arena_Wall_Near", F, AX0, AX1, AY1, AY1 + 100, 0, 700)
block("Arena_Wall_Back", F, AX1, AX1 + 100, AY0, AY1 + 100, 0, 700)
block("Arena_Wall_Front", F, AX0 - 100, AX0, AY0 + 900, AY1 + 100, 0, 700)       # opening at the hall side
block("Arena_Wall_SideA", F, AX0, 3200, AY0 - 100, AY0, 0, 700)
block("Arena_Wall_SideB", F, 4800, AX1, AY0 - 100, AY0, 0, 700)                   # gap in the middle = entrance
cx, cy = (AX0 + AX1) / 2, (AY0 + AY1) / 2
block("Arena_CenterPlatform", F, cx - 500, cx + 500, cy - 500, cy + 500, 0, 300, MAT_STEP)
for k, (dx, dy) in enumerate(((-800, 0), (800, 0), (0, -800), (0, 800))):
    spawn_bp(JUMP_PAD, "Arena_JumpPad_%d" % k, F, (cx + dx, cy + dy, 0))
for k, (x, y) in enumerate(((1800, 3300), (6200, 3300), (1800, 6200), (6200, 6200), (4000, 3400), (4000, 6100))):
    block("Arena_Cover_%d" % k, F, x - 150, x + 150, y - 60, y + 60, 0, 130, MAT_ACCENT)
for k, (x, y) in enumerate(((2300, 4700), (5700, 4700))):
    cylinder("Arena_Pillar_%d" % k, F, (x, y, 0), 120, 900)
for k, (x, y, yaw) in enumerate(((1500, 6600, -45), (6600, 6600, -135), (6600, 2900, 135), (cx, cy, 0))):
    z = 330 if k == 3 else 100
    sp = spawn_bp(NPC_SPAWNER, "Arena_NPCSpawner_%d" % k, F, (x, y, z), yaw)
    try:
        sp.set_editor_property("SpawnCount", 1)
        sp.set_editor_property("InitialSpawnDelay", 3.0 + k)
    except Exception as e:  # property names follow AShooterNPCSpawner
        log.append("spawner props: %s" % e)
sign("Sign_ArenaInside", "ARENA: slide into enemies to launch them, keep them airborne", (cx, AY0 + 150, 450), yaw=-90, size=45, color=ORANGE)

# I  Aim trainer (to the right of the bunny-hop strip). Targets face -X; the player stands 15 m back.
F = "Showcase/I_AimTrainer"
trainer = eas.spawn_actor_from_class(unreal.CarnageAimTrainer, unreal.Vector(12500, 4500, 0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
_finish(trainer, "AimTrainer", F)
sign("Sign_AimTrainer", "AIM TRAINER  >>>", (11600, 650, 220), yaw=180, size=60, color=ORANGE)
sign("Sign_AimTrainerHow", "AIM TRAINER: shoot a mode button to start", (10950, 4500, 330), yaw=180, size=40)

# Level rules: the showcase opens in bodycam mode (B toggles it).
rules = eas.spawn_actor_from_class(unreal.CarnageLevelRules, unreal.Vector(-400, 0, 300))
rules.set_editor_property("bStartWithBodycam", True)
_finish(rules, "LevelRules", "Showcase/Rules")

# Navigation for the NPCs: one bounds volume over everything, generated at runtime.
nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(10000, 3000, 500))
nav.set_actor_scale3d(unreal.Vector(11300 / 100.0, 4700 / 100.0, 800 / 100.0))
_finish(nav, "NavBounds", "Showcase/Nav")
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.RecastNavMesh):
        try:
            a.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.DYNAMIC)
            log.append("navmesh set to dynamic")
        except Exception as e:
            log.append("navmesh prop: %s" % e)

ok = les.save_current_level()
log.append("saved=%s actors=%d" % (ok, len(eas.get_all_level_actors())))
for line in log:
    unreal.log("[Showcase] " + line)
