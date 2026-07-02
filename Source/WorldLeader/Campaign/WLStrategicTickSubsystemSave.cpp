// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLStrategicTickSubsystemPrivate.h"

#include "Campaign/WLDataRegistry.h"

using WLStrategicTickPrivate::ClampPublicOrder;
using WLStrategicTickPrivate::NormalizeIso;
using WLStrategicTickPrivate::ProvinceSupportsBuilding;

void UWLStrategicTickSubsystem::WriteSaveSnapshot(
	int32& OutYear,
	int32& OutMonth,
	TArray<FWLNationTreasurySave>& OutTreasuries,
	TArray<FWLProvinceBuildingsSave>& OutProvinceBuildings,
	TArray<FWLProvinceRuntimeState>& OutProvinceStates,
	TArray<FWLMarketShockState>* OutMarketShocks,
	TArray<FWLFinancialInstrumentState>* OutFinancialInstruments,
	TArray<FWLForeignSupportState>* OutForeignSupportStates) const
{
	OutYear = CurrentYear;
	OutMonth = CurrentMonth;

	OutTreasuries.Reset();
	for (const TPair<FString, int64>& Pair : Treasuries)
	{
		FWLNationTreasurySave SavedTreasury;
		SavedTreasury.NationIso = Pair.Key;
		SavedTreasury.Treasury = Pair.Value;
		if (const int32* TaxRate = TaxRates.Find(Pair.Key))
		{
			SavedTreasury.TaxRatePercent = *TaxRate;   // FE1.2: -1 se mantiene si nunca se ajusto
		}
		if (const int32* TariffRate = TariffRates.Find(Pair.Key))
		{
			SavedTreasury.TariffRatePercent = *TariffRate;   // FE4.3
		}
		OutTreasuries.Add(SavedTreasury);
	}
	OutTreasuries.Sort([](const FWLNationTreasurySave& A, const FWLNationTreasurySave& B)
	{
		return A.NationIso < B.NationIso;
	});

	OutProvinceBuildings.Reset();
	for (const TPair<FString, TArray<FString>>& Pair : ProvinceBuildings)
	{
		if (Pair.Value.IsEmpty())
		{
			continue;
		}

		FWLProvinceBuildingsSave SavedBuildings;
		SavedBuildings.ProvinceId = Pair.Key;
		TArray<TPair<FString, int32>> Entries;
		for (const FString& BuildingId : Pair.Value)
		{
			int32 Level = 1;
			if (const TMap<FString, int32>* Levels = ProvinceBuildingLevels.Find(Pair.Key))
			{
				if (const int32* SavedLevel = Levels->Find(BuildingId))
				{
					Level = FMath::Max(1, *SavedLevel);
				}
			}
			Entries.Add(TPair<FString, int32>(BuildingId, Level));
		}
		Entries.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
		{
			return A.Key < B.Key;
		});
		for (const TPair<FString, int32>& Entry : Entries)
		{
			SavedBuildings.BuildingIds.Add(Entry.Key);
			SavedBuildings.BuildingLevels.Add(Entry.Value);
		}
		OutProvinceBuildings.Add(SavedBuildings);
	}
	OutProvinceBuildings.Sort([](const FWLProvinceBuildingsSave& A, const FWLProvinceBuildingsSave& B)
	{
		return A.ProvinceId < B.ProvinceId;
	});

	OutProvinceStates.Reset();
	for (const TPair<FString, FWLProvinceRuntimeState>& Pair : ProvinceStates)
	{
		FWLProvinceRuntimeState SavedState = Pair.Value;
		SavedState.ProvinceId = Pair.Key;
		if (SavedState.ControllerIso.IsEmpty())
		{
			FWLProvinceData Province;
			if (const UWLDataRegistry* Registry = GetDataRegistry(); Registry && Registry->GetProvince(Pair.Key, Province))
			{
				SavedState.ControllerIso = Province.CountryIso;
			}
		}
		else
		{
			SavedState.ControllerIso = NormalizeIso(SavedState.ControllerIso);
		}
		SavedState.Population = FMath::Max<int64>(0, SavedState.Population);
		SavedState.PublicOrder = ClampPublicOrder(SavedState.PublicOrder);
		OutProvinceStates.Add(SavedState);
	}
	OutProvinceStates.Sort([](const FWLProvinceRuntimeState& A, const FWLProvinceRuntimeState& B)
	{
		return A.ProvinceId < B.ProvinceId;
	});

	if (OutMarketShocks)
	{
		OutMarketShocks->Reset();
		for (const FWLMarketShockState& Shock : ActiveMarketShocks)
		{
			if (Shock.IsValid())
			{
				OutMarketShocks->Add(Shock);
			}
		}
		OutMarketShocks->Sort([](const FWLMarketShockState& A, const FWLMarketShockState& B)
		{
			return A.ShockId < B.ShockId;
		});
	}

	if (OutFinancialInstruments)
	{
		OutFinancialInstruments->Reset();
		for (const FWLFinancialInstrumentState& Instrument : FinancialInstruments)
		{
			if (Instrument.PrincipalRemaining > 0 || Instrument.bDefaulted)
			{
				OutFinancialInstruments->Add(Instrument);
			}
		}
		OutFinancialInstruments->Sort([](const FWLFinancialInstrumentState& A, const FWLFinancialInstrumentState& B)
		{
			return A.InstrumentId < B.InstrumentId;
		});
	}

	if (OutForeignSupportStates)
	{
		OutForeignSupportStates->Reset();
		for (const FWLForeignSupportState& Support : ForeignSupportStates)
		{
			if (!Support.bCompleted || Support.Type == EWLForeignSupportType::ForeignDirectInvestment)
			{
				OutForeignSupportStates->Add(Support);
			}
		}
		OutForeignSupportStates->Sort([](const FWLForeignSupportState& A, const FWLForeignSupportState& B)
		{
			return A.SupportId < B.SupportId;
		});
	}
}

