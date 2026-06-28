# Campaign 3D — Ejércitos "stack" (1 ficha = N NPCs) — Plan completo por fases

> Objetivo: que UNA ficha en el mapa de campaña (un general/bandera) represente y DIBUJE una fuerza de
> muchos NPCs (p.ej. 20 tanques, 200 soldados, 200 aviones), estilo Total War: Rome 2.
> Este doc reúne **lo que YA tenemos**, **las decisiones abiertas** y el **plan por fases** (detallado,
> con archivos/funciones reales y cómo verificar cada fase).

Última actualización: 2026-06-27.

---

## 0. Glosario (cómo se llama esto)

- **Force / Army (ejército)** = la ficha del mapa. En estrategia se le dice **"stack"** (pila de unidades).
- **Composition** = las unidades del ejército (p.ej. 20 MBT + 200 infantería).
- **Unit** = un tipo de unidad; en batalla son muchos **entities** (soldados/vehículos individuales).
- **Instancing (GPU instanced rendering)** = dibujar el mismo modelo cientos de veces casi gratis. Es lo
  que hace que "un modelo represente a muchos". `UInstancedStaticMeshComponent` (ISM) en UE.
- **Banner/marker** = el estandarte/ficha clicable que marca el ejército en el mapa (la bandera de Rome).

---

## 1. LO QUE YA TENEMOS

### 1.1 Assets de unidad (modelos)
Pipeline propio en `gen_vehicle.py` → `/Game/GenVehicle/`, **UNLIT vertex-color** (LINEAR), 1 malla, miran
a +X, **1 unidad ≈ 1 m** (escalas coherentes entre tipos). Muy bajo poly (ideal para instanciar a cientos):

| Asset | Tipo | Polys | Estilo |
|---|---|---|---|
| `veh_mbt` | Tanque (Abrams/Leopard) | ~700 | unlit vertex-color (estático) |
| `veh_ifv` | Vehículo de combate de infantería | ~470 | unlit vertex-color |
| `veh_apc` | Transporte blindado 8x8 | ~380 | unlit vertex-color |
| `veh_aircraft` | Caza (delta) | ~150 | unlit vertex-color |
| `veh_ship` | Fragata/destructor | ~90 | unlit vertex-color |
| `veh_soldier` | Infantería (estático) | ~370 | unlit vertex-color (instanciable) |
| `SK_Quaternius_Swat` | Infantería SWAT **rigged** | — | **LIT/PBR + esqueleto + ~25 animaciones** (Idle, Walk, Run, Gun_Shoot, Death…) en `/Game/GenVehicle/Quaternius/` |

> Hay **dos soldados a propósito**: `veh_soldier` (estático, barato, para la MASA instanciada) y
> `SK_Quaternius_Swat` (animado, bonito, para PRIMER PLANO / pocos "héroes"). Ver decisión §2.

Iterar/regenerar un asset propio:
`blender --background --python gen_vehicle.py -- "<out.fbx>" <mbt|ifv|apc|soldier|aircraft|ship> <seed> [camo]`
· preview unlit: `preview_vehicle.py` · reimport: `import_genvehicle.py`.

### 1.2 Sistema de fuerzas (YA EXISTE — no reinventar)
- `FWLCampaign3DForceView` (`WLCampaign3DView.h:85`): id, nombre, país, `ForceType`, `MarkerCategory`
  (land/air/naval), posición `Lon/Lat/WorldLocation`, `EstimatedStrength` (un solo int), `Posture`,
  `Supply`, `Morale`, `bAir/bNaval/bMovable`. **Falta `Composition`** (ver Fase 1).
- Carga desde `Content/Data/Campaign3D/MilitaryForces.json` (`WLCampaign3DViewForces.cpp:155`).
- **Movimiento por nodos** (ya funciona): `UpdateForceMovementLocation`, `GetValidMovementDestinations`,
  `FindNearestMovementNodeId`, `GetForceMarkerLocationForNode`, `FWLCampaign3DMovementNodeView`.
