import unreal, os

SRC  = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated"
DEST = "/Game/GenNature"
FILES = [
    "nature_conifer.fbx", "nature_broadleaf.fbx", "nature_palm.fbx",
    "nature_rock.fbx", "nature_peak.fbx", "nature_mount.fbx",
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
    # Conserva los VERTEX COLORS del FBX (naturaleza UNLIT vertex-color, como el terreno/ciudades).
    o.static_mesh_import_data.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE
    t.options = o
    tasks.append(t)

at.import_asset_tasks(tasks)
unreal.EditorAssetLibrary.save_directory(DEST, False, True)

unreal.log("WL_GENNATURE_RESULT_BEGIN")
for a in unreal.EditorAssetLibrary.list_assets(DEST, True, False):
    unreal.log("WL_GENNATURE_ASSET: " + a)
unreal.log("WL_GENNATURE_RESULT_END")
