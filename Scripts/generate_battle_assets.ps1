param(
    [string]$BlenderPath = "C:\Users\PC\Blender42\blender-4.2.9-windows-x64\blender.exe",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot "ExternalAssets\Generated\Battle"
}
if (-not (Test-Path -LiteralPath $BlenderPath)) {
    throw "Blender no encontrado: $BlenderPath"
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

function Add-Task {
    param([string]$Group, [string]$Script, [string]$File, [string[]]$GeneratorArgs)
    [PSCustomObject]@{
        Group = $Group
        Script = Join-Path $RepoRoot $Script
        Output = Join-Path $OutputDir $File
        GeneratorArgs = $GeneratorArgs
    }
}

$Tasks = @(
    # 1. El campo ocupa la mayor parte del encuadre.
    Add-Task "01-terreno" "gen_battlefield.py" "battlefield_grassland.fbx" @("104")

    # 2. Cuatro modulos intactos y sus cuatro variantes destruidas.
    Add-Task "02-edificios" "gen_battle_buildings.py" "battle_house.fbx" @("house", "intact", "201")
    Add-Task "02-edificios" "gen_battle_buildings.py" "battle_house_ruin.fbx" @("house", "ruin", "202")
    Add-Task "02-edificios" "gen_battle_buildings.py" "battle_block.fbx" @("block", "intact", "203")
    Add-Task "02-edificios" "gen_battle_buildings.py" "battle_block_ruin.fbx" @("block", "ruin", "204")
    Add-Task "02-edificios" "gen_battle_buildings.py" "battle_warehouse.fbx" @("warehouse", "intact", "205")
    Add-Task "02-edificios" "gen_battle_buildings.py" "battle_warehouse_ruin.fbx" @("warehouse", "ruin", "206")
    Add-Task "02-edificios" "gen_battle_buildings.py" "battle_gas_station.fbx" @("gas_station", "intact", "207")
    Add-Task "02-edificios" "gen_battle_buildings.py" "battle_gas_station_ruin.fbx" @("gas_station", "ruin", "208")

    # 3. Tres arboles, dos arbustos y un tronco caido.
    Add-Task "03-naturaleza" "gen_battle_nature.py" "battle_tree_broadleaf_a.fbx" @("tree_broadleaf_a", "301")
    Add-Task "03-naturaleza" "gen_battle_nature.py" "battle_tree_broadleaf_b.fbx" @("tree_broadleaf_b", "302")
    Add-Task "03-naturaleza" "gen_battle_nature.py" "battle_tree_conifer.fbx" @("tree_conifer", "303")
    Add-Task "03-naturaleza" "gen_battle_nature.py" "battle_shrub_a.fbx" @("shrub_a", "304")
    Add-Task "03-naturaleza" "gen_battle_nature.py" "battle_shrub_b.fbx" @("shrub_b", "305")
    Add-Task "03-naturaleza" "gen_battle_nature.py" "battle_fallen_log.fbx" @("fallen_log", "306")

    # 4. Estandartes por bando.
    Add-Task "04-estandartes" "gen_battle_banners.py" "battle_banner_ve.fbx" @("ve")
    Add-Task "04-estandartes" "gen_battle_banners.py" "battle_banner_co.fbx" @("co")

    # 5. Restos separados del generador de vehiculos vivos.
    Add-Task "05-restos" "gen_battle_wrecks.py" "battle_wreck_mbt.fbx" @("mbt", "501")
    Add-Task "05-restos" "gen_battle_wrecks.py" "battle_wreck_ifv.fbx" @("ifv", "502")
    Add-Task "05-restos" "gen_battle_wrecks.py" "battle_wreck_apc.fbx" @("apc", "503")
    Add-Task "05-restos" "gen_battle_wrecks.py" "battle_wreck_artillery.fbx" @("artillery", "504")
    Add-Task "05-restos" "gen_battle_wrecks.py" "battle_wreck_sam.fbx" @("sam", "505")
    Add-Task "05-restos" "gen_battle_wrecks.py" "battle_wreck_heli.fbx" @("heli", "506")

    # 6. Fortificaciones visibles.
    Add-Task "06-fortificaciones" "gen_battle_fortifications.py" "battle_fort_sandbags.fbx" @("sandbags", "601")
    Add-Task "06-fortificaciones" "gen_battle_fortifications.py" "battle_fort_trench_straight.fbx" @("trench_straight", "602")
    Add-Task "06-fortificaciones" "gen_battle_fortifications.py" "battle_fort_trench_corner.fbx" @("trench_corner", "603")
    Add-Task "06-fortificaciones" "gen_battle_fortifications.py" "battle_fort_bunker.fbx" @("bunker", "604")
    Add-Task "06-fortificaciones" "gen_battle_fortifications.py" "battle_fort_wire.fbx" @("wire", "605")
    Add-Task "06-fortificaciones" "gen_battle_fortifications.py" "battle_fort_checkpoint.fbx" @("checkpoint", "606")

    # 7. Dos poses por cada paleta de bando.
    Add-Task "07-poses" "gen_battle_soldiers.py" "battle_soldier_kneeling.fbx" @("kneeling", "green")
    Add-Task "07-poses" "gen_battle_soldiers.py" "battle_soldier_prone.fbx" @("prone", "green")
    Add-Task "07-poses" "gen_battle_soldiers.py" "battle_soldier_kneeling_desert.fbx" @("kneeling", "desert")
    Add-Task "07-poses" "gen_battle_soldiers.py" "battle_soldier_prone_desert.fbx" @("prone", "desert")

    # 8. Densidad ambiental de bajo coste.
    Add-Task "08-props" "gen_battle_props.py" "battle_prop_rocks_a.fbx" @("rocks_a", "801")
    Add-Task "08-props" "gen_battle_props.py" "battle_prop_rocks_b.fbx" @("rocks_b", "802")
    Add-Task "08-props" "gen_battle_props.py" "battle_prop_fence.fbx" @("fence", "803")
    Add-Task "08-props" "gen_battle_props.py" "battle_prop_utility_pole.fbx" @("utility_pole", "804")
    Add-Task "08-props" "gen_battle_props.py" "battle_prop_crater.fbx" @("crater", "805")
)

$CurrentGroup = ""
foreach ($Task in $Tasks) {
    if ($Task.Group -ne $CurrentGroup) {
        $CurrentGroup = $Task.Group
        Write-Host "`n[$CurrentGroup]"
    }
    Write-Host "  -> $(Split-Path -Leaf $Task.Output)"
    if (-not $Task.GeneratorArgs -or $Task.GeneratorArgs.Count -eq 0) {
        throw "Tarea sin argumentos de generador: $($Task.Output)"
    }
    $BlenderArgs = @("--background", "--python", $Task.Script, "--", $Task.Output) + [string[]]$Task.GeneratorArgs
    & $BlenderPath $BlenderArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Fallo Blender ($LASTEXITCODE): $($Task.Output)"
    }
}

$Generated = Get-ChildItem -LiteralPath $OutputDir -Filter *.fbx
if ($Generated.Count -ne $Tasks.Count) {
    throw "Se esperaban $($Tasks.Count) FBX y hay $($Generated.Count) en $OutputDir"
}
Write-Host "`nWL_BATTLE_BATCH_DONE: $($Generated.Count) FBX en $OutputDir"
