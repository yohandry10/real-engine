# Campaign 3D — Registro de avance (Fase 1: Expansión regional) — COMPLETADA

> Estado: **HECHO**. El teatro detallado de Campaign 3D cubre todo el hemisferio
> (Canadá → Tierra del Fuego). El plan de la fase siguiente vive en
> `docs/CAMPAIGN3D_PLAN.md`.

## Objetivo logrado
Pasar de "teatro CO/VE + overview del resto" a un **sistema continental** donde
cada país se renderiza con terreno por biomas, contorno limpio, ciudades sobre
tierra, labels por zoom y cámara fluida — **sin romper CO/VE** (referencia).

## Cobertura (todos los lotes cerrados)
- **Referencia**: Colombia, Venezuela.
- **Lote 1 Andes norte**: Ecuador, Perú, Bolivia.
- **Sudamérica**: Brasil, Argentina, Chile, Uruguay, Paraguay, Guyana, Surinam.
- **Lote 4**: México + Centroamérica (Guatemala, Belice, Honduras, El Salvador, Nicaragua, Costa Rica, Panamá).
- **Lote 5 Caribe**: Cuba, Jamaica, Haití, R. Dominicana, Puerto Rico, Bahamas, Trinidad.
- **Lote 6**: Estados Unidos. **+ Canadá.**

## Sistemas construidos
- **Clasificador de biomas continental** (`WLCampaignVisualStyle::ClassifyVisualBiome`):
  biomas por **geografía/ubicación** (no por país). Andes (cresta por latitud), costa
  Pacífico/Caribe, llanos, Amazonía, altiplano/Patagonia/Atacama/bosque valdiviano,
  y un bloque unificado de **Norteamérica** (bosque este, Grandes Llanuras, Rocosas,
  desierto SO, costa oeste/Pacífico NW) + istmo tropical de Centroamérica.
- **Terreno con celdas adaptativas** (`WLCampaignTerrainBuilder`): países enormes
  (>25°) usan malla 0.10 para no disparar vértices; los chicos 0.040.
- **Una sola línea de contorno** (se eliminó el borde nacional duplicado del
  TerritoryLayer; el `BoundaryMesh` del terreno, recortado a la región, es la única).
- **Empuje costero automático** de ciudades (`NudgeSettlementToLand`, 8 direcciones):
  los marcadores costeros se desplazan a tierra; no mueve capitales en islas chicas.
- **Estándar de labels por zoom**: al **alejar** (Global/Region) solo **países**; al
  **acercar** (Teatro/Cercano) aparecen **ciudades**. Arrays separados `Labels`
  (país) vs `SettlementLabels` (ciudad); labels de ciudad del overview desactivadas.
- **Labels orientados y escalados**: rotación corregida (90,180,0) y tamaño por país.
- **Cámara**: edge-pan rápido + arrastre con click derecho (agarrar mapa). LOD de
  zoom: Global / Region / Teatro / Cercano.
- **Fix de hit-test bajo letterbox** (cámara 16:9 → offset de Canvas) para los
  botones del HUD.

## Cómo se añade un país (estándar actual)
1. `FWLCampaignRegionGeometry::IsTheaterIso()` += ISO (lo vuelve "core": malla fina,
   borde dorado, biomas reales). Si no estaba, añadir también a `IsContextIso`.
2. Expandir bounds en `WLCampaign3DView.h` (`RegionMin/MaxLon/Lat`) si el país queda fuera.
3. Si su geografía es nueva, extender `ClassifyVisualBiome` (por zona lon/lat, no por país).
4. En `BuildCampaignVisualLayer()`: `AddTheaterCountryLabel(nombre, lon, lat, color, size)`
   + `AddSettlementCluster(...)` por ciudad (Capital/LargeCity/Port/Industrial/Frontier).

## Deuda técnica / pendientes (heredados)
- **CameraRig no extraído**: `ApplyZoomLOD`/cámara siguen dentro de `WLCampaign3DView`;
  falta una **única fuente de verdad de LOD**.
- **Modelo de datos hardcodeado**: países y ciudades están en código
  (`IsTheaterIso` + `AddSettlementCluster`), no en datos. Migrar a data-driven.
- **`Provinces.json` casi vacío** (5 provincias): la selección "rica" solo aplica a esas.
- **Validación Standalone** formal pendiente (hasta ahora PIE/editor).
- Quedan overlays decorativos hardcodeados menores (Andes/Patagonia) por revisar.
