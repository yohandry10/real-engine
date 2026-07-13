"""Genera arboles, arbustos y troncos tacticos a escala de batalla."""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from battle_gen_common import BattleMesh, reset_scene


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle\battle_tree_broadleaf_a.fbx"
KIND = argv[1] if len(argv) > 1 else "tree_broadleaf_a"
SEED = int(argv[2]) if len(argv) > 2 else 301
random.seed(SEED)
reset_scene()

m = BattleMesh()
TRUNK = (0.29, 0.19, 0.10)
TRUNK_DARK = (0.20, 0.13, 0.08)
LEAVES = [(0.12, 0.34, 0.12), (0.18, 0.43, 0.15), (0.23, 0.48, 0.18), (0.15, 0.39, 0.17)]
DRY = (0.30, 0.36, 0.14)


def broadleaf(variant):
    height = 8.3 if variant == "a" else 6.8
    m.cylinder(0.0, 0.0, height * 0.28, 0.34, height * 0.56, "z", TRUNK, 9, 0.26)
    for side in (-1, 1):
        m.cylinder_between((0.0, 0.0, height * 0.43), (side * 1.2, 0.35, height * 0.68), 0.16, TRUNK, 7, 0.09)
    count = 8 if variant == "a" else 6
    for index in range(count):
        angle = index * 2.399
        radius = 1.2 + (index % 3) * 0.45
        cx = __import__("math").cos(angle) * radius
        cy = __import__("math").sin(angle) * radius
        cz = height * 0.69 + (index % 4) * 0.35
        m.ico(cx, cy, cz, 1.65 if variant == "a" else 1.45, random.choice(LEAVES), 1, 1.15, 0.95, 0.88)


def conifer():
    m.cylinder(0.0, 0.0, 3.8, 0.28, 7.6, "z", TRUNK_DARK, 8, 0.2)
    for level in range(6):
        z = 1.25 + level * 1.15
        radius = 3.0 - level * 0.38
        m.cone(0.0, 0.0, z, radius, 0.08, 2.5, LEAVES[level % len(LEAVES)], 10)


def shrub(variant):
    count = 7 if variant == "a" else 5
    for index in range(count):
        angle = index * 2.2
        radius = 0.42 + 0.2 * (index % 2)
        cx = __import__("math").cos(angle) * radius
        cy = __import__("math").sin(angle) * radius
        m.ico(cx, cy, 0.55 + 0.08 * (index % 3), 0.68, random.choice(LEAVES + [DRY]), 1, 1.15, 0.9, 0.72)


def fallen_log():
    m.cylinder(0.0, 0.0, 0.42, 0.42, 5.8, "x", TRUNK, 10, 0.34)
    m.cylinder(-1.1, 0.0, 0.86, 0.13, 1.25, "z", TRUNK_DARK, 7, 0.06)
    m.cylinder(1.45, 0.0, 0.72, 0.11, 0.95, "z", TRUNK_DARK, 7, 0.05)
    for x, y, scale in ((-2.3, -0.45, 0.7), (2.1, 0.55, 0.8), (0.8, -0.7, 0.55)):
        m.ico(x, y, 0.26, scale, random.choice(LEAVES), 1, 1.2, 0.9, 0.55)


builders = {
    "tree_broadleaf_a": lambda: broadleaf("a"),
    "tree_broadleaf_b": lambda: broadleaf("b"),
    "tree_conifer": conifer,
    "shrub_a": lambda: shrub("a"),
    "shrub_b": lambda: shrub("b"),
    "fallen_log": fallen_log,
}
builders.get(KIND, builders["tree_broadleaf_a"])()
m.export(OUT, "BattleNature_%s" % KIND)
