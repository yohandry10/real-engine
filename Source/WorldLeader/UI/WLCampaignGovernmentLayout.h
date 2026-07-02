// Copyright World Leader project. See ROADMAP.md.
//
// Geometria del boton "GOBIERNO" de la barra superior (abre el consejo
// presidencial). La ventana en si ya NO se dibuja en Canvas: es un widget UMG
// (UWLGovernmentWidget) que el PlayerController crea/destruye. Aqui solo queda
// el rect del boton, que el HUD dibuja y el PlayerController usa para el
// hit-test; deben coincidir, por eso esta centralizado.

#pragma once

#include "CoreMinimal.h"

namespace WLGovernmentLayout
{
	/**
	 * Boton "GOBIERNO" de la barra superior. Se coloca a la IZQUIERDA de
	 * "Campana 3D" (que esta en W-386), en la misma fila (Y=58). W es el ancho
	 * del Canvas del HUD.
	 */
	inline FBox2D GovernmentButton(float W)
	{
		const float Width = 158.f;
		const float Height = 38.f;
		const float X = W - 552.f;   // 386 (Campana 3D) + 158 (ancho) + 8 (gap)
		const float Y = 58.f;
		return FBox2D(FVector2D(X, Y), FVector2D(X + Width, Y + Height));
	}
}
