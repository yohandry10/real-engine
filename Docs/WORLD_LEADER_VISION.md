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

- **Fase activa:** UIX + batalla visual. El backend local de gobierno/economia/militar ya compila y esta
  cubierto por tests; la ventana GOBIERNO opera los endpoints principales. El backend de gobierno ya no esta
  limitado a CO/VE: `AmericaLowDetail` se promueve a 38 naciones con capital backend, relaciones diplomaticas
  completas, IA economica no jugadora, gabinete/pool ministerial por pais y Gobierno P1/P2 real
  (agenda, 50 programas ministeriales, arbol de reformas, partidos, elecciones, perfiles politicos, patronazgo,
  medios, regiones, crisis encadenadas, Congreso/base politica, grupos sociales, capacidad estatal,
  memoria de crisis, calibracion e IA politica ampliada). La batalla tactica ya existe como
  backend, puede devolver resultado oficial a campaña, tiene IA tactica determinista basica y conserva
  unidades retiradas como reservas reorganizables; el movimiento estrategico tiene grafo/rutas testeables.
  La IA de campania ya tiene dificultad **Facil / Medio / Dificil** desde backend (`FWLBalanceRules`) y
  afecta economia, fisco, diplomacia, guerra, intriga y reclutamiento.
- **Próxima tarea (UIX/pulido):** (a) selector/preview de combate **auto-resolve vs tactico**,
  (b) vista tactica 3D con camara, seleccion y ordenes, (c) UIX de Gobierno P1/P2 real: agenda, programas,
  reformas, partidos, elecciones, personajes, patronazgo, medios, regiones, crisis, calibracion e IA, (d) diplomacia
  continental con filtros/busqueda, (e) UIX de contratacion ministerial con comparador de candidatos,
  (f) panel de **slots de edificios** en HUD de provincia, (g) popup modal de eventos + feed de noticias del mes,
  (h) accion FDI con selector, (i) playtest de calibracion 24-36 meses.
- **Backlog UIX separado:** `Docs/UIX_PENDING_TASKS.md`.
- **Última actualización:** 2026-07-02 (backend Gobierno P2 real implementado; UIX pendiente documentada; build OK + suite WorldLeader 55/55)

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
- [x] **F1.2 — Almacén de personajes.** Subsistema `UWLCharacterSubsystem` (GameInstance) con
  `TMap<FString,FWLCharacter>`: `CreateGeneral(iso)`, `GetCharacter(id)`, `GetGenerals(iso)`.
  *Hecho = compila + se crea un general por código.*
- [x] **F1.3 — Pool de nombres/rasgos (JSON).** `Content/Data/Characters/Characters.json`
  (nombres, apellidos, rasgos) + loader → `CreateGeneral` usa nombres variados.
  *Hecho = nombres reales, no "General 1".*
- [x] **F1.4 — General al reclutar.** Al completar reclutamiento en un fuerte, crear/asignar general;
  el ejército guarda su Id; el nombre del token pasa a "&lt;Rango&gt; &lt;Apellido&gt;".
  *Hecho = el ejército reclutado se llama por su general.*
- [~] **F1.5 — General en el panel del ejército.** Backend/nombre de token listos; falta UIX fina de ficha militar
  fuera de GOBIERNO con nombre, rango, skill y reasignación.
- [x] **F1.6 — Tab ALTO MANDO.** En `WLGovernmentWidget`, tab que lista generales
  (nombre, rango, skill, lealtad, ambición) con el patrón de tarjetas. *Hecho = se ven los generales del país.*
- [x] **F1.7 — Ascender / Dar de baja.** Botones en ALTO MANDO: ascender (sube rango) /
  dar de baja (retira). *Hecho = cambian el estado del general.*
- [x] **F1.8 — Experiencia por turno.** Generales ganan renombre al avanzar turnos / al combatir;
  al pasar un umbral, +1 rasgo o +skill. *Hecho = el renombre sube al avanzar turnos.*

### F2 — Poder interno & Golpes  *(nace el juego de supervivencia)*
**Objetivo:** los generales desleales y la oposición pueden derrocarte.

- [x] **F2.1 — Riesgo de golpe.** Cálculo por nación desde lealtad/ambición de generales + orden público; medidor en POLÍTICA.
- [x] **F2.2 — Intento de golpe.** En fin de turno, si el riesgo supera umbral, dispara golpe (resuelto por skill/lealtad/ejércitos leales).
- [x] **F2.3 — Oposición.** Entidad con fuerza/popularidad que crece con bajo orden público.
- [x] **F2.4 — Acciones internas.** Recompensar general (+lealtad, cuesta tesoro), purgar (elimina, -lealtad de otros), reprimir oposición.
- [x] **F2.5 — Orden público efectivo.** El orden público por provincia afecta ingreso y alimenta oposición/golpe.
- [x] **F2.6 — Panel POLÍTICA rehecho.** Medidor de golpe + oposición + acciones (reemplaza los "Fase futura").

### F3 — Diplomacia & Guerra  *(el juego internacional)*
- [x] **F3.1 — Relación entre países.** Opinión -100..100 por par de países + almacén.
- [x] **F3.2 — Líderes extranjeros.** Como `FWLCharacter` (Role=ForeignLeader) con personalidad.
- [x] **F3.3 — Estados de relación.** Paz / tensión / guerra + casus belli.
- [x] **F3.4 — Declarar guerra.** Acción CO↔VE que cambia estado y habilita ataques entre países.
- [x] **F3.5 — Tratados.** Comercio, no-agresión, alianza (IA acepta según opinión).
- [x] **F3.6 — Tab DIPLOMACIA (reemplaza NACION).** Lista de países, opinión, acciones.
  *Aquí se pliega la tabla de provincias en RESUMEN y se elimina el tab NACION.*

### F4 — Intriga exterior  *(la mano oculta)*
- [x] **F4.1 — Agentes/espías.** `FWLCharacter` (Role=Spy) + red de inteligencia por país.
- [x] **F4.2 — Sabotaje.** Daña economía/ejército rival, con riesgo de descubrimiento.
- [x] **F4.3 — Financiar golpe/rebeldes.** Sube el riesgo de golpe del rival (reusa F2).
- [x] **F4.4 — Propaganda.** Baja el orden público rival / sube el tuyo.
- [x] **F4.5 — Contraespionaje.** Incidentes de descubrimiento que afectan la relación.

### F5 — Eventos & Metas  *(el tejido y el cierre)*
- [x] **F5.1 — Motor de eventos.** Definición de evento (condición → opciones → efectos) + cargador JSON.
- [~] **F5.2 — Cola + popup.** Cola/backend y resolución listos; falta popup/modal dedicado y feed visual completo.
- [x] **F5.3 — Victoria.** Dominación / Hegemonía / Régimen chequeadas por turno.
- [x] **F5.4 — Derrota.** Golpe exitoso / revolución / conquista → fin de partida.
- [x] **F5.5 — Agenda del líder.** Rasgos del líder que modifican eventos/opciones.

### Contrato backend/frontend F1-F5

> Estado: backend compilado y cubierto por tests. Lo pendiente aquí es UIX, no datos hardcodeados en widgets.

**F1 Personajes & Generales — endpoint `UWLCharacterSubsystem`**
- Lecturas UI: `GetCharactersByNation`, `GetCharactersByRole`, `GetGenerals`, `GetCabinet`,
  `GetCabinetMinister`, `GetGovernmentStats`, `GetPoliticalCapital`, `GetAssignedGeneralForArmy`.
- Acciones UI: `AppointMinister`, `DismissMinister`, `CreateMinister`, `HireMinister`, `CreateGeneral`, `CreateAndAssignGeneralToArmy`,
  `AssignGeneralToArmy`, `PromoteGeneral`, `RetireCharacter`, `AddRenownToGeneral`,
  `AdjustCharacterLoyalty`, `AdjustPoliticalCapital`.
- Persistencia: `UWLLocalSaveGame.Characters` + `PoliticalCapital` (save local v10).
- Cobertura backend: cada nacion cargada tiene jefe de Estado/personaje diplomatico, general, oposicion, agente,
  cinco ministros asignados y al menos dos candidatos por cartera; saves viejos se restauran encima del roster
  generado para no perder paises nuevos.
