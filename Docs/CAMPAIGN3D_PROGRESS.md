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

---

# Fase 2 — Red vial continental + relieve + LOD/labels (EN CURSO)

> Objetivo de esta fase: el mapa deja de ser decorado y se vuelve el **tablero de
> guerra/intriga** (tropas se mueven por carreteras). Ver `ROADMAP.md` (anexo).

## Carreteras (sistema definitivo)
- **Cinta procedural principal** (`FWLCampaignRouteBuilder`, `RoadMesh` + `VertexColorMaterial`):
  asfalto oscuro (lineal ~0.06) + línea central amarilla + arcenes. Se mantiene como base
  continua porque evita los saltos de tiles discretos en curvas/pendiente.
- **Preservar primero lo que ya funciona**: las polilineas CO/VE curadas, el drapeado sobre
  relieve y la colocacion visual actual de carreteras son contrato de compatibilidad para la
  refactorizacion de escala. Cualquier `ProjectDetail`/`DetailRoot` debe reproyectar esas mismas
  rutas lon/lat; no se deben regenerar automaticamente, enderezar, mover a ojo ni reemplazar por
  otra red.
- **Capa de assets de carretera existente**: el codigo carga meshes de
  `Cartoon_City_Free/Meshes/Roads` en `RoadAssetMeshes` y mantiene
  `BuildVenezuelaRoadAssetLayer`/`AddRoadAssetRoute` para colocar tramos por polilinea. Antes de
  tocar escala hay que auditar si esa capa esta activa en Standalone y, si se conserva, mover
  `RoadAssetComponents` junto con `RoadMesh` al espacio de detalle, manteniendo sus rutas,
  rotacion, ZOffset y segmentacion como punto de partida.
- **Tiras SIMPLES sin solape**: añadir overlap longitudinal o discos (caps) coplanares =
  Z-fighting → pista "rota/segmentada". Tiras adyacentes que comparten borde = continua.
- **Densificado a ~0.12° (`DensifyLonLat` en `AddRoute`)**: sin esto, una arista entre
  ciudades lejanas que cruza los Andes es una tira plana que la cresta TAPA → el camino se
  ve "cortado" en el medio. Densificar lo evita (la cinta pega al relieve).
- **Corredores reales como datos**: CO/VE curados en `BuildDefaultTheaterRoutes`; el resto
  vía `BuildIntercityRoads` (MST + links locales) + **cruces fronterizos automáticos**
  (par de ciudades más cercano por país, cap de distancia, chequeo "por tierra" para no
  trazar sobre el mar) + corredores SA en `WLCampaign3DViewSouthAmericaRoadCorridors.inl`.

## Vegetación / relieve de naturaleza
- **Reemplazado el scatter placeholder de conos** por 6 assets Blender unlit vertex-color
  (`gen_nature.py` → `/Game/GenNature`: conifer, broadleaf, palm, rock, peak, mount). Importador
  `import_nature.py`. En C++: un ISM por tipo (`NatureInstances`/`EWLNatureKind`) con `VertexColorMaterial`.
- **`AddVegetationScatter` ahora es biome-driven**: por celda muestrea `ClassifyVisualBiome` → asset por
  bioma, ancla con `VisualBiomeZOffset` (misma superficie que el terreno), enmascara mar y ciudades.
  Sigue solo en CO/VE (las 4 zonas de la vertical slice); extender a más países cuando se curen.

## Ciudades / geografía
- **Venezuela completa**: ~28 ciudades (capitales de estado + nodos de Troncal) y red vial
  completa hasta la salida a Brasil (Santa Elena→Pacaraima→Boa Vista).
- **Colombia**: eje andino (Panamericana) + caribeño + relleno oriente/sur/Pacífico +
  cruces CO-VE oriental y CO-EC. Guyana enganchada vía Brasil (Lethem/Manaus).
- **Cuenca de Maracaibo tallada** en `SampleTerrainHeight` (el modelo de Andes de cresta
  única metía montaña donde hay tierra baja); lago reformado a ras; ciudad en ribera oeste.
- **Huella de ciudad compactada** (`AddSettlement` Scale ×0.5 + `BuildMeshCity` radio/celda)
  para que ciudades a 50-80 km no se solapen.
- **Exageración de relieve 19000→12000**; **Andes sin franja pálida** (se quitó el realce de
  color por altura en `ShadeTerrainVertex`; antes el Andes alto salía casi blanco).

## LOD / labels (estándar nuevo)
- **Zoom tope 1200k** (`CampaignMaxCameraHeight`; antes 4200k). 4 LOD: Global/Region/Teatro/Cercano.
- **Carreteras** solo en Teatro/Cercano (`bFineDetail`), NO en Region (eran ruido al alejar).
- **Frontera (`BoundaryMesh`)** solo continental (`CameraHeight >= 130000`), oculta de cerca
  (70/97k traza toda la costa recortada = ruido).
- **Nombres de país (overview)**: solo GRANDES de tierra firme en Region/Global
  (`bGlobalPriority && !bCaribbean && !bSpecialTerritory`); islas/Caribe/territorios
  (Trinidad, Guyana Francesa…) fuera del overview (su nombre se salía del territorio).
  Tamaños reducidos (CO/VE 22000→10000).
- **Ciudades (mallas + nombres)** solo `CameraHeight <= 200000` (`bCityDetail`): nada de
  ciudades a 247k (vista regional = terreno + carreteras + fronteras).

