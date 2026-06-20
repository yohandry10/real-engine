# Campaign 3D — Expansión regional escalable

Convertir Campaign 3D de "teatro CO/VE + overview del resto" a un sistema regional
escalable (LatAm, Centroamérica, EEUU) con detalle equivalente, **sin romper CO/VE**.
Primero arquitectura + datos; después expandir por regiones en lotes verificables.

---

## ESTADO (auditoría 2026-06-20)

Leyenda: [x] hecho · [~] parcial · [ ] pendiente

- [x] **Checkpoint seguro.** Estado actual commiteado y pusheado (rama `codex/world-leader-main-menu`, commit `a561396`).
- [~] **Partir WLCampaign3DView.cpp en builders.** Hechos e integrados: `WLCampaignRegionGeometry`, `WLCampaignTerrainBuilder`, `WLCampaignVisualStyle`, `WLCampaignSettlementBuilder`, `WLCampaignRouteBuilder`, `WLCampaignWaterBuilder`, `WLCampaignTerritoryLayerComponent`. **Falta: `WLCampaignCameraRig`** (cámara + LOD siguen dentro de `WLCampaign3DView`).
- [ ] **Modelo de datos único para "teatro detallado".** No hecho. Sigue todo hardcodeado: `FWLCampaignRegionGeometry::IsTheaterIso()` = solo CO/VE, y las ciudades CO/VE están a mano en `BuildCampaignVisualLayer()`. `Provinces.json` solo tiene 5 provincias (VE-ZU, VE-BO, VE-DC, CO-GUA, CO-DC).
- [x] **CO/VE funcionando (referencia de no-regresión).** Se ven y seleccionan. Ver "Receta CO/VE" abajo. **NO TOCAR.**
- [~] **Lote 1: Andes norte (EC/PE/BO).** EN CURSO.
  - [x] **Ecuador**: EC → core (`IsTheaterIso`) ⇒ malla fina + borde dorado + relieve. Label "ECUADOR" + 8 ciudades. **Verificado por el usuario ("se ve perfecto").** Pendiente menor: regiones internas seleccionables + fuerzas para EC.
  - [ ] **Perú (PE)** · [ ] **Bolivia (BO)**. NOTA: PE/BO se extienden al sur (Lat ~-18/-22) y el bounds del teatro hoy corta en `RegionMinLat=-5.2`. Antes de PE/BO hay que **expandir el bounds del teatro hacia el sur** (y revisar encuadre de cámara). Es un paso un poco mayor que Ecuador (que cabía en el bounds actual).
- [x] **Estilos visuales por geografía (v1).** `ClassifyVisualBiome` ahora es continental (cresta de los Andes por latitud) y el terreno **sí pinta** por bioma + relieve: Andes (marrón), costa Pacífico/Caribe (arena), llanos (sabana), Amazonía (verde). Pendiente para el sur: biomas desierto costero (Perú/Atacama), altiplano andino, pampas, Patagonia, Chaco, Atlántico brasileño.
- [ ] Lote 2 Brasil · [ ] Lote 3 Cono Sur · [ ] Lote 4 Centroamérica+México · [ ] Lote 5 Caribe · [ ] Lote 6 EEUU.
- [~] **Labels/selección por zoom.** Estándar país↔ciudad implementado en el overview (Global=países, Region=ciudades; commit `a561396`). Falta escalarlo dentro del teatro y por región nueva.
- [ ] **Unificar LOD/cámara (una sola fuente de verdad).** Pendiente; revisar mezcla Teatro vs Global de TerritoryLayer.
- [ ] **Validación Standalone por lote.** Hasta ahora se valida en PIE (editor). Falta flujo Standalone formal.

## Receta CO/VE (REFERENCIA — NO TOCAR, replicar para nuevos países)

