# Active les "usages" manquants sur les materiaux (sinon Unreal affiche un materiau gris par defaut) :
#  - zombies Mixamo : used_with_skeletal_mesh
#  - materiaux Megascans de la map : used_with_instanced_static_meshes
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG_PATH = os.path.join(PROJECT_DIR, "Saved", "Logs", "claude_fixmat.txt")
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
lines = []

FLAGS = {
    "/Game/Bodycam/Characters": ["used_with_skeletal_mesh"],
    "/Game/Bodycam/Weapons": ["used_with_skeletal_mesh", "used_with_nanite"],
    "/Game/DerelictCorridor": ["used_with_instanced_static_meshes", "used_with_nanite"],
}

for root, flags in FLAGS.items():
    for ad in reg.get_assets_by_path(root, recursive=True):
        if str(ad.asset_class_path.asset_name) != "Material":
            continue
        path = str(ad.package_name)
        m = unreal.load_asset(path)
        if not m:
            continue
        changed = []
        for f in flags:
            try:
                if not m.get_editor_property(f):
                    m.set_editor_property(f, True)
                    changed.append(f)
            except Exception as e:
                lines.append("  ! %s %s: %r" % (path, f, e))
        if changed:
            mel.recompile_material(m)
            eal.save_loaded_asset(m)
            lines.append("%s : %s" % (path, ", ".join(changed)))

lines.append("FIN")
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
open(LOG_PATH, "w", encoding="utf-8").write("\n".join(lines))
