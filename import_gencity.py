import unreal, os

SRC  = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated"
DEST = "/Game/GenCity"
FILES = [
    "city_large.fbx", "city_large_b.fbx", "city_large_c.fbx",
    "city_medium.fbx", "city_medium_b.fbx", "city_medium_c.fbx",
    "city_small.fbx", "city_small_b.fbx", "city_small_c.fbx",
]

at = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for f in FILES:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = DEST
    t.automated = True
    t.save = True
    t.replace_existing = True
    o = unreal.FbxImportUI()
    o.import_mesh = True
    o.import_as_skeletal = False
    o.import_materials = False
    o.import_textures = False
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.static_mesh_import_data.combine_meshes = True
    o.static_mesh_import_data.generate_lightmap_u_vs = True
    o.static_mesh_import_data.auto_generate_collision = False
    # Conserva los VERTEX COLORS del FBX (la ciudad es UNLIT vertex-color, como el terreno).
    o.static_mesh_import_data.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE
    t.options = o
    tasks.append(t)

at.import_asset_tasks(tasks)
unreal.EditorAssetLibrary.save_directory(DEST, False, True)

unreal.log("WL_GENCITY_RESULT_BEGIN")
for a in unreal.EditorAssetLibrary.list_assets(DEST, True, False):
    unreal.log("WL_GENCITY_ASSET: " + a)
unreal.log("WL_GENCITY_RESULT_END")
