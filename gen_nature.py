"""
Genera ASSETS DE NATURALEZA low-poly con VERTEX COLORS (unlit), para que peguen con el mapa de
Campaign 3D. Cada cara: color base x sombreado falso por orientacion (sol fake +X+Z) -> volumen
SIN luz real. Tonos VIVOS con buen rango de sombra para que RESALTEN sobre el terreno.

Tipos (kind): conifer, broadleaf, palm, rock, peak (pico andino con nieve), mount (montaña verde).
Uso:  blender --background --python gen_nature.py -- "<salida.fbx>" <kind> <seed>
"""
import bpy, bmesh, math, random, sys, os, mathutils

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT  = argv[0] if len(argv) > 0 else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\nature_conifer.fbx"
KIND = argv[1] if len(argv) > 1 else "conifer"
SEED = int(argv[2]) if len(argv) > 2 else 1
random.seed(SEED)

bpy.ops.wm.read_factory_settings(use_empty=True)

# Paleta VIVA. Arboles = verde bosque rico (mas oscuro/saturado que el terreno olivo -> contrastan
# y "resaltan", con la cara al sol bien luminosa = volumen).
LEAF = [(0.15, 0.44, 0.17), (0.20, 0.52, 0.21), (0.12, 0.38, 0.15), (0.26, 0.54, 0.22), (0.17, 0.48, 0.18)]
TRUNK = (0.34, 0.23, 0.13)
PALM_TRUNK = (0.47, 0.39, 0.24)
PALM_LEAF = (0.24, 0.54, 0.22)
COCO = (0.30, 0.22, 0.12)
ROCK = (0.48, 0.47, 0.50)
ROCK2 = (0.38, 0.37, 0.36)
ANDES_ROCK = (0.42, 0.38, 0.33)
ANDES_DARK = (0.32, 0.29, 0.26)
MOUNT_GRASS = (0.22, 0.42, 0.17)
MOUNT_ROCK = (0.42, 0.39, 0.34)
SNOW = (0.93, 0.95, 0.98)

bm = bmesh.new()
col = bm.loops.layers.color.new("Col")
SUN = mathutils.Vector((0.55, -0.25, 0.78)).normalized()

def colorize(faces, base):
    for f in faces:
        f.normal_update()
        d = max(0.0, f.normal.dot(SUN))
        s = 0.58 + 0.78 * d  # 0.58 sombra -> 1.36 al sol (rango amplio = resalta + volumen)
        c = (min(1.0, base[0] * s), min(1.0, base[1] * s), min(1.0, base[2] * s), 1.0)
        for lp in f.loops:
            lp[col] = c

def newverts_faces(before):
    nf = [f for f in bm.faces if f not in before]
    nv = set(v for f in nf for v in f.verts)
    return nv, nf

def cone(cx, cy, z0, r1, r2, depth, seg, base, jitter=0.0, mat=None):
    before = set(bm.faces)
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg,
                          radius1=r1, radius2=max(0.03, r2), depth=depth)
    nv, nf = newverts_faces(before)
    for v in nv:
        v.co.z += depth / 2.0
        if jitter:
            v.co.x += (random.random() - 0.5) * jitter * r1
            v.co.y += (random.random() - 0.5) * jitter * r1
        if mat is not None:
            v.co = mat @ v.co
        v.co.x += cx; v.co.y += cy; v.co.z += z0
    colorize(nf, base)

def sphere(cx, cy, cz, radius, subdiv, base, sz=1.0, jitter=0.0):
    before = set(bm.faces)
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=radius)
    nv, nf = newverts_faces(before)
    for v in nv:
        if jitter:
            v.co.x += (random.random() - 0.5) * jitter * radius
            v.co.y += (random.random() - 0.5) * jitter * radius
            v.co.z += (random.random() - 0.5) * jitter * radius
        v.co.x += cx; v.co.y += cy; v.co.z = v.co.z * sz + cz
    colorize(nf, base)

