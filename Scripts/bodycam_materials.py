"""
Matériau maître "aligné sur le monde" + instances de surfaces (asphalte, brique, béton...).
Avec ce matériau, la texture garde la même échelle quelle que soit la taille du bloc :
plus de textures étirées sur les murs.
Importé par build_quartier.py (pas besoin de le lancer seul).
"""
import unreal

MAT_DIR = "/Game/Bodycam/Materials"
MASTER_PATH = MAT_DIR + "/M_WorldAligned"
SC = "/Game/StarterContent/Textures/"

mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def _tex(name):
    return unreal.load_asset(SC + name)


def _check(ok, what):
    if not ok:
        unreal.log_error("BODYCAM_MAT_CONNECT_FAILED " + what)


def _ensure_usage(mat):
    """Sans ces drapeaux, le jeu remplace le matériau par le damier gris par défaut."""
    mat.set_editor_property("used_with_static_lighting", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(mat)
    assets.save_loaded_asset(mat)
    return mat


def get_master():
    if assets.does_asset_exist(MASTER_PATH):
        return _ensure_usage(unreal.load_asset(MASTER_PATH))

    mat = tools.create_asset("M_WorldAligned", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())

    base_tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -1000, -300)
    base_tex.set_editor_property("parameter_name", "BaseTex")
    base_tex.set_editor_property("texture", _tex("T_Concrete_Poured_D"))

    normal_tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -1000, 100)
    normal_tex.set_editor_property("parameter_name", "NormalTex")
    normal_tex.set_editor_property("texture", _tex("T_Concrete_Poured_N"))
    normal_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)

    size = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1000, -50)
    size.set_editor_property("parameter_name", "TextureSize")
    size.set_editor_property("default_value", unreal.LinearColor(300.0, 300.0, 300.0, 0.0))

    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -450)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -300, 300)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.85)

    wat = mel.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -600, -300)
    wat.set_editor_property("material_function", unreal.load_asset(
        "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"))
    wan = mel.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -600, 100)
    wan.set_editor_property("material_function", unreal.load_asset(
        "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal"))

    multiply = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, -300)

    _check(mel.connect_material_expressions(base_tex, "", wat, "TextureObject"), "base->wat")
    _check(mel.connect_material_expressions(size, "", wat, "TextureSize"), "size->wat")
    _check(mel.connect_material_expressions(wat, "XYZ Texture", multiply, "A"), "wat->mul")
    _check(mel.connect_material_expressions(tint, "", multiply, "B"), "tint->mul")
    _check(mel.connect_material_expressions(normal_tex, "", wan, "TextureObject"), "normal->wan")
    _check(mel.connect_material_expressions(size, "", wan, "TextureSize"), "size->wan")
    _check(mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR), "basecolor")
    _check(mel.connect_material_property(wan, "XYZ Texture", unreal.MaterialProperty.MP_NORMAL), "normal")
    _check(mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS), "roughness")

    # WorldAlignedNormal renvoie une normale en espace monde.
    mat.set_editor_property("tangent_space_normal", False)
    return _ensure_usage(mat)