- Gobierno P1 real: `UWLPoliticalSubsystem` expone agenda nacional, programas por ministerio, dinamica de
  gabinete, instituciones/Congreso, grupos sociales, capacidad estatal, memoria politica y plan IA de gobierno.
- UIX actual: ventana **GOBIERNO/ALTO MANDO** ya lista roster, gabinete, generales y acciones principales.
- Falta UIX fuera/debajo de GOBIERNO: selector/asignacion de general desde ficha militar del mapa, lista de
  candidatos contratables por ministerio, comparador de skill/lealtad/ambicion/popularidad/rasgos, coste de
  capital politico, agenda nacional, programas por cartera, riesgos de gabinete, Congreso, grupos sociales,
  capacidad estatal, memoria de crisis, plan IA y mejor feedback visual de renombre/ascenso en tokens o paneles
  de ejercito.

**F2 Poder interno & Golpes — endpoint `UWLPoliticalSubsystem`**
- Lecturas UI: `GetInternalPower`, `GetCampaignOutcome`.
- Acciones UI: `AttemptCoup`, `RewardGeneral`, `PurgeCharacter`, `RepressOpposition`.
- Tick backend: `ProcessPoliticalMonth` se llama desde `UWLCampaignGameInstance::WLAdvanceMonth` y desde el
  avance diario del `AWLCampaignPlayerController` solo cuando hay rollover de mes.
- Persistencia: `UWLLocalSaveGame.InternalPowerStates` + `CampaignOutcome` (save local v10).
- UIX actual: tab **POLITICA** ya muestra riesgo/oposicion y acciones principales.
- Falta UIX: feedback visual de revuelta/golpe en mapa, confirmaciones y registro historico mas legible.

**F3 Diplomacia & Guerra — endpoint `UWLPoliticalSubsystem`**
- Lecturas UI: `GetRelationsForNation`, `GetRelation`.
- Acciones UI: `SetRelationOpinion`, `AdjustRelationOpinion`, `DeclareWar`, `SignTreaty`, `BreakTreaty`.
- Cobertura backend: `UWLDataRegistry` promueve `Content/Data/Campaign3D/AmericaLowDetail/*.json` a naciones
  backend con capital sintetica cuando no existe dato detallado; `SeedRelations` crea relacion bilateral para
  cada par de naciones cargadas. Colombia ve toda America, no solo Venezuela.
- Integración backend: `UWLMilitarySubsystem::AutoResolveBattle` bloquea combate entre países si la relación
  no está en `EWLDiplomaticStatus::War`; declarar guerra habilita el auto-resolve.
- Persistencia: `UWLLocalSaveGame.DiplomaticRelations` (save local v10).
- UIX actual: tab **DIPLOMACIA** ya lista paises, relacion, tratados, guerra e intriga basica.
- Falta UIX: overlay/mapa diplomatico, confirmaciones, filtros/busqueda/ordenamiento por region/estado/tratado
  y explicacion visual de rutas/bloqueos para una lista continental.

**F4 Intriga exterior — endpoint `UWLPoliticalSubsystem`**
- Lecturas UI: `GetIntelligenceNetwork`.
- Acciones UI: `BuildSpyNetwork`, `RunSpyOperation` con `SabotageEconomy`, `SabotageArmy`, `FundCoup`,
  `Propaganda`, `CounterIntelligence`.
- Efectos backend: operaciones modifican red/exposición, orden público, oposición, riesgo/funding de golpe y
  opinión diplomática si son detectadas.
- Persistencia: `UWLLocalSaveGame.IntelligenceNetworks` (save local v10).
- UIX actual: acciones de intriga estan disponibles desde **DIPLOMACIA**.
- Falta UIX: selector dedicado de agente/objetivo, riesgo de exposicion mas claro y resultado persistente.

**F5 Eventos & Metas — endpoint `UWLPoliticalSubsystem`**
- Datos: `Content/Data/Political/PoliticalEvents.json`.
- Lecturas UI: `GetQueuedEvents`, `GetCampaignOutcome`, `GetLeaderAgendaTraits`.
- Acciones UI: `ResolveEvent`.
- Tick backend: `ProcessPoliticalMonth` evalúa eventos, agenda del líder, golpes y outcome; `CheckCampaignOutcome`
  cubre dominación y golpe exitoso/revolución como derrota.
- Persistencia: `UWLLocalSaveGame.PoliticalEvents` + `CampaignOutcome` (save local v10).
- UIX actual: eventos y opciones ya aparecen en **POLITICA/REGISTROS**.
- Falta UIX: popup/modal de evento, feed de noticias del mes y registro historico de decisiones.

### Contrato backend/frontend IA de campania

> Estado: backend listo. No requiere frontend para actuar, pero falta UIX para escoger/mostrar dificultad.

- Endpoint backend: `UWLBalanceSubsystem::SetAIDifficulty`, `GetAIDifficulty`, `SetRuntimeRules`.
- Dato persistido: `UWLLocalSaveGame.AIDifficulty` (save local v10; default/fallback `Medium`).
- Cobertura backend: la IA economica corre para todas las naciones no jugadoras cargadas; con America activa
  ya no presupone que solo existe CO/VE.
- Niveles:
  - **Facil:** IA economica conserva mas reserva, exige mejor orden publico/retorno, evita guerras salvo opinion
    muy baja y ventaja amplia, espia menos y recluta solo cuando esta claramente por detras.
  - **Medio:** conserva el comportamiento anterior como baseline.
  - **Dificil:** puede construir mas por mes, acepta retornos mas largos, baja umbrales diplomaticos para tratados,
    declara con menos ventaja militar, tolera mas exposicion de espionaje y recluta de forma preventiva.
- Tests esperados: `WorldLeader.Balance.AIDifficultyTuning`,
  `WorldLeader.EconomyAI.DifficultyCadence`, `WorldLeader.Politics.StrategicAIDifficultyWarPosture`.
- Falta UIX: selector de dificultad al iniciar/cargar campania, indicador visible en GOBIERNO/ajustes y texto de
  explicacion de impactos. No duplicar reglas en UI; leer desde `UWLBalanceSubsystem`.

### Contrato backend/frontend edificios provinciales

> Estado: backend compilado y cubierto por tests. Falta UIX para operar slots/niveles desde HUD.

- Datos: `Content/Data/Buildings/Buildings.json` define los 9 slots del GDD (`Economic`, `Industrial`,
  `Military`, `Naval`, `Air`, `Tech`, `Financial`, `Infrastructure`, `Defensive`) con `max_level` 1-5,
  coste, multiplicador de upgrade, upkeep mensual y efectos.
- Lecturas UI: `GetProvinceBuildings`, `GetProvinceBuildingLevel`, `GetProvinceBuildingUpgradeCost`,
  `GetProvinceBuildingEffects`, `IsBuildingSupportedInProvince`.
- Acciones UI/backend: `BuildBuilding`, `UpgradeBuilding`.
- Efectos backend: producción/PIB, ingreso provincial, mantenimiento, orden público mensual, capacidad militar
  abstracta, capacidad naval/aérea/tecnológica y bonus defensivo leído por `UWLMilitarySubsystem::AutoResolveBattle`.
- IA: `RunEconomicAI` puede construir un slot vacío o mejorar un edificio existente si el retorno mensual lo justifica.
- Persistencia: `UWLLocalSaveGame.ProvinceBuildings.BuildingLevels` + save local v10; saves viejos sin niveles
  restauran edificios como nivel 1.
- Falta UIX: panel de slots reales, nivel, coste de upgrade, mantenimiento, preview de efectos y botón construir/mejorar.

### P2/P3 profundidad backend realista

> Gobierno P2 local ya esta implementado como backend. Lo pendiente en esta seccion ya no es el nucleo P2 de
> gobierno, sino UIX, contenido data-driven especifico y capas P3 para que la simulacion gane sutileza.
> Si la pregunta es "backend del proyecto entero", tambien faltan backend online Rust/Axum, Dedicated Server
> PvP, datos mundiales completos y persistencia online. Eso no es frontend.

