"""Utilidades compartidas para los generadores Blender de la batalla tactica."""
import math
import os

import bpy
import bmesh
import mathutils


SUN = mathutils.Vector((0.48, -0.32, 0.81)).normalized()


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def rotation(axis, degrees):
    return mathutils.Matrix.Rotation(math.radians(degrees), 4, axis.upper())


class BattleMesh:
    def __init__(self):
        self.bm = bmesh.new()
        self.colors = self.bm.loops.layers.color.new("Col")

    def _new_geometry(self, before):
        faces = [face for face in self.bm.faces if face not in before]
        verts = {vert for face in faces for vert in face.verts}
        return verts, faces

    def colorize(self, faces, base, shade=True):
        for face in faces:
            face.normal_update()
            light = max(0.0, face.normal.dot(SUN)) if shade else 0.45
            scale = 0.68 + 0.66 * light
            color = (
                min(1.0, base[0] * scale),
                min(1.0, base[1] * scale),
                min(1.0, base[2] * scale),
                1.0,
            )
            for loop in face.loops:
                loop[self.colors] = color

    def box(self, cx, cy, cz, sx, sy, sz, color, rot=None):
        before = set(self.bm.faces)
        bmesh.ops.create_cube(self.bm, size=1.0)
        verts, faces = self._new_geometry(before)
        for vert in verts:
            point = mathutils.Vector((vert.co.x * sx, vert.co.y * sy, vert.co.z * sz))
            if rot is not None:
                point = rot @ point
            vert.co = (point.x + cx, point.y + cy, point.z + cz)
        self.colorize(faces, color)
        return faces

    def floor_box(self, cx, cy, z0, sx, sy, sz, color, rot=None):
        return self.box(cx, cy, z0 + sz * 0.5, sx, sy, sz, color, rot)

    def cylinder(self, cx, cy, cz, radius, length, axis, color, segments=10, radius2=None):
        before = set(self.bm.faces)
        bmesh.ops.create_cone(
            self.bm,
            cap_ends=True,
            cap_tris=False,
            segments=segments,
            radius1=radius,
            radius2=radius if radius2 is None else max(0.005, radius2),
            depth=length,
        )
        verts, faces = self._new_geometry(before)
        if axis.lower() == "x":
            rot = rotation("Y", 90)
        elif axis.lower() == "y":
            rot = rotation("X", 90)
        else:
            rot = mathutils.Matrix.Identity(4)
        for vert in verts:
            point = rot @ vert.co
            vert.co = (point.x + cx, point.y + cy, point.z + cz)
        self.colorize(faces, color)
        return faces

    def cylinder_between(self, start, end, radius, color, segments=8, radius2=None):
        start = mathutils.Vector(start)
        end = mathutils.Vector(end)
        delta = end - start
        length = delta.length
        if length <= 0.0001:
            return []
        before = set(self.bm.faces)
        bmesh.ops.create_cone(
            self.bm,
            cap_ends=True,
            cap_tris=False,
            segments=segments,
            radius1=radius,
            radius2=radius if radius2 is None else max(0.005, radius2),
            depth=length,
        )
        verts, faces = self._new_geometry(before)
        direction = delta.normalized()
        quat = mathutils.Vector((0.0, 0.0, 1.0)).rotation_difference(direction)
        center = (start + end) * 0.5
        for vert in verts:
            point = quat @ vert.co
            vert.co = point + center
        self.colorize(faces, color)
        return faces

    def cone(self, cx, cy, z0, radius1, radius2, depth, color, segments=10):
        before = set(self.bm.faces)
        bmesh.ops.create_cone(
            self.bm,
            cap_ends=True,
            cap_tris=False,
            segments=segments,
            radius1=radius1,
            radius2=max(0.005, radius2),
            depth=depth,
        )
        verts, faces = self._new_geometry(before)
        for vert in verts:
            vert.co.z += z0 + depth * 0.5
            vert.co.x += cx
            vert.co.y += cy
        self.colorize(faces, color)
        return faces

    def ico(self, cx, cy, cz, radius, color, subdivisions=1, sx=1.0, sy=1.0, sz=1.0):
        before = set(self.bm.faces)
        bmesh.ops.create_icosphere(self.bm, subdivisions=subdivisions, radius=radius)
        verts, faces = self._new_geometry(before)
        for vert in verts:
            vert.co = (
                vert.co.x * sx + cx,
                vert.co.y * sy + cy,
                vert.co.z * sz + cz,
            )
        self.colorize(faces, color)
        return faces

    def add_poly_mesh(self, vertices, polygons, colors):
        verts = [self.bm.verts.new(vertex) for vertex in vertices]
        self.bm.verts.ensure_lookup_table()
        created = []
        for index, polygon in enumerate(polygons):
            face = self.bm.faces.new([verts[i] for i in polygon])
            created.append(face)
            base = colors[index] if isinstance(colors, list) else colors
            self.colorize([face], base)
        return created

    def export(self, out_path, object_name, recalc_normals=True):
        if recalc_normals:
            bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        mesh = bpy.data.meshes.new(object_name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        mesh.materials.append(bpy.data.materials.new("BattleVertexColor"))
        obj = bpy.data.objects.new(object_name, mesh)
        bpy.context.scene.collection.objects.link(obj)

        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.fbx(
            filepath=out_path,
            use_selection=True,
            apply_unit_scale=True,
            mesh_smooth_type="FACE",
            colors_type="LINEAR",
        )
        print(
            "WL_BATTLE_ASSET_DONE: %s verts=%d polys=%d"
            % (out_path, len(mesh.vertices), len(mesh.polygons))
        )
