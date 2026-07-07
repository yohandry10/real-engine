# Batalla Táctica — Guía de diseño e implementación

Objetivo: batallas modernas de armas combinadas estilo **Rome 2 Total War** — el jugador
selecciona contingentes (tanques, infantería, artillería, AA, helos, cazas), da órdenes de
mover/atacar, y el resultado depende de **contras realistas** (quién ataca a quién, dónde).

---

## 1. Decisión de stack: **C++ / Unreal Engine. NO Three.js.**

| Criterio | C++ / UE (elegido) | Three.js |
|---|---|---|
| Acceso al estado del juego | Directo (mismos subsistemas) | Requiere puente/IPC con UE |
| Base ya construida | `StartTacticalBattle`, `TickTacticalBattle`, `WLTacticalBattleView`, reconciliación de bajas | Cero: todo desde el principio |
| Render | Instancing, terreno, VFX, LOD nativos | WebView embebido dentro de un juego de escritorio |
| Simulación | Un solo modelo (backend C++) sirve batalla manual Y auto-resolve | Habría que duplicar la simulación en JS |
| Riesgo | Bajo (extender lo que hay) | Alto (dos runtimes, dos lenguajes, sincronización) |

Three.js es para juegos web. Este es un juego de escritorio UE 5.8 con el modelo de batalla
**ya iniciado** en C++. Cambiar de stack sería tirar la base y añadir un puente frágil.
**Decisión: todo en C++/UE.**

## 2. Lo que YA existe (base real, no empezamos de cero)

- `Core/WLTacticalBattleTypes.h` — `FWLTacticalUnitState` (salud, moral, orden), órdenes
  `Idle/Moving/Attacking/Routing`, `EWLTacticalBattleResult`.
- `WLMilitarySubsystem::StartTacticalBattle(...)` — crea la batalla desde dos ejércitos reales.
- `AWLCampaignPlayerController` — `EnterTacticalBattle`, `TickTacticalBattle`,
  `AutoFinishTacticalBattle`, `ExitTacticalBattle(bApplyResult)`.
- `WLMilitarySubsystem::ReconcileArmyFromTacticalBattle` — aplica bajas/derrotas a campaña.
- `WLTacticalBattleView` (287 líneas) — vista 3D mínima: un mesh por unidad, anillo de
  selección, color por estado (routing).
- Composición real de ejércitos: `infantry / apc / ifv / mbt / artillery / heli / aircraft / ship`
  (RecruitableUnits.json), guarniciones vivas por fuerte.

## 3. Visión de gameplay (el "Rome 2 moderno")

- Cada ejército entra a batalla con sus **contingentes** (1 contingente = 1 grupo de unidades
  del mismo tipo: "4 tanques", "50 de infantería").
- El jugador **selecciona un contingente y ordena**: mover a punto, atacar a contingente
  enemigo, mantener posición. La IA comanda el bando rival.
- El combate se resuelve por **tick continuo** (no por turnos): alcance, contras, terreno,
  moral. Las unidades con moral rota entran en `Routing` (huyen; pueden recuperarse).
- La batalla termina por aniquilación/derrota general de un bando o retirada voluntaria.
  El resultado vuelve a campaña (bajas reales, ocupación) — ese pipe ya existe.

## 4. Matriz de contras (realista, la "piedra-papel-tijera" moderna)

Regla de oro (equivalente moderno de "caballería vs piqueros"):

| Atacante ↓ vs Defensor → | Infantería | APC/IFV | MBT (tanque) | Artillería | SAM/AA | Helo | Caza |
|---|---|---|---|---|---|---|---|
| **Infantería** | 1.0 | 0.8 | **0.4 abierto / 1.4 urbano-bosque** (ATGM emboscada) | 1.6 | 1.5 | 0.3 | — |
| **APC/IFV** | **1.6** | 1.0 | 0.4 | 1.4 | 1.3 | 0.4 | — |
| **MBT** | **1.8 abierto / 0.7 urbano** | 1.6 | 1.0 | **1.8** | 1.6 | 0.3 | — |
| **Artillería** (indirecta) | **1.8** vs estático | 1.2 | 0.8 | 1.0 (contrabatería) | 1.4 | — | — |
| **SAM/AA** | 0.2 | 0.2 | 0.1 | 0.3 | 1.0 | **2.2** | **1.8** |
| **Helo** | 1.3 | 1.6 | **2.0** | 1.6 | **0.3 (muere vs SAM)** | 1.0 | 0.5 |
| **Caza** | 1.0 | 1.3 | 1.5 | 1.5 | **0.4 (SEAD arriesgado)** | 1.6 | 1.0 |

