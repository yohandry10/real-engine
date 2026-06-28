import os

import unreal

SRC = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Quaternius\UltimateModularMen\Swat.fbx"
DEST = "/Game/GenVehicle/Quaternius"


def set_prop(obj, name, value):
    if not obj:
        return
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:
        unreal.log_warning(f"WL_QUATERNIUS_IMPORT_OPTION_SKIPPED: {name}: {exc}")


if not os.path.exists(SRC):
    raise RuntimeError(f"Missing Quaternius FBX: {SRC}")

task = unreal.AssetImportTask()
task.filename = SRC
task.destination_path = DEST
task.destination_name = "SK_Quaternius_Swat"
task.automated = True
task.save = True
task.replace_existing = True

opts = unreal.FbxImportUI()
set_prop(opts, "import_mesh", True)
set_prop(opts, "import_as_skeletal", True)
set_prop(opts, "import_animations", True)
set_prop(opts, "import_materials", True)
set_prop(opts, "import_textures", False)
set_prop(opts, "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)

skeletal_data = opts.get_editor_property("skeletal_mesh_import_data")
set_prop(skeletal_data, "preserve_smoothing_groups", True)
set_prop(skeletal_data, "import_meshes_in_bone_hierarchy", True)
set_prop(skeletal_data, "convert_scene", True)
set_prop(skeletal_data, "convert_scene_unit", True)

anim_data = opts.get_editor_property("anim_sequence_import_data")
set_prop(anim_data, "import_custom_attribute", True)
set_prop(anim_data, "delete_existing_custom_attribute_curves", True)

task.options = opts

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
unreal.EditorAssetLibrary.save_directory(DEST, False, True)

unreal.log("WL_QUATERNIUS_SOLDIER_RESULT_BEGIN")
for asset in unreal.EditorAssetLibrary.list_assets(DEST, True, False):
    unreal.log("WL_QUATERNIUS_SOLDIER_ASSET: " + asset)
unreal.log("WL_QUATERNIUS_SOLDIER_RESULT_END")
