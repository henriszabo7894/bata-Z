# Import des armes FPS (bras gantes + arme + animations) : M4 et Glock 17.
# Chaque FBX contient le squelette, les bras, l'arme (chargeurs, culasse, gachette en pieces separees)
# et 6 animations : Reload, Reload vide, Fire, Draw, Holster, Idle.
# Les textures sont importees ici et les materiaux crees a partir du materiau maitre M_Gear.
# Lance par Importer_Armes_FPS.bat. Resultat : Saved/Logs/claude_import_fps.txt
import os
import traceback
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "Armes"))
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_import_fps.txt")
MAT_DIR = "/Game/Bodycam/Materials/Gear"
TEX_COMMON = "/Game/Bodycam/Weapons/FPS/Textures"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
_lines = []


def log(m):
    _lines.append(str(m))
    unreal.log("[CLAUDE] " + str(m))


def flush():
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(_lines))


# Textures partagees (les deux armes ont les memes bras gantes)
COMMON = {
    "hand":  dict(tex={"BaseColor": "FPS_Mains/Hand_D.jpg", "Normal": "FPS_Mains/Hand_N.png"}, rough=0.7, metal=0.0),
    "glove": dict(tex={"BaseColor": "FPS_Mains/Glove_D.png", "Normal": "FPS_Mains/Glove_N.png"}, rough=0.8, metal=0.0),
}

RIGS = [
    dict(name="M4", fbx="M4/M4.fbx", dest="/Game/Bodycam/Weapons/M4",
         gun=dict(tex={"BaseColor": "M4/M4_D.png", "Normal": "M4/M4_N.png"}, rough=0.5, metal=0.6),
         bullet=dict(tex={"BaseColor": "M4/Bullet5.56x45mm_D.png", "Normal": "M4/Bullet5.56x45mm_N.png"}, rough=0.35, metal=1.0)),
    dict(name="Glock17", fbx="Glock17/glock17.fbx", dest="/Game/Bodycam/Weapons/Glock17",
         gun=dict(tex={"BaseColor": "Glock17/Glock_D.tga"}, rough=0.45, metal=0.3),
         bullet=dict(tex={"BaseColor": "Glock17/Bullet_9mm_D.tga"}, rough=0.35, metal=1.0)),
]


def import_file(filename, dest, name, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    if options:
        t.set_editor_property("options", options)
    tools.import_asset_tasks([t])


def import_texture(rel_path, dest, name, role):
    full = os.path.join(SRC, rel_path)
    if not os.path.exists(full):
        log("  ! fichier introuvable: %s" % full)
        return None
    if eal.does_asset_exist(dest + "/" + name):
        return unreal.load_asset(dest + "/" + name)
    import_file(full, dest, name)
    tex = unreal.load_asset(dest + "/" + name)
    if not tex:
        log("  ! texture non importee: %s" % rel_path)
        return None
    if role == "BaseColor":
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif role == "Normal":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("flip_green_channel", True)   # normales au format OpenGL (pack venant d'Unity)
    tex.set_editor_property("max_texture_size", 2048)        # economie de memoire (UHD 770)
    eal.save_loaded_asset(tex)
    return tex


def make_instance(mi_name, spec, tex_dest, master, prefix):
    path = MAT_DIR + "/" + mi_name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = tools.create_asset(mi_name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    for role, rel in spec["tex"].items():
        t = import_texture(rel, tex_dest, "T_%s_%s" % (prefix, role), role)
        if t:
            mel.set_material_instance_texture_parameter_value(mi, role, t)
    mel.set_material_instance_scalar_parameter_value(mi, "RoughnessMul", spec["rough"])
    mel.set_material_instance_scalar_parameter_value(mi, "MetallicMul", spec["metal"])
    eal.save_loaded_asset(mi)
    return mi


def import_rig(rig, master, common_mis):
    log("=== %s" % rig["name"])
    fbx = os.path.join(SRC, rig["fbx"])
    if not os.path.exists(fbx):
        log("  ! FBX introuvable: %s" % fbx)
        return
    o = unreal.FbxImportUI()
    o.set_editor_property("automated_import_should_detect_type", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    o.set_editor_property("import_as_skeletal", True)
    o.set_editor_property("import_mesh", True)
    o.set_editor_property("import_materials", False)
    o.set_editor_property("import_textures", False)
    o.set_editor_property("import_animations", True)
    o.set_editor_property("create_physics_asset", False)
    try:
        o.get_editor_property("skeletal_mesh_import_data").set_editor_property("import_meshes_in_bone_hierarchy", True)
    except Exception as e:
        log("  import_meshes_in_bone_hierarchy: %r" % e)
    name = "SK_" + rig["name"]
    import_file(fbx, rig["dest"], name, o)
    mesh = unreal.load_asset(rig["dest"] + "/" + name)
    if not mesh:
        log("  ! ECHEC import du maillage")
        return

    gun = make_instance("MI_%s_Gun" % rig["name"], rig["gun"], rig["dest"] + "/Textures", master, rig["name"] + "_Gun")
    bullet = make_instance("MI_%s_Bullet" % rig["name"], rig["bullet"], rig["dest"] + "/Textures", master, rig["name"] + "_Bullet")

    new = []
    for slot in mesh.get_editor_property("materials"):
        sname = str(slot.get_editor_property("material_slot_name")).lower()
        if "hand" in sname:
            mi, kind = common_mis["hand"], "mains"
        elif "glove" in sname:
            mi, kind = common_mis["glove"], "gants"
        elif "bullet" in sname:
            mi, kind = bullet, "balle"
        else:
            mi, kind = gun, "arme"
        slot.set_editor_property("material_interface", mi)
        new.append(slot)
        log("  emplacement '%s' -> %s" % (sname, kind))
    mesh.set_editor_property("materials", new)
    eal.save_loaded_asset(mesh)

    b = mesh.get_bounds()
    log("  SkeletalMesh %s : %d emplacement(s), taille=%.1f x %.1f x %.1f cm" % (
        name, len(new), b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2))
    n_anim = 0
    for p in sorted(eal.list_assets(rig["dest"], recursive=True)):
        obj = unreal.load_asset(p)
        cls = obj.get_class().get_name() if obj else "?"
        extra = ""
        if isinstance(obj, unreal.AnimSequence):
            n_anim += 1
            extra = " duree=%.2fs" % obj.get_play_length()
        log("  %s [%s]%s" % (p, cls, extra))
    log("  %d animation(s) importee(s)" % n_anim)


def run():
    master_path = MAT_DIR + "/M_Gear"
    master = unreal.load_asset(master_path)
    if not master:
        log("! Materiau maitre M_Gear introuvable (%s) : lance d'abord Importer_Armes.bat" % master_path)
        return
    common_mis = {}
    for key, spec in COMMON.items():
        common_mis[key] = make_instance("MI_FPS_" + key.capitalize(), spec, TEX_COMMON, master, "FPS_" + key.capitalize())
    for rig in RIGS:
        try:
            import_rig(rig, master, common_mis)
        except Exception as e:
            log("ERREUR %s: %r\n%s" % (rig["name"], e, traceback.format_exc()))
        flush()


try:
    run()
except Exception as e:
    log("ERREUR: %r\n%s" % (e, traceback.format_exc()))
log("FIN")
flush()
