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

Changed:
- `AWLCampaignGameMode` now orchestrates both presentation layers.
- `AWLCampaignPlayerController` owns view switching and routes selection by active view.
- `AWLWorldMap` can now be activated/hidden without destroying it.
- `AWLCampaignHUD` exposes compact Campaign 3D / Diplomacy controls.

## Current Slice Scope

This slice intentionally covers only Colombia and Venezuela in Campaign 3D.
Neighboring geography is contextual only. The existing America political map remains the source for Diplomacy.

No battle, diplomacy mechanics, AI, campaign rules, or save format changes were added in this slice.
