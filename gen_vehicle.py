"""
Genera VEHICULOS MILITARES MODERNOS low-poly con VERTEX COLORS (unlit), coherentes con el mapa de
Campaign 3D (mismo enfoque que gen_city.py / gen_nature.py): cada cara = color base x sombreado
falso por orientacion (sol fake +X+Z) -> volumen SIN luz real. En UE se les asigna el mismo
VertexColorMaterial UNLIT del terreno, asi NUNCA salen oscuros (ni por relieve ni por sombras).

El vehiculo MIRA hacia +X (el canon apunta a +X). Origen: z=0 = suelo (parte baja de las orugas/
ruedas), centrado en XY. Una sola malla cohesiva por vehiculo (rapido, robusto).

Tipos (kind):
  mbt      -> Main Battle Tank moderno (Abrams / Leopard 2): glacis, orugas, torreta, canon con manguito.
  ifv      -> Vehiculo de combate de infanteria oruga (Bradley / CV90): canon automatico, ATGM.
  apc      -> Transporte blindado 8x8 con ruedas (Stryker / Guarani): RWS, 4 ruedas por lado.
  soldier  -> Soldado de infanteria moderno de pie con fusil, casco y chaleco (para formaciones de tropa).
  aircraft -> Caza moderno (F-16 / Su): fuselaje, alas en delta, cabina, cola, tren simple.
  ship     -> Fragata/destructor moderno: casco con proa en cuña, superestructura, mastil, torreta, helipuerto.

Todos: UNA malla cohesiva, miran a +X, 1 unidad ~= 1 metro (escalas coherentes entre tipos para formaciones).
Camuflaje (camo, opcional 4o arg): green (OTAN, por defecto) | desert (arena) | grey (urbano).
  (soldier usa camo; aircraft/ship usan gris militar fijo.)

Uso:  blender --background --python gen_vehicle.py -- "<salida.fbx>" <mbt|ifv|apc|soldier|aircraft|ship> <seed> [camo]
"""
import bpy, bmesh, math, random, sys, os, mathutils

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT  = argv[0] if len(argv) > 0 else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\veh_mbt.fbx"
KIND = argv[1] if len(argv) > 1 else "mbt"
SEED = int(argv[2]) if len(argv) > 2 else 1
CAMO = argv[3] if len(argv) > 3 else "green"
random.seed(SEED)

bpy.ops.wm.read_factory_settings(use_empty=True)

# === PALETA (UNLIT vertex color = literalmente lo que se ve) ===
# Tonos MEDIOS (0.10-0.55): el sombreado por normal sube hasta x1.36, asi que base alta satura a
# blanco (ver Docs/CAMPAIGN3D_CITY_BLENDER.md #5). Camuflaje = base + 1-2 manchas mas oscuras.
CAMOS = {
    "green":  {"hull": (0.34, 0.39, 0.24), "blot": (0.24, 0.28, 0.17), "blot2": (0.42, 0.36, 0.21)},
    "desert": {"hull": (0.56, 0.48, 0.31), "blot": (0.44, 0.36, 0.23), "blot2": (0.40, 0.35, 0.25)},
    "grey":   {"hull": (0.40, 0.42, 0.44), "blot": (0.28, 0.30, 0.32), "blot2": (0.33, 0.34, 0.35)},
}
PAL    = CAMOS.get(CAMO, CAMOS["green"])
HULL   = PAL["hull"]
BLOT   = PAL["blot"]
BLOT2  = PAL["blot2"]
METAL  = (0.40, 0.41, 0.43)   # gunmetal: canon, mantelete
DARK   = (0.19, 0.19, 0.20)   # orugas, neumaticos, ranuras (oscuro pero NO negro -> lee unlit)
RUBBER = (0.16, 0.16, 0.17)
STEEL  = (0.50, 0.51, 0.53)   # detalles metalicos claros (ametralladora, ganchos)
GLASS  = (0.16, 0.26, 0.28)   # vision blocks / opticas
OPTIC  = (0.34, 0.50, 0.52)   # lente del visor (un toque mas claro = "lee" como cristal)
LIGHT  = (0.62, 0.58, 0.42)   # faro
SKIN   = (0.60, 0.45, 0.36)   # cara/manos del soldado
JET    = (0.44, 0.46, 0.49)   # gris caza
JET2   = (0.30, 0.32, 0.35)   # gris caza oscuro (panza/escape)
NAVY   = (0.42, 0.44, 0.46)   # gris naval (superestructura)
NAVY2  = (0.31, 0.33, 0.35)   # gris naval oscuro (casco)
DECK   = (0.33, 0.34, 0.31)   # cubierta de barco

