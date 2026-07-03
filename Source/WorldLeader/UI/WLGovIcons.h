// Copyright World Leader project. See ROADMAP.md.
//
// Iconos de la UI de gobierno generados EN RUNTIME (UTexture2D rasterizado por
// codigo, sin assets externos ni import en el editor). Cada icono se dibuja una
// vez con anti-aliasing por cobertura y se cachea. Se muestran con UImage via
// MakeIcon(). Esto da iconografia real (moneda, poblacion, escudo, balanza...)
// que es lo que separa un HUD "de debug" de uno de juego.

#pragma once

#include "CoreMinimal.h"

class UImage;
class UWidgetTree;
class UTexture2D;

enum class EWLGovIcon : uint8
{
	Treasury,     // moneda
	Balance,      // barras (ingreso/gasto)
	Population,   // grupo de personas
	Provinces,    // pin de mapa
	Growth,       // flecha ascendente
	Order,        // escudo
	Capital,      // estrella
	Politics,     // balanza / justicia
	Diplomacy,    // globo
	Military,     // casco/escudo con cruz
	Approval,     // pulgar / corazon simplificado (circulo con check)
	Crisis        // triangulo de alerta
};

namespace WLGovIconsNS
{
	/** Textura del icono (cacheada por icono+tamano+color). Nunca nullptr salvo fallo de asignacion. */
	UTexture2D* GetIconTexture(EWLGovIcon Icon, int32 SizePx, const FLinearColor& Color);
}
