# World Leader

Juego PC de estrategia geopolítica moderna — ADN de *Rome Total War 2* aplicado a la guerra moderna.
**Motor:** Unreal Engine 5.8 + C++. El diseño completo está en [ROADMAP.md](ROADMAP.md) (GDD v2.1).

> Estado: **Phase 0 — Unreal Foundation** (en curso). Esqueleto C++ + datos de prueba listos;
> pendiente abrir en el editor para mapa y menú.

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
    WorldLeader.Build.cs      # Dependencias del módulo (incluye Json)
    WorldLeader.(h|cpp)       # Módulo principal + categoría de log
    Core/
      WLGameTypes.h           # Enums + structs FWLProvinceData / FWLNationData
      WLConstants.h           # Coeficientes económicos semilla (provisional)
    Campaign/
      WLDataRegistry.(h|cpp)            # Carga JSON de provincias/naciones
      WLStrategicTickSubsystem.(h|cpp)  # Motor de campaña (tick mensual + economía)
    Economy/
      WLEconomyLibrary.(h|cpp)          # Cálculo de ingreso/upkeep/balance provincial
Content/Data/
  Provinces/Provinces.json    # 5 provincias de prueba (Venezuela + Colombia)
  Nations/Nations.json        # 2 naciones de prueba
Config/                       # DefaultEngine.ini, DefaultGame.ini
```

## Qué funciona ya (Phase 0)

- **Sistema de datos** (`UWLDataRegistry`): carga provincias y naciones desde JSON al iniciar
  la GameInstance y las expone a C++ y Blueprint.
- **Economía simple** (`UWLEconomyLibrary`): ingreso = recursos·precio + impuesto poblacional;
  gasto = infraestructura·factor; balance = ingreso − gasto.
- **Tick estratégico** (`UWLStrategicTickSubsystem`): `AdvanceMonth()` avanza la fecha
  (1 tick = 1 mes), recalcula el tesoro de cada nación a partir de sus provincias y emite
  `OnMonthAdvanced`.

## Próximos pasos (ver ROADMAP.md)

- Mapa estratégico y selección de nación (UMG/CommonUI) — requiere el editor.
- Migrar coeficientes de `WLConstants.h` a un DataAsset de balance editable.
- Construcción de edificios, orden público, IA económica.
