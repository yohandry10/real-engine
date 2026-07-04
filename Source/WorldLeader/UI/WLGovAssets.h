// Copyright World Leader project. See ROADMAP.md.
//
// Pipeline de ASSETS de la UI de gobierno cargados EN RUNTIME desde disco (PNG ->
// UTexture2D) SIN necesidad de importarlos en el editor. El juego queda "asset-ready":
// si el usuario deja los archivos en las carpetas esperadas, la UI los usa; si no,
// cae a un fallback procedural. Asi se puede subir el nivel visual con arte real
// (banderas, texturas de panel, iconos) sin tocar el pipeline de import del editor.
//
// Carpetas esperadas (relativas a la carpeta Content del proyecto):
//   UI/Flags/<ISO>.png     -> bandera de cada pais (BR.png, AR.png, US.png...)
//   UI/Portraits/<ID>.png   -> retrato exacto; pools leader/min/general/opposition/spy para dinamicos
//   UI/gov_panel_bg.png    -> textura de fondo del panel de gobierno (opcional)
//   UI/Icons/<nombre>.png  -> icono que sustituye al procedural (opcional)

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

namespace WLGovAssetsNS
{
	/** Carga (y cachea) un PNG desde <Content>/RelPath. Devuelve nullptr si no existe. */
	UTexture2D* LoadExternalTexture(const FString& RelPath);

	/** Bandera del pais: UI/Flags/<ISO>.png si existe, si no nullptr (la UI usa un emblema de color). */
	UTexture2D* GetFlag(const FString& Iso);

	/**
	 * Fondo del panel: UI/gov_panel_bg.png si el usuario lo dejo; si no, una textura generada en
	 * runtime (gradiente vertical + viñeta sutil) que da profundidad frente al relleno plano.
	 */
	UTexture2D* GetPanelBackground();

	/** True si hay al menos una bandera en disco (para decidir mostrar emblema-bandera vs chip de color). */
	bool HasFlags();
}
