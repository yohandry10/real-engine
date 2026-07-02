// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLCampaignHUD.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "UI/WLCampaignBuildingSlotData.h"
#include "UI/WLCampaignSelectionPanelData.h"
#include "UI/WLCampaignGovernmentLayout.h"
#include "UI/WLTacticalHudLayout.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

namespace
{
#include "UI/WLCampaignHUDPanelHelpers.inl"
#include "UI/WLCampaignHUDPanels.inl"
}

UWLDataRegistry* AWLCampaignHUD::GetRegistry() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

void AWLCampaignHUD::DrawTacticalBattleHud(const AWLCampaignPlayerController* PC, float W, float H, UFont* Font, UFont* SmallFont)
{
	const FLinearColor Ink(0.010f, 0.014f, 0.018f, 0.90f);
	const FLinearColor InkHard(0.006f, 0.010f, 0.014f, 0.95f);
	const FLinearColor Gold(0.90f, 0.72f, 0.28f, 1.f);
	const FLinearColor Text(0.90f, 0.94f, 0.95f, 1.f);
	const FLinearColor Muted(0.58f, 0.68f, 0.70f, 1.f);
	const FLinearColor Good(0.55f, 0.86f, 0.52f, 1.f);
	const FLinearColor Bad(0.94f, 0.52f, 0.42f, 1.f);

	// Cabecera.
	DrawRect(InkHard, 0.f, 0.f, W, 66.f);
	DrawText(PC->GetTacticalBattleHeadline(), Gold, 24.f, 12.f, Font, 1.0f);
	DrawText(PC->GetTacticalBattleStatusLine(), Text, 24.f, 38.f, SmallFont, 0.92f);

	// Barra inferior: unidad seleccionada + ayuda de controles.
	DrawRect(InkHard, 0.f, H - 34.f, W, 34.f);
	DrawText(PC->GetTacticalSelectedUnitInfo(), Text, 24.f, H - 24.f, SmallFont, 0.9f);

	// Panel de ayuda (izquierda).
	DrawRect(Ink, 24.f, 78.f, 360.f, 92.f);
	DrawText(TEXT("CONTROLES DE BATALLA"), Gold, 36.f, 86.f, SmallFont, 0.86f);
	DrawText(TEXT("Clic en tu unidad (azul): seleccionar"), Muted, 36.f, 106.f, SmallFont, 0.78f);
	DrawText(TEXT("Clic en suelo: mover   ·   Clic en enemigo (rojo): atacar"), Muted, 36.f, 124.f, SmallFont, 0.78f);
	DrawText(TEXT("La IA comanda al rival. AUTO-RESOLVER juega el resto."), Muted, 36.f, 142.f, SmallFont, 0.78f);

	// Botones (mismas coords que el hit-test del PlayerController).
	const bool bFinished = PC->IsTacticalBattleFinished();
	const FBox2D AutoBtn = WLTacticalHudLayout::AutoResolveButton(W, H);
	const FBox2D ExitBtn = WLTacticalHudLayout::ExitButton(W, H);
	if (!bFinished)
	{
		DrawRect(FLinearColor(0.30f, 0.24f, 0.08f, 0.96f), AutoBtn.Min.X, AutoBtn.Min.Y,
			AutoBtn.Max.X - AutoBtn.Min.X, AutoBtn.Max.Y - AutoBtn.Min.Y);
		DrawRect(Gold, AutoBtn.Min.X, AutoBtn.Min.Y, AutoBtn.Max.X - AutoBtn.Min.X, 3.f);
		DrawText(TEXT("AUTO-RESOLVER"), Text, AutoBtn.Min.X + 22.f, AutoBtn.Min.Y + 12.f, Font, 0.92f);
	}
	const FLinearColor ExitFill = bFinished ? FLinearColor(0.16f, 0.34f, 0.16f, 0.97f) : FLinearColor(0.34f, 0.14f, 0.12f, 0.96f);
	DrawRect(ExitFill, ExitBtn.Min.X, ExitBtn.Min.Y, ExitBtn.Max.X - ExitBtn.Min.X, ExitBtn.Max.Y - ExitBtn.Min.Y);
	DrawRect(Gold, ExitBtn.Min.X, ExitBtn.Min.Y, ExitBtn.Max.X - ExitBtn.Min.X, 3.f);
	DrawText(bFinished ? TEXT("TERMINAR Y APLICAR") : TEXT("RETIRARSE Y APLICAR"),
		Text, ExitBtn.Min.X + 22.f, ExitBtn.Min.Y + 12.f, Font, 0.92f);

	// Banner de resultado.
	if (bFinished)
	{
		const FString ResultText = PC->GetTacticalBattleResultText();
		const bool bWon = ResultText == TEXT("VICTORIA");
		const bool bDraw = ResultText == TEXT("EMPATE");
		const float BannerH = 96.f;
		const float BannerY = H * 0.5f - BannerH * 0.5f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.42f), 0.f, 0.f, W, H);
		DrawRect(bWon ? FLinearColor(0.15f, 0.13f, 0.04f, 0.97f)
			: (bDraw ? FLinearColor(0.10f, 0.11f, 0.12f, 0.97f) : FLinearColor(0.15f, 0.05f, 0.04f, 0.97f)),
			0.f, BannerY, W, BannerH);
		DrawRect(Gold, 0.f, BannerY, W, 3.f);
		DrawText(ResultText, bWon ? Gold : (bDraw ? Text : Bad), W * 0.5f - 70.f, BannerY + 20.f, Font, 1.7f);
		DrawText(TEXT("Pulsa TERMINAR Y APLICAR para volver a la campana con las bajas y la ocupacion."),
			Muted, W * 0.5f - 300.f, BannerY + 62.f, SmallFont, 0.95f);
	}
}

