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


4) RETRATOS DE PERSONAJES  ->  Content/UI/Portraits/<CharacterId>.png
----------------------------------------------------------------------
Retratos opacos 4:5 para las tarjetas de personajes. El loader busca primero el
archivo exacto por ID de personaje, por ejemplo:
  Content/UI/Portraits/CO-GEN-PADILLA.png
  Content/UI/Portraits/VE-GEN-ZAMORA.png

Si no existe retrato exacto, la UI usa pools por rol con hash estable del ID.
Eso permite cubrir presidentes/lideres, gabinetes, generales y oposicion para
todos los paises cargados sin dibujar un archivo exacto por cada personaje:

  Lideres/presidentes:
    Content/UI/Portraits/leader_01.png ... leader_20.png

  Ministros/candidatos:
    Content/UI/Portraits/minister_01.png ... minister_150.png

  Generales:
  Content/UI/Portraits/general_01.png
  Content/UI/Portraits/general_02.png
    ...
    Content/UI/Portraits/general_19.png

  Oposicion:
    Content/UI/Portraits/opposition_01.png ... opposition_15.png

  Inteligencia/espias:
    Content/UI/Portraits/spy_01.png ... spy_05.png

Formato recomendado: PNG 512x640 px, fondo opaco, sin texto ni marco horneado.


Nota: estas rutas son relativas a la carpeta Content del proyecto
(C:/Users/PC/Desktop/rome-actual/Content/UI/...).
