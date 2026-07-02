# UIX Pending Tasks

Estado: backlog separado de frontend/UIX pendiente. El backend local P0/P1 ya existe para estas areas; la UIX debe consumir endpoints y snapshots existentes, no duplicar reglas de simulacion en widgets.

Ultima validacion backend relacionada: build `WorldLeaderEditor Win64 Development` OK + suite `Automation RunTests WorldLeader` 55/55 (`EXIT CODE: 0`). Standalone no se abrio en la pasada de backend Gobierno P2.

## Progreso UIX (2026-07-02, sesion Claude)

Hecho y compilando (`Build.bat WorldLeaderEditor Win64 Development` OK):

- **Gobierno P1/P2 UIX completo** en la pestana POLITICA, reconvertida en hub con 9 subsecciones
  (PODER, AGENDA, PROGRAMAS, LEYES, CONGRESO, ELECCIONES, MEDIOS, REGIONES, CRISIS). Cada una lee sus
  endpoints (`GetGovernmentAgenda`/`SetGovernmentAgenda`, `GetAvailable/ActiveMinistryPrograms`+`StartMinistryProgram`,
  `GetAvailable/ActivePolicyReforms`+`EnactPolicyReform`, `GetPoliticalParties`+`NegotiatePartySupport`+`HoldPartyInternalElection`,
  `GetElectionState`+`MakeCampaignPromise`, `GetPatronageState`+`UsePatronage`, `GetMediaPublicOpinion`+`RunMediaAction`,
  `GetRegionGovernors`+`RunRegionPolicy`, `GetActiveCrisisChains`+`GetPoliticalMemory`, grupos sociales y capacidad estatal).
  Bloqueos legibles (prerequisito/coalicion/capacidad/capital/tesoro), previews de impacto y confirmacion en dos clics.
- **RESUMEN** gana grid "Pulso de Gobierno" (aprobacion, legitimidad, eleccion, coalicion, capacidad, crisis).
- **ALTO MANDO**: gabinete vivo (rivalidad/escandalo/sabotaje/renuncia), comparador de candidatos por cartera
  (`opencompare`/`appointc`/`hire`), fichas politicas de personajes ordenables (`GetCharacterPoliticalProfiles`).
- **REGISTROS**: plan de IA politica por pais (`GetGovernmentAIPlan`) y panel de calibracion (`GetGovernmentCalibration`).
- **Diplomacia continental**: listado compacto de 38 naciones con buscador (nombre/ISO), filtros de estado,
  ordenamiento y panel de gestion por pais (tratados, guerra, ayuda, FDI, intriga). Sin lista plana.
- **Eventos modal + feed**: `UWLEventModalWidget` (popup con opciones/coste/riesgo/impacto y confirmacion) que el
  PlayerController abre al cerrar el mes o con [E]; feed de noticias del mundo en el HUD (bottom-left).
- **Militar/generales** (ALTO MANDO): seccion EJERCITOS con ATK/DEF, efectivas vs desorganizadas
  (`RecoveringUnits`), ficha del general asignado, REORGANIZAR (`ReorganizeArmy`) y asignar/reasignar
  general (`AssignGeneralToArmy`).
- **Dificultad visible** (RESUMEN): nivel de IA activo + selector Facil/Medio/Dificil
  (`UWLBalanceSubsystem::SetAIDifficulty`) con explicacion de impactos.
- **Flujo de combate** (ALTO MANDO): boton ATACAR por objetivo enemigo al alcance
  (`GetAttackableTargetIds`), preview de poder legible (`PreviewBattle`: composicion, generales,
  terreno, defensas, pronostico) y eleccion AUTO-RESOLVER (`AutoResolveBattle`) vs BATALLA TACTICA
  (`ResolveTacticalBattleToEnd`: juega la IA tactica determinista y aplica bajas/ocupacion), ambas con
  confirmacion. Backend nuevo minimo: `FWLBattlePreview` + los 3 endpoints, con el calculo de poder
  refactorizado a `ComputeBattlePower` (fuente unica compartida con auto-resolve).
- Infra compartida: `WLGovernmentWidgetShared.h` (paleta + fabrica de widgets: MakeGaugeRow, MakeBar, MakeBadge,
  MakeAlert, MakeActionButton con estado de confirmacion). Widget partido en `WLGovernmentWidget.cpp` +
  `WLGovernmentWidgetGovernance.cpp`.

