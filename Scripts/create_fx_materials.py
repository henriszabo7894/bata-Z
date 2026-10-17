# Materiaux d'effets : flash de bouche, decalcomanie de sang, impact de balle.
# + mise a jour du materiau de vision nocturne (grain plus fin, image moins claire).
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_fx.txt")
DIR = "/Game/Bodycam/Materials/FX"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
_l = []


def log(m):
    _l.append(str(m))
    unreal.log("[CLAUDE] " + str(m))


def new_mat(name, folder=DIR):
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def custom(m, code, inputs, out_type, x=-300, y=0):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    ins = []
    for name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c


def scalar(m, name, val, x=-600, y=300):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", val)
    return n


def set_enum(obj, prop, enum_cls, *names):
    for n in names:
        if hasattr(enum_cls, n):
            obj.set_editor_property(prop, getattr(enum_cls, n))
            return True
    log("  ! enum introuvable %s %s" % (prop, names))
    return False


def finish(m):
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("OK " + m.get_path_name())


# ---------------------------------------------------------------- Flash de bouche
FLASH = r"""
float2 p = UV - 0.5;
float r = length(p) * 2.0;
float a = atan2(p.y, p.x);
float core = pow(saturate(1.0 - r), 3.0);
float spikes = pow(abs(cos(a * 2.5 + Seed * 6.28)), 18.0) * saturate(1.0 - r * 0.9);
float spikes2 = pow(abs(cos(a * 4.0 + Seed * 3.1)), 30.0) * saturate(1.0 - r * 1.3) * 0.6;
float v = saturate(core * 1.6 + spikes + spikes2);
return float3(1.0, 0.62, 0.28) * v * Intensity;
"""


def build_flash():
    m = new_mat("M_MuzzleFlash")
    set_enum(m, "blend_mode", unreal.BlendMode, "BLEND_ADDITIVE")
    set_enum(m, "shading_model", unreal.MaterialShadingModel, "MSM_UNLIT")
    m.set_editor_property("two_sided", True)
    tc = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    c = custom(m, FLASH, ["UV", "Seed", "Intensity"], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    mel.connect_material_expressions(tc, "", c, "UV")
    mel.connect_material_expressions(scalar(m, "Seed", 0.0, -600, 200), "", c, "Seed")
    mel.connect_material_expressions(scalar(m, "Intensity", 12.0, -600, 300), "", c, "Intensity")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


# ---------------------------------------------------------------- Décalcomanies
SPLAT = r"""
float2 p = (UV - 0.5) * 2.0;
float r = length(p);
float a = atan2(p.y, p.x);
float edge = 0.55 + 0.18 * sin(a * 5.0 + Seed * 11.0) + 0.1 * sin(a * 11.0 + Seed * 3.0) + 0.06 * sin(a * 23.0 + Seed * 7.0);
float blob = 1.0 - smoothstep(edge - 0.08, edge, r);
float2 q = p * 3.0 + Seed * 5.0;
float drops = step(0.82, frac(sin(dot(floor(q), float2(12.9898, 78.233))) * 43758.5453)) * (1.0 - smoothstep(0.2, 0.3, length(frac(q) - 0.5))) * step(r, 0.95);
return saturate(blob + drops) * Opacity;
"""

HOLE = r"""
float r = length((UV - 0.5) * 2.0);
float hole = 1.0 - smoothstep(0.18, 0.24, r);
float soot = (1.0 - smoothstep(0.2, 1.0, r)) * 0.55;
return saturate(hole + soot);
"""


def build_decal(name, code, color, rough, inputs):
    m = new_mat(name)
    set_enum(m, "material_domain", unreal.MaterialDomain, "MD_DEFERRED_DECAL")
    set_enum(m, "blend_mode", unreal.BlendMode, "BLEND_TRANSLUCENT")
    tc = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    c = custom(m, code, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    mel.connect_material_expressions(tc, "", c, "UV")
    if "Seed" in inputs:
        mel.connect_material_expressions(scalar(m, "Seed", 0.3, -600, 200), "", c, "Seed")
    if "Opacity" in inputs:
        mel.connect_material_expressions(scalar(m, "Opacity", 0.95, -600, 300), "", c, "Opacity")
    col = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, -300)
    col.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rg = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, -150)
    rg.set_editor_property("r", rough)
    mel.connect_material_property(rg, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


for fn in (build_flash,
           lambda: build_decal("M_Decal_Blood", SPLAT, (0.09, 0.004, 0.003), 0.25, ["UV", "Seed", "Opacity"]),
           lambda: build_decal("M_Decal_BulletHole", HOLE, (0.02, 0.018, 0.016), 0.9, ["UV"])):
    try:
        fn()
    except Exception as e:
        import traceback
        log("ERREUR: %r\n%s" % (e, traceback.format_exc()))

log("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(_l))