Lecturas clave (las "cosas lógicas" pedidas):
- **Tanque vs infantería en campo abierto = masacre** (como caballería vs arqueros).
- **Tanque que entra a zona urbana/bosque con infantería = sale gravemente dañado**
  (ATGM/emboscada — el equivalente de la caballería contra piqueros).
- **Helo caza tanques… y el SAM caza helos.** Sin SAM enemigo, el aire domina; con SAM
  vivo, mandar aire primero es un error (primero se suprime la defensa aérea).
- **Artillería devastadora contra objetivos estáticos**; si un contingente rápido la alcanza,
  muere (defensa 6). Mantenerla protegida es decisión del jugador.
- La matriz vive en **JSON data-driven** (no hardcodeada) para balancear sin recompilar.

## 5. Modelo de datos (extensión de `Content/Data/Units/Units.json`)

Cada tipo de unidad gana campos:

```json
{
  "id": "mbt",
  "class": "armor",           // infantry|light_vehicle|armor|artillery|air_defense|helo|aircraft
  "soft_attack": 30,           // daño a objetivos blandos (infantería, artillería)
  "hard_attack": 24,           // daño a blindaje
  "armor": 20,                 // mitiga soft_attack casi entero, hard_attack parcial
  "range_m": 2400,             // alcance de fuego directo
  "speed": 46,                 // velocidad táctica
  "morale": 60,
  "is_indirect": false,        // artillería: dispara sin línea de visión, con retardo
  "is_air": false,             // helo/caza: capa aérea (solo AA y cazas les pegan)
  "terrain_mod": { "urban": 0.6, "forest": 0.7, "open": 1.2 }
}
```

`counter_matrix.json` aparte con la tabla de la sección 4 (atacante→defensor→multiplicador,
con variante por terreno donde aplica).

## 6. Fases de implementación (incrementales y verificables)

**F1 — Núcleo de contingentes y órdenes** *(la base jugable, y ya DECENTE a la vista)*
- Contingentes desde la composición real del ejército (agrupar por tipo).
- **Formaciones instanciadas** (estructural, no cosmético): un contingente de 50 de
  infantería son 50 instancias en cuadrícula que se mueven juntas; 4 tanques en cuña.
  Al perder salud DESAPARECEN individuos (ves la unidad reducirse en vivo). Esto es el
  70% de que una batalla "parezca batalla" y con instancing es casi gratis.
- Input: clic selecciona contingente propio; clic derecho en suelo = mover; en enemigo = atacar.
- Tick: perseguir objetivo, entrar en alcance, intercambio de daño con `soft/hard attack`,
  `armor` y la matriz de contras (terreno "open" fijo en F1).
- **Feedback mínimo de combate**: trazadoras entre atacante y objetivo, humo en vehículos
  dañados, y los RESTOS se quedan en el campo (chatarra humeante cuenta la batalla).
- **UI de batalla**: barra inferior con cartas de contingente reutilizando los renders 3/4
  de unidades ya generados (mismo estilo del panel de ejército), salud/moral por contingente,
  anillo de selección y marcadores de orden (mover/atacar).
- Verificar: test automation — MBT vs infantería en abierto, gana MBT con >70% de salud;
  costes iguales infantería-ATGM vs MBT en urbano (F2) invierte el resultado.

**F2 — Terreno y posicionamiento** ✅ IMPLEMENTADA
- Parches circulares de terreno (`FWLTacticalTerrainPatch`: urbano/bosque) en el estado de
  batalla. Las batallas de campaña generan zona urbana sobre la línea defensora (el defensor
  defiende el asentamiento) y un bosque a mitad de campo; `AddTacticalTerrainPatch` permite
  escenarios a medida.
- Reglas: la EMBOSCADA nace de la cobertura del que dispara (infantería en urbano vs blindado
  1.4, vs vehículo ligero 1.6; bosque 1.1/1.2); la PROTECCIÓN, de la cobertura del que recibe
  (blindado vs infantería urbana 0.7; aviación ×0.6 y artillería ×0.75 contra urbano). Contra
  un objetivo en cobertura el fuego DIRECTO se recorta al borde del parche (urbano 380,
  bosque 500) — el tanque tiene que ENTRAR a la ciudad; artillería/naval tiran por elevación.
  Mantener posición (Idle) da bono defensivo ×0.85.
