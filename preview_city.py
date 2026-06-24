"""Render rapido de un city_*.fbx con vertex color UNLIT (igual que en UE) para verificar el
aspecto sin pasar por el reimport de Unreal. Workbench FLAT + color VERTEX = unlit vertex color.
Uso: blender --background --python preview_city.py -- <city.fbx> <out_prefix>"""
import bpy, sys, math, mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
FBX = argv[0]
PREFIX = argv[1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=FBX)
obj = next(o for o in bpy.context.scene.objects if o.type == 'MESH')

sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.light = 'FLAT'
sc.display.shading.color_type = 'VERTEX'
sc.render.resolution_x = 1100
sc.render.resolution_y = 760
sc.world = bpy.data.worlds.new("W")
sc.world.color = (0.55, 0.62, 0.40)  # fondo verdoso tipo terreno

cx = cy = 0.0
dim = max(obj.dimensions.x, obj.dimensions.y)
top = obj.location.z + obj.dimensions.z

def render(name, loc, look, ortho=None):
    cam_data = bpy.data.cameras.new(name)
    cam = bpy.data.objects.new(name, cam_data)
    sc.collection.objects.link(cam)
    cam.location = loc
    d = (mathutils.Vector(look) - mathutils.Vector(loc))
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    if ortho:
        cam_data.type = 'ORTHO'
        cam_data.ortho_scale = ortho
    sc.camera = cam
    sc.render.filepath = PREFIX + name + ".png"
    bpy.ops.render.render(write_still=True)

# Top-down: para ver la CUADRICULA de calles/parques.
render("_top", (0, 0, dim * 1.4), (0, 0, 0), ortho=dim * 1.15)
# Oblicuo a ~46 grados (angulo actual de la camara cercana).
render("_obl", (dim * 0.74, -dim * 0.74, dim * 1.08), (0, 0, top * 0.12))
# Oblicuo a ~62 grados (mas cenital): para comprobar si un angulo mas picado revela calles/suelo.
render("_steep", (dim * 0.50, -dim * 0.50, dim * 1.33), (0, 0, top * 0.10))
print("WL_PREVIEW_DONE")
