"""Liste les meshes/matériaux/textures du Starter Content avec leurs dimensions (outil de dev)."""
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
for path in ("/Game/StarterContent/Props", "/Game/StarterContent/Architecture", "/Game/StarterContent/Textures",
             "/Game/StarterContent/Materials", "/Game/StarterContent/Shapes"):
    for data in registry.get_assets_by_path(path, recursive=True):
        name = str(data.package_name)
        cls = str(data.asset_class_path.asset_name)
        line = "ASSET %s | %s" % (cls, name)
        if cls == "StaticMesh":
            mesh = unreal.load_asset(name)
            box = mesh.get_bounding_box()
            line += " | min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f)" % (
                box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)
        unreal.log(line)
unreal.log("INSPECT_DONE")