- **IA estrategica fuera de gobierno:** profundizar preparacion de guerra, retirada por coste esperado,
  aseguramiento de insumos y bloques con horizonte de varios meses.
- **Comercio:** hoy hay rutas diplomaticas agregadas; profundidad P2 seria capacidad por puertos/carreteras,
  coste logistico, bloqueo naval/aereo, dependencia por bien critico y sustitucion parcial de insumos.
- **Ministerios/personajes:** backend P2 ya cubre programas ampliados, perfiles, facciones, patronos/rivales,
  escandalos, patronazgo y sucesion. P3 seria biografias y eventos handcrafted por pais/rasgo.
- **Intriga:** hoy red/exposicion/operaciones funcionan; P2 seria niebla de guerra, contraoperaciones reactivas,
  atribucion incierta y cadenas de represalia diplomaticas.
- **Eventos:** backend P2 ya cubre crisis encadenadas persistentes; falta autorado data-driven amplio y UIX.
- **IA tactica:** hoy es determinista basica; P2 seria roles por unidad, flanqueo, reserva, retirada ordenada,
  preferencia por terreno/objetivos y perfil distinto por dificultad.
- **Balance:** todos los P2 deben seguir entrando por `FWLBalanceRules`/`Content/Data`, con tests que prueben
  conducta observable, no solo campos nuevos.

### Contrato backend/frontend Gobierno P1 real

> Estado: backend compilado. Falta UIX para operar/visualizar, no reglas en widgets.

- Lecturas UI: `GetGovernmentAgenda`, `GetAvailableMinistryPrograms`, `GetActiveMinistryPrograms`,
  `GetCabinetDynamics`, `GetInstitutionalPower`, `GetPublicGroups`, `GetStateCapacity`, `GetPoliticalMemory`,
  `GetGovernmentAIPlan`.
- Acciones UI/backend: `SetGovernmentAgenda`, `StartMinistryProgram`, `PassGovernmentReform`.
- Tick backend: `ProcessPoliticalMonth` ejecuta agenda, programas, gabinete vivo, instituciones, grupos sociales,
  capacidad estatal, memoria/cadenas de crisis e IA politica de naciones no jugadoras.
- Persistencia: save local v12 conserva `GovernmentAgendas`, `MinistryPrograms`, `CabinetDynamics`,
  `InstitutionalPower`, `PublicGroupSupport`, `StateCapacity`, `PoliticalMemory`, `GovernmentAIPlans`.
- Falta UIX: selector de agenda, panel por ministerio, progreso/riesgo de programas, votaciones de reforma,
  tablero de grupos sociales, medidor de capacidad estatal, timeline de memoria politica y plan visible de IA.

### Contrato backend/frontend Gobierno P2 real

> Estado: backend compilado y suite `WorldLeader` 55/55. Falta UIX; detalle completo en `Docs/UIX_PENDING_TASKS.md`.

- Lecturas UI: `GetAvailablePolicyReforms`, `GetActivePolicyReforms`, `GetPoliticalParties`, `GetElectionState`,
  `GetCharacterPoliticalProfiles`, `GetCharacterPoliticalProfile`, `GetPatronageState`, `GetMediaPublicOpinion`,
  `GetRegionGovernors`, `GetActiveCrisisChains`, `GetGovernmentCalibration`.
- Acciones UI/backend: `EnactPolicyReform`, `NegotiatePartySupport`, `HoldPartyInternalElection`,
  `MakeCampaignPromise`, `UsePatronage`, `RunMediaAction`, `RunRegionPolicy`.
- Tick backend: `ProcessPoliticalMonth` aplica reformas largas, partidos/coalicion, ciclo electoral, perfiles
  politicos, patronazgo, medios/opinion, gobernadores/regiones, crisis persistentes y calibracion.
- Contenido backend: 24 reformas en 12 areas y 50 programas ministeriales en cinco carteras.
- Persistencia: save local v12 agrega `ActivePolicyReforms`, `PoliticalParties`, `ElectionStates`,
  `CharacterPoliticalProfiles`, `PatronageStates`, `MediaStates`, `RegionGovernors`, `CrisisChains`,
  `GovernmentCalibration`.
- Falta UIX: arbol de reformas, Congreso por bancadas, panel electoral, perfiles/personajes, patronazgo,
  medios, regiones/gobernadores, crisis encadenadas, IA politica visible y panel de calibracion.

### B — Batallas  *(paralelo; hasta entonces auto-resuelto)*
- [x] **B.1 — Auto-resolución.** Combate por composición, skill del general parametrizado por balance,
  terreno, defensa provincial, guerra diplomática, bajas, ocupación y renombre posterior.
- [~] **B.2 — Batalla táctica.** Base backend iniciada; falta la batalla visual Total War real.
- [x] **B.2a — Backend táctico determinista.** Estado de batalla, unidades desde `FWLArmy`, órdenes de
  mover/atacar, daño, moral, captura de objetivo y victoria/derrota sin UI.
- [ ] **B.2b — Vista/cámara/selección táctica.** Mapa 3D de batalla, cámara tipo Total War y selección de unidades.
- [x] **B.2c — Conexión campaña→batalla→resultado.** Entrar desde campaña, simular/terminar y devolver
  bajas, ocupación y renombre a los `FWLArmy` oficiales.
- [x] **B.2d — IA táctica.** Comportamiento determinista básico para mover, atacar, capturar y retirarse sin input humano.
- [x] **B.2e — Retirada/reorganización operacional.** Que unidades en retirada puedan volver como reservas,
  prisioneros o pérdidas diferidas en vez de desaparecer siempre como baja efectiva.

### Contrato backend/frontend B.1

> Estado: backend compilado y cubierto por tests. Falta UIX solo para explicar el desglose de poder en la ficha de batalla.

- Lecturas/acción UI: `UWLMilitarySubsystem::AutoResolveBattle` devuelve un reporte con poder de ataque/defensa,
  multiplicador de general, terreno y defensas.
- Balance: `FWLBalanceRules.GeneralSkillCombatEffectAtMax` controla el impacto máximo del skill del general
  (`0.25` por defecto: skill 100 = `x1.25`, skill 0 = `x0.75`).
- Efectos backend: requiere guerra diplomática, valida adyacencia, aplica bajas, ocupación si corresponde y
  renombre para generales asignados.
- Falta UIX: preview/desglose antes de combatir y registro de batalla legible para el jugador.

### Contrato backend/frontend B.2a

> Estado: backend táctico inicial cubierto por tests. Falta UI/3D.

- Lecturas UI: `GetTacticalBattleState` devuelve `FWLTacticalBattleState` con unidades, objetivos, tiempo,
  resultado, ganador y enlace `AttackerArmyId`/`DefenderArmyId`.
- Acciones UI/backend: `StartTacticalBattleFromArmies`, `IssueMoveOrder`, `IssueAttackOrder`,
  `AdvanceTacticalBattle`.
- Balance: reglas `TacticalMoveSpeedUnitsPerSecond`, `TacticalAttackRangeUnits`,
  `TacticalDamagePerAttackPerSecond`, `TacticalDefenseMitigationPerPoint`,
  `TacticalMoraleDamagePerHealth`, `TacticalRoutMoraleThreshold` y `TacticalObjectiveCaptureSeconds`.
- Efectos backend: unidades tácticas avanzan, atacan, pierden salud/moral, entran en retirada, capturan
  objetivos y cierran la batalla con victoria/derrota/empate.
- Falta UIX: render táctico, cámara, selección, órdenes con mouse, barra de resultado y feedback de retirada.

### Contrato backend/frontend B.2c

> Estado: backend compilado y cubierto por tests. No requiere UI para producir resultado oficial de campaña.

- Acciones UI/backend: `UWLMilitarySubsystem::StartTacticalBattle` valida guerra diplomática, adyacencia y
  ejércitos reales antes de crear una `FWLTacticalBattleState`; `ApplyTacticalBattleResult` aplica una batalla
  cerrada al estado estratégico.
