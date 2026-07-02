// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WLCampaignHUD.generated.h"

class UWLDataRegistry;
class UWLStrategicTickSubsystem;
class AWLCampaignPlayerController;
class UFont;

/**
 * HUD de campania. Dibuja el estado en pantalla con Canvas (sin assets UMG,
 * para la vertical slice): fecha, naciones, tesoros y controles. Es temporal;
 * en fases posteriores se sustituye por UMG/CommonUI (regla del roadmap).
 */
UCLASS()
class WORLDLEADER_API AWLCampaignHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Tamano del Canvas usado en el ultimo DrawHUD (espacio de dibujo del HUD). */
	FVector2D GetLastCanvasSize() const { return LastCanvasSize; }

private:
	UWLDataRegistry* GetRegistry() const;
	UWLStrategicTickSubsystem* GetTick() const;

	/** Barra de BATALLA TACTICA: reemplaza el HUD de campana mientras la batalla 3D esta activa. */
	void DrawTacticalBattleHud(const AWLCampaignPlayerController* PC, float W, float H, UFont* Font, UFont* SmallFont);

	FVector2D LastCanvasSize = FVector2D::ZeroVector;
};
