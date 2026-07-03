WORLD LEADER — ASSETS DE UI (se cargan en RUNTIME desde disco, sin importar en el editor)
=========================================================================================

La ventana GOBIERNO usa arte real si dejas los archivos aqui. No hace falta importarlos
al editor: se cargan como PNG desde disco al abrir el juego. Si un archivo no existe, la
UI cae a un fallback (chip de color / textura procedural) y sigue funcionando.

Tras dejar/actualizar archivos: reinicia el Play (PIE) para que se recarguen.


1) BANDERAS DE PAISES  ->  Content/UI/Flags/<ISO>.png
-----------------------------------------------------
Una imagen por pais, nombrada por su codigo ISO 3166-1 alpha-2 (mayus o minus da igual).
Formato recomendado: PNG, ~120x80 px (proporcion 3:2), fondo opaco.

Codigos que usa el juego (America):
  Norte/Centro:  US CA MX GL  GT HN NI CR PA SV BZ
  Caribe:        CU DO HT JM PR TT BS  AG BB DM GD KN LC VC
  Sudamerica:    CO VE EC GY SR GF  BR  PE BO CL  AR UY PY

Ejemplos de archivo:  Content/UI/Flags/BR.png   Content/UI/Flags/ar.png   Content/UI/Flags/US.png

Packs libres recomendados (dominio publico / CC0), renombra a <ISO>.png:
  - "flag-icons" (github.com/lipis/flag-icons)  -> ya vienen como "br.png", "ar.png"...
  - Banderas de Wikimedia Commons (PD).


2) FONDO DEL PANEL (opcional)  ->  Content/UI/gov_panel_bg.png
-------------------------------------------------------------
Textura de fondo del panel de gobierno (metal oscuro, pergamino, ruido...). Se estira al panel.
Si no lo pones, se usa un gradiente+viñeta generado en runtime (ya da profundidad).
Formato: PNG, 512x512 o 1024x1024, oscuro para que el texto claro se lea.


3) ICONOS (opcional)  ->  Content/UI/Icons/<nombre>.png
-------------------------------------------------------
(Pendiente de cablear) Sustituyen a los iconos vectoriales generados en runtime.


Nota: estas rutas son relativas a la carpeta Content del proyecto
(C:/Users/PC/Desktop/rome-actual/Content/UI/...).