Pendiente: **vista tactica 3D interactiva** (camara, seleccion y ordenes con mouse sobre el mapa de
batalla) — el backend tactico existe y se juega solo AI-vs-AI desde `ResolveTacticalBattleToEnd`, pero la
vista jugable es su propia fase. Y validar todo en Standalone (build compila; falta playtest visual).

## Principio De Implementacion

- Mantener la logica de juego en subsistemas (`UWLPoliticalSubsystem`, `UWLCharacterSubsystem`, `WLStrategicTickSubsystem*`, `UWLMilitarySubsystem`, `UWLTacticalBattleSubsystem`, `UWLBalanceSubsystem`).
- La UIX solo debe leer estado, mostrar preview legible, pedir confirmacion y llamar acciones backend.
- Cada accion visible debe tener feedback: exito, bloqueo, coste, impacto esperado y registro posterior.
- Las listas grandes de America no deben ser listas planas sin filtros.
- No mezclar esta lista con backend online/PvP; eso es otra fase.

## Orden Sugerido

1. Flujo de combate y batalla visual.
2. Gobierno P1 UIX completo.
3. Diplomacia continental y contratacion ministerial.
4. Edificios provinciales y eventos modal/feed.
5. Finanzas/FDI, generales militares, dificultad visible y pulido de feedback.
6. Playtest de calibracion 24-36 meses.

## Gobierno P1 Real

Backend disponible: `UWLPoliticalSubsystem` con agenda, programas ministeriales, gabinete vivo, instituciones, grupos sociales, capacidad estatal, memoria politica y plan IA.

### Agenda Nacional UIX

- Selector de hasta 3 prioridades: seguridad, crecimiento, austeridad, industrializacion, diplomacia, control interno.
- Mostrar efectos previstos sobre eventos, presupuesto, opinion publica, ministerios, diplomacia, orden y grupos sociales.
- Mostrar meses activos y ultimo reporte de agenda.
- Validar duplicados y limite antes de llamar `SetGovernmentAgenda`.
- Feedback de capital politico/coste si una futura regla lo exige.

### Programas Ministeriales UIX

- Panel por cartera: Economia, Defensa, Interior, Exterior, Inteligencia.
- Listar programas disponibles desde backend con coste politico, coste fiscal, duracion, requisito legislativo y descripcion.
- Mostrar programa activo por ministerio, progreso, meses restantes, bloqueo y ultimo reporte.
- Preview de impacto esperado y riesgo de fallo por capacidad estatal.
- Accion para iniciar programa con confirmacion antes de llamar `StartMinistryProgram`.
- Mensaje claro cuando ya hay un programa activo en esa cartera o no alcanza tesoro/capital politico.

### Gabinete Vivo UIX

- Mostrar rivalidad, faccionalismo, riesgo de escandalo, riesgo de sabotaje y riesgo de renuncia.
- Vincular incidentes recientes con ministro/cartera afectada cuando el backend lo reporte.
- En tarjetas de ministros, mostrar skill, lealtad, ambicion, popularidad, rasgos y efecto de cartera.
- Separar "nombrar", "destituir" y "contratar" como flujos distintos.
- Comparador de candidatos por ministerio: skill, lealtad, ambicion, popularidad, rasgos, coste politico.
- Confirmacion antes de reemplazar ministro en ejercicio.

### Congreso / Base Politica UIX

- Mostrar coalicion oficialista, oposicion legislativa, coste de reforma y riesgo de bloqueo institucional.
- Panel de votacion/reforma que llame `PassGovernmentReform`.
- Explicar por que una reforma fue aprobada, bloqueada o fallida.
- Mostrar impacto de gridlock sobre programas y capacidad estatal.

### Grupos Sociales UIX

- Mostrar apoyo y presion para empresarios, militares, trabajadores, regiones, clase media y sindicatos.
- Mostrar razon del ultimo cambio de apoyo.
- Usar colores de riesgo para presion alta y soporte bajo.
- Conectar decisiones de agenda/programa/evento con los grupos afectados en el feed.

### Capacidad Estatal UIX

