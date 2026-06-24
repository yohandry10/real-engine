import unreal
import os

# Importa los edificios COMPLETOS de los Kenney City Kits (CC0) a /Game/KenneyCity.
# Todos los modelos comparten un atlas "colormap.png" (texturas planas), asi que con
# import_materials/import_textures cada edificio queda con su color correcto.

BASE = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\CityKits\Kenney"
KITS = {
    "Commercial": os.path.join(BASE, r"city-kit-commercial\Models\FBX format"),
    "Industrial": os.path.join(BASE, r"city-kit-industrial\Models\FBX format"),
    "Suburban":   os.path.join(BASE, r"city-kit-suburban\Models\FBX format"),
}
DEST_ROOT = "/Game/KenneyCity"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def make_options():
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = False
    opts.import_materials = True
    opts.import_textures = True
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    opts.static_mesh_import_data.combine_meshes = True
    opts.static_mesh_import_data.generate_lightmap_u_vs = True
    opts.static_mesh_import_data.auto_generate_collision = False
    return opts


def import_kit(name, folder):
    if not os.path.isdir(folder):
        unreal.log_warning("WL_KENNEY: carpeta no encontrada: " + folder)
        return 0
    dest = DEST_ROOT + "/" + name
    files = [f for f in os.listdir(folder) if f.lower().endswith(".fbx")]
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(folder, f)
        t.destination_path = dest
        t.automated = True
        t.save = True
        t.replace_existing = True
        t.options = make_options()
        tasks.append(t)
    asset_tools.import_asset_tasks(tasks)
    return len(files)


total = 0
for kit_name, kit_folder in KITS.items():
    n = import_kit(kit_name, kit_folder)
    unreal.log("WL_KENNEY: %s -> %d FBX" % (kit_name, n))
    total += n

unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)

# Resumen: contar StaticMesh reales por carpeta
unreal.log("WL_KENNEY_RESULT_BEGIN")
all_assets = unreal.EditorAssetLibrary.list_assets(DEST_ROOT, recursive=True, include_folder=False)
mesh_count = 0
for a in all_assets:
    ad = unreal.EditorAssetLibrary.find_asset_data(a)
    if ad.asset_class_path.asset_name == "StaticMesh":
        mesh_count += 1
unreal.log("WL_KENNEY_STATICMESHES: %d" % mesh_count)
unreal.log("WL_KENNEY_TOTAL_ASSETS: %d" % len(all_assets))
unreal.log("WL_KENNEY_RESULT_END")