UWLStrategicTickSubsystem* AWLCampaignHUD::GetTick() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}

void AWLCampaignHUD::DrawHUD()
{
	Super::DrawHUD();

	if (Canvas)
	{
		// Guardamos el espacio de dibujo real del HUD para que el PlayerController
		// pueda hacer hit-test de los botones en estas mismas coordenadas (evita el
		// descuadre entre Canvas y mouse/viewport cuando hay escala DPI/ventana).
		LastCanvasSize = FVector2D(Canvas->ClipX, Canvas->ClipY);
	}

	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const UWLCampaignGameInstance* CampaignGI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!Registry || !Tick || !Canvas || !CampaignGI || !CampaignGI->HasActiveCampaign())
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : Font;
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float LineHeight = 19.f;
	const AWLCampaignPlayerController* PC = Cast<AWLCampaignPlayerController>(GetOwningPlayerController());

	// Batalla tactica 3D activa: el HUD de campana se sustituye por la barra de batalla.
	if (PC && PC->IsTacticalBattleActive())
	{
		DrawTacticalBattleHud(PC, W, H, Font, SmallFont);
		return;
	}

	const bool bDiplomacy = PC && PC->IsDiplomacyViewActive();

	const FLinearColor Ink(0.012f, 0.020f, 0.024f, 0.82f);
	const FLinearColor InkHard(0.006f, 0.010f, 0.014f, 0.92f);
	const FLinearColor Gold(0.88f, 0.70f, 0.26f, 1.f);
	const FLinearColor Text(0.88f, 0.93f, 0.94f, 1.f);
	const FLinearColor Muted(0.55f, 0.66f, 0.68f, 1.f);
	const FLinearColor Active(0.42f, 0.33f, 0.12f, 0.94f);

	DrawRect(InkHard, 0.f, 0.f, W, 38.f);
	DrawText(TEXT("WORLD LEADER"), Text, 28.f, 10.f, Font, 0.88f);
	DrawText(bDiplomacy ? TEXT("DIPLOMACIA / MAPA POLITICO") : TEXT("CAMPAIGN 3D / TEATRO COLOMBIA-VENEZUELA"),
		Gold, 172.f, 11.f, SmallFont, 0.92f);

	// F5.2: aviso de eventos pendientes — el juego te avisa, no hace falta abrir el tab correcto por azar.
	// El popup modal se abre solo al cerrar el mes; este banner permite reabrirlo con [E] si lo pospusiste.
	if (const UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (const UWLPoliticalSubsystem* Politics = GI->GetSubsystem<UWLPoliticalSubsystem>())
		{
			int32 PendingEvents = 0;
			for (const FWLPoliticalEventInstance& Event : Politics->GetQueuedEvents(CampaignGI->GetSelectedNationIso()))
			{
				if (!Event.bResolved)
				{
					++PendingEvents;
				}
			}
			if (PendingEvents > 0)
			{
				DrawRect(FLinearColor(0.30f, 0.23f, 0.06f, 0.96f), W * 0.5f - 250.f, 42.f, 500.f, 26.f);
				DrawText(FString::Printf(TEXT("%d evento%s esperan tu decision — pulsa [E]"),
					PendingEvents, PendingEvents == 1 ? TEXT("") : TEXT("s")),
					Gold, W * 0.5f - 232.f, 47.f, SmallFont, 0.95f);
			}
		}
	}

	const float ButtonY = 58.f;
	const float ButtonW = 158.f;
	const float ButtonH = 38.f;
	const float CampaignX = W - 386.f;
	const float DiplomacyX = W - 210.f;
	DrawRect(bDiplomacy ? Ink : Active, CampaignX, ButtonY, ButtonW, ButtonH);
	DrawRect(bDiplomacy ? Active : Ink, DiplomacyX, ButtonY, ButtonW, ButtonH);
	DrawText(TEXT("Campana 3D"), bDiplomacy ? Text : Gold, CampaignX + 18.f, ButtonY + 9.f, SmallFont, 1.0f);
	DrawText(TEXT("Diplomacia"), bDiplomacy ? Gold : Text, DiplomacyX + 25.f, ButtonY + 9.f, SmallFont, 1.0f);

	// Boton GOBIERNO (abre el consejo presidencial, estilo panel de faccion de Total War).
	const bool bGovernmentOpen = PC && PC->IsGovernmentWindowOpen();
	const FBox2D GovBtn = WLGovernmentLayout::GovernmentButton(W);
	DrawRect(bGovernmentOpen ? Active : Ink, GovBtn.Min.X, GovBtn.Min.Y, GovBtn.Max.X - GovBtn.Min.X, GovBtn.Max.Y - GovBtn.Min.Y);
	DrawRect(Gold, GovBtn.Min.X, GovBtn.Min.Y, 3.f, GovBtn.Max.Y - GovBtn.Min.Y);
	DrawText(TEXT("GOBIERNO"), bGovernmentOpen ? Gold : Text, GovBtn.Min.X + 24.f, GovBtn.Min.Y + 9.f, SmallFont, 1.0f);

	if (PC && !bDiplomacy)
	{
		const float ControlY = 102.f;
		const float ControlH = 34.f;
		const float ZoomW = 44.f;
		const float Gap = 8.f;
		const float ZoomInX = W - 444.f;
		const float ZoomOutX = ZoomInX + ZoomW + Gap;
		const float ResetX = ZoomOutX + ZoomW + Gap;
		const float ResetW = 84.f;
		const float FocusX = ResetX + ResetW + Gap;
		const float FocusW = 104.f;
		const float AmericaX = FocusX + FocusW + Gap;
		const float AmericaW = 112.f;
		const FLinearColor ControlFill(0.018f, 0.032f, 0.038f, 0.86f);
		const FLinearColor ControlAccent(0.56f, 0.45f, 0.20f, 0.74f);
		DrawRect(ControlFill, ZoomInX, ControlY, ZoomW, ControlH);
		DrawRect(ControlFill, ZoomOutX, ControlY, ZoomW, ControlH);
		DrawRect(ControlFill, ResetX, ControlY, ResetW, ControlH);
		DrawRect(ControlAccent, FocusX, ControlY, FocusW, ControlH);
		DrawRect(ControlAccent, AmericaX, ControlY, AmericaW, ControlH);
		DrawText(TEXT("+"), Gold, ZoomInX + 16.f, ControlY + 5.f, Font, 0.86f);
		DrawText(TEXT("-"), Text, ZoomOutX + 18.f, ControlY + 5.f, Font, 0.86f);
		DrawText(TEXT("Reset"), Text, ResetX + 15.f, ControlY + 8.f, SmallFont, 0.94f);
		DrawText(TEXT("Teatro"), Gold, FocusX + 18.f, ControlY + 8.f, SmallFont, 0.94f);
		DrawText(TEXT("America"), Gold, AmericaX + 17.f, ControlY + 8.f, SmallFont, 0.94f);
		DrawText(FString::Printf(TEXT("Zoom: %s  %.0fk"), *PC->GetCampaignZoomLODLabel(), PC->GetCampaignCameraHeight() / 1000.f),
			Muted, ZoomInX, ControlY + ControlH + 8.f, SmallFont, 0.82f);

		// Boton grande "AVANZAR DIA" abajo-derecha (estilo "Finalizar turno" de Total War). El hit-test esta en
		// WLCampaignPlayerControllerViews.cpp::TryHandleViewToggleClick con EL MISMO rect -> mantener en sync.
		const float AdvW = 220.f;
		const float AdvH = 50.f;
		const float AdvX = W - AdvW - 26.f;
		const float AdvY = Canvas->ClipY - AdvH - 28.f;
		DrawRect(FLinearColor(0.62f, 0.48f, 0.16f, 0.97f), AdvX, AdvY, AdvW, AdvH);
		DrawRect(FLinearColor(0.88f, 0.70f, 0.26f, 1.f), AdvX, AdvY, AdvW, 3.f);
		DrawText(TEXT("AVANZAR DIA"), FLinearColor(0.06f, 0.06f, 0.05f, 1.f), AdvX + 22.f, AdvY + 9.f, Font, 1.0f);
		DrawText(TEXT("[M] o clic aqui"), FLinearColor(0.14f, 0.11f, 0.05f, 1.f), AdvX + 22.f, AdvY + 32.f, SmallFont, 0.72f);
	}

	float X = 36.f;
	float Y = 62.f;
	DrawRect(Ink, X, Y, 332.f, 112.f);
	X += 18.f;
	Y += 14.f;

	FWLNationData SelectedNation;
	if (CampaignGI->GetSelectedNation(SelectedNation))
	{
		DrawText(FString::Printf(TEXT("%s (%s)"), *SelectedNation.Name, *SelectedNation.Iso), Gold, X, Y, Font, 0.92f);
		Y += LineHeight * 1.25f;
	}

	DrawText(FString::Printf(TEXT("Fecha: %02d/%02d/%d"), Tick->GetCurrentDay(), Tick->GetCurrentMonth(), Tick->GetCurrentYear()),
		FLinearColor(0.75f, 0.9f, 1.0f), X, Y, SmallFont);
	Y += LineHeight;

	if (SelectedNation.IsValid())
	{
		// El balance es MENSUAL; el avance por dias aplica 1/30 al tesoro cada dia.
		const int64 MonthlyBalance = Tick->GetMonthlyBalance(SelectedNation.Iso);
		DrawText(FString::Printf(TEXT("Tesoro: %lld   Balance/mes: %+lld (%+lld/dia)"),
			Tick->GetTreasury(SelectedNation.Iso), MonthlyBalance,
			static_cast<int64>(FMath::RoundToDouble(static_cast<double>(MonthlyBalance) / 30.0))),
			Text, X, Y, SmallFont);
		Y += LineHeight;
	}
	if (PC)
	{
		if (PC->HasLastActionMessage())
		{
			Y += LineHeight * 0.25f;
			DrawText(PC->GetLastActionMessage(),
				PC->WasLastActionSuccessful() ? FLinearColor(0.55f, 0.95f, 0.55f) : FLinearColor(0.95f, 0.45f, 0.35f),
				X, Y, SmallFont, 0.86f);
		}

		if (!bDiplomacy && PC->HasCampaignSelectionPanel())
		{
			if (PC->GetCampaignSelectionKind() == EWLCampaignSelectionKind::Force)
			{
				DrawCampaignForcePanel(this, Canvas, Font, SmallFont, PC);
			}
			else
			{
				DrawCampaignSelectionPanel(this, Canvas, Font, SmallFont, PC, Registry, Tick);
			}
		}
		else if (bDiplomacy && PC->HasSelectedProvince())
		{
			FWLProvinceData Province;
			if (Registry->GetProvince(PC->GetSelectedProvinceId(), Province))
			{
				const float PanelX = W - 442.f;
				float PanelY = bDiplomacy ? 118.f : 154.f;
				DrawRect(Ink, PanelX, PanelY, 404.f, 190.f);
				PanelY += 18.f;
				FWLProvinceRuntimeState ProvinceState;
				if (!Tick->GetProvinceState(Province.Id, ProvinceState))
				{
					ProvinceState.ProvinceId = Province.Id;
					ProvinceState.Population = Province.Population;
					ProvinceState.PublicOrder = Tick->GetBalanceRules().InitialPublicOrder;
				}

				DrawText(TEXT("PROVINCIA SELECCIONADA"), Gold, PanelX + 18.f, PanelY, SmallFont);
				PanelY += LineHeight * 1.2f;
				DrawText(FString::Printf(TEXT("%s (%s)"), *Province.Name, *Province.Id),
					Text, PanelX + 18.f, PanelY, Font, 1.0f);
				PanelY += LineHeight * 1.25f;
				const FString ControllerIso = Tick->GetProvinceControllerIso(Province.Id);
				DrawText(FString::Printf(TEXT("Region: %s    Pais base: %s    Control: %s    Terreno: %s"),
					*Province.Region, *Province.CountryIso, *ControllerIso, *TerrainToText(Province.Terrain)),
					FLinearColor(0.75f, 0.9f, 1.0f), PanelX + 18.f, PanelY, SmallFont);
				PanelY += LineHeight;
				DrawText(FString::Printf(TEXT("Capital: %s    Poblacion: %lld    Orden publico: %d    Infra: %d"),
					*Province.Capital, ProvinceState.Population, ProvinceState.PublicOrder, Province.Infrastructure),
					FLinearColor(0.85f, 0.85f, 0.85f), PanelX + 18.f, PanelY, SmallFont);
				PanelY += LineHeight;
				DrawText(FString::Printf(TEXT("Recursos  Oil:%d  Gas:%d  Food:%d  Minerals:%d  Industry:%d"),
					Province.BaseOil, Province.BaseGas, Province.BaseFood, Province.BaseMinerals, Province.BaseIndustry),
					FLinearColor(0.85f, 0.85f, 0.85f), PanelX + 18.f, PanelY, SmallFont);
				PanelY += LineHeight;
				DrawText(FString::Printf(TEXT("Ingreso:%lld  Upkeep:%lld  Balance:%+lld"),
					Tick->GetProvinceMonthlyIncome(Province.Id),
					Tick->GetProvinceMonthlyUpkeep(Province.Id),
					Tick->GetProvinceMonthlyBalance(Province.Id)),
					FLinearColor(0.55f, 0.95f, 0.55f), PanelX + 18.f, PanelY, SmallFont);

				const bool bOwnProvince = ControllerIso == CampaignGI->GetSelectedNationIso();
				DrawText(bOwnProvince
						? TEXT("[B] Construir edificio recomendado")
						: TEXT("Provincia no controlada por tu nacion"),
					bOwnProvince ? FLinearColor(1.0f, 0.85f, 0.2f) : FLinearColor(0.7f, 0.45f, 0.45f),
					PanelX + 18.f, PanelY + LineHeight, SmallFont);
			}
		}
		else if (bDiplomacy && PC->HasSelectedCountry())
		{
			const float PanelX = W - 442.f;
			float PanelY = bDiplomacy ? 118.f : 154.f;
			DrawRect(Ink, PanelX, PanelY, 404.f, 126.f);
			PanelY += 18.f;
			DrawText(TEXT("PAIS SELECCIONADO"), Gold, PanelX + 18.f, PanelY, SmallFont);
			PanelY += LineHeight * 1.2f;
			DrawText(FString::Printf(TEXT("%s (%s)"), *PC->GetSelectedCountryName(), *PC->GetSelectedCountryIso()),
				Text, PanelX + 18.f, PanelY, Font, 1.0f);
			PanelY += LineHeight;
			DrawText(PC->GetSelectedCountryContinent(), FLinearColor(0.75f, 0.9f, 1.0f), PanelX + 18.f, PanelY, SmallFont);

			FWLNationData Nation;
			if (Registry->GetNation(PC->GetSelectedCountryIso(), Nation))
			{
				const TArray<FWLProvinceData> NationProvinces = Registry->GetProvincesByNation(Nation.Iso);
				PanelY += LineHeight * 1.2f;
				DrawText(FString::Printf(TEXT("Provincias: %d"), NationProvinces.Num()),
					FLinearColor(0.85f, 0.85f, 0.85f), PanelX + 18.f, PanelY, SmallFont);
			}
		}
	}

	// Feed de noticias del mundo (guerras, golpes, espias, eventos IA...): cronica viva del mes,
	// abajo-izquierda por encima del footer. Nuevas primero; se recorta a las mas recientes.
	if (const UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (const UWLPoliticalSubsystem* Politics = GI->GetSubsystem<UWLPoliticalSubsystem>())
		{
			const TArray<FString>& News = Politics->GetNewsLog();
			if (News.Num() > 0 && !bDiplomacy)
			{
				const int32 MaxLines = 5;
				const int32 Shown = FMath::Min(MaxLines, News.Num());
				const float FeedW = 470.f;
				const float FeedLine = 16.f;
				const float FeedH = 26.f + Shown * FeedLine;
				const float FeedX = 36.f;
				const float FeedY = H - 34.f - FeedH - 10.f;
				DrawRect(Ink, FeedX, FeedY, FeedW, FeedH);
				DrawRect(Gold, FeedX, FeedY, 3.f, FeedH);
				DrawText(TEXT("NOTICIAS DEL MUNDO"), Gold, FeedX + 12.f, FeedY + 6.f, SmallFont, 0.9f);
				for (int32 i = 0; i < Shown; ++i)
				{
					FString Line = News[i];
					if (Line.Len() > 74)
					{
						Line = Line.Left(71) + TEXT("...");
					}
					DrawText(Line, i == 0 ? Text : Muted, FeedX + 12.f, FeedY + 24.f + i * FeedLine, SmallFont, 0.78f);
				}
			}
		}
	}

	DrawRect(InkHard, 0.f, H - 34.f, W, 34.f);
	DrawText(TEXT("[D] Vista   Rueda/+/- Zoom   Flechas Pan   [R] Reset   [F] Teatro   [G] America   [C] Gobierno   [E] Eventos   [M] Avanzar dia   [F5] Guardar   [B] Construir"),
		Muted, 36.f, H - 24.f, SmallFont, 0.88f);

	// F5.3/F5.4: fin de partida REAL — banner central por encima de todo; el avance de tiempo ya esta
	// bloqueado en el PlayerController. El detalle completo vive en GOBIERNO > RESUMEN.
	if (const UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (const UWLPoliticalSubsystem* Politics = GI->GetSubsystem<UWLPoliticalSubsystem>())
		{
			const FWLCampaignOutcomeState Outcome = Politics->GetCampaignOutcome();
			if (Outcome.bGameOver)
			{
				const bool bPlayerWon = Outcome.WinningNationIso.Equals(
					CampaignGI->GetSelectedNationIso(), ESearchCase::IgnoreCase);
				const float BannerH = 118.f;
				const float BannerY = H * 0.5f - BannerH * 0.5f;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), 0.f, 0.f, W, H);
				DrawRect(bPlayerWon ? FLinearColor(0.16f, 0.13f, 0.04f, 0.97f) : FLinearColor(0.14f, 0.05f, 0.04f, 0.97f),
					0.f, BannerY, W, BannerH);
				DrawRect(Gold, 0.f, BannerY, W, 3.f);
				DrawRect(Gold, 0.f, BannerY + BannerH - 3.f, W, 3.f);
				DrawText(bPlayerWon ? TEXT("VICTORIA") : TEXT("FIN DE LA PARTIDA"),
					bPlayerWon ? Gold : FLinearColor(0.95f, 0.55f, 0.45f, 1.f),
					W * 0.5f - 90.f, BannerY + 22.f, Font, 1.6f);
				DrawText(FString::Printf(TEXT("%s — %s"), *Outcome.OutcomeType, *Outcome.Reason),
					Text, W * 0.5f - 320.f, BannerY + 62.f, SmallFont, 1.05f);
				DrawText(TEXT("El tiempo ya no avanza. Abre GOBIERNO [C] para el resumen final."),
					Muted, W * 0.5f - 320.f, BannerY + 86.f, SmallFont, 0.95f);
			}
		}
	}

	// La ventana de Gobierno / Presidencia ya no se dibuja en Canvas: es un widget UMG modal
	// (UWLGovernmentWidget) que el PlayerController crea/destruye. Aqui solo queda el boton GOBIERNO.
}
