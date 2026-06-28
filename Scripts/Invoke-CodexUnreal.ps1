param(
    [ValidateSet('Env', 'GitStatus', 'Build', 'Standalone', 'StandaloneAmerica', 'StandaloneOutpostScreenshot', 'StandalonePortScreenshot')]
    [string]$Action = 'Env',

    [switch]$WaitForBuildView,
    [int]$WaitTimeoutSeconds = 300,
    [switch]$NoStopEditor,
    [string]$OutpostVisualTest = 'VENEZUELA',
    [float]$OutpostVisualHeight = 42000,
    [string]$PortVisualTest = 'VENEZUELA',
    [float]$PortVisualHeight = 36000
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
$Project = Join-Path $RepoRoot 'WorldLeader.uproject'

function New-CodexDirectory {
    param([string]$Path)
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
    return (Resolve-Path $Path).Path
}

function Set-CodexUnrealEnvironment {
    $CodexRoot = New-CodexDirectory (Join-Path $RepoRoot 'Intermediate\CodexRuntime')
    $SessionRoot = New-CodexDirectory (Join-Path $CodexRoot ("Sessions\{0:yyyyMMdd-HHmmss}-{1}" -f (Get-Date), $PID))
    $CodexHome = New-CodexDirectory (Join-Path $CodexRoot 'Home')
    $CodexConfig = New-CodexDirectory (Join-Path $CodexHome '.config')
    $CodexAppData = New-CodexDirectory (Join-Path $CodexRoot 'AppDataRoaming')
    $CodexLocalAppData = New-CodexDirectory (Join-Path $CodexRoot 'AppDataLocal')
    $CodexTemp = New-CodexDirectory (Join-Path $SessionRoot 'Temp')
    $CodexDdc = New-CodexDirectory (Join-Path $CodexRoot 'DDC')
    $CodexZenData = New-CodexDirectory (Join-Path $CodexRoot 'Zen\Data')
    $CodexShaderDir = New-CodexDirectory (Join-Path $SessionRoot 'ShaderWorkingDir')

    $env:HOME = $CodexHome
    $env:XDG_CONFIG_HOME = $CodexConfig
    $env:APPDATA = $CodexAppData
    $env:LOCALAPPDATA = $CodexLocalAppData
    $env:TEMP = $CodexTemp
    $env:TMP = $CodexTemp
    $env:GIT_CONFIG_GLOBAL = 'NUL'
    $env:GIT_CONFIG_SYSTEM = 'NUL'
    $env:GIT_CONFIG_NOSYSTEM = '1'

    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $CodexDdc, 'Process')
    [Environment]::SetEnvironmentVariable('UE-ZenDataPath', $CodexZenData, 'Process')
    [Environment]::SetEnvironmentVariable('UE-ZenSubprocessDataPath', $CodexZenData, 'Process')

    return [pscustomobject]@{
        RepoRoot = $RepoRoot.Path
        AppData = $CodexAppData
        LocalAppData = $CodexLocalAppData
        Temp = $CodexTemp
        DDC = $CodexDdc
        ZenData = $CodexZenData
        ShaderWorkingDir = $CodexShaderDir
    }
}

function Stop-UnrealProcesses {
    if (-not $NoStopEditor) {
        Get-Process UnrealEditor,LiveCodingConsole -ErrorAction SilentlyContinue | Stop-Process -Force
    }
}

function Wait-WorldLeaderBuildView {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds
    )

    $log = Join-Path $RepoRoot 'Saved\Logs\WorldLeader.log'
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $seen = @{}
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        $alive = Get-Process -Id $Process.Id -ErrorAction SilentlyContinue
        if (Test-Path $log) {
            $matches = rg -n "Command Line:|Campania iniciada|Campaign3D BuildView|road asset layer|Fatal error|LogWindows: Error|Error:|LogZenServiceInstance|ShaderWorkingDir|UnrealShaderWorkingDir|RequestExit|LogExit" $log 2>$null
            foreach ($match in $matches) {
                if (-not $seen.ContainsKey($match)) {
                    $seen[$match] = $true
                    $match
                }
            }
            if ($matches -match 'Campaign3D BuildView complete') {
                'VALIDATION_REACHED_BUILDVIEW_COMPLETE'
                return
            }
            if ($matches -match 'Fatal error|LogWindows: Error') {
                'VALIDATION_FOUND_FATAL'
                return
            }
        }
        if (-not $alive) {
            'VALIDATION_PROCESS_EXITED'
            return
        }
    }
    'VALIDATION_TIMEOUT'
}

