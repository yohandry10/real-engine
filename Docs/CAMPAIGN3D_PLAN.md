# Campaign 3D — Plan v3: "Vertical slice estilo Total War, por país (Venezuela primero)"

> Reescrito 2026-06-21 tras feedback con referencia directa a **Total War: Rome II**
> (ciudad Tarraco). La vara es ESE nivel. Cambio de estrategia: **NO más continente
> a la vez** — se construye **un país completo a alta fidelidad (Venezuela)** como
> plantilla, y luego se replica país por país.

---

## 0. La verdad sin adornos (leer antes de nada)
- La referencia (Tarraco) es **arte AAA de Creative Assembly**: modelos 3D hechos a
  mano por cultura, terreno esculpido, SpeedTree, shaders, equipo de artistas, años.
- **El código procedural NO llega a eso.** Cajas generadas nunca serán Tarraco.
- Para ese look hacen falta **ASSETS 3D reales** (kits de edificios, foliage, materiales
  de terreno). **Decisión tomada: assets AHORA**, no "después".
- **Techo honesto:** con packs de assets + buena técnica, por país, ~**7–8/10** de la
  referencia (se siente TW). Igualar el arte bespoke de CA = equipo de arte (fuera de
  alcance). Apuntamos al **feel**, no al pixel exacto.

## 1. Estrategia: vertical slice por país
- Se elige **Venezuela** como primer país y se lleva a **alta fidelidad completa**.
- Venezuela se vuelve el **ESTÁNDAR** (checklist replicable). Solo cuando Venezuela
  esté al nivel, se pasa al siguiente país con el mismo molde.
- El resto del continente queda como **fondo de baja resolución** mientras tanto (no
  se borra; solo no se detalla aún).

## 2. El ESTÁNDAR por país (lo que "hecho" significa)
Cada país, para considerarse terminado, debe tener:
1. **Terreno 3D real**: relieve esculpido (cordilleras, valles, costa) con **materiales
   texturizados** (hierba, roca, tierra, arena) — no color plano.
2. **Foliage**: árboles/vegetación como **meshes** (asset/SpeedTree), por densidad de
   zona, con streaming por proximidad.
3. **Ciudades como modelos**: kit de edificios 3D (no cajas) por **tipo y tamaño**
   (capital, metrópoli, puerto, industrial, pueblo). Con murallas/puerto donde aplique.
4. **Carreteras en spline drapeado** sobre el terreno, **serpenteantes**, texturizadas
   (tierra/asfalto). Conectan las ciudades y **por ellas se mueven las tropas**.
5. **Tropas**: modelo 3D sobre el terreno que **camina por las carreteras**.
6. **Ríos / agua** principales con shader.
7. **Datos por país (JSON)**: ciudades (tipo+tamaño), red de carreteras (waypoints),
   rasgos de terreno. Editable sin recompilar.

## 3. Cimiento técnico (transversal, primero)
- **R0b — Cámara inclinada (perspectiva, pitch dinámico)**: cenital al alejar, ~40–45°
  al acercar (como TW). EN CURSO. Sin esto nada se ve 3D.
- **R0a — Relieve continental** (hecho a nivel base; se refinará con materiales).
- **Pipeline de assets**: importar packs (Fab/Marketplace) de edificios, foliage y
  materiales de terreno al proyecto. Requiere añadir contenido (paso del usuario o
  packs gratuitos autorizados).
- **Rendimiento**: hardware bajo specs UE5 → detalle pesado **solo cerca de cámara**
  (LOD/streaming). Medir FPS por hito.

## 4. Secuencia para VENEZUELA (vertical slice)
- **V0 — Cámara inclinada** funcionando (selección/nav OK). [transversal]
- **V1 — Terreno de Venezuela**: relieve real (cordillera de la costa, Andes
  merideños, Guayana, llanos, delta) + **materiales texturizados**.
- **V2 — Foliage**: selva al sur/Guayana, sabana en llanos, costa — con meshes.
- **V3 — Carreteras spline** entre ciudades venezolanas, drapeadas y serpenteantes.
- **V4 — Ciudades como modelos** (kit de edificios) por tipo: Caracas (capital),
  Maracaibo (puerto/petróleo), Valencia/Maracay (industrial), Ciudad Guayana, etc.
- **V5 — Tropas 3D** que se mueven por las carreteras venezolanas.
- **V6 — Agua/ríos** (Orinoco) + pulido + materiales finales.
- **V7 — Datos JSON** de Venezuela (ciudades/carreteras/rasgos) como plantilla.
- → Congelar el **ESTÁNDAR** y replicar a Colombia, etc.

## 5. Qué se descarta / cambia del intento anterior
- **Mancha urbana "tetris"** (cajas top-down) → reemplazada por ciudades de meshes (V4).
- **Vegetación de conos** → reemplazada por foliage de meshes (V2).
- **Carreteras planas** → splines drapeados (V3); además se corrigen las redes rotas
  (Perú rota, Brasil inexistente) — pero **eso es secundario**: el foco es Venezuela.
- **Bioma como objetivo** → solo telón de fondo; el objetivo es el detalle 3D + assets.
- Se conserva como FONDO: cobertura del hemisferio en baja resolución, labels, LOD.

## 6. Definición de HECHO (la vara, sin negociación)
Al máximo zoom en Venezuela, debe verse como la referencia de TW: **el camino se ve
un camino**, **la ciudad se ve una ciudad** (edificios, no cajas), **los árboles son
árboles**, el **terreno tiene relieve y textura**, y **una tropa se ve sobre el
camino**. Si no, no está hecho.

## 7. Honestidad continua
En cada hito diré con claridad qué se logró, qué no, y si algo necesita assets que no
tenemos. No se vende como "TW" lo que sea una caja. La vara es la 3ª imagen.
