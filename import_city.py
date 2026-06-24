import unreal

fbx = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\CityKits\Fab\City-SpatialNeglect\fbx\cityfbx_extracted\City.fbx"
dest = "/Game/CitySpatialNeglect"

task = unreal.AssetImportTask()
task.filename = fbx
task.destination_path = dest
task.automated = True
task.save = True
task.replace_existing = True

opts = unreal.FbxImportUI()
opts.import_mesh = True
opts.import_as_skeletal = False
opts.import_materials = True
opts.import_textures = True
opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
opts.static_mesh_import_data.combine_meshes = False
opts.static_mesh_import_data.generate_lightmap_u_vs = True
opts.static_mesh_import_data.auto_generate_collision = True
task.options = opts

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

# Report what got created
assets = unreal.EditorAssetLibrary.list_assets(dest, recursive=True, include_folder=False)
unreal.log("WL_IMPORT_RESULT_BEGIN")
for a in assets:
    unreal.log("WL_ASSET: " + a)
unreal.log("WL_IMPORT_RESULT_END")
unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True)
