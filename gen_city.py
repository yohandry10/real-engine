"""
Genera una CIUDAD 3D cohesiva (un FBX) con VERTEX COLORS (no materiales lit):
cada cara lleva color base x sombreado falso por orientacion (techo claro, lados graduados) ->
da volumen SIN luz real. En UE se le asigna el mismo VertexColorMaterial UNLIT del terreno, asi
la ciudad es coherente con el mapa y NUNCA sale oscura (ni por relieve ni por sombras).

Objetivo de lectura: que LEA como ciudad (no como mancha): cuadricula de CALLES VISIBLES (aceras
claras vs asfalto oscuro = alto contraste), edificios SEPARADOS con hueco de acera entre ellos,
mucho VERDE (parques, plazas, arboles de calle), borde de baja altura y centro alto (skyline),
marcas de carril/pasos de cebra, landmark central para la capital.

Uso:  blender.exe --background --python gen_city.py -- "<salida.fbx>" <large|medium|small> <seed>
"""
import bpy, bmesh, math, random, sys, os

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT  = argv[0] if len(argv) > 0 else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\city_large.fbx"
SIZE = argv[1] if len(argv) > 1 else "large"
SEED = int(argv[2]) if len(argv) > 2 else 7
random.seed(SEED)

bpy.ops.wm.read_factory_settings(use_empty=True)

# === PALETA (UNLIT vertex color = literalmente lo que se ve) ===
# Antes todo era OSCURO (acera 0.50, asfalto 0.32, cristal 0.24) -> ciudad = mancha oscura sin
# calles. Ahora tonos MEDIO-CLAROS con CONTRASTE: aceras/plazas CLARAS que resaltan sobre el
# asfalto OSCURO -> la cuadricula de calles se LEE. (No pasar de ~0.70 en base: el shade del techo
# x1.12 satura a blanco; ver Docs/CAMPAIGN3D_CITY_BLENDER.md §5 #5.)
# Edificios: concreto claro dominante + variaciones (tan, gris frio, crema) + cristal claro.
C_WALL   = (0.58, 0.59, 0.62)
C_WALL2  = (0.60, 0.50, 0.39)
C_WALL3  = (0.49, 0.57, 0.66)
C_WALL4  = (0.64, 0.62, 0.54)
C_GLASS  = (0.40, 0.58, 0.70)
C_GLASS2 = (0.40, 0.58, 0.60)
C_ROOF   = (0.38, 0.38, 0.41)
C_ROOFT  = (0.58, 0.33, 0.22)   # azotea terracota (residencial) -> variacion calida tipo ciudad real
# Suelo: ASFALTO oscuro (la calle) + ACERA clara (alto contraste = se ven las calles) + plaza clara.
C_STREET = (0.24, 0.25, 0.27)
C_SIDE   = (0.66, 0.67, 0.69)
C_PLAZA  = (0.70, 0.65, 0.52)
C_FOUND  = (0.27, 0.27, 0.29)
C_PARK   = (0.32, 0.56, 0.30)
C_GRASS  = (0.34, 0.46, 0.22)  # apron verde/oliva para fundir con el terreno sin verse como losa crema
C_TREE   = (0.22, 0.48, 0.24)
C_TREE2  = (0.27, 0.54, 0.29)
C_TRUNK  = (0.30, 0.20, 0.12)
C_CARS   = [(0.82, 0.82, 0.84), (0.66, 0.20, 0.18), (0.18, 0.24, 0.44), (0.30, 0.50, 0.62)]
C_LINE   = (0.90, 0.76, 0.26)   # linea de carril amarilla brillante
C_CROSS  = (0.78, 0.78, 0.80)   # paso de cebra / marcas claras
C_ACCENT = (0.86, 0.70, 0.32)
WALLS = [C_WALL, C_WALL2, C_WALL3, C_WALL4]
GLASS = [C_GLASS, C_GLASS2]

# Shade falso por cara (orden: bottom, top, -Y, +X, +Y, -X). Sol fake desde +X +Z. Techo suave.
SHADES = [0.62, 1.14, 0.88, 1.05, 0.95, 0.78]

bm = bmesh.new()
col_layer = bm.loops.layers.color.new("Col")

