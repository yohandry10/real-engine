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

**Las 4 arenas + el motor económico:**

- **GUERRA** — ejércitos liderados por generales (estilo Total War): reclutar en fuertes, marchar, batallas.
- **ALTO MANDO** — tu círculo: ascender, purgar, oposición, sobrevivir golpes.
- **DIPLOMACIA** — relaciones, tratados, embajadas, casus belli, declarar guerra.
- **INTRIGA** — espías, sabotaje, financiar golpes/rebeldes en el rival, propaganda.
- **ECONOMÍA (motor profundo — pilar clave, no un simple recurso).** Sistema realista con **causalidad**:
  producción por **sectores**, **bienes** y **cadenas**, **mercado** con precios dinámicos y shocks,
  **fisco** (impuestos/gasto/deuda), **comercio exterior** y **ciclo de crecimiento/recesión**. Es lo que
  todo lo demás gasta y estresa, y lo que puede crecer o hundir a tu nación. Ver pilar **FE**.

**Loop de turno:** ingresos/gastos → eventos & decisiones → órdenes (militar / interno / diplomacia)
→ fin de turno (IA, batallas, chequeo de golpe/revuelta).

**Metas (victoria):** Dominación (someter al rival) · Hegemonía (control económico/diplomático) ·
Régimen/Legado (mantente en el poder). **Derrota:** golpe, revolución o conquista.

**Reorg de la ventana GOBIERNO:** RESUMEN (dashboard) · **ECONOMÍA** (presupuesto, sectores, bienes,
comercio, PIB) · ALTO MANDO · POLÍTICA (interno, pequeño) · DIPLOMACIA (reemplaza NACION) · REGISTROS.
*NACION se elimina; su tabla de provincias se pliega en RESUMEN/ECONOMÍA cuando lleguen DIPLOMACIA (F3.6) y FE.*

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

## 🏦 FE — ECONOMÍA (pilar profundo, *paralelo*)

**Objetivo:** una economía **realista y con causalidad**, no un número plano. Hoy es una economía cerrada
(recursos a precio fijo + impuesto per cápita − upkeep de infraestructura, × orden público) que **NO** modela
comercio, mantenimiento de ejércitos, deuda, ni precios dinámicos. Esto lo convierte en un **motor por capas**:

> **provincias → SECTORES producen BIENES** (con trabajo + insumos) → **DEMANDA** (población · industria ·
> ejército · exportación) marca **superávit/déficit** → **MERCADO** fija **precios dinámicos** (+shocks) →
> **FISCO** (impuestos − gasto, deuda, PIB) → **CRECIMIENTO / RECESIÓN**. **Comercio y ayuda exterior**
> conectan economías (vía DIPLOMACIA F3).

Se construye **de abajo hacia el jugador, MVP primero**. Cada sub-fase es un incremento jugable. *Recomendado
intercalar **FE1 pronto**: tapa el hueco de "ejércitos gratis" y activa la tensión PODER⇄COSTE.*
Todo parametrizado en `FWLBalanceRules` / `Content/Data/` (nunca hardcodear balance).

### FE1 — Fisco y presupuesto  *(fundamentos + huecos críticos)*
- [ ] **FE1.1 — Mantenimiento de ejércitos por turno.** Cada ejército/guarnición drena tesoro cada mes
  (coste por unidad). *Hecho = reclutar y mantener ejércitos baja el balance mensual.*
- [ ] **FE1.2 — Palanca de impuestos.** Tasa ajustable por nación: subir = +ingreso, −orden público
  (curva tipo Laffer). *Hecho = mover el impuesto cambia ingreso y orden.*
- [ ] **FE1.3 — Presupuesto por categorías.** Gasto en militar / infraestructura / salarios / social + panel
  ECONOMÍA que los desglosa. *Hecho = se ve ingreso y gasto por categoría.*
- [ ] **FE1.4 — Deuda y déficit.** Tesoro negativo → interés mensual + límite de crédito + penalización.
  *Hecho = gastar de más genera deuda con interés.*
