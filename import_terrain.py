"""
M1 (baseline SEGURO): importa texturas de terreno a /Game/GenTerrain (para usarlas en el paso
siguiente verificado) y crea un material LIT SIMPLE M_TerrainLit = vertex color (bioma) como base
color, lit, con sombra de relieve real. SIN texturas/normal en el material todavia (eso causaba el
cian: normal mal armada + ambiente azul). Las texturas se anaden como paso aparte ya verificado.
"""
import unreal, os

SRC  = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Terrain"
DEST = "/Game/GenTerrain"
FILES = ["T_Grass_D", "T_Grass_N", "T_Rock_D", "T_Rock_N", "T_Dirt_D", "T_Dirt_N", "T_Snow_D", "T_Snow_N"]

at = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for f in FILES:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f + ".png")
    t.destination_path = DEST
    t.automated = True
    t.save = True
    t.replace_existing = True
    tasks.append(t)
at.import_asset_tasks(tasks)
for f in FILES:
    if f.endswith("_N"):
        tex = unreal.EditorAssetLibrary.load_asset(DEST + "/" + f)
        if tex:
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
unreal.EditorAssetLibrary.save_directory(DEST, False, True)

# Material LIT simple y robusto.
mel = unreal.MaterialEditingLibrary
if unreal.EditorAssetLibrary.does_asset_exist(DEST + "/M_TerrainLit"):
    unreal.EditorAssetLibrary.delete_asset(DEST + "/M_TerrainLit")
mat = at.create_asset("M_TerrainLit", DEST, unreal.Material, unreal.MaterialFactoryNew())
def E(cls, x, y): return mel.create_material_expression(mat, cls, x, y)

vc    = E(unreal.MaterialExpressionVertexColor, -420, -120)
rough = E(unreal.MaterialExpressionConstant, -420, 120); rough.set_editor_property("r", 0.90)
metal = E(unreal.MaterialExpressionConstant, -420, 240); metal.set_editor_property("r", 0.0)
spec  = E(unreal.MaterialExpressionConstant, -420, 360); spec.set_editor_property("r", 0.10)

r = []
r.append(mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR))
r.append(mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS))
r.append(mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC))
r.append(mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR))

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(DEST + "/M_TerrainLit")
unreal.log("WL_TERRAIN_MAT_SIMPLE: connections=%s (base,rough,metal,spec)" % r)