- Mostrar burocracia, corrupcion, eficiencia administrativa, autoridad central y riesgo de fallo de politicas.
- Incluir preview de capacidad estatal antes de iniciar programas.
- Explicar provincias o condiciones que reducen autoridad/eficiencia cuando haya dato disponible.
- Mostrar ultimo reporte de capacidad estatal.

### Memoria Politica Y Crisis Encadenadas UIX

- Timeline de decisiones recordadas por pais.
- Mostrar crisis en curso y escalamiento: protesta -> huelga -> represion -> escandalo -> golpe.
- Mostrar duracion restante o relevancia de memorias.
- Mostrar consecuencias diferidas en REGISTROS/feed.
- Vincular resoluciones de eventos con memoria politica creada.

### IA Politica Visible UIX

- Para cada pais IA, mostrar objetivo: estabilizar, expandirse, endeudarse, militarizarse, alinearse o industrializarse.
- Mostrar programa actual, meses en plan y motivo del plan.
- En diplomacia, mostrar por que una IA podria firmar tratado, reprimir, invertir o declarar guerra.
- No permitir que la UI decida el plan; solo exponer lo que `GetGovernmentAIPlan` devuelve.

## Gobierno P2 Real: Tareas UIX Por Punto

Backend disponible: Gobierno P2 agrega reformas, partidos, elecciones, perfiles politicos de personajes, patronazgo, medios, gobernadores regionales, crisis encadenadas, IA politica ampliada y metricas de calibracion. La UIX debe exponer estos sistemas sin mover reglas fuera del backend.

### 1. Arbol De Leyes / Reformas

Endpoints backend: `GetAvailablePolicyReforms`, `GetActivePolicyReforms`, `EnactPolicyReform`.

- Vista de arbol por area: tributaria, laboral, seguridad, educacion, salud, descentralizacion, militar, justicia, medios, energia, comercio, constitucion.
- Mostrar prerequisitos, coste politico, coste fiscal, coalicion requerida, capacidad estatal requerida y riesgo de protesta.
- Mostrar grupos afectados y delta esperado de apoyo.
- Mostrar efectos a largo plazo: meses restantes, progreso, backlash, ingreso mensual, legitimidad, corrupcion y capacidad estatal.
- Confirmacion antes de aprobar reforma.
- Bloqueos legibles: prerequisito faltante, coalicion insuficiente, capacidad insuficiente, capital politico insuficiente o tesoro insuficiente.
- Timeline de reformas aprobadas/consolidadas y promesas electorales incumplidas.

### 2. Partidos E Ideologias

Endpoints backend: `GetPoliticalParties`, `NegotiatePartySupport`, `HoldPartyInternalElection`.

- Panel de Congreso por bancadas: oficialismo, aliados, oposicion blanda y oposicion dura.
- Mostrar ideologia, escanos, disciplina, lealtad al gobierno, corrupcion y si esta en coalicion.
- Accion de negociar apoyo con preview de coste politico y riesgo clientelar.
- Accion de eleccion interna con resultado de disciplina/lealtad.
- Alertas de traicion, ruptura de coalicion, bloqueo institucional y juicio politico.
- Filtros por ideologia y rol parlamentario.

### 3. Elecciones Y Legitimidad

Endpoints backend: `GetElectionState`, `MakeCampaignPromise`.

- Panel electoral con meses hasta eleccion, fase, aprobacion presidencial, encuestas, legitimidad, abstencion y riesgo de fraude.
- Flujo de promesa de campania ligado a una reforma real.
- Mostrar si una promesa fue cumplida, incumplida o usada para campania.
- Mostrar resultado electoral, reeleccion, derrota, transicion o crisis constitucional.
- Alertas cuando fraude/censura/patronazgo erosionen legitimidad.
- Grafico historico de aprobacion y encuestas por mes.

### 4. Contenido Ministerial Ampliado

Endpoints backend: `GetAvailableMinistryPrograms`, `GetActiveMinistryPrograms`, `StartMinistryProgram`.

