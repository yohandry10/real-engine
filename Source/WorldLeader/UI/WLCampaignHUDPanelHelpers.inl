FString TerrainToText(EWLTerrainType Terrain)
	{
		switch (Terrain)
		{
		case EWLTerrainType::Mountain: return TEXT("Montana");
		case EWLTerrainType::Desert: return TEXT("Desierto");
		case EWLTerrainType::Jungle: return TEXT("Selva");
		case EWLTerrainType::Coastal: return TEXT("Costera");
		case EWLTerrainType::Urban: return TEXT("Urbana");
		case EWLTerrainType::Arctic: return TEXT("Artica");
		case EWLTerrainType::Maritime: return TEXT("Maritima");
		default: return TEXT("Llano");
		}
	}

	const FWLCampaignSelectionPanelData& GetSelectionPanelData()
	{
		static FWLCampaignSelectionPanelData Data;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			FWLCampaignSelectionPanelDataLoader::Load(Data);
			bLoaded = true;
		}
		return Data;
	}

	const FWLCampaignBuildingSlotCatalog& GetCampaignHudBuildingSlotCatalog()
	{
		static FWLCampaignBuildingSlotCatalog Catalog;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			FWLCampaignBuildingSlotDataLoader::Load(Catalog);
			bLoaded = true;
		}
		return Catalog;
	}

	FString JoinDisplayList(const TArray<FString>& Values, const FString& EmptyText = TEXT("Sin datos placeholder"))
	{
		if (Values.IsEmpty())
		{
			return EmptyText;
		}
		return FString::Join(Values, TEXT("  |  "));
	}

	FString ShortenForPanel(const FString& Text, int32 MaxChars)
	{
		if (Text.Len() <= MaxChars)
		{
			return Text;
		}
		return Text.Left(FMath::Max(0, MaxChars - 3)) + TEXT("...");
	}

	FWLCampaignSelectionPanelEntry MakeProvinceFallbackEntry(
		const AWLCampaignPlayerController* PC,
		const UWLDataRegistry* Registry,
		const UWLStrategicTickSubsystem* Tick)
	{
		FWLCampaignSelectionPanelEntry Entry;
		if (!PC)
		{
			return Entry;
		}

		const FString Id = PC->GetCampaignSelectionId();
		FWLProvinceData Province;
		if (Registry && Registry->GetProvince(Id, Province))
		{
			Entry.Id = Province.Id;
			Entry.Name = Province.Name;
			Entry.CountryIso = Province.CountryIso;
			Entry.Country = Province.CountryIso;
			Entry.TypeLabel = Province.bIsCapital ? TEXT("capital province") : TEXT("province");
			Entry.CapitalOrMainCity = Province.Capital;
			Entry.Owner = Province.CountryIso;
			Entry.Controller = Tick ? Tick->GetProvinceControllerIso(Province.Id) : Province.CountryIso;
			Entry.Population = FString::Printf(TEXT("%lld placeholder"), Province.Population);
			Entry.PublicOrder = TEXT("70 / 100");
			Entry.Infrastructure = FString::Printf(TEXT("%d / 100"), Province.Infrastructure);
			Entry.StrategicImportance = FString::Printf(TEXT("Strategic value %d"), Province.StrategicValue);
			Entry.DetailLevel = (Province.CountryIso.Equals(TEXT("CO"), ESearchCase::IgnoreCase)
				|| Province.CountryIso.Equals(TEXT("VE"), ESearchCase::IgnoreCase))
					? TEXT("high-detail theater")
					: TEXT("low-detail");
			if (Province.BaseOil > 0) Entry.Resources.Add(TEXT("oil"));
			if (Province.BaseGas > 0) Entry.Resources.Add(TEXT("gas"));
			if (Province.BaseFood > 0) Entry.Resources.Add(TEXT("food"));
			if (Province.BaseMinerals > 0) Entry.Resources.Add(TEXT("minerals"));
			if (Province.BaseIndustry > 0) Entry.Resources.Add(TEXT("industry"));
			if (Province.bHasPort) Entry.Ports.Add(Province.Capital);
			Entry.Cities.Add(Province.Capital);
			Entry.BuildingSlots = { TEXT("Economic"), TEXT("Infrastructure"), TEXT("Security"), TEXT("Logistics") };
			Entry.DisabledActions = { TEXT("Administrar edificios"), TEXT("Reclutar"), TEXT("Mejorar infraestructura"), TEXT("Gestionar orden publico") };
			return Entry;
		}

		Entry.Id = Id;
		Entry.Name = PC->GetSelectedTerritoryName();
		Entry.CountryIso = PC->GetSelectedTerritoryCountryIso();
		Entry.Country = Entry.CountryIso;
		Entry.TypeLabel = PC->GetSelectedTerritoryType();
		Entry.CapitalOrMainCity = TEXT("Ciudad principal placeholder");
		Entry.Owner = Entry.CountryIso;
		Entry.Controller = Entry.CountryIso;
		Entry.Population = TEXT("Placeholder");
		Entry.PublicOrder = TEXT("70 / 100");
		Entry.Infrastructure = TEXT("50 / 100");
		Entry.StrategicImportance = TEXT("Lectura estrategica placeholder");
		Entry.DetailLevel = TEXT("theater placeholder");
		Entry.Resources = { TEXT("regional"), TEXT("logistics") };
		Entry.Cities = { Entry.CapitalOrMainCity };
		Entry.BuildingSlots = { TEXT("Infrastructure"), TEXT("Logistics"), TEXT("Security") };
		Entry.DisabledActions = { TEXT("Administrar edificios"), TEXT("Mejorar infraestructura"), TEXT("Gestionar orden publico") };
		return Entry;
	}

	FWLCampaignSelectionPanelEntry MakeCityFallbackEntry(const AWLCampaignPlayerController* PC)
	{
		FWLCampaignSelectionPanelEntry Entry;
		if (!PC)
		{
			return Entry;
		}
		Entry.Id = PC->GetSelectedCityId();
		Entry.Name = PC->GetSelectedCityName();
		Entry.CountryIso = PC->GetSelectedCityCountryIso();
		Entry.Country = Entry.CountryIso;
		Entry.TypeLabel = PC->GetSelectedCityType();
		Entry.TerritoryId = PC->GetSelectedCityTerritoryId();
		Entry.TerritoryName = PC->GetSelectedCityTerritoryName();
		Entry.Population = TEXT("Placeholder");
		Entry.Infrastructure = TEXT("55 / 100");
		Entry.StrategicImportance = TEXT("Ciudad estrategica placeholder");
		Entry.PortStatus = Entry.TypeLabel.Contains(TEXT("port")) ? TEXT("Port placeholder active") : TEXT("No port");
		Entry.Resources = { TEXT("urban"), TEXT("logistics") };
		Entry.UrbanSlots = { TEXT("Urban"), TEXT("Infrastructure"), TEXT("Security") };
		Entry.DisabledActions = { TEXT("Administrar edificios"), TEXT("Reclutar"), TEXT("Mejorar infraestructura") };
		return Entry;
	}

	bool BuildSelectionPanelEntry(
		const AWLCampaignPlayerController* PC,
		const UWLDataRegistry* Registry,
		const UWLStrategicTickSubsystem* Tick,
		FWLCampaignSelectionPanelEntry& OutEntry,
		bool& bOutCityMode)
	{
		if (!PC || !PC->HasCampaignSelectionPanel())
		{
			return false;
		}

		const FWLCampaignSelectionPanelData& Data = GetSelectionPanelData();
		const FString Id = PC->GetCampaignSelectionId();
		bOutCityMode = PC->GetCampaignSelectionKind() == EWLCampaignSelectionKind::City;
		if (bOutCityMode)
		{
			if (const FWLCampaignSelectionPanelEntry* Found = Data.Cities.Find(Id))
			{
				OutEntry = *Found;
			}
			else
			{
				OutEntry = MakeCityFallbackEntry(PC);
			}
		}
		else
		{
			if (const FWLCampaignSelectionPanelEntry* Found = Data.Provinces.Find(Id))
			{
				OutEntry = *Found;
			}
			else
			{
				OutEntry = MakeProvinceFallbackEntry(PC, Registry, Tick);
			}
		}
		return !OutEntry.Id.IsEmpty() && !OutEntry.Name.IsEmpty();
	}

	void DrawSectionTitle(AWLCampaignHUD* HUD, UFont* Font, const FString& Text, float X, float& Y, const FLinearColor& Color)
	{
		HUD->DrawText(Text, Color, X, Y, Font, 0.82f);
		Y += 18.f;
	}

	void DrawMetric(AWLCampaignHUD* HUD, UFont* Font, const FString& Label, const FString& Value, float X, float Y, float W)
	{
		const FLinearColor Card(0.018f, 0.032f, 0.038f, 0.88f);
		const FLinearColor Muted(0.55f, 0.66f, 0.68f, 1.f);
		const FLinearColor Text(0.88f, 0.93f, 0.94f, 1.f);
		HUD->DrawRect(Card, X, Y, W, 42.f);
		HUD->DrawText(Label, Muted, X + 9.f, Y + 6.f, Font, 0.72f);
		HUD->DrawText(ShortenForPanel(Value, 24), Text, X + 9.f, Y + 22.f, Font, 0.78f);
	}

	void DrawTagRow(AWLCampaignHUD* HUD, UFont* Font, const TArray<FString>& Values, float X, float& Y, float MaxW, const FLinearColor& Fill, const FLinearColor& TextColor)
	{
		if (Values.IsEmpty())
		{
			HUD->DrawText(TEXT("Sin datos placeholder"), FLinearColor(0.55f, 0.66f, 0.68f, 1.f), X, Y, Font, 0.76f);
			Y += 21.f;
			return;
		}

		float CursorX = X;
		for (const FString& Value : Values)
		{
			const FString Label = ShortenForPanel(Value, 18);
			const float W = FMath::Clamp(48.f + Label.Len() * 6.0f, 76.f, 150.f);
			if (CursorX + W > X + MaxW)
			{
				CursorX = X;
				Y += 27.f;
			}
			HUD->DrawRect(Fill, CursorX, Y, W, 22.f);
			HUD->DrawText(Label, TextColor, CursorX + 8.f, Y + 4.f, Font, 0.70f);
			CursorX += W + 7.f;
		}
		Y += 29.f;
	}

	float GetCampaignHudSlotStartY(float PanelY)
	{
		return PanelY + 383.f;
	}

	float GetCampaignHudDetailsY(float PanelY, int32 SlotCount)
	{
		const int32 SlotRows = FMath::Max(1, FMath::DivideAndRoundUp(FMath::Clamp(SlotCount, 1, 6), 2));
		return GetCampaignHudSlotStartY(PanelY) + static_cast<float>(SlotRows) * 56.f + 16.f;
	}

	FString SlotStateText(EWLCampaignBuildingSlotState State)
	{
		switch (State)
		{
		case EWLCampaignBuildingSlotState::Occupied:
			return TEXT("OCUPADO");
		case EWLCampaignBuildingSlotState::Locked:
			return TEXT("BLOQUEADO");
		default:
			return TEXT("VACIO");
		}
	}

	FLinearColor SlotStateColor(EWLCampaignBuildingSlotState State, bool bSelected)
	{
		if (bSelected)
		{
			return FLinearColor(0.42f, 0.34f, 0.16f, 0.95f);
		}
		switch (State)
		{
		case EWLCampaignBuildingSlotState::Occupied:
			return FLinearColor(0.11f, 0.17f, 0.14f, 0.93f);
		case EWLCampaignBuildingSlotState::Locked:
			return FLinearColor(0.08f, 0.09f, 0.095f, 0.86f);
		default:
			return FLinearColor(0.025f, 0.043f, 0.050f, 0.92f);
		}
	}

	void DrawCampaignBuildingSlotCards(
		AWLCampaignHUD* HUD,
		UFont* SmallFont,
		const AWLCampaignPlayerController* PC,
		const FWLCampaignBuildingSlotCatalog& Catalog,
		EWLCampaignBuildingPanelContext Context,
		const TArray<FString>& Slots,
		float PanelX,
		float PanelY,
		float PanelW,
		const FLinearColor& Gold,
		const FLinearColor& Text,
		const FLinearColor& Muted)
	{
		const float SlotTitleY = GetCampaignHudSlotStartY(PanelY) - 23.f;
		if (Context == EWLCampaignBuildingPanelContext::City)
		{
			HUD->DrawText(TEXT("SLOTS DE EDIFICIOS"), Gold, PanelX + 18.f, SlotTitleY, SmallFont, 0.82f);
		}

		const float SlotStartY = GetCampaignHudSlotStartY(PanelY);
		const float SlotW = (PanelW - 48.f) * 0.5f;
		const float SlotH = 48.f;
		const float SlotGap = 8.f;
		const float SlotX0 = PanelX + 18.f;
		const bool bCityMode = Context == EWLCampaignBuildingPanelContext::City;

		for (int32 Index = 0; Index < Slots.Num() && Index < 6; ++Index)
		{
			const FString& SlotLabel = Slots[Index];
			const int32 Col = Index % 2;
			const int32 Row = Index / 2;
			const float SlotX = SlotX0 + static_cast<float>(Col) * (SlotW + SlotGap);
			const float SlotY = SlotStartY + static_cast<float>(Row) * (SlotH + SlotGap);
			const FString SlotKey = FWLCampaignBuildingSlotRules::MakeSlotKey(
				PC->GetCampaignSelectionId(),
				Context,
				SlotLabel,
				Index);
			const bool bSelected = PC->GetSelectedBuildingSlotKey() == SlotKey;
			const EWLCampaignBuildingSlotState State = PC->GetCampaignBuildingSlotState(SlotLabel, Index, bCityMode);
			const FString BuildingId = PC->GetCampaignBuildingIdForSlot(SlotLabel, Index, bCityMode);
			const FWLCampaignBuildingDefinition* Building = BuildingId.IsEmpty() ? nullptr : Catalog.FindBuilding(BuildingId);
			const FString DisplayLabel = FWLCampaignBuildingSlotRules::GetSlotDisplayLabel(Catalog, Context, SlotLabel);
			const FString Occupant = State == EWLCampaignBuildingSlotState::Locked
				? TEXT("Fase futura")
				: (Building ? Building->Name : TEXT("Disponible"));

			HUD->DrawRect(SlotStateColor(State, bSelected), SlotX, SlotY, SlotW, SlotH);
			if (bSelected)
			{
				HUD->DrawRect(FLinearColor(0.84f, 0.68f, 0.26f, 0.96f), SlotX, SlotY, SlotW, 2.f);
			}
			HUD->DrawText(ShortenForPanel(DisplayLabel, 20), Text, SlotX + 9.f, SlotY + 7.f, SmallFont, 0.74f);
			HUD->DrawText(SlotStateText(State), State == EWLCampaignBuildingSlotState::Locked ? Muted : Gold,
				SlotX + SlotW - 68.f, SlotY + 7.f, SmallFont, 0.62f);
			HUD->DrawText(ShortenForPanel(Occupant, 24), Muted, SlotX + 9.f, SlotY + 28.f, SmallFont, 0.68f);
		}
	}

	void DrawCampaignBuildingDetail(
		AWLCampaignHUD* HUD,
		UFont* SmallFont,
		const AWLCampaignPlayerController* PC,
		const FWLCampaignBuildingSlotCatalog& Catalog,
		EWLCampaignBuildingPanelContext Context,
		const TArray<FString>& Slots,
		float PanelX,
		float PanelY,
		float PanelW,
		float PanelH,
		const FLinearColor& Gold,
		const FLinearColor& Text,
		const FLinearColor& Muted,
		const FLinearColor& Disabled)
	{
		float Y = GetCampaignHudDetailsY(PanelY, Slots.Num());
		if (Y > PanelY + PanelH - 92.f)
		{
			return;
		}

		HUD->DrawRect(FLinearColor(0.014f, 0.026f, 0.032f, 0.92f), PanelX + 18.f, Y, PanelW - 36.f, FMath::Min(PanelY + PanelH - Y - 16.f, 228.f));
		HUD->DrawText(TEXT("CONSTRUCCION PLACEHOLDER"), Gold, PanelX + 30.f, Y + 10.f, SmallFont, 0.76f);
		Y += 30.f;

		if (!PC->HasSelectedBuildingSlot())
		{
			HUD->DrawText(TEXT("Selecciona un slot para ver edificios compatibles."), Muted, PanelX + 30.f, Y, SmallFont, 0.74f);
			Y += 28.f;
			HUD->DrawRect(Disabled, PanelX + 30.f, Y, PanelW - 60.f, 24.f);
			HUD->DrawText(TEXT("Mejorar / demoler / gestionar - bloqueado"), Muted, PanelX + 42.f, Y + 5.f, SmallFont, 0.68f);
			return;
		}

		const FString& SlotLabel = PC->GetSelectedBuildingSlotLabel();
		const bool bCityMode = Context == EWLCampaignBuildingPanelContext::City;
		const EWLCampaignBuildingSlotState State = PC->GetCampaignBuildingSlotState(SlotLabel, Slots.Find(SlotLabel), bCityMode);
		const FString DisplayLabel = FWLCampaignBuildingSlotRules::GetSlotDisplayLabel(Catalog, Context, SlotLabel);
		HUD->DrawText(ShortenForPanel(DisplayLabel, 30), Text, PanelX + 30.f, Y, SmallFont, 0.86f);
		Y += 19.f;
		HUD->DrawText(ShortenForPanel(FWLCampaignBuildingSlotRules::GetSlotDescription(Catalog, Context, SlotLabel), 58),
			Muted, PanelX + 30.f, Y, SmallFont, 0.68f);
		Y += 25.f;

		if (State == EWLCampaignBuildingSlotState::Locked)
		{
			HUD->DrawRect(Disabled, PanelX + 30.f, Y, PanelW - 60.f, 30.f);
			HUD->DrawText(TEXT("Slot bloqueado para costos, upgrades o sistemas futuros."), Muted, PanelX + 42.f, Y + 8.f, SmallFont, 0.70f);
			return;
		}

		const FString BuildingId = PC->GetCampaignBuildingIdForSlot(SlotLabel, Slots.Find(SlotLabel), bCityMode);
		if (!BuildingId.IsEmpty())
		{
			const FWLCampaignBuildingDefinition* Building = Catalog.FindBuilding(BuildingId);
			if (!Building)
			{
				return;
			}
			HUD->DrawText(ShortenForPanel(Building->Name, 34), Text, PanelX + 30.f, Y, SmallFont, 0.88f);
			HUD->DrawText(FString::Printf(TEXT("%s  |  Nivel %d"), *Building->TypeLabel, Building->Level),
				Gold, PanelX + PanelW - 180.f, Y, SmallFont, 0.70f);
			Y += 20.f;
			HUD->DrawText(ShortenForPanel(Building->Description, 60), Muted, PanelX + 30.f, Y, SmallFont, 0.68f);
			Y += 24.f;
			DrawTagRow(HUD, SmallFont, Building->Effects, PanelX + 30.f, Y, PanelW - 72.f, FLinearColor(0.12f, 0.17f, 0.14f, 0.92f), Text);
			if (Y < PanelY + PanelH - 72.f)
			{
				HUD->DrawRect(Disabled, PanelX + 30.f, Y, 116.f, 24.f);
				HUD->DrawText(TEXT("Mejorar - bloqueado"), Muted, PanelX + 38.f, Y + 5.f, SmallFont, 0.64f);
				HUD->DrawRect(Disabled, PanelX + 154.f, Y, 116.f, 24.f);
				HUD->DrawText(TEXT("Demoler - bloqueado"), Muted, PanelX + 162.f, Y + 5.f, SmallFont, 0.64f);
				HUD->DrawRect(Disabled, PanelX + 278.f, Y, 104.f, 24.f);
				HUD->DrawText(TEXT("Gestionar"), Muted, PanelX + 292.f, Y + 5.f, SmallFont, 0.64f);
			}
			return;
		}

		TArray<FWLCampaignBuildingDefinition> CompatibleBuildings;
		FWLCampaignBuildingSlotRules::GetCompatibleBuildings(Catalog, Context, SlotLabel, CompatibleBuildings);
		const int32 MaxOptions = FMath::Min(CompatibleBuildings.Num(), 3);
		if (MaxOptions == 0)
		{
			HUD->DrawText(TEXT("Sin edificios compatibles en este catalogo placeholder."), Muted, PanelX + 30.f, Y, SmallFont, 0.70f);
			return;
		}

		for (int32 Index = 0; Index < MaxOptions && Y < PanelY + PanelH - 48.f; ++Index)
		{
			const FWLCampaignBuildingDefinition& Building = CompatibleBuildings[Index];
			HUD->DrawRect(FLinearColor(0.020f, 0.038f, 0.044f, 0.94f), PanelX + 30.f, Y, PanelW - 60.f, 46.f);
			HUD->DrawText(ShortenForPanel(Building.Name, 28), Text, PanelX + 40.f, Y + 7.f, SmallFont, 0.76f);
			HUD->DrawText(ShortenForPanel(JoinDisplayList(Building.Effects), 36), Muted, PanelX + 40.f, Y + 25.f, SmallFont, 0.62f);
			HUD->DrawRect(FLinearColor(0.50f, 0.40f, 0.18f, 0.96f), PanelX + PanelW - 105.f, Y + 12.f, 78.f, 23.f);
			HUD->DrawText(TEXT("Construir"), Text, PanelX + PanelW - 94.f, Y + 17.f, SmallFont, 0.62f);
			Y += 51.f;
		}
	}
