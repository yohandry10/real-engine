import unreal
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
pkg = "/Game/GenVehicle"; name = "M_VehicleUnlit"; full = pkg + "/" + name
out = []
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
mat = at.create_asset(name, pkg, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
# Color del tanque = vertex color (unlit) -> emissive (salida por defecto, nombre vacio).
vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -380, 0)
out.append("emissive=%s" % mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR))
# Siempre visible (no lo tapan edificios/terreno): translucido + disable depth test + opacidad 1 (solido).
ddt = False
try:
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("disable_depth_test", True)
    op = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -380, 220)
    op.set_editor_property("r", 1.0)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    ddt = True
except Exception as e:
    out.append("ddt_err=%s" % e)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
out.append("disable_depth_test=%s" % ddt)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full)
out.append("shading=%s blend=%s" % (str(mat.get_editor_property("shading_model")), str(mat.get_editor_property("blend_mode"))))
with open(r"C:\Users\PC\Desktop\rome-actual\mat_result.txt", "w") as f:
    f.write("\n".join(out))
