// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLCampaignHUD.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
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

	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const UWLCampaignGameInstance* CampaignGI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!Registry || !Tick || !Canvas || !CampaignGI || !CampaignGI->HasActiveCampaign())
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	float X = 60.f;
	float Y = 60.f;
	const float LineHeight = 24.f;

	DrawText(TEXT("WORLD LEADER  -  Campana estrategica"), FLinearColor::White, X, Y, Font, 1.3f);
	Y += LineHeight * 1.6f;

	DrawText(FString::Printf(TEXT("Fecha: %02d/%d"), Tick->GetCurrentMonth(), Tick->GetCurrentYear()),
		FLinearColor(0.75f, 0.9f, 1.0f), X, Y, Font);
	Y += LineHeight * 1.5f;

	FWLNationData SelectedNation;
	if (CampaignGI->GetSelectedNation(SelectedNation))
	{
		DrawText(FString::Printf(TEXT("Nacion del jugador: %s (%s)"), *SelectedNation.Name, *SelectedNation.Iso),
			FLinearColor(1.0f, 0.85f, 0.2f), X, Y, Font);
		Y += LineHeight * 1.5f;
	}

	TArray<FWLNationData> Nations = Registry->GetAllNations();
	Nations.Sort([](const FWLNationData& A, const FWLNationData& B)
	{
		return A.Iso < B.Iso;
	});
	for (const FWLNationData& Nation : Nations)
	{
		const FString Line = FString::Printf(TEXT("%s (%s)    Tesoro: %lld    Balance/mes: %+lld"),
			*Nation.Name, *Nation.Iso,
			Tick->GetTreasury(Nation.Iso), Tick->GetMonthlyBalance(Nation.Iso));
		DrawText(Line, FLinearColor(1.0f, 0.85f, 0.2f), X, Y, Font);
		Y += LineHeight;
	}

	Y += LineHeight;
	DrawText(FString::Printf(TEXT("Provincias cargadas: %d    Naciones: %d"),
		Registry->GetProvinceCount(), Registry->GetNationCount()),
		FLinearColor(0.6f, 0.6f, 0.6f), X, Y, Font);
	Y += LineHeight * 1.5f;

	DrawText(TEXT("[M] Avanzar mes      [P] Imprimir estado al log      [F5] Guardar      [B] Construir"),
		FLinearColor(0.55f, 0.8f, 0.55f), X, Y, Font);

	if (const AWLCampaignPlayerController* PC = Cast<AWLCampaignPlayerController>(GetOwningPlayerController()))
	{
		if (PC->HasLastActionMessage())
		{
			Y += LineHeight * 1.2f;
			DrawText(PC->GetLastActionMessage(),
				PC->WasLastActionSuccessful() ? FLinearColor(0.55f, 0.95f, 0.55f) : FLinearColor(0.95f, 0.45f, 0.35f),
				X, Y, Font);
		}

		if (PC->HasSelectedProvince())
		{
			FWLProvinceData Province;
			if (Registry->GetProvince(PC->GetSelectedProvinceId(), Province))
			{
				FWLProvinceRuntimeState ProvinceState;
				if (!Tick->GetProvinceState(Province.Id, ProvinceState))
				{
					ProvinceState.ProvinceId = Province.Id;
					ProvinceState.Population = Province.Population;
					ProvinceState.PublicOrder = Tick->GetBalanceRules().InitialPublicOrder;
				}

				Y += LineHeight * 2.f;
				DrawText(TEXT("Provincia seleccionada"), FLinearColor::White, X, Y, Font, 1.1f);
				Y += LineHeight;
				DrawText(FString::Printf(TEXT("%s (%s)"), *Province.Name, *Province.Id),
					FLinearColor(1.0f, 0.85f, 0.2f), X, Y, Font);
				Y += LineHeight;
				const FString ControllerIso = Tick->GetProvinceControllerIso(Province.Id);
				DrawText(FString::Printf(TEXT("Region: %s    Pais base: %s    Control: %s    Terreno: %s"),
					*Province.Region, *Province.CountryIso, *ControllerIso, *TerrainToText(Province.Terrain)),
					FLinearColor(0.75f, 0.9f, 1.0f), X, Y, Font);
				Y += LineHeight;
				DrawText(FString::Printf(TEXT("Capital: %s    Poblacion: %lld    Orden publico: %d    Infra: %d"),
					*Province.Capital, ProvinceState.Population, ProvinceState.PublicOrder, Province.Infrastructure),
					FLinearColor(0.85f, 0.85f, 0.85f), X, Y, Font);
				Y += LineHeight;
				DrawText(FString::Printf(TEXT("Recursos  Oil:%d  Gas:%d  Food:%d  Minerals:%d  Industry:%d"),
					Province.BaseOil, Province.BaseGas, Province.BaseFood, Province.BaseMinerals, Province.BaseIndustry),
					FLinearColor(0.85f, 0.85f, 0.85f), X, Y, Font);
				Y += LineHeight;
				DrawText(FString::Printf(TEXT("Ingreso:%lld  Upkeep:%lld  Balance:%+lld"),
					Tick->GetProvinceMonthlyIncome(Province.Id),
					Tick->GetProvinceMonthlyUpkeep(Province.Id),
					Tick->GetProvinceMonthlyBalance(Province.Id)),
					FLinearColor(0.55f, 0.95f, 0.55f), X, Y, Font);

				const TArray<FString> BuiltIds = Tick->GetProvinceBuildings(Province.Id);
				Y += LineHeight;
				if (BuiltIds.IsEmpty())
				{
					DrawText(TEXT("Edificios: ninguno"), FLinearColor(0.6f, 0.6f, 0.6f), X, Y, Font);
				}
				else
				{
					TArray<FString> BuiltNames;
					for (const FString& BuildingId : BuiltIds)
					{
						FWLBuildingData Building;
						BuiltNames.Add(Registry->GetBuilding(BuildingId, Building) ? Building.Name : BuildingId);
					}
					DrawText(FString::Printf(TEXT("Edificios: %s"), *FString::Join(BuiltNames, TEXT(", "))),
						FLinearColor(0.85f, 0.85f, 0.85f), X, Y, Font);
				}

				Y += LineHeight;
				const bool bOwnProvince = ControllerIso == CampaignGI->GetSelectedNationIso();
				DrawText(bOwnProvince
						? TEXT("[B] Construir edificio recomendado")
						: TEXT("Provincia no controlada por tu nacion"),
					bOwnProvince ? FLinearColor(1.0f, 0.85f, 0.2f) : FLinearColor(0.7f, 0.45f, 0.45f),
					X, Y, Font);
			}
		}

		if (PC->HasSelectedCountry())
		{
			Y += LineHeight * 2.f;
			DrawText(TEXT("Pais seleccionado"), FLinearColor::White, X, Y, Font, 1.1f);
			Y += LineHeight;
			DrawText(FString::Printf(TEXT("%s (%s)"), *PC->GetSelectedCountryName(), *PC->GetSelectedCountryIso()),
				FLinearColor(1.0f, 0.85f, 0.2f), X, Y, Font);
			Y += LineHeight;
			DrawText(PC->GetSelectedCountryContinent(), FLinearColor(0.75f, 0.9f, 1.0f), X, Y, Font);

			FWLNationData Nation;
			if (Registry->GetNation(PC->GetSelectedCountryIso(), Nation))
			{
				const TArray<FWLProvinceData> NationProvinces = Registry->GetProvincesByNation(Nation.Iso);
				FString CapitalName = Nation.CapitalProvinceId;
				FWLProvinceData Capital;
				if (Registry->GetProvince(Nation.CapitalProvinceId, Capital))
				{
					CapitalName = Capital.Name;
				}

				Y += LineHeight * 1.2f;
				DrawText(TEXT("Jugable en Phase 1"), FLinearColor(0.55f, 0.95f, 0.55f), X, Y, Font);
				Y += LineHeight;
				DrawText(FString::Printf(TEXT("Gobierno: %s"), *Nation.GovernmentType),
					FLinearColor(0.85f, 0.85f, 0.85f), X, Y, Font);
				Y += LineHeight;
				DrawText(FString::Printf(TEXT("Capital: %s"), *CapitalName),
					FLinearColor(0.85f, 0.85f, 0.85f), X, Y, Font);
				Y += LineHeight;
				DrawText(FString::Printf(TEXT("Provincias: %d"), NationProvinces.Num()),
					FLinearColor(0.85f, 0.85f, 0.85f), X, Y, Font);
				Y += LineHeight;
				DrawText(FString::Printf(TEXT("Tesoro: %lld    Balance/mes: %+lld"),
					Tick->GetTreasury(Nation.Iso), Tick->GetMonthlyBalance(Nation.Iso)),
					FLinearColor(1.0f, 0.85f, 0.2f), X, Y, Font);
			}
			else
			{
				Y += LineHeight * 1.2f;
				DrawText(TEXT("Visible en mapa"), FLinearColor(0.85f, 0.85f, 0.85f), X, Y, Font);
				Y += LineHeight;
				DrawText(TEXT("Sin datos jugables todavia"), FLinearColor(0.6f, 0.6f, 0.6f), X, Y, Font);
			}
		}
	}
}
