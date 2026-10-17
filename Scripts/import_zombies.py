# Import des zombies supplementaires (Persos/) et des animations (Anims/)
# sur le squelette commun SK_Zombie_Ch10_Skeleton. Ne reimporte pas ce qui existe deja.
import os
import glob
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "Zombies"))
DEST_MESH = "/Game/Bodycam/Characters/Zombie"
DEST_ANIM = DEST_MESH + "/Anims"
SKELETON = DEST_MESH + "/SK_Zombie_Ch10_Skeleton"
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_import_zombies.txt")

_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log("[CLAUDE] " + str(msg))


def task(filename, dest, name, options):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("options", options)
    return t


def words(path):
    base = os.path.splitext(os.path.basename(path))[0].replace("(", "").replace(")", "")
    return "".join(w.capitalize() for w in base.replace("-", " ").split())


def exists(p):
    return unreal.EditorAssetLibrary.does_asset_exist(p)


skel = unreal.load_asset(SKELETON)
log("Squelette: %s" % (skel.get_path_name() if skel else "INTROUVABLE"))
tools = unreal.AssetToolsHelpers.get_asset_tools()

if skel:
    # Personnages
    tasks = []
    for f in sorted(glob.glob(os.path.join(SRC, "Persos", "*.fbx"))):
        short = os.path.basename(f).split()[0]          # "Parasite", "Warzombie"...
        name = "SK_Zombie_" + short
        if exists(DEST_MESH + "/" + short + "/" + name):
            log("deja la: " + name)
            continue
        o = unreal.FbxImportUI()
        o.set_editor_property("automated_import_should_detect_type", False)
        o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        o.set_editor_property("import_mesh", True)
        o.set_editor_property("import_as_skeletal", True)
        o.set_editor_property("import_animations", False)
        o.set_editor_property("import_materials", True)
        o.set_editor_property("import_textures", True)
        o.set_editor_property("create_physics_asset", True)
        o.set_editor_property("skeleton", skel)
        tasks.append(task(f, DEST_MESH + "/" + short, name, o))
    if tasks:
        tools.import_asset_tasks(tasks)

    # Animations
    tasks = []
    for f in sorted(glob.glob(os.path.join(SRC, "Anims", "*.fbx"))):
        name = "A_Z_" + words(f)
        if exists(DEST_ANIM + "/" + name):
            log("deja la: " + name)
            continue
        a = unreal.FbxImportUI()
        a.set_editor_property("automated_import_should_detect_type", False)
        a.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        a.set_editor_property("import_mesh", False)
        a.set_editor_property("import_as_skeletal", True)
        a.set_editor_property("import_animations", True)
        a.set_editor_property("import_materials", False)
        a.set_editor_property("import_textures", False)
        a.set_editor_property("skeleton", skel)
        tasks.append(task(f, DEST_ANIM, name, a))
    if tasks:
        tools.import_asset_tasks(tasks)

for p in sorted(unreal.EditorAssetLibrary.list_assets(DEST_MESH, recursive=True)):
    obj = unreal.load_asset(p)
    if isinstance(obj, (unreal.SkeletalMesh, unreal.AnimSequence, unreal.Skeleton)):
        extra = ""
        if isinstance(obj, unreal.SkeletalMesh):
            b = obj.get_bounds()
            extra = " hauteur=%.0fcm skel=%s" % (b.box_extent.z * 2, obj.skeleton.get_name() if obj.skeleton else "?")
        elif isinstance(obj, unreal.AnimSequence):
            extra = " len=%.2fs" % obj.get_play_length()
        log("  %s [%s]%s" % (p, obj.get_class().get_name(), extra))
log("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(_lines))