- Catalogo por cartera con decenas de programas: economia, defensa, interior, exterior e inteligencia.
- Filtros por coste, duracion, requisito legislativo, objetivo y riesgo.
- Mostrar programas activos por ministerio, bloqueo, progreso y ultimo reporte.
- Preview de efecto generico/especifico por cartera.
- Evitar que una cartera con programa activo acepte otro sin explicar el bloqueo.
- Diferenciar programa ministerial de reforma legal: programa es ejecucion; reforma es ley con efecto largo.

### 5. Personajes Con Biografia Y Agenda Propia

Endpoints backend: `GetCharacterPoliticalProfiles`, `GetCharacterPoliticalProfile`.

- Ficha politica por personaje: biografia, faccion, patron, rivales, aliados, agenda, ambicion presidencial, corrupcion personal, calor de escandalo y puntaje de sucesion.
- Mostrar vinculos entre ministros, generales, opositores y espias.
- Alertas de escandalo personal, sucesion, rivalidad y patronazgo.
- Ordenar personajes por sucesion, ambicion, corrupcion, lealtad y faccion.
- Integrar estos datos en ALTO MANDO/GABINETE sin duplicar la ficha militar.

### 6. Patronazgo Y Corrupcion Politica

Endpoints backend: `GetPatronageState`, `UsePatronage`.

- Panel de red clientelar: poder de patronazgo, presion clientelista, corrupcion de contratos, maquinaria regional y backlash por concesiones.
- Acciones: nombrar leal, conceder contrato, repartir favor, financiar gobernador, otorgar concesion.
- Preview de beneficio politico vs corrupcion/deuda/oposicion/escandalo.
- Registro de tratos recientes y sus consecuencias.
- Alertas cuando patronazgo dispare escandalo o crisis de corrupcion.

### 7. Medios, Opinion Publica Y Narrativa

Endpoints backend: `GetMediaPublicOpinion`, `RunMediaAction`.

- Panel de prensa/opinion: libertad de prensa, control mediatico, aprobacion presidencial, alcance propagandistico, backlash de censura, fake news y riesgo de crisis mediatica.
- Acciones: rueda de prensa, cadena nacional, propaganda interna, censura, contra fake news.
- Preview de impacto en aprobacion, legitimidad, crisis mediatica y elecciones.
- Feed narrativo de prensa con ultima narrativa.
- Alertas cuando censura o fake news escalen a protesta/crisis.

### 8. Regiones Y Gobernadores

Endpoints backend: `GetRegionGovernors`, `RunRegionPolicy`.

- Mapa/lista regional con gobernador, alineacion, obediencia, autonomia, protesta, control central, inversion, secesion y rebelion.
- Acciones: nombrar gobernador, inversion regional, pacto autonomico, operacion de seguridad.
- Preview de impacto en obediencia, autonomia, protesta y control central.
- Alertas de secesion, rebelion o protesta regional.
- Integrar con provincia/HUD para que cada region tenga entrada visual clara.

### 9. Crisis Encadenadas Con Contenido

Endpoints backend: `GetActiveCrisisChains`, `GetPoliticalMemory`, `GetQueuedEvents`, `ResolveEvent`.

- Panel de crisis activas: tipo, etapa, intensidad, meses activos y ultimo reporte.
- Tipos visibles: paro nacional, escandalo de corrupcion, crisis militar, crisis de deuda, protesta estudiantil, huelga petrolera, crisis fronteriza, juicio politico, golpe blando y estado de excepcion.
- Mostrar memoria politica que alimenta cada crisis.
- Mostrar escalamiento y bifurcaciones: protesta -> huelga -> represion -> escandalo -> golpe.
- Resolucion de eventos con consecuencias diferidas visibles.
- Feed historico por pais y por crisis.

### 10. IA Politica Mas Ambiciosa

Endpoints backend: `GetGovernmentAIPlan`, `GetPoliticalParties`, `GetElectionState`, `GetMediaPublicOpinion`, `GetRegionGovernors`.

- Mostrar no solo objetivo IA, sino acciones de plan: reforma buscada, programa activo, negociacion de coalicion, promesa de campania, accion de medios, region prioritaria y uso de patronazgo.
- En diplomacia, explicar por que un pais IA se alinea, militariza, reprime, se endeuda, invierte o prepara guerra.
- Indicador de meses en plan y motivo textual.
- Registro de decisiones IA importantes en REGISTROS.
- Filtro para ver planes IA de toda America.

