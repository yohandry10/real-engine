"""Render en fila de los assets de naturaleza (unlit vertex color, igual que UE) para revisarlos."""
import bpy, os, math, mathutils

bpy.ops.wm.read_factory_settings(use_empty=True)
SRC = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated"
KINDS = ["conifer", "broadleaf", "palm", "rock", "peak", "mount"]

x = 0.0
maxh = 0.0
for k in KINDS:
    bpy.ops.import_scene.fbx(filepath=os.path.join(SRC, "nature_%s.fbx" % k))
    obj = bpy.context.selected_objects[0]
    obj.location.x = x
    x += max(9.0, obj.dimensions.x * 1.2 + 5.0)
    maxh = max(maxh, obj.dimensions.z)

sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.light = 'FLAT'
sc.display.shading.color_type = 'VERTEX'
sc.render.resolution_x = 1500
sc.render.resolution_y = 480
sc.world = bpy.data.worlds.new("W")
sc.world.color = (0.52, 0.60, 0.40)

cx = x * 0.5 - 4.5
cam_data = bpy.data.cameras.new("C")
cam = bpy.data.objects.new("C", cam_data)
sc.collection.objects.link(cam)
cam.location = (cx, -x * 0.75, maxh * 0.85)
look = mathutils.Vector((cx, 0.0, maxh * 0.42))
cam.rotation_euler = (look - mathutils.Vector(cam.location)).to_track_quat('-Z', 'Y').to_euler()
cam_data.type = 'ORTHO'
cam_data.ortho_scale = x * 1.04
sc.camera = cam
sc.render.filepath = os.path.join(SRC, "prev_nature_lineup.png")
bpy.ops.render.render(write_still=True)
print("WL_PREVIEW_DONE")
