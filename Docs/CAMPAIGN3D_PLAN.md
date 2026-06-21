# Campaign 3D — Plan REFORMULADO: "Campaña 3D estilo Total War"

> Reformulado tras feedback (2026-06-21): lo construido hasta ahora es **2/10**. El
> objetivo es la **fidelidad del mapa de campaña de Total War** (cámara inclinada 3D,
> terreno con relieve, caminos que se ven caminos por donde pasan tropas, ciudades que
> se ven ciudades). Fase 1 (cobertura del hemisferio) sigue válida: `CAMPAIGN3D_PROGRESS.md`.

---

## 0. Diagnóstico honesto (por qué estamos en 2/10)
El problema no es "falta detalle": es que **el enfoque base es incompatible** con TW.
- **Cámara cenital** (desde arriba) → nunca se ve el relieve ni la profundidad.
- **Terreno plano** con color por vértice → no hay montañas/valles reales.
- **Ciudades = cajas top-down** → parecen "tetris", no ciudades.
- **Caminos = cintas planas rectas** → no parecen caminos.
- **Tropas = conos/marcadores** → no son unidades.

Con esa base, detallar más **no acerca a TW**. Hay que cambiar los cimientos.

## 1. La verdad sobre el techo (cuándo NO se puede solo con código)
- **Procedural puro** (cámara inclinada + relieve + caminos drapeados + mejores
  volúmenes): techo honesto ~**6/10**. Mundo 3D legible y muy superior al actual.
- **TW de verdad (~9/10)**: necesita **ASSETS 3D** (modelos de ciudad, árboles,
  soldados animados, texturas/normales de terreno). Eso **no sale del código**;
  hay que **importar packs** (Fab/Marketplace) y conectarlos.
- Decisión del proyecto: hasta dónde invertir en assets. El plan soporta ambos:
  procedural primero (cimientos), assets después para subir de 6 a 9.

## 2. Cimiento innegociable (sin esto, nada se ve TW)
**R0 — Cámara inclinada + relieve 3D real.**
- Cámara con **pitch ~35–45°** (perspectiva), no cenital. Rehacer el hit-test del
  HUD/selección acorde (ya resolvimos el letterbox; esto lo amplía).
- Terreno con **elevación real** (heightmap): cordilleras se elevan, valles se hunden,
  costa con desnivel. Que al mirar se vea un mundo 3D.
- Solo esto sube el feel de 2 a ~4 y **habilita** caminos drapeados, tropas, etc.

## 3. CAMBIOS POR PAÍS — el estándar (data-driven) que pediste
Cada país define en **datos (JSON)**, no en C++, su ficha de detalle:
- **Ciudades**: lista con `tipo` (capital / metrópoli / puerto / industrial / pueblo)
  y `tamaño` → modelo/silueta acorde (no una caja genérica).
- **Red de carreteras**: qué ciudades conecta y por **waypoints** (para que el camino
  serpentee por terreno real, no recto) → es a la vez visual y **ruta de tropas**.
- **Rasgos del terreno**: cordillera principal, río(s), costa, paso fronterizo.
- **Identidad ligera** (opcional): 1 rasgo reconocible (silueta de la capital).
Así cada país se siente propio y se edita sin recompilar.

## 4. Caminos de verdad (POR AHÍ PASAN TROPAS) — R1
- Camino = **spline drapeado sobre el terreno** (sigue el relieve), con **ancho y
  textura de camino** (tierra/asfalto), **serpenteante** (no recto).
- Es **a la vez** lo visual y la **red de movimiento**: las tropas se desplazan a lo
  largo del spline entre ciudades (ya existe el grafo de adyacencia; se reusa).
- Reemplaza las cintas planas actuales. Referencia: el sendero de la 3ª imagen.

## 5. Ciudades de verdad (no "tetris") — R2
- Por **tipo y tamaño**: silueta coherente, con la cámara inclinada se lee como ciudad.
- Mínimo procedural: volúmenes con **materiales reales** (no color plano), techos,
  variación, muelles en puertos. Ideal: **meshes de edificios** (assets) para el salto.
- Aparecen/crecen por **proximidad de cámara** (rinde).

## 6. Tropas en el mapa — R3
- Unidad = **modelo 3D** sobre el terreno (placeholder digno ahora; soldado/vehículo
  con assets después) que **se mueve por los caminos** entre ciudades.

## 7. Terreno y materiales hacia TW — R4
- Texturas/normales de terreno (hierba, roca, tierra, arena), **foliage** (árboles
  como meshes, no conos) por proximidad, agua con shader, costa con desnivel.

## 8. Bioma: DEPRIORIZADO (por instrucción)
- El color por bioma queda **solo como telón de fondo**, NO es el objetivo. El nivel
  de detalle objetivo es **mucho mayor** (relieve 3D + assets + caminos + tropas).
- No se invierte más en bioma hasta tener los cimientos (R0–R3).

## 9. Rendimiento / hardware (honesto)
- TW-level en todo el continente es caro y el hardware está por debajo de specs UE5.
- Regla dura: **detalle pesado solo cerca de la cámara** (LOD/streaming por proximidad).
  Lejos = versión ligera. Medir FPS y tiempo de carga en cada fase.

## 10. Secuencia reformulada (commit por fase, verificable)
- **R0 — Cámara inclinada + relieve 3D** (cimiento; arregla hit-test). *Mayor cambio de feel.*
- **R1 — Caminos drapeados** (visual + ruta de tropas).
- **R2 — Ciudades como modelos por tipo** (quitar "tetris").
- **R3 — Tropas 3D moviéndose por caminos.**
- **R4 — Terreno/materiales + foliage** hacia TW.
- **R5 — Estándar por país en JSON** (ciudades + carreteras + rasgos) para todo el continente.
- **(Assets)** — Insertar packs 3D (Fab/Marketplace) donde el procedural no llega
  (ciudades, árboles, tropas) para subir de ~6 a ~9.

## 11. Definición de "HECHO" (la vara)
Al **máximo zoom**, como la 3ª imagen de TW: el **camino se ve un camino**, la
**ciudad se ve una ciudad**, y una **tropa se ve sobre el terreno/camino**. Si no se
cumple eso, no está hecho.

## 12. Qué se descarta del intento anterior
- Mancha urbana "tetris" (cajas top-down) → se reemplaza por R2.
- Vegetación por bioma (scatter de conos) → descartada por ahora (bioma deprioritizado).
- Las carreteras planas rectas → se reemplazan por caminos drapeados (R1).
- Se conserva: cobertura del hemisferio, ciudades/labels base, LOD de zoom, contorno.