- Verificado: test `WorldLeader.Battle.TacticalTerrainUrbanFlip` — el MISMO matchup 4 MBT vs
  50 infantería que en abierto gana el tanque, en ciudad lo gana la infantería defensora.

**F3 — Capa indirecta y aérea** ✅ IMPLEMENTADA
- Artillería/naval: fuego INDIRECTO por salvas (`FWLTacticalShellState`): cada 4 s dispara
  contra la POSICIÓN actual del objetivo con tiempo de vuelo (1.2 s + distancia/700); al
  impactar daña en área (radio 160) a todo enemigo que siga allí — mata estáticos, falla
  contra móviles. Sin daño directo continuo. La vista dibuja el proyectil en arco balístico
  y deja cráter.
- Capa aérea: al aire SOLO le pegan SAM, cazas y buques (canal `aa_attack` en Units.json:
  sam 30, caza 16, buque 10, helo 4; contras tierra→aire = 0). El SAM dispara AUTOMÁTICO
  (sin orden) al aéreo enemigo más cercano dentro de su alcance: ese radio ES el paraguas.
- IA: no persigue lo que no puede dañar (un tanque ya no apunta a un caza).
- Verificado: `TacticalAirUmbrella` — helos vs blindados+SAM pierde el atacante; sin SAM el
  helo limpia blindados sin un rasguño. `TacticalIndirectFire` — la artillería demuele a un
  estático a 2200 de distancia por salvas.

**F4 — Moral, supresión y flanqueo**
- Moral baja por bajas, fuego de artillería (supresión) y flanqueo; `Routing` al romperse,
  recuperación fuera de combate. Bono de daño por atacar por flanco/retaguardia.
- Verificar: rodear un contingente lo rompe antes que des-gastarlo de frente.

**F5 — IA de batalla y paridad con auto-resolve**
- IA rival: evalúa la matriz (manda ATGM al bosque, protege su SAM, caza artillería con
  rápidos). El resultado esperado de la batalla manual ≈ probabilidades del auto-resolve
  (mismo modelo de fuerza), para que elegir "auto" no sea trampa ni castigo.

**F6 — Presentación (de "decente" a "buena")**
- **Modelos low-poly reales por el pipeline Blender ya probado**: igual que las ciudades
  salen de `gen_city.py` (modelos vertex-color coherentes con el mapa), un `gen_units.py`
  genera tanque/APC/soldado/obús/helo flat-shaded. Sin packs externos ni bloqueos de arte.
- Fogonazos e impactos (Niagara simple), cámara RTS pulida (zoom baja el pitch, shake leve
  con artillería), minimapa, y sonido al final (disparos, motores).

### Escalera visual (por impacto, para no aceptar una batalla fea)
1. Formaciones instanciadas + bajas visibles ......... F1 (70% del efecto, casi gratis)
2. Trazadoras + humo + restos en el campo ............ F1
3. Cartas de contingente con los renders ya hechos ... F1
4. Terreno con contexto (urbano/bosque/fuerte) ....... F2 (es mecánica, no adorno)
5. Modelos low-poly reales (gen_units.py) ............ F6
6. VFX/cámara/sonido ................................. F6

## 7. Integración con campaña (pipe existente)

`Atacar` (ya en la UI de fuerzas) → `EnterTacticalBattle(AttackerId, DefenderId)` →
batalla manual (o `AutoFinishTacticalBattle`) → `ExitTacticalBattle(true)` →
`ReconcileArmyFromTacticalBattle` aplica bajas y resultado a los ejércitos de campaña.
Requisito de guerra declarada y todo el flujo diplomático ya se validan en el backend.

## 8. Reglas del proyecto aplicadas

- Mismo patrón que el resto: **backend C++ (subsistema) manda; la vista solo presenta.**
- Datos en JSON (`Units.json`, `counter_matrix.json`): balancear sin recompilar.
- Cada fase con tests automation (como `WLBalanceTests`) — los matchups de la matriz son
  contratos verificables, no vibras.
- El hardware no es restricción (directiva): instancing generoso, sin miedo a N unidades.
