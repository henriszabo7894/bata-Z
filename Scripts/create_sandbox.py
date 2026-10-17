"""
Crée la carte de test /Game/Bodycam/Maps/L_Sandbox (sol, couverts, piliers, salle sombre,
escalier + plateforme pour tester la chute) avec le GameMode Bodycam.
Relançable : la carte est recréée à chaque exécution.

Lancement (Unreal fermé) :
  UnrealEditor-Cmd.exe BodycamUE.uproject -run=pythonscript -script="<chemin>/create_sandbox.py"
Ou dans l'éditeur : Outils > Exécuter un script Python.
"""
import unreal

MAP_DIR = "/Game/Bodycam/Maps"
MAP_PATH = MAP_DIR + "/L_Sandbox"

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
MAT_FLOOR = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02.MI_PrototypeGrid_Gray_02")
MAT_WALL = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray")
MAT_COVER = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_Solid_Blue.MI_Solid_Blue")

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def box(label, center, size, material=MAT_WALL):
    """Cube de size (cm) centré sur center (cm). Le cube moteur fait 100 cm."""
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*center))
    component = actor.static_mesh_component
    component.set_static_mesh(CUBE)
    component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    actor.set_actor_label(label)
    actor.set_folder_path("Blockout")
    return actor


def movable(actor):
    actor.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    return actor


unreal.EditorAssetLibrary.make_directory(MAP_DIR)
if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    unreal.EditorAssetLibrary.delete_asset(MAP_PATH)

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

# --- Sol ---
box("Sol", (0, 0, -10), (6000, 6000, 20), MAT_FLOOR)

# --- Couvert bas (s'accroupir derrière) ---
box("Couvert_Bas_1m", (600, -350, 50), (60, 300, 100), MAT_COVER)
box("Couvert_Bas_1m_2", (650, 350, 50), (200, 60, 100), MAT_COVER)

# --- Piliers (tester le penché A/E) ---
for i, y in enumerate((-600, 0, 600)):
    box("Pilier_%d" % i, (950, y, 150), (50, 50, 300))

# --- Salle sombre avec porte (tester l'auto-exposition lente) ---
X0, X1, HALF_W, H, T = 1300, 2100, 300, 300, 20
box("Salle_Facade_G", (X0, -180, H / 2), (T, 240, H))
box("Salle_Facade_D", (X0, 180, H / 2), (T, 240, H))
box("Salle_Linteau", (X0, 0, 260), (T, 120, 80))
box("Salle_Fond", (X1, 0, H / 2), (T, 2 * HALF_W + T, H))
box("Salle_Cote_G", ((X0 + X1) / 2, -HALF_W, H / 2), (X1 - X0, T, H))
box("Salle_Cote_D", ((X0 + X1) / 2, HALF_W, H / 2), (X1 - X0, T, H))
box("Salle_Plafond", ((X0 + X1) / 2, 0, H + 10), (X1 - X0 + T, 2 * HALF_W + T, 20))
lamp = movable(actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(1900, 0, 270)))
lamp.set_actor_label("Salle_Lampe")
lamp.point_light_component.set_editor_property("attenuation_radius", 700.0)
lamp.point_light_component.set_light_color(unreal.LinearColor(1.0, 0.75, 0.45, 1.0))

# --- Escalier (12 marches de 20 cm) + plateforme à 2,4 m (tester chute / atterrissage) ---
for i in range(12):
    step_height = (i + 1) * 20
    box("Marche_%02d" % i, (-500 - i * 30, 600, step_height / 2), (30, 200, step_height))
box("Plateforme", (-1010, 600, 230), (300, 300, 20))
box("Garde_Corps", (-1010, 760, 280), (300, 20, 100), MAT_COVER)

# --- Éclairage extérieur (dynamique, pas de précalcul nécessaire) ---
sun = movable(actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(0, -40, 30)))
sun.set_actor_label("Soleil")
sky = movable(actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600)))
sky.set_actor_label("SkyLight")
sky.light_component.set_editor_property("real_time_capture", True)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("Atmosphere")
actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0)).set_actor_label("Brouillard")

# --- Point d'apparition ---
start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 100))
start.set_actor_label("PlayerStart")

# --- GameMode Bodycam pour cette carte ---
game_mode = unreal.load_class(None, "/Script/BodycamUE.BodycamGameMode")
world.get_world_settings().set_editor_property("default_game_mode", game_mode)

if unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    unreal.log("BODYCAM_SANDBOX_OK " + MAP_PATH)
else:
    unreal.log_error("BODYCAM_SANDBOX_FAILED")
