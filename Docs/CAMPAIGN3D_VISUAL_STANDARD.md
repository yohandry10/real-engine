# Campaign 3D — Estándar visual (lit/PBR, AAA) — ÚNICO Y NO NEGOCIABLE

> Doc rector de la calidad visual de Campaign 3D. Deroga el enfoque "todo unlit por
> hardware". Léelo antes de tocar terreno, vegetación, ciudades, agua o iluminación.
> Última actualización: 2026-06-24.

## 1. El estándar (referencia única)
- **Total War: Rome 2** (mapa de campaña) y **Cities: Skylines** (vista aérea) son el
  estándar. No "inspiración": el **objetivo a igualar**.
- Significa: **lit / PBR**, terreno **texturizado** (no color plano), **foliage real con
  LODs**, **iluminación + atmósfera** (sol con sombras, niebla, color grading).

## 2. El hardware NO es restricción (regla)
- El usuario usará una máquina potente si hace falta. **No bajar el estándar por hardware.**
- Tope de costo de referencia: el juego **no debe pesar más que FIFA 26 o TW Rome 2** en una
  máquina capaz. Eso se logra con **LOD/HLOD, streaming, luz horneada e instancing**, NO
  recortando calidad.
- **PROHIBIDO** volver a justificar decisiones con "el hardware está por debajo de specs".

## 3. Qué queda DEROGADO
- La regla histórica "todo el mapa es UNLIT vertex-color por hardware" (antigua AGENT_NOTES §2).
  El mapa migra a **lit/PBR**. La técnica del cubemap ambiente sigue útil como parte de la luz,
  pero el framing "unlit por diseño" se elimina.
- El **scatter de vegetación unlit** (conos / blobs vertex-color, picos sueltos sobre llano):
  **descartado**. Se reemplaza por foliage lit con LODs (M2).
- La regla de ciudades "NUNCA materiales LIT": **invertida**. Las ciudades migran a lit/PBR (M3).

## 4. Plan por milestones (vertical slice: Venezuela primero, luego escalar)
Cada milestone se **verifica con screenshot** antes de avanzar.

- **M1 — Terreno lit + texturizado.** Material PBR que mezcla pasto/roca/tierra/nieve por
  **pendiente + altura**, con **normal maps**, teñido por bioma. **Sol direccional con sombras
  + Sky Atmosphere + niebla**. Resultado: el relieve deja de ser plano olivo; se ve volumen,
  luz y sombra reales.
- **M2 — Foliage lit con LODs.** Árboles/arbustos/pasto reales (pack VRS lit u otros), en
  **bosques** que se pegan al relieve, densidad por bioma, con instancing + LOD/HLOD. Estilo
  Rome 2 (masas de bosque con claros, no alfombra de puntos).
- **M3 — Ciudades lit/PBR.** Migrar las ciudades a materiales lit (texturas de fachada / PBR),
  conservando el modelo cohesivo por asentamiento.
- **M4 — Carreteras + agua lit.** Asfalto texturizado; agua con material de agua real
  (reflejo/normal/profundidad), no plano azul.
- **M5 — Post / atmósfera.** Color grading, niebla de distancia, exposición y encuadre de
  cámara estilo Rome 2.

## 5. Reglas de oro (nuevas)
- **Verificar SIEMPRE con screenshot propio antes de mostrar.** No iterar a ciegas.
- **No prometer lo que no se ve en pantalla.** El estándar es 3/4; si un paso no se acerca, se
  rehace, no se entrega.
- **Optimizar por LOD/streaming**, nunca bajando el estándar.
- **Lit/PBR en todo** lo que esté sobre el mapa (terreno, foliage, ciudades, agua, caminos).
