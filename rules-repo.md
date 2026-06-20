# WORLD LEADER - Reglas de arquitectura y mantenimiento

Este repositorio es un proyecto Unreal Engine 5.x de estrategia geopolitica PC.
La prioridad no es solo que compile: el codigo debe poder crecer hacia un juego
real sin convertirse en archivos gigantes, Blueprints/C++ mezclados sin criterio o
pantallas tecnicas imposibles de mantener.

## 1. Regla principal

No se aceptan soluciones que funcionen a costa de volver inmantenible el repo.

Antes de agregar codigo:

1. Lee el modulo existente.
2. Identifica la responsabilidad correcta.
3. Reutiliza datos, clases y patrones ya existentes.
4. Mantiene separadas reglas de campania, presentacion visual, UI y datos.
5. Si el archivo destino ya esta grande, extrae primero.

## 2. Limites de tamano

Estos limites aplican a codigo nuevo y a modificaciones relevantes:

- Objetivo por archivo `.cpp`: 250 a 500 lineas.
- Maximo normal por archivo `.cpp`: 700 lineas.
- Maximo excepcional: 800 lineas, solo con justificacion clara.
- Un archivo sobre 800 lineas es deuda tecnica activa.
- Un archivo sobre 1000 lineas debe partirse antes de agregar features.
- Headers `.h`: objetivo menor a 250 lineas.
- Funciones: objetivo menor a 80 lineas.
- Funciones sobre 120 lineas deben extraer helpers o clases.

Regla operativa:

- No agregar mas logica a un archivo de 700+ lineas salvo que el cambio sea una
  extraccion o una correccion puntual.
- No crear archivos de 1000, 1300 o 1500 lineas.
- Si una feature necesita mucho codigo, crear componentes/modulos pequenos.

Deuda actual detectada:

- `Source/WorldLeader/Presentation/WLCampaign3DView.cpp` supera 1400 lineas.
- `Source/WorldLeader/Campaign/WLStrategicTickSubsystem.cpp` supera 790 lineas.
- `Source/WorldLeader/Campaign/WLCampaignPlayerController.cpp` supera 760 lineas.
- `Source/WorldLeader/UI/WLFrontendMenuWidget.cpp` supera 740 lineas.
- `Source/WorldLeader/Map/WLWorldMap.cpp` supera 600 lineas.

La siguiente iteracion sobre Campaign 3D debe partir `WLCampaign3DView.cpp`; no
se debe seguir agregando render, datos, camara y construccion visual en ese mismo
archivo.

## 3. Arquitectura del juego

Separar estas capas:

- Estado de campania: paises, provincias, ownership, economia, rutas, ejercitos.
- Presentacion Campaign 3D: terreno, mar, biomas, asentamientos, camara.
- Presentacion Diplomacy Map: mapa politico/cartografico y seleccion regional.
- UI/HUD: UMG/CommonUI, botones, paneles, estado visible.
- Datos: DataTables, JSON/CSV, assets y localizacion.

Reglas:

- La vista 3D no decide reglas de juego.
- Diplomacia no debe ser la vista principal de campania.
- Campaign 3D y Diplomacy Map deben leer el mismo estado compartido.
- GameMode orquesta el arranque; no debe contener construccion visual pesada.
- PlayerController maneja input y cambio de vista; no debe construir mundo.
- HUD dibuja informacion minima; no debe contener logica de simulacion.

## 4. Campaign 3D

La Campaign 3D debe sentirse como vista de campania de juego, no como GIS.

Permitido:

- Terreno regional 3D.
- Mar con material/mesh propio.
- Relieve, costa, rios, caminos, asentamientos fisicos.
- Instanced meshes para vegetacion y elementos repetibles.
- Datos geograficos usados como entrada, no como look final.

No permitido como resultado final:

- Fondo negro.
- Poligonos planos coloreados como vista principal.
- Fronteras politicas dominando la composicion.
- Lineas debug/procedurales como lenguaje visual principal.
- Iconos 2D sustituyendo ciudades fisicas.
- Un solo archivo gigante que construye todo.

Particion esperada para la vista 3D:

- `WLCampaign3DView`: actor coordinador fino.
- `WLCampaignRegionGeometry`: carga/proyeccion/recorte de geometria.
- `WLCampaignTerrainBuilder`: malla de terreno y biomas.
- `WLCampaignWaterBuilder`: mar, costa y rios.
- `WLCampaignSettlementBuilder`: ciudades, puertos y marcadores fisicos.
- `WLCampaignRouteBuilder`: caminos/rutas.
- `WLCampaignCameraRig`: camara, zoom, pan y limites.
- `WLCampaignVisualStyle`: colores, materiales y parametros visuales.

## 5. Diplomacy Map

El mapa politico existente se conserva como base de Diplomacy Map.

Reglas:

- No borrar el mapa politico actual.
- No usarlo como vista principal de campania.
- Mantenerlo como pantalla separada.
- Mejorarlo como cartografia: ownership, seleccion, labels y overlays.
- No mezclar materiales improvisados de Campaign 3D con diplomacia.

## 6. UI y frontend

UMG/CommonUI se usa para:

- Navegacion.
- Botones.
- HUD.
- Paneles.
- Estados de foco/hover.
- Barras y textos.

No se usa UMG para pintar mapas complejos ni para simular terreno.

Reglas UI:

- El menu principal debe sentirse como juego premium, no dashboard web.
- Los textos de UI deben usar localization keys cuando sea parte del producto.
- Botones y estados deben ser consistentes.
- El HUD de campania debe ser discreto; no debe tapar la lectura del mundo.
- Evitar widgets gigantes con toda la pantalla en un solo archivo.

## 7. Datos y assets

Los datos de juego deben vivir fuera de la presentacion:

