"""Valida los FBX tacticos reimportandolos en Blender."""
import hashlib
import os
import sys

import bpy


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SRC = argv[0] if argv else r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Battle"
files = sorted(name for name in os.listdir(SRC) if name.lower().endswith(".fbx"))
errors = []
hashes = {}

if len(files) != 38:
    errors.append("expected 38 FBX, found %d" % len(files))

for filename in files:
    path = os.path.join(SRC, filename)
    digest = hashlib.sha256(open(path, "rb").read()).hexdigest()
    if digest in hashes:
        errors.append("duplicate content: %s == %s" % (filename, hashes[digest]))
    hashes[digest] = filename

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if len(objects) != 1:
        errors.append("%s: expected one mesh, found %d" % (filename, len(objects)))
        continue
    obj = objects[0]
    mesh = obj.data
    colors = list(mesh.color_attributes)
    dims = tuple(float(value) for value in obj.dimensions)
    if not colors:
        errors.append("%s: no vertex color attribute" % filename)
    if not mesh.vertices or not mesh.polygons:
        errors.append("%s: empty mesh" % filename)
    if min(dims) <= 0.0:
        errors.append("%s: invalid dimensions %r" % (filename, dims))
    if filename == "battlefield_grassland.fbx":
        color_values = [item.color for item in colors[0].data]
        print(
            "WL_BATTLE_TERRAIN_COLORS: r=%.3f..%.3f g=%.3f..%.3f b=%.3f..%.3f"
            % (
                min(c[0] for c in color_values), max(c[0] for c in color_values),
                min(c[1] for c in color_values), max(c[1] for c in color_values),
                min(c[2] for c in color_values), max(c[2] for c in color_values),
            )
        )
        down_faces = sum(1 for polygon in mesh.polygons if polygon.normal.z < 0.0)
        print(
            "WL_BATTLE_TERRAIN_NORMALS: down=%d min_z=%.3f max_z=%.3f"
            % (down_faces, min(p.normal.z for p in mesh.polygons), max(p.normal.z for p in mesh.polygons))
        )
        # Las rocas cerradas aportan caras inferiores; la superficie del campo son 8192
        # triangulos y debe conservar su winding +Z.
        if down_faces > 1000:
            errors.append("battlefield has %d downward faces" % down_faces)
        if not (219.0 <= dims[0] <= 221.0 and 219.0 <= dims[1] <= 221.0):
            errors.append("battlefield dimensions are not 220x220 m: %r" % (dims,))
        if dims[2] > 4.0:
            errors.append("battlefield relief is too tall: %.3f m" % dims[2])
    print(
        "WL_BATTLE_VALIDATE: %s verts=%d polys=%d colors=%s dims=%.2fx%.2fx%.2f"
        % (filename, len(mesh.vertices), len(mesh.polygons), ",".join(c.name for c in colors), dims[0], dims[1], dims[2])
    )

if errors:
    for error in errors:
        print("WL_BATTLE_VALIDATE_ERROR: " + error)
    raise RuntimeError("battle asset validation failed: %d errors" % len(errors))

print("WL_BATTLE_VALIDATE_DONE: %d unique FBX" % len(files))