bool UWLStrategicTickSubsystem::RestoreSaveSnapshot(
	int32 SavedYear,
	int32 SavedMonth,
	const TArray<FWLNationTreasurySave>& SavedTreasuries,
	const TArray<FWLProvinceBuildingsSave>& SavedProvinceBuildings,
	const TArray<FWLProvinceRuntimeState>& SavedProvinceStates,
	FString& OutMessage)
{
	return RestoreSaveSnapshot(
		SavedYear,
		SavedMonth,
		SavedTreasuries,
		SavedProvinceBuildings,
		SavedProvinceStates,
		TArray<FWLMarketShockState>(),
		TArray<FWLFinancialInstrumentState>(),
		TArray<FWLForeignSupportState>(),
		OutMessage);
}

bool UWLStrategicTickSubsystem::RestoreSaveSnapshot(
	int32 SavedYear,
	int32 SavedMonth,
	const TArray<FWLNationTreasurySave>& SavedTreasuries,
	const TArray<FWLProvinceBuildingsSave>& SavedProvinceBuildings,
	const TArray<FWLProvinceRuntimeState>& SavedProvinceStates,
	const TArray<FWLMarketShockState>& SavedMarketShocks,
	FString& OutMessage)
{
	return RestoreSaveSnapshot(
		SavedYear,
		SavedMonth,
		SavedTreasuries,
		SavedProvinceBuildings,
		SavedProvinceStates,
		SavedMarketShocks,
		TArray<FWLFinancialInstrumentState>(),
		TArray<FWLForeignSupportState>(),
		OutMessage);
}

