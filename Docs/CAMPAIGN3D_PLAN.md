# Campaign 3D — Fase 2: "Mapa Vivo" (capa de detalle por zoom)

> Fase 1 (expansión regional, hemisferio completo) → **COMPLETADA**.
> Registro: `docs/CAMPAIGN3D_PROGRESS.md`. Este documento es el plan de la Fase 2.

---

## 0. Por qué (la vara)
World Leader compite como **uno de los primeros juegos hechos con Claude**. El
objetivo no es "estar bien": es **llevar la vara altísima** con un mapa de campaña
que se sienta **vivo, reconocible y propio**. Ya tenemos una base sólida (todo el
hemisferio con biomas y ciudades). Esta fase la **perfecciona** hasta que cada
país se sienta **identificable** — que el jugador mire y diga "ese es mi país".

## 1. Pilares de diseño
1. **Legibilidad estratégica primero.** Es gran estrategia: el detalle nunca tapa
   la lectura del mapa (fronteras, ciudades, fuerzas).
2. **Que se sienta un lugar.** Prioridad real: **ciudades que parezcan ciudades**
   (edificios), **caminos visibles** y **terreno legible** (montañas/llanuras) — por
   encima de monumentos. La identidad por país es un **toque ligero** (silueta de la
   capital), no el foco.
3. **Realismo estilizado, no película.** Nos *acercamos* al realismo, pero
   priorizamos silueta, color y reconocimiento sobre fidelidad fotográfica.
4. **Todo por proximidad de cámara + LOD.** El detalle solo existe cerca y al
   acercar → rinde en hardware modesto. (Edificios solo cerca de la cámara.)
5. **Data-driven.** Perfiles de bioma y ciudades viven en datos → editable.
6. **No-regresión.** Cada capa es aditiva y verificada; lo que funciona no se rompe.

## 2. "¿No rompe?" — Estándar de no-regresión
- Cada fase es **aditiva**: nuevas capas sobre el terreno actual (que ya está verificado).
- El detalle nuevo se activa **por tier de zoom**; en Region/America (lo más usado
  para leer el mapa) **no cambia casi nada**.
- **Checkpoint por fase**: compilar (editor cerrado) → Play → no-regresión de lo
  anterior → commit + push. Si algo se rompe sin arreglo claro, se revierte la fase.
- **Presupuestos de rendimiento** explícitos (sección 7): si una capa baja el FPS
  bajo el umbral, se reduce densidad/LOD antes de avanzar.

## 3. Arquitectura: DOS capas (responde "¿por país o región?")
> **Las dos, en capas.** Región/geografía para la base; país para la identidad.

### Capa 1 — BASE por geografía (compartida, escalable)
Gobernada por el **modelo continental de biomas** (ya existe, por lon/lat). Define
**color, relieve, vegetación, props naturales y agua**. NO depende de fronteras.
Una región nueva hereda todo automáticamente.

### Capa 2 — IDENTIDAD por país (curada, memorable)
Cada país recibe **1–3 rasgos icónicos** colocados en sus coordenadas reales: un
**landmark**, un **acento de paleta** y un **hito urbano** en su capital. Es lo
que da el "se parece a mi país" sin modelar el país entero. Pocos elementos,
máximo reconocimiento. (Tabla en sección 5.)

Las **fronteras políticas** solo mandan en bordes, labels y selección — nunca en
el bioma. Así no hay "capas de geología por país" inconsistentes.

## 4. Estándar A — LOD por zoom (UNA sola fuente de verdad)
Hoy el LOD está disperso (`ApplyZoomLOD` + TerritoryLayer). **F0 lo unifica**
(`WLCampaignCameraRig`/`LODController`). Tiers y qué entra en cada uno:

| Tier | Cámara | Terreno/Natura | Ciudades | Identidad | Labels |
|---|---|---|---|---|---|
| **Global** (América) | muy lejos | color base | — | — | países |
| **Region** (continente) | lejos | + relieve suave, agua principal | siluetas | landmark mayor (Uyuni, Amazonas) | países |
| **Teatro** (país) | medio | + vegetación por bioma | mancha urbana estilizada | landmark + acento | ciudades |
| **Cercano** | cerca | + props, agricultura, densidad alta | edificios/skyline | hito urbano | ciudades + barrios |

Regla de oro: **streaming por proximidad** — solo se generan props/vegetación en
un radio alrededor de la cámara; al alejar, se descargan.

## 5. Estándar B+C — Bioma → perfil, y la IDENTIDAD por país

### Bioma → perfil de detalle (Capa 1)
`{ colorBase, relieveZ, vegetación(tipo+densidad), propsNaturales, agua }`

| Bioma | Color | Relieve | Vegetación | Props/agua |
|---|---|---|---|---|
| Amazonía/selva | verde profundo | bajo | árboles densos | ríos anchos |
| Bosque templado/boreal | verde | medio | árboles medios | lagos, ríos |
| Andes/Rocosas | marrón | **alto** | escasa, roca | nieve en cumbres |
| Altiplano | marrón claro | alto/plano | muy escasa | salares |
| Llanos/praderas/pampa | sabana | bajo | pastizal, manchas | ríos serpenteantes |
| Costa Pacífico/Caribe | arena | bajo | palmeras dispersas | espuma costera |
| Desierto (Atacama/SO/N México) | arena | bajo/medio | cactus, roca | seco |

