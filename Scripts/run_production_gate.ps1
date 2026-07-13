param(
    [switch]$Package,
    [switch]$SkipStandalone
)

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $Repo 'WorldLeader.uproject'
$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$Build = Join-Path $EngineRoot 'Build\BatchFiles\Build.bat'
$EditorCmd = Join-Path $EngineRoot 'Binaries\Win64\UnrealEditor-Cmd.exe'
$Editor = Join-Path $EngineRoot 'Binaries\Win64\UnrealEditor.exe'
$Log = Join-Path $Repo 'Saved\Logs\WorldLeader.log'

function Invoke-AutomationGroup([string]$Filter) {
    Write-Host "[gate] Automation: $Filter"
    & $EditorCmd $Project -Unattended -NoSplash -NullRHI `
        "-ExecCmds=Automation RunTests $Filter; Quit" `
        '-TestExit=Automation Test Queue Empty' -log
    if ($LASTEXITCODE -ne 0) {
        throw "UnrealEditor-Cmd fallo para $Filter con codigo $LASTEXITCODE."
    }
    $Failures = Select-String -Path $Log -Pattern 'Test Completed\. Result=\{(Fail|Failed)\}.*Path=\{WorldLeader\.'
    if ($Failures) {
        $Failures | ForEach-Object { Write-Error $_.Line }
        throw "Pruebas fallidas en $Filter."
    }
    $Successes = @(Select-String -Path $Log -Pattern 'Test Completed\. Result=\{Success\}.*Path=\{WorldLeader\.').Count
    if ($Successes -eq 0) {
        throw "El filtro $Filter no ejecuto pruebas WorldLeader."
    }
    Write-Host "[gate] ${Filter}: $Successes pruebas correctas."
}

Push-Location $Repo
try {
    Get-Process UnrealEditor, UnrealEditor-Cmd, LiveCodingConsole -ErrorAction SilentlyContinue |
        Stop-Process -Force

    Write-Host '[gate] Compilando WorldLeaderEditor Win64 Development.'
    & $Build WorldLeaderEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw 'La compilacion de editor fallo.' }

    # Los grupos costosos se separan para identificar cuellos de botella y conservar logs concluyentes.
    @(
        'WorldLeader.Campaign.Domain',
        'WorldLeader.Campaign.Integration',
        'WorldLeader.Campaign.RouteGraph',
        'WorldLeader.Campaign.Simulation',
        'WorldLeader.Balance',
        'WorldLeader.Battle',
        'WorldLeader.Construction',
        'WorldLeader.Data',
        'WorldLeader.Economy',
        'WorldLeader.EconomyAI',
        'WorldLeader.Government.Characters',
        'WorldLeader.Government.MinisterEffects',
        'WorldLeader.Government.P2',
        'WorldLeader.Government.PoliticalAction',
        'WorldLeader.Government.RealGovernment',
        'WorldLeader.Military',
        'WorldLeader.Politics',
        'WorldLeader.ProvinceState',
        'WorldLeader.Save',
        'WorldLeader.SaveGame'
    ) | ForEach-Object { Invoke-AutomationGroup $_ }

    if (-not $SkipStandalone) {
        Write-Host '[gate] Smoke Standalone CO.'
        $Args = @(
            $Project,
            '/Engine/Maps/Entry?game=/Script/WorldLeader.WLCampaignGameMode',
            '-game', '-windowed', '-ResX=1280', '-ResY=720', '-WLAutoStart=CO', '-log'
        )
        $Process = Start-Process -FilePath $Editor -ArgumentList $Args -PassThru
        Start-Sleep -Seconds 25
        if ($Process.HasExited -and $Process.ExitCode -ne 0) {
            throw "Standalone termino con codigo $($Process.ExitCode)."
        }
        if (-not $Process.HasExited) { Stop-Process -Id $Process.Id -Force }
        if (Select-String -Path $Log -Pattern 'Fatal error:|Unhandled Exception|Assertion failed:') {
            throw 'Standalone registro un error fatal.'
        }
    }

    if ($Package) {
        Write-Host '[gate] Empaquetando Win64 Shipping.'
        $UAT = Join-Path $EngineRoot 'Build\BatchFiles\RunUAT.bat'
        $Archive = Join-Path $Repo 'Build\ProductionGate'
        & $UAT BuildCookRun "-project=$Project" -noP4 -platform=Win64 -clientconfig=Shipping `
            -build -cook -stage -pak -archive "-archivedirectory=$Archive"
        if ($LASTEXITCODE -ne 0) { throw 'El empaquetado Shipping fallo.' }
    }

    Write-Host '[gate] PRODUCCION: VERDE'
}
finally {
    Pop-Location
}
