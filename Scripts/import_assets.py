# Import des zombies Mixamo + audit de la map Derelict Corridor.
# Lance par Importer_Assets.bat (UnrealEditor-Cmd -run=pythonscript).
import os
import glob
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ZOMBIE_SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "Zombies"))
DEST_MESH = "/Game/Bodycam/Characters/Zombie"
DEST_ANIM = "/Game/Bodycam/Characters/Zombie/Anims"
MAP_PATH = "/Game/DerelictCorridor/Maps/DerelictCorridor"
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_import.txt")

_lines = []


def log(msg):
    msg = str(msg)
    _lines.append(msg)
    unreal.log("[CLAUDE] " + msg)


def flush():
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(_lines))


def clean_name(path):
    base = os.path.splitext(os.path.basename(path))[0]
    base = base.replace("(", "").replace(")", "").strip()
    return "A_Z_" + "".join(w.capitalize() for w in base.replace("-", " ").split())


def make_task(filename, dest, name, options):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("options", options)
    return t


def import_zombies():
    fbx = glob.glob(os.path.join(ZOMBIE_SRC, "**", "*.fbx"), recursive=True)
    log("FBX trouves: %d dans %s" % (len(fbx), ZOMBIE_SRC))
    if not fbx:
        return
    fbx.sort(key=os.path.getsize, reverse=True)
    mesh_file, anim_files = fbx[0], fbx[1:]
    log("Maillage: %s (%.1f Mo)" % (os.path.basename(mesh_file), os.path.getsize(mesh_file) / 1e6))

    tools = unreal.AssetToolsHelpers.get_asset_tools()

    # 1) Personnage (maillage + squelette + physique + materiaux)
    o = unreal.FbxImportUI()
    o.set_editor_property("automated_import_should_detect_type", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    o.set_editor_property("import_mesh", True)
    o.set_editor_property("import_as_skeletal", True)
    o.set_editor_property("import_animations", False)
    o.set_editor_property("import_materials", True)
    o.set_editor_property("import_textures", True)
    o.set_editor_property("create_physics_asset", True)
    tools.import_asset_tasks([make_task(mesh_file, DEST_MESH, "SK_Zombie_Ch10", o)])

    skel = unreal.load_asset(DEST_MESH + "/SK_Zombie_Ch10_Skeleton")
    if not skel:
        # nom alternatif selon la version
        for a in unreal.EditorAssetLibrary.list_assets(DEST_MESH, recursive=False):
            obj = unreal.load_asset(a)
            if isinstance(obj, unreal.Skeleton):
                skel = obj
                break
    log("Squelette: %s" % (skel.get_path_name() if skel else "INTROUVABLE"))
    if not skel:
        return

    # 2) Animations (sans skin, sur le squelette du zombie)
    tasks = []
    for f in anim_files:
        a = unreal.FbxImportUI()
        a.set_editor_property("automated_import_should_detect_type", False)
        a.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        a.set_editor_property("import_mesh", False)
        a.set_editor_property("import_as_skeletal", True)
        a.set_editor_property("import_animations", True)
        a.set_editor_property("import_materials", False)
        a.set_editor_property("import_textures", False)
        a.set_editor_property("skeleton", skel)
        tasks.append(make_task(f, DEST_ANIM, clean_name(f), a))
    tools.import_asset_tasks(tasks)

    for a in sorted(unreal.EditorAssetLibrary.list_assets(DEST_MESH, recursive=True)):
        obj = unreal.load_asset(a)
        extra = ""
        if isinstance(obj, unreal.AnimSequence):
            extra = " len=%.2fs" % obj.get_play_length()
        log("  %s [%s]%s" % (a, obj.get_class().get_name() if obj else "?", extra))


def audit_map():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = reg.get_assets_by_path("/Game/DerelictCorridor", recursive=True)
    counts = {}
    tri_total, nanite_on, sm = 0, 0, 0
    biggest = []
    for ad in assets:
        cls = str(ad.asset_class_path.asset_name)
        counts[cls] = counts.get(cls, 0) + 1
        if cls == "StaticMesh":
            sm += 1
            t = ad.get_tag_value("Triangles")
            try:
                n = int(str(t).replace(",", ""))
            except Exception:
                n = 0
            tri_total += n
            biggest.append((n, str(ad.package_name)))
            if str(ad.get_tag_value("NaniteEnabled")).lower() == "true":
                nanite_on += 1
    log("--- Assets /Game/DerelictCorridor ---")
    for k in sorted(counts, key=lambda k: -counts[k]):
        log("  %s: %d" % (k, counts[k]))
    log("StaticMesh: %d, Nanite actif: %d, triangles (LOD0/fallback) cumules: %d" % (sm, nanite_on, tri_total))
    biggest.sort(reverse=True)
    for n, p in biggest[:15]:
        log("  %9d tris  %s" % (n, p))
    if assets:
        smd = [a for a in assets if str(a.asset_class_path.asset_name) == "StaticMesh"]
        if smd:
            log("Tags exemple: %s" % ", ".join(str(k) for k in smd[0].tags_and_values.keys()) if hasattr(smd[0], "tags_and_values") else "")

    log("--- Carte %s ---" % MAP_PATH)
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
        log("Carte chargee: %s" % (world.get_name() if world else "None"))
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        log("Acteurs World Partition: %d" % len(descs))
        by_class = {}
        notable = []
        for d in descs:
            txt = d.export_text()
            cname = str(d.get_editor_property("native_class").get_name()) if d.get_editor_property("native_class") else "?"
            by_class[cname] = by_class.get(cname, 0) + 1
            if any(k in cname for k in ("Light", "PlayerStart", "PostProcess", "Fog", "Sky", "Sequence", "Camera", "Volume", "Reflection", "DataLayer")):
                b = d.get_editor_property("bounds")
                notable.append("%s | %s | %s" % (cname, d.get_editor_property("label"), b.get_center() if hasattr(b, "get_center") else txt[:120]))
        for k in sorted(by_class, key=lambda k: -by_class[k]):
            log("  %s: %d" % (k, by_class[k]))
        log("--- Acteurs notables ---")
        for n in notable[:200]:
            log("  " + n)
        # Bornes globales
        mn, mx = None, None
        for d in descs:
            b = d.get_editor_property("bounds")
            try:
                lo, hi = b.min, b.max
            except Exception:
                continue
            mn = lo if mn is None else unreal.Vector(min(mn.x, lo.x), min(mn.y, lo.y), min(mn.z, lo.z))
            mx = hi if mx is None else unreal.Vector(max(mx.x, hi.x), max(mx.y, hi.y), max(mx.z, hi.z))
        log("Bornes carte: min=%s max=%s" % (mn, mx))
    except Exception as e:
        log("ERREUR audit carte: %r" % e)


try:
    import_zombies()
except Exception as e:
    log("ERREUR import zombies: %r" % e)
try:
    audit_map()
except Exception as e:
    log("ERREUR audit: %r" % e)
log("FIN")
flush()
