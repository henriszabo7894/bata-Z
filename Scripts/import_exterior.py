# Decor exterieur (batiment post-apocalyptique, pack "Post Apocalypse Survivor Kitbash" version gratuite, allege sous Blender)
# + releve des fenetres / portes de la carte pour savoir par ou le drone sortira.
# Lance par Importer_Exterieur.bat. Resultat : Saved/Logs/claude_exterior.txt
import os, traceback
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "..", "Assets_Import", "Exterieur"))
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_exterior.txt")
DEST = "/Game/Bodycam/Exterior"
MAP = "/Game/DerelictCorridor/Maps/DerelictCorridor"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lines = []


def log(m):
    lines.append(str(m))


def flush():
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    open(LOG_PATH, "w", encoding="utf-8").write("\n".join(lines))


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


def tex(name, srgb, comp):
    t = import_file(os.path.join(SRC, name + ".png"), DEST + "/Textures", name)
    if t:
        t.set_editor_property("srgb", srgb)
        t.set_editor_property("compression_settings", comp)
        t.set_editor_property("max_texture_size", 2048)
        eal.save_loaded_asset(t)
    log("texture %s : %s" % (name, "ok" if t else "ECHEC"))
    return t


def build_material():
    TC = unreal.TextureCompressionSettings
    bc = tex("T_PA_color_dark", True, TC.TC_DEFAULT)
    nm = tex("T_PA_normal", False, TC.TC_NORMALMAP)
    ro = tex("T_PA_roughness", False, TC.TC_MASKS)
    me = tex("T_PA_metalness", False, TC.TC_MASKS)
    op = tex("T_PA_opacity", False, TC.TC_MASKS)
    path = DEST + "/M_PA_Building"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_PA_Building", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    for flag in ("used_with_instanced_static_meshes", "used_with_static_lighting"):
        try:
            m.set_editor_property(flag, True)
        except Exception:
            pass
    MS = unreal.MaterialSamplerType

    def samp(t, y, st):
        n = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, y)
        n.set_editor_property("texture", t)
        n.set_editor_property("sampler_type", st)
        return n
    if bc: mel.connect_material_property(samp(bc, -300, MS.SAMPLERTYPE_COLOR), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if nm: mel.connect_material_property(samp(nm, -50, MS.SAMPLERTYPE_NORMAL), "RGB", unreal.MaterialProperty.MP_NORMAL)
    if ro: mel.connect_material_property(samp(ro, 200, MS.SAMPLERTYPE_MASKS), "R", unreal.MaterialProperty.MP_ROUGHNESS)
    if me: mel.connect_material_property(samp(me, 450, MS.SAMPLERTYPE_MASKS), "R", unreal.MaterialProperty.MP_METALLIC)
    if op: mel.connect_material_property(samp(op, 700, MS.SAMPLERTYPE_MASKS), "R", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("M_PA_Building cree")
    return m


def import_building(mat):
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
    try:
        smd.set_editor_property("build_nanite", False)
    except Exception:
        pass
    mesh = import_file(os.path.join(SRC, "SM_PA_Building12.fbx"), DEST, "SM_PA_Building12", o)
    if not mesh:
        log("! ECHEC import du batiment")
        return
    mats = []
    for slot in mesh.get_editor_property("static_materials"):
        slot.set_editor_property("material_interface", mat)
        mats.append(slot)
    mesh.set_editor_property("static_materials", mats)
    # Collision au plus juste (le drone doit pouvoir passer sous les passerelles et exploser contre les murs).
    try:
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    except Exception as e:
        log("collision: %r" % e)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    log("SM_PA_Building12 : taille %.0f x %.0f x %.0f cm, centre %s" % (b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2, b.origin))


def survey_map():
    # Releve des ouvertures de la carte : par ou le drone peut sortir dehors.
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    keys = ("window", "fenetre", "door", "porte", "glass", "vitre", "frame", "opening", "garage", "shutter", "exit", "gate")
    mn = mx = None
    for d in descs:
        cls = d.get_editor_property("native_class")
        cname = cls.get_name() if cls else "?"
        label = str(d.get_editor_property("label"))
        b = d.get_editor_property("bounds")
        try:
            lo, hi = b.min, b.max
        except Exception:
            continue
        if cname in ("StaticMeshActor", "PackedLevelActor"):
            mn = lo if mn is None else unreal.Vector(min(mn.x, lo.x), min(mn.y, lo.y), min(mn.z, lo.z))
            mx = hi if mx is None else unreal.Vector(max(mx.x, hi.x), max(mx.y, hi.y), max(mx.z, hi.z))
        if any(k in label.lower() for k in keys):
            c = (lo + hi) * 0.5
            log("OUVERTURE %-22s %-40s centre=(%.0f,%.0f,%.0f) taille=(%.0f,%.0f,%.0f)" % (cname, label, c.x, c.y, c.z, hi.x - lo.x, hi.y - lo.y, hi.z - lo.z))
    if mn:
        log("BORNES CARTE min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f)" % (mn.x, mn.y, mn.z, mx.x, mx.y, mx.z))


try:
    mat = build_material()
    import_building(mat)
    flush()
    survey_map()
except Exception as e:
    log("ERREUR %r\n%s" % (e, traceback.format_exc()))
log("FIN")
flush()
