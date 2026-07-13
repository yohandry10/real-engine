"""Smoke test de editor: crea una batalla y verifica que no use placeholders visuales."""
import unreal


def unit(unit_id, tactical_id, owner, x, y, elements, destroyed=False):
    value = unreal.WLTacticalUnitState()
    value.set_editor_property("unit_id", unit_id)
    value.set_editor_property("tactical_unit_id", tactical_id)
    value.set_editor_property("owner_iso", owner)
    value.set_editor_property("display_name", tactical_id)
    value.set_editor_property("position", unreal.Vector2D(x, y))
    value.set_editor_property("element_count", elements)
    value.set_editor_property("initial_element_count", elements)
    value.set_editor_property("health", 0.0 if destroyed else 100.0)
    value.set_editor_property("morale", 100.0)
    value.set_editor_property("destroyed", destroyed)
    return value


def patch(patch_id, terrain, x, y, radius):
    value = unreal.WLTacticalTerrainPatch()
    value.set_editor_property("patch_id", patch_id)
    value.set_editor_property("terrain", terrain)
    value.set_editor_property("position", unreal.Vector2D(x, y))
    value.set_editor_property("radius", radius)
    return value


unit_specs = [
    ("infantry", "VE-INF", "VE", -1500.0, -320.0, 14),
    ("mbt", "VE-MBT", "VE", -1250.0, 420.0, 4),
    ("artillery", "VE-ART", "VE", -1800.0, 700.0, 3),
    ("apc", "VE-APC", "VE", -1750.0, -760.0, 3),
    ("infantry", "CO-INF", "CO", 1500.0, 320.0, 14),
    ("ifv", "CO-IFV", "CO", 1250.0, -420.0, 4),
    ("sam", "CO-SAM", "CO", 1800.0, -700.0, 3),
    ("heli", "CO-HELI", "CO", 1750.0, 760.0, 2),
]
terrain_patches = [
    patch("URBAN-SMOKE", unreal.WLTacticalTerrain.URBAN, -520.0, 0.0, 440.0),
    patch("FOREST-SMOKE", unreal.WLTacticalTerrain.FOREST, 560.0, 0.0, 470.0),
]
objective = unreal.WLTacticalObjectiveState()
objective.set_editor_property("objective_id", "FORT-SMOKE")
objective.set_editor_property("position", unreal.Vector2D(0.0, 0.0))
objective.set_editor_property("radius", 600.0)


def battle_state(destroyed=False):
    value = unreal.WLTacticalBattleState()
    value.set_editor_property("battle_id", "CODEX-VISUAL-SMOKE")
    value.set_editor_property("attacker_iso", "VE")
    value.set_editor_property("defender_iso", "CO")
    value.set_editor_property("active", not destroyed)
    value.set_editor_property("elapsed_seconds", 1.0 if destroyed else 0.0)
    value.set_editor_property("units", [unit(*spec, destroyed=destroyed) for spec in unit_specs])
    value.set_editor_property("terrain_patches", terrain_patches)
    value.set_editor_property("objectives", [objective])
    return value

world = unreal.EditorLevelLibrary.get_editor_world()
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.WLTacticalBattleView, unreal.Vector())
if not actor:
    raise RuntimeError("No se pudo crear WLTacticalBattleView")
actor.initialize(battle_state(), "VE")
actor.refresh_from_state(battle_state(destroyed=True))

components = actor.get_components_by_class(unreal.StaticMeshComponent)
mesh_names = []
for component in components:
    mesh = component.get_editor_property("static_mesh")
    if mesh:
        mesh_names.append(mesh.get_name())

required = {
    "battlefield_grassland",
    "battle_house", "battle_block", "battle_warehouse", "battle_gas_station",
    "battle_house_ruin", "battle_block_ruin", "battle_warehouse_ruin", "battle_gas_station_ruin",
    "battle_tree_broadleaf_a", "battle_tree_broadleaf_b", "battle_tree_conifer",
    "battle_shrub_a", "battle_shrub_b", "battle_fallen_log",
    "battle_fort_sandbags", "battle_fort_trench_straight", "battle_fort_trench_corner",
    "battle_fort_bunker", "battle_fort_wire", "battle_fort_checkpoint",
    "battle_prop_rocks_a", "battle_prop_rocks_b", "battle_prop_fence",
    "battle_prop_utility_pole", "battle_prop_crater",
    "battle_banner_ve", "battle_banner_co",
    "battle_soldier_kneeling", "battle_soldier_prone",
    "battle_soldier_kneeling_desert", "battle_soldier_prone_desert",
    "battle_wreck_mbt", "battle_wreck_ifv", "battle_wreck_apc",
    "battle_wreck_artillery", "battle_wreck_sam", "battle_wreck_heli",
}
missing = sorted(required.difference(mesh_names))
placeholders = sorted(name for name in mesh_names if name in {"Cube", "Plane"})
if missing:
    raise RuntimeError("Assets no conectados: " + ", ".join(missing))
if placeholders:
    raise RuntimeError("Placeholders visuales encontrados: " + ", ".join(placeholders))

atmospheres = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkyAtmosphere)
clouds = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.VolumetricCloud)
fogs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog)
if not atmospheres or not clouds or not fogs:
    raise RuntimeError("Atmosfera tactica incompleta")

unreal.log(
    "WL_BATTLE_RUNTIME_VALIDATE_DONE: components=%d unique_meshes=%d required=%d"
    % (len(components), len(set(mesh_names)), len(required))
)
actor.destroy_actor()
