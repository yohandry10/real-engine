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

- **Fase activa:** UIX. La ventana GOBIERNO ya opera TODO el backend (6 tabs: RESUMEN · ECONOMIA ·
  ALTO MANDO · POLITICA · DIPLOMACIA · REGISTROS). Quedan 2 piezas de UIX fuera de esa ventana:
- **Próxima tarea (UIX/pulido):** (a) panel de **slots de edificios** en el HUD de provincia,
  (b) popup modal de eventos + feed de noticias del mes en REGISTROS, (c) acción FDI con selector,
  (d) **playtest de calibración** (partida de 24-36 meses registrando curvas de deuda/golpe/precios).
- **Última actualización:** 2026-07-02 (mapa↔backend militar conectado, IA estratégica jugando,
  victorias Hegemonía/Régimen + fin de partida real; suite 35/35)

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

### Contrato backend/frontend F1-F5

> Estado: backend compilado y cubierto por tests. Lo pendiente aquí es UIX, no datos hardcodeados en widgets.

**F1 Personajes & Generales — endpoint `UWLCharacterSubsystem`**
- Lecturas UI: `GetCharactersByNation`, `GetCharactersByRole`, `GetGenerals`, `GetCabinet`,
  `GetCabinetMinister`, `GetGovernmentStats`, `GetPoliticalCapital`, `GetAssignedGeneralForArmy`.
- Acciones UI: `AppointMinister`, `DismissMinister`, `CreateGeneral`, `CreateAndAssignGeneralToArmy`,
  `AssignGeneralToArmy`, `PromoteGeneral`, `RetireCharacter`, `AddRenownToGeneral`,
  `AdjustCharacterLoyalty`, `AdjustPoliticalCapital`.
- Persistencia: `UWLLocalSaveGame.Characters` + `PoliticalCapital` (save local v9).
- Falta UIX: tab **ALTO MANDO**, cards de gabinete/personajes, selector de general y botones de ascenso/baja.
- Hook pendiente 3D/presentación: `WLCampaign3DView::SyncRecruitedArmyTokens` todavía crea tokens visuales
  `ARMY-<fort>`; cuando esa capa se conecte al backend debe llamar a `CreateAndAssignGeneralToArmy` o mapear
  el token visual a un `FWLArmy` real para que el nombre visible salga del general.

**F2 Poder interno & Golpes — endpoint `UWLPoliticalSubsystem`**
- Lecturas UI: `GetInternalPower`, `GetCampaignOutcome`.
- Acciones UI: `AttemptCoup`, `RewardGeneral`, `PurgeCharacter`, `RepressOpposition`.
- Tick backend: `ProcessPoliticalMonth` se llama desde `UWLCampaignGameInstance::WLAdvanceMonth` y desde el
  avance diario del `AWLCampaignPlayerController` solo cuando hay rollover de mes.
- Persistencia: `UWLLocalSaveGame.InternalPowerStates` + `CampaignOutcome` (save local v9).
- Falta UIX: medidor de golpe, oposición, botones de acciones internas, feedback y registro.

**F3 Diplomacia & Guerra — endpoint `UWLPoliticalSubsystem`**
- Lecturas UI: `GetRelationsForNation`, `GetRelation`.
- Acciones UI: `SetRelationOpinion`, `AdjustRelationOpinion`, `DeclareWar`, `SignTreaty`, `BreakTreaty`.
- Integración backend: `UWLMilitarySubsystem::AutoResolveBattle` bloquea combate entre países si la relación
  no está en `EWLDiplomaticStatus::War`; declarar guerra habilita el auto-resolve.
- Persistencia: `UWLLocalSaveGame.DiplomaticRelations` (save local v9).
- Falta UIX: tab **DIPLOMACIA**, lista de países, estado de relación, tratados y botones de declarar guerra/tratados.

**F4 Intriga exterior — endpoint `UWLPoliticalSubsystem`**
- Lecturas UI: `GetIntelligenceNetwork`.
- Acciones UI: `BuildSpyNetwork`, `RunSpyOperation` con `SabotageEconomy`, `SabotageArmy`, `FundCoup`,
  `Propaganda`, `CounterIntelligence`.
- Efectos backend: operaciones modifican red/exposición, orden público, oposición, riesgo/funding de golpe y
  opinión diplomática si son detectadas.
- Persistencia: `UWLLocalSaveGame.IntelligenceNetworks` (save local v9).
- Falta UIX: panel de espías, selector de agente/objetivo, riesgo de exposición, resultado de operación.

**F5 Eventos & Metas — endpoint `UWLPoliticalSubsystem`**
- Datos: `Content/Data/Political/PoliticalEvents.json`.
- Lecturas UI: `GetQueuedEvents`, `GetCampaignOutcome`, `GetLeaderAgendaTraits`.
- Acciones UI: `ResolveEvent`.
- Tick backend: `ProcessPoliticalMonth` evalúa eventos, agenda del líder, golpes y outcome; `CheckCampaignOutcome`
  cubre dominación y golpe exitoso/revolución como derrota.
- Persistencia: `UWLLocalSaveGame.PoliticalEvents` + `CampaignOutcome` (save local v9).
- Falta UIX: cola visible, popup/modal de evento, botones de opciones, registro histórico de decisiones.

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
- Persistencia: `UWLLocalSaveGame.ProvinceBuildings.BuildingLevels` + save local v9; saves viejos sin niveles
  restauran edificios como nivel 1.
- Falta UIX: panel de slots reales, nivel, coste de upgrade, mantenimiento, preview de efectos y botón construir/mejorar.

### B — Batallas  *(paralelo; hasta entonces auto-resuelto)*
- [~] **B.1 — Auto-resolución.** Combate por composición, terreno, defensa provincial, guerra diplomática,
  bajas, ocupación y renombre posterior; falta que el skill del general pese directamente en el cálculo.
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