- Efectos backend: las unidades tácticas destruidas salen de la composición del `FWLArmy`; las retiradas pasan
  a `RecoveringUnits` por B.2e; victoria atacante con defensor sin fuerza efectiva ocupa provincia y cambia
  controlador; victoria/derrota otorga renombre a generales asignados.
- Falta UIX/sistema posterior: elección auto-resolve vs táctica, pantalla de carga/resultado, log legible y
  visualización de reservas/retirada.

### Contrato backend/frontend B.2d

> Estado: backend compilado y cubierto por tests. No requiere UI para que una facción actúe en batalla táctica.

- Acciones UI/backend: `SetTacticalAIControl(BattleId, OwnerIso, bEnabled)` marca qué país usa IA en una
  `FWLTacticalBattleState`.
- Efectos backend: `AdvanceTacticalBattle` emite órdenes automáticas para unidades IA: mantiene ataques
  válidos, ataca al enemigo efectivo más cercano si está en rango, avanza a objetivos si no puede disparar,
  y las unidades rotas se retiran hacia su lado del campo.
- Falta UIX: toggle/estado visible de control IA y feedback visual de órdenes automáticas.

### Contrato backend/frontend B.2e

> Estado: backend compilado y cubierto por tests. Falta UIX para explicar reservas, retirada y reorganizacion.

- Lecturas UI: `FWLArmy::RecoveringUnits` expone unidades desorganizadas que siguen existiendo pero no cuentan
  como fuerza efectiva hasta reorganizarse.
- Acciones UI/backend: `UWLMilitarySubsystem::ReorganizeArmy(ArmyId, MaxUnits, OutMessage)` devuelve reservas
  al ejército activo; `ApplyTacticalBattleResult` reconcilia destruidas vs retiradas y mueve ejércitos derrotados
  a una provincia aliada si hay ruta de retirada.
- Efectos backend: un ejército con solo `RecoveringUnits` no aporta ataque, no se borra del snapshot y puede
  volver al frente por reorganización; si no hay provincia aliada de retirada, las unidades atrapadas se pierden.
- Falta UIX: mostrar reservas/desorganizacion en ficha de ejército, botón de reorganizar, resultado de retirada
  en el log de batalla y explicación visual cuando una fuerza queda sin ataque efectivo.

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
- [x] **FE1.1 — Mantenimiento de ejércitos por turno.** Cada ejército/guarnición drena tesoro cada mes
  (coste por unidad). *Hecho = reclutar y mantener ejércitos baja el balance mensual.*
  ✅ Verificado runtime: CO 34.200 efectivos → 11.970/mes (balance 51.470→39.500); VE 28.800 → 10.080/mes.
- [x] **FE1.2 — Palanca de impuestos.** Tasa ajustable por nación: subir = +ingreso, −orden público
  (curva tipo Laffer). *Hecho = mover el impuesto cambia ingreso y orden.*
- [x] **FE1.3 — Presupuesto por categorías.** Gasto en militar / infraestructura / salarios / social + panel
  ECONOMÍA que los desglosa. *Hecho = se ve ingreso y gasto por categoría.*
- [x] **FE1.4 — Deuda y déficit.** Tesoro negativo → interés mensual + límite de crédito + penalización.
  *Hecho = gastar de más genera deuda con interés.*
- [x] **FE1.5 — PIB y crecimiento.** Métrica agregada de PIB + tasa de crecimiento mensual, en ECONOMÍA/RESUMEN.
  *Hecho = se ve el PIB y si sube o baja.*

### FE2 — Bienes, sectores y cadenas de producción  *(el corazón realista)*
- [x] **FE2.1 — Catálogo de bienes (JSON).** Crudos (petróleo, gas, carbón, minerales, alimentos, café) y
  manufacturados (combustible, acero, bienes de consumo, armamento).
- [x] **FE2.2 — Sectores por provincia.** Extracción / manufactura / servicios producen bienes usando trabajo.
- [x] **FE2.3 — Cadenas de producción.** petróleo→refinería→combustible; hierro+carbón→acero→armas/maquinaria.
- [x] **FE2.4 — Empleo y productividad.** La población trabaja en sectores; desempleo; el nivel de
  industrialización sube la productividad por trabajador.

### FE3 — Demanda, mercado y precios dinámicos
- [x] **FE3.1 — Demanda.** Consumo de la población (según nivel de vida) + insumos de industria + suministro del ejército.
- [x] **FE3.2 — Balance por bien.** Superávit/déficit y sus efectos: déficit de alimentos → −orden público;
  déficit de insumos → las fábricas subproducen.
- [x] **FE3.3 — Precios dinámicos.** Precio por oferta/demanda (reemplaza los precios fijos de `FWLBalanceRules`).
- [x] **FE3.4 — Shocks de mercado.** Shock del precio del petróleo, boom/crisis (evento; conecta con F5).

### Contrato backend/frontend FE3.4

> Estado: backend compilado y cubierto por tests. Falta UIX para mostrar shocks activos y sus impactos.

- Lecturas UI: `GetActiveMarketShocks`, `GetMarketShockMultiplier`, `GetNationGoodMarketBalance`,
  `GetNationInflationRate`, `GetNationEconomicCycleLabel`.
- Acciones UI/backend: `ApplyMarketShock`, `ClearMarketShock`.
- Efectos backend: `FWLMarketShockState` multiplica precios por bien o `*` global; modifica valor de producción,
  coste de importación, ingreso por exportación, inflación, PIB y ciclo económico.
- Integración F5: `FWLPoliticalEventOption` puede declarar `market_shock_good_id`,
  `market_shock_price_multiplier`, `market_shock_duration_months` y `market_shock_title`; `ResolveEvent`
  dispara el shock.
- Persistencia: `UWLLocalSaveGame.ActiveMarketShocks` + save local v9; al avanzar mes baja
  `RemainingMonths` y expira automáticamente.
- Falta UIX: panel/log de shocks activos, bien afectado, duración restante, multiplicador e impacto en inflación/comercio.

### FE4 — Comercio exterior  *(conecta con DIPLOMACIA F3)*
- [x] **FE4.1 — Exportar/importar.** Vender superávit / comprar déficit en un mercado regional.
- [x] **FE4.2 — Acuerdos comerciales.** Suben volumen e ingreso de **ambos** países (se firman en Diplomacia).
- [x] **FE4.3 — Aranceles.** Ingreso + protección de la industria local, a costa de la relación.
- [x] **FE4.4 — Embargos / sanciones.** Arma económica: cortan el comercio del rival (daño mutuo).
- [x] **FE4.5 — Rutas comerciales.** Cortables en guerra (bloqueo naval, frontera cerrada) → escasez.

### Contrato backend/frontend FE4.2-FE4.5

> Estado: backend compilado y cubierto por tests. Falta UIX para operar y leer esta capa desde ECONOMIA/DIPLOMACIA.

- Lecturas UI: `GetTradeRoutesForNation`, `GetTradeRouteBetween`, `GetTariffRate`,
  `GetNationGoodMarketBalance`, `GetNationBudget`.
- Acciones UI/backend: `SignTreaty(TradeAgreement)`, `SignTreaty(Embargo)`, `BreakTreaty`,
  `SetNationTariffRate`.
- Efectos backend: acuerdos comerciales suben acceso/volumen bilateral; aranceles reducen volumen importable,
  generan `TariffIncome` y penalizan relación; embargos/sanciones cierran la ruta regional; guerra prioriza
  cierre por frontera/bloqueo; el mercado global sigue como fallback cuando la región no alcanza.
- Persistencia: `FWLNationTreasurySave.TariffRatePercent` + `UWLLocalSaveGame.DiplomaticRelations`
  en save local v9.
- Falta UIX: panel de comercio exterior con rutas por país, estado de ruta, acuerdos/embargos, slider o stepper
  de arancel, preview de impacto en importaciones/exportaciones, presupuesto y relación diplomática.

