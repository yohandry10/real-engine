# World Leader — Visión & Plan de ejecución

> **Este es el ÚNICO doc de visión + tablero de tareas del proyecto.** Una sola fuente de
> verdad, pensada para trabajarse en **sesiones cortas y en cuentas distintas**: cada sesión
> termina UNA tarea (compilada, no a medias) y la marca como hecha, para que la siguiente
> sesión —aunque sea otra cuenta— continúe sobre lo construido sin perder el hilo.
> No dupliques esta info en otros docs.

---

## ⚙️ Cómo trabajar este doc (protocolo — LÉEME primero)

1. **Ve a "📍 Estado actual"** (justo abajo): ahí está la fase activa y la próxima tarea.
2. **Toma la PRIMERA tarea sin marcar `[ ]`** de la fase activa, en orden.
3. **Complétala ENTERA en esta sesión**: código que **compila**
   (`Build.bat WorldLeaderEditor Win64 Development`) y, si aplica, verificado. **Nunca la dejes a medias.**
4. Si la tarea es demasiado grande para una sesión, **divídela aquí** en subtareas
   (p.ej. `F1.4a`, `F1.4b`) y haz **solo la primera**; deja el resto marcado `[ ]`.
5. **Márcala `[x]`**, añade una línea en "📒 Registro" (fecha + qué quedó + archivos).
6. **Actualiza "📍 Estado actual"** con la próxima tarea.
7. **Commit** con mensaje `feat(F#.#): <resumen>`.

**Regla de oro:** *una sesión = una tarea terminada y marcada.* Construir sobre lo construido, nunca a medias.

---

## 📍 Estado actual

- **Fase activa:** F1 — Personajes & Generales
- **Próxima tarea:** **F1.2** — Almacén de personajes (subsistema)
- **Última actualización:** 2026-07-01

---

## 🎯 La visión (la brújula)

**World Leader** — grand strategy geopolítico moderno donde **gobiernas** una nación, no solo
comandas ejércitos. *Tu mayor enemigo puede estar sentado a tu mesa.*

- **La espina — PODER ⇄ CONTROL.** Para ganar acumulas poder (ejércitos, generales fuertes,
  economía, aliados); **cada gramo de poder amenaza tu control** (golpes, revueltas, traición).
  Manejar ese filo *es* el juego. Eso lo separa de "mover ejércitos + leer estadísticas".
- **El motor — todo son PERSONAJES.** Generales, ministros, oposición, líderes rivales, espías:
  el **mismo modelo** (Skill · Lealtad · Ambición · Popularidad · Rasgos · Agenda). Se construye
  **una vez** y alimenta las 4 arenas. Es el principio organizador del proyecto.

**Las 4 arenas + la base:**

- **GUERRA** — ejércitos liderados por generales (estilo Total War): reclutar en fuertes, marchar, batallas.
- **ALTO MANDO** — tu círculo: ascender, purgar, oposición, sobrevivir golpes.
- **DIPLOMACIA** — relaciones, tratados, embajadas, casus belli, declarar guerra.
- **INTRIGA** — espías, sabotaje, financiar golpes/rebeldes en el rival, propaganda.
- **Base: NACIÓN & ECONOMÍA** — el recurso que todo consume (no una meta en sí misma).

**Loop de turno:** ingresos/gastos → eventos & decisiones → órdenes (militar / interno / diplomacia)
→ fin de turno (IA, batallas, chequeo de golpe/revuelta).

**Metas (victoria):** Dominación (someter al rival) · Hegemonía (control económico/diplomático) ·
Régimen/Legado (mantente en el poder). **Derrota:** golpe, revolución o conquista.

**Reorg de la ventana GOBIERNO:** RESUMEN (dashboard + economía) · ALTO MANDO · POLÍTICA (interno,
pequeño) · DIPLOMACIA (reemplaza NACION) · REGISTROS. *NACION se elimina; su tabla de provincias se
pliega en RESUMEN cuando llegue DIPLOMACIA (F3.6).*

---

## 🗺️ Plan por fases (con tareas)

Cada tarea es **pequeña y de una sesión**. `[ ]` pendiente / `[x]` hecha.
Criterio de "hecho" por tarea: **compila** + lo que indique la tarea.

### F1 — Personajes & Generales  *(el motor; conecta con los fuertes/ejércitos ya hechos)*
**Objetivo:** reclutar un ejército crea un **general nombrado** que lo lidera en el mapa; gestionable en ALTO MANDO.

- [x] **F1.1 — Modelo `FWLCharacter`.** USTRUCT en `Core/WLCharacterTypes.h` con: Id, Name, CountryIso,
  Role, Rank, Skill, Loyalty, Ambition, Popularity, Renown, Traits, AssignedArmyId + enums
  `EWLCharacterRole` y `EWLMilitaryRank`. *Hecho = compila.*
- [ ] **F1.2 — Almacén de personajes.** Subsistema `UWLCharacterSubsystem` (GameInstance) con
  `TMap<FString,FWLCharacter>`: `CreateGeneral(iso)`, `GetCharacter(id)`, `GetGenerals(iso)`.
  *Hecho = compila + se crea un general por código.*