Cómo logran CO/VE su "detalle" hoy (todo en `WLCampaign3DView.cpp`):
1. `IsTheaterIso(Iso)` → CO/VE entran al tier "core": terreno fino (`CellDegrees` 0.040 vs 0.16), borde dorado, estilo de país núcleo.
2. `BuildCampaignVisualLayer()`:
   - `AddTheaterCountryLabel("COLOMBIA"/"VENEZUELA", lon, lat, color)`.
   - ~18 `AddSettlementCluster(id, nombre, ISO, provId, provNombre, lon, lat, tipo, color)` (Capital/LargeCity/Port/Industrial/Frontier) = ciudades físicas + labels seleccionables.
   - Vegetación (`AddVegetationScatter`), parches de bioma (`AddBiomePatch`).
   - `BuildMovementNodesAndEdges()` + `AddMilitaryForceMarkers()` (rutas/fuerzas placeholder).
3. `TerritoryLayer->BuildLayer(...)` → regiones internas seleccionables.
4. Bounds del teatro: Lon [-82.6, -57.0], Lat [-5.2, 17.0] (cubren CO/VE/EC/norte PE/oeste BR).

## Errores / riesgos encontrados

- **CameraRig no extraído**: `ApplyZoomLOD`/cámara siguen en la vista monolítica → toca unificar antes de que cada región meta sus umbrales.
- **Provincias casi vacías**: solo 5 en `Provinces.json`. Las provincias citadas en los settlement clusters (CO-ANT, CO-VAC, etc.) NO existen en el registro; son etiquetas. La selección "rica" real solo aplica a esas 5.
- **Teatro hardcodeado**: añadir país = editar `IsTheaterIso` + más `AddSettlementCluster` a mano (deuda que el plan quería evitar con el "modelo de datos único").
- **Artefactos visuales de overlays hardcodeados**: ya se quitó la línea verde de Brasil (polilínea decorativa a mano); quedan otras (Andes, Patagonia) por revisar.
- **Bug de hit-test bajo letterbox** ya resuelto (cámara 16:9 → offset de Canvas).

## Próximo paso — DECISIÓN PENDIENTE (Lote 1: Ecuador primero)

Dos caminos para que EC/PE/BO se vean como CO/VE:
- **A) Modelo de datos primero (lo que pide el plan):** crear el modelo único (países detallados → ciudades/regiones/biomas en datos), migrar CO/VE a él SIN cambio visual (test de no-regresión), y luego EC/PE/BO = solo datos. Más limpio y escalable; toca CO/VE (riesgo controlado, verificable).
- **B) Ecuador aditivo ya (rápido y seguro para CO/VE):** replicar la receta CO/VE para EC (label + settlement clusters + `IsTheaterIso(EC)` + regiones/fuerzas) sin tocar CO/VE. Progreso visible inmediato; añade hardcodes que luego se migran al modelo.

> Recomendación: **B** para un primer avance visible y seguro de Ecuador, y migrar todo a **A** como paso de arquitectura cuando EC/PE/BO estén validados. Confirmar antes de ejecutar.

---

## Plan original (referencia)

Scope — In: Campaign 3D, datos de países/provincias/ciudades/fuerzas/rutas, LOD/cámara, selección, labels, validación Standalone. Out: diplomacia profunda, economía completa por país, IA militar avanzada, assets finales AAA.

Decisiones (open questions cerradas):
- EEUU: sí, mismo nivel jugable/detallado que CO/VE.
- Caribe completo: sí, administrable/detallado (no solo overview).
- Primer lote tras CO/VE: EC/PE/BO (Andes norte).

Orden de lotes: CO/VE (ref) → **Lote 1 EC/PE/BO** → Lote 2 Brasil → Lote 3 Cono Sur (CL/AR/UY/PY) → Lote 4 Centroamérica+México → Lote 5 Caribe → Lote 6 EEUU.
Cada lote: TerritoryRegions, ciudades, capitales, rutas, selección, fuerzas placeholder, estilos por geografía, y validación (Standalone, [G] America, [F] Teatro, zoom/pan, selección, labels legibles, sin artefactos), cerrado con commit propio.
