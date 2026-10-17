# Caisse de munitions (pack JMFeria AmmoCrates, modele 01) : maillage + 3 materiaux (caisse, poignee, loquet).
# Lance par Importer_Caisse.bat. Resultat : Saved/Logs/claude_ammocrate.txt
import os, traceback
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "Props", "AmmoCrate"))
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_ammocrate.txt")
DEST = "/Game/Bodycam/Props/AmmoCrate"
MAT_DIR = "/Game/Bodycam/Materials/Gear"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lines = []


def log(m):
    lines.append(str(m))


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
    return unreal.load_asset(dest + "/" + name)


def tex(fname, role):
    name = "T_" + os.path.splitext(fname)[0]
    t = import_file(os.path.join(SRC, fname), DEST + "/Textures", name)
    if not t:
        log("texture %s ECHEC" % fname)
        return None
    if role == "BaseColor":
        t.set_editor_property("srgb", True)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif role == "Normal":
        t.set_editor_property("srgb", False)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    else:
        t.set_editor_property("srgb", False)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    t.set_editor_property("max_texture_size", 1024)
    eal.save_loaded_asset(t)
    return t


SETS = {
    "crate": "AmmoCrate01_Crate_%s.png",
    "handle": "AmmoCrate01_Handle_%s.png",
    "padlock": ("AmmoCrate01_Padlocks_%s.png", "AmmoCrate01_Padlock_%s.png"),
}


def make_mi(key, master):
    name = "MI_AmmoCrate_" + key.capitalize()
    p = MAT_DIR + "/" + name
    if eal.does_asset_exist(p):
        eal.delete_asset(p)
    mi = tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    pat = SETS[key]
    for role in ("BaseColor", "Normal", "Roughness", "Metallic"):
        if isinstance(pat, tuple):
            f = (pat[0] if role in ("BaseColor", "Normal") else pat[1]) % role
        else:
            f = pat % role
        t = tex(f, role)
        if t:
            mel.set_material_instance_texture_parameter_value(mi, role, t)
    eal.save_loaded_asset(mi)
    return mi


def run():
    o = unreal.FbxImportUI()
    o.set_editor_property("automated_import_should_detect_type", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property("import_as_skeletal", False)
    o.set_editor_property("import_materials", False)
    o.set_editor_property("import_textures", False)
    o.set_editor_property("import_animations", False)
    smd = o.get_editor_property("static_mesh_import_data")
    smd.set_editor_property("combine_meshes", True)
    smd.set_editor_property("auto_generate_collision", True)
    mesh = import_file(os.path.join(SRC, "AmmoCrate01.fbx"), DEST, "SM_AmmoCrate", o)
    if not mesh:
        log("! ECHEC import SM_AmmoCrate")
        return
    master = unreal.load_asset(MAT_DIR + "/M_Gear")
    mis = {k: make_mi(k, master) for k in SETS}
    mats = []
    for slot in mesh.get_editor_property("static_materials"):
        n = str(slot.get_editor_property("material_slot_name")).lower()
        k = "handle" if "escaleras" in n else "padlock" if "columnas" in n else "crate"
        slot.set_editor_property("material_interface", mis[k])
        mats.append(slot)
        log("slot %s -> %s" % (n, k))
    mesh.set_editor_property("static_materials", mats)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    log("SM_AmmoCrate taille %.1f x %.1f x %.1f cm" % (b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2))


try:
    run()
except Exception as e:
    log("ERREUR %r\n%s" % (e, traceback.format_exc()))
log("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(lines))
