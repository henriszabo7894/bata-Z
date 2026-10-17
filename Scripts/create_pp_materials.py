# Cree les materiaux post-process du jeu :
#  M_PP_NVG       : vision nocturne militaire (phosphore blanc, double tube, grain, scintillement)
#  M_PP_Infection : contamination (vision double, flou, desaturation, pulsation rouge)
#  M_PP_Drone     : retour video du drone (monochrome, lignes, grain)
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_pp.txt")
DIR = "/Game/Bodycam/Materials/PP"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
_l = []


def log(m):
    _l.append(str(m))
    unreal.log("[CLAUDE] " + str(m))


def new_pp_material(name):
    path = DIR + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    for cand in ("BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING", "BL_SceneColorAfterTonemapping"):
        if hasattr(unreal.BlendableLocation, cand):
            m.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, cand))
            break
    return m


def scalar(m, name, val, x, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", val)
    return n


def vector(m, name, col, x, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(*col))
    return n


def custom(m, code, inputs, x, y):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c


def common_nodes(m):
    st = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, -300)
    st.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    sp = mel.create_material_expression(m, unreal.MaterialExpressionScreenPosition, -900, -100)
    vs = mel.create_material_expression(m, unreal.MaterialExpressionViewSize, -900, 50)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -900, 150)
    return st, sp, vs, tm


def finish(m, c):
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("OK %s" % m.get_path_name())


NVG_CODE = r"""
float2 res = max(ViewSize, float2(1,1));
float aspect = res.x / res.y;
float2 p = (UV - 0.5) * float2(aspect, 1.0);
float d = min(length(p - float2(-Sep, 0.0)), length(p - float2(Sep, 0.0)));
float mask = 1.0 - smoothstep(Radius - Soft, Radius, d);
float lum = dot(Color.rgb, float3(0.299, 0.587, 0.114));
lum = pow(1.0 - exp(-lum * Gain), 1.15);
float2 pp = floor(UV * res);
float3 p3 = frac(float3(pp.xyx) * 0.1031 + frac(Time * 24.0) * 0.37);
p3 += dot(p3, p3.yzx + 33.33);
float n = frac((p3.x + p3.y) * p3.z);
float scint = step(0.9992, n) * 0.5;
float grain = (n - 0.5) * Noise;
float edge = 1.0 - smoothstep(Radius * 0.45, Radius, d) * 0.7;
float v = saturate(lum + grain * (0.4 + lum) + scint) * edge * Power;
return Tint.rgb * v * mask;
"""

INFECT_CODE = r"""
float2 o = float2(sin(Time * 1.3), cos(Time * 0.9)) * Amount * float2(0.012, 0.007);
float2 b0 = ViewportUVToBufferUV(UV);
float2 b1 = ViewportUVToBufferUV(UV + o);
float2 b2 = ViewportUVToBufferUV(UV - o * 0.7);
float3 a = SceneTextureLookup(b0, 14, false).rgb;
float3 b = SceneTextureLookup(b1, 14, false).rgb;
float3 c = SceneTextureLookup(b2, 14, false).rgb;
float3 col = a * (1.0 - 0.5 * Amount) + (b * 0.3 + c * 0.2) * Amount;
float l = dot(col, float3(0.299, 0.587, 0.114));
col = lerp(col, float3(l, l, l) * float3(1.08, 0.88, 0.82), saturate(Amount * 0.7));
float2 p = UV - 0.5;
float pulse = 0.75 + 0.25 * sin(Time * (2.0 + Amount * 3.0));
float vig = saturate(1.0 - dot(p, p) * (0.6 + 2.6 * Amount * pulse));
col = col * vig + float3(0.35, 0.0, 0.0) * (1.0 - vig) * Amount;
col += float3(0.4, 0.0, 0.0) * Hurt * (1.0 - vig * 0.5);
return col + Dummy.rgb * 0.0;
"""

