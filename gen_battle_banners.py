"""Genera estandartes de formacion VE/CO con bandera ondulada vertex-color."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from battle_gen_common import BattleMesh, reset_scene


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle\battle_banner_ve.fbx"
FACTION = argv[1].lower() if len(argv) > 1 else "ve"
reset_scene()

m = BattleMesh()
POLE = (0.27, 0.29, 0.28)
GOLD = (0.86, 0.68, 0.12)
YELLOW = (0.88, 0.70, 0.10)
BLUE = (0.08, 0.19, 0.48)
RED = (0.62, 0.08, 0.08)
WHITE = (0.90, 0.92, 0.90)

# Mastil, remate y zapata discreta.
m.cylinder(0.0, 0.0, 2.5, 0.055, 5.0, "z", POLE, 10)
m.ico(0.0, 0.0, 5.08, 0.11, GOLD, 1, 1.0, 1.0, 1.15)
m.floor_box(0.0, 0.0, 0.0, 0.52, 0.52, 0.10, POLE)

cols, rows = 12, 9
x0, width = 0.08, 2.05
z_top, height = 4.85, 1.25
verts = []
for row in range(rows + 1):
    v = row / rows
    for col in range(cols + 1):
        u = col / cols
        x = x0 + width * u
        y = 0.075 * math.sin(u * math.tau * 1.35 + v * 0.65) * (0.25 + 0.75 * u)
        z = z_top - height * v + 0.035 * math.sin(u * math.tau * 0.8) * v
        verts.append(m.bm.verts.new((x, y, z)))

m.bm.verts.ensure_lookup_table()


def stripe_color(v):
    if FACTION == "co":
        if v < 0.5:
            return YELLOW
        if v < 0.75:
            return BLUE
        return RED
    if v < 1.0 / 3.0:
        return YELLOW
    if v < 2.0 / 3.0:
        return BLUE
    return RED


for row in range(rows):
    for col in range(cols):
        a = row * (cols + 1) + col
        b = a + 1
        c = a + cols + 1
        d = c + 1
        base = stripe_color((row + 0.5) / rows)
        front = m.bm.faces.new([verts[a], verts[b], verts[d], verts[c]])
        m.colorize([front], base, shade=False)

# Ocho estrellas simplificadas para Venezuela, visibles por ambas caras.
if FACTION == "ve":
    for index in range(8):
        t = index / 7.0
        x = x0 + 0.44 + t * 1.18
        z = z_top - height * 0.50 + 0.10 * math.sin(t * math.pi)
        y = 0.075 * math.sin(((x - x0) / width) * math.tau * 1.35 + 0.33)
        for side in (-1, 1):
            m.box(x, y + side * 0.028, z, 0.065, 0.025, 0.065, WHITE, __import__("battle_gen_common").rotation("Y", 45.0))

m.export(OUT, "BattleBanner_%s" % FACTION.upper())
