# Campaign 3D — Ciudades 3D generadas en Blender (pipeline, estado y errores)

> Doc para cualquier agente (Claude/Codex) que continúe las **ciudades** de Campaign 3D.
> Explica el estado actual, el pipeline EXACTO con Blender (reproducible), la integración en
> código y TODOS los muros con los que chocamos y cómo se resolvieron. Léelo entero antes de
> tocar `gen_city.py`, `import_gencity.py` o `BuildMeshCity`.
> Complementa `Docs/AGENT_NOTES.md` (§2 lit/unlit) y `Docs/CAMPAIGN3D_PROGRESS.md`.

Última actualización: 2026-06-23.

---

## 1. Enfoque actual (qué es una "ciudad" hoy)

Cada asentamiento se dibuja como **UN modelo 3D cohesivo de ciudad** (no edificios sueltos, no
una sola caja), generado **proceduralmente en Blender** y colocado como una sola pieza por
ciudad, **estilo "settlement" de Total War**.

- Modelo = cuadrícula de manzanas + calles + aceras + edificios (3 tipos) + azoteas + árboles +
  coches + líneas de carril + cimiento/pódium que se entierra en el terreno.
- 3 tamaños: `city_large` (capital), `city_medium` (ciudad grande/puerto/industrial),
  `city_small` (frontera).
- **UNLIT con vertex color** (igual que el terreno) — ver §5, error #4. NO materiales lit.

### Estado / parámetros vigentes
- Blender: **4.2.9 LTS portable** en `C:\Users\PC\Blender42\blender-4.2.9-windows-x64\blender.exe`
  (fuera del repo, no versionado).
- Generador: `gen_city.py` (raíz del repo). Importador: `import_gencity.py` (raíz).
- FBX intermedios: `ExternalAssets/Generated/city_{large,medium,small}.fbx` (no versionar pesados).
- Assets UE: `/Game/GenCity/city_{large,medium,small}` (Content/GenCity/). Material en UE =
  `VertexColorMaterial` (`/Engine/EngineDebugMaterials/VertexColorMaterial`, UNLIT) asignado por código.