### Identidad por país (Capa 2) — OPCIONAL / DIFERIDA
> Decisión: los **monumentos** (Cristo, Machu Picchu…) son *polish caro y arriesgado*
> y **no son la prioridad**. La identidad real se logra casi gratis con la **silueta
> de la capital** (CDMX densa, NY con rascacielos, La Paz alta) + el **carácter del
> bioma**. Los landmarks de abajo quedan como **futuro opcional, solo si sobra
> presupuesto** tras ciudades/caminos/relieve.

| País | Landmark(s) icónico(s) | Acento / hito urbano |
|---|---|---|
| **EEUU** | Manhattan skyline (NY), Golden Gate (SF), Gran Cañón (SO) | rascacielos en DC/Chicago |
| **Canadá** | Rocosas/Banff, lagos boreales | CN Tower (Toronto) |
| **México** | pirámides (Teotihuacán/Yucatán), cenotes | CDMX denso, Cancún resort |
| **Colombia** | eje cafetero andino, murallas de Cartagena | Bogotá en altiplano |
| **Venezuela** | tepuyes + Salto Ángel (Canaima) | torres de Maracaibo (petróleo) |
| **Brasil** | Cristo Redentor (Rio), Amazonas, Iguazú | São Paulo skyline |
| **Argentina** | Patagonia (Fitz Roy), Pampa/estancias, Iguazú | Obelisco (Buenos Aires) |
| **Chile** | Atacama, volcanes andinos, Torres del Paine | Santiago al pie de los Andes |
| **Perú** | Machu Picchu (Andes), Lago Titicaca, costa desértica | Lima costera |
| **Bolivia** | **Salar de Uyuni**, altiplano | La Paz (la ciudad más alta) |
| **Ecuador** | volcanes (Cotopaxi), Galápagos (offshore) | Quito andino |
| **Cuba** | casco viejo de La Habana, vegas de tabaco | malecón |
| **Panamá** | el Canal (esclusas) | skyline de Ciudad de Panamá |
| **Centroamérica** | cadena de volcanes, selva maya | capitales coloniales |
| **Caribe** | playas/arrecifes, montañas tropicales | fuertes coloniales |

> Estos son **estilizados** (silueta + decal + mesh simple), no réplicas AAA. Bastan
> para identidad. La lista es ampliable en datos.

## 6. Estándar D — Ciudades modernas estilizadas (el "embellecer ciudades")
La ciudad deja de ser un cubo y pasa a **mancha urbana que crece por tamaño/tipo**:
- **Capital**: núcleo de torres + hito nacional (label dorado, aparece antes).
- **LargeCity**: bloques medios.
- **Industrial**: naves + chimeneas/torres (nicho moderno).
- **Port**: muelles + grúas sobre la costa.
- **Frontier**: caserío disperso.
Crece con el zoom (Teatro = silueta; Cercano = edificios), sobre terreno con
relieve y rodeada de campos → deja de "flotar".

## 7. Estándar E — Rendimiento (hardware modesto)
- **Streaming por proximidad**: vegetación/props solo en radio de cámara; instanced.
- **Instancing agresivo** (un draw call por tipo) + **LOD por tier**.
- **Presupuesto por tier** (objetivo): Region fluido; Cercano sin tirones.
- **Celdas adaptativas** ya activas para países enormes; extender a props.
- Métrica de aceptación por fase: FPS estable + tiempo de carga razonable.

## 8. Estándar F — Data-driven
- `BiomeProfiles` (perfil por bioma) y `CountryIdentity` (landmarks/acento/hito por
  país) en **datos** (JSON), no en C++ disperso.
- Paga la deuda heredada (hoy países/ciudades están hardcodeados).
- Permite balancear/añadir sin recompilar.

## 9. Fases de ejecución (lotes verificables, commit por fase)
Orden por **prioridad real**: que se sienta un lugar (ciudades, caminos, terreno).
- **F0 — Cimientos**: unificar LOD/cámara (una fuente de verdad de "zoom adecuado y
  estándar") + extraer `CameraRig`. Sin cambio visual. Habilita lo demás.
- **F1 — Relieve legible**: montañas se elevan, llanuras planas, costa — el terreno
  se lee como terreno **al zoom correcto** (extiende el Z por bioma existente).
- **F2 — Ciudades que parecen ciudades** (FOCO): LOD de 3 niveles
  (punto → silueta → edificios) por **tamaño/tipo** y **proximidad de cámara**;
  capital con silueta distintiva. Reemplaza el cubo actual.
- **F3 — Caminos**: arterias entre ciudades clave + insinuación de trama urbana al
  acercar (curado, no exhaustivo).
- **F4 — Vegetación por bioma** (ambiente de apoyo): bosques/pastos/cactus por perfil,
  scatter por proximidad. Más ligero que el resto.
- **F5 — Agua + pulido**: ríos/lagos principales, nieve en cumbres, niebla; ajustes.
- **F6 — Data-driven**: biomas/ciudades a JSON (paga deuda heredada).
- **(Diferido/opcional)** — Monumentos de identidad por país (sección 5), solo si
  sobra presupuesto.

## 10. Validación y "Definición de Hecho" por fase
1. Compilar con editor cerrado → relanzar.
2. **No-regresión**: alejar = solo países, limpio; lo anterior intacto.
3. **Mejora**: la capa nueva se ve y aporta identidad/vida al acercar.
4. **Rendimiento**: FPS y carga aceptables en hardware modesto.
5. **Checkpoint**: commit propio + push.

### Definición de calidad objetivo
Que al panear/zoom, cada país tenga **al menos un rasgo reconocible** y el conjunto
se sienta **un mundo vivo y coherente** — estilizado pero creíble — manteniendo la
**legibilidad de gran estrategia**. Esa es la vara.