def add_box(cx, cy, z0, sx, sy, sz, col):
    x0, x1 = cx - sx / 2, cx + sx / 2
    y0, y1 = cy - sy / 2, cy + sy / 2
    z1 = z0 + sz
    v = [bm.verts.new(p) for p in [
        (x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
        (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]]
    quads = [(0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
    for fi, q in enumerate(quads):
        face = bm.faces.new([v[i] for i in q])
        s = SHADES[fi]
        c = (col[0] * s, col[1] * s, col[2] * s, 1.0)
        for loop in face.loops:
            loop[col_layer] = c

def tree(cx, cy, z, s=1.0):
    t = random.choice([C_TREE, C_TREE2])
    add_box(cx, cy, z, 0.7 * s, 0.7 * s, 2.3 * s, C_TRUNK)
    add_box(cx, cy, z + 2.1 * s, 2.9 * s, 2.9 * s, 2.9 * s, t)
    add_box(cx, cy, z + 4.4 * s, 1.9 * s, 1.9 * s, 2.1 * s, t)

def car(cx, cy, z, along_x):
    col = random.choice(C_CARS)
    w, d = (4.0, 1.9) if along_x else (1.9, 4.0)
    add_box(cx, cy, z, w, d, 1.3, col)
    add_box(cx, cy, z + 1.3, w * 0.55, d * 0.6, 0.9, col)

def roof_details(cx, cy, z, w, d, height, resid):
    # Cap de azotea: en residencial bajo se VARIA (terracota / gris / concreto claro) para que no sea
    # un mar de techos rojos uniformes; gris en torres.
    cap = random.choice([C_ROOFT, C_ROOF, C_SIDE, C_ROOFT]) if resid else C_ROOF
    add_box(cx, cy, z, w * 1.04, d * 1.04, 0.8, cap)
    z += 0.8
    for _ in range(random.randint(1, 2)):
        ox = cx + (random.random() - 0.5) * w * 0.55
        oy = cy + (random.random() - 0.5) * d * 0.55
        add_box(ox, oy, z, w * 0.15, d * 0.15, 1.6, C_ROOF)  # tanque/AC
    if height > 48 and random.random() < 0.5:
        add_box(cx, cy, z, w * 0.4, d * 0.4, 2.4, C_ROOF)
        add_box(cx, cy, z + 2.4, w * 0.05, d * 0.05, height * 0.14, C_ACCENT)  # antena
    elif height > 30 and random.random() < 0.35:
        add_box(cx, cy, z, w * 0.5, d * 0.5, 0.3, C_ROOF)
        add_box(cx, cy, z + 0.3, w * 0.28, d * 0.28, 0.14, C_LINE)  # helipuerto

def building(cx, cy, w, d, height, z0, kind):
    wall = random.choice(WALLS)
    glass = random.choice(GLASS)
    z = z0
    if kind == "podium":
        add_box(cx, cy, z, w, d, 4.6, wall); z += 4.6
        add_box(cx, cy, z, w * 0.99, d * 0.99, 0.4, glass); z += 0.4
        w *= 0.74; d *= 0.74
    add_box(cx, cy, z, w, d, 2.2, wall); z += 2.2
    body_z0, cw, cd = z, w, d
    avail = max(5.0, height - (z - z0))
    n = max(1, int(round(avail / 6.4)))
    setback = random.randint(2, max(3, n))
    for i in range(n):
        gh = 4.6 if kind != "resid" else 3.2
        add_box(cx, cy, z, cw * 0.99, cd * 0.99, gh, glass); z += gh
        add_box(cx, cy, z, cw, cd, 1.8, wall); z += 1.8
        if i == setback and n > 5:
            cw *= 0.80; cd *= 0.80
    if kind == "tower":
        bh = z - body_z0
        add_box(cx, cy, body_z0, w * 1.006, 0.32, bh, wall)   # mullion vertical (simula ventanas)
        add_box(cx, cy, body_z0, 0.32, d * 1.006, bh, wall)
    roof_details(cx, cy, z, cw, cd, height, kind == "resid")

# === LAYOUT ===
# N manzanas por lado, HMAX = altura tope del skyline (capital baja de 135 a 92 para no hacer agujas
# ver Docs §5 #9). Calles ANCHAS (street/pitch ~28%) para que la cuadricula se lea de lejos.
cfg = {"large": (8, 88.0), "medium": (5, 66.0), "small": (4, 32.0)}[SIZE]
N, HMAX = cfg
block, street = 26.0, 10.0
pitch = block + street
span = N * pitch
PODIUM = 60.0
half = (N - 1) / 2.0
PARK_P = 0.13 if SIZE == "large" else 0.11

# --- Suelo ---
# Apron de pasto (geometria Blender, no codigo): anillo verde que funde la ciudad con el terreno.
add_box(0, 0, -1.0, span + street + 52.0, span + street + 52.0, 1.3, C_GRASS)
# Calzada de ASFALTO (la calle) + cimiento que se entierra en el relieve.
add_box(0, 0, -PODIUM, span + street, span + street, PODIUM + 0.25, C_STREET)
add_box(0, 0, -PODIUM, span + street + 1.0, span + street + 1.0, PODIUM, C_FOUND)

# Marcas de carril (amarillas) en CADA calle de la cuadricula -> red vial graficamente clara desde
# arriba. grid = centros de calle (N+1 lineas). Pasos de cebra (claros) en cada cruce.
grid = [(i - N / 2.0) * pitch for i in range(N + 1)]
road_len = span + street
for g in grid:
    add_box(0.0, g, 0.30, road_len, 0.5, 0.06, C_LINE)   # linea calle horizontal
    add_box(g, 0.0, 0.30, 0.5, road_len, 0.06, C_LINE)   # linea calle vertical
for gx in grid:
    for gy in grid:
        for s in (-1, 1):
            add_box(gx + s * street * 0.32, gy, 0.31, street * 0.16, block * 0.5, 0.05, C_CROSS)
            add_box(gx, gy + s * street * 0.32, 0.31, block * 0.5, street * 0.16, 0.05, C_CROSS)
# Coches sueltos en algunas calles.
for g in grid[1:-1]:
    if random.random() < 0.6:
        car(g, random.uniform(-span / 2, span / 2), 0.33, False)
    if random.random() < 0.6:
        car(random.uniform(-span / 2, span / 2), g, 0.33, True)

def pick_kind(h):
    if h > HMAX * 0.55:
        return "tower"
    if h > HMAX * 0.30:
        return random.choice(["podium", "tower"])
    return "resid"

def place_lot(ox, oy, cell, dist):
    # Edificio que NO llena la celda: deja hueco de acera alrededor (se ven veredas entre edificios).
    fp = cell * random.uniform(0.58, 0.74)
    # CLAVE para que se VEA el suelo/calles/verde (como la maqueta): la MAYORIA de la ciudad es BAJA
    # (2-5 pisos) y solo un NUCLEO central pequeno es alto (downtown). Si todo es alto, desde el angulo
    # de camara (~46 grados) los edificios tapan el suelo entre ellos. Falloff fuerte = nucleo compacto.
    falloff = max(0.0, 1.0 - dist * 1.5) ** 2.5
    base_h = HMAX * (0.06 + 0.94 * falloff)
    h = max(7.0, base_h * (0.6 + random.random() * 0.7))
    if dist < 0.22 and random.random() < 0.40:
        h *= 1.45
    building(ox, oy, fp, fp, h, 1.0, pick_kind(h))

for ix in range(N):
    for iy in range(N):
        bx = (ix - half) * pitch
        by = (iy - half) * pitch
        dist = math.hypot(bx, by) / max(1.0, span / 2)
        # Manzana = losa de ACERA clara (esto, separado por el asfalto oscuro, dibuja la cuadricula).
        add_box(bx, by, 0.5, block, block, 0.42, C_SIDE)

        # Landmark central de la capital (una torre iconica, no un bosque de agujas).
        if SIZE == "large" and ix == int(half) and iy == int(half):
            add_box(bx, by, 0.92, block * 0.86, block * 0.86, 0.18, C_PLAZA)  # explanada
            building(bx, by, block * 0.42, block * 0.42, HMAX * 1.35, 1.0, "tower")
            add_box(bx, by, 1.0 + HMAX * 1.35 * 0.93, block * 0.07, block * 0.07, HMAX * 0.16, C_ACCENT)
            for a in range(8):
                ang = a / 8.0 * math.tau
                tree(bx + math.cos(ang) * block * 0.40, by + math.sin(ang) * block * 0.40, 0.92)
            continue

        # Parque: manzana verde con arboles + caminos. Algo mas frecuente al borde (cinturon verde).
        if random.random() < PARK_P + (0.06 if dist > 0.62 else 0.0):
            add_box(bx, by, 0.92, block * 0.9, block * 0.9, 0.2, C_PARK)
            add_box(bx, by, 0.95, block * 0.16, block * 0.9, 0.06, C_SIDE)  # camino
            add_box(bx, by, 0.95, block * 0.9, block * 0.16, 0.06, C_SIDE)
            for _ in range(5):
                tree(bx + (random.random() - 0.5) * block * 0.78,
                     by + (random.random() - 0.5) * block * 0.78, 1.1, random.uniform(0.8, 1.2))
            continue

        # Manzana edificada DENSA: rejilla de lotes casi llena (hueco fino de acera) -> bloques densos
        # SEPARADOS por las calles (asfalto oscuro) = lectura de ciudad real, no casas dispersas.
        lots = 3
        cell = block / lots
        for lx in range(lots):
            for ly in range(lots):
                ox = bx + (lx - (lots - 1) / 2.0) * cell
                oy = by + (ly - (lots - 1) / 2.0) * cell
                if random.random() < 0.06:        # muy pocos huecos -> manzana llena
                    continue
                place_lot(ox, oy, cell, dist)
        # Arbol de calle ocasional en el borde (arbolado, sin saturar ni dispersar).
        if random.random() < 0.3:
            sgn = random.choice((-1, 1))
            tree(bx + sgn * block * 0.5, by + random.uniform(-1, 1) * block * 0.3, 0.92, 0.85)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me = bpy.data.meshes.new("City")
bm.to_mesh(me); bm.free()
mat = bpy.data.materials.new("CityVColor")  # placeholder; UE usara VertexColorMaterial
me.materials.append(mat)
ob = bpy.data.objects.new("City", me)
bpy.context.scene.collection.objects.link(ob)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.object.select_all(action='DESELECT')
ob.select_set(True)
bpy.context.view_layer.objects.active = ob
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, apply_unit_scale=True,
                         mesh_smooth_type='FACE', colors_type='LINEAR')
print("WL_GENCITY_DONE: %s verts=%d polys=%d" % (OUT, len(me.vertices), len(me.polygons)))