- `gen_city.py` (**reescrito 2026-06-23 para legibilidad**, ver §5 #11): export `colors_type='LINEAR'`;
  shade falso por cara `SHADES=[0.55,1.12,0.82,1.00,0.90,0.70]` (orden bottom,top,-Y,+X,+Y,-X).
  Paleta **clara con CONTRASTE** = lo que hace que LEA como ciudad: acera clara `C_SIDE 0.66` vs
  asfalto oscuro `C_STREET 0.27` → la cuadrícula de calles se ve; edificios concreto claro 0.55–0.67 +
  cristal claro 0.43–0.61, azoteas terracota en residencial. **Layout:** manzana 26 / calle 10; edificios
  con HUECO de acera (footprint 0.58–0.74 de celda, no 0.95); altura por falloff fuerte (borde bajo, núcleo
  alto: `HMAX` large **88**/medium 66/small 32; antes 135/72/36); marcas de carril + pasos de cebra en cada
  cruce; verde repartido (`PARK_P` ~0.13) + arbolado de calle; **apron de pasto** alrededor; landmark central
  solo en la capital.
- **Preview sin UE:** `preview_city.py` (raíz) renderiza un FBX con Workbench FLAT + color VERTEX
  (= unlit vertex color, igual que UE) a PNG top-down + oblicuo. **Verifica el aspecto ANTES** del reimport:
  `blender --background --python preview_city.py -- <city.fbx> <prefijo>` → `<prefijo>_top.png`/`_obl.png`.
- `BuildMeshCity` (`WLCampaign3DViewVisual.cpp`): `TargetWidth` por tipo (Capital 11500, LargeCity
  7200, Port 5800, Industrial 6600, Frontier 4400 u); escala uniforme XY + **Z ×1.4** (skyline sin
  tapar el suelo); ancla el ORIGEN del modelo (z=0=calle) a `ProjectLonLat+20` (el pódium se entierra);
  asigna `VertexColorMaterial` a todos los slots.
- Nombres de ciudad (`AddSettlementCluster`): Z elevado por tipo (Capital 8800, LargeCity 4800,
  Port 4000, Industrial 4400, Frontier 2200 u sobre el terreno) para que floten SOBRE los edificios.
- Iluminación (`WLCampaign3DViewScene.cpp`): SkyLight 2.9 / DirLight 4.6. (Con ciudades unlit la luz
  ya casi no afecta a la ciudad; se mantiene suave por el resto.)

---

## 2. Pipeline EXACTO (reproducible, headless, SIN MCP)

> No se usa el "MCP de Blender". Se controla Blender con **scripts `bpy` headless**. Es lo mismo que
> haría el MCP (Blender ejecuta el código) pero reproducible y sin reconfigurar la sesión.

**Ciclo de iteración de una ciudad:**

1. Editar `gen_city.py` (geometría/colores/tamaños).
2. **Regenerar** los 3 FBX (Blender headless):
   ```bash
   BL="/c/Users/PC/Blender42/blender-4.2.9-windows-x64/blender.exe"
   OUT="C:/Users/PC/Desktop/rome-actual/ExternalAssets/Generated"
   "$BL" --background --python gen_city.py -- "$OUT/city_large.fbx"  large  7
   "$BL" --background --python gen_city.py -- "$OUT/city_medium.fbx" medium 13
   "$BL" --background --python gen_city.py -- "$OUT/city_small.fbx"  small  21
   ```
   Args tras `--`: `<salida.fbx> <large|medium|small> <seed>`. Éxito = línea `WL_GENCITY_DONE: ... polys=N`.
3. **Cerrar el editor** (`Stop-Process -Name UnrealEditor -Force`) — necesario para reimport y compilar.
4. **Reimportar** a UE (headless; requiere `PythonScriptPlugin`, ya habilitado en el `.uproject`):
   ```bash
   "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
     "C:/Users/PC/Desktop/rome-actual/WorldLeader.uproject" \
     -ExecutePythonScript="C:/Users/PC/Desktop/rome-actual/import_gencity.py" \
     -unattended -nosplash -nopause
   ```
   OJO: el stdout solo trae el preámbulo de UBT; el log real (`LogPython`, `WL_GENCITY_*`) está en
   `Saved/Logs/WorldLeader.log`. Verificación rápida: mtime/tamaño de `Content/GenCity/city_*.uasset`.
5. **Compilar SOLO si tocaste C++** (`BuildMeshCity`, etc.). Si solo cambiaste `gen_city.py`/colores,
   **no hace falta compilar** — reimportar basta. Build:
   ```bash
   "/c/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" WorldLeaderEditor Win64 \
     Development -project="C:/Users/PC/Desktop/rome-actual/WorldLeader.uproject" -waitmutex
   ```
6. **Verificar**: en Standalone (regla del repo). El usuario suele abrir él mismo el editor/PIE.

**Import (`import_gencity.py`) — claves:** `import_materials=False`, `import_textures=False`,
`combine_meshes=True`, `vertex_color_import_option=REPLACE`. Importar SIN materiales (los crea el
código con `VertexColorMaterial`) y CONSERVANDO vertex colors.

---

## 3. Cómo está hecho `gen_city.py` (para extenderlo)

- `bmesh` puro: `add_box(cx,cy,z0,sx,sy,sz, col)` crea una caja y pinta sus 6 caras con
  `col × SHADES[cara]` en una capa de vertex color (`bm.loops.layers.color`). **Todo el detalle son
  cajas** (rápido, robusto, 1 sola malla).
- Origen del modelo: `z=0` = nivel de calle. El **pódium** baja a `z=-60` (se entierra en UE).
- Layout: cuadrícula `N×N` manzanas (`large` N=8, `medium` 5, `small` 4); cada manzana = acera +
  3×3 lotes de edificios pegados; parques con árboles; avenidas centrales con línea amarilla y coches;
  **landmark** central en `large`.
- `building(kind)`: `tower` (cristal + mullions = cruz vertical de pared para simular ventanas),
  `podium` (base ancha + torre), `resid` (bajo, ventanas chicas). Azoteas: parapeto, tanques,
  helipuerto/antena.
- Altura: `HMAX` por tamaño (large **92**, medium 72, small 36; large **bajó de 135** para no hacer
  "agujas" — ver §5 #9), decae hacia el borde (`dist**1.4`) → skyline (centro alto). Landmark central
  ×1.3 (antes ×1.7); pico ocasional cerca del centro ×1.45 (antes ×1.7).
- **Apron de pasto** (`C_GRASS`, box `span+street+44`, top z≈0.3 bajo el nivel de calle): anillo verde
  alrededor de la ciudad para fundirla con el terreno en vez de cortar con un borde de losa duro.
  Ensancha el bounds → la ciudad encoge ~12-15% al mismo `TargetWidth` (beneficioso: menos solape).
- Export: `colors_type='LINEAR'` (coincide con cómo el proyecto usa vertex color; con `'SRGB'` salía
  lavado/blanco).

Para subir realismo (siguiente): texturas reales de fachada (UV+textura, no solo vertex color),
variación por seed entre ciudades del mismo tipo, muros/río/puerto por tipo.

---

## 4. Integración en C++ (qué tocar)

- `AWLCampaign3DView::AWLCampaign3DView()` (`WLCampaign3DView.cpp`): carga los 3 modelos con
  `FObjectFinder` a `CityModelLarge/Medium/Small` (cook garantizado por referencia dura).
- `AddSettlementCluster` (`WLCampaign3DViewVisual.cpp`): si `CityModelLarge` existe → `BuildMeshCity`
  para TODA ciudad; si no, cae al footprint procedural (`FWLCampaignSettlementBuilder::AddSettlement`).
  También coloca el label (nombre) elevado por tipo.
- `BuildMeshCity`: elige modelo+`TargetWidth` por tipo, escala uniforme XY (+Z×1.4), ancla a
  `ProjectLonLat+20`, asigna `VertexColorMaterial`, añade el componente a `CityBuildingComponents`
  (lo gestiona el LOD en `WLCampaign3DViewLOD.cpp`, `bCityDetail`).

---

## 5. Errores encontrados y solución (cronológico — NO repetir)

1. **Una sola malla "London White City" (80 MB) por ciudad.** Salía un bloque diminuto, translúcido
   (vidrios), fuera de sitio (pivot/escala) y pesadísimo. → Descartado. No usar un asset de ciudad
   real gigante encogido por punto.
2. **Edificios sueltos esparcidos (grid de mallas de packs Kenney/CitySpatialNeglect).** "No es una
   ciudad, son edificios dispersos." → Descartado. La ciudad debe ser UN modelo cohesivo.
3. **Importé props que NO son edificios** (Kenney): chimeneas (cilindro), `detail-tank`, toldos
   (parecían bancos), vallas, caminos, árboles → ensuciaban. Lección: al importar packs, FILTRAR solo
   `building*`. (Hoy ya no usamos packs para la ciudad, sino el generador propio.)
4. **CIUDADES OSCURAS (causa raíz).** Todo el mapa es **UNLIT** (`VertexColorMaterial`); las ciudades
   importadas eran **LIT** (Principled del FBX) → recibían sombreado/sombra de montaña → oscuras e
   incoherentes. Subir SkyLight NO lo arregla; `metallic` lo empeora (refleja cubemap tenue); la
   emisión del cristal no se exporta por FBX. **Solución:** ciudades **unlit con vertex color** (color
   × shade falso por cara) + asignar `VertexColorMaterial` en código. (Generaliza `AGENT_NOTES.md §2`.)
5. **CIUDADES BLANCAS.** Tras pasar a vertex color, los colores base eran muy claros (0.85) y el shade
   del techo (×1.18) saturaba a blanco; exportar en `SRGB` lo aclaraba más. **Solución:** colores de
   **tono medio** (0.16–0.52) + export `LINEAR` + shade del techo más suave (1.12).
6. **TORRES FLACAS / no se ve el suelo ni los nombres.** Exagerar Z ×2.4 hizo edificios altísimos y
   delgados que tapaban el piso y enterraban los labels. **Solución:** Z **×1.4**; aclarar el asfalto
   (0.16→0.32) para que el piso se vea; **elevar los labels por tipo** (8800/4800/… u) sobre los edificios.
7. **Ciudades "cortadas"/flotando sobre el relieve.** Base plana fina. **Solución:** PÓDIUM grueso
   (z=-60) que se entierra anclando el origen (z=0=calle) al terreno → no flota en pendiente.
8. **Demasiado anchas (parecían "alfombra").** Caracas a 15000 u ≈ 47 km. **Solución:** compactar
   `TargetWidth` (Caracas 11500) + altura (skyline) para que se lean como ciudad, no mancha.
9. **CAPITAL = bosque de agujas (Caracas "sin piso", sin calles).** El preset `large` (HMAX 135 +
   landmark ×1.7) tras escala (~35-40×) y Z×1.4 daba torres ~7500u y landmark ~12800u sobre una base
   de 11500u → **más altas que ancha la ciudad**: tapaban suelo, calles y parques (Valle de la Pascua
   = medium HMAX 72 sí se veía bien). **Solución (gen_city):** `large` HMAX **92**, landmark **×1.3**,
   pico ocasional **×1.45**; ahora la capital se lee como ciudad con más manzanas (N=8) + **apron de
   pasto** alrededor. Regenerar los 3 FBX + reimportar (la escala se recalcula sola por bounds).
11. **El relieve se "come"/inclina la ciudad (CAUSA REAL del "no se ve plano").** La ciudad es UN mesh
   rígido y plano anclado en UN punto (altura del centro), pero el terreno se malla por-vértice con
   `SampleTerrainHeight`, así que sube/baja sobre la huella. Con `DetailWorldUnitsPerKm=320` y
   `VerticalDetailExaggeration=4` el terreno es `H·48000`; **solo el ruido** varía ~0.055 en H sobre la
   huella de la capital (~0.32°) = **~2600u de desnivel**, mientras los edificios de borde (bajos) miden
   ~300–500u → el terreno los entierra/inclina. El pódium solo baja, no tapa terreno que sube por encima.
   **Solución (código):** **flat pads** — discos donde `SampleTerrainHeight` aplana el terreno a la altura
   NATURAL del centro de cada ciudad. Pre-pasada `bCollectFlatPadsOnly` que corre `BuildCampaignVisualLayer`
   en modo "solo pads" (registra `CityFlatPads` vía `RegisterCityFlatPad`, no construye nada) ANTES de
   `BuildTerrain`. `SampleTerrainHeightRaw` = relieve natural; `SampleTerrainHeight` = raw + mezcla al pad.
   El relieve de alrededor (Andes/Mérida) se conserva fuera del pad; la ciudad NO se mueve (el pad usa la
   altura del centro), el terreno sube/baja para encontrarla. Radio del pad = media huella (`TargetWidth`)
   +35%, rampa suave ×1.8. Para excluir una ciudad del aplanado: no registrar su pad.
12. **EL PISO DE LA CIUDAD NO SE VE / "beige" entre los edificios (CAUSA REAL, costó ~10 rondas; la
   encontró una sesión Codex en paralelo).** El terreno NO se dibuja en `ProjectLonLat.Z`: el
   `WLCampaignTerrainBuilder` suma un `VisualBiomeZOffset` POR BIOMA a cada vértice (`A.Z += ZOffset`,
   ~línea 117). Offsets core (`WLCampaignVisualStyle::VisualBiomeZOffset`): **Costa +95, Llanos +135,
   Montaña +560, Urbano +210, default +165; no-core -120**. La superficie VISIBLE = `ProjectLonLat.Z +
   offset`. `BuildMeshCity` anclaba la ciudad en `ProjectLonLat + 20` (sin el offset) → el mesh del
   terreno quedaba ENCIMA y tapaba calles/aceras/parques (solo asomaban los edificios altos; lo "beige"
   entre edificios era el terreno, no el piso de la ciudad). **Solución:** anclar en `ProjectLonLat +
   VisualBiomeZOffset(ClassifyVisualBiome(Lon,Lat,bCore), bCore)` con `bCore = IsTheaterIso(CountryIso)`
   (misma fuente que el terreno, `Country.bCoreCountry`). LECCIÓN: ciudad plana "sin piso" = problema de
   Z/vertical (terreno sobre el piso), NO cámara/ángulo/zoom/altura de edificios. Mirar la composición Z
   real del terreno PRIMERO. Ver memoria [[campaign3d-terrain-visual-zoffset]].
10. **Marcadores de fuerza sobre las ciudades (triángulos amarillos + nombre duplicado).** Las fuerzas
   militares (placeholder, sin gameplay) ponían un **cono dorado** (`/Engine/BasicShapes/Cone`) + una
   etiqueta `nearby_city` que **repetía el nombre de la ciudad** (2º "Caracas"). **Solución (código, NO
   gen_city):** `bShowMilitaryForceMarkers=false` en `WLCampaign3DView.h`; `AddMilitaryForceMarker`
   registra los datos en `ForceViews` (paneles/reactivación) pero **no crea cono/etiqueta/hitbox** (el
   hitbox además se tragaba el click de la ciudad). Reactivar con `true` cuando las fuerzas sean jugables.
13. **CIUDAD = REJÍCULA DE CRUCES BLANCAS (parecía wireframe roto).** `gen_city.py` dibujaba pasos de
   cebra (`C_CROSS`) de `block*0.5` (= MEDIA manzana, ~13u) y muy brillantes (0.78) en CADA intersección,
   + aceras casi blancas (`C_SIDE 0.66`), + edificios chicos y separados (footprint 0.58–0.74, falloff
   fuerte). Resultado: una retícula blanca dominaba sobre clusters dispersos de edificios = "graph paper",
   no ciudad. **Solución (SOLO gen_city, NO tocar el puerto que es C++ `AddPortFacility`/`*Instances`):**
   crosswalks PEQUEÑOS contenidos en el ancho de calle (`street*0.62` largo, `C_CROSS 0.52`); acera a tono
   medio (`C_SIDE 0.45`); línea de carril algo más tenue (`C_LINE 0.74`); manzana DENSA (footprint
   0.78–0.94, falloff suave `dist*1.1**1.8`, piso de altura 0.22·HMAX); calles más finas (block 28/street 8).
   Regenerar las 9 variantes + reimportar (sin compilar; es solo el modelo). Verificado con `preview_city.py`.
14. **CIUDADES-PUERTO se veían "rotas"/diminutas (Valparaíso, Mar del Plata).** El MODELO estaba bien
   (se ve perfecto en `preview_city.py` oblicuo/steep). El problema era de ESCALA: `Port` usaba
   `TargetWidth=5800` (apenas > Frontier 4400), así que estas ciudades MAYORES salían minúsculas al
   lado de las capitales (17000) y leían como un tablero plano. Confirmado que NO era bug del sur ni de
   geometría: Puerto Cabello (teatro core) renderiza idéntico a Valparaíso. **Solución (SOLO ciudad, NO
   tocar `AddPortFacility`/`PortInstances` = el muelle):** subir `Port TargetWidth` 5800→**8000** en los
   **3 sitios que DEBEN ir en sync**: `BuildMeshCity` (WLCampaign3DViewVisual.cpp), `RegisterCityFlatPad`
   (WLCampaign3DViewGeometry.cpp) y la llamada a `NudgePortSettlementFootprintToLand` (Visual.cpp). El
   nudge siguió dando `land=81/81` (ciudad entera en tierra). Pista de diagnóstico: la cámara de captura
   de `StandalonePortScreenshot` era **cenital pura** (`-90°`) y ocultaba escala/skyline; se pasó a
   **oblicua ~52°** para validar como lo ve el jugador. Es C++ → recompilar (no hace falta reimportar).

---

## 6. Reglas de oro para el siguiente agente

- **NUNCA materiales LIT en assets sobre este mapa unlit** → salen oscuros. Usa vertex color + shade.
- **Exporta vertex color en `LINEAR`**, no SRGB. Colores base en tono medio (no >0.6, satura a blanco).
- **Una ciudad = un modelo cohesivo** (no piezas sueltas, no caja única gigante).
- **Itera barato:** cambios solo de `gen_city.py`/colores → regenerar + reimportar, **sin compilar**.
  Solo compila si tocaste C++.
- **Cierra el editor** antes de reimportar/compilar (lock). El usuario abre/verifica él mismo (Standalone).
- **No subir/medir alturas sin subir también los labels** (quedan enterrados) ni sin ver el suelo.
- Mantén el peso: todo son cajas, 1 malla por ciudad (large ~55k polys, ligero para hardware bajo).
