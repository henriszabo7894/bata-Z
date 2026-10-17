# Etape 1 qualite graphique.
#  1) Materiaux de la map qui retombent sur le gris par defaut (usage "instanced static meshes" manquant
#     sur le materiau PARENT des instances Megascans).
#  2) Textures mal reglees (normal map, masques ORM/specular, sRGB, LOD bias, taille max).
#  3) Rapport : materiaux/textures des zombies, instances avec texture vide, meshes Nanite de la map.
# Resultat lisible dans Saved/Logs/claude_graphics.txt
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_graphics.txt")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
lines = []


def log(s):
    lines.append(s)


def flush():
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def assets(root, classes):
    out = []
    for ad in reg.get_assets_by_path(root, recursive=True):
        if str(ad.asset_class_path.asset_name) in classes:
            out.append(str(ad.package_name))
    return out


MAP_ROOT = "/Game/DerelictCorridor"
ZOMBIE_ROOT = "/Game/Bodycam/Characters/Zombie"

# ---------------------------------------------------------------- 1) materiaux parents
log("== 1) Materiaux parents des instances (map) ==")
bases = {}
for path in assets(MAP_ROOT, ("MaterialInstanceConstant",)):
    try:
        mi = unreal.load_asset(path)
        base = mi.get_base_material() if mi else None
    except Exception as e:
        log("  ! %s : %r" % (path, e))
        continue
    if base is None:
        log("  ? pas de parent : %s" % path)
        continue
    bases[base.get_path_name().split(".")[0]] = base

for bpath, base in sorted(bases.items()):
    if bpath.startswith("/Engine"):
        log("  (moteur, ignore) %s" % bpath)
        continue
    changed = []
    for flag in ("used_with_instanced_static_meshes",):
        try:
            if not base.get_editor_property(flag):
                base.set_editor_property(flag, True)
                changed.append(flag)
        except Exception as e:
            log("  ! %s %s : %r" % (bpath, flag, e))
    if changed:
        try:
            mel.recompile_material(base)
            eal.save_loaded_asset(base)
            log("  CORRIGE %s : %s" % (bpath, ", ".join(changed)))
        except Exception as e:
            log("  ! sauvegarde %s : %r" % (bpath, e))
    else:
        log("  ok %s" % bpath)
flush()

# ---------------------------------------------------------------- 2) textures
log("")
log("== 2) Textures ==")
TC = unreal.TextureCompressionSettings


def wanted(name):
    n = name.lower()
    if "glow" in n or "emissive" in n:
        return None
    if "normal" in n or n.endswith("_n"):
        return (TC.TC_NORMALMAP, False, "normal")
    if ("_orm" in n or "specular" in n or "glossiness" in n or "roughness" in n
            or n.endswith("_r") or "_mask" in n):
        return (TC.TC_MASKS, False, "masque")
    if "diffuse" in n or n.endswith("_b") or "albedo" in n or "basecolor" in n:
        return (TC.TC_DEFAULT, True, "couleur")
    return None


nfix = 0
nseen = 0
for root in (ZOMBIE_ROOT, MAP_ROOT):
    for path in assets(root, ("Texture2D",)):
        tex = unreal.load_asset(path)
        if not tex:
            continue
        nseen += 1
        name = path.split("/")[-1]
        changes = []
        try:
            # (ne touche plus a la compression / sRGB : ca cassait les materiaux dont le noeud texture attend un type precis)
            if tex.get_editor_property("lod_bias") != 0:
                tex.set_editor_property("lod_bias", 0)
                changes.append("lod_bias=0")
            if tex.get_editor_property("max_texture_size") != 0:
                tex.set_editor_property("max_texture_size", 0)
                changes.append("max_size=0")
            if tex.get_editor_property("never_stream"):
                tex.set_editor_property("never_stream", False)
                changes.append("never_stream=False")
        except Exception as e:
            log("  ! %s : %r" % (path, e))
            continue
        if changes:
            nfix += 1
            try:
                eal.save_loaded_asset(tex)
                log("  CORRIGE %s : %s" % (path, ", ".join(changes)))
            except Exception as e:
                log("  ! sauvegarde %s : %r" % (path, e))
log("  %d textures lues, %d corrigees" % (nseen, nfix))
flush()

# ---------------------------------------------------------------- 3a) rapport zombies
log("")
log("== 3a) Zombies : emplacements de materiaux ==")
for path in sorted(assets(ZOMBIE_ROOT, ("SkeletalMesh",))):
    sm = unreal.load_asset(path)
    if not sm:
        continue
    log("  %s" % path)
    try:
        slots = sm.get_editor_property("materials")
    except Exception as e:
        log("    ! %r" % e)
        continue
    for i, slot in enumerate(slots):
        mat = slot.get_editor_property("material_interface")
        if mat is None:
            log("    slot %d : AUCUN MATERIAU (gris par defaut)" % i)
            continue
        info = []
        try:
            base = mat.get_base_material() if hasattr(mat, "get_base_material") else mat
            node = mel.get_material_property_input_node(base, unreal.MaterialProperty.MP_BASE_COLOR)
            info.append("couleur=%s" % ("branchee" if node else "VIDE"))
            node_n = mel.get_material_property_input_node(base, unreal.MaterialProperty.MP_NORMAL)
            info.append("normal=%s" % ("branchee" if node_n else "vide"))
            texs = mel.get_used_textures(base)
            info.append("textures=[%s]" % ", ".join(t.get_name() for t in texs))
            info.append("skel_usage=%s" % base.get_editor_property("used_with_skeletal_mesh"))
        except Exception as e:
            info.append("analyse impossible: %r" % e)
        log("    slot %d : %s | %s" % (i, mat.get_path_name().split(".")[0], " ; ".join(info)))
flush()

# ---------------------------------------------------------------- 3b) instances avec texture vide
log("")
log("== 3b) Instances de materiaux avec parametre texture vide (map + zombies) ==")
nempty = 0
for root in (MAP_ROOT, ZOMBIE_ROOT):
    for path in assets(root, ("MaterialInstanceConstant",)):
        mi = unreal.load_asset(path)
        if not mi:
            continue
        try:
            for p in mi.get_editor_property("texture_parameter_values"):
                if p.get_editor_property("parameter_value") is None:
                    nempty += 1
                    log("  VIDE %s : %s" % (path, p.get_editor_property("parameter_info").get_editor_property("name")))
        except Exception as e:
            log("  ! %s : %r" % (path, e))
log("  %d parametres vides" % nempty)
flush()

# ---------------------------------------------------------------- 3c) Nanite (inactif sur UHD 770)
log("")
log("== 3c) Meshes Nanite de la map (la carte graphique ne gere pas Nanite : ils utilisent le mesh de secours) ==")
n_total = 0
n_nanite = 0
worst = []
for path in assets(MAP_ROOT, ("StaticMesh",))[:300]:
    sm = unreal.load_asset(path)
    if not sm:
        continue
    n_total += 1
    try:
        ns = sm.get_editor_property("nanite_settings")
        if ns.get_editor_property("enabled"):
            n_nanite += 1
            worst.append((ns.get_editor_property("fallback_relative_error"), path))
    except Exception:
        pass
worst.sort(reverse=True)
log("  %d meshes lus, %d en Nanite" % (n_total, n_nanite))
for err, p in worst[:15]:
    log("    erreur de secours %.3f : %s" % (err, p))

log("FIN")
flush()