### 11. Balance Y Calibracion

Endpoints backend: `GetGovernmentCalibration`.

- Panel de telemetria jugable para playtest: deuda vs crecimiento, represion vs legitimidad, subsidios vs inflacion, reformas vs gridlock, militares vs civiles, aliados vs soberania.
- Mostrar meses observados y ultimo reporte.
- Exportar o copiar resumen de 24-36 meses para calibracion.
- Alertas cuando un dilema domina demasiado y no hay tradeoff real.
- Mantener este panel como herramienta de debug/playtest, no como pantalla principal para jugador final.

## Diplomacia Continental UIX

Backend disponible: relaciones bilaterales para 38 naciones americanas cargadas, tratados, guerra, embargos, rutas comerciales e intriga.

- Vista continental con filtros por region, estado diplomatico, tratado, guerra, embargo, opinion y busqueda por nombre/ISO.
- Ordenamiento por opinion, tesoro, provincias, estado, tratado y riesgo.
- Panel de detalle por pais con opinion, estado, tratados, rutas, acciones y ultimos incidentes.
- Confirmaciones para declarar guerra, embargo, alianza, no-agresion, comercio y romper tratados.
- Mostrar bloqueo de combate si no hay guerra.
- Mostrar rutas comerciales afectadas por guerra/embargo y multiplicadores relevantes.
- Evitar lista plana inmanejable para America completa.

## Intriga UIX

Backend disponible: redes de inteligencia, exposicion y operaciones principales.

- Selector dedicado de agente, objetivo y operacion.
- Mostrar red actual, exposicion, riesgo de descubrimiento y efecto esperado.
- Operaciones visibles: construir red, sabotear economia, sabotear ejercito, financiar golpe, propaganda, contraespionaje.
- Resultado persistente en feed/registro, no solo texto fugaz.
- Mostrar impacto diplomatico cuando una operacion es descubierta.

## Politica Interna Y Eventos UIX

Backend disponible: oposicion, golpe, eventos, opciones, outcome y memoria politica.

- Feedback visual de revuelta/golpe en mapa y en panel de POLITICA.
- Confirmaciones para recompensar general, purgar, reprimir oposicion y resolver evento.
- Popup/modal de evento con opciones, coste, riesgo e impacto esperado.
- Feed de noticias del mes.
- Registro historico de decisiones y consecuencias.
- Estado de derrota/victoria con causa clara y bloqueo de avance cuando corresponda.

## Economia / Finanzas / Comercio UIX

Backend disponible: FE1-FE6, finanzas avanzadas, shocks, comercio, gobernanza economica y FDI.

### Finanzas Avanzadas

- Panel de rating, deuda, servicio mensual, default, FMI, prestamos, ayuda exterior y capacidad de credito.
- Acciones para emitir bono, registrar/pedir prestamo, solicitar FMI, declarar default, conceder ayuda e iniciar FDI.
- Preview de impacto presupuestario antes de confirmar.
- Mostrar vencimientos o duracion restante de instrumentos financieros cuando aplique.

### FDI Y Ayuda

- Selector de provincia/edificio/objetivo antes de ejecutar inversion extranjera.
- Mostrar origen, destino, coste, beneficio esperado y efecto diplomatico.
- Mostrar `ForeignAidIncome` y `ForeignInvestmentInflow` en presupuesto y registros.

### Comercio Exterior

- Panel de rutas por pais: estado de ruta, volumen, acceso, tratados, embargos, aranceles, importaciones/exportaciones.
- Control de arancel nacional con preview fiscal/diplomatico.
- Mostrar sanciones/embargos y su impacto.
- Conectar rutas cortadas por guerra con feedback visible.

### Shocks De Mercado

- Panel/log de shocks activos.
- Mostrar bien afectado, duracion restante, multiplicador e impacto en inflacion/comercio.
- Mostrar origen del shock si viene de evento, intriga o decision.

### Gobernanza Economica

- Panel con ministro de Economia, skill/rasgos, tecnologia, corrupcion, eficiencia fiscal y perdida por corrupcion.
- Mostrar productividad nacional y explicacion de efectos FE6.
- Conectar corrupcion/capacidad estatal con fallos de programas.

## Edificios Provinciales UIX

