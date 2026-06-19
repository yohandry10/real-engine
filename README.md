# World Leader

Juego PC de estrategia geopolítica moderna — ADN de *Rome Total War 2* aplicado a la guerra moderna.
**Motor:** Unreal Engine 5.8 + C++. El diseño completo está en [ROADMAP.md](ROADMAP.md) (GDD v2.1).

> Estado: **Phase 1 — Campaña estratégica MVP** (inicio). Phase 0 quedó cerrada
> como vertical slice base; ahora empieza la administración básica de provincias.

---

## Requisitos

- **Unreal Engine 5.8** (instalado vía Epic Games Launcher).
- **Visual Studio 2022** con el workload *"Game development with C++"* (Desarrollo de juegos con C++).
- Git + Git LFS.

## Cómo abrir el proyecto

1. Click derecho sobre `WorldLeader.uproject` → **Generate Visual Studio project files**.
2. Abrir `WorldLeader.sln` en Visual Studio y compilar la configuración **Development Editor | Win64**.
   (o doble click en `WorldLeader.uproject`, que ofrecerá compilar los módulos).
3. El editor abre con el módulo `WorldLeader` cargado.

## Estructura

```
WorldLeader.uproject        # Proyecto Unreal (EngineAssociation 5.8)
Source/
  WorldLeader.Target.cs       # Target del juego
  WorldLeaderEditor.Target.cs # Target del editor
  WorldLeader/
    WorldLeader.Build.cs      # Dependencias del módulo (incluye Json, UMG, DeveloperSettings)
    WorldLeader.(h|cpp)       # Módulo principal + categoría de log
    Balance/
      WLBalanceTypes.h        # Reglas editables de balance económico/calendario
      WLBalanceDataAsset.*    # DataAsset de balance para tuning sin recompilar
      WLBalanceSettings.*     # Project Settings -> World Leader Balance
      WLBalanceSubsystem.*    # Fuente runtime de reglas saneadas
    Core/
      WLGameTypes.h           # Enums + structs FWLProvinceData / FWLNationData
      WLConstants.h           # Legado; no contiene valores de balance
    Campaign/
      WLDataRegistry.(h|cpp)            # Carga JSON de provincias/naciones
      WLStrategicTickSubsystem.(h|cpp)  # Motor de campaña (tick mensual + economía)
      WLCampaignPlayerController.(h|cpp)# Input de campaña: cámara RTS + selección
      WLCampaignGameInstance.(h|cpp)    # Estado global: nación elegida / campaña activa
      WLCampaignGameMode.(h|cpp)        # Arranque del mundo de campaña
    Economy/
      WLEconomyLibrary.(h|cpp)          # Cálculo de ingreso/upkeep/balance provincial
    Map/
      WLWorldMap.(h|cpp)                # Mapa GeoJSON, labels LOD y markers de país/provincia
    UI/
      WLCampaignHUD.(h|cpp)             # HUD temporal Canvas de campaña
      WLMainMenuWidget.(h|cpp)          # Menú UMG: nueva campaña + selección de nación
    Save/
      WLLocalSaveGame.(h|cpp)           # SaveGame local de campaña
Scripts/
  CreateDefaultBalanceAsset.py # Crea/guarda el DataAsset de balance por comando
Content/Data/
  Balance/DA_WLBalance.uasset  # Balance editable activo
  Provinces/Provinces.json    # 5 provincias de prueba (Venezuela + Colombia)
  Nations/Nations.json        # 2 naciones de prueba
  Geo/Countries.geojson       # Siluetas reales de países (Natural Earth)
Config/                       # DefaultEngine.ini, DefaultGame.ini
```

## Qué funciona ya

- **Sistema de datos** (`UWLDataRegistry`): carga provincias y naciones desde JSON al iniciar
  la GameInstance y las expone a C++ y Blueprint.
- **Economía simple** (`UWLEconomyLibrary`): ingreso = recursos·precio + impuesto poblacional;
  gasto = infraestructura·factor; balance = ingreso − gasto. El estado runtime aplica además
  modificadores por orden público.
- **Balance editable** (`UWLBalanceDataAsset` + `UWLBalanceSubsystem`): precios,
  impuesto poblacional, upkeep de infraestructura y calendario salen de reglas saneadas.
  El asset activo es `/Game/Data/Balance/DA_WLBalance`; si falta, usa defaults deterministas
  equivalentes a la vertical slice.
- **Tick estratégico** (`UWLStrategicTickSubsystem`): `AdvanceMonth()` avanza la fecha
  (1 tick = 1 mes), recalcula el tesoro de cada nación, actualiza población/orden público por
  provincia y emite `OnMonthAdvanced`.
- **Mapa estratégico** (`AWLWorldMap`): renderiza siluetas reales de Norteamérica y
  Sudamérica desde GeoJSON, colorea naciones jugables y mantiene markers seleccionables para
  países pequeños/islas sin saturar el mapa con texto.
- **Cámara de campaña** (`AWLCampaignPlayerController`): edge scrolling, drag panning y zoom
  con límites sobre el mapa renderizado.
- **Selección básica de país**: click izquierdo sobre marker de país y panel temporal en HUD
  con nombre, ISO, continente y datos de nación cuando el país existe en el prototipo
  (tesoro, balance mensual, capital y provincias).
- **Flujo formal de campaña**: `Main Menu -> Nueva campaña -> elegir nación -> mapa de campaña`.
  La campaña no spawnea el mapa hasta que se elige nación; la cámara inicial se centra en la
  capital definida en `Provinces.json`.
- **SaveGame local**: guarda/carga nación seleccionada, fecha estratégica, tesoros, edificios
  construidos, estado de provincias y ejércitos. El menú muestra `Cargar campaña` cuando existe
  un save y el HUD permite guardar con `F5`.
- **Administración básica de provincias**: markers de provincias desde `Provinces.json`, selección
  por click, panel de detalle económico y construcción inicial con `[B]` en provincias propias.
- **Estado provincial runtime**: población actual, orden público y controlador actual evolucionan
  durante la campaña y se reflejan en el balance económico mostrado en el HUD.
- **IA económica simple**: al avanzar mes, cada nación no controlada por el jugador intenta
  construir una mejora económica válida, respeta tesoro de reserva, orden público, retorno de
  inversión y compatibilidad entre edificio y recursos de la provincia.

## Próximos pasos (ver ROADMAP.md)

- Tunear `DA_WLBalance` con números de economía menos provisionales.
- Ampliar construcción a UI formal con cola/costes visibles.
- Diplomacia/estado de guerra mínimo antes de escalar conquistas.
