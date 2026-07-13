"""Genera edificios modulares intactos y en ruinas para combate urbano."""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from battle_gen_common import BattleMesh, reset_scene, rotation


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle\battle_house.fbx"
KIND = argv[1] if len(argv) > 1 else "house"
VARIANT = argv[2] if len(argv) > 2 else "intact"
SEED = int(argv[3]) if len(argv) > 3 else 201
random.seed(SEED)
reset_scene()

m = BattleMesh()
CONCRETE = (0.52, 0.50, 0.45)
CONCRETE2 = (0.43, 0.47, 0.48)
PLASTER = (0.62, 0.58, 0.49)
BRICK = (0.48, 0.28, 0.20)
ROOF = (0.39, 0.18, 0.14)
METAL = (0.35, 0.38, 0.39)
DARK = (0.12, 0.15, 0.16)
GLASS = (0.20, 0.34, 0.38)
WHITE = (0.67, 0.66, 0.61)
RED = (0.58, 0.14, 0.10)
DIRT = (0.32, 0.25, 0.17)
CHAR = (0.11, 0.10, 0.09)


def window_x(x, y, z, sy=1.2, sz=1.05):
    m.box(x, y, z, 0.05, sy, sz, GLASS)


def window_y(x, y, z, sx=1.2, sz=1.05):
    m.box(x, y, z, sx, 0.05, sz, GLASS)


def rubble(area_x, area_y, count, z=0.12):
    colors = [CONCRETE, CONCRETE2, BRICK, DIRT, CHAR]
    for _ in range(count):
        sx = random.uniform(0.25, 1.0)
        sy = random.uniform(0.22, 0.85)
        sz = random.uniform(0.15, 0.55)
        rot = rotation("Z", random.uniform(0.0, 180.0)) @ rotation("Y", random.uniform(-18.0, 18.0))
        m.box(
            random.uniform(-area_x, area_x),
            random.uniform(-area_y, area_y),
            z + sz * 0.5,
            sx,
            sy,
            sz,
            random.choice(colors),
            rot,
        )


def house_intact():
    w, d, h = 10.0, 7.2, 3.3
    m.floor_box(0.0, 0.0, 0.0, w, d, 0.25, CONCRETE2)
    m.floor_box(0.0, 0.0, 0.25, w, d, h, PLASTER)
    m.floor_box(w * 0.20, -d * 0.51, 0.25, 1.4, 0.12, 2.25, DARK)
    for x in (-3.0, 2.8):
        window_y(x, -d * 0.505, 1.95, 1.45, 1.15)
        window_y(x, d * 0.505, 1.95, 1.45, 1.15)
    for y in (-2.15, 2.1):
        window_x(-w * 0.505, y, 1.95)
        window_x(w * 0.505, y, 1.95)
    roof_angle = math.degrees(math.atan2(1.35, w * 0.5))
    slope = math.sqrt((w * 0.5) ** 2 + 1.35 ** 2)
    m.box(-w * 0.25, 0.0, h + 0.92, slope, d + 0.65, 0.22, ROOF, rotation("Y", -roof_angle))
    m.box(w * 0.25, 0.0, h + 0.92, slope, d + 0.65, 0.22, ROOF, rotation("Y", roof_angle))
    m.floor_box(-2.9, 1.6, h + 0.9, 0.7, 0.7, 1.25, BRICK)


def block_intact():
    w, d, floors = 12.0, 9.5, 3
    m.floor_box(0.0, 0.0, 0.0, w + 0.8, d + 0.8, 0.3, CONCRETE2)
    m.floor_box(0.0, 0.0, 0.3, w, d, floors * 3.05, CONCRETE)
    for floor in range(floors):
        z = 1.75 + floor * 3.05
        for x in (-4.2, -1.4, 1.4, 4.2):
            window_y(x, -d * 0.505, z, 1.25, 1.25)
            window_y(x, d * 0.505, z, 1.25, 1.25)
        for y in (-3.0, 0.0, 3.0):
            window_x(-w * 0.505, y, z, 1.15, 1.25)
            window_x(w * 0.505, y, z, 1.15, 1.25)
        m.floor_box(0.0, -d * 0.55, z - 0.75, w * 0.82, 0.85, 0.12, METAL)
    m.floor_box(0.0, 0.0, floors * 3.05 + 0.3, w + 0.25, d + 0.25, 0.35, METAL)
    m.floor_box(2.7, 1.8, floors * 3.05 + 0.65, 2.4, 2.0, 1.0, CONCRETE2)


