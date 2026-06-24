import unreal

fbx = r"C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\FabLibrary\London_White_City-e0383571\fbx\london-white-city_extracted\source\White-City.fbx"
dest = "/Game/LondonCity"

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
opts.static_mesh_import_data.combine_meshes = True   # UNA sola ciudad coherente
opts.static_mesh_import_data.generate_lightmap_u_vs = True
opts.static_mesh_import_data.auto_generate_collision = False
task.options = opts

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

assets = unreal.EditorAssetLibrary.list_assets(dest, recursive=True, include_folder=False)
unreal.log("WL_IMPORT_RESULT_BEGIN")
for a in assets:
    unreal.log("WL_ASSET: " + a)
unreal.log("WL_IMPORT_RESULT_END")
unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True)
