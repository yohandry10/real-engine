// Copyright World Leader project. See ROADMAP.md.
//
// Geometria compartida de los botones del HUD de BATALLA TACTICA: el HUD
// (AWLCampaignHUD) los dibuja y el PlayerController hace hit-test con LAS MISMAS
// coordenadas (evita descuadre Canvas vs mouse, ver [[hud-canvas-hittest]]).

#pragma once

#include "CoreMinimal.h"

namespace WLTacticalHudLayout
{
	// Los dos botones viven abajo-derecha, uno sobre otro.
	inline FBox2D AutoResolveButton(float CanvasW, float CanvasH)
	{
		const float BtnW = 220.f;
		const float BtnH = 44.f;
		const float X = CanvasW - BtnW - 26.f;
		const float Y = CanvasH - BtnH * 2.f - 34.f;
		return FBox2D(FVector2D(X, Y), FVector2D(X + BtnW, Y + BtnH));
	}

	inline FBox2D ExitButton(float CanvasW, float CanvasH)
	{
		const float BtnW = 220.f;
		const float BtnH = 44.f;
		const float X = CanvasW - BtnW - 26.f;
		const float Y = CanvasH - BtnH - 24.f;
		return FBox2D(FVector2D(X, Y), FVector2D(X + BtnW, Y + BtnH));
	}

	// F1b: cartas de CONTINGENTE del jugador (fila centrada abajo, estilo Total War). El HUD las
	// dibuja y el PlayerController hace hit-test con estas MISMAS coordenadas (clic = seleccionar).
	inline float ContingentCardW() { return 150.f; }
	inline float ContingentCardH() { return 124.f; }

	inline FBox2D ContingentCardBox(float CanvasW, float CanvasH, int32 Index, int32 Count)
	{
		const float CardW = ContingentCardW();
		const float CardH = ContingentCardH();
		const float Gap = 10.f;
		const float TotalW = static_cast<float>(Count) * CardW + static_cast<float>(FMath::Max(0, Count - 1)) * Gap;
		const float X0 = FMath::Max(16.f, (CanvasW - TotalW) * 0.5f);
		const float Y = CanvasH - 34.f - CardH - 12.f;
		const float X = X0 + static_cast<float>(Index) * (CardW + Gap);
		return FBox2D(FVector2D(X, Y), FVector2D(X + CardW, Y + CardH));
	}
}
