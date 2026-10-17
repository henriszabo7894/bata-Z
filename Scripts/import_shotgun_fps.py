# Import du fusil a pompe FPS (bras + Remington + animations) nettoye sous Blender.
# Lance par Importer_Fusil_FPS.bat. Resultat : Saved/Logs/claude_import_shotgun.txt
import os
import traceback
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "Armes", "Shotgun"))
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_import_shotgun.txt")
MAT_DIR = "/Game/Bodycam/Materials/Gear"
DEST = "/Game/Bodycam/Weapons/Shotgun12"

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


def import_texture(fname, name, role):
    full = os.path.join(SRC, fname)
    if not os.path.exists(full):
        log("  ! fichier introuvable: %s" % full)
        return None
    d = DEST + "/Textures"
    import_file(full, d, name)
    tex = unreal.load_asset(d + "/" + name)
    if not tex:
        log("  ! texture non importee: %s" % fname)
        return None
    if role == "BaseColor":
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif role == "Normal":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("flip_green_channel", True)   # normales OpenGL (Blender / Substance)
    else:
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("max_texture_size", 2048)
    eal.save_loaded_asset(tex)
    return tex


def make_instance(mi_name, master, files, rough, metal, tint=None):
    path = MAT_DIR + "/" + mi_name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = tools.create_asset(mi_name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    for role, fname in files.items():
        t = import_texture(fname, "T_%s_%s" % (mi_name[3:], role), role)
        if t:
            mel.set_material_instance_texture_parameter_value(mi, role, t)
    mel.set_material_instance_scalar_parameter_value(mi, "RoughnessMul", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "MetallicMul", metal)
    if tint:
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint))
    eal.save_loaded_asset(mi)
    return mi


ANIM_NAMES = [  # (fin du nom, nom final) : du plus long au plus court
    ("reloadstart", "ReloadStart"), ("reloadend", "ReloadEnd"), ("reload", "ReloadLoop"),
    ("shoot", "Fire"), ("take", "Draw"), ("hide", "Holster"), ("watch", "Inspect"),
]


def run():
    master = unreal.load_asset(MAT_DIR + "/M_Gear")
    if not master:
        log("! M_Gear introuvable : lance d'abord Importer_Armes.bat")
        return

    fbx = os.path.join(SRC, "Shotgun12.fbx")
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
    import_file(fbx, DEST, "SK_Shotgun12", o)
    mesh = unreal.load_asset(DEST + "/SK_Shotgun12")
    if not mesh:
        log("! ECHEC import du maillage")
        return

    mis = {
        "gun": make_instance("MI_SG12_Gun", master, {"BaseColor": "Material.001_Base_color.png", "Normal": "Material.001_Normal_OpenGL.png",
                             "Roughness": "Material.001_Roughness.png", "Metallic": "Material.001_Metallic.png", "AO": "Material.001_Mixed_AO.png"}, 1.0, 1.0),
        "shell": make_instance("MI_SG12_Shell", master, {"BaseColor": "Material.005_Base_color.png", "Normal": "Material.005_Normal_OpenGL.png",
                               "Roughness": "Material.005_Roughness.png", "Metallic": "Material.005_Metallic.png", "AO": "Material.005_Mixed_AO.png"}, 1.0, 1.0),
        "glove": make_instance("MI_SG12_Glove", master, {"BaseColor": "glove_hardknuckle_Base_color.png", "Normal": "glove_hardknuckle_Normal_OpenGL.png"}, 0.8, 0.0),
        "sleeve": make_instance("MI_SG12_Sleeve", master, {"BaseColor": "sleeve_st6_generalist_Base_color.png", "Normal": "sleeve_st6_generalist_Normal_OpenGL.png"}, 0.9, 0.0),
        "skin": make_instance("MI_SG12_Skin", master, {}, 0.6, 0.0, tint=(0.38, 0.27, 0.22, 1.0)),
    }
    new = []
    for slot in mesh.get_editor_property("materials"):
        sname = str(slot.get_editor_property("material_slot_name")).lower()
        if "001" in sname:
            k = "gun"
        elif "005" in sname:
            k = "shell"
        elif "glove" in sname:
            k = "glove"
        elif "sleeve" in sname:
            k = "sleeve"
        else:
            k = "skin"
        slot.set_editor_property("material_interface", mis[k])
        new.append(slot)
        log("  emplacement '%s' -> %s" % (sname, k))
    mesh.set_editor_property("materials", new)
    eal.save_loaded_asset(mesh)

    b = mesh.get_bounds()
    log("SkeletalMesh : taille=%.1f x %.1f x %.1f cm, centre=%s" % (b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2, b.origin))

    # Noms d'animations stables pour le code : SG_Anim_Fire, SG_Anim_ReloadStart...
    for p in sorted(eal.list_assets(DEST, recursive=False)):
        obj = unreal.load_asset(p)
        if isinstance(obj, unreal.AnimSequence):
            base = p.split("/")[-1].split(".")[0]
            low = base.lower()
            final = None
            for key, nm in ANIM_NAMES:
                if low.endswith(key):
                    final = nm
                    break
            if final and not base.startswith("SG_Anim_"):
                newp = DEST + "/SG_Anim_" + final
                if eal.does_asset_exist(newp):
                    eal.delete_asset(newp)
                ok = eal.rename_asset(p.split(".")[0], newp)
                log("  anim %s -> SG_Anim_%s (%s)" % (base, final, "ok" if ok else "ECHEC"))
            elif not final:
                log("  anim non reconnue : %s" % base)
    n = 0
    for p in sorted(eal.list_assets(DEST, recursive=False)):
        obj = unreal.load_asset(p)
        if isinstance(obj, unreal.AnimSequence):
            n += 1
            log("  %s duree=%.2fs" % (p, obj.get_play_length()))
    log("%d animation(s)" % n)


try:
    run()
except Exception as e:
    log("ERREUR: %r\n%s" % (e, traceback.format_exc()))
log("FIN")
flush()
