"""
Crea /Game/UI/Battle/M_BattleSprite: material UNLIT + TRANSLUCENT + two-sided con un
TextureSampleParameter2D "Sprite" (RGB -> Emissive x Tint, A -> Opacity x OpacityScale).
La vista de batalla crea un MID por efecto, le pone la textura del sprite (cargada en runtime
con LoadExternalTexture) y lo aplica a un plano que encara la camara. Un solo material para
humo/explosion/fogonazo/impacto.

Uso:  UnrealEditor-Cmd <proj> -ExecutePythonScript="create_battle_material.py" -unattended -nosplash
"""
import unreal

PKG = "/Game/UI/Battle"
NAME = "M_BattleSprite"
full = PKG + "/" + NAME

if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)

tools = unreal.AssetToolsHelpers.get_asset_tools()
mat = tools.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)

MEL = unreal.MaterialEditingLibrary

tex = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
tex.set_editor_property("parameter_name", "Sprite")

tint = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 320)
tint.set_editor_property("parameter_name", "Tint")
tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

opac = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 470)
opac.set_editor_property("parameter_name", "OpacityScale")
opac.set_editor_property("default_value", 1.0)

mul_rgb = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 40)
MEL.connect_material_expressions(tex, "RGB", mul_rgb, "A")
MEL.connect_material_expressions(tint, "", mul_rgb, "B")

mul_a = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 350)
MEL.connect_material_expressions(tex, "A", mul_a, "A")
MEL.connect_material_expressions(opac, "", mul_a, "B")

MEL.connect_material_property(mul_rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(mul_a, "", unreal.MaterialProperty.MP_OPACITY)

MEL.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full)
unreal.log("WL_BATTLEMAT_DONE: " + full)
