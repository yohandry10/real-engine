// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLCampaignHUD.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "UI/WLCampaignBuildingSlotData.h"
#include "UI/WLCampaignSelectionPanelData.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

namespace
{
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
		HUD->DrawText(TEXT("FUERZA MILITAR PLACEHOLDER"), Gold, PanelX + 24.f, Y, SmallFont, 0.78f);
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
				? FString::Printf(TEXT("Ruta: %s (%d turno placeholder)"), *PC->GetForceMovementRouteSummary(), PC->GetForceMovementEstimatedTurns())
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

		HUD->DrawText(TEXT("ACCIONES BLOQUEADAS"), Gold, PanelX + 18.f, DisabledStartY, SmallFont, 0.76f);
		DisabledStartY += 22.f;
		TArray<FString> DisabledActions;
		for (const FString& Action : PC->GetSelectedForceDisabledActions())
		{
			if (!Action.Equals(TEXT("Mover"), ESearchCase::IgnoreCase))
			{
				DisabledActions.Add(Action);
			}
		}
		const int32 MaxActions = FMath::Min(DisabledActions.Num(), PC->IsForceMovementModeActive() ? 4 : 6);
		for (int32 Index = 0; Index < MaxActions; ++Index)
		{
			const int32 Col = Index % 2;
			const int32 Row = Index / 2;
			const float ButtonX = ButtonX0 + static_cast<float>(Col) * (ButtonW + Gap);
			const float ButtonY = DisabledStartY + static_cast<float>(Row) * (ButtonH + Gap);
			HUD->DrawRect(Disabled, ButtonX, ButtonY, ButtonW, ButtonH);
			HUD->DrawText(ShortenForPanel(DisabledActions[Index], 20), Muted, ButtonX + 9.f, ButtonY + 7.f, SmallFont, 0.64f);
		}

		HUD->DrawText(ShortenForPanel(PC->GetSelectedForceDetailLevel(), 46), Muted, PanelX + 18.f, PanelY + PanelH - 24.f, SmallFont, 0.70f);
	}
}

UWLDataRegistry* AWLCampaignHUD::GetRegistry() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
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

	const float ButtonY = 58.f;
	const float ButtonW = 158.f;
	const float ButtonH = 38.f;
	const float CampaignX = W - 386.f;
	const float DiplomacyX = W - 210.f;
	DrawRect(bDiplomacy ? Ink : Active, CampaignX, ButtonY, ButtonW, ButtonH);
	DrawRect(bDiplomacy ? Active : Ink, DiplomacyX, ButtonY, ButtonW, ButtonH);
	DrawText(TEXT("Campana 3D"), bDiplomacy ? Text : Gold, CampaignX + 18.f, ButtonY + 9.f, SmallFont, 1.0f);
	DrawText(TEXT("Diplomacia"), bDiplomacy ? Gold : Text, DiplomacyX + 25.f, ButtonY + 9.f, SmallFont, 1.0f);

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

	DrawText(FString::Printf(TEXT("Fecha: %02d/%d"), Tick->GetCurrentMonth(), Tick->GetCurrentYear()),
		FLinearColor(0.75f, 0.9f, 1.0f), X, Y, SmallFont);
	Y += LineHeight;

	if (SelectedNation.IsValid())
	{
		DrawText(FString::Printf(TEXT("Tesoro: %lld   Balance/mes: %+lld"),
			Tick->GetTreasury(SelectedNation.Iso), Tick->GetMonthlyBalance(SelectedNation.Iso)),
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

	DrawRect(InkHard, 0.f, H - 34.f, W, 34.f);
	DrawText(TEXT("[D] Alternar vista   Rueda/+/- Zoom   Flechas Pan   [R] Reset   [F] Teatro   [G] America   [M] Mes   [F5] Guardar   [B] Construir"),
		Muted, 36.f, H - 24.f, SmallFont, 0.88f);
}
