# Flash de bouche realiste (textures du pack "turret gun flash") : M_GunFlash additif non eclaire.
# Lance par Corriger_Armes.bat. Resultat : Saved/Logs/claude_gunflash.txt
import os, traceback
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "FX"))
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_gunflash.txt")
DEST = "/Game/Bodycam/Materials/FX"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lines = []


def log(m):
    lines.append(str(m))


def import_tex(fname, name):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(SRC, fname))
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    tools.import_asset_tasks([t])
    tex = unreal.load_asset(DEST + "/" + name)
    if tex:
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        eal.save_loaded_asset(tex)
    log("texture %s : %s" % (name, "ok" if tex else "ECHEC"))
    return tex


def run():
    front = import_tex("T_Flash_Front.png", "T_Flash_Front")
    reddot = import_tex("T_RedDot.png", "T_RedDot")
    side = import_tex("T_Flash_Side.png", "T_Flash_Side")
    path = DEST + "/M_GunFlash"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_GunFlash", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property("parameter_name", "Flash")
    tex.set_editor_property("texture", front)
    br = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 300)
    br.set_editor_property("parameter_name", "Brightness")
    br.set_editor_property("default_value", 4.0)
    a = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 0)
    mel.connect_material_expressions(tex, "RGB", a, "A")
    mel.connect_material_expressions(tex, "A", a, "B")
    b = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 0)
    mel.connect_material_expressions(a, "", b, "A")
    mel.connect_material_expressions(br, "", b, "B")
    mel.connect_material_property(b, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("M_GunFlash cree")
    for n, t in (("MI_GunFlash_Front", front), ("MI_GunFlash_Side", side), ("MI_RedDot", reddot)):
        p = DEST + "/" + n
        if eal.does_asset_exist(p):
            eal.delete_asset(p)
        mi = tools.create_asset(n, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, m)
        if t:
            mel.set_material_instance_texture_parameter_value(mi, "Flash", t)
        eal.save_loaded_asset(mi)
        log("%s cree" % n)


def simple_material(name, blend, with_opacity):
    path = DEST + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", blend)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    try:
        m.set_editor_property("used_with_skeletal_mesh", True)
    except Exception as e:
        log("skel flag: %r" % e)
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, 0)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    br = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 250)
    br.set_editor_property("parameter_name", "Brightness")
    br.set_editor_property("default_value", 1.0)
    mu = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 0)
    mel.connect_material_expressions(col, "", mu, "A")
    mel.connect_material_expressions(br, "", mu, "B")
    mel.connect_material_property(mu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if with_opacity:
        op = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 450)
        op.set_editor_property("parameter_name", "Opacity")
        op.set_editor_property("default_value", 1.0)
        mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("%s cree" % name)
    return m


def instance(name, parent, color, brightness, opacity=None):
    p = DEST + "/" + name
    if eal.does_asset_exist(p):
        eal.delete_asset(p)
    mi = tools.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
    mel.set_material_instance_scalar_parameter_value(mi, "Brightness", brightness)
    if opacity is not None:
        mel.set_material_instance_scalar_parameter_value(mi, "Opacity", opacity)
    eal.save_loaded_asset(mi)
    log("%s cree" % name)


def run_fx():
    add = simple_material("M_FX_Additive", unreal.BlendMode.BLEND_ADDITIVE, False)
    tr = simple_material("M_FX_Translucent", unreal.BlendMode.BLEND_TRANSLUCENT, True)
    instance("MI_Tracer", add, (1.0, 0.55, 0.2, 1.0), 6.0)
    instance("MI_Invisible", add, (0.0, 0.0, 0.0, 1.0), 0.0)
    instance("MI_BloodFX", tr, (0.3, 0.008, 0.005, 1.0), 1.0, 0.97)
    unl = simple_material("M_FX_Unlit", unreal.BlendMode.BLEND_OPAQUE, False)
    for flag in ("used_with_instanced_static_meshes", "used_with_nanite", "used_with_static_lighting"):
        try:
            unl.set_editor_property(flag, True)
        except Exception as e:
            log("flag %s: %r" % (flag, e))
    mel.recompile_material(unl)
    eal.save_loaded_asset(unl)
    instance("MI_NightBlack", unl, (0.0, 0.0, 0.0, 1.0), 0.0)


try:
    run()
    run_fx()
except Exception as e:
    log("ERREUR %r\n%s" % (e, traceback.format_exc()))
log("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(lines))