bool UWLStrategicTickSubsystem::RestoreSaveSnapshot(
	int32 SavedYear,
	int32 SavedMonth,
	const TArray<FWLNationTreasurySave>& SavedTreasuries,
	const TArray<FWLProvinceBuildingsSave>& SavedProvinceBuildings,
	const TArray<FWLProvinceRuntimeState>& SavedProvinceStates,
	const TArray<FWLMarketShockState>& SavedMarketShocks,
	const TArray<FWLFinancialInstrumentState>& SavedFinancialInstruments,
	const TArray<FWLForeignSupportState>& SavedForeignSupportStates,
	FString& OutMessage)
{
	const FWLBalanceRules Rules = GetBalanceRules();
	if (SavedYear <= 0 || SavedMonth < 1 || SavedMonth > Rules.MonthsPerYear)
	{
		OutMessage = FString::Printf(TEXT("Fecha invalida en save: %02d/%d"), SavedMonth, SavedYear);
		return false;
	}

	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		OutMessage = TEXT("Registro de datos no disponible.");
		return false;
	}

	CurrentYear = SavedYear;
	CurrentMonth = SavedMonth;
	ProvinceBuildings.Reset();
	ProvinceBuildingLevels.Reset();
	ActiveMarketShocks.Reset();
	NextMarketShockNumber = 1;
	FinancialInstruments.Reset();
	ForeignSupportStates.Reset();
	NextFinancialInstrumentNumber = 1;
	NextForeignSupportNumber = 1;
	TaxRates.Reset();
	TariffRates.Reset();
	PreviousGDP.Reset();   // FE1.5: el crecimiento se vuelve a medir tras cargar
	GDPGrowth.Reset();
	InitTreasuriesFromData();
	InitProvinceStatesFromData();

	int32 RestoredTreasuries = 0;
	for (const FWLNationTreasurySave& SavedTreasury : SavedTreasuries)
	{
		FWLNationData Nation;
		if (Registry->GetNation(SavedTreasury.NationIso, Nation))
		{
			Treasuries.FindOrAdd(Nation.Iso) = SavedTreasury.Treasury;
			if (SavedTreasury.TaxRatePercent >= 0)
			{
				SetTaxRate(Nation.Iso, SavedTreasury.TaxRatePercent);   // FE1.2
			}
			if (SavedTreasury.TariffRatePercent >= 0)
			{
				SetTariffRate(Nation.Iso, SavedTreasury.TariffRatePercent);   // FE4.3
			}
			++RestoredTreasuries;
		}
	}

	int32 RestoredBuildings = 0;
	for (const FWLProvinceBuildingsSave& SavedProvinceEntry : SavedProvinceBuildings)
	{
		FWLProvinceData Province;
		if (!Registry->GetProvince(SavedProvinceEntry.ProvinceId, Province))
		{
			continue;
		}

		TArray<FString> ValidBuildings;
		TMap<FString, int32> ValidLevels;
		TSet<EWLBuildingSlot> OccupiedSlots;
		for (const FString& ExistingId : ValidBuildings)
		{
			FWLBuildingData ExistingBuilding;
			if (Registry->GetBuilding(ExistingId, ExistingBuilding))
			{
				OccupiedSlots.Add(ExistingBuilding.Slot);
			}
		}
		for (int32 Index = 0; Index < SavedProvinceEntry.BuildingIds.Num(); ++Index)
		{
			const FString& BuildingId = SavedProvinceEntry.BuildingIds[Index];
			FWLBuildingData Building;
			if (Registry->GetBuilding(BuildingId, Building)
				&& ProvinceSupportsBuilding(Province, Building)
				&& !ValidBuildings.Contains(Building.Id)
				&& !OccupiedSlots.Contains(Building.Slot))
			{
				ValidBuildings.Add(Building.Id);
				const int32 RawLevel = SavedProvinceEntry.BuildingLevels.IsValidIndex(Index)
					? SavedProvinceEntry.BuildingLevels[Index]
					: 1;
				ValidLevels.Add(Building.Id, FMath::Clamp(RawLevel, 1, FMath::Clamp(Building.MaxLevel, 1, 5)));
				OccupiedSlots.Add(Building.Slot);
				++RestoredBuildings;
			}
		}
		if (!ValidBuildings.IsEmpty())
		{
			ValidBuildings.Sort();
			ProvinceBuildings.Add(Province.Id, ValidBuildings);
			ProvinceBuildingLevels.Add(Province.Id, MoveTemp(ValidLevels));
		}
	}

	int32 RestoredProvinceStates = 0;
	for (const FWLProvinceRuntimeState& SavedState : SavedProvinceStates)
	{
		FWLProvinceData Province;
		if (Registry->GetProvince(SavedState.ProvinceId, Province))
		{
			FWLProvinceRuntimeState RuntimeState;
			RuntimeState.ProvinceId = Province.Id;
			RuntimeState.ControllerIso = SavedState.ControllerIso.IsEmpty()
				? Province.CountryIso
				: NormalizeIso(SavedState.ControllerIso);
			RuntimeState.Population = FMath::Max<int64>(0, SavedState.Population);
			RuntimeState.PublicOrder = ClampPublicOrder(SavedState.PublicOrder);
			FWLNationData Controller;
			if (Registry->GetNation(RuntimeState.ControllerIso, Controller))
			{
				ProvinceStates.FindOrAdd(Province.Id) = RuntimeState;
				++RestoredProvinceStates;
			}
		}
	}

	int32 RestoredMarketShocks = 0;
	for (const FWLMarketShockState& SavedShock : SavedMarketShocks)
	{
		if (!SavedShock.IsValid())
		{
			continue;
		}

		FWLMarketShockState Shock = SavedShock;
		Shock.GoodId = Shock.GoodId.TrimStartAndEnd().ToLower();
		if (Shock.GoodId == TEXT("all"))
		{
			Shock.GoodId = TEXT("*");
		}
		FWLGoodData Good;
		if (Shock.GoodId != TEXT("*") && !Registry->GetGood(Shock.GoodId, Good))
		{
			continue;
		}
		if (Shock.GoodId != TEXT("*"))
		{
			Shock.GoodId = Good.Id;
		}
		Shock.PriceMultiplier = FMath::Clamp(
			Shock.PriceMultiplier,
			Rules.MinMarketShockPriceMultiplier,
			Rules.MaxMarketShockPriceMultiplier);
		Shock.TotalMonths = FMath::Clamp(
			Shock.TotalMonths > 0 ? Shock.TotalMonths : Shock.RemainingMonths,
			1,
			Rules.MaxMarketShockDurationMonths);
		Shock.RemainingMonths = FMath::Clamp(Shock.RemainingMonths, 1, Shock.TotalMonths);
		if (Shock.Title.TrimStartAndEnd().IsEmpty())
		{
			Shock.Title = FString::Printf(TEXT("Shock de mercado: %s"), *Shock.GoodId);
		}
		ActiveMarketShocks.Add(Shock);
		if (Shock.ShockId.StartsWith(TEXT("MS-")))
		{
			NextMarketShockNumber = FMath::Max(NextMarketShockNumber, FCString::Atoi(*Shock.ShockId.Mid(3)) + 1);
		}
		++RestoredMarketShocks;
	}

	int32 RestoredFinancialInstruments = 0;
	for (FWLFinancialInstrumentState Instrument : SavedFinancialInstruments)
	{
		FWLNationData Nation;
		if (!Registry->GetNation(Instrument.NationIso, Nation))
		{
			continue;
		}
		Instrument.NationIso = Nation.Iso;
		Instrument.CreditorIso = Instrument.CreditorIso.TrimStartAndEnd().ToUpper();
		Instrument.OriginalPrincipal = FMath::Max<int64>(0, Instrument.OriginalPrincipal);
		Instrument.PrincipalRemaining = FMath::Max<int64>(0, Instrument.PrincipalRemaining);
		Instrument.MonthlyPayment = FMath::Max<int64>(0, Instrument.MonthlyPayment);
		Instrument.MonthlyInterestRate = FMath::Clamp(Instrument.MonthlyInterestRate, 0.0, 1.0);
		Instrument.TotalMonths = FMath::Clamp(
			Instrument.TotalMonths > 0 ? Instrument.TotalMonths : Instrument.RemainingMonths,
			1,
			Rules.FinancialInstrumentMaxMonths);
		Instrument.RemainingMonths = FMath::Clamp(Instrument.RemainingMonths, 0, Instrument.TotalMonths);
		if (Instrument.InstrumentId.TrimStartAndEnd().IsEmpty())
		{
			Instrument.InstrumentId = FString::Printf(TEXT("FIN-%04d"), NextFinancialInstrumentNumber++);
		}
		if (Instrument.InstrumentId.Contains(TEXT("-")))
		{
			const int32 DashIndex = Instrument.InstrumentId.Find(TEXT("-"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (DashIndex != INDEX_NONE)
			{
				NextFinancialInstrumentNumber = FMath::Max(
					NextFinancialInstrumentNumber,
					FCString::Atoi(*Instrument.InstrumentId.Mid(DashIndex + 1)) + 1);
			}
		}
		if (Instrument.PrincipalRemaining > 0 || Instrument.bDefaulted)
		{
			FinancialInstruments.Add(Instrument);
			++RestoredFinancialInstruments;
		}
	}

	int32 RestoredForeignSupport = 0;
	for (FWLForeignSupportState Support : SavedForeignSupportStates)
	{
		FWLNationData Recipient;
		FWLNationData Sponsor;
		if (!Registry->GetNation(Support.RecipientIso, Recipient) || !Registry->GetNation(Support.SponsorIso, Sponsor))
		{
			continue;
		}
		Support.RecipientIso = Recipient.Iso;
		Support.SponsorIso = Sponsor.Iso;
		Support.TotalAmount = FMath::Max<int64>(0, Support.TotalAmount);
		Support.MonthlyAmount = FMath::Max<int64>(0, Support.MonthlyAmount);
		Support.AmountDelivered = FMath::Max<int64>(0, Support.AmountDelivered);
		Support.TotalMonths = FMath::Clamp(
			Support.TotalMonths > 0 ? Support.TotalMonths : Support.RemainingMonths,
			1,
			Rules.ForeignSupportMaxMonths);
		Support.RemainingMonths = FMath::Clamp(Support.RemainingMonths, 0, Support.TotalMonths);
		if (Support.SupportId.TrimStartAndEnd().IsEmpty())
		{
			Support.SupportId = FString::Printf(TEXT("SUP-%04d"), NextForeignSupportNumber++);
		}
		if (Support.SupportId.Contains(TEXT("-")))
		{
			const int32 DashIndex = Support.SupportId.Find(TEXT("-"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (DashIndex != INDEX_NONE)
			{
				NextForeignSupportNumber = FMath::Max(
					NextForeignSupportNumber,
					FCString::Atoi(*Support.SupportId.Mid(DashIndex + 1)) + 1);
			}
		}
		if (!Support.bCompleted || Support.Type == EWLForeignSupportType::ForeignDirectInvestment)
		{
			ForeignSupportStates.Add(Support);
			++RestoredForeignSupport;
		}
	}

	OnMonthAdvanced.Broadcast(CurrentYear, CurrentMonth);
	OutMessage = FString::Printf(TEXT("Save restaurado: %02d/%d, %d tesoros, %d edificios, %d estados de provincia, %d shocks de mercado, %d instrumentos financieros, %d apoyos exteriores."),
		CurrentMonth, CurrentYear, RestoredTreasuries, RestoredBuildings, RestoredProvinceStates, RestoredMarketShocks,
		RestoredFinancialInstruments, RestoredForeignSupport);
	return true;
}