Backend disponible: catalogo JSON, slots, niveles 1-5, coste, mantenimiento, efectos e IA economica.

- Panel de provincia con slots reales: economico, industrial, militar, naval, aereo, tecnologico, financiero, infra, defensa.
- Mostrar edificio actual, nivel, coste de construccion/mejora, mantenimiento y efectos.
- Acciones construir/mejorar con preview y confirmacion.
- Mostrar bloqueo por tesoro insuficiente, slot invalido o prerequisito.
- Mostrar impactos economicos/militares/orden publico.
- Integrar bonus defensivo en preview de combate cuando el edificio lo aporte.

## Militar / Generales UIX

Backend disponible: ejercitos reales, generales, renombre, asignacion, auto-resolve, batalla tactica y reorganizacion.

- Ficha de ejercito con general asignado, rango, skill, renombre, lealtad relevante y rasgos.
- Accion para asignar/reasignar general desde lista valida.
- Mostrar renombre ganado por batalla o por turno.
- Mostrar reservas/desorganizacion y unidades `RecoveringUnits`.
- Boton de reorganizar ejercito que llame `ReorganizeArmy`.
- Log claro cuando una fuerza se retira, queda atrapada, es capturada o vuelve al frente.

## Combate Y Batalla Tactica UIX

Backend disponible: auto-resolve B.1, batalla tactica B.2a/B.2c, IA tactica B.2d y retirada/reorganizacion B.2e.

### Flujo De Combate

- Selector/preview `Auto-resolve` vs `Batalla tactica`.
- Desglose de poder antes de confirmar: composicion, skill del general, terreno, defensas, guerra valida y modificadores.
- Confirmacion antes de iniciar combate.
- Registro de batalla legible con bajas, ocupacion, renombre y outcome.

### Vista Tactica

- Mapa 3D de batalla, camara, seleccion de unidades y ordenes con mouse.
- Barra de objetivo, moral, control de puntos y estado de victoria.
- Feedback de movimiento, ataque, retirada, captura de objetivo y unidades destruidas.
- Pantalla de carga/resultado al volver a campaña.

### IA Tactica Visible

- Toggle/estado visible de control IA por pais cuando aplique.
- Mostrar ordenes automaticas emitidas o al menos intencion general.
- Feedback claro de retirada IA y cambio de objetivo.

## Mapa De Campania / Rutas UIX

Backend disponible: grafo de rutas `FWLCampaignRouteGraph`.

- Visualizar ruta seleccionada antes de mover.
- Mostrar nodos alcanzables, ruta bloqueada y razon del bloqueo.
- Feedback de carretera/frontera/puerto cuando el movimiento usa esos tramos.
- Mantener region zoom como mapa estrategico y detalle solo en Theater/Close, segun reglas de Campaign 3D.

## Dificultad Y Ajustes UIX

Backend disponible: `UWLBalanceSubsystem` con `EWLAIDifficulty`.

- Selector de dificultad al iniciar/cargar campania.
- Indicador visible del nivel activo.
- Explicacion corta de impactos: economia, fisco, diplomacia, guerra, intriga y reclutamiento.
- Leer reglas desde backend; no duplicar umbrales en UI.

## Feedback General Y Pulido

- Mensajes claros para fondos insuficientes, capital politico insuficiente, accion bloqueada, guerra no declarada, tratado incompatible y programa ya activo.
- Tooltips compactos para terminos de sistema: capacidad estatal, gridlock, exposicion, rating, shock, ruta bloqueada.
- Evitar botones sin contexto en listas grandes.
- Mantener la ventana GOBIERNO como interfaz operativa, no landing page.
- Revisar que texto no se superponga en 1600x900 y resoluciones menores.
- Playtest 24-36 meses con registro de deuda, golpe, orden publico, precios, programas, guerras y eventos.

## No Es UIX

- Backend online Rust/Axum + PostgreSQL + Redis.
- Matchmaking, perfiles, resultados PvP y persistencia online.
- Dedicated Server autoritativo.
- Datos mundiales profundos handcrafted fuera de America/CO-VE.
- Profundidad P2 de simulacion: IA con planes largos, comercio logistico granular, patronazgo profundo, intriga con atribucion incierta y eventos narrativos mas complejos.