DRONE_CODE = r"""
float2 res = max(ViewSize, float2(1,1));
float lum = dot(Color.rgb, float3(0.299, 0.587, 0.114));
lum = 1.0 - exp(-lum * Gain);
float scan = 0.88 + 0.12 * sin(UV.y * res.y * 1.6);
float n = frac(sin(dot(floor(UV * res * 0.5) + floor(Time * 24.0) * float2(7.3, 3.1), float2(12.9898, 78.233))) * 43758.5453);
float roll = smoothstep(0.0, 0.02, abs(frac(UV.y - Time * 0.07) - 0.5)) * 0.15 + 0.85;
float2 p = UV - 0.5;
float vig = saturate(1.1 - dot(p, p) * 1.6);
float v = saturate(lum + (n - 0.5) * Noise) * scan * roll * vig * Power;
return Tint.rgb * v;
"""


def build_nvg():
    m = new_pp_material("M_PP_NVG")
    st, sp, vs, tm = common_nodes(m)
    c = custom(m, NVG_CODE, ["UV", "Color", "ViewSize", "Time", "Tint", "Gain", "Noise", "Power", "Radius", "Sep", "Soft"], -300, 0)
    mel.connect_material_expressions(sp, "ViewportUV", c, "UV")
    mel.connect_material_expressions(st, "Color", c, "Color")
    mel.connect_material_expressions(vs, "", c, "ViewSize")
    mel.connect_material_expressions(tm, "", c, "Time")
    mel.connect_material_expressions(vector(m, "Tint", (0.82, 0.92, 1.0, 1.0), -600, 300), "", c, "Tint")
    mel.connect_material_expressions(scalar(m, "Gain", 2.2, -600, 400), "", c, "Gain")
    mel.connect_material_expressions(scalar(m, "Noise", 0.22, -600, 480), "", c, "Noise")
    mel.connect_material_expressions(scalar(m, "Power", 1.0, -600, 560), "", c, "Power")
    mel.connect_material_expressions(scalar(m, "Radius", 0.47, -600, 640), "", c, "Radius")
    mel.connect_material_expressions(scalar(m, "Sep", 0.30, -600, 720), "", c, "Sep")
    mel.connect_material_expressions(scalar(m, "Soft", 0.012, -600, 800), "", c, "Soft")
    finish(m, c)


def build_infection():
    m = new_pp_material("M_PP_Infection")
    st, sp, vs, tm = common_nodes(m)
    c = custom(m, INFECT_CODE, ["UV", "Time", "Amount", "Hurt", "Dummy"], -300, 0)
    mel.connect_material_expressions(sp, "ViewportUV", c, "UV")
    mel.connect_material_expressions(tm, "", c, "Time")
    mel.connect_material_expressions(st, "Color", c, "Dummy")
    mel.connect_material_expressions(scalar(m, "Amount", 0.0, -600, 300), "", c, "Amount")
    mel.connect_material_expressions(scalar(m, "Hurt", 0.0, -600, 400), "", c, "Hurt")
    finish(m, c)


def build_drone():
    m = new_pp_material("M_PP_Drone")
    st, sp, vs, tm = common_nodes(m)
    c = custom(m, DRONE_CODE, ["UV", "Color", "ViewSize", "Time", "Tint", "Gain", "Noise", "Power"], -300, 0)
    mel.connect_material_expressions(sp, "ViewportUV", c, "UV")
    mel.connect_material_expressions(st, "Color", c, "Color")
    mel.connect_material_expressions(vs, "", c, "ViewSize")
    mel.connect_material_expressions(tm, "", c, "Time")
    mel.connect_material_expressions(vector(m, "Tint", (0.75, 0.95, 0.8, 1.0), -600, 300), "", c, "Tint")
    mel.connect_material_expressions(scalar(m, "Gain", 5.0, -600, 400), "", c, "Gain")
    mel.connect_material_expressions(scalar(m, "Noise", 0.2, -600, 480), "", c, "Noise")
    mel.connect_material_expressions(scalar(m, "Power", 1.0, -600, 560), "", c, "Power")
    finish(m, c)


for fn in (build_nvg, build_infection, build_drone):
    try:
        fn()
    except Exception as e:
        import traceback
        log("ERREUR %s: %r\n%s" % (fn.__name__, e, traceback.format_exc()))

log("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(_l))
