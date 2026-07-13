// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignSimulationAudit.h"

#include "Save/WLLocalSaveGame.h"
#include "Core/WLTacticalBattleTypes.h"

namespace
{
	void AppendField(FString& Canonical, const FString& Value)
	{
		Canonical += FString::FromInt(Value.Len());
		Canonical += TEXT(":");
		Canonical += Value;
		Canonical += TEXT("|");
	}

	template <typename T>
	void AppendNumber(FString& Canonical, T Value)
	{
		AppendField(Canonical, LexToString(Value));
	}

	FString StableFnv1a64(const FString& Canonical)
	{
		const FTCHARToUTF8 Utf8(*Canonical);
		uint64 Hash = 14695981039346656037ull;
		for (int32 Index = 0; Index < Utf8.Length(); ++Index)
		{
			Hash ^= static_cast<uint8>(Utf8.Get()[Index]);
			Hash *= 1099511628211ull;
		}
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
	}

	template <typename T, typename KeyFn>
	TArray<const T*> SortedPointers(const TArray<T>& Items, KeyFn GetKey)
	{
		TArray<const T*> Result;
		Result.Reserve(Items.Num());
		for (const T& Item : Items)
		{
			Result.Add(&Item);
		}
		Result.Sort([&GetKey](const T& Left, const T& Right)
		{
			return GetKey(Left) < GetKey(Right);
		});
		return Result;
	}
}