def warehouse_intact():
    w, d, h = 18.0, 10.5, 5.2
    m.floor_box(0.0, 0.0, 0.0, w + 1.0, d + 1.0, 0.28, CONCRETE2)
    m.floor_box(0.0, 0.0, 0.28, w, d, h, METAL)
    m.floor_box(w * 0.32, -d * 0.505, 0.3, 4.2, 0.12, 3.9, DARK)
    m.floor_box(-w * 0.22, -d * 0.505, 0.3, 1.2, 0.12, 2.2, CONCRETE)
    for x in (-6.0, -2.0, 2.0, 6.0):
        window_y(x, d * 0.505, 3.8, 1.8, 0.75)
    roof_angle = math.degrees(math.atan2(1.0, d * 0.5))
    slope = math.sqrt((d * 0.5) ** 2 + 1.0 ** 2)
    m.box(0.0, -d * 0.25, h + 0.8, w + 0.6, slope, 0.18, CONCRETE2, rotation("X", roof_angle))
    m.box(0.0, d * 0.25, h + 0.8, w + 0.6, slope, 0.18, CONCRETE2, rotation("X", -roof_angle))
    for x in (-5.5, 0.0, 5.5):
        m.floor_box(x, 0.0, h + 1.25, 1.2, 1.1, 0.55, DARK)


def gas_station_intact():
    # Tienda, marquesina, cuatro surtidores y poste de precio sin texto.
    m.floor_box(-3.2, 2.6, 0.0, 8.4, 5.6, 3.5, WHITE)
    for x in (-5.4, -2.7, 0.0):
        window_y(x, -0.22, 1.85, 1.55, 1.55)
    m.floor_box(5.0, -1.7, 0.0, 12.5, 7.8, 0.18, CONCRETE2)
    for x in (1.0, 8.7):
        for y in (-4.2, 0.7):
            m.cylinder(x, y, 2.2, 0.18, 4.4, "z", METAL, 10)
    m.floor_box(4.8, -1.7, 4.35, 13.5, 8.8, 0.38, RED)
    m.floor_box(4.8, -1.7, 4.73, 13.5, 8.8, 0.18, WHITE)
    for x in (2.2, 6.8):
        for y in (-3.25, -0.1):
            m.floor_box(x, y, 0.18, 0.72, 0.62, 1.45, RED)
            m.box(x, y - 0.32, 1.2, 0.42, 0.05, 0.42, DARK)
    m.cylinder(-8.0, -3.8, 2.5, 0.14, 5.0, "z", METAL, 10)
    m.box(-8.0, -3.8, 4.4, 0.2, 2.2, 1.25, RED)


def ruin(kind):
    dims = {
        "house": (10.0, 7.2),
        "block": (12.0, 9.5),
        "warehouse": (18.0, 10.5),
        "gas_station": (15.0, 11.0),
    }
    w, d = dims[kind]
    wall = PLASTER if kind == "house" else (METAL if kind == "warehouse" else CONCRETE)
    m.floor_box(0.0, 0.0, -0.12, w + 0.7, d + 0.7, 0.3, DIRT)
    # Paredes quebradas por segmentos, con alturas asimetricas y huecos claros.
    for side in (-1, 1):
        for index in range(4):
            x = -w * 0.38 + index * w * 0.25
            if random.random() < 0.2:
                continue
            height = random.uniform(0.7, 2.9 if kind != "block" else 5.0)
            m.floor_box(x, side * d * 0.49, 0.12, w * 0.20, 0.28, height, wall)
        for index in range(3):
            y = -d * 0.30 + index * d * 0.30
            if random.random() < 0.25:
                continue
            height = random.uniform(0.8, 3.2 if kind != "block" else 5.6)
            m.floor_box(side * w * 0.49, y, 0.12, 0.28, d * 0.23, height, wall)
    if kind == "block":
        m.floor_box(-2.2, 1.0, 0.12, 4.5, 3.8, 6.6, CONCRETE2)
        for z in (1.8, 4.7):
            window_y(-2.2, -0.93, z, 1.25, 1.1)
    elif kind == "gas_station":
        for x, y in ((3.8, -3.1), (7.4, 0.2)):
            m.cylinder(x, y, 1.45, 0.2, 2.9, "z", CHAR, 8)
        m.box(4.8, -1.6, 0.65, 8.5, 3.8, 0.28, RED, rotation("Y", 12.0))
    elif kind == "warehouse":
        m.box(2.8, 0.0, 1.0, 7.0, d * 0.8, 0.25, METAL, rotation("Y", -16.0))
    rubble(w * 0.48, d * 0.48, 36 if kind == "block" else 25)


if VARIANT == "ruin":
    ruin(KIND)
else:
    {
        "house": house_intact,
        "block": block_intact,
        "warehouse": warehouse_intact,
        "gas_station": gas_station_intact,
    }.get(KIND, house_intact)()

m.export(OUT, "BattleBuilding_%s_%s" % (KIND, VARIANT))
