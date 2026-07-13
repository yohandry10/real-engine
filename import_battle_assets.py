"""Importa el lote tactico FBX en /Game/GenBattle y aplica material vertex-color unlit."""
import os

import unreal


SRC = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle"
DEST = "/Game/GenBattle"
FILES = [
    "battlefield_grassland.fbx",
    "battle_house.fbx", "battle_house_ruin.fbx",
    "battle_block.fbx", "battle_block_ruin.fbx",
    "battle_warehouse.fbx", "battle_warehouse_ruin.fbx",
    "battle_gas_station.fbx", "battle_gas_station_ruin.fbx",
    "battle_tree_broadleaf_a.fbx", "battle_tree_broadleaf_b.fbx", "battle_tree_conifer.fbx",
    "battle_shrub_a.fbx", "battle_shrub_b.fbx", "battle_fallen_log.fbx",
    "battle_banner_ve.fbx", "battle_banner_co.fbx",
    "battle_wreck_mbt.fbx", "battle_wreck_ifv.fbx", "battle_wreck_apc.fbx",
    "battle_wreck_artillery.fbx", "battle_wreck_sam.fbx", "battle_wreck_heli.fbx",
    "battle_fort_sandbags.fbx", "battle_fort_trench_straight.fbx", "battle_fort_trench_corner.fbx",
    "battle_fort_bunker.fbx", "battle_fort_wire.fbx", "battle_fort_checkpoint.fbx",
    "battle_soldier_kneeling.fbx", "battle_soldier_prone.fbx",
    "battle_soldier_kneeling_desert.fbx", "battle_soldier_prone_desert.fbx",
    "battle_prop_rocks_a.fbx", "battle_prop_rocks_b.fbx", "battle_prop_fence.fbx",
    "battle_prop_utility_pole.fbx", "battle_prop_crater.fbx",
]


def get_or_create_material():
    path = DEST + "/M_BattleUnlit"
    material = unreal.EditorAssetLibrary.load_asset(path)
    if material:
        return material
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset("M_BattleUnlit", DEST, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -300, 0
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        vertex_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


missing = [name for name in FILES if not os.path.isfile(os.path.join(SRC, name))]
if missing:
    raise RuntimeError("Faltan FBX: " + ", ".join(missing))

tasks = []
for filename in FILES:
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, filename)
    task.destination_path = DEST
    task.automated = True
    task.save = True
    task.replace_existing = True
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.generate_lightmap_u_vs = True
    options.static_mesh_import_data.auto_generate_collision = False
    options.static_mesh_import_data.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE
    task.options = options
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
material = get_or_create_material()
for filename in FILES:
    name = os.path.splitext(filename)[0]
    mesh = unreal.EditorAssetLibrary.load_asset(DEST + "/" + name)
    if not mesh:
        raise RuntimeError("No se importo " + name)
    mesh.set_material(0, material)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

unreal.EditorAssetLibrary.save_directory(DEST, False, True)
unreal.log("WL_BATTLE_IMPORT_DONE: %d assets" % len(FILES))
