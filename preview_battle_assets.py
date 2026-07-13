"""Renderiza previews Workbench vertex-color del lote tactico."""
import os
import sys

import bpy
import mathutils


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SRC = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle"


def setup_scene(width, height):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "FLAT"
    scene.display.shading.color_type = "VERTEX"
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = "WORLD"
    scene.render.resolution_x = width
    scene.render.resolution_y = height
    scene.render.resolution_percentage = 100
    scene.world = bpy.data.worlds.new("BattlePreviewWorld")
    scene.world.color = (0.36, 0.39, 0.34)
    return scene


def add_camera(scene, location, target, ortho_scale, name="PreviewCamera"):
    data = bpy.data.cameras.new(name)
    camera = bpy.data.objects.new(name, data)
    scene.collection.objects.link(camera)
    camera.location = location
    camera.rotation_euler = (mathutils.Vector(target) - mathutils.Vector(location)).to_track_quat("-Z", "Y").to_euler()
    data.type = "ORTHO"
    data.ortho_scale = ortho_scale
    scene.camera = camera


def import_one(filename, location=(0.0, 0.0, 0.0)):
    bpy.ops.import_scene.fbx(filepath=os.path.join(SRC, filename))
    obj = next(obj for obj in bpy.context.selected_objects if obj.type == "MESH")
    obj.location = location
    return obj


def render_field():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = setup_scene(1400, 900)
    scene.display.shading.show_shadows = False
    scene.display.shading.show_cavity = False
    import_one("battlefield_grassland.fbx")
    add_camera(scene, (118.0, -146.0, 158.0), (0.0, 0.0, -0.1), 245.0)
    scene.render.filepath = os.path.join(SRC, "preview_battlefield.png")
    bpy.ops.render.render(write_still=True)


def render_lineup(name, files, columns, spacing_x, spacing_y, width=1600, height=900):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = setup_scene(width, height)
    rows = (len(files) + columns - 1) // columns
    for index, filename in enumerate(files):
        col = index % columns
        row = index // columns
        x = (col - (columns - 1) * 0.5) * spacing_x
        y = (row - (rows - 1) * 0.5) * spacing_y
        import_one(filename, (x, y, 0.0))
    span_x = max(spacing_x * columns, 20.0)
    span_y = max(spacing_y * rows, 16.0)
    camera_y = -span_y * 1.45
    camera_z = max(18.0, span_y * 1.05)
    add_camera(scene, (0.0, camera_y, camera_z), (0.0, 0.0, 1.8), max(span_y * 1.78, span_x * 0.72))
    scene.render.filepath = os.path.join(SRC, name)
    bpy.ops.render.render(write_still=True)


render_field()
render_lineup(
    "preview_battle_buildings.png",
    [
        "battle_house.fbx", "battle_block.fbx", "battle_warehouse.fbx", "battle_gas_station.fbx",
        "battle_house_ruin.fbx", "battle_block_ruin.fbx", "battle_warehouse_ruin.fbx", "battle_gas_station_ruin.fbx",
    ],
    4, 22.0, 20.0, 1700, 900,
)
render_lineup(
    "preview_battle_nature_forts.png",
    [
        "battle_tree_broadleaf_a.fbx", "battle_tree_broadleaf_b.fbx", "battle_tree_conifer.fbx",
        "battle_shrub_a.fbx", "battle_shrub_b.fbx", "battle_fallen_log.fbx",
        "battle_fort_sandbags.fbx", "battle_fort_trench_straight.fbx", "battle_fort_trench_corner.fbx",
        "battle_fort_bunker.fbx", "battle_fort_wire.fbx", "battle_fort_checkpoint.fbx",
    ],
    4, 17.0, 15.0, 1700, 1000,
)
render_lineup(
    "preview_battle_units_props.png",
    [
        "battle_banner_ve.fbx", "battle_banner_co.fbx", "battle_wreck_mbt.fbx", "battle_wreck_ifv.fbx",
        "battle_wreck_apc.fbx", "battle_wreck_artillery.fbx", "battle_wreck_sam.fbx", "battle_wreck_heli.fbx",
        "battle_soldier_kneeling.fbx", "battle_soldier_prone.fbx", "battle_prop_rocks_a.fbx", "battle_prop_rocks_b.fbx",
        "battle_prop_fence.fbx", "battle_prop_utility_pole.fbx", "battle_prop_crater.fbx",
    ],
    5, 14.0, 12.0, 1800, 1000,
)
print("WL_BATTLE_PREVIEW_DONE")
