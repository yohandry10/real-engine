void DrawCampaignSelectionPanel(
		AWLCampaignHUD* HUD,
		UCanvas* Canvas,
		UFont* Font,
		UFont* SmallFont,
		const AWLCampaignPlayerController* PC,
		const UWLDataRegistry* Registry,
		const UWLStrategicTickSubsystem* Tick)
	{
		FWLCampaignSelectionPanelEntry Entry;
		bool bCityMode = false;
		if (!BuildSelectionPanelEntry(PC, Registry, Tick, Entry, bCityMode))
		{
			return;
		}

		const float W = Canvas->ClipX;
		const float H = Canvas->ClipY;
		const float PanelW = 430.f;
		const float PanelX = W - 468.f;
		const float PanelY = 154.f;
		const float PanelH = FMath::Clamp(H - PanelY - 54.f, 360.f, 690.f);
		const FLinearColor Panel(0.010f, 0.018f, 0.022f, 0.94f);
		const FLinearColor PanelSoft(0.020f, 0.036f, 0.042f, 0.90f);
		const FLinearColor Gold(0.88f, 0.70f, 0.26f, 1.f);
		const FLinearColor Text(0.88f, 0.93f, 0.94f, 1.f);
		const FLinearColor Muted(0.55f, 0.66f, 0.68f, 1.f);
		const FLinearColor Disabled(0.18f, 0.22f, 0.23f, 0.88f);

		HUD->DrawRect(Panel, PanelX, PanelY, PanelW, PanelH);
		HUD->DrawRect(FLinearColor(0.50f, 0.40f, 0.18f, 0.90f), PanelX, PanelY, PanelW, 3.f);
		HUD->DrawRect(PanelSoft, PanelX + 14.f, PanelY + 14.f, PanelW - 28.f, 62.f);
		HUD->DrawRect(FLinearColor(0.06f, 0.09f, 0.095f, 0.95f), PanelX + PanelW - 38.f, PanelY + 14.f, 24.f, 24.f);
		HUD->DrawText(TEXT("X"), Gold, PanelX + PanelW - 32.f, PanelY + 17.f, Font, 0.74f);

		float Y = PanelY + 22.f;
		const FString ModeLabel = bCityMode ? TEXT("CIUDAD / ASENTAMIENTO") : TEXT("PROVINCIA / DEPARTAMENTO");
		HUD->DrawText(ModeLabel, Gold, PanelX + 24.f, Y, SmallFont, 0.80f);
		Y += 19.f;
		HUD->DrawText(ShortenForPanel(Entry.Name, 30), Text, PanelX + 24.f, Y, Font, 1.12f);
		Y += 24.f;
		const FString Subtitle = bCityMode
			? FString::Printf(TEXT("%s  |  %s"), *Entry.Country, *Entry.TypeLabel)
			: FString::Printf(TEXT("%s  |  %s"), *Entry.Country, *Entry.TypeLabel);
		HUD->DrawText(ShortenForPanel(Subtitle, 48), Muted, PanelX + 24.f, Y, SmallFont, 0.78f);

		Y = PanelY + 92.f;
		const float MetricW = (PanelW - 58.f) * 0.5f;
		DrawMetric(HUD, SmallFont, bCityMode ? TEXT("Territorio") : TEXT("Capital / principal"),
			bCityMode ? Entry.TerritoryName : Entry.CapitalOrMainCity, PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, bCityMode ? TEXT("Puerto") : TEXT("Control"),
			bCityMode ? Entry.PortStatus : Entry.Controller, PanelX + 30.f + MetricW, Y, MetricW);
		Y += 50.f;
		DrawMetric(HUD, SmallFont, TEXT("Poblacion"), Entry.Population, PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Infraestructura"), Entry.Infrastructure, PanelX + 30.f + MetricW, Y, MetricW);
		Y += 58.f;

		DrawSectionTitle(HUD, SmallFont, TEXT("IMPORTANCIA ESTRATEGICA"), PanelX + 18.f, Y, Gold);
		HUD->DrawText(ShortenForPanel(Entry.StrategicImportance, 58), Text, PanelX + 18.f, Y, SmallFont, 0.78f);
		Y += 25.f;

		DrawSectionTitle(HUD, SmallFont, bCityMode ? TEXT("RECURSOS CONECTADOS") : TEXT("RECURSOS PRINCIPALES"), PanelX + 18.f, Y, Gold);
		DrawTagRow(HUD, SmallFont, Entry.Resources, PanelX + 18.f, Y, PanelW - 36.f, FLinearColor(0.12f, 0.17f, 0.16f, 0.92f), Text);

		const FWLCampaignBuildingSlotCatalog& SlotCatalog = GetCampaignHudBuildingSlotCatalog();
		const EWLCampaignBuildingPanelContext SlotContext = FWLCampaignBuildingSlotRules::ContextFromCityMode(bCityMode);
		const TArray<FString>& Slots = bCityMode ? Entry.UrbanSlots : Entry.BuildingSlots;
		DrawCampaignBuildingSlotCards(HUD, SmallFont, PC, SlotCatalog, SlotContext, Slots, PanelX, PanelY, PanelW, Gold, Text, Muted);

		if (!bCityMode)
		{
			Y = FMath::Max(Y, GetCampaignHudSlotStartY(PanelY) - 63.f);
			DrawSectionTitle(HUD, SmallFont, TEXT("PUERTOS / CIUDADES"), PanelX + 18.f, Y, Gold);
			HUD->DrawText(ShortenForPanel(FString::Printf(TEXT("Puertos: %s"), *JoinDisplayList(Entry.Ports, TEXT("No aplica"))), 58),
				Muted, PanelX + 18.f, Y, SmallFont, 0.75f);
			Y += 18.f;
			HUD->DrawText(ShortenForPanel(FString::Printf(TEXT("Ciudades: %s"), *JoinDisplayList(Entry.Cities)), 58),
				Muted, PanelX + 18.f, Y, SmallFont, 0.75f);
			Y += 26.f;
		}

		DrawCampaignBuildingDetail(HUD, SmallFont, PC, SlotCatalog, SlotContext, Slots, PanelX, PanelY, PanelW, PanelH, Gold, Text, Muted, Disabled);

		HUD->DrawText(ShortenForPanel(Entry.DetailLevel, 46), Muted, PanelX + 18.f, PanelY + PanelH - 24.f, SmallFont, 0.70f);
	}

	// Color de banda por categoria de unidad (deducida de la etiqueta), estilo Total War.
	FLinearColor CampaignTroopCardColor(const FString& Label)
	{
		const FString L = Label.ToLower();
		if (L.Contains(TEXT("artill")))                                                       return FLinearColor(0.74f, 0.46f, 0.20f, 1.f); // naranja
		if (L.Contains(TEXT("tanque")) || L.Contains(TEXT("veh")) || L.Contains(TEXT("blind"))) return FLinearColor(0.40f, 0.46f, 0.52f, 1.f); // acero
		if (L.Contains(TEXT("transp")))                                                       return FLinearColor(0.55f, 0.50f, 0.30f, 1.f); // tan
		if (L.Contains(TEXT("heli")) || L.Contains(TEXT("aero")) || L.Contains(TEXT("caza")) || L.Contains(TEXT("avion"))) return FLinearColor(0.28f, 0.46f, 0.62f, 1.f); // azul aire
		if (L.Contains(TEXT("buque")) || L.Contains(TEXT("naval")) || L.Contains(TEXT("marin"))) return FLinearColor(0.24f, 0.52f, 0.54f, 1.f); // teal naval
		return FLinearColor(0.42f, 0.52f, 0.26f, 1.f); // olivo (infanteria / por defecto)
	}

	// Categoria de icono deducida de la etiqueta de la unidad.
	enum class EWLTroopIcon { Infantry, Tank, Artillery, Transport, Air, Naval };
	EWLTroopIcon CampaignTroopIconFor(const FString& Label)
	{
		const FString L = Label.ToLower();
		if (L.Contains(TEXT("artill")))                                                        return EWLTroopIcon::Artillery;
		if (L.Contains(TEXT("tanque")) || L.Contains(TEXT("veh")) || L.Contains(TEXT("blind"))) return EWLTroopIcon::Tank;
		if (L.Contains(TEXT("transp")))                                                        return EWLTroopIcon::Transport;
		if (L.Contains(TEXT("heli")) || L.Contains(TEXT("aero")) || L.Contains(TEXT("caza")) || L.Contains(TEXT("avion"))) return EWLTroopIcon::Air;
		if (L.Contains(TEXT("buque")) || L.Contains(TEXT("naval")) || L.Contains(TEXT("marin"))) return EWLTroopIcon::Naval;
		return EWLTroopIcon::Infantry;
	}

	// Dibuja una SILUETA de unidad (estilo icono Total War) con rectangulos dentro de (ix,iy,iw,ih), para
	// identificar de un vistazo el tipo: infanteria, tanque, artilleria, transporte, avion, buque.
	void DrawTroopIcon(AWLCampaignHUD* HUD, EWLTroopIcon Icon, float ix, float iy, float iw, float ih)
	{
		const FLinearColor Body(0.84f, 0.88f, 0.92f, 1.f);
		const FLinearColor Dark(0.32f, 0.36f, 0.40f, 1.f);
		auto R = [&](float fx, float fy, float fw, float fh, const FLinearColor& C)
		{
			HUD->DrawRect(C, ix + iw * fx, iy + ih * fy, iw * fw, ih * fh);
		};
		switch (Icon)
		{
		case EWLTroopIcon::Tank:
			R(0.05f, 0.66f, 0.90f, 0.20f, Dark);   // orugas
			R(0.12f, 0.44f, 0.76f, 0.26f, Body);   // casco
			R(0.34f, 0.22f, 0.34f, 0.24f, Body);   // torreta
			R(0.62f, 0.31f, 0.40f, 0.08f, Body);   // canon
			break;
		case EWLTroopIcon::Artillery:
			R(0.16f, 0.62f, 0.20f, 0.30f, Dark);   // rueda izq
			R(0.46f, 0.62f, 0.20f, 0.30f, Dark);   // rueda der
			R(0.16f, 0.48f, 0.54f, 0.16f, Body);   // cureña
			R(0.40f, 0.24f, 0.58f, 0.10f, Body);   // canon largo
			break;
		case EWLTroopIcon::Transport:
			R(0.14f, 0.66f, 0.20f, 0.26f, Dark);   // rueda izq
			R(0.56f, 0.66f, 0.20f, 0.26f, Dark);   // rueda der
			R(0.08f, 0.32f, 0.52f, 0.36f, Body);   // caja
			R(0.60f, 0.42f, 0.30f, 0.26f, Body);   // cabina
			break;
		case EWLTroopIcon::Air:
			R(0.44f, 0.06f, 0.12f, 0.86f, Body);   // fuselaje
			R(0.08f, 0.40f, 0.84f, 0.13f, Body);   // alas
			R(0.30f, 0.80f, 0.40f, 0.10f, Body);   // cola
			break;
		case EWLTroopIcon::Naval:
			R(0.06f, 0.56f, 0.88f, 0.22f, Body);   // casco
			R(0.30f, 0.34f, 0.40f, 0.24f, Body);   // superestructura
			R(0.47f, 0.14f, 0.09f, 0.22f, Dark);   // chimenea
			break;
		case EWLTroopIcon::Infantry:
		default:
			R(0.42f, 0.05f, 0.16f, 0.18f, Body);   // cabeza
			R(0.34f, 0.24f, 0.32f, 0.42f, Body);   // torso
			R(0.36f, 0.66f, 0.11f, 0.30f, Body);   // pierna izq
			R(0.53f, 0.66f, 0.11f, 0.30f, Body);   // pierna der
			R(0.64f, 0.22f, 0.06f, 0.48f, Dark);   // fusil
			break;
		}
	}

	// Barra de TROPAS estilo Total War: fila ANCHA de cartas grandes a lo largo de la parte inferior de la
	// pantalla (no amontonadas en el panel lateral). Muestra TODA la composicion de la fuerza seleccionada.
	void DrawCampaignForceTroopBar(
		AWLCampaignHUD* HUD,
		UCanvas* Canvas,
		UFont* Font,
		UFont* SmallFont,
		const AWLCampaignPlayerController* PC)
	{
		if (!HUD || !Canvas || !PC || PC->GetCampaignSelectionKind() != EWLCampaignSelectionKind::Force)
		{
			return;
		}
		const TArray<FWLCampaignForceCompositionEntry> Comp = PC->GetSelectedForceTotalComposition();
		if (Comp.Num() == 0)
		{
			return;   // fuerte vacio o fuerza sin tropas -> sin barra
		}

		const float W = Canvas->ClipX;
		const float H = Canvas->ClipY;
		const FLinearColor Gold(0.88f, 0.70f, 0.26f, 1.f);
		const FLinearColor Text(0.90f, 0.94f, 0.95f, 1.f);
		const FLinearColor Muted(0.58f, 0.68f, 0.70f, 1.f);
		const FLinearColor BarBg(0.016f, 0.028f, 0.032f, 0.96f);
		const FLinearColor CardBg(0.055f, 0.090f, 0.100f, 0.98f);

		const float CardW = 132.f;
		const float CardH = 110.f;
		const float CardGap = 10.f;
		const float PadX = 18.f;
		const float NameW = 236.f;   // bloque izquierdo: nombre del ejercito + efectivos

		// Cuantas cartas caben a lo ancho (todas las demas se resumen en "+N").
		const float AvailW = W - 40.f - (NameW + 18.f + PadX * 2.f);
		const int32 MaxVisible = FMath::Max(1, FMath::FloorToInt((AvailW + CardGap) / (CardW + CardGap)));
		const int32 N = Comp.Num();
		const int32 Shown = FMath::Min(N, MaxVisible);

		const float CardsW = static_cast<float>(Shown) * CardW + static_cast<float>(Shown - 1) * CardGap;
		const float ContentW = NameW + 18.f + CardsW + PadX * 2.f;
		const float BarH = CardH + 32.f;
		const float BarX = FMath::Max(16.f, (W - ContentW) * 0.5f);
		const float BarY = H - BarH - 30.f;

		HUD->DrawRect(BarBg, BarX, BarY, ContentW, BarH);
		HUD->DrawRect(FLinearColor(0.50f, 0.40f, 0.18f, 0.95f), BarX, BarY, ContentW, 3.f);

		int32 Total = 0;
		for (const FWLCampaignForceCompositionEntry& E : Comp)
		{
			Total += E.Count;
		}
		HUD->DrawText(TEXT("EJERCITO"), Gold, BarX + PadX, BarY + 12.f, SmallFont, 0.74f);
		HUD->DrawText(ShortenForPanel(PC->GetSelectedForceName(), 24), Text, BarX + PadX, BarY + 34.f, SmallFont, 0.94f);
		HUD->DrawText(FString::Printf(TEXT("Efectivos: %d  -  %d tipos"), Total, N), Muted, BarX + PadX, BarY + 60.f, SmallFont, 0.70f);

		const float CardsY = BarY + (BarH - CardH) * 0.5f;
		float cx = BarX + PadX + NameW + 18.f;
		const FLinearColor DarkText(0.06f, 0.07f, 0.05f, 1.f);
		const FLinearColor BadgeBg(0.03f, 0.045f, 0.05f, 0.95f);
		for (int32 i = 0; i < Shown; ++i)
		{
			const bool bOverflow = (i == Shown - 1) && (N > Shown);
			const FLinearColor Band = bOverflow ? FLinearColor(0.30f, 0.34f, 0.36f, 1.f) : CampaignTroopCardColor(Comp[i].Label);
			HUD->DrawRect(CardBg, cx, CardsY, CardW, CardH);
			HUD->DrawRect(Band, cx, CardsY, CardW, 20.f);   // banda de categoria (arriba) con la etiqueta
			if (bOverflow)
			{
				HUD->DrawText(TEXT("MAS"), DarkText, cx + 8.f, CardsY + 3.f, SmallFont, 0.66f);
				HUD->DrawText(FString::Printf(TEXT("+%d"), N - Shown + 1), Gold, cx + 12.f, CardsY + 46.f, Font, 1.6f);
				HUD->DrawText(TEXT("tipos mas"), Muted, cx + 12.f, CardsY + CardH - 22.f, SmallFont, 0.60f);
			}
			else
			{
				HUD->DrawText(ShortenForPanel(Comp[i].Label, 15), DarkText, cx + 7.f, CardsY + 3.f, SmallFont, 0.64f);
				// Silueta de la unidad: identifica el tipo de un vistazo (tanque, infanteria, avion, etc.).
				DrawTroopIcon(HUD, CampaignTroopIconFor(Comp[i].Label), cx + 16.f, CardsY + 24.f, CardW - 32.f, 52.f);
				// Badge inferior con el numero de efectivos (grande).
				HUD->DrawRect(BadgeBg, cx + 6.f, CardsY + CardH - 28.f, CardW - 12.f, 24.f);
				HUD->DrawText(FString::Printf(TEXT("%d"), Comp[i].Count), Gold, cx + 12.f, CardsY + CardH - 26.f, Font, 0.92f);
				HUD->DrawText(TEXT("ef."), Muted, cx + CardW - 32.f, CardsY + CardH - 22.f, SmallFont, 0.62f);
			}
			cx += CardW + CardGap;
		}
	}

	void DrawCampaignForcePanel(
		AWLCampaignHUD* HUD,
		UCanvas* Canvas,
		UFont* Font,
		UFont* SmallFont,
		const AWLCampaignPlayerController* PC)
	{
		if (!HUD || !Canvas || !PC || PC->GetCampaignSelectionKind() != EWLCampaignSelectionKind::Force)
		{
			return;
		}

		const float W = Canvas->ClipX;
		const float H = Canvas->ClipY;
		const float PanelW = 430.f;
		const float PanelX = W - 468.f;
		const float PanelY = 154.f;
		const float PanelH = FMath::Clamp(H - PanelY - 54.f, 360.f, 690.f);
		const FLinearColor Panel(0.010f, 0.018f, 0.022f, 0.94f);
		const FLinearColor PanelSoft(0.020f, 0.036f, 0.042f, 0.90f);
		const FLinearColor Gold(0.88f, 0.70f, 0.26f, 1.f);
		const FLinearColor Text(0.88f, 0.93f, 0.94f, 1.f);
		const FLinearColor Muted(0.55f, 0.66f, 0.68f, 1.f);
		const FLinearColor Disabled(0.18f, 0.22f, 0.23f, 0.88f);

		HUD->DrawRect(Panel, PanelX, PanelY, PanelW, PanelH);
		HUD->DrawRect(FLinearColor(0.50f, 0.40f, 0.18f, 0.90f), PanelX, PanelY, PanelW, 3.f);
		HUD->DrawRect(PanelSoft, PanelX + 14.f, PanelY + 14.f, PanelW - 28.f, 68.f);
		HUD->DrawRect(FLinearColor(0.06f, 0.09f, 0.095f, 0.95f), PanelX + PanelW - 38.f, PanelY + 14.f, 24.f, 24.f);
		HUD->DrawText(TEXT("X"), Gold, PanelX + PanelW - 32.f, PanelY + 17.f, Font, 0.74f);

		float Y = PanelY + 22.f;
		HUD->DrawText(TEXT("FUERZA MILITAR"), Gold, PanelX + 24.f, Y, SmallFont, 0.82f);
		Y += 19.f;
		HUD->DrawText(ShortenForPanel(PC->GetSelectedForceName(), 30), Text, PanelX + 24.f, Y, Font, 1.08f);
		Y += 24.f;
		const FString Subtitle = FString::Printf(TEXT("%s  |  %s"),
			*PC->GetSelectedForceCountryName(),
			*PC->GetSelectedForceType());
		HUD->DrawText(ShortenForPanel(Subtitle, 48), Muted, PanelX + 24.f, Y, SmallFont, 0.78f);

		Y = PanelY + 98.f;
		const float MetricW = (PanelW - 58.f) * 0.5f;
		DrawMetric(HUD, SmallFont, TEXT("Ubicacion"), PC->GetSelectedForceLocation(), PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Provincia"), PC->GetSelectedForceProvinceName(), PanelX + 30.f + MetricW, Y, MetricW);
		Y += 50.f;
		DrawMetric(HUD, SmallFont, TEXT("Ciudad cercana"), PC->GetSelectedForceNearbyCity(), PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Efectivos est."), FString::Printf(TEXT("%d"), PC->GetSelectedForceEstimatedStrength()), PanelX + 30.f + MetricW, Y, MetricW);
		Y += 50.f;
		DrawMetric(HUD, SmallFont, TEXT("Movilidad"), PC->GetSelectedForceMobility(), PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Abastecimiento"), PC->GetSelectedForceSupply(), PanelX + 30.f + MetricW, Y, MetricW);
		Y += 50.f;
		DrawMetric(HUD, SmallFont, TEXT("Moral"), PC->GetSelectedForceMorale(), PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Estado"), PC->GetSelectedForceOperationalState(), PanelX + 30.f + MetricW, Y, MetricW);
		Y += 58.f;

		DrawSectionTitle(HUD, SmallFont, TEXT("POSTURA / ROL"), PanelX + 18.f, Y, Gold);
		HUD->DrawText(ShortenForPanel(PC->GetSelectedForcePosture(), 54), Text, PanelX + 18.f, Y, SmallFont, 0.78f);
		Y += 18.f;
		HUD->DrawText(ShortenForPanel(PC->GetSelectedForceStrategicRole(), 58), Muted, PanelX + 18.f, Y, SmallFont, 0.72f);
		Y += 18.f;
		HUD->DrawText(ShortenForPanel(FString::Printf(TEXT("Movimiento: %s"), *PC->GetForceMovementStatus()), 58),
			Muted, PanelX + 18.f, Y, SmallFont, 0.70f);
		if (PC->IsForceMovementModeActive())
		{
			Y += 17.f;
			const FString Destination = PC->HasForceMovementDestination()
				? PC->GetForceMovementDestinationName()
				: TEXT("elige un destino resaltado");
			HUD->DrawText(ShortenForPanel(FString::Printf(TEXT("Destino: %s"), *Destination), 58),
				Text, PanelX + 18.f, Y, SmallFont, 0.70f);
		}

		const float ButtonW = (PanelW - 48.f) * 0.5f;
		const float ButtonH = 28.f;
		const float Gap = 8.f;
		const float ButtonX0 = PanelX + 18.f;
		const float ButtonX1 = ButtonX0 + ButtonW + Gap;
		const float ActionY = PanelY + 412.f;
		HUD->DrawText(TEXT("ORDEN DE MOVIMIENTO"), Gold, PanelX + 18.f, ActionY - 24.f, SmallFont, 0.78f);
		HUD->DrawRect(PC->CanSelectedForceMove() ? FLinearColor(0.50f, 0.40f, 0.18f, 0.96f) : Disabled,
			ButtonX0, ActionY, ButtonW, ButtonH);
		HUD->DrawText(PC->IsForceMovementModeActive() ? TEXT("Cambiar destino") : TEXT("Mover"),
			PC->CanSelectedForceMove() ? Text : Muted, ButtonX0 + 9.f, ActionY + 7.f, SmallFont, 0.66f);

		float DisabledStartY = ActionY + 40.f;
		if (PC->IsForceMovementModeActive())
		{
			const FString RouteText = PC->HasForceMovementDestination()
				? FString::Printf(TEXT("Ruta: %s (%d turno%s)"),
					*PC->GetForceMovementRouteSummary(),
					PC->GetForceMovementEstimatedTurns(),
					PC->GetForceMovementEstimatedTurns() == 1 ? TEXT("") : TEXT("s"))
				: TEXT("Hover/click en destino valido para previsualizar ruta.");
			HUD->DrawText(ShortenForPanel(RouteText, 60), Muted, PanelX + 18.f, ActionY + 38.f, SmallFont, 0.64f);

			const float ConfirmY = ActionY + 86.f;
			HUD->DrawRect(PC->HasForceMovementDestination() ? FLinearColor(0.38f, 0.52f, 0.32f, 0.96f) : Disabled,
				ButtonX0, ConfirmY, ButtonW, ButtonH);
			HUD->DrawText(TEXT("Confirmar movimiento"),
				PC->HasForceMovementDestination() ? Text : Muted, ButtonX0 + 9.f, ConfirmY + 7.f, SmallFont, 0.64f);
			HUD->DrawRect(FLinearColor(0.36f, 0.26f, 0.22f, 0.96f), ButtonX1, ConfirmY, ButtonW, ButtonH);
			HUD->DrawText(TEXT("Cancelar"), Text, ButtonX1 + 9.f, ConfirmY + 7.f, SmallFont, 0.64f);
			DisabledStartY = ConfirmY + 40.f;
		}

		// RECLUTAMIENTO + TROPAS (solo cuando NO estas dando una orden de movimiento). El boton "Reclutar"
		// va al MISMO Y/anchura que comprueba el handler de clic en WLCampaignPlayerControllerViews.cpp
		// (DisabledStartY = ActionY+40) -> mantener en sync.
		if (!PC->IsForceMovementModeActive())
		{
			// RECLUTAR (solo FUERTES): una carta-boton por unidad. El layout (RBtnW/RBtnH/RGridY) DEBE
			// coincidir con el hit-test de Views.cpp -> mantener en sync. Un ejercito de campo no recluta
			// (Options vacio). Las TROPAS ya NO van aqui: se muestran GRANDES en la BARRA INFERIOR (Total War).
			const TArray<FWLCampaignRecruitButton> Options = PC->GetSelectedForceRecruitOptions();
			if (Options.Num() > 0)
			{
				// Carta GRANDE por unidad (icono + nombre + coste + dias), estilo Total War. El layout
				// (RBtnW/RBtnH/RBtnGap/RGridY) DEBE coincidir con el hit-test de Views.cpp -> mantener en sync.
				const float RBtnW = (PanelW - 48.f) * 0.5f;
				const float RBtnH = 60.f;
				const float RBtnGap = 8.f;
				HUD->DrawText(TEXT("RECLUTAR UNIDADES"), Gold, PanelX + 18.f, DisabledStartY, SmallFont, 0.92f);
				const float RGridY = DisabledStartY + 22.f;
				const int32 MaxOpt = FMath::Min(Options.Num(), 6);
				const FLinearColor RCardBg(0.07f, 0.12f, 0.10f, 0.98f);
				for (int32 i = 0; i < MaxOpt; ++i)
				{
					const float bx = PanelX + 18.f + static_cast<float>(i % 2) * (RBtnW + RBtnGap);
					const float by = RGridY + static_cast<float>(i / 2) * (RBtnH + RBtnGap);
					HUD->DrawRect(RCardBg, bx, by, RBtnW, RBtnH);
					HUD->DrawRect(CampaignTroopCardColor(Options[i].Label), bx, by, 6.f, RBtnH);   // franja de categoria
					DrawTroopIcon(HUD, CampaignTroopIconFor(Options[i].Label), bx + 14.f, by + 9.f, 42.f, 42.f);
					HUD->DrawText(ShortenForPanel(Options[i].Label, 15), Text, bx + 64.f, by + 7.f, SmallFont, 0.98f);
					HUD->DrawText(FString::Printf(TEXT("$%lld"), static_cast<long long>(Options[i].Cost)), Gold, bx + 64.f, by + 32.f, SmallFont, 0.9f);
					HUD->DrawText(FString::Printf(TEXT("%d dia%s"), Options[i].Turns, Options[i].Turns == 1 ? TEXT("") : TEXT("s")),
						Muted, bx + RBtnW - 66.f, by + 32.f, SmallFont, 0.82f);
				}
				const int32 RRows = (MaxOpt + 1) / 2;
				const float AfterGridY = RGridY + static_cast<float>(RRows) * (RBtnH + RBtnGap) + 4.f;
				HUD->DrawText(ShortenForPanel(PC->GetSelectedForceRecruitStatus(), 56), Muted, PanelX + 18.f, AfterGridY, SmallFont, 0.74f);
			}
		}

		// Las TROPAS de la fuerza seleccionada se muestran GRANDES en la barra inferior (estilo Total War).
		DrawCampaignForceTroopBar(HUD, Canvas, Font, SmallFont, PC);

		HUD->DrawText(ShortenForPanel(PC->GetSelectedForceDetailLevel(), 46), Muted, PanelX + 18.f, PanelY + PanelH - 24.f, SmallFont, 0.70f);
	}
