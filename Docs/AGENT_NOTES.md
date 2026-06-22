# AGENT NOTES — World Leader Campaign 3D (problemas y soluciones)

> Contexto para agentes (Codex/Claude) que trabajen en la vista Campaign 3D.
> Recopila los **muros con los que ya chocamos** y **cómo se resuelven**, para no
> repetirlos. Léelo antes de tocar `Source/WorldLeader/Presentation/WLCampaign3D*`.
> Planes/estado: `docs/CAMPAIGN3D_PLAN.md` y `docs/CAMPAIGN3D_PROGRESS.md`.

---

## 1. Build / editor (flujo obligatorio)
- **Cerrar el editor ANTES de compilar.** Live Coding bloquea las DLL → el build
  falla con "Unable to build while Live Coding is active". Cerrar `UnrealEditor.exe`,
  compilar, relanzar.
- Comando de build (incremental ~25-60s):
  `"/c/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" WorldLeaderEditor Win64 Development -project="C:/Users/PC/Desktop/rome-actual/WorldLeader.uproject" -waitmutex`
- Relanzar: `Start-Process "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList '"C:\Users\PC\Desktop\rome-actual\WorldLeader.uproject"'`
- Hardware del usuario **por debajo de specs UE5** → preferir low-poly, texturas
  bajas, detalle solo cerca de cámara.

## 2. ⚠️ EL MAPA SE DIBUJA PLANO / SIN ILUMINACIÓN (el muro más grande)
- El terreno usa **`VertexColorMaterial` (UNLIT)**: el color se ve siempre, sin luz.
- Por eso, al meter **mallas de assets (LIT/PBR)** (edificios, árboles), **salían
  NEGRAS**: la escena no tenía **luz ambiental** (el `SkyLight` por defecto captura
  el fondo negro de la escena → 0 ambiente).
- **Diagnóstico que lo confirmó:** abrir la malla en el visor del asset → se ve
  perfecta (el visor tiene HDRI). En el mapa, negra. ⇒ es la escena, no el asset.
- **SOLUCIÓN (en `SetupLightingAndCamera`)**: dar ambiente real con un **cubemap**:
  ```cpp
  SkyComp->SourceType = SLS_SpecifiedCubemap;
  SkyComp->Cubemap = LoadObject<UTextureCube>(nullptr,
      TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
  SkyComp->SetIntensity(1.6f);
  SkyComp->RecaptureSky();
  ```
  + luz direccional con intensidad/ángulo hacia la cámara. Incluir
  `Components/SkyLightComponent.h`, `Components/DirectionalLightComponent.h`,
  `Engine/TextureCube.h`.
- **REGLA:** cualquier asset *lit* nuevo necesita este ambiente. Si algo sale negro,
  es esto.

## 3. Escala del mundo y relieve
- `ProjectLonLat`: `X = (Lat-centerLat)*GeoScale`, `Y = (Lon-centerLon)*GeoScale`,
  `Z = SampleTerrainHeight(Lon,Lat)`. **`GeoScale = 9000` u/grado (~81 u/km).**
  +X = Norte, +Y = Este.
- El terreno (y todo lo que va encima: ciudades, caminos, labels) usa `ProjectLonLat`.
  La malla del terreno = `ProjectLonLat` + `VisualBiomeZOffset(bioma)`.
- **Lo vertical está exagerado** para que se lea en el mapa (montañas/edificios de
  cientos-miles de unidades). No es escala real.
- `SampleTerrainHeight` = modelo de relieve continental (Andes, Rocosas, Sierra
  Madre, Apalaches). `ShadeTerrainVertex` aclara por altura.

## 4. Cámara
- Originalmente **cenital** (pitch -90). Ahora **pitch DINÁMICO** en `ApplyZoomLOD`:
  casi cenital al alejar, inclinada (~-46°) al acercar (estilo Total War). Yaw 0
  (mira al Norte). La navegación/selección usan **deproyección** (rayo real) →
  funcionan a cualquier ángulo.
- HUD: bug de hit-test bajo **letterbox** (cámara 16:9 → el Canvas es un rect
  centrado en un viewport más alto). El mouse→canvas necesita **offset**
  `(viewport-canvas)/2`. Ver `WLCampaignPlayerController` (`GetCampaignCanvasMetrics`).

## 5. Biomas / labels / ciudades (estándares ya aplicados)
- **Biomas por GEOGRAFÍA (lon/lat), no por país** (`ClassifyVisualBiome`).
- Labels por zoom: **país** al alejar (Region/Global), **ciudad** solo al acercar
  (`Labels` vs `SettlementLabels`).
- Ciudades: builder procedural (cajas) en `WLCampaignSettlementBuilder`. Para **VE**
  (vertical slice) se mantiene esa base procedural y `BuildMeshCity` agrega encima
  edificios reales de `Cartoon_City_Free` como `UStaticMeshComponent` normales
  (no ISM), escalados por *bounds* con tope de esbeltez.
