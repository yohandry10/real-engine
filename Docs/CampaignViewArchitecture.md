# WORLD LEADER Campaign Presentation Architecture

## Decision

Campaign state and campaign presentation are separate.

Shared state lives in the existing campaign systems:
- `UWLCampaignGameInstance`: active campaign and selected nation.
- `UWLDataRegistry`: countries, provinces, cities, terrain, ownership seed data.
- `UWLStrategicTickSubsystem`: mutable campaign state such as treasury, date, province control and buildings.
- `UWLMilitarySubsystem`: armies and military state.

Visual representation is split into two presentation layers:

1. `AWLCampaign3DView`
   - Primary campaign view.
   - Regional 3D theatre for Colombia and Venezuela.
   - Shows sea, relief, routes, province/city markers and camera/light setup.
   - Owns presentation LOD only; it does not own campaign rules.
   - Reads the same province/nation data as the political map.

2. `AWLWorldMap`
   - Diplomacy/cartography view.
   - Keeps the existing America political map.
   - It is not the default campaign camera anymore.
   - It is activated only when the player enters Diplomacy.

## Runtime Flow

`AWLCampaignGameMode::StartCampaignWorld()` creates both views:

- Campaign 3D View starts active and owns the camera.
- Diplomacy Map View is built, hidden and has collision disabled.
- `AWLCampaignPlayerController` switches between views with the HUD button or `D`.
- Both views query the same `UWLDataRegistry` and campaign subsystems; no gameplay state is duplicated.

## Campaign 3D Scale And LOD

Campaign 3D uses a multi-scale presentation stack:

- Sea/water base remains visible at every campaign zoom.
- `FWLCampaignOverviewBuilder` adds low-detail North America, Central America, Caribbean and South America landmasses for far zooms.
- `Content/Data/Campaign3D/AmericaLowDetail.json` owns the approved Campaign 3D low-detail country and city marker list.
- Detailed Colombia/Venezuela terrain remains the only high-detail theatre.
- Roads, settlement meshes, trees, ports and army markers are hidden progressively as the camera moves away.

LOD thresholds are currently presentation-only:

- `Close`: settlement meshes, roads, labels and minor markers visible.
- `Theater`: active Colombia/Venezuela theatre remains detailed.
- `Region`: overview landmass appears; fine detail is reduced.
- `Global`: only strategic overview, labels and sea remain visible.

The player controller owns camera input and clamps movement against bounds that expand with zoom. This keeps close zoom focused on the theatre while allowing far zooms to frame the Caribbean/Americas context.

## Verification Rule

Visual/runtime verification for this repo must use Standalone Game. Editor viewport screenshots, desktop screenshots, or captures with editor panels are not valid final evidence for Campaign 3D work.

## Known Follow-Up

The overview layer now reads approved America-scale low-detail data and real GeoJSON country silhouettes, with clean placeholders only if a listed territory cannot be read from the local GeoJSON. Future work can improve art direction with bespoke meshes/materials, but should keep Colombia/Venezuela as the only high-detail theatre until the scope changes.

## Reused Assets And Data

Reused:
- `Content/Data/Nations/Nations.json`
- `Content/Data/Provinces/Provinces.json`
- `Content/Data/Geo/Countries.geojson`
- `Content/Data/Buildings/Buildings.json`
- `Content/Data/Units/Units.json`
- `AWLWorldMap` political/cartographic renderer
- `AWLCampaignHUD` as the temporary command UI layer

New:
- `Source/WorldLeader/Presentation/WLCampaign3DView.h`
- `Source/WorldLeader/Presentation/WLCampaign3DView.cpp`
- `Source/WorldLeader/Presentation/WLCampaignAmericaLowDetailData.h`
- `Source/WorldLeader/Presentation/WLCampaignAmericaLowDetailData.cpp`
- `Source/WorldLeader/Presentation/WLCampaignOverviewBuilder.h`
- `Source/WorldLeader/Presentation/WLCampaignOverviewBuilder.cpp`
- `Content/Data/Campaign3D/AmericaLowDetail.json`

Changed:
- `AWLCampaignGameMode` now orchestrates both presentation layers.
- `AWLCampaignPlayerController` owns view switching and routes selection by active view.
- `AWLCampaignPlayerController` owns zoom, pan, reset and active-theatre focus input.
- `AWLWorldMap` can now be activated/hidden without destroying it.
- `AWLCampaignHUD` exposes Campaign 3D / Diplomacy controls plus zoom/reset/focus buttons.

## Current Slice Scope

This slice keeps Colombia and Venezuela as the high-detail Campaign 3D theatre.
The rest of North America, Central America, Caribbean and South America appears only as low-detail campaign context: country silhouettes/placeholders, national borders, country labels and simple city markers.

No battle, diplomacy mechanics, country selection, AI, campaign rules, menu flow or save format changes were added in this slice.