# nom : (texture couleur, texture normale, taille du motif en cm, teinte RGB, rugosité)
SURFACES = {
    "Asphalt":        ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 250, (0.13, 0.13, 0.14), 0.30),
    "Sidewalk":       ("T_Concrete_Tiles_D", "T_Concrete_Tiles_N", 160, (0.55, 0.54, 0.52), 0.45),
    "BrickRed":       ("T_Brick_Clay_Old_D", "T_Brick_Clay_Old_N", 220, (0.85, 0.80, 0.78), 0.90),
    "BrickBeige":     ("T_Brick_Cut_Stone_D", "T_Brick_Cut_Stone_N", 260, (0.90, 0.86, 0.78), 0.90),
    "ConcretePanels": ("T_Concrete_Panels_D", "T_Concrete_Panels_N", 400, (0.80, 0.80, 0.80), 0.85),
    "ConcreteDirty":  ("T_Concrete_Grime_D", "T_Concrete_Poured_N", 350, (0.70, 0.68, 0.64), 0.90),
    "ConcreteLight":  ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 300, (0.85, 0.84, 0.80), 0.80),
    "Plaster":        ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 500, (0.80, 0.76, 0.68), 0.90),
    "FloorTiles":     ("T_Concrete_Tiles_D", "T_Concrete_Tiles_N", 120, (0.55, 0.52, 0.48), 0.60),
    "Gravel":         ("T_Ground_Gravel_D", "T_Ground_Gravel_N", 250, (0.60, 0.58, 0.55), 0.60),
    "MetalRust":      ("T_Metal_Rust_D", "T_Metal_Rust_N", 200, (1.00, 1.00, 1.00), 0.70),
    "SteelDark":      ("T_Metal_Steel_D", "T_Metal_Steel_N", 150, (0.30, 0.30, 0.32), 0.50),
    "ContainerBlue":  ("T_Metal_Rust_D", "T_Metal_Rust_N", 250, (0.25, 0.40, 0.65), 0.65),
    "ContainerRed":   ("T_Metal_Rust_D", "T_Metal_Rust_N", 250, (0.70, 0.25, 0.20), 0.65),
    "Wood":           ("T_Wood_Pine_D", "T_Wood_Pine_N", 120, (0.80, 0.70, 0.55), 0.80),
    "Cardboard":      ("T_Wood_Oak_D", "T_Wood_Oak_N", 200, (0.75, 0.60, 0.40), 0.95),
    "Glass":          ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 600, (0.02, 0.025, 0.03), 0.08),
    "Paint":          ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 300, (0.80, 0.78, 0.65), 0.75),
    "PaintRed":       ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 300, (0.55, 0.06, 0.05), 0.55),
    "PaintYellow":    ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 300, (0.75, 0.55, 0.05), 0.60),
    "SignBlue":       ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 600, (0.05, 0.15, 0.45), 0.40),
    "CarWhite":       ("T_Metal_Steel_D", "T_Metal_Steel_N", 400, (0.85, 0.85, 0.85), 0.25),
    "CarGrey":        ("T_Metal_Steel_D", "T_Metal_Steel_N", 400, (0.35, 0.36, 0.38), 0.25),
    "CarBlue":        ("T_Metal_Steel_D", "T_Metal_Steel_N", 400, (0.06, 0.10, 0.25), 0.25),
    "CarRed":         ("T_Metal_Steel_D", "T_Metal_Steel_N", 400, (0.40, 0.04, 0.03), 0.25),
    "Charred":        ("T_Metal_Rust_D", "T_Metal_Rust_N", 150, (0.08, 0.07, 0.06), 0.90),
    "Rubber":         ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 100, (0.03, 0.03, 0.03), 0.80),
    "Puddle":         ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 600, (0.05, 0.05, 0.05), 0.03),
    "TrashBag":       ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 200, (0.02, 0.02, 0.025), 0.30),
    "Tarp":           ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 200, (0.10, 0.22, 0.45), 0.60),
    "Forklift":       ("T_Metal_Rust_D", "T_Metal_Rust_N", 200, (0.85, 0.65, 0.10), 0.55),
    "Paper":          ("T_Concrete_Poured_D", "T_Concrete_Poured_N", 300, (0.85, 0.84, 0.80), 0.90),
}

EMISSIVE_PATH = MAT_DIR + "/M_Emissive"

# nom : (couleur RGB, intensité par défaut)
EMISSIVES = {
    "WindowWarm": ((1.0, 0.62, 0.30), 2.5),
    "WindowCold": ((0.55, 0.70, 1.0), 2.0),
    "StreetLamp": ((1.0, 0.55, 0.20), 25.0),
    "Tube":       ((0.85, 0.95, 1.0), 15.0),
    "NeonRed":    ((1.0, 0.05, 0.08), 20.0),
    "NeonBlue":   ((0.10, 0.35, 1.0), 20.0),
    "Headlight":  ((1.0, 0.95, 0.85), 4.0),
}


def get_emissive_master():
    if assets.does_asset_exist(EMISSIVE_PATH):
        return _ensure_usage(unreal.load_asset(EMISSIVE_PATH))
    mat = tools.create_asset("M_Emissive", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.8, 0.6, 1.0))
    intensity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 200)
    intensity.set_editor_property("parameter_name", "Intensity")
    intensity.set_editor_property("default_value", 10.0)
    multiply = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 50)
    base = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, -150)
    base.set_editor_property("constant", unreal.LinearColor(0.02, 0.02, 0.02, 1.0))
    _check(mel.connect_material_expressions(color, "", multiply, "A"), "emissive color")
    _check(mel.connect_material_expressions(intensity, "", multiply, "B"), "emissive intensity")
    _check(mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive")
    _check(mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR), "emissive base")
    return _ensure_usage(mat)


def get_emissives():
    master = get_emissive_master()
    result = {}
    for name, (rgb, intensity) in EMISSIVES.items():
        path = MAT_DIR + "/MI_Emissive_" + name
        if assets.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset("MI_Emissive_" + name, MAT_DIR, unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Intensity", intensity)
        assets.save_loaded_asset(mi)
        result[name] = mi
    return result


def get_surfaces():
    master = get_master()
    result = {}
    for name, (tex, ntex, size, tint, rough) in SURFACES.items():
        path = MAT_DIR + "/MI_" + name
        if assets.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset("MI_" + name, MAT_DIR, unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        mel.set_material_instance_texture_parameter_value(mi, "BaseTex", _tex(tex))
        mel.set_material_instance_texture_parameter_value(mi, "NormalTex", _tex(ntex))
        mel.set_material_instance_vector_parameter_value(mi, "TextureSize", unreal.LinearColor(size, size, size, 0.0))
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
        assets.save_loaded_asset(mi)
        result[name] = mi
    return result


RAIN_PATH = MAT_DIR + "/M_Rain"


def get_rain_material():
    if assets.does_asset_exist(RAIN_PATH):
        return _ensure_usage(unreal.load_asset(RAIN_PATH))
    mat = tools.create_asset("M_Rain", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(0.55, 0.6, 0.68, 1.0))
    opacity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 200)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.22)
    _check(mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "rain color")
    _check(mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY), "rain opacity")
    return _ensure_usage(mat)