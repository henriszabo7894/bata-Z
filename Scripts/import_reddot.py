# Viseur point rouge (modele telecharge, nettoye sous Blender) : maillage + materiaux corps / vitre.
# Lance par Corriger_Armes.bat. Resultat : Saved/Logs/claude_reddot.txt
import os, traceback
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "Armes", "RedDot"))
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_reddot.txt")
DEST = "/Game/Bodycam/Weapons/RedDot"
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


def tex(role):
    t = import_file(os.path.join(SRC, "T_RedDotSight_%s.png" % role), DEST + "/Textures", "T_RedDotSight_%s" % role)
    if not t:
        log("texture %s ECHEC" % role)
        return None
    if role == "BaseColor":
        t.set_editor_property("srgb", True)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif role == "Normal":
        t.set_editor_property("srgb", False)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("flip_green_channel", True)
    else:
        t.set_editor_property("srgb", False)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    eal.save_loaded_asset(t)
    return t


def new_mi(name, parent):
    p = MAT_DIR + "/" + name
    if eal.does_asset_exist(p):
        eal.delete_asset(p)
    mi = tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
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
    smd.set_editor_property("auto_generate_collision", False)
    mesh = import_file(os.path.join(SRC, "SM_RedDot.fbx"), DEST, "SM_RedDot", o)
    if not mesh:
        log("! ECHEC import SM_RedDot")
        return
    gear = unreal.load_asset(MAT_DIR + "/M_Gear")
    body = new_mi("MI_RedDotSight_Body", gear)
    for role in ("BaseColor", "Normal", "Roughness", "Metallic"):
        t = tex(role)
        if t:
            mel.set_material_instance_texture_parameter_value(body, role, t)
    eal.save_loaded_asset(body)
    glass_parent = unreal.load_asset("/Game/Bodycam/Materials/FX/M_FX_Translucent")
    glass = new_mi("MI_RedDotSight_Glass", glass_parent) if glass_parent else None
    if glass:
        mel.set_material_instance_vector_parameter_value(glass, "Color", unreal.LinearColor(0.03, 0.05, 0.045, 1.0))
        mel.set_material_instance_scalar_parameter_value(glass, "Opacity", 0.18)
        eal.save_loaded_asset(glass)
    mats = []
    for slot in mesh.get_editor_property("static_materials"):
        n = str(slot.get_editor_property("material_slot_name")).lower()
        slot.set_editor_property("material_interface", glass if ("glass" in n and glass) else body)
        mats.append(slot)
        log("slot %s -> %s" % (n, "vitre" if "glass" in n else "corps"))
    mesh.set_editor_property("static_materials", mats)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    log("SM_RedDot taille %.1f x %.1f x %.1f cm" % (b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2))


try:
    run()
except Exception as e:
    log("ERREUR %r\n%s" % (e, traceback.format_exc()))
log("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(lines))
