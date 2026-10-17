# Repare les materiaux des zombies.
#  1) Annule ma modification precedente : les textures specular/glossiness des zombies etaient passees en
#     compression "Masks", ce qui ne correspond pas au noeud du materiau (Sampler type Color) -> le
#     materiau ne compilait plus et Unreal affichait le materiau par defaut (plus de couleur).
#  2) Rebranche chaque materiau simplement : BaseColor = texture diffuse, Normal = texture normale,
#     Metallic = 0, Roughness = 0.7, Specular = 0.4. On contourne ainsi la fonction d'import
#     "Phong vers Metal/Roughness" qui rendait la peau metallique (sombre, sans couleur).
# Resultat lisible dans Saved/Logs/claude_zombiemat.txt
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_zombiemat.txt")
ROOT = "/Game/Bodycam/Characters/Zombie"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
MP = unreal.MaterialProperty
lines = []


def log(s):
    lines.append(s)


def flush():
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def assets(root, classes):
    return [str(ad.package_name) for ad in reg.get_assets_by_path(root, recursive=True)
            if str(ad.asset_class_path.asset_name) in classes]


# ------------------------------------------------------------ 1) textures remises comme avant
log("== 1) Textures specular / glossiness remises en compression par defaut (sRGB) ==")
for path in assets(ROOT, ("Texture2D",)):
    n = path.split("/")[-1].lower()
    if "specular" not in n and "glossiness" not in n:
        continue
    tex = unreal.load_asset(path)
    if not tex:
        continue
    changes = []
    try:
        if tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_DEFAULT:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            changes.append("compression=defaut")
        if not tex.get_editor_property("srgb"):
            tex.set_editor_property("srgb", True)
            changes.append("sRGB=True")
        if changes:
            eal.save_loaded_asset(tex)
            log("  CORRIGE %s : %s" % (path, ", ".join(changes)))
        else:
            log("  ok %s" % path)
    except Exception as e:
        log("  ! %s : %r" % (path, e))
flush()


# ------------------------------------------------------------ 2) rebranchement des materiaux
def collect_texture_params(mat):
    """Parcourt le graphe depuis les sorties principales et retourne {nom_parametre: noeud texture}."""
    found = {}
    seen = set()
    stack = []
    for prop in (MP.MP_BASE_COLOR, MP.MP_NORMAL, MP.MP_ROUGHNESS, MP.MP_METALLIC, MP.MP_SPECULAR):
        try:
            n = mel.get_material_property_input_node(mat, prop)
        except Exception:
            n = None
        if n:
            stack.append(n)
    while stack:
        e = stack.pop()
        key = e.get_path_name()
        if key in seen:
            continue
        seen.add(key)
        if isinstance(e, unreal.MaterialExpressionTextureSampleParameter2D):
            found[str(e.get_editor_property("parameter_name"))] = e
        try:
            for sub in mel.get_inputs_for_material_expression(mat, e):
                if sub:
                    stack.append(sub)
        except Exception:
            pass
    return found


def connect(expr, prop):
    for out in ("RGB", ""):
        try:
            if mel.connect_material_property(expr, out, prop):
                return True
        except Exception:
            pass
    return False


def constant(mat, value, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", value)
    return c


log("")
log("== 2) Rebranchement des materiaux ==")
done = 0
for path in sorted(assets(ROOT, ("Material",))):
    mat = unreal.load_asset(path)
    if not mat:
        continue
    params = collect_texture_params(mat)
    diff = params.get("DiffuseColorMap")
    norm = params.get("NormalMap")
    log("  %s : parametres trouves = %s" % (path, ", ".join(sorted(params)) or "AUCUN"))
    if diff is None:
        log("    -> pas de texture diffuse trouvee, materiau laisse tel quel")
        continue
    ok_color = connect(diff, MP.MP_BASE_COLOR)
    ok_norm = connect(norm, MP.MP_NORMAL) if norm is not None else None
    ok_metal = connect(constant(mat, 0.0, -420, 200), MP.MP_METALLIC)
    ok_rough = connect(constant(mat, 0.7, -420, 300), MP.MP_ROUGHNESS)
    ok_spec = connect(constant(mat, 0.4, -420, 400), MP.MP_SPECULAR)
    try:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    except Exception:
        pass
    try:
        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)
        done += 1
        log("    -> couleur=%s normal=%s metallic=%s roughness=%s specular=%s" % (ok_color, ok_norm, ok_metal, ok_rough, ok_spec))
    except Exception as e:
        log("    ! sauvegarde : %r" % e)
log("  %d materiaux rebranches" % done)
log("FIN")
flush()
