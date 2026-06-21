# Campaign 3D — Fase 2: "Mapa Vivo" (capa de detalle por zoom)

> Fase 1 (expansión regional, todo el hemisferio) → **completada**, registro en
> `docs/CAMPAIGN3D_PROGRESS.md`. Esta es la fase siguiente.

## Visión
Acercarnos a la sensación del mapa de campaña de **Total War** — **lejos: mapa
estratégico limpio y legible; cerca: un mundo vivo y detallado** — pero con
**nuestro estilo y nicho**: gran estrategia **geopolítica moderna** sobre las
Américas reales (autopistas, puertos, zonas industriales, bases aéreas), estética
**realista estilizada** y **data-driven** (no AAA fotorrealista; legibilidad
estratégica primero).

## Principio rector (responde "¿geología por país?")
El detalle se decide por **GEOGRAFÍA (ubicación), NO por país**. Hay **un solo
modelo continental** ya existente — los biomas por lon/lat (`ClassifyVisualBiome`) —
y ese mismo modelo gobierna **color, relieve, vegetación, props y agua**. Las
fronteras políticas solo afectan **bordes, labels y selección**, nunca el bioma.
Esto evita "capas por país" inconsistentes y escala solo a regiones nuevas.

## El estándar de detalle = Bioma → Perfil de detalle
Cada bioma tiene un **perfil de detalle** (dato, no hardcode disperso):
`{ colorBase, relieveZ, vegetación(tipo+densidad), props, agua, agricultura }`.

| Bioma | Color | Relieve | Vegetación | Props/agua |
|---|---|---|---|---|
| Amazonía / selva | verde profundo | bajo | árboles densos | ríos anchos |
| Bosque templado/boreal | verde | medio | árboles medios | lagos, ríos |
| Andes / Rocosas | marrón | **alto** (se elevan) | escasa, roca | nieve en cumbres, ríos de montaña |
| Altiplano | marrón claro | alto/plano | muy escasa | salares |
| Llanos / praderas / pampa | sabana | bajo | pastizal, manchas | ríos serpenteantes |
| Costa Pacífico / Caribe | arena | bajo | palmeras dispersas | espuma costera |
| Desierto (Atacama/SO/N México) | arena | bajo/medio | cactus, roca | seco |

## Estándar de zoom-LOD (UNA sola fuente de verdad)
Hoy el LOD vive disperso (`ApplyZoomLOD` + TerritoryLayer). **Primer trabajo de
esta fase: unificarlo** (extraer `WLCampaignCameraRig`/`LODController`). Tiers:

| Tier | Qué se ve | Detalle |
|---|---|---|
| **Global** (América) | países (color + nombre) | 0 detalle |
| **Region** (continente) | países + relieve suave + agua principal | sin props |
| **Teatro** (país) | + vegetación por bioma + ciudades estilizadas + caminos | medio |
| **Cercano** | + props ambientales + agricultura + densidad alta | full |

Regla de oro: el detalle **solo aparece cerca de la cámara** (streaming por
proximidad), nunca todo el continente a la vez → rinde en hardware modesto.

## Capas (en orden de impacto/seguridad)
1. **Relieve por bioma** — montañas se elevan, valles, costa baja (extiende el Z por bioma que ya existe).
2. **Agua** — ríos principales + lagos + costa (hay `WLCampaignWaterBuilder` infrautilizado).
3. **Vegetación por bioma** — bosques/pastos/cactus/palmeras según perfil + densidad, **instanced por proximidad de cámara** (reemplaza el scatter genérico CO/VE actual).
4. **Ciudades estilizadas** — ver estándar abajo (reemplaza el cubo actual).
5. **Agricultura + caminos** — campos alrededor de ciudades y rutas entre ellas (ya hay route builder).
6. **Ambiente** — nieve en cumbres, niebla por zona, roca dispersa, pulido.

## Estándar de ciudad (el "embellecer ciudades", dentro de la capa)
La ciudad es una **mancha urbana estilizada** que crece por **tamaño y tipo**, no
un cubo:
- **Capital**: núcleo denso de torres + hito (sale antes en el zoom, label dorado).
- **LargeCity**: bloques medios.
- **Industrial**: naves + chimeneas/torres (nicho moderno).
- **Port**: muelles + grúas sobre la línea de costa.
- **Frontier**: caserío disperso pequeño.
Aparece/crece con el zoom (Teatro = silueta; Cercano = edificios). Sobre terreno
con relieve y rodeada de campos → deja de "flotar".

## Nuestro nicho (no clon de Total War)
- **Era moderna**: autopistas, puertos de contenedores, zonas industriales, bases
  aéreas/militares, megaciudades — no aldeas medievales.
- **Geografía real de las Américas** como tablero; teatros con identidad.
- **Data-driven**: perfiles de bioma y ciudades en datos → editable sin tocar C++.
- **Legibilidad estratégica primero**: el detalle nunca tapa la lectura del mapa.

## Fases de ejecución (lotes verificables, commit por fase)
- **F0 — Cimientos**: unificar LOD/cámara (una fuente de verdad) + extraer `CameraRig`. (Habilita lo demás; sin cambio visual.)
- **F1 — Vegetación por bioma** (mayor impacto visible): perfiles de bioma + scatter por proximidad.
- **F2 — Relieve + agua**.
- **F3 — Ciudades estilizadas** (por tamaño/tipo).
- **F4 — Agricultura + caminos**.
- **F5 — Ambiente y pulido** (nieve, niebla, roca).
- **F6 (paralelo) — Migrar a data-driven**: biomas/ciudades/perfiles a datos (paga la deuda heredada).

## Estándar de validación (cada fase)
Compilar (editor cerrado) → Play → probar **alejar (solo países, limpio)** y
**acercar (detalle aparece por proximidad, sin tirones)** → revisar **rendimiento
de carga/FPS** en hardware modesto → CO/VE y el resto **sin regresión** → cerrar
con **commit propio + push** (checkpoint).
