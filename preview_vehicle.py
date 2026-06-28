"""Render rapido de un veh_*.fbx con vertex color UNLIT (igual que en UE) para verificar el aspecto
sin pasar por el reimport de Unreal. Workbench FLAT + color VERTEX = unlit vertex color.
Uso: blender --background --python preview_vehicle.py -- <veh.fbx> <out_prefix>"""
import bpy, sys, mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
FBX, PREFIX = argv[0], argv[1]

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
sc.world.color = (0.50, 0.56, 0.38)  # fondo verdoso tipo terreno

dim = max(obj.dimensions.x, obj.dimensions.y, obj.dimensions.z)
look = (0, 0, obj.dimensions.z * 0.45)

def render(name, loc, ortho=None):
    cam = bpy.data.objects.new(name, bpy.data.cameras.new(name))
    sc.collection.objects.link(cam)
    cam.location = loc
    cam.rotation_euler = (mathutils.Vector(look) - mathutils.Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    if ortho:
        cam.data.type = 'ORTHO'; cam.data.ortho_scale = ortho
    sc.camera = cam
    sc.render.filepath = PREFIX + name + ".png"
    bpy.ops.render.render(write_still=True)

# 3/4 hero (frente-derecha), lateral y top: las vistas que delatan proporciones de un vehiculo.
render("_hero",  (dim * 0.95, -dim * 0.85, dim * 0.62), ortho=dim * 1.25)
render("_side",  (0.2, -dim * 1.3, dim * 0.28), ortho=dim * 1.2)
render("_top",   (0, 0, dim * 1.5), ortho=dim * 1.15)
print("WL_PREVIEW_DONE")
