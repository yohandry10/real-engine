# Codex Instructions For This Repo

These notes are repo-level operational context for Codex threads working in
`C:\Users\PC\Desktop\rome-actual`.

## Build And Validation Rule

After code changes, validate from the repo root. Do not assume the project is
good until it compiles and the relevant Standalone flow has been checked.

Before building:

```powershell
git status -sb
Get-Process UnrealEditor,LiveCodingConsole -ErrorAction SilentlyContinue | Stop-Process -Force
```

Do not revert or overwrite unrelated dirty files. Treat existing local changes
as user work unless the user explicitly asks to discard them.

## Compile Command Used Successfully

Use Unreal's `Build.bat` from the repo root as the normal path:

```powershell
$uproject = Resolve-Path 'WorldLeader.uproject'
$build = 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat'
& $build WorldLeaderEditor Win64 Development -Project="$uproject" -WaitMutex -NoHotReloadFromIDE
```

This is the known-good compile route used in the working session. Prefer it
before trying direct `dotnet.exe UnrealBuildTool.dll` calls or wrapper scripts.

If the build fails before UHT/C++ and mentions items like `0xE0434352`,
`Trace.uba`, `AppData`, `Zen`, `Turnkey`, `DerivedDataCache`, or
`UnrealBuildTool`, diagnose it as a local environment/sandbox/permission issue
first, not as a gameplay code failure. Do not hide this as a normal compile
error.

If Codex is running with restricted filesystem permissions and cannot write to
Unreal's AppData/Zen/DDC paths, say that clearly. Do not keep adding code
workarounds before confirming the build environment can actually run Unreal.

## Standalone Validation Command

After compiling, launch Standalone with the campaign auto-started:

```powershell
$editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$uproject = 'C:\Users\PC\Desktop\rome-actual\WorldLeader.uproject'
$args = @(
  $uproject,
  '/Engine/Maps/Entry?game=/Script/WorldLeader.WLCampaignGameMode',
  '-game',
  '-windowed',
  '-ResX=1600',
  '-ResY=900',
  '-WLAutoStart=CO',
  '-log'
)
Start-Process -FilePath $editor -ArgumentList $args -PassThru
```

For the America view, add:

```powershell
'-WLAmericaView'
```

Useful log checks:

```powershell
rg -n "Campaign3D zoom|panel closed|selected territory|selected city|selected force" Saved/Logs/WorldLeader.log
```

## Campaign 3D Regression Checks

When touching Campaign 3D, verify these in Standalone:

- Region zoom (about 620k-1.2M camera height, e.g. 700k/730k) is a
  strategic map only. Do not show detailed 3D terrain, relief/biome bands,
  roads, settlements, forces, vegetation, ports, or detailed-layer labels in
  Region. Detailed Campaign 3D presentation starts only in Theater/Close zoom.
- `+` zooms in consistently.
- `-` zooms out consistently.
- Mouse wheel follows the same direction convention.
- Zoom does not select provinces, cities, or forces by itself.
- Right-click drag/pan keeps working.
- `[G] America` and `[F] Teatro` keep their expected views.
- The contextual panel `X` closes the panel and clears the active selection.
- Close-range labels do not cover city labels with huge country labels.
- Colombia/Venezuela high-detail theatre remains intact.
- Diplomacy and main menu are not touched unless explicitly requested.

## Git

Do not push, force-push, reset, or discard work unless the user explicitly asks.
If reverting to a commit is requested, compile afterward and report the exact
commit and build result.