- **Selección/click** (ya funciona): `TryGetForceForComponent`, `TryGetForceById`,
  `TryGetForceNearWorldLocation`.
- **Marker hoy DESACTIVADO**: `AddMilitaryForceMarker` dibuja un cono dorado; `bShowMilitaryForceMarkers
  = false` (`WLCampaign3DView.h:259`). Es el punto de enganche del visual nuevo (ver `Docs/CAMPAIGN3D_
  CITY_BLENDER.md` §5 #10: el cono + etiqueta duplicaban el nombre de la ciudad; cuidar eso).

### 1.3 Infraestructura de render reutilizable
- **Instancing**: `UInstancedStaticMeshComponent` ya usado (`SettlementBlockInstances`,
  `SettlementTowerInstances`, `PortInstances`); helpers `AddInstance(Comp, World, Rot, Scale)` y
  `ConfigureInstancedComponent(Comp, Mesh, Color)` (`WLCampaign3DView.cpp:110-181`).
- **LOD por zoom**: `WLCampaign3DViewLOD.cpp` con `bCityDetail` y `Comp->SetVisibility(bCityDetail)` —
  alterna detalle según altura de cámara. Reusar para "icono lejos / formación cerca".
- **Anclaje Z correcto (CLAVE)**: el terreno se dibuja en `ProjectLonLat.Z + VisualBiomeZOffset(bioma)`.
  Toda malla rígida sobre el mapa debe anclarse con el MISMO offset o se hunde/flota
  (ver memoria `campaign3d-terrain-visual-zoffset` y el fix de ciudades-puerto en CITY_BLENDER §5 #12-14).
  Patrón a copiar: `BuildMeshCity` (Visual.cpp) + `RegisterCityFlatPad` + `CityLevelPads` (terreno plano
  bajo la huella).
- **Material**: `VertexColorMaterial` (unlit) para todo lo del mapa.

### 1.4 Docs relacionados
`Docs/CAMPAIGN3D_CITY_BLENDER.md` (pipeline Blender unlit + errores), `Docs/CAMPAIGN3D_VISUAL_STANDARD.md`
(estándar visual), memorias `campaign3d-*`.

---

## 2. DECISIONES ABIERTAS (resolver antes de lo visual)

**D1 — Estilo: UNLIT vs LIT.** El mapa, ciudades y vehículos son **unlit vertex-color**. El soldado
Quaternius es **LIT/PBR**. Mezclarlos = incoherencia (el soldado se verá sombreado/oscuro sobre el mapa
unlit; problema #4 del doc de ciudades). Opciones:
- **(A) Unlit-todo (recomendado AHORA):** asignar al `SK_Quaternius_Swat` un material **unlit** (o usar
  `veh_soldier`) para que cuadre con los vehículos. Menos riesgo, coherente con el mapa actual.
- **(B) Lit-todo (a futuro):** subir el mapa + unidades a lit/PBR. Es el "estándar AAA" que quiere el
  proyecto, pero el intento lit pasado dejó el mapa **cian** y se revirtió → requiere resolver eso primero.
→ **Plan asume (A)** por ahora; (B) es una fase aparte (Fase 7).

**D2 — Masa: instanced static vs skeletal animado.** "200 soldados por ejército × varios ejércitos" en
**skeletal animado** es inviable (no instancia barato). Solución estándar:
- **Formación (cientos):** `veh_soldier`/`veh_*` **instanciados estáticos** (baratísimos).
- **Primer plano / pocos héroes:** `SK_Quaternius_Swat` animado, solo al máximo zoom y para 1-3 figuras.
→ El sistema usa ISM para la masa; lo skeletal es pulido opcional (Fase 7).

**D3 — Escala representativa.** Vehículos: 1:1 (cuentas chicas, 20-40). Infantería: si 200 es demasiado
denso/pesado, dibujar 1 modelo cada K hombres (configurable) manteniendo el número real en el dato.

---

## 3. PLAN POR FASES

> Cada fase es incremental y **verificable sola**. Marcadas con ✅ las ya hechas.

### Fase 0 — Decisión de estilo + unificar el soldado  *(rápido, desbloquea todo lo visual)*
- **Qué:** aplicar D1(A): que `SK_Quaternius_Swat` use material unlit (o decidir usar `veh_soldier` para la
  masa y dejar el SWAT solo para héroe). Confirmar que todos los assets se ven coherentes en el mapa.
- **Cómo:** material unlit sobre el skeletal (o `MID` con su textura sin lighting); test en una escena.
- **Verificar:** poner un soldado y un tanque juntos en el mapa → mismo "nivel" de luz, ninguno se ve oscuro.
- **Archivos:** material asset; nota en CITY_BLENDER.

### Fase 1 — Modelo de datos: composición  *(C++, sin assets nuevos)*
- **Qué:** que una fuerza diga "20 MBT + 10 IFV + 200 infantería".
- **Cómo:**
  ```cpp
  USTRUCT(BlueprintType) struct FWLForceUnitGroup {
      GENERATED_BODY()
      UPROPERTY(BlueprintReadOnly) FString UnitType; // "mbt|ifv|apc|infantry|aircraft|ship|artillery|heli"
      UPROPERTY(BlueprintReadOnly) int32   Count = 0;
  };
  // en FWLCampaign3DForceView:
  UPROPERTY(BlueprintReadOnly) TArray<FWLForceUnitGroup> Composition;
  ```
  JSON por fuerza: `"composition":[{"unit":"mbt","count":20},{"unit":"infantry","count":200}]`.
  Parsear en `LoadCampaignMilitaryForceDefinitions`. `EstimatedStrength` = suma (o override).
- **Verificar:** el panel de la fuerza muestra "20 MBT · 10 IFV · 200 inf" (solo dato, sin visual aún).
- **Archivos:** `WLCampaign3DView.h`, `WLCampaign3DViewForces.cpp`, `MilitaryForces.json`, panel UI.

### Fase 2 — Assets que faltan  *(Blender, mismo pipeline unlit)*
- ✅ Hechos: mbt, ifv, apc, aircraft, ship, soldier.
- **Faltan:**
  - **Artillería** (`artillery`): obús remolcado o autopropulsado / lanzacohetes.
  - **Helicóptero** (`heli`): de ataque (tipo Apache) y/o transporte. Distinto del caza.
  - **Bandera/estandarte del ejército** (`banner`): mástil + paño (tinte por bando) = la "ficha" clicable.
  - Opcional: general/oficial (reusar soldado), submarino, antiaéreo/SAM, camión de suministro.
- **Cómo:** añadir kinds a `gen_vehicle.py` (`build_artillery`, `build_heli`, `build_banner`), generar,
  `preview_vehicle.py`, importar.
- **Verificar:** preview unlit de cada uno legible desde 3/4 y cenital.

### Fase 3 — Render de la formación (el núcleo: 1 ficha = N NPCs)  *(C++)*
- **Qué:** por cada fuerza, dibujar su `Composition` como una **formación instanciada** + un **banner**.
- **Cómo:**
  - Un `UInstancedStaticMeshComponent` por malla (mbt/ifv/.../infantry) — patrón `ConfigureInstancedComponent`.
  - Para cada `FWLForceUnitGroup`, colocar `Count` instancias en **formación** (rejilla/cuña/línea) con
    `AddInstance`, separación ~ tamaño de la unidad, orientadas al rumbo de la fuerza.
  - **Anclaje Z** = `ProjectLonLat(Lon,Lat) + VisualBiomeZOffset(ClassifyVisualBiome(Lon,Lat,bCore))`
    (igual que ciudades, para que no se hundan). Para fuerzas detenidas, registrar un flat pad pequeño.
  - **Banner** del líder al frente de la formación (mástil+paño tintado, etiqueta con nombre+contador).
  - Re-activar `bShowMilitaryForceMarkers=true`; `AddMilitaryForceMarker` ahora crea banner+formación+hitbox.
  - No pisar ciudades: colocar la formación fuera de `CityFlatPads`.
- **Verificar:** una fuerza de prueba ("20 MBT") en zoom Cercano se ve como 20 tanques en formación.
- **Archivos:** `WLCampaign3DView.cpp/.h` (nuevos ISM), `WLCampaign3DViewForces.cpp` (AddMilitaryForceMarker).

### Fase 4 — LOD (icono de lejos / formación de cerca)  *(C++)*
- **Qué:** lejos = banner+contador (barato); cerca = formación completa.
- **Cómo:** enganchar a `WLCampaign3DViewLOD.cpp` (umbral de altura, como `bCityDetail`): ocultar/mostrar
  los ISM de formación con `SetVisibility`. Banner siempre visible (escala por pantalla, como labels).
- **Verificar:** alejar → solo banners; acercar → aparecen las formaciones.

### Fase 5 — Selección, panel y color de bando  *(C++)*
- **Qué:** click en banner/formación → selecciona la fuerza (panel ya existe) y muestra la composición;
  tinte por bando.
- **Cómo:** reusar `TryGetForceForComponent`; para ISM, mapear índice de instancia → forceId (o banner como
  único proxy clicable en v1). Color de bando = `PerInstanceCustomData` leído por el material (o 1 ISM por
  país en v1).
- **Verificar:** clic abre el panel correcto; fuerzas de distinto país con distinto color.

### Fase 6 — Movimiento, marcha y formas por tipo  *(C++)*
- **Qué:** al mover la fuerza (sistema de nodos ya existe), banner + formación se mueven juntos; formas de
  formación por `ForceType` (cuña blindada, línea de infantería, V/columna aérea-naval).
- **Cómo:** interpolar la posición de la fuerza por el camino; reorientar la formación al rumbo. Formas =
  funciones de layout por tipo en la Fase 3.
- **Verificar:** una fuerza marcha de un nodo a otro arrastrando su formación.

### Fase 7 — Pulido / opcionales
- **Héroe animado:** al MÁXIMO zoom, sustituir 1-3 instancias por `SK_Quaternius_Swat` animado (Idle/Walk).
- **Estilo lit/PBR (D1-B):** si se decide subir todo a lit, resolver primero el "mapa cian".
- **Más unidades:** submarino, SAM, suministro; variantes camo (desert/grey) por bando/región.
- **Rendimiento:** medir con muchos ejércitos; aplicar escala representativa (D3) si hace falta.

---

## 4. Riesgos / reglas de oro
- **Z / hundimiento:** anclar SIEMPRE con `VisualBiomeZOffset` del bioma del centro (lección ciudades).
- **No tocar puerto ni ciudades:** el sistema de ejércitos es su propio componente/ISM aparte.
- **Coherencia de estilo:** no mezclar lit y unlit en el mapa (decisión D1).
- **Masa = instanced static**, no skeletal (decisión D2). Skeletal solo para primer plano.
- **Iterar barato:** assets propios no necesitan compilar (regenerar+reimport); el sistema (C++) sí.

---

## 5. Orden recomendado de ejecución
1. **Fase 0** (estilo: soldado unlit) — desbloquea lo visual.
2. **Fase 1** (composición en datos) — base, rápida, sin assets.
3. **Fase 3 + 4** (formación instanciada + banner + LOD) — aquí ya SE VE el ejército.
4. **Fase 2 (resto de assets: artillería, helicóptero, bandera)** — completan el roster.
5. **Fases 5-6** (selección/color, marcha).
6. **Fase 7** (pulido / lit / héroe animado).
