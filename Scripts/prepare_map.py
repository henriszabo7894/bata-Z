# Prepare la map DerelictCorridor pour le jeu :
#  - ajoute un NavMeshBoundsVolume (navigation des zombies, generee en jeu autour du joueur)
#  - releve les zones (volumes post-process par couloir), cameras de cinematique, lumieres
# Resultat : Saved/Logs/claude_map.txt
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_map.txt")
MAP = "/Game/DerelictCorridor/Maps/DerelictCorridor"
_l = []


def log(m):
    _l.append(str(m))
    unreal.log("[CLAUDE] " + str(m))


def v(x):
    return "(%.0f, %.0f, %.0f)" % (x.x, x.y, x.z)


try:
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    log("Carte: %s" % world.get_name())
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()

    mn = mx = None
    for d in descs:
        cname = d.get_editor_property("native_class").get_name() if d.get_editor_property("native_class") else "?"
        label = str(d.get_editor_property("label"))
        b = d.get_editor_property("bounds")
        try:
            lo, hi = b.min, b.max
        except Exception:
            continue
        if cname in ("StaticMeshActor", "PackedLevelActor"):
            mn = lo if mn is None else unreal.Vector(min(mn.x, lo.x), min(mn.y, lo.y), min(mn.z, lo.z))
            mx = hi if mx is None else unreal.Vector(max(mx.x, hi.x), max(mx.y, hi.y), max(mx.z, hi.z))
        if cname in ("PostProcessVolume", "CineCameraActor", "RectLight", "SpotLight", "PointLight",
                     "SkyLight", "PlayerStart", "NavMeshBoundsVolume", "ExponentialHeightFog", "LevelSequenceActor",
                     "DirectionalLight"):
            c = (lo + hi) * 0.5
            log("%-20s | %-28s | centre=%s min=%s max=%s" % (cname, label, v(c), v(lo), v(hi)))
    log("Bornes geometrie: min=%s max=%s" % (v(mn), v(mx)))

    # Navigation : un seul volume qui couvre toute la geometrie (+ marge).
    has_nav = any((d.get_editor_property("native_class") and d.get_editor_property("native_class").get_name() == "NavMeshBoundsVolume") for d in descs)
    if not has_nav and mn is not None:
        centre = (mn + mx) * 0.5
        ext = (mx - mn) * 0.5 + unreal.Vector(300, 300, 200)
        vol = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.NavMeshBoundsVolume, centre, unreal.Rotator(0, 0, 0))
        vol.set_actor_scale3d(unreal.Vector(ext.x / 100.0, ext.y / 100.0, ext.z / 100.0))
        vol.set_actor_label("Nav_Bounds_Bodycam")
        try:
            vol.set_editor_property("is_spatially_loaded", False)
        except Exception as e:
            log("is_spatially_loaded: %r" % e)
        log("NavMeshBoundsVolume ajoute: centre=%s demi-taille=%s" % (v(centre), v(ext)))
        ok = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        log("Sauvegarde: %s" % ok)
    else:
        log("NavMeshBoundsVolume deja present")
except Exception as e:
    import traceback
    log("ERREUR: %r\n%s" % (e, traceback.format_exc()))

log("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(_l))