- No usar calles/veredas/palmeras del pack como `UInstancedStaticMeshComponent`:
  sus materiales no anuncian `InstancedStaticMeshes` y en Standalone renderizan
  como bloques negros/default. Si se reactivan, convertirlos a componentes normales
  o crear materiales compatibles explícitos.
- Caminos inter-ciudad: `BuildIntercityRoads` (MST por país) drapean sobre el relieve.
  Para **VE** hay además `BuildVenezuelaRoadAssetLayer`: usa `SM_road_001` del pack
  como `UStaticMeshComponent` en tramos cortos sobre las rutas curadas.
- No usar `USplineMeshComponent` con materiales de `Cartoon_City_Free`: los materiales
  no anuncian `SplineMeshes` y Standalone cae al material default/negro. La solución
  validada es mesh estático segmentado + material nativo del asset.

## 6. Assets (Fab) — cómo importar (ver también memoria `fab-asset-import`)
- Usar el **panel Fab DENTRO del editor** (Ventanas → Fab). El "Añadir al proyecto"
  del **Launcher NO ve el proyecto** (está en Desktop, no registrado) → primero
  abrir el `.uproject` una vez **desde el Launcher** (Project Browser → Explorar).
- **Megascans NO es gratis para UE5** (2026): solo tier "UEFN-referencia". Usar
  **packs low-poly de comunidad** (licencia **Personal = Gratis**, filtro Precio→Gratis).
- Al "Añadir al proyecto": sale "Recurso no compatible con 5.8" → elegir la **versión
  más alta** (p.ej. 5.7); importa bien. Caen en `Content/<Pack>/`.
- Packs ya en el proyecto: `Content/VRS_LowPolyNatureEssentials` (vegetación),
  `Content/Cartoon_City_Free` (edificios/calles/autos, **3 materiales: M_Color,
  M_Glass, M_Color_Glossy** — se ven perfectos en el visor).
- Cargar en C++ por ruta: `/Game/Cartoon_City_Free/Meshes/Buildings/SM_...` con
  `ConstructorHelpers::FObjectFinder` (constructor) o `LoadObject` (runtime). NO
  aplicarles color plano (perder sus materiales) — usar sus materiales nativos.
- **Binarios de assets NO se commitean** (inflan el repo; usar Git LFS si se quiere
  versionar). Quedan en disco local; el código los referencia por ruta.

## 6.1 Validación visual Standalone en sandbox Codex
- Para validar ciudades sin depender de capturas de escritorio, existen flags de
  screenshot en `AWLCampaign3DView`:
  `-WLCityVisualTest=Caracas -WLCityVisualHeight=52000 -WLCityVisualScreenshot=65
  -WLCityVisualQuitAfterScreenshot`.
- En el sandbox actual, agregar también:
  `-unattended -DDC-ForceMemoryCache -ShaderWorkingDir=C:\Users\PC\Desktop\rome-actual\Intermediate\CodexShaderWorkingDir`.
  Sin `ShaderWorkingDir`, los materiales del pack pueden fallar al compilar shaders
  por permisos fuera del repo.
- `WLCityVisualTest` desactiva solo la colisión del terreno para acelerar la prueba
  visual en este PC/sandbox; el flujo normal conserva colisión.

## 7. computer-use (controlar el editor)
- El editor **NO se resuelve por nombre** ("Unreal Editor" no existe en Inicio).
  Pedir acceso por **basename**: `unrealeditor.exe`, `epicgameslauncher.exe`.
- **`open_application` LANZA UNA INSTANCIA NUEVA** del editor (no enfoca la abierta).
  No usarlo para enfocar; el editor recién relanzado ya viene al frente.
- La **barra de tareas es de Explorer** → no se puede clicar para cambiar de ventana.
- Cerrar instancias por proceso con PowerShell (filtrar por `MainWindowTitle`).

## 8. Errores de proceso ya resueltos
- Push rechazado (codex en remoto) → `git push --force-with-lease` cuando el usuario
  lo autoriza explícitamente.
- Mayúsculas de ruta en Windows (`docs` vs `Docs`) → usar `git add -A`.
- Aviso "SM6 / mapas de sombras virtuales" del proyecto: no bloquea; se puede
  descartar (no es la causa del negro — ese era el ambiente, sección 2).

## 9. Lecciones de proceso (qué evitar)
- **No prometer "Total War foto-realista" con procedural puro**: requiere assets +
  iluminación. Ser honesto con el techo (~6/10 procedural; assets para subir).
- **No iterar a ciegas en visual**: cada build+verificación cuesta. Diagnosticar la
  causa (p.ej. abrir el asset en el visor) ANTES de re-aplicar.
- **Checkpoints**: commit+push tras cada cambio verificado (el usuario lo pide).
- Trabajar **por país** (vertical slice: Venezuela primero), no todo el continente.
