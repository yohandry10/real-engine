"""Genera restos quemados separados del pipeline de vehiculos vivos."""
import os
import random
import sys

import mathutils

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from battle_gen_common import BattleMesh, reset_scene, rotation


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle\battle_wreck_mbt.fbx"
KIND = argv[1] if len(argv) > 1 else "mbt"
SEED = int(argv[2]) if len(argv) > 2 else 501
random.seed(SEED)
reset_scene()

m = BattleMesh()
CHAR = (0.12, 0.105, 0.09)
ASH = (0.23, 0.22, 0.19)
RUST = (0.38, 0.20, 0.10)
METAL = (0.30, 0.30, 0.28)
RUBBER = (0.11, 0.105, 0.10)
SCORCH = (0.055, 0.050, 0.043)


def tracked_base(length, width, body_h=0.85):
    for side in (-1, 1):
        y = side * (width * 0.5 + 0.28)
        m.floor_box(0.0, y, 0.0, length, 0.55, 0.68, RUBBER)
        for index in range(6):
            x = -length * 0.40 + index * length * 0.16
            m.cylinder(x, y + side * 0.28, 0.34, 0.31, 0.10, "y", random.choice([ASH, RUST, SCORCH]), 9)
    m.floor_box(0.0, 0.0, 0.42, length - 0.25, width, body_h, CHAR)
    m.box(length * 0.22, -0.45, 0.42 + body_h, length * 0.35, width * 0.36, 0.07, RUST, rotation("Z", -9.0))


def wheeled_base(length, width):
    for side in (-1, 1):
        y = side * (width * 0.5 + 0.14)
        for x in (-length * 0.36, -length * 0.13, length * 0.13, length * 0.36):
            m.cylinder(x, y, 0.46, 0.50, 0.34, "y", RUBBER, 12)
            if random.random() < 0.7:
                m.cylinder(x, y + side * 0.03, 0.46, 0.21, 0.38, "y", RUST, 9)
    m.floor_box(0.0, 0.0, 0.52, length, width, 0.96, CHAR)


def add_damage(length, width):
    for _ in range(7):
        x = random.uniform(-length * 0.36, length * 0.36)
        y = random.uniform(-width * 0.35, width * 0.35)
        m.ico(x, y, random.uniform(1.05, 1.55), random.uniform(0.12, 0.28), SCORCH, 1, 1.35, 1.0, 0.32)
    for _ in range(5):
        m.box(
            random.uniform(-length * 0.6, length * 0.6),
            random.uniform(-width, width),
            random.uniform(0.08, 0.35),
            random.uniform(0.3, 0.9),
            random.uniform(0.18, 0.6),
            random.uniform(0.08, 0.22),
            random.choice([RUST, ASH, CHAR]),
            rotation("Z", random.uniform(0.0, 180.0)),
        )


def armor_wreck(kind):
    specs = {
        "mbt": (6.8, 2.7, 0.70, 2.8, 2.3),
        "ifv": (5.9, 2.6, 0.95, 1.7, 1.45),
        "artillery": (7.3, 2.9, 0.72, 3.2, 2.5),
        "sam": (6.5, 2.9, 0.92, 2.2, 2.0),
    }
    length, width, body_h, turret_l, turret_w = specs[kind]
    tracked_base(length, width, body_h)
    # Torreta o modulo arrancado, desplazado y ladeado; deliberadamente sin canon.
    rot = rotation("Z", 23.0) @ rotation("Y", 14.0)
    m.box(-0.55, 0.62, 1.36, turret_l, turret_w, 0.62 if kind != "artillery" else 0.92, ASH, rot)
    if kind == "sam":
        for side in (-1, 1):
            m.box(0.25, side * 0.52, 1.10, 2.8, 0.34, 0.34, CHAR, rotation("Y", 18.0) @ rotation("Z", -16.0))
        m.box(-2.1, -0.65, 0.75, 0.12, 1.45, 1.05, RUST, rotation("Y", 62.0))
    elif kind == "artillery":
        m.floor_box(-2.3, 0.0, 0.95, 0.45, width - 0.35, 0.42, SCORCH)
    add_damage(length, width)


def apc_wreck():
    wheeled_base(6.2, 2.6)
    m.box(0.45, 0.0, 1.32, 4.8, 2.35, 0.48, ASH, rotation("Y", -7.0))
    m.box(-0.45, 0.78, 1.85, 0.85, 0.85, 0.34, CHAR, rotation("Z", 31.0) @ rotation("Y", 15.0))
    add_damage(6.2, 2.6)


def heli_wreck():
    m.box(0.35, 0.0, 0.62, 4.5, 1.15, 1.05, CHAR, rotation("Y", -8.0))
    m.box(-3.15, 0.2, 0.35, 3.8, 0.42, 0.42, ASH, rotation("Z", 11.0))
    m.box(2.7, -0.15, 0.46, 1.5, 0.86, 0.62, SCORCH, rotation("Y", 19.0))
    # Rotor roto en piezas, sin apariencia de unidad operativa.
    for x, y, angle, length in ((-0.8, 0.6, 16.0, 4.5), (1.4, -1.0, -34.0, 3.2), (-2.9, -0.8, 58.0, 2.4)):
        m.box(x, y, 0.12, length, 0.16, 0.08, METAL, rotation("Z", angle))
    add_damage(6.0, 2.0)


if KIND == "apc":
    apc_wreck()
elif KIND == "heli":
    heli_wreck()
else:
    armor_wreck(KIND if KIND in ("mbt", "ifv", "artillery", "sam") else "mbt")

m.export(OUT, "BattleWreck_%s" % KIND)