def box(cx, cy, z0, sx, sy, sz, base):
    before = set(bm.faces)
    bmesh.ops.create_cube(bm, size=1.0)
    nv, nf = newverts_faces(before)
    for v in nv:
        v.co.x = v.co.x * sx + cx
        v.co.y = v.co.y * sy + cy
        v.co.z = v.co.z * sz + z0 + sz / 2.0
    colorize(nf, base)

if KIND == "conifer":
    leaf = random.choice(LEAF)
    box(0, 0, 0, 0.5, 0.5, 2.2, TRUNK)
    zb = 1.5
    for i in range(5):
        r = 2.8 - i * 0.46
        h = 2.1 - i * 0.10
        cone(0, 0, zb, r, 0.0, h + 0.9, 11, leaf, jitter=0.13)
        zb += h * 0.60

elif KIND == "broadleaf":
    leaf, leaf2 = random.choice(LEAF), random.choice(LEAF)
    box(0, 0, 0, 0.6, 0.6, 2.9, TRUNK)
    crown = [(0, 0, 4.5, 2.6), (1.6, 0.3, 3.9, 1.8), (-1.4, -0.5, 4.0, 1.8),
             (0.4, 1.5, 4.0, 1.6), (-0.3, -1.4, 4.1, 1.5), (0.2, 0.1, 5.6, 1.6)]
    for (cxx, cyy, czz, rr) in crown:
        sphere(cxx, cyy, czz, rr, 1, random.choice([leaf, leaf2]), sz=0.92, jitter=0.20)

elif KIND == "palm":
    x = z = 0.0
    lean = 0.0
    for i in range(8):
        box(x, 0, z, 0.46 - i * 0.026, 0.46 - i * 0.026, 1.02, PALM_TRUNK)
        z += 0.94; lean += 0.045; x += lean
    top = (x, 0.0, z)
    sphere(top[0], top[1], top[2] - 0.3, 0.55, 1, COCO, sz=1.0, jitter=0.1)
    for k in range(9):
        a = k / 9.0 * math.tau
        droop = math.radians(48 + (k % 3) * 14)
        rot = mathutils.Matrix.Rotation(a, 4, 'Z') @ mathutils.Matrix.Rotation(droop, 4, 'Y')
        cone(top[0], top[1], top[2], 0.58, 0.04, 4.4, 4, PALM_LEAF, mat=rot)

elif KIND == "rock":
    sphere(0, 0, 0.7, 1.8, 1, ROCK, sz=0.62, jitter=0.36)
    sphere(1.1, 0.6, 0.5, 1.1, 1, ROCK2, sz=0.6, jitter=0.36)
    sphere(-0.8, -0.7, 0.4, 0.8, 1, ROCK, sz=0.55, jitter=0.36)

elif KIND == "peak":
    # Pico andino: cono rocoso alto y afilado + arista secundaria + casquete de nieve en el tercio alto.
    cone(0, 0, 0, 4.6, 0.3, 9.8, 8, ANDES_ROCK, jitter=0.17)
    cone(1.7, 0.9, 0, 2.3, 0.2, 5.8, 7, ANDES_DARK, jitter=0.19)
    cone(0, 0, 6.6, 1.8, 0.05, 3.4, 8, SNOW, jitter=0.11)

elif KIND == "mount":
    # Montaña verde redondeada con cumbre rocosa.
    sphere(0, 0, -1.0, 5.3, 2, MOUNT_GRASS, sz=0.60, jitter=0.18)
    cone(0, 0, 1.7, 2.2, 0.5, 2.2, 9, MOUNT_ROCK, jitter=0.22)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me = bpy.data.meshes.new("Nature")
bm.to_mesh(me); bm.free()
me.materials.append(bpy.data.materials.new("NatureVColor"))
ob = bpy.data.objects.new("Nature", me)
bpy.context.scene.collection.objects.link(ob)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.object.select_all(action='DESELECT')
ob.select_set(True)
bpy.context.view_layer.objects.active = ob
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, apply_unit_scale=True,
                         mesh_smooth_type='FACE', colors_type='LINEAR')
print("WL_NATURE_DONE: %s kind=%s verts=%d polys=%d" % (OUT, KIND, len(me.vertices), len(me.polygons)))
