"""Genera props de densidad: rocas, cerca, poste y crater tridimensional."""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from battle_gen_common import BattleMesh, reset_scene, rotation


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle\battle_prop_rocks_a.fbx"
KIND = argv[1] if len(argv) > 1 else "rocks_a"
SEED = int(argv[2]) if len(argv) > 2 else 801
random.seed(SEED)
reset_scene()

m = BattleMesh()
ROCK = [(0.34, 0.34, 0.32), (0.42, 0.40, 0.36), (0.29, 0.30, 0.29)]
WOOD = (0.33, 0.23, 0.13)
WOOD_DARK = (0.23, 0.15, 0.09)
METAL = (0.34, 0.36, 0.35)
CERAMIC = (0.46, 0.43, 0.36)
DIRT = (0.36, 0.27, 0.16)
DIRT_DARK = (0.18, 0.14, 0.09)
SCORCH = (0.075, 0.065, 0.052)


def rocks(variant):
    count = 6 if variant == "a" else 9
    spread = 2.6 if variant == "a" else 3.8
    for index in range(count):
        angle = index * 2.13
        distance = (0.4 + (index % 4) * 0.27) * spread
        x = math.cos(angle) * distance
        y = math.sin(angle) * distance
        radius = random.uniform(0.45, 1.2 if variant == "a" else 0.85)
        m.ico(x, y, radius * 0.38, radius, random.choice(ROCK), 1, random.uniform(0.8, 1.35), random.uniform(0.75, 1.2), random.uniform(0.45, 0.72))


def fence():
    length = 10.0
    for x in (-5.0, -2.5, 0.0, 2.5, 5.0):
        m.cylinder(x, 0.0, 1.05, 0.10, 2.1, "z", WOOD_DARK, 8, 0.075)
    for z in (0.72, 1.42):
        m.box(0.0, 0.0, z, length + 0.3, 0.13, 0.16, WOOD, rotation("Y", random.uniform(-1.5, 1.5)))
    for x in (-3.7, 1.4):
        m.box(x, 0.0, 1.05, 3.0, 0.09, 0.10, WOOD_DARK, rotation("Y", 36.0))


def utility_pole():
    m.cylinder(0.0, 0.0, 4.1, 0.16, 8.2, "z", WOOD_DARK, 10, 0.11)
    m.box(0.0, 0.0, 7.35, 0.20, 3.1, 0.20, WOOD)
    m.box(0.0, 0.0, 6.65, 0.18, 2.35, 0.18, WOOD)
    for y, z in ((-1.3, 7.55), (0.0, 7.55), (1.3, 7.55), (-0.95, 6.85), (0.95, 6.85)):
        m.cylinder(0.0, y, z, 0.10, 0.34, "z", CERAMIC, 8, 0.07)
    m.box(0.18, 0.0, 4.5, 0.18, 0.72, 1.15, METAL)


def crater():
    rings = 3
    segments = 28
    radii = (0.0, 1.9, 3.15, 4.2)
    heights = (-0.42, -0.28, 0.34, 0.0)
    verts = []
    # Centro repetido como un unico vertice; anillos exteriores irregulares.
    center = m.bm.verts.new((0.0, 0.0, heights[0]))
    verts.append([center])
    for ring in range(1, rings + 1):
        ring_verts = []
        for index in range(segments):
            angle = index / segments * math.tau
            jitter = 1.0 + 0.07 * math.sin(index * 2.7 + SEED * 0.01)
            radius = radii[ring] * jitter
            z = heights[ring] + 0.07 * math.sin(index * 1.9)
            ring_verts.append(m.bm.verts.new((math.cos(angle) * radius, math.sin(angle) * radius, z)))
        verts.append(ring_verts)
    m.bm.verts.ensure_lookup_table()
    for index in range(segments):
        nxt = (index + 1) % segments
        face = m.bm.faces.new([verts[0][0], verts[1][index], verts[1][nxt]])
        m.colorize([face], SCORCH)
    for ring in range(1, rings):
        base = DIRT_DARK if ring == 1 else DIRT
        for index in range(segments):
            nxt = (index + 1) % segments
            face = m.bm.faces.new([verts[ring][index], verts[ring + 1][index], verts[ring + 1][nxt], verts[ring][nxt]])
            m.colorize([face], base)
    for index in range(10):
        angle = index / 10.0 * math.tau + 0.23
        radius = 4.2 + 0.7 * (index % 3)
        m.ico(math.cos(angle) * radius, math.sin(angle) * radius, 0.12, 0.42, DIRT, 1, 1.4, 0.8, 0.42)


builders = {
    "rocks_a": lambda: rocks("a"),
    "rocks_b": lambda: rocks("b"),
    "fence": fence,
    "utility_pole": utility_pole,
    "crater": crater,
}
builders.get(KIND, builders["rocks_a"])()
m.export(OUT, "BattleProp_%s" % KIND)