### FE5 — Finanzas avanzadas, ayuda y ciclo económico
- [x] **FE5.1 — Bonos / préstamos.** Emitir deuda, calificación crediticia, riesgo de default, FMI.
- [x] **FE5.2 — Inflación.** Por masa monetaria / escasez; erosiona ingreso y nivel de vida.
- [x] **FE5.3 — Ayuda e inversión extranjera.** Aliados que subsidian/financian + FDI (empresas extranjeras
  construyen en tu país).
- [x] **FE5.4 — Crecimiento vs recesión.** Motor con drivers ↑ (inversión, estabilidad, apertura comercial,
  tecnología, precios favorables) y ↓ (guerra, sanciones, shock, inestabilidad, deuda, corrupción) + ciclo/recesión.

### Contrato backend/frontend FE5.1/FE5.3

> Estado: backend compilado y cubierto por tests. Falta UIX para operar bonos, deuda externa, FMI, ayuda y FDI desde ECONOMIA/DIPLOMACIA.

- Lecturas UI: `GetFinancialProfile`, `GetFinancialInstrumentsForNation`, `GetForeignSupportForNation`,
  `GetNationBudget`.
- Acciones UI/backend: `IssueBond`, `RegisterBilateralLoan`, `RequestIMFProgram`, `MarkDebtDefault`,
  `GrantForeignAid`, `StartForeignInvestment`.
- Efectos backend: bonos, préstamos y FMI suman tesoro y servicio de deuda mensual; el perfil financiero expone
  rating, deuda, crédito disponible y riesgo/default; el default marca instrumentos y baja orden público; la ayuda
  entra como `ForeignAidIncome`; la FDI se expone como `ForeignInvestmentInflow` y construye el edificio objetivo
  al completarse.
- Persistencia: `UWLLocalSaveGame.FinancialInstruments` + `ForeignSupportStates` en save local v9.
- Falta UIX: panel de rating/deuda/default/FMI, acciones de ayuda/FDI, preview de impacto presupuestario,
  historial de instrumentos y apoyos externos.

### FE6 — Gobernanza económica  *(conecta con PERSONAJES)*
- [x] **FE6.1 — Ministro de Economía.** Su competencia/corrupción mueve el ingreso y la eficiencia de recaudación.
- [x] **FE6.2 — Corrupción sistémica.** Skim del tesoro + peor "facilidad de negocios" (menos inversión/crecimiento).
- [x] **FE6.3 — Modernización/tecnología.** Multiplicador de productividad por nivel tecnológico (hook para tech futura).

### Contrato backend/frontend FE6

> Estado: backend compilado y cubierto por tests. Falta UIX para mostrar y operar esta capa desde ECONOMIA/GABINETE.

- Lecturas UI: `GetEconomicGovernanceStats`, `GetNationSystemicCorruption`, `GetNationTechnologyLevel`,
  `GetNationBudget`.
- Datos fuente: gabinete/personajes desde `UWLCharacterSubsystem`; tecnologia agregada desde edificios con
  `BonusTechnology`.
- Efectos backend: el ministro de Economia y sus rasgos ajustan eficiencia fiscal y corrupcion sistemica;
  la corrupcion agrega `CorruptionLoss` al presupuesto y penaliza productividad; la tecnologia aumenta
  productividad nacional y afecta produccion/ingreso/PIB.
- Falta UIX: panel de gobernanza economica con ministro actual, skill/rasgos, corrupcion, tecnologia,
  eficiencia fiscal, perdida por corrupcion y preview de impacto economico.

---

## 📒 Registro (bitácora de tareas hechas)

<!-- Añade la más reciente arriba. Formato: fecha · tarea — resumen (archivos) -->
- **2026-07-02 · Gobierno P2 real backend** — `WLPoliticalTypes.h` y `UWLPoliticalSubsystem` agregan arbol de
  leyes/reformas, partidos/ideologias, ciclo electoral/legitimidad, 50 programas ministeriales, perfiles
  politicos de personajes, patronazgo/corrupcion politica, medios/opinion publica, regiones/gobernadores,
  crisis encadenadas persistentes, IA politica ampliada y metricas de calibracion. `WLLocalSaveGame` sube a
  v12 y persiste `ActivePolicyReforms`, `PoliticalParties`, `ElectionStates`, `CharacterPoliticalProfiles`,
  `PatronageStates`, `MediaStates`, `RegionGovernors`, `CrisisChains` y `GovernmentCalibration`. Test nuevo
  `WorldLeader.Government.P2.RealPoliticsSystems`; build `WorldLeaderEditor Win64 Development` OK y suite
  `Automation RunTests WorldLeader` 55/55 (`EXIT CODE: 0`). Falta UIX completa documentada en `Docs/UIX_PENDING_TASKS.md`.
- **2026-07-02 · Gobierno P1 real backend validado** — `UWLPoliticalSubsystem` ahora modela agenda nacional,
  programas por ministerio, gabinete vivo, Congreso/base politica, grupos sociales, capacidad estatal, memoria
  de crisis y planificador IA de gobierno para las naciones cargadas; los programas afectan tesoro, capital
  politico, diplomacia, orden, corrupcion, reclutamiento, industria e intriga sin depender de UIX. `WLLocalSaveGame`
  sube a version 11 y guarda/restaura agenda, programas, dinamica de gabinete, instituciones, grupos sociales,
  capacidad estatal, memoria politica y planes IA. Tests nuevos: `WorldLeader.Government.RealGovernment.AgendaProgramsState`,
  `WorldLeader.Government.RealGovernment.MemoryChainsAndAI`; roundtrip actualizado en `WorldLeader.Save.LocalCampaignRoundTrip`.
  Validacion: build `WorldLeaderEditor Win64 Development` OK + suite `Automation RunTests WorldLeader` 55/55
  (`EXIT CODE: 0`). No se abrio Standalone. Falta UIX documentada: selector de agenda, paneles por ministerio,
  progreso/riesgo de programas, Congreso/base politica, grupos sociales, capacidad estatal, memoria y objetivo IA visible.
- **2026-07-02 · Auditoria backend senior y alcance real** — Verificado contra roadmap/codigo/datos: el
  backend local de gobierno/campaña para la vertical slice CO/VE esta en P0/P1, con IA de campaña por dificultad
  (`UWLBalanceSubsystem`/`FWLBalanceRules`) y contratos backend/frontend documentados. No equivale a "100% del
  backend del proyecto entero": faltan backend online Rust/Axum, Dedicated Server PvP, datos mundiales completos
  y capas P2 de realismo (planes IA, comercio logistico, agendas/rivalidades, intriga con incertidumbre,
  eventos encadenados e IA tactica por roles). Build `WorldLeaderEditor Win64 Development` OK y suite
  `Automation RunTests WorldLeader` 48/48. No se abrio Standalone.
- **2026-07-02 · Grafo de rutas de campaña backend** — `FWLCampaignRouteGraph` extrae el pathfinding
  de carreteras a lógica core testeable: aristas bidireccionales, BFS de ruta corta, alcanzables filtrados
  y conteo de componentes. `AWLCampaign3DView` usa el helper para destinos/rutas, dejando la vista como
  consumidora del backend. Tests nuevos `WorldLeader.Campaign.RouteGraph.ShortestPath` y
  `WorldLeader.Campaign.RouteGraph.Filters`; build OK y suite `Automation RunTests WorldLeader` 43/43.
- **2026-07-02 · B.2e retirada/reorganización operacional backend** — `FWLArmy` conserva
  `RecoveringUnits`; `UWLMilitarySubsystem::ApplyTacticalBattleResult` separa destruidas de retiradas,
  mueve fuerzas derrotadas a una provincia aliada cuando hay ruta y captura las reservas si quedan atrapadas;
  `ReorganizeArmy` devuelve reservas al ejército activo y el snapshot conserva ejércitos sin fuerza efectiva.
  Test nuevo `WorldLeader.Military.OperationalRecovery`; build OK y suite `Automation RunTests WorldLeader` 41/41.
