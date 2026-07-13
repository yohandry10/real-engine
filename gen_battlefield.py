"""Genera el campo tactico de 220 m con relieve, camino, tierra y rocas."""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from battle_gen_common import BattleMesh, reset_scene


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle\battlefield_grassland.fbx"
SEED = int(argv[1]) if len(argv) > 1 else 104
random.seed(SEED)
reset_scene()

mesh = BattleMesh()
size = 220.0
steps = 64
half = size * 0.5


def terrain_height(x, y):
    broad = 0.30 * math.sin((x + 18.0) / 35.0) + 0.24 * math.cos((y - 9.0) / 29.0)
    hill_a = 0.55 * math.exp(-((x + 56.0) ** 2 + (y - 44.0) ** 2) / 1800.0)
    hill_b = 0.42 * math.exp(-((x - 61.0) ** 2 + (y + 48.0) ** 2) / 1500.0)
    hollow = -0.28 * math.exp(-((x - 12.0) ** 2 + (y - 16.0) ** 2) / 900.0)
    micro = 0.08 * math.sin(x * 0.19 + y * 0.11) * math.cos(y * 0.17)
    return broad + hill_a + hill_b + hollow + micro - 0.28


def road_center(x):
    return 17.0 * math.sin((x + 12.0) / 43.0) - 0.18 * x


def terrain_color(x, y):
    distance = abs(y - road_center(x))
    if distance < 3.7:
        rut = 0.025 * math.sin(x * 0.45 + y)
        if abs(distance - 2.0) < 0.42:
            return (0.43 + rut, 0.32 + rut, 0.20 + rut)
        return (0.57 + rut, 0.44 + rut, 0.28 + rut)
    if distance < 7.0:
        return (0.48, 0.52, 0.32)

    patch_a = math.exp(-((x - 36.0) ** 2 + (y - 42.0) ** 2) / 650.0)
    patch_b = math.exp(-((x + 52.0) ** 2 + (y + 36.0) ** 2) / 850.0)
    noise = 0.50 + 0.25 * math.sin(x * 0.077 + y * 0.031) + 0.25 * math.sin(x * 0.029 - y * 0.089 + 1.7)
    tones = [
        (0.50, 0.58, 0.36),
        (0.54, 0.62, 0.39),
        (0.58, 0.65, 0.41),
        (0.52, 0.60, 0.35),
    ]
    scaled = max(0.0, min(0.999, noise)) * 3.0
    index = min(2, int(scaled))
    blend = scaled - index
    grass = tuple(tones[index][channel] * (1.0 - blend) + tones[index + 1][channel] * blend for channel in range(3))
    dirt = min(0.72, patch_a * 0.72 + patch_b * 0.55)
    return (
        grass[0] * (1.0 - dirt) + 0.58 * dirt,
        grass[1] * (1.0 - dirt) + 0.47 * dirt,
        grass[2] * (1.0 - dirt) + 0.34 * dirt,
    )


verts = []
for iy in range(steps + 1):
    y = -half + size * iy / steps
    for ix in range(steps + 1):
        x = -half + size * ix / steps
        verts.append(mesh.bm.verts.new((x, y, terrain_height(x, y))))

mesh.bm.verts.ensure_lookup_table()
for iy in range(steps):
    for ix in range(steps):
        a = iy * (steps + 1) + ix
        b = a + 1
        c = a + (steps + 1)
        d = c + 1
        for indices in ((a, b, d), (a, d, c)):
            face = mesh.bm.faces.new([verts[i] for i in indices])
            face.normal_update()
            light = max(0.0, face.normal.dot(__import__("battle_gen_common").SUN))
            shade = 0.82 + 0.38 * light
            for loop in face.loops:
                co = loop.vert.co
                base = terrain_color(co.x, co.y)
                loop[mesh.colors] = (
                    min(1.0, base[0] * shade),
                    min(1.0, base[1] * shade),
                    min(1.0, base[2] * shade),
                    1.0,
                )

# Rocas pequenas en los laterales, fuera del eje principal de movimiento.
rock_colors = [(0.33, 0.32, 0.30), (0.40, 0.39, 0.36), (0.28, 0.29, 0.27)]
for index in range(22):
    x = random.uniform(-98.0, 98.0)
    y = random.uniform(-98.0, 98.0)
    if abs(y - road_center(x)) < 13.0 or (abs(x) < 42.0 and abs(y) < 42.0):
        y += 34.0 if y < 0.0 else -34.0
    radius = random.uniform(0.45, 1.25)
    z = terrain_height(x, y) + radius * 0.28
    mesh.ico(x, y, z, radius, random.choice(rock_colors), 1, 1.0, random.uniform(0.75, 1.2), random.uniform(0.45, 0.72))

mesh.export(OUT, "BattlefieldGrassland", recalc_normals=False)