FWLCampaignAuditResult FWLCampaignSimulationAudit::Audit(
	const UWLLocalSaveGame& Save,
	int32 MonthsPerYear,
	int32 DaysPerMonth)
{
	FWLCampaignAuditResult Result;
	if (Save.CurrentYear < 1 || Save.CurrentMonth < 1 || Save.CurrentMonth > MonthsPerYear
		|| Save.CurrentDay < 1 || Save.CurrentDay > DaysPerMonth)
	{
		Result.Violations.Add(TEXT("La fecha de campania esta fuera del calendario configurado."));
	}

	FString Canonical;
	AppendNumber(Canonical, Save.SaveVersion);
	AppendField(Canonical, Save.SelectedNationIso.ToUpper());
	AppendNumber(Canonical, Save.CurrentYear);
	AppendNumber(Canonical, Save.CurrentMonth);
	AppendNumber(Canonical, Save.CurrentDay);

	TSet<FString> NationIds;
	for (const FWLNationTreasurySave* Treasury : SortedPointers(Save.NationTreasuries,
		[](const FWLNationTreasurySave& Item) { return Item.NationIso.ToUpper(); }))
	{
		const FString Id = Treasury->NationIso.ToUpper();
		if (Id.IsEmpty() || NationIds.Contains(Id))
		{
			Result.Violations.Add(FString::Printf(TEXT("Tesoro con nacion vacia o duplicada: %s."), *Id));
		}
		NationIds.Add(Id);
		if (!FMath::IsFinite(Treasury->DailyTreasuryRemainder))
		{
			Result.Violations.Add(FString::Printf(TEXT("Resto diario no finito para %s."), *Id));
		}
		AppendField(Canonical, Id);
		AppendNumber(Canonical, Treasury->Treasury);
		AppendNumber(Canonical, FMath::RoundToInt64(Treasury->DailyTreasuryRemainder * 1000000.0));
		AppendNumber(Canonical, Treasury->TaxRatePercent);
		AppendNumber(Canonical, Treasury->TariffRatePercent);
	}

	TSet<FString> ProvinceIds;
	for (const FWLProvinceRuntimeState* Province : SortedPointers(Save.ProvinceStates,
		[](const FWLProvinceRuntimeState& Item) { return Item.ProvinceId.ToLower(); }))
	{
		const FString Id = Province->ProvinceId.ToLower();
		if (Id.IsEmpty() || ProvinceIds.Contains(Id))
		{
			Result.Violations.Add(FString::Printf(TEXT("Provincia vacia o duplicada: %s."), *Id));
		}
		ProvinceIds.Add(Id);
		if (Province->ControllerIso.IsEmpty() || Province->Population < 0
			|| Province->PublicOrder < 0 || Province->PublicOrder > 100)
		{
			Result.Violations.Add(FString::Printf(TEXT("Estado provincial invalido: %s."), *Id));
		}
		AppendField(Canonical, Id);
		AppendField(Canonical, Province->ControllerIso.ToUpper());
		AppendNumber(Canonical, Province->Population);
		AppendNumber(Canonical, Province->PublicOrder);
	}

	TSet<FString> ArmyIds;
	for (const FWLArmy* Army : SortedPointers(Save.Armies,
		[](const FWLArmy& Item) { return Item.Id.ToLower(); }))
	{
		const FString Id = Army->Id.ToLower();
		if (Id.IsEmpty() || ArmyIds.Contains(Id) || Army->OwnerIso.IsEmpty() || Army->ProvinceId.IsEmpty())
		{
			Result.Violations.Add(FString::Printf(TEXT("Ejercito invalido o duplicado: %s."), *Id));
		}
		ArmyIds.Add(Id);
		AppendField(Canonical, Id);
		AppendField(Canonical, Army->OwnerIso.ToUpper());
		AppendField(Canonical, Army->ProvinceId.ToLower());
		AppendField(Canonical, Army->General);
		AppendField(Canonical, Army->SourceBaseId.ToUpper());
		TArray<FString> Units = Army->Units;
		Units.Sort();
		for (const FString& Unit : Units)
		{
			AppendField(Canonical, Unit.ToLower());
		}
		TArray<FString> Recovering = Army->RecoveringUnits;
		Recovering.Sort();
		AppendNumber(Canonical, Recovering.Num());
		for (const FString& Unit : Recovering)
		{
			AppendField(Canonical, Unit.ToLower());
		}
	}

	for (const FWLGarrisonUnitSave* Garrison : SortedPointers(Save.GarrisonUnits,
		[](const FWLGarrisonUnitSave& Item) { return Item.BaseId.ToUpper() + TEXT("|") + Item.UnitType.ToLower(); }))
	{
		if (Garrison->BaseId.IsEmpty() || Garrison->UnitType.IsEmpty() || Garrison->Count < 0)
		{
			Result.Violations.Add(TEXT("Fila de guarnicion invalida."));
		}
		AppendField(Canonical, Garrison->BaseId.ToUpper());
		AppendField(Canonical, Garrison->UnitType.ToLower());
		AppendNumber(Canonical, Garrison->Count);
	}

	for (const FWLRecruitOrderSave* Order : SortedPointers(Save.RecruitOrders,
		[](const FWLRecruitOrderSave& Item) { return Item.BaseId.ToUpper() + TEXT("|") + Item.UnitType.ToLower(); }))
	{
		if (Order->BaseId.IsEmpty() || Order->UnitType.IsEmpty() || Order->Batch <= 0
			|| Order->TurnsTotal <= 0 || Order->TurnsRemaining < 0 || Order->TurnsRemaining > Order->TurnsTotal)
		{
			Result.Violations.Add(TEXT("Orden de reclutamiento invalida."));
		}
		AppendField(Canonical, Order->BaseId.ToUpper());
		AppendField(Canonical, Order->UnitType.ToLower());
		AppendNumber(Canonical, Order->Batch);
		AppendNumber(Canonical, Order->TurnsRemaining);
		AppendNumber(Canonical, Order->TurnsTotal);
	}

	Result.StableHash = StableFnv1a64(Canonical);
	Result.bValid = Result.Violations.IsEmpty();
	return Result;
}

TArray<FString> FWLCampaignSimulationAudit::AuditActiveBattles(
	const TArray<FWLTacticalBattleState>& Battles)
{
	TArray<FString> Violations;
	TSet<FString> ActiveArmyIds;
	TSet<FString> BattleIds;
	for (const FWLTacticalBattleState& Battle : Battles)
	{
		if (!Battle.bActive)
		{
			continue;
		}
		const FString BattleId = Battle.BattleId.TrimStartAndEnd().ToUpper();
		if (BattleId.IsEmpty() || BattleIds.Contains(BattleId))
		{
			Violations.Add(FString::Printf(TEXT("Batalla activa vacia o duplicada: %s."), *BattleId));
		}
		BattleIds.Add(BattleId);
		for (const FString& RawArmyId : { Battle.AttackerArmyId, Battle.DefenderArmyId })
		{
			const FString ArmyId = RawArmyId.TrimStartAndEnd().ToUpper();
			if (ArmyId.IsEmpty() || ActiveArmyIds.Contains(ArmyId))
			{
				Violations.Add(FString::Printf(TEXT("Ejercito en mas de una batalla activa: %s."), *ArmyId));
			}
			ActiveArmyIds.Add(ArmyId);
		}
	}
	return Violations;
}
