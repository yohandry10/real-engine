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
}