$envInfo = Set-CodexUnrealEnvironment
Set-Location $RepoRoot

switch ($Action) {
    'Env' {
        $envInfo | Format-List
    }

    'GitStatus' {
        git -c core.excludesfile= status -sb
    }

    'Build' {
        Stop-UnrealProcesses
        $build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
        $buildLog = Join-Path $RepoRoot 'Saved\UBT-Codex-Build.log'
        & $build WorldLeaderEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE "-Log=$buildLog"
        if ($LASTEXITCODE -ne 0) {
            $buildBatExitCode = $LASTEXITCODE
            $patchedUbt = Join-Path $RepoRoot 'Intermediate\CodexUBT\UnrealBuildTool\UnrealBuildTool.dll'
            if (-not (Test-Path $patchedUbt)) {
                throw "Build.bat failed with exit code $buildBatExitCode and patched UBT fallback is missing at $patchedUbt"
            }

            Write-Warning "Build.bat failed before compilation with exit code $buildBatExitCode; using local patched UBT fallback."
            $env:UE_CODEX_USER_SETTING_DIR = New-CodexDirectory (Join-Path $RepoRoot 'Intermediate\CodexUEUserSettings')
            $env:UE_CODEX_ROOT_DIRECTORY = $EngineRoot
            $env:UBA_ROOT = New-CodexDirectory (Join-Path $RepoRoot 'Intermediate\CodexUBAInstalled')
            $dotnet = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
            $patchedLog = Join-Path $RepoRoot 'Saved\UBT-Codex-PatchedInstalled.log'
            & $dotnet $patchedUbt "-rootdirectory=$EngineRoot" WorldLeaderEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE "-Log=$patchedLog"
            if ($LASTEXITCODE -ne 0) {
                throw "Patched UBT fallback failed with exit code $LASTEXITCODE"
            }
        }
    }

    'Standalone' {
        Stop-UnrealProcesses
        $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
        $args = @(
            $Project,
            '/Engine/Maps/Entry?game=/Script/WorldLeader.WLCampaignGameMode',
            '-game',
            '-windowed',
            '-ResX=1600',
            '-ResY=900',
            '-WLAutoStart=CO',
            "-ShaderWorkingDir=$($envInfo.ShaderWorkingDir)",
            '-DDC-ForceMemoryCache',
            '-NoZenAutoLaunch',
            '-log'
        )
        $process = Start-Process -FilePath $editor -ArgumentList $args -PassThru
        $process
        if ($WaitForBuildView) {
            Wait-WorldLeaderBuildView -Process $process -TimeoutSeconds $WaitTimeoutSeconds
        }
    }

    'StandaloneAmerica' {
        Stop-UnrealProcesses
        $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
        $args = @(
            $Project,
            '/Engine/Maps/Entry?game=/Script/WorldLeader.WLCampaignGameMode',
            '-game',
            '-windowed',
            '-ResX=1600',
            '-ResY=900',
            '-WLAutoStart=CO',
            '-WLAmericaView',
            "-ShaderWorkingDir=$($envInfo.ShaderWorkingDir)",
            '-DDC-ForceMemoryCache',
            '-NoZenAutoLaunch',
            '-log'
        )
        $process = Start-Process -FilePath $editor -ArgumentList $args -PassThru
        $process
        if ($WaitForBuildView) {
            Wait-WorldLeaderBuildView -Process $process -TimeoutSeconds $WaitTimeoutSeconds
        }
    }

    'StandaloneOutpostScreenshot' {
        Stop-UnrealProcesses
        $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
        $screenshot = Join-Path $RepoRoot 'Saved\Screenshots\Windows\CodexCityVisual.png'
        if (Test-Path $screenshot) {
            Remove-Item -LiteralPath $screenshot -Force
        }
        $isSequence = $OutpostVisualTest -in @('ALL', 'COUNTRIES')
        $args = @(
            $Project,
            '/Engine/Maps/Entry?game=/Script/WorldLeader.WLCampaignGameMode',
            '-game',
            '-windowed',
            '-ResX=1600',
            '-ResY=900',
            '-WLAutoStart=CO',
            '-unattended',
            "-WLBorderOutpostVisualTest=$OutpostVisualTest",
            "-WLBorderOutpostVisualHeight=$OutpostVisualHeight",
            "-ShaderWorkingDir=$($envInfo.ShaderWorkingDir)",
            '-DDC-ForceMemoryCache',
            '-NoZenAutoLaunch',
            '-log'
        )
        if ($isSequence) {
            $args += "-WLBorderOutpostScreenshotSequence=$OutpostVisualTest"
            $args += '-WLBorderOutpostQuitAfterSequence'
        }
        else {
            $args += '-WLCityVisualScreenshot=6'
            $args += '-WLCityVisualQuitAfterScreenshot'
        }
        $startedAt = Get-Date
        $process = Start-Process -FilePath $editor -ArgumentList $args -PassThru
        $process
        $deadline = (Get-Date).AddSeconds($WaitTimeoutSeconds)
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Seconds 3
            if ($isSequence -and -not (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) {
                Get-ChildItem (Join-Path $RepoRoot 'Saved\Screenshots\Windows') -Filter 'BorderOutpost_*.png' |
                    Where-Object { $_.LastWriteTime -ge $startedAt } |
                    Sort-Object Name
                return
            }
            if ((Test-Path $screenshot) -and -not (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) {
                Get-Item $screenshot
                return
            }
            if (-not (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) {
                break
            }
        }
        if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) {
            Stop-Process -Id $process.Id -Force
        }
        if ($isSequence) {
            $sequenceShots = Get-ChildItem (Join-Path $RepoRoot 'Saved\Screenshots\Windows') -Filter 'BorderOutpost_*.png' |
                Where-Object { $_.LastWriteTime -ge $startedAt } |
                Sort-Object Name
            if (-not $sequenceShots) {
                throw "Outpost screenshot sequence was not created for $OutpostVisualTest"
            }
            $sequenceShots
            return
        }
        if (-not (Test-Path $screenshot)) {
            throw "Outpost screenshot was not created for $OutpostVisualTest"
        }
        Get-Item $screenshot
    }

    'StandalonePortScreenshot' {
        Stop-UnrealProcesses
        $editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
        $isSequence = $true
        $args = @(
            $Project,
            '/Engine/Maps/Entry?game=/Script/WorldLeader.WLCampaignGameMode',
            '-game',
            '-windowed',
            '-ResX=1600',
            '-ResY=900',
            '-WLAutoStart=CO',
            '-unattended',
            "-WLPortFacilityVisualTest=$PortVisualTest",
            "-WLPortFacilityVisualHeight=$PortVisualHeight",
            "-ShaderWorkingDir=$($envInfo.ShaderWorkingDir)",
            '-DDC-ForceMemoryCache',
            '-NoZenAutoLaunch',
            '-log'
        )
        $args += "-WLPortFacilityScreenshotSequence=$PortVisualTest"
        $args += '-WLPortFacilityQuitAfterSequence'
        $startedAt = Get-Date
        $process = Start-Process -FilePath $editor -ArgumentList $args -PassThru
        $process
        $deadline = (Get-Date).AddSeconds($WaitTimeoutSeconds)
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Seconds 3
            if ($isSequence -and -not (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) {
                Get-ChildItem (Join-Path $RepoRoot 'Saved\Screenshots\Windows') -Filter 'PortFacility_*.png' |
                    Where-Object { $_.LastWriteTime -ge $startedAt } |
                    Sort-Object Name
                return
            }
            if (-not (Get-Process -Id $process.Id -ErrorAction SilentlyContinue)) {
                break
            }
        }
        if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) {
            Stop-Process -Id $process.Id -Force
        }
        $sequenceShots = Get-ChildItem (Join-Path $RepoRoot 'Saved\Screenshots\Windows') -Filter 'PortFacility_*.png' |
            Where-Object { $_.LastWriteTime -ge $startedAt } |
            Sort-Object Name
        if (-not $sequenceShots) {
            throw "Port facility screenshot sequence was not created for $PortVisualTest"
        }
        $sequenceShots
    }
}