- Paises.
- Provincias.
- Ownership.
- Ciudades.
- Rutas.
- Ejercitos.
- Localizacion.

Reglas:

- No duplicar datos entre Campaign 3D y Diplomacy Map.
- No hardcodear datos extensos si pueden ir a DataTables/JSON/CSV.
- Los hardcodes temporales deben estar aislados y marcados como placeholder.
- Assets raster/video/meshes deben ser preferidos para arte final.
- Procedural es aceptable para vertical slices, no como excusa para debug look.

## 8. C++ Unreal

Reglas C++:

- `Tick` desactivado por defecto; activarlo solo si es necesario.
- Evitar logica pesada en `DrawHUD`, `OnPaint` o loops de UI.
- Usar `UInstancedStaticMeshComponent` para elementos repetidos.
- Usar componentes o builders para construccion visual pesada.
- Mantener headers pequenos; mover implementacion a `.cpp`.
- Evitar singletons globales innecesarios.
- Validar punteros Unreal antes de usarlos.
- Separar spawning, configuracion visual y datos.

## 9. Validacion minima

Para cambios C++:

```powershell
$uproject = Resolve-Path 'WorldLeader.uproject'
$build = 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat'

Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
& $build WorldLeaderEditor Win64 Development -Project="$uproject" -WaitMutex -NoHotReloadFromIDE
```

Ejecutar siempre desde PowerShell en la raiz del repo
(`C:\Users\PC\Desktop\rome-actual`). No invocar UBT mezclando `cmd`, cadenas raras
o wrappers improvisados.

Regla practica de diagnostico:

- Cerrar Unreal antes de compilar. Si Live Coding esta bloqueando DLLs, tambien cerrarlo.
- Correr `Build.bat` normal desde la raiz del repo.
- Si falla antes de entrar a UHT/C++, no asumir que es codigo: tratarlo como problema de
  entorno, permisos, caches o procesos bloqueando.
- Si aparecen `0xE0434352`, `Trace.uba`, `AppData`, `Zen` o `UnrealBuildTool` antes del
  primer error real de C++/UHT, clasificarlo como problema de entorno y resolver eso antes
  de tocar codigo.
- Si falla dentro de UHT/C++, leer el primer error real y corregir ese punto.

Para levantar Campaign 3D en Standalone desde consola, usar siempre
`UnrealEditor.exe -game` con el GameMode de campania y auto-iniciar una nacion:

```powershell
$p = Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
  -ArgumentList @(
    "C:\Users\PC\Desktop\rome-actual\WorldLeader.uproject",
    "/Engine/Maps/Entry?game=/Script/WorldLeader.WLCampaignGameMode",
    "-game",
    "-windowed",
    "-ResX=1600",
    "-ResY=900",
    "-WLAutoStart=CO",
    "-log"
  ) -PassThru
$p.Id
```

Para abrir directamente la vista America en Standalone, agregar `-WLAmericaView` a los
argumentos:

```powershell
"-WLAmericaView"
```

Comandos de verificacion rapida despues de levantar Standalone:

- Probar `+`: debe acercar, bajando la altura de camara.
- Probar `-`: debe alejar, subiendo la altura de camara.
- Probar `[G] America`: debe mostrar America completa.
- Probar `[F] Teatro`: debe volver al teatro Colombia/Venezuela.
- Si hay panel contextual abierto, la `X` debe cerrarlo sin seleccionar el mapa detras.
- Revisar logs con:

```powershell
rg -n "Campaign3D zoom|panel closed|selected territory|selected city|selected force" Saved/Logs/WorldLeader.log
```

Para cambios visuales:

- En este repo, usar Standalone Game lanzado con `UnrealEditor.exe -game` como regla
  para validar gameplay/UI runtime.
- No aceptar capturas del editor, del viewport con paneles, ni del escritorio como evidencia final.
- Capturar evidencia visual.
- Verificar que no haya paneles del editor tapando el resultado.
- Si el cambio empeora la lectura visual, revertir o ajustar antes de cerrar.
- Regla permanente: cambio que se hace, cambio que se valida contra los criterios de rechazo
  aplicables antes de continuar con mas cambios. Si no hay evidencia runtime/visual suficiente,
  no declarar exito ni avanzar a otro lote; dejar el estado como no validado o bloqueado.
- Regla permanente del proyecto: despues de cada cambio relevante, revisar el diff/estado,
  compilar y validar el resultado en runtime. Para Campaign 3D, la validacion aceptable es
  Standalone Game con zoom, pan y controles basicos probados.
- Si Unreal Editor, Live Coding o un binario bloqueado impiden linkear/compilar, no contar
  esa revision como exitosa; cerrar/desbloquear el proceso y volver a compilar antes de cerrar.

## 10. Git

Reglas:

- No commitear archivos no relacionados.
- No incluir notas locales si no son parte del repo.
- Antes de commit: `git status -sb` y `git diff --stat`.
- Commits pequenos cuando sea posible.
- Si una tarea toca arquitectura y visuales a la vez, separar si el estado lo permite.
- No usar `git reset --hard` ni revertir trabajo ajeno sin permiso explicito.

## 11. Criterio de aceptacion de mantenimiento

Una entrega no esta lista si:

- Agrega mas codigo a un archivo de 1000+ lineas sin partirlo.
- Mezcla reglas de juego con rendering.
- Duplica estado entre vistas.
- Introduce una pantalla que parece debug.
- Compila pero no tiene captura cuando el cambio es visual.
- Deja TODOs criticos sin plan.

Una entrega si esta lista cuando:

- Compila.
- Mantiene responsabilidades separadas.
- Tiene archivos de tamano razonable o reduce deuda.
- Conserva datos compartidos.
- Incluye evidencia visual si aplica.
- Deja claro que es placeholder y que es pipeline definitivo.