- [ ] **F1.3 — Pool de nombres/rasgos (JSON).** `Content/Data/Characters/Characters.json`
  (nombres, apellidos, rasgos) + loader → `CreateGeneral` usa nombres variados.
  *Hecho = nombres reales, no "General 1".*
- [ ] **F1.4 — General al reclutar.** Al completar reclutamiento en un fuerte, crear/asignar general;
  el ejército guarda su Id; el nombre del token pasa a "&lt;Rango&gt; &lt;Apellido&gt;".
  *Hecho = el ejército reclutado se llama por su general.*
- [ ] **F1.5 — General en el panel del ejército.** La ficha del ejército muestra general
  (nombre, rango, skill). *Hecho = se ve al seleccionar el ejército.*
- [ ] **F1.6 — Tab ALTO MANDO.** En `WLGovernmentWidget`, tab que lista generales
  (nombre, rango, skill, lealtad, ambición) con el patrón de tarjetas. *Hecho = se ven los generales del país.*
- [ ] **F1.7 — Ascender / Dar de baja.** Botones en ALTO MANDO: ascender (sube rango) /
  dar de baja (retira). *Hecho = cambian el estado del general.*
- [ ] **F1.8 — Experiencia por turno.** Generales ganan renombre al avanzar turnos / al combatir;
  al pasar un umbral, +1 rasgo o +skill. *Hecho = el renombre sube al avanzar turnos.*

### F2 — Poder interno & Golpes  *(nace el juego de supervivencia)*
**Objetivo:** los generales desleales y la oposición pueden derrocarte.

- [ ] **F2.1 — Riesgo de golpe.** Cálculo por nación desde lealtad/ambición de generales + orden público; medidor en POLÍTICA.
- [ ] **F2.2 — Intento de golpe.** En fin de turno, si el riesgo supera umbral, dispara golpe (resuelto por skill/lealtad/ejércitos leales).
- [ ] **F2.3 — Oposición.** Entidad con fuerza/popularidad que crece con bajo orden público.
- [ ] **F2.4 — Acciones internas.** Recompensar general (+lealtad, cuesta tesoro), purgar (elimina, −lealtad de otros), reprimir oposición.
- [ ] **F2.5 — Orden público efectivo.** El orden público por provincia afecta ingreso y alimenta oposición/golpe.
- [ ] **F2.6 — Panel POLÍTICA rehecho.** Medidor de golpe + oposición + acciones (reemplaza los "Fase futura").

### F3 — Diplomacia & Guerra  *(el juego internacional)*
- [ ] **F3.1 — Relación entre países.** Opinión −100..100 por par de países + almacén.
- [ ] **F3.2 — Líderes extranjeros.** Como `FWLCharacter` (Role=ForeignLeader) con personalidad.
- [ ] **F3.3 — Estados de relación.** Paz / tensión / guerra + casus belli.
- [ ] **F3.4 — Declarar guerra.** Acción CO↔VE que cambia estado y habilita ataques entre países.
- [ ] **F3.5 — Tratados.** Comercio, no-agresión, alianza (IA acepta según opinión).
- [ ] **F3.6 — Tab DIPLOMACIA (reemplaza NACION).** Lista de países, opinión, acciones.
  *Aquí se pliega la tabla de provincias en RESUMEN y se elimina el tab NACION.*

### F4 — Intriga exterior  *(la mano oculta)*
- [ ] **F4.1 — Agentes/espías.** `FWLCharacter` (Role=Spy) + red de inteligencia por país.
- [ ] **F4.2 — Sabotaje.** Daña economía/ejército rival, con riesgo de descubrimiento.
- [ ] **F4.3 — Financiar golpe/rebeldes.** Sube el riesgo de golpe del rival (reusa F2).
- [ ] **F4.4 — Propaganda.** Baja el orden público rival / sube el tuyo.
- [ ] **F4.5 — Contraespionaje.** Incidentes de descubrimiento que afectan la relación.

### F5 — Eventos & Metas  *(el tejido y el cierre)*
- [ ] **F5.1 — Motor de eventos.** Definición de evento (condición → opciones → efectos) + cargador JSON.
- [ ] **F5.2 — Cola + popup.** Eventos por turno con UI para resolverlos.
- [ ] **F5.3 — Victoria.** Dominación / Hegemonía / Régimen chequeadas por turno.
- [ ] **F5.4 — Derrota.** Golpe exitoso / revolución / conquista → fin de partida.
- [ ] **F5.5 — Agenda del líder.** Rasgos del líder que modifican eventos/opciones.

### B — Batallas  *(paralelo; hasta entonces auto-resuelto)*
- [ ] **B.1 — Auto-resolución.** Combate entre dos ejércitos por skill del general + composición → `EWLBattleResult`.
- [ ] **B.2 — Batalla táctica.** (futuro) Total War real.

---

## 📒 Registro (bitácora de tareas hechas)

<!-- Añade la más reciente arriba. Formato: fecha · tarea — resumen (archivos) -->
- **2026-07-01 · F1.1** — Modelo `FWLCharacter` + enums `EWLCharacterRole`/`EWLMilitaryRank` creados en
  `Source/WorldLeader/Core/WLCharacterTypes.h`. UHT procesa y el módulo compila+linkea (WorldLeaderEditor Development).
