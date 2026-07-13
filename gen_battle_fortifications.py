"""Genera fortificaciones modulares low-poly para la batalla tactica."""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from battle_gen_common import BattleMesh, reset_scene, rotation


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle\battle_fort_sandbags.fbx"
KIND = argv[1] if len(argv) > 1 else "sandbags"
SEED = int(argv[2]) if len(argv) > 2 else 601
random.seed(SEED)
reset_scene()

m = BattleMesh()
SAND = (0.48, 0.42, 0.27)
SAND_DARK = (0.36, 0.31, 0.20)
DIRT = (0.31, 0.23, 0.14)
DIRT_DARK = (0.23, 0.17, 0.11)
WOOD = (0.30, 0.20, 0.11)
CONCRETE = (0.39, 0.40, 0.38)
CONCRETE_DARK = (0.27, 0.28, 0.27)
METAL = (0.31, 0.33, 0.32)
DARK = (0.08, 0.09, 0.085)
WHITE = (0.64, 0.63, 0.56)
RED = (0.58, 0.12, 0.09)


def sandbag(x, y, z, angle=0.0):
    m.ico(x, y, z, 0.48, random.choice([SAND, SAND_DARK]), 1, 1.35, 0.72, 0.50)
    # Costura central visible desde arriba.
    m.box(x, y, z + 0.22, 0.58, 0.04, 0.025, SAND_DARK, rotation("Z", angle))


def sandbag_wall():
    for row, z in enumerate((0.25, 0.70, 1.10)):
        count = 9 - row
        for index in range(count):
            x = (index - (count - 1) * 0.5) * 0.93 + (0.46 if row % 2 else 0.0)
            sandbag(x, 0.0, z)
    for side in (-1, 1):
        for index in range(4):
            sandbag(side * 4.0, -0.7 - index * 0.88, 0.25)
            if index < 3:
                sandbag(side * 4.0, -0.7 - index * 0.88, 0.70)


def trench_straight():
    m.floor_box(0.0, 0.0, -0.65, 12.0, 1.75, 0.16, DIRT_DARK)
    for side in (-1, 1):
        m.floor_box(0.0, side * 1.35, -0.48, 12.0, 0.16, 1.0, WOOD)
        for x in (-5.4, -3.6, -1.8, 0.0, 1.8, 3.6, 5.4):
            m.floor_box(x, side * 1.42, -0.62, 0.12, 0.25, 1.25, WOOD)
        for index in range(13):
            x = -5.6 + index * 0.93
            m.ico(x, side * 2.0, 0.12 + random.uniform(-0.05, 0.12), 0.72, DIRT, 1, 1.1, 0.85, 0.48)


def trench_corner():
    trench_straight()
    # Rama perpendicular desde el centro hacia +Y.
    for side in (-1, 1):
        x = side * 1.35
        m.floor_box(x, 4.7, -0.48, 0.16, 7.8, 1.0, WOOD)
        for y in (1.2, 3.0, 4.8, 6.6, 8.0):
            m.floor_box(x, y, -0.62, 0.25, 0.12, 1.25, WOOD)
        for index in range(9):
            y = 1.0 + index * 0.93
            m.ico(side * 2.0, y, 0.12 + random.uniform(-0.05, 0.12), 0.72, DIRT, 1, 0.85, 1.1, 0.48)
    m.floor_box(0.0, 4.7, -0.65, 1.75, 7.8, 0.16, DIRT_DARK)


def bunker():
    m.floor_box(0.0, 0.0, -0.2, 9.0, 7.2, 0.35, DIRT)
    m.floor_box(0.0, 0.0, 0.0, 7.8, 6.0, 2.15, CONCRETE_DARK)
    m.floor_box(0.0, 0.0, 2.15, 8.7, 6.9, 0.62, CONCRETE)
    m.box(3.91, 0.0, 1.45, 0.10, 3.6, 0.42, DARK)
    m.floor_box(-3.91, 0.0, 0.0, 0.12, 1.55, 1.85, DARK)
    for x in (-3.2, -1.6, 0.0, 1.6, 3.2):
        m.ico(x, -3.4, 0.24, 0.62, DIRT, 1, 1.3, 0.85, 0.48)
        m.ico(x, 3.4, 0.24, 0.62, DIRT, 1, 1.3, 0.85, 0.48)
    m.cylinder(-2.7, 1.9, 3.15, 0.06, 1.6, "z", METAL, 8)


def wire():
    length = 11.0
    for x in (-length * 0.5, -length * 0.25, 0.0, length * 0.25, length * 0.5):
        m.cylinder(x, 0.0, 0.95, 0.07, 1.9, "z", METAL, 7)
        m.box(x, 0.0, 0.75, 1.45, 0.055, 0.055, METAL, rotation("Y", 45.0))
        m.box(x, 0.0, 0.75, 1.45, 0.055, 0.055, METAL, rotation("Y", -45.0))
    for z in (0.45, 0.92, 1.38):
        m.cylinder_between((-length * 0.5, 0.0, z), (length * 0.5, 0.0, z), 0.025, METAL, 5)
    # Bucles grandes de concertina: lectura clara desde camara alta.
    for index in range(15):
        x = -5.25 + index * 0.75
        angle = index / 14.0 * math.tau * 3.5
        y = math.cos(angle) * 0.58
        z = 0.64 + math.sin(angle) * 0.58
        if index:
            prev_angle = (index - 1) / 14.0 * math.tau * 3.5
            prev = (-5.25 + (index - 1) * 0.75, math.cos(prev_angle) * 0.58, 0.64 + math.sin(prev_angle) * 0.58)
            m.cylinder_between(prev, (x, y, z), 0.022, METAL, 5)


def checkpoint():
    m.floor_box(-3.7, 1.9, 0.0, 3.2, 2.8, 2.7, CONCRETE)
    m.box(-3.7, 0.48, 1.55, 1.55, 0.06, 0.82, DARK)
    m.floor_box(-3.7, 1.9, 2.7, 3.6, 3.2, 0.18, METAL)
    m.cylinder(0.0, 0.0, 0.75, 0.12, 1.5, "z", METAL, 9)
    m.box(3.9, 0.0, 1.42, 7.8, 0.24, 0.24, WHITE, rotation("Y", -8.0))
    for index in range(5):
        m.box(0.8 + index * 1.45, -0.13, 1.42 - index * 0.03, 0.64, 0.26, 0.26, RED, rotation("Y", -8.0))
    for x in (-1.8, 1.8):
        m.floor_box(x, -2.25, 0.0, 1.4, 1.4, 0.75, CONCRETE_DARK)
        m.box(x, -2.25, 0.76, 1.45, 1.45, 0.12, WHITE)


builders = {
    "sandbags": sandbag_wall,
    "trench_straight": trench_straight,
    "trench_corner": trench_corner,
    "bunker": bunker,
    "wire": wire,
    "checkpoint": checkpoint,
}
builders.get(KIND, sandbag_wall)()
m.export(OUT, "BattleFort_%s" % KIND)
