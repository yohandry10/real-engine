"""Genera las poses tacticas extra sin modificar gen_vehicle.py."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from battle_gen_common import BattleMesh, reset_scene, rotation


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle\battle_soldier_kneeling.fbx"
POSE = argv[1] if len(argv) > 1 else "kneeling"
CAMO = argv[2] if len(argv) > 2 else "green"
reset_scene()

m = BattleMesh()
palettes = {
    "green": ((0.34, 0.39, 0.24), (0.23, 0.28, 0.17), (0.42, 0.36, 0.21)),
    "desert": ((0.56, 0.48, 0.31), (0.44, 0.36, 0.23), (0.40, 0.35, 0.25)),
}
CAMO_BASE, CAMO_DARK, CAMO_ALT = palettes.get(CAMO, palettes["green"])
SKIN = (0.60, 0.45, 0.36)
WEAPON = (0.13, 0.14, 0.14)


def head(x, y, z):
    m.cylinder(x, y, z - 0.13, 0.05, 0.10, "z", SKIN, 8)
    m.ico(x, y, z, 0.105, SKIN, 2)
    m.ico(x - 0.01, y, z + 0.045, 0.14, CAMO_DARK, 2, 1.0, 1.0, 0.68)
    m.box(x + 0.105, y, z + 0.02, 0.10, 0.24, 0.035, CAMO_DARK)


def rifle(start, end, z_offset=0.0):
    s = (start[0], start[1], start[2] + z_offset)
    e = (end[0], end[1], end[2] + z_offset)
    m.cylinder_between(s, e, 0.035, WEAPON, 7)
    m.box(s[0] + 0.18, s[1], s[2] - 0.07, 0.12, 0.05, 0.14, WEAPON, rotation("Y", -10.0))


def kneeling():
    # Pierna izquierda adelantada; derecha apoyada en la rodilla.
    m.cylinder_between((-0.20, -0.13, 0.56), (0.28, -0.13, 0.48), 0.085, CAMO_BASE, 8)
    m.cylinder_between((0.28, -0.13, 0.48), (0.42, -0.13, 0.06), 0.075, CAMO_BASE, 8)
    m.box(0.50, -0.13, 0.06, 0.30, 0.16, 0.10, CAMO_DARK)
    m.cylinder_between((-0.18, 0.13, 0.55), (-0.36, 0.13, 0.17), 0.085, CAMO_ALT, 8)
    m.cylinder_between((-0.36, 0.13, 0.17), (-0.04, 0.13, 0.08), 0.075, CAMO_BASE, 8)
    m.box(0.07, 0.13, 0.07, 0.30, 0.16, 0.10, CAMO_DARK)
    m.box(-0.08, 0.0, 0.88, 0.30, 0.42, 0.55, CAMO_BASE, rotation("Y", -7.0))
    m.box(-0.22, 0.0, 0.88, 0.12, 0.38, 0.43, CAMO_DARK, rotation("Y", -7.0))
    head(-0.02, 0.0, 1.32)
    m.cylinder_between((-0.02, -0.21, 1.05), (0.33, -0.10, 1.08), 0.065, CAMO_BASE, 8)
    m.cylinder_between((-0.02, 0.21, 1.05), (0.45, 0.02, 1.10), 0.065, CAMO_ALT, 8)
    rifle((0.16, 0.0, 1.12), (0.88, 0.0, 1.12))


def prone():
    # Cuerpo paralelo al suelo, fusil hacia +X.
    m.box(-0.22, 0.0, 0.34, 0.72, 0.42, 0.30, CAMO_BASE, rotation("Y", -3.0))
    m.box(-0.42, 0.0, 0.39, 0.25, 0.38, 0.30, CAMO_DARK)
    head(0.22, 0.0, 0.55)
    for side in (-1, 1):
        m.cylinder_between((-0.55, side * 0.13, 0.28), (-1.02, side * 0.20, 0.17), 0.08, CAMO_BASE, 8)
        m.cylinder_between((-1.02, side * 0.20, 0.17), (-1.42, side * 0.28, 0.08), 0.07, CAMO_ALT, 8)
        m.box(-1.55, side * 0.28, 0.07, 0.32, 0.16, 0.10, CAMO_DARK, rotation("Z", side * 5.0))
    m.cylinder_between((0.0, -0.20, 0.40), (0.50, -0.12, 0.25), 0.063, CAMO_BASE, 8)
    m.cylinder_between((0.0, 0.20, 0.40), (0.62, 0.05, 0.25), 0.063, CAMO_ALT, 8)
    rifle((0.36, 0.0, 0.29), (1.22, 0.0, 0.29))


(prone if POSE == "prone" else kneeling)()
m.export(OUT, "BattleSoldier_%s_%s" % (POSE, CAMO))
