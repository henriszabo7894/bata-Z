# Import des armes + jumelles de vision nocturne, avec un materiau maitre M_Gear
# (textures BaseColor/Normal/Roughness/Metallic/AO + multiplicateurs).
# Lance par Importer_Armes.bat. Resultat : Saved/Logs/claude_import_armes.txt
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import"))
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_import_armes.txt")
MAT_DIR = "/Game/Bodycam/Materials/Gear"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
_lines = []


def log(m):
    _lines.append(str(m))
    unreal.log("[CLAUDE] " + str(m))


# kind: static | skeletal ; tex: role -> (fichier, type) ; type: color | normal_gl | mask
ITEMS = [
    dict(name="AK47", fbx="Armes/AK47/AK47.fbx", dest="/Game/Bodycam/Weapons/AK47", kind="static",
         tex={"BaseColor": "AK_Base_color.png", "Normal": "AK_Normal_OpenGL.png",
              "Roughness": "AK_Roughness.png", "Metallic": "AK_Metallic.png", "AO": "AK_Mixed_AO.png"}),
    dict(name="Pistol", fbx="Armes/Pistolet/Pistol.fbx", dest="/Game/Bodycam/Weapons/Pistol", kind="static",
         tex={}, tint=(0.035, 0.035, 0.04), rough=0.45, metal=0.7),   # pas de textures fournies
    dict(name="Shotgun", fbx="Armes/Fusil_Pompe/Shotgun.fbx", dest="/Game/Bodycam/Weapons/Shotgun", kind="skeletal",
         tex={"BaseColor": "4k_Shotgun_colour.png", "Normal": "2kNormal_shotgun.png",
              "Roughness": "4kRoughness_shotgun.png", "Metallic": "4k_Shotgun_Metallic.png"}),
    dict(name="NVG", fbx="JVN/JVN.fbx", dest="/Game/Bodycam/Gear/NVG", kind="static",
         tex={"BaseColor": "Binoculars_Albedo.png", "Normal": "Binoculars_Normal.png",
              "Roughness": "Binoculars_Roughness.png", "Metallic": "Binoculars_Metalness.png", "AO": "Binoculars_AO.png"}),
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
    paths = list(t.get_editor_property("imported_object_paths"))
    return paths


def import_texture(path, dest, name, role):
    p = import_file(path, dest, name)
    tex = unreal.load_asset(dest + "/" + name)
    if not tex:
        log("  ! texture non importee: %s" % path)
        return None
    if role == "BaseColor":
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif role == "Normal":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("flip_green_channel", True)   # textures OpenGL (Blender/Substance)
    else:
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("max_texture_size", 2048)        # economie memoire (UHD 770)
    eal.save_loaded_asset(tex)
    return tex


def ensure_defaults():
    d = "/Game/Bodycam/Textures/Defaults"
    out = {}
    for fname, role in (("T_White_Linear.png", "Roughness"), ("T_Flat_Normal.png", "Normal"), ("T_Grey_Color.png", "BaseColor")):
        name = os.path.splitext(fname)[0]
        if eal.does_asset_exist(d + "/" + name):
            out[role] = unreal.load_asset(d + "/" + name)
        else:
            out[role] = import_texture(os.path.join(SRC, "Defaults", fname), d, name, role)
            if role == "Normal" and out[role]:
                out[role].set_editor_property("flip_green_channel", False)
                eal.save_loaded_asset(out[role])
    return out


def build_master(defaults):
    path = MAT_DIR + "/M_Gear"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_Gear", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    for flag in ("used_with_skeletal_mesh", "used_with_static_lighting", "used_with_nanite",
                 "used_with_instanced_static_meshes"):
        try:
            m.set_editor_property(flag, True)
        except Exception as e:
            log("  flag %s: %r" % (flag, e))

    def texp(name, y, sampler, default):
        n = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("sampler_type", sampler)
        n.set_editor_property("texture", default)
        return n

    def scal(name, y, val):
        n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", val)
        return n

    def mul(a, ao, b, bo, y):
        n = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, y)
        mel.connect_material_expressions(a, ao, n, "A")
        mel.connect_material_expressions(b, bo, n, "B")
        return n

    MS = unreal.MaterialSamplerType
    bc = texp("BaseColor", -400, MS.SAMPLERTYPE_COLOR, defaults["BaseColor"])
    nm = texp("Normal", -150, MS.SAMPLERTYPE_NORMAL, defaults["Normal"])
    ro = texp("Roughness", 100, MS.SAMPLERTYPE_MASKS, defaults["Roughness"])
    me = texp("Metallic", 350, MS.SAMPLERTYPE_MASKS, defaults["Roughness"])
    ao = texp("AO", 600, MS.SAMPLERTYPE_MASKS, defaults["Roughness"])
    tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, -500)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))

    mel.connect_material_property(mul(bc, "RGB", tint, "", -400), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(mul(ro, "R", scal("RoughnessMul", 100, 1.0), "", 100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(mul(me, "R", scal("MetallicMul", 350, 1.0), "", 350), "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def import_mesh(item):
    fbx = os.path.join(SRC, item["fbx"])
    o = unreal.FbxImportUI()
    o.set_editor_property("automated_import_should_detect_type", False)
    o.set_editor_property("import_materials", True)
    o.set_editor_property("import_textures", False)
    o.set_editor_property("import_animations", item["kind"] == "skeletal")
    if item["kind"] == "static":
        o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        o.set_editor_property("import_as_skeletal", False)
        sd = o.get_editor_property("static_mesh_import_data")
        sd.set_editor_property("combine_meshes", True)
        sd.set_editor_property("generate_lightmap_u_vs", False)
        sd.set_editor_property("auto_generate_collision", False)
        try:
            sd.set_editor_property("build_nanite", True)
        except Exception as e:
            log("  build_nanite: %r" % e)
    else:
        o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        o.set_editor_property("import_as_skeletal", True)
        o.set_editor_property("create_physics_asset", False)
    name = ("SM_" if item["kind"] == "static" else "SK_") + item["name"]
    import_file(fbx, item["dest"], name, o)
    return unreal.load_asset(item["dest"] + "/" + name), name


def run():
    defaults = ensure_defaults()
    master = build_master(defaults)
    log("Materiau maitre: %s" % master.get_path_name())

    for it in ITEMS:
        log("=== %s" % it["name"])
        mesh, name = import_mesh(it)
        if not mesh:
            log("  ! ECHEC import maillage")
            continue
        # Instance de materiau
        mi_name = "MI_" + it["name"]
        if eal.does_asset_exist(MAT_DIR + "/" + mi_name):
            eal.delete_asset(MAT_DIR + "/" + mi_name)
        mi = tools.create_asset(mi_name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        tex_dir = it["dest"] + "/Textures"
        folder = os.path.dirname(os.path.join(SRC, it["fbx"]))
        for role, fname in it["tex"].items():
            t = import_texture(os.path.join(folder, fname), tex_dir, "T_%s_%s" % (it["name"], role), role)
            if t:
                mel.set_material_instance_texture_parameter_value(mi, role, t)
        if "tint" in it:
            mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*it["tint"], 1))
        if "rough" in it:
            mel.set_material_instance_scalar_parameter_value(mi, "RoughnessMul", it["rough"])
        if "metal" in it:
            mel.set_material_instance_scalar_parameter_value(mi, "MetallicMul", it["metal"])
        eal.save_loaded_asset(mi)

        # Affecte l'instance a tous les emplacements
        if isinstance(mesh, unreal.StaticMesh):
            n = len(mesh.get_editor_property("static_materials"))
            for i in range(n):
                mesh.set_material(i, mi)
            tris = mesh.get_num_triangles(0) if hasattr(mesh, "get_num_triangles") else -1
            b = mesh.get_bounding_box()
            log("  StaticMesh %s : %d slot(s), tris LOD0=%s, taille=%.1f x %.1f x %.1f cm" % (
                name, n, tris, b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z))
            try:
                ns = mesh.get_editor_property("nanite_settings")
                log("  Nanite: %s" % ns.get_editor_property("enabled"))
            except Exception:
                pass
        else:
            mats = mesh.get_editor_property("materials")
            new = []
            for sm in mats:
                sm.set_editor_property("material_interface", mi)
                new.append(sm)
            mesh.set_editor_property("materials", new)
            b = mesh.get_bounds()
            skel = mesh.get_editor_property("skeleton")
            log("  SkeletalMesh %s : %d slot(s), taille=%.1f x %.1f x %.1f cm" % (
                name, len(new), b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2))
            if skel:
                refs = mesh.get_editor_property("ref_skeleton") if False else None
                bones = [str(mesh.get_bone_name(i)) for i in range(mesh.get_num_bones())] if hasattr(mesh, "get_num_bones") else []
                log("  Os: %s" % ", ".join(bones))
        eal.save_loaded_asset(mesh)

    for d in ("/Game/Bodycam/Weapons", "/Game/Bodycam/Gear"):
        for p in sorted(eal.list_assets(d, recursive=True)):
            log("  " + p)


try:
    run()
except Exception as e:
    import traceback
    log("ERREUR: %r\n%s" % (e, traceback.format_exc()))
log("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(_lines))