bm = bmesh.new()
col = bm.loops.layers.color.new("Col")
SUN = mathutils.Vector((0.55, -0.25, 0.78)).normalized()

def colorize(faces, base):
    for f in faces:
        f.normal_update()
        d = max(0.0, f.normal.dot(SUN))
        s = 0.68 + 0.66 * d  # 0.68 sombra -> 1.34 al sol (piso alto = no se va a negro unlit)
        c = (min(1.0, base[0] * s), min(1.0, base[1] * s), min(1.0, base[2] * s), 1.0)
        for lp in f.loops:
            lp[col] = c

def newfaces(before):
    nf = [f for f in bm.faces if f not in before]
    nv = set(v for f in nf for v in f.verts)
    return nv, nf

def box(cx, cy, cz, sx, sy, sz, base, rot=None):
    """Caja centrada en (cx,cy,cz) de tamaños sx,sy,sz. rot = Matrix opcional (giro sobre el centro)."""
    before = set(bm.faces)
    bmesh.ops.create_cube(bm, size=1.0)
    nv, nf = newfaces(before)
    for v in nv:
        p = mathutils.Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz))
        if rot is not None:
            p = rot @ p
        v.co = (p.x + cx, p.y + cy, p.z + cz)
    colorize(nf, base)

def floorbox(cx, cy, z0, sx, sy, sz, base, rot=None):
    """Caja apoyada: z0 = base inferior."""
    box(cx, cy, z0 + sz / 2.0, sx, sy, sz, base, rot)

def cyl(cx, cy, cz, radius, length, axis, base, seg=12, r2=None):
    """Cilindro centrado en (cx,cy,cz). axis = 'x'|'y'|'z' (eje del cilindro). r2 = radio del otro
    extremo (cono) si se da."""
    before = set(bm.faces)
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg,
                          radius1=radius, radius2=(radius if r2 is None else max(0.01, r2)),
                          depth=length)
    nv, nf = newfaces(before)
    if axis == 'x':
        rot = mathutils.Matrix.Rotation(math.radians(90), 4, 'Y')
    elif axis == 'y':
        rot = mathutils.Matrix.Rotation(math.radians(90), 4, 'X')
    else:
        rot = mathutils.Matrix.Identity(4)
    for v in nv:
        p = rot @ v.co
        v.co = (p.x + cx, p.y + cy, p.z + cz)
    colorize(nf, base)

def ico(cx, cy, cz, r, base, subdiv=1, sx=1.0, sy=1.0, sz=1.0):
    """Esfera (icosfera) centrada en (cx,cy,cz), con escala por eje opcional (para domos/torsos)."""
    before = set(bm.faces)
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=r)
    nv, nf = newfaces(before)
    for v in nv:
        v.co = (v.co.x * sx + cx, v.co.y * sy + cy, v.co.z * sz + cz)
    colorize(nf, base)

def rotY(deg):
    return mathutils.Matrix.Rotation(math.radians(deg), 4, 'Y')