- **2026-07-02 · B.2d IA táctica backend** — `FWLTacticalBattleState` expone `AIControlledOwnerIsos`;
  `UWLTacticalBattleSubsystem::SetTacticalAIControl` activa/desactiva IA por país; `AdvanceTacticalBattle`
  ahora genera órdenes deterministas para unidades IA antes de resolver movimiento/daño/captura: ataca al
  enemigo efectivo más cercano, avanza a objetivos si no puede disparar y retira unidades rotas hacia su lado
  del campo. Test nuevo `WorldLeader.Battle.TacticalAI`; build OK y suite `WorldLeader.Battle` 4/4.
- **2026-07-02 · B.2c campaña→batalla táctica→resultado oficial** — `FWLTacticalBattleState` guarda
  `AttackerArmyId`/`DefenderArmyId`; `UWLMilitarySubsystem::StartTacticalBattle` inicia batallas tácticas
  desde `FWLArmy` reales con las mismas reglas de guerra/adyacencia del auto-resolve; `ApplyTacticalBattleResult`
  reconcilia unidades efectivas, elimina fuerzas derrotadas, ocupa provincia si corresponde y otorga renombre.
  Test nuevo `WorldLeader.Battle.TacticalCampaignResult`; build `WorldLeaderEditor Win64 Development` OK y
  suite `Automation RunTests WorldLeader` 39/39.
- **2026-07-02 · B.1 auto-resolve parametrizado por skill del general** — `FWLBalanceRules` expone
  `GeneralSkillCombatEffectAtMax`; `UWLMilitaryLibrary::GeneralSkillCombatMultiplier` centraliza la fórmula;
  `UWLMilitarySubsystem::AutoResolveBattle` aplica el multiplicador al poder efectivo y lo reporta para UI.
  Tests: `WorldLeader.Military.ResolveBattle`, `WorldLeader.Military.GeneralSkillAutoResolve`,
  `WorldLeader.Balance.SanitizeRules`.
- **2026-07-02 · B.2a backend táctico determinista** — `FWLTacticalBattleState`/`FWLTacticalUnitState`
  modelan batalla táctica sin UI; `UWLTacticalBattleSubsystem` inicia batallas desde `FWLArmy`, recibe órdenes
  de movimiento/ataque, avanza daño/moral/captura y declara ganador. Balance táctico agregado a
  `FWLBalanceRules`. Tests: `WorldLeader.Battle.TacticalBackend`, `WorldLeader.Battle.TacticalMoveOrder`,
  `WorldLeader.Balance.SanitizeRules`.
- **2026-07-02 · Mapa↔ejércitos reales + IA estratégica + victorias/fin de partida** — **(1)** Los tokens
  de ejército del mapa ya SON ejércitos reales: `FWLArmy.SourceBaseId` liga token↔backend;
  `SyncArmyFromGarrison` crea/actualiza el `FWLArmy` (tipos de reclutamiento mapeados a Units.json:
  mbt/ifv/apc→tank, heli/aircraft/ship→drone) con **general nombrado automático (F1.4)**; el token del mapa
  se llama por su general y muestra su skill/renombre (F1.5); al moverse, la provincia del ejército real
  sigue al token (`SetArmyProvince`; la capa visual es la autoridad de posición). **(2)** IA estratégica
  mensual (`RunStrategicAIForNation`): sube impuestos en déficit, ajusta aranceles al balance comercial,
  firma comercio/no-agresión/alianza según opinión, declara la guerra con opinión ≤−60 y superioridad
  militar, pide la paz si pierde, espía a su peor relación (red → financiar golpe/propaganda alternos) y
  recluta si va por detrás (HQ sintético `ISO-AI-HQ`). **(3)** Victorias **Hegemonía** (≥65% del PIB
  continental, `HegemonyGDPShare`) y **Régimen** (sobrevivir 120 meses, `RegimeVictoryMonths`) + fin de
  partida REAL: banner central en el HUD y el avance de tiempo se bloquea. Test `Politics.MakePeaceEndsWar`.
  Suite 35/35. Archivos: `WLGameTypes.h`, `WLMilitarySubsystem.h/.cpp`, `WLCampaign3DViewForces.cpp`,
  `WLCampaign3DViewMovement.cpp`, `WLPoliticalSubsystem.h/.cpp`, `WLBalanceTypes.h`,
  `WLCampaignPlayerController.cpp`, `WLCampaignHUD.cpp`, `WLPoliticalTests.cpp`.
- **2026-07-02 · Fase 3 auditoría: gabinete con efectos reales + espionaje escalado** — Los 5 ministros ya
  importan (factor = (skill−50)/50, −1..+1; vacante = neutro; un inepto PERJUDICA): **Defensa** ±15% upkeep
  militar (`DefenseMinisterUpkeepEffect`), **Interior** ±2 orden público/mes por provincia, **Exterior** ±2
  opinión/mes con cada país, **Inteligencia** ±20 skill efectivo de espías; Economía ya existía (FE6). Los
  efectos de espionaje ahora ESCALAN con `SuccessScore` (0.35x–1.25x; antes "éxito/limitado" era texto). Las
  tarjetas del GABINETE en ALTO MANDO muestran el efecto real de cada cartera (verde/rojo). Parámetros en
  `FWLBalanceRules` (categoría Cabinet). Test `Government.MinisterEffects` (destituir a Defensa sube el
  upkeep). Suite 34/34. Archivos: `WLBalanceTypes.h`, `WLCharacterSubsystem.h/.cpp`,
  `WLStrategicTickSubsystem.cpp`, `WLPoliticalSubsystem.h/.cpp`, `WLGovernmentWidget.cpp`,
  `WLCharacterTests.cpp`.
- **2026-07-02 · Gobierno America backend cerrado** — `UWLDataRegistry` promueve
  `Content/Data/Campaign3D/AmericaLowDetail/*.json` a 38 naciones backend con capital sintetica para las que
  no tienen provincia detallada; usa la ciudad capital visual para coordenadas/puerto cuando existe.
  `UWLCharacterSubsystem` genera para cada nacion jefe de Estado, general, oposicion, agente y gabinete de cinco
  ministerios con pool de candidatos por cartera; agrega `CreateMinister` y `HireMinister` para contratar y
  nombrar sin UI. `UWLPoliticalSubsystem` ya crea relaciones bilaterales completas entre todas las naciones
  cargadas; la IA economica actua para todas las no jugadoras. Tests nuevos/actualizados:
  `WorldLeader.Data.AmericaDiplomacyNations`, `WorldLeader.Government.Characters.AmericaCabinetCoverage`,
  `WorldLeader.Government.Characters.AmericaCabinetHiring`, `WorldLeader.Politics.F3.AmericaDiplomacyCoverage`,
  `WorldLeader.EconomyAI.BuildsForNonPlayerOnly`, `WorldLeader.EconomyAI.MonthlyTickRequiresActiveCampaign`.
  Validacion: build `WorldLeaderEditor Win64 Development` OK + suite `Automation RunTests WorldLeader` 52/52.
  Falta UIX documentado: filtros/busqueda para diplomacia continental y lista/comparador/confirmacion de
  contratacion ministerial.
- **2026-07-02 · Auditoría de gameplay (fases 1-2 + parte de 3-4)** — Revisión UI→backend→efecto de cada
  mecánica. Corregido: **(1) escala temporal** — AVANZAR DÍA aplicaba un MES entero de economía y finanzas por
  día (30x; un bono de 24 meses se amortizaba en 24 días); ahora el día aplica 1/30 del balance al tesoro
  (`ApplyDailyEconomy`) y finanzas/provincias/PIB/shocks/IA corren solo al cerrar mes; HUD pasa de
  "Balance/dia" a "Balance/mes (+x/día)". **(2) La guerra era eterna** — nuevo `MakePeace` (guerra→tensión,
  opinión −30) + botón NEGOCIAR PAZ. **(3) La ayuda exterior creaba dinero** — ahora el patrocinador la paga
  (`FWLNationBudget.ForeignAidExpense`) y concederla da +8 de opinión. **(4) Golpe en nación IA terminaba TU
  partida** — ahora es cambio de régimen (junta: −oposición, −orden, fuga de capitales); solo el golpe en la
  nación del jugador es derrota. **(5) La IA no resolvía sus eventos** — `AutoResolveEventsForAI` elige la
  opción que más baja su oposición. **(6) La exposición de espionaje nunca bajaba** — decae −4/mes. **(7) El
  skill del general no pesaba en batalla** — ahora ±25% de poder (skill 0–100). **(8) UI** — filas de botones
  en WrapBox (ya no se desbordan de la tarjeta) y el presupuesto muestra las líneas que faltaban (aranceles,
  ayuda recibida/concedida, servicio de deuda, corrupción). PENDIENTE (Fase 3): ministros de Defensa/Interior/
  Exterior/Inteligencia sin efecto; efectos de espionaje no escalan con éxito. Suite 33/33. Archivos:
  `WLStrategicTickSubsystem.h/.cpp`, `WLPoliticalSubsystem.h/.cpp`, `WLMilitarySubsystem.cpp`,
  `WLCampaignHUD.cpp`, `WLGovernmentWidget.cpp`.