- [ ] **FE1.5 — PIB y crecimiento.** Métrica agregada de PIB + tasa de crecimiento mensual, en ECONOMÍA/RESUMEN.
  *Hecho = se ve el PIB y si sube o baja.*

### FE2 — Bienes, sectores y cadenas de producción  *(el corazón realista)*
- [ ] **FE2.1 — Catálogo de bienes (JSON).** Crudos (petróleo, gas, carbón, minerales, alimentos, café) y
  manufacturados (combustible, acero, bienes de consumo, armamento).
- [ ] **FE2.2 — Sectores por provincia.** Extracción / manufactura / servicios producen bienes usando trabajo.
- [ ] **FE2.3 — Cadenas de producción.** petróleo→refinería→combustible; hierro+carbón→acero→armas/maquinaria.
- [ ] **FE2.4 — Empleo y productividad.** La población trabaja en sectores; desempleo; el nivel de
  industrialización sube la productividad por trabajador.

### FE3 — Demanda, mercado y precios dinámicos
- [ ] **FE3.1 — Demanda.** Consumo de la población (según nivel de vida) + insumos de industria + suministro del ejército.
- [ ] **FE3.2 — Balance por bien.** Superávit/déficit y sus efectos: déficit de alimentos → −orden público;
  déficit de insumos → las fábricas subproducen.
- [ ] **FE3.3 — Precios dinámicos.** Precio por oferta/demanda (reemplaza los precios fijos de `FWLBalanceRules`).
- [ ] **FE3.4 — Shocks de mercado.** Shock del precio del petróleo, boom/crisis (evento; conecta con F5).

### FE4 — Comercio exterior  *(conecta con DIPLOMACIA F3)*
- [ ] **FE4.1 — Exportar/importar.** Vender superávit / comprar déficit en un mercado regional.
- [ ] **FE4.2 — Acuerdos comerciales.** Suben volumen e ingreso de **ambos** países (se firman en Diplomacia).
- [ ] **FE4.3 — Aranceles.** Ingreso + protección de la industria local, a costa de la relación.
- [ ] **FE4.4 — Embargos / sanciones.** Arma económica: cortan el comercio del rival (daño mutuo).
- [ ] **FE4.5 — Rutas comerciales.** Cortables en guerra (bloqueo naval, frontera cerrada) → escasez.

### FE5 — Finanzas avanzadas, ayuda y ciclo económico
- [ ] **FE5.1 — Bonos / préstamos.** Emitir deuda, calificación crediticia, riesgo de default, FMI.
- [ ] **FE5.2 — Inflación.** Por masa monetaria / escasez; erosiona ingreso y nivel de vida.
- [ ] **FE5.3 — Ayuda e inversión extranjera.** Aliados que subsidian/financian + FDI (empresas extranjeras
  construyen en tu país).
- [ ] **FE5.4 — Crecimiento vs recesión.** Motor con drivers ↑ (inversión, estabilidad, apertura comercial,
  tecnología, precios favorables) y ↓ (guerra, sanciones, shock, inestabilidad, deuda, corrupción) + ciclo/recesión.

### FE6 — Gobernanza económica  *(conecta con PERSONAJES)*
- [ ] **FE6.1 — Ministro de Economía.** Su competencia/corrupción mueve el ingreso y la eficiencia de recaudación.
- [ ] **FE6.2 — Corrupción sistémica.** Skim del tesoro + peor "facilidad de negocios" (menos inversión/crecimiento).
- [ ] **FE6.3 — Modernización/tecnología.** Multiplicador de productividad por nivel tecnológico (hook para tech futura).

---

## 📒 Registro (bitácora de tareas hechas)

<!-- Añade la más reciente arriba. Formato: fecha · tarea — resumen (archivos) -->
- **2026-07-01 · F1.1** — Modelo `FWLCharacter` + enums `EWLCharacterRole`/`EWLMilitaryRank` creados en
  `Source/WorldLeader/Core/WLCharacterTypes.h`. UHT procesa y el módulo compila+linkea (WorldLeaderEditor Development).