# =====================================================================================
#  MAIN BATTLE TANK  (estilo Abrams / Leopard 2)
# =====================================================================================
def build_mbt():
    HULL_L, HULL_W = 7.0, 2.7
    TRK_W, TRK_H = 0.6, 0.95
    ty = HULL_W / 2 + TRK_W / 2 - 0.04
    # --- Orugas (dos bloques largos oscuros) + faldones laterales ---
    for sgn in (-1, 1):
        floorbox(0.0, sgn * ty, 0.0, HULL_L, TRK_W, TRK_H, DARK)
        # Ruedas de rodadura (7) + tensora/motriz -> discos a la vista por el lado.
        for i in range(7):
            wx = -HULL_L / 2 + 0.7 + i * (HULL_L - 1.4) / 6.0
            cyl(wx, sgn * (ty + TRK_W / 2 - 0.02), 0.46, 0.46, 0.12, 'y', RUBBER, seg=12)
            cyl(wx, sgn * (ty + TRK_W / 2 - 0.05), 0.46, 0.20, 0.16, 'y', STEEL, seg=10)
        # Faldon blindado sobre la oruga (placa lateral).
        floorbox(0.2, sgn * (ty + 0.02), TRK_H - 0.05, HULL_L - 0.6, 0.10, 0.62, BLOT)
    # --- Casco: bandeja inferior + glacis inclinado al frente ---
    floorbox(0.0, 0.0, TRK_H - 0.15, HULL_L - 0.3, HULL_W, 0.55, HULL)
    hull_top = TRK_H - 0.15 + 0.55
    # Glacis (placa frontal muy inclinada).
    box(HULL_L / 2 - 0.55, 0.0, hull_top - 0.18, 1.5, HULL_W - 0.05, 0.10, BLOT, rot=rotY(38))
    # Cubierta del motor (rejillas traseras = bandas oscuras).
    floorbox(-HULL_L / 2 + 1.0, 0.0, hull_top, 1.8, HULL_W - 0.35, 0.18, HULL)
    for gx in range(4):
        floorbox(-HULL_L / 2 + 0.45 + gx * 0.42, 0.0, hull_top + 0.02, 0.16, HULL_W - 0.6, 0.16, DARK)
    # Guardabarros + faros delanteros.
    for sgn in (-1, 1):
        floorbox(HULL_L / 2 - 0.45, sgn * (HULL_W / 2 - 0.1), hull_top - 0.02, 0.5, 0.45, 0.16, LIGHT)
    # --- Torreta (centro-atras), frente inclinado + mantelete + cesta trasera ---
    TUR_L, TUR_W, TUR_H = 3.0, 2.45, 0.78
    tcx = -0.35
    tz = hull_top
    floorbox(tcx, 0.0, tz, TUR_L, TUR_W, TUR_H, HULL)
    # Manchas de camuflaje sobre la torreta y el casco.
    box(tcx + 0.4, 0.5, tz + TUR_H - 0.02, 1.1, 0.9, 0.06, BLOT, rot=None)
    box(tcx - 0.7, -0.6, tz + TUR_H - 0.02, 0.9, 0.8, 0.06, BLOT2)
    box(HULL_L / 2 - 1.6, 0.4, hull_top + 0.02, 1.3, 0.8, 0.06, BLOT2)
    # Frente inclinado de la torreta (cuña).
    box(tcx + TUR_L / 2 - 0.1, 0.0, tz + 0.28, 0.9, TUR_W - 0.1, 0.1, BLOT, rot=rotY(34))
    tur_front = tcx + TUR_L / 2
    # Mantelete (bloque del que sale el canon).
    floorbox(tur_front - 0.05, 0.0, tz + 0.16, 0.55, 1.0, 0.5, METAL)
    # --- Canon: manguito termico (grueso) + extractor de humos (bulto) + boca ---
    gun_z = tz + 0.42
    gun_x0 = tur_front + 0.2
    cyl(gun_x0 + 1.4, 0.0, gun_z, 0.16, 2.9, 'x', METAL, seg=14)          # manguito termico
    cyl(gun_x0 + 1.55, 0.0, gun_z, 0.225, 0.7, 'x', METAL, seg=14)         # extractor de humos (bulto)
    cyl(gun_x0 + 3.4, 0.0, gun_z, 0.12, 1.5, 'x', DARK, seg=14)            # tubo de la boca
    cyl(gun_x0 + 4.12, 0.0, gun_z, 0.14, 0.18, 'x', DARK, seg=14)          # bocacha
    # --- Cesta de estiba trasera (rejilla) ---
    floorbox(tcx - TUR_L / 2 - 0.25, 0.0, tz + 0.06, 0.5, TUR_W - 0.2, 0.5, DARK)
    floorbox(tcx - TUR_L / 2 - 0.25, 0.0, tz + 0.56, 0.5, TUR_W - 0.2, 0.04, STEEL)
    # --- Cupula del comandante + escotilla del cargador + ametralladoras ---
    cyl(tcx - 0.4, 0.55, tz + TUR_H + 0.11, 0.42, 0.22, 'z', HULL, seg=12)   # cupula comandante
    cyl(tcx + 0.5, -0.55, tz + TUR_H + 0.07, 0.34, 0.14, 'z', HULL, seg=12)  # escotilla cargador
    # Ametralladora pesada sobre la cupula.
    floorbox(tcx - 0.4, 0.55, tz + TUR_H + 0.22, 0.2, 0.2, 0.18, STEEL)
    cyl(tcx + 0.25, 0.55, tz + TUR_H + 0.34, 0.05, 1.1, 'x', DARK, seg=8)
    # Visor de puntería principal (con lente que "lee" como cristal).
    floorbox(tcx + 0.9, 0.45, tz + TUR_H - 0.02, 0.4, 0.4, 0.3, METAL)
    box(tcx + 1.12, 0.45, tz + TUR_H + 0.12, 0.06, 0.34, 0.24, OPTIC)
    # --- Lanza-botes de humo (racimos a ambos lados del frente de torreta) ---
    for sgn in (-1, 1):
        for j in range(4):
            cyl(tur_front - 0.5 - (j % 2) * 0.24, sgn * (TUR_W / 2 - 0.06 - (j // 2) * 0.0),
                tz + 0.42 + (j // 2) * 0.22, 0.07, 0.26, 'x', DARK, seg=8)
    # --- Antenas (dos varillas finas atras) ---
    for sgn in (-1, 1):
        cyl(tcx - TUR_L / 2 + 0.1, sgn * (TUR_W / 2 - 0.3), tz + TUR_H + 0.9, 0.025, 1.8, 'z', STEEL, seg=6)

# =====================================================================================
#  IFV  (vehiculo de combate de infanteria, oruga — estilo Bradley / CV90)
# =====================================================================================
def build_ifv():
    HULL_L, HULL_W = 6.0, 2.6
    TRK_W, TRK_H = 0.55, 0.9
    ty = HULL_W / 2 + TRK_W / 2 - 0.04
    for sgn in (-1, 1):
        floorbox(0.0, sgn * ty, 0.0, HULL_L, TRK_W, TRK_H, DARK)
        for i in range(6):
            wx = -HULL_L / 2 + 0.6 + i * (HULL_L - 1.2) / 5.0
            cyl(wx, sgn * (ty + TRK_W / 2 - 0.02), 0.44, 0.42, 0.12, 'y', RUBBER, seg=12)
            cyl(wx, sgn * (ty + TRK_W / 2 - 0.05), 0.44, 0.18, 0.16, 'y', STEEL, seg=10)
        floorbox(0.1, sgn * (ty + 0.02), TRK_H - 0.05, HULL_L - 0.5, 0.10, 0.6, BLOT)
    # Casco alto y anguloso (lleva tropa). Frente muy inclinado.
    floorbox(-0.2, 0.0, TRK_H - 0.15, HULL_L - 0.4, HULL_W, 1.05, HULL)
    hull_top = TRK_H - 0.15 + 1.05
    box(HULL_L / 2 - 0.7, 0.0, TRK_H + 0.5, 1.7, HULL_W - 0.06, 0.12, BLOT, rot=rotY(42))
    # Manchas de camuflaje.
    box(-1.4, 0.5, hull_top - 0.01, 1.4, 0.9, 0.05, BLOT)
    box(0.6, -0.6, hull_top - 0.01, 1.1, 0.8, 0.05, BLOT2)
    # Escotillas de techo de la tropa + puerta trasera (rampa).
    for hx in (-1.6, -0.7):
        floorbox(hx, 0.5, hull_top, 0.7, 0.7, 0.06, BLOT2)
    box(-HULL_L / 2 + 0.05, 0.0, TRK_H + 0.5, 0.1, HULL_W - 0.3, 1.0, BLOT2)  # rampa trasera
    # Faros.
    for sgn in (-1, 1):
        floorbox(HULL_L / 2 - 0.35, sgn * (HULL_W / 2 - 0.15), hull_top - 0.45, 0.3, 0.3, 0.14, LIGHT)
    # --- Torreta de dos plazas (pequeña, descentrada a la derecha) ---
    TUR_L, TUR_W, TUR_H = 1.7, 1.5, 0.7
    tcx, tcy = 0.4, -0.35
    tz = hull_top
    floorbox(tcx, tcy, tz, TUR_L, TUR_W, TUR_H, HULL)
    box(tcx + TUR_L / 2 - 0.1, tcy, tz + 0.25, 0.6, TUR_W - 0.05, 0.08, BLOT2, rot=rotY(30))
    # Cañón automatico (mas fino y largo que MBT).
    gun_z = tz + 0.42
    gx0 = tcx + TUR_L / 2
    cyl(gx0 + 1.5, tcy, gun_z, 0.09, 3.0, 'x', METAL, seg=12)
    cyl(gx0 + 3.05, tcy, gun_z, 0.07, 0.5, 'x', DARK, seg=10)  # apagallamas
    floorbox(gx0 - 0.05, tcy, tz + 0.18, 0.4, 0.5, 0.42, METAL)  # mantelete
    # Lanzador ATGM doble en el lateral de la torreta.
    box(tcx - 0.1, tcy + TUR_W / 2 + 0.18, tz + 0.55, 0.9, 0.34, 0.4, DARK, rot=rotY(-8))
    # Optica + escotilla del jefe + antena.
    box(tcx + 0.4, tcy - 0.3, tz + TUR_H + 0.04, 0.22, 0.3, 0.26, OPTIC)
    cyl(tcx - 0.3, tcy + 0.2, tz + TUR_H + 0.08, 0.3, 0.16, 'z', HULL, seg=12)
    cyl(tcx - TUR_L / 2, tcy - TUR_W / 2, tz + TUR_H + 0.9, 0.025, 1.8, 'z', STEEL, seg=6)

# =====================================================================================
#  APC 8x8  (transporte blindado con ruedas — estilo Stryker / Guarani)
# =====================================================================================
def build_apc():
    HULL_L, HULL_W = 6.4, 2.6
    WR = 0.62           # radio de rueda
    AX = WR             # altura del eje
    body_z0 = WR + 0.18
    # --- Ruedas: 4 por lado (8x8) ---
    wy = HULL_W / 2 + 0.12
    xs = [-HULL_L / 2 + 0.9, -HULL_L / 2 + 2.3, HULL_L / 2 - 2.3, HULL_L / 2 - 0.9]
    for sgn in (-1, 1):
        for wx in xs:
            cyl(wx, sgn * wy, AX, WR, 0.42, 'y', RUBBER, seg=16)      # neumatico
            cyl(wx, sgn * (wy + 0.02), AX, 0.24, 0.46, 'y', STEEL, seg=12)  # llanta
    # --- Casco monocasco anguloso (suelo + cuerpo + frente inclinado en V) ---
    floorbox(0.0, 0.0, body_z0, HULL_L - 0.2, HULL_W, 0.95, HULL)
    hull_top = body_z0 + 0.95
    # Glacis frontal inclinado.
    box(HULL_L / 2 - 0.75, 0.0, body_z0 + 0.55, 1.8, HULL_W - 0.05, 0.12, BLOT, rot=rotY(46))
    # Placa baja anti-mina en V (dos planos).
    for sgn in (-1, 1):
        box(0.0, sgn * (HULL_W / 4), body_z0 - 0.18, HULL_L - 1.0, HULL_W / 2, 0.08, DARK,
            rot=mathutils.Matrix.Rotation(math.radians(sgn * 22), 4, 'X'))
    # Manchas de camuflaje.
    box(-1.2, 0.5, hull_top - 0.01, 1.5, 0.9, 0.05, BLOT)
    box(1.0, -0.6, hull_top - 0.01, 1.2, 0.8, 0.05, BLOT2)
    box(HULL_L / 2 - 1.7, -0.3, hull_top - 0.01, 1.0, 0.7, 0.05, BLOT)
    # Parabrisas blindados del conductor (visores con lente).
    for sgn in (-1, 1):
        box(HULL_L / 2 - 1.0, sgn * 0.55, hull_top - 0.02, 0.2, 0.5, 0.3, OPTIC, rot=rotY(20))
    # Faros + guardabarros.
    for sgn in (-1, 1):
        floorbox(HULL_L / 2 - 0.3, sgn * (HULL_W / 2 - 0.18), hull_top - 0.35, 0.28, 0.3, 0.14, LIGHT)
    # Puerta/rampa trasera.
    box(-HULL_L / 2 + 0.05, 0.0, body_z0 + 0.45, 0.1, HULL_W - 0.4, 0.85, BLOT2)
    # Escotillas de techo de la tropa.
    for hx in (-1.8, -0.9):
        floorbox(hx, 0.0, hull_top, 0.7, 0.8, 0.05, BLOT2)
    # --- Estacion de armas remota (RWS): pequeña, sin tripulante asomado ---
    rx, ry = -0.2, 0.0
    floorbox(rx, ry, hull_top, 0.85, 0.85, 0.3, METAL)        # base giratoria
    box(rx, ry, hull_top + 0.42, 0.55, 0.6, 0.28, DARK)        # caja del arma
    cyl(rx + 0.95, ry, hull_top + 0.45, 0.06, 1.7, 'x', DARK, seg=10)  # ametralladora .50
    box(rx - 0.1, ry + 0.42, hull_top + 0.5, 0.28, 0.16, 0.2, OPTIC)   # camara/optica
    # Antena.
    cyl(-HULL_L / 2 + 0.6, HULL_W / 2 - 0.3, hull_top + 0.9, 0.025, 1.8, 'z', STEEL, seg=6)

# =====================================================================================
#  SOLDIER  (infanteria moderna de pie con fusil) — mira a +X, origen z=0 = pies (~1.8 alto)
# =====================================================================================
def build_soldier():
    # Soldado low-poly EQUILIBRADO: torso de caja (pecho), cabeza/casco esfericos, brazos/piernas
    # cilindricos (lo que usan los buenos low-poly; ni todo cubos ni bola). Mira a +X. z=0=pies. ~1.78.
    yL = 0.13  # media separacion de piernas (hueco claro)
    for s in (-1, 1):
        floorbox(0.03, s * yL, 0.0, 0.27, 0.15, 0.10, DARK)            # bota
        cyl(0.0, s * yL, 0.31, 0.078, 0.46, 'z', HULL, seg=10)         # espinilla
        cyl(0.0, s * yL, 0.74, 0.092, 0.44, 'z', HULL, seg=10)         # muslo
    floorbox(-0.01, 0.0, 0.96, 0.22, 0.34, 0.16, HULL)                 # cadera (une piernas)
    # Torso = CAJA de pecho (no esfera) + chaleco antibalas + mochila.
    floorbox(-0.01, 0.0, 1.10, 0.23, 0.42, 0.42, HULL)                 # tronco/pecho
    floorbox(0.10,  0.0, 1.13, 0.08, 0.38, 0.34, BLOT)                 # placa del chaleco (frente)
    floorbox(-0.02, 0.0, 1.46, 0.21, 0.50, 0.10, HULL)                 # linea de hombros
    floorbox(-0.17, 0.0, 1.14, 0.13, 0.30, 0.36, BLOT2)               # mochila (espalda)
    # Brazos (cilindros): hombro->codo vertical, antebrazos al frente sujetando el fusil (ambas manos).
    for s in (-1, 1):
        cyl(0.0, s * 0.23, 1.28, 0.070, 0.30, 'z', HULL, seg=8)        # brazo alto
    cyl(0.20, 0.13, 1.20, 0.066, 0.30, 'x', HULL, seg=8)              # antebrazo der -> gatillo
    cyl(0.26, -0.05, 1.21, 0.066, 0.34, 'x', HULL, seg=8)             # antebrazo izq -> guardamano
    ico(0.35, 0.12, 1.17, 0.058, SKIN, subdiv=1)                       # mano der
    ico(0.45, -0.04, 1.19, 0.058, SKIN, subdiv=1)                      # mano izq
    # Cuello + cabeza (esfera) + casco (domo) + visera.
    cyl(0.0, 0.0, 1.55, 0.055, 0.08, 'z', SKIN, seg=8)                 # cuello
    ico(0.02, 0.0, 1.645, 0.105, SKIN, subdiv=2)                       # cabeza
    ico(0.0, 0.0, 1.675, 0.138, BLOT2, subdiv=2, sz=0.70)             # casco (domo)
    box(0.105, 0.0, 1.655, 0.10, 0.24, 0.035, BLOT2)                   # visera del casco
    # Fusil al pecho (a lo largo de +X), apuntando al frente.
    floorbox(0.25, 0.02, 1.18, 0.50, 0.045, 0.08, DARK)               # cuerpo del fusil
    floorbox(0.19, 0.02, 1.09, 0.08, 0.045, 0.10, DARK)              # cargador
    floorbox(0.07, 0.02, 1.17, 0.14, 0.045, 0.09, DARK)             # culata
    cyl(0.64, 0.02, 1.22, 0.015, 0.24, 'x', DARK, seg=6)            # canon

# =====================================================================================
#  AIRCRAFT  (caza moderno) — nariz a +X, origen z=0 = suelo (tren). Largo ~13, env ~10.
# =====================================================================================
def build_aircraft():
    cz = 1.35  # eje del fuselaje
    cyl(2.9, 0.0, cz, 0.55, 6.2, 'x', JET, seg=14)                     # cuerpo
    cyl(6.6, 0.0, cz, 0.55, 1.9, 'x', JET, seg=14, r2=0.05)            # nariz (cono a +X)
    cyl(-0.7, 0.0, cz, 0.50, 1.1, 'x', JET2, seg=14, r2=0.40)          # tobera de escape (atras)
    box(4.3, 0.0, cz + 0.5, 1.5, 0.6, 0.45, GLASS, rot=rotY(-5))       # cabina (canopy)
    floorbox(3.0, 0.0, cz - 0.72, 2.8, 0.85, 0.5, JET2)                # toma de aire (panza)
    # Alas en delta barridas (con leve diedro).
    for s in (-1, 1):
        box(2.1, s * 2.7, cz - 0.04, 3.4, 4.8, 0.16, JET,
            rot=(mathutils.Matrix.Rotation(math.radians(s * 5), 4, 'X')
                 @ mathutils.Matrix.Rotation(math.radians(26), 4, 'Z')))
    # Estabilizadores horizontales (cola).
    for s in (-1, 1):
        box(-0.3, s * 1.15, cz, 1.5, 1.7, 0.12, JET,
            rot=mathutils.Matrix.Rotation(math.radians(22), 4, 'Z'))
    box(-0.3, 0.0, cz + 0.95, 1.3, 0.12, 1.5, JET, rot=rotY(18))       # deriva vertical
    # Tren de aterrizaje simple (3 patas + ruedas) para que se apoye.
    for (gx, gy) in [(5.0, 0.0), (2.4, 1.0), (2.4, -1.0)]:
        cyl(gx, gy, 0.55, 0.06, 1.0, 'z', STEEL, seg=6)               # pata
        cyl(gx, gy, 0.16, 0.16, 0.14, 'y', RUBBER, seg=10)            # rueda

# =====================================================================================
#  SHIP  (fragata / destructor moderno) — proa a +X, origen z=0 = linea de flotacion. Largo ~38.
# =====================================================================================
def build_ship():
    floorbox(-2.0, 0.0, -1.2, 32.0, 5.0, 2.6, NAVY2)                   # casco
    box(15.5, 0.0, 0.1, 5.2, 5.0, 1.4, NAVY2, rot=rotY(20))           # proa (cuña a +X)
    floorbox(-2.5, 0.0, 1.30, 31.0, 5.0, 0.30, DECK)                   # cubierta
    # Superestructura escalonada (centro).
    floorbox(0.0, 0.0, 1.6, 10.0, 4.0, 2.2, NAVY)
    floorbox(2.0, 0.0, 3.8, 5.2, 3.2, 1.6, NAVY)
    floorbox(3.2, 0.0, 5.4, 2.8, 2.4, 1.2, NAVY)                       # puente
    cyl(2.2, 0.0, 6.6, 0.12, 2.6, 'z', STEEL, seg=8)                   # mastil
    box(2.2, 0.0, 7.5, 0.30, 1.9, 0.30, STEEL)                         # antena/radar
    floorbox(-4.2, 0.0, 1.9, 2.4, 2.2, 2.2, NAVY2)                     # chimenea
    # Torreta principal + canon (proa).
    floorbox(9.5, 0.0, 1.6, 2.6, 2.6, 1.1, NAVY)
    cyl(12.2, 0.0, 2.05, 0.16, 3.4, 'x', DARK, seg=10)                # canon
    # Helipuerto (popa) con marca.
    floorbox(-12.5, 0.0, 1.6, 6.0, 4.4, 0.18, DECK)
    box(-12.5, 0.0, 1.73, 1.6, 1.6, 0.03, LIGHT)                       # marca "H"

BUILDERS = {"mbt": build_mbt, "ifv": build_ifv, "apc": build_apc,
            "soldier": build_soldier, "aircraft": build_aircraft, "ship": build_ship}
BUILDERS.get(KIND, build_mbt)()

bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me = bpy.data.meshes.new("Vehicle")
bm.to_mesh(me); bm.free()
me.materials.append(bpy.data.materials.new("VehicleVColor"))  # placeholder; UE usara VertexColorMaterial
ob = bpy.data.objects.new("Vehicle", me)
bpy.context.scene.collection.objects.link(ob)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.object.select_all(action='DESELECT')
ob.select_set(True)
bpy.context.view_layer.objects.active = ob
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, apply_unit_scale=True,
                         mesh_smooth_type='FACE', colors_type='LINEAR')
print("WL_VEHICLE_DONE: %s kind=%s camo=%s verts=%d polys=%d" % (OUT, KIND, CAMO, len(me.vertices), len(me.polygons)))