- **2026-07-02 · UIX ventana GOBIERNO completa** — `WLGovernmentWidget` reorganizado a los 6 tabs de la
  visión (NACION eliminado; su tabla vive en RESUMEN) y conectado a TODOS los contratos backend/frontend:
  **RESUMEN** (+banner de victoria/derrota de `GetCampaignOutcome` + territorio), **ECONOMIA** (+shocks
  activos, arancel con stepper −/+ vía `SetNationTariffRate`, finanzas soberanas: rating/deuda/riesgo con
  botones EMITIR BONO · FMI · DEFAULT, instrumentos y apoyos activos, gobernanza FE6: ministro/corrupción/
  tecnología/eficiencia), **ALTO MANDO** (capital político+stats, gabinete real con NOMBRAR —mejor candidato
  disponible— y DESTITUIR, generales con ASCENDER · RECOMPENSAR · PURGAR · DAR DE BAJA · +CREAR GENERAL),
  **POLITICA** (medidor de riesgo de golpe + oposición + financiación externa + REPRIMIR + último golpe +
  eventos F5 con opciones y preview de efectos), **DIPLOMACIA** (por país: opinión/estado/tratados/ruta
  comercial + DECLARAR GUERRA · 4 tratados firmar/romper · AYUDA + intriga F4: red/exposición + 6 operaciones
  de espionaje). Columna GABINETE del marco ahora lee el gabinete real. Mecánica UI: `UWLGovActionButton`
  (botón con payload "verbo:args") → dispatcher `HandleAction` → endpoint backend → strip de feedback.
  Compila + suite completa 33/33. Pendiente UIX (fuera de esta ventana): panel de slots de edificios en HUD,
  general en panel de ejército/token 3D, popup modal de eventos, FDI con selector. Archivos:
  `WLGovernmentWidget.h/.cpp`.
- **2026-07-02 · FE5.1/FE5.3 finanzas avanzadas backend** — `FWLFinancialInstrumentState`,
  `FWLFinancialProfile` y `FWLForeignSupportState` agregan deuda soberana, bonos, préstamos bilaterales,
  FMI, rating/default, ayuda exterior y FDI; `UWLStrategicTickSubsystem` expone endpoints para emitir deuda,
  registrar préstamos, pedir FMI, declarar default, conceder ayuda e iniciar FDI; `FWLNationBudget` ahora muestra
  `DebtService`, `ForeignAidIncome` y `ForeignInvestmentInflow`; `UWLLocalSaveGame` sube a v9 y persiste
  instrumentos/apoyos. Tests: `WorldLeader.Economy.AdvancedFinanceFE5`,
  `WorldLeader.Save.LocalCampaignRoundTrip`, `WorldLeader.Balance.SanitizeRules`; batería completa:
  `Automation RunTests WorldLeader` 33/33.
- **2026-07-02 · FE4.2-FE4.5 comercio avanzado backend** — `FWLTradeRouteState` expone rutas
  bilaterales desde diplomacia; `UWLStrategicTickSubsystem` calcula comercio por socio con acuerdos,
  tension, embargo y guerra, expone aranceles nacionales y suma `TariffIncome` al presupuesto;
  `UWLPoliticalSubsystem::SetNationTariffRate` ajusta arancel y penaliza relaciones; `UWLLocalSaveGame`
  sube a v8 y persiste `TariffRatePercent`. Tests: `WorldLeader.Economy.AdvancedTradeFE4`,
  `WorldLeader.Economy.NationalMarketAndMacro`, `WorldLeader.Economy.MarketShocksFE34`,
  `WorldLeader.Politics.F3.DiplomacyTreatyWar`, `WorldLeader.Save.LocalCampaignRoundTrip`,
  `WorldLeader.Balance.SanitizeRules`; batería completa: `Automation RunTests WorldLeader` 32/32.
- **2026-07-02 · FE3.4 shocks de mercado backend** — `FWLMarketShockState` agrega shocks temporales
  por bien o globales; `UWLStrategicTickSubsystem` expone `ApplyMarketShock`, `ClearMarketShock`,
  `GetActiveMarketShocks` y `GetMarketShockMultiplier`; `FWLGoodMarketBalance` muestra
  `MarketShockMultiplier`; precios, comercio, inflacion, PIB y ciclo economico consumen el multiplicador;
  `FWLPoliticalEventOption` puede disparar shocks desde eventos F5; `UWLLocalSaveGame` sube a v7 y
  persiste `ActiveMarketShocks`. Tests: `WorldLeader.Economy.MarketShocksFE34`,
  `WorldLeader.Economy.NationalMarketAndMacro`, `WorldLeader.Save.LocalCampaignRoundTrip`,
  `WorldLeader.Politics.F2F4F5.IntrigueEventsSave`, `WorldLeader.Balance.SanitizeRules`.
- **2026-07-02 · FE6 gobernanza economica backend** — `FWLEconomicGovernanceStats` expone ministro de
  Economia, skill, corrupcion sistemica, tecnologia, clima de inversion, eficiencia fiscal, productividad
  y skim por corrupcion; `FWLNationBudget` ahora incluye `CorruptionLoss`; produccion, ingreso provincial,
  PIB y presupuesto consumen los multiplicadores FE6; edificios tecnologicos elevan el nivel tecnologico
  nacional. Tests: `WorldLeader.Economy.GovernanceFE6`, `WorldLeader.Economy.NationalMarketAndMacro`,
  `WorldLeader.Government.Characters`, `WorldLeader.Construction`, `WorldLeader.Save.LocalCampaignRoundTrip`.
- **2026-07-02 · Edificios provinciales backend** — `FWLBuildingData` ampliado con niveles 1-5,
  coste de upgrade, upkeep y efectos por slot; `Content/Data/Buildings/Buildings.json` ahora cubre los 9 slots
  del GDD; `UWLStrategicTickSubsystem` expone build/upgrade, level, upgrade cost y efectos agregados; producción,
  PIB, upkeep, orden público e IA económica consumen esos efectos; `UWLMilitarySubsystem::AutoResolveBattle`
  lee el bonus defensivo provincial; `UWLLocalSaveGame` sube a v6 y persiste niveles. Tests:
  `WorldLeader.Construction`, `WorldLeader.EconomyAI`, `WorldLeader.Save.LocalCampaignRoundTrip`,
  `WorldLeader.Military.SubsystemGuards`.
- **2026-07-02 · F1-F5 backend pass** — Backend de gobierno vivo: `UWLCharacterSubsystem` ampliado con
  `CreateGeneral`, `CreateAndAssignGeneralToArmy`, autogeneral en `CreateArmy`, ascenso/baja, renombre mensual/batalla y capital político;
  `UWLPoliticalSubsystem` nuevo para poder interno/golpes, diplomacia/guerra/tratados, intriga/espionaje,
  eventos JSON y outcome de campaña; `UWLMilitarySubsystem::AutoResolveBattle` ahora exige guerra diplomática
  y otorga renombre a generales asignados; `UWLLocalSaveGame` sube a v6 y persiste política/diplomacia/intriga/
  eventos/outcome. Datos: `Content/Data/Political/PoliticalEvents.json`. Tests: `WorldLeader.Politics`,
  `WorldLeader.Government.Characters`, `WorldLeader.Save.LocalCampaignRoundTrip`,
  `WorldLeader.Military.SubsystemGuards`. Falta UIX: conectar `WLGovernmentWidget`, panel militar y mapa
  diplomático a los endpoints listados arriba; no se lanzó Standalone por instrucción del usuario.