## Checkpoint de diseño: escala de ciudades, LOD y crecimiento
Contexto del usuario (2026-06-22): las ciudades se ven demasiado pequeñas en el
mapa expandido, especialmente en 170k-250k. El juego necesita que una ciudad sea
legible como ficha jugable y que, con economia/infraestructura, pueda crecer hasta
sentirse como una polis. A la vez, San Antonio, Cucuta y San Cristobal tienen
distancias reales cortas (aprox. 11-33 km entre algunos pares), asi que agrandar
todo sin control volveria a pegarlas.

Respuesta tecnica consolidada:
- No se debe resolver haciendo todos los meshes de ciudad gigantes ni quitando
  zooms intermedios. El patron estandar de alto nivel es LOD/HLOD: a distancia se
  reemplaza el detalle por proxies/simbolos simplificados, y el detalle fisico se
  muestra solo de cerca.
- Las ciudades estan pequenas como elemento jugable, pero la huella urbana real no
  debe crecer brutalmente. El simbolo de lectura puede ser mas grande que la huella
  real; la huella y los edificios deben respetar distancia a ciudades vecinas.
- Separar tres capas:
  1. **Simbolo jugable de ciudad**: visible desde mas lejos; resuelve lectura y click.
  2. **Huella urbana real**: mas contenida, pegada al terreno, crece con economia,
     poblacion e infraestructura, con limite por distancia a ciudades cercanas.
  3. **Mesh detallado**: edificios/calles/relleno urbano; solo en Close/cercano.
- Decision propuesta para implementar:
  - `>360k`: estrategico limpio, sin carreteras detalladas ni ciudades.
  - `320k-220k`: rutas + simbolos claros de ciudad, sin meshes pesados.
  - `220k-120k`: huella urbana visible y labels legibles.
  - `<120k`: edificios/detalle.
- El crecimiento economico debe modificar huella/edificios/densidad, no el tamano
  basico del simbolo de lectura.
- No ensanchar carreteras por ahora; el problema principal observado era que las
  carreteras aparecian solas mientras las ciudades seguian ocultas o minusculas.

Criterios aceptables despues de implementar:
- 550k/450k/375k: no ver carreteras detalladas solas.
- 308k/250k: ver ciudades como nodos claros junto a carreteras.
- 170k/139k: ciudades legibles, sin duplicados ni labels gigantes.
- 114k: ver huella/edificios y que Merida siga leyendo como zona andina.
- San Antonio/Cucuta/San Cristobal separados, no amontonados.
- Maracaibo en cuenca baja, no visualmente pegada a la montana andina.

Referencias de criterio:
- Epic Games: Hierarchical Level of Detail (HLOD)
  https://dev.epicgames.com/documentation/unreal-engine/hierarchical-level-of-detail-in-unreal-engine?lang=en-US
- Epic Games: Static Mesh Automatic LOD Generation
  https://dev.epicgames.com/documentation/unreal-engine/static-mesh-automatic-lod-generation-in-unreal-engine?lang=en-US

## Biblioteca externa de ciudades
- Se creo `ExternalAssets/CityKits/` como biblioteca de evaluacion previa a
  importacion Unreal. No todo lo descargado debe entrar automaticamente en `Content/`.
- Kenney City Kits: CC0, utiles como piezas ligeras modulares.
- Quaternius Downtown City MegaKit Standard: CC0, mas cercano al lenguaje moderno/
  futurista deseado. El ZIP original queda local sin trackear porque supera 100 MB;
  los archivos extraidos quedan disponibles para evaluacion.
- Fab `City` de SpatialNeglect: descargado desde la biblioteca Fab del usuario y
  copiado desde VaultCache a `ExternalAssets/CityKits/Fab/City-SpatialNeglect`.
  Contiene un `City.fbx`, 18 texturas PNG, 6 grupos de material y metadata Fab que
  declara 72,100 tris. No trae `License.txt`; conservar metadata/origen de Fab.
- Proxima prueba correcta: importar el Fab City en carpeta aislada
  `Content/ExternalTests/FabCity_SpatialNeglect`, revisar escala, materiales, pivot
  y rendimiento, y solo despues conectarlo al sistema de ciudades.

## Líos por agente paralelo (codex) — resueltos
- Colisiones de **unity build**: helpers duplicados en anonymous-namespace de dos `.cpp`
  (`ScaleToBounds`/`MakeBottomAnchoredTransform` en Visual.cpp y Roads.cpp) → renombrar uno.
- Menú frontend: `WLText`/`WLError` definidos dos veces (header + .cpp) → de-duplicar.
- `RouteBuilder`: su overlap/caps rompieron la continuidad de la cinta → revertido.

## Pendiente (Fase 2)
- **ScaleAudit antes de tocar proyeccion**: registrar distancias km/UU, ancho de carreteras,
  radios de seleccion, labels visibles, secciones de `RoadMesh`, cantidad de `RoadAssetComponents`
  y capturas 42k/76k/200k/620k. La primera regla es no romper Maracaibo bajo, Merida/San Cristobal
  andinos ni las carreteras curadas existentes.
- **Ecuador / Perú "limpios"** (corredores curados como CO/VE) + resto del continente.
- Convertir la red en **grafo de pathfinding** de tropas (ROADMAP anexo).
- **Fuente** de etiquetas (asignar asset a `UTextRenderComponent`; hoy default Roboto).
- **Nombre de país-isla al acercar** (hoy solo se ocultan del overview; falta mostrarlo en detalle).