- **2026-07-01 · FE2.2** — Sectores por provincia: extracción (bases oil/gas/minerals/food → crudos) y
  manufactura (base_industry → manufacturados repartidos por `industry_share` del catálogo) producen bienes
  usando TRABAJO: fuerza laboral = población × `LaborParticipationRate` (0.45); cada punto de base pide
  `WorkersPerBasePoint` (200) trabajadores; si no alcanzan, `LaborCoverage` < 1 subproduce (Arauca ~0.6).
  Servicios = resto del empleo. API: `GetProvinceProduction`/`GetNationProduction`/
  `GetProvinceEmployment` (`FWLGoodOutput`, `FWLSectorEmployment`). UI: sección PRODUCCIÓN NACIONAL/MES en
  ECONOMIA (bien, sector, unidades). Actualizado después: FE2.3/FE3 ya consumen insumos, demanda y precios.
  Test `Economy.ProvinceSectorsProduction` (suite 20/21). Archivos: `WLBalanceTypes.h`, `Goods.json`,
  `WLGameTypes.h`, `WLDataRegistry.cpp`, `WLStrategicTickSubsystem.h/.cpp`, `WLGovernmentWidget.cpp`,
  `WLBalanceTests.cpp`.
- **2026-07-01 · FE2.1** — Catálogo de bienes: `Content/Data/Goods/Goods.json` (6 crudos: oil/gas/coal/
  minerals/food/coffee · 4 manufacturados: fuel/steel/consumer_goods/weapons, con `inputs` ya declarados para
  FE2.3) + `FWLGoodData`/`EWLGoodCategory` en WLGameTypes + loader/getters en `WLDataRegistry`
  (`GetGood`/`GetAllGoods`/`GetGoodCount`) con validación de insumos existentes. Test `Data.GoodsCatalog`.
  Incluye el campo `Leader` de FWLNationData que faltó en el commit del layout de gobierno. Archivos:
  `Goods.json`, `WLGameTypes.h`, `WLDataRegistry.h/.cpp`, `WLCampaignGuardTests.cpp`.
- **2026-07-01 · FE1.5** — PIB y crecimiento: `GetNationGDP` = Σ provincias [(producción a precios de mercado
  sin la parte fiscal + edificios) + población × `GDPPerCapitaActivity` (0.002)] × modificador de orden público;
  `GetNationGDPGrowth` mide la variación entre ticks económicos (`UpdateGDPHistory` en AdvanceMonth/AdvanceDay;
  base en el 1er tick, tasa desde el 2º; se resetea al cargar save). UI: tarjetas "PIB / mes" y "Crecimiento"
  (verde/rojo) en RESUMEN + línea PIB·crecimiento sobre el presupuesto en ECONOMIA. Global por nación (ISO).
  Test `Balance.GDPAndGrowth`. **FE1 completa.** Archivos: `WLBalanceTypes.h`, `WLStrategicTickSubsystem.h/.cpp`,
  `WLStrategicTickSubsystemSave.cpp`, `WLGovernmentWidget.cpp`, `WLBalanceTests.cpp`.
- **2026-07-01 · FE1.4** — Deuda y déficit: el tesoro negativo cobra interés mensual (`DebtMonthlyInterestRate`
  2%/mes) como línea nueva del presupuesto (`FWLNationBudget.DebtInterest` → entra en `GetMonthlyBalance`);
  construir y reclutar ahora pueden gastar EN DÉFICIT (generan deuda) hasta el límite de crédito
  (`GetCreditLimit` = `DebtCreditLimitIncomeMonths` (12) × ingreso mensual) — con el crédito agotado el gasto
  se bloquea con mensaje. La penalización de orden público por bancarrota ya existía y sigue aplicando. La IA
  económica conserva su reserva mínima (no se endeuda). UI: fila "Intereses de deuda" + tarjeta DEUDA (deuda,
  interés, crédito restante) o línea de crédito disponible en ECONOMIA. Global: presupuesto/interés/límite por
  nación (ISO). Test `Balance.DebtAndCreditLimit` (interés esperado, gasto en déficit endeuda, crédito agotado
  bloquea). Archivos: `WLBalanceTypes.h`, `WLStrategicTickSubsystem.h/.cpp`,
  `WLStrategicTickSubsystemBuildings.cpp`, `WLStrategicTickSubsystemRecruit.cpp`, `WLGovernmentWidget.cpp`,
  `WLBalanceTests.cpp`.
- **2026-07-01 · FE1.3** — Presupuesto por categorías: `FWLNationBudget` (ingresos: recursos/producción +
  impuestos; gastos: militar + infraestructura + salarios públicos + gasto social) vía `GetNationBudget`;
  `GetMonthlyBalance` ahora ES el neto del presupuesto (única fuente de verdad). Gastos nuevos per cápita
  (`PublicWagesPerCapita` 0.0002, `SocialSpendingPerCapita` 0.0001). Tab **ECONOMIA** nuevo en GOBIERNO con el
  desglose + balance neto; la palanca de impuestos (FE1.2) se movió de RESUMEN a ECONOMIA. Tests: suite completa
  16/17 (`Balance.NationBudgetBreakdown` nuevo verifica neto == balance con subsistema real;
  `ControllerDrivesEconomy` actualizado al modelo con gasto per cápita). El fallo restante
  (`Military.SubsystemGuards`) es preexistente: referencia la provincia CO-GUA que ya no existe en
  Provinces.json. Archivos: `WLBalanceTypes.h`, `WLStrategicTickSubsystem.h/.cpp`, `WLGovernmentWidget.h/.cpp`,
  `WLBalanceTests.cpp`, `WLProvinceStateTests.cpp`.
- **2026-07-01 · FE1.2** — Palanca de impuestos por nación (10%–60%, default 30%). Recaudación con curva
  Laffer normalizada (`CalculateTaxRateIncomeMultiplier`: default = ×1.0, rendimiento decreciente) aplicada a
  la parte de impuesto poblacional del ingreso provincial; orden público drena/recupera cada mes según
  `(tasa − default) × TaxPublicOrderPerPointPerMonth`. UI: tarjeta IMPUESTOS con botones −/+ (pasos de 5) en
  RESUMEN de la ventana GOBIERNO. Tasa persistida en el save (`FWLNationTreasurySave.TaxRatePercent`, −1 =
  default) y reseteada con la campaña. Test `WorldLeader.Balance.TaxLeverLaffer`. Archivos: `WLBalanceTypes.h`,
  `WLEconomyLibrary.h/.cpp`, `WLStrategicTickSubsystem.h/.cpp`, `WLStrategicTickSubsystemSave.cpp`,
  `WLLocalSaveGame.h`, `WLGovernmentWidget.h/.cpp`, `WLBalanceTests.cpp`.
- **2026-07-01 · FE1.1** — Upkeep militar mensual = efectivos (fuerzas desplegadas de MilitaryForces.json +
  guarniciones reclutadas) × `MilitaryUpkeepPerStrength` (0.35). Restado en `GetMonthlyBalance` y sumado al
  "Mantenimiento/mes" del RESUMEN. Verificado en runtime: CO 34.200→11.970/mes (balance 51.470→39.500),
  VE 28.800→10.080/mes. Archivos: `WLBalanceTypes.h`, `WLStrategicTickSubsystem.h/.cpp`,
  `WLStrategicTickSubsystemRecruit.cpp`, `WLGovernmentWidget.cpp`, `WLCampaignGameInstance.cpp` (instantánea económica).
- **2026-07-01 · F1.1** — Modelo `FWLCharacter` + enums `EWLCharacterRole`/`EWLMilitaryRank` creados en
  `Source/WorldLeader/Core/WLCharacterTypes.h`. UHT procesa y el módulo compila+linkea (WorldLeaderEditor Development).
