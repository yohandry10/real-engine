// Copyright World Leader project. See ROADMAP.md.

#include "Military/WLMilitarySubsystem.h"
#include "Military/WLMilitaryLibrary.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

namespace
{
	FString NormalizeArmyId(const FString& In)
	{
		return In.TrimStartAndEnd().ToUpper();
	}

	FString NormalizeIso(const FString& In)
	{
		return In.TrimStartAndEnd().ToUpper();
	}

	FString NormalizeProvinceId(const FString& In)
	{
		return In.TrimStartAndEnd().ToUpper();
	}

	FString NormalizeDataId(const FString& In)
	{
		return In.TrimStartAndEnd().ToLower();
	}
}

UWLDataRegistry* UWLMilitarySubsystem::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLStrategicTickSubsystem* UWLMilitarySubsystem::GetStrategicTick() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}

void UWLMilitarySubsystem::ResetMilitaryState()
{
	Armies.Reset();
	NextArmyNumber = 1;
}

FWLArmy* UWLMilitarySubsystem::FindArmy(const FString& ArmyId)
{
	const FString NormalizedId = NormalizeArmyId(ArmyId);
	return Armies.FindByPredicate([&NormalizedId](const FWLArmy& A) { return A.Id == NormalizedId; });
}

const FWLArmy* UWLMilitarySubsystem::FindArmy(const FString& ArmyId) const
{
	const FString NormalizedId = NormalizeArmyId(ArmyId);
	return Armies.FindByPredicate([&NormalizedId](const FWLArmy& A) { return A.Id == NormalizedId; });
}

int32 UWLMilitarySubsystem::SumUnitStat(const FWLArmy& Army, bool bAttack) const
{
	const UWLDataRegistry* Reg = GetRegistry();
	if (!Reg) return 0;

	int32 Sum = 0;
	for (const FString& UnitId : Army.Units)
	{
		FWLUnitData Unit;
		if (Reg->GetUnit(UnitId, Unit))
		{
			Sum += bAttack ? Unit.Attack : Unit.Defense;
		}
	}
	return Sum;
}

void UWLMilitarySubsystem::ApplyCasualties(FWLArmy& Army, float LossFraction) const
{
	const int32 Losses = FMath::FloorToInt(Army.Units.Num() * LossFraction);
	for (int32 i = 0; i < Losses && Army.Units.Num() > 0; ++i)
	{
		Army.Units.Pop();
	}
}

FString UWLMilitarySubsystem::CreateArmy(const FString& OwnerIso, const FString& ProvinceId, const FString& UnitId, int32 Count, const FString& General)
{
	const UWLDataRegistry* Reg = GetRegistry();
	if (!Reg) return FString();

	const FString NormalizedOwner = NormalizeIso(OwnerIso);
	const FString NormalizedProvinceId = NormalizeProvinceId(ProvinceId);
	const FString NormalizedUnitId = NormalizeDataId(UnitId);

	FWLNationData Owner;
	if (!Reg->GetNation(NormalizedOwner, Owner))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CreateArmy: nacion desconocida %s"), *OwnerIso);
		return FString();
	}
	if (Count <= 0)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CreateArmy: count invalido %d para %s"), Count, *NormalizedOwner);
		return FString();
	}

	FWLUnitData Unit;
	if (!Reg->GetUnit(NormalizedUnitId, Unit))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CreateArmy: unidad desconocida %s"), *UnitId);
		return FString();
	}

	FWLProvinceData Province;
	if (!Reg->GetProvince(NormalizedProvinceId, Province))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CreateArmy: provincia desconocida %s"), *ProvinceId);
		return FString();
	}
	const UWLStrategicTickSubsystem* Tick = GetStrategicTick();
	const FString ControllerIso = Tick ? Tick->GetProvinceControllerIso(Province.Id) : Province.CountryIso;
	if (ControllerIso != Owner.Iso)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CreateArmy: %s no controla %s (%s)."),
			*Owner.Iso, *Province.Id, *ControllerIso);
		return FString();
	}

	FWLArmy Army;
	Army.Id = FString::Printf(TEXT("A%d"), NextArmyNumber++);
	Army.OwnerIso = Owner.Iso;
	Army.ProvinceId = Province.Id;
	Army.General = General.IsEmpty() ? TEXT("Comandante") : General;

	for (int32 i = 0; i < Count; ++i)
	{
		Army.Units.Add(Unit.Id);
	}

	Armies.Add(Army);
	UE_LOG(LogWorldLeader, Log, TEXT("Ejercito %s creado: %s x%d en %s (%s)"),
		*Army.Id, *Unit.Name, Count, *Province.Name, *Army.OwnerIso);
	return Army.Id;
}

bool UWLMilitarySubsystem::GetArmy(const FString& ArmyId, FWLArmy& OutArmy) const
{
	if (const FWLArmy* Army = FindArmy(ArmyId))
	{
		OutArmy = *Army;
		return true;
	}
	return false;
}

bool UWLMilitarySubsystem::MoveArmy(const FString& ArmyId, const FString& ToProvinceId, FString& OutMessage)
{
	const UWLDataRegistry* Reg = GetRegistry();
	FWLArmy* Army = FindArmy(ArmyId);
	if (!Reg || !Army)
	{
		OutMessage = FString::Printf(TEXT("Ejercito desconocido: %s"), *ArmyId);
		return false;
	}

	const FString NormalizedToProvinceId = NormalizeProvinceId(ToProvinceId);
	FWLProvinceData From;
	if (!Reg->GetProvince(Army->ProvinceId, From))
	{
		OutMessage = TEXT("Provincia de origen invalida.");
		return false;
	}

	FWLProvinceData To;
	if (!Reg->GetProvince(NormalizedToProvinceId, To))
	{
		OutMessage = FString::Printf(TEXT("Provincia destino desconocida: %s"), *ToProvinceId);
		return false;
	}

	if (Army->ProvinceId == To.Id)
	{
		OutMessage = FString::Printf(TEXT("%s ya esta en %s."), *Army->Id, *To.Id);
		return false;
	}

	if (!From.Neighbors.Contains(To.Id))
	{
		OutMessage = FString::Printf(TEXT("%s no es adyacente a %s."), *To.Id, *Army->ProvinceId);
		return false;
	}

	Army->ProvinceId = To.Id;
	OutMessage = FString::Printf(TEXT("%s se movio a %s."), *Army->Id, *To.Id);
	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutMessage);
	return true;
}

int32 UWLMilitarySubsystem::GetArmyAttack(const FString& ArmyId) const
{
	const FWLArmy* Army = FindArmy(ArmyId);
	return Army ? SumUnitStat(*Army, true) : 0;
}

int32 UWLMilitarySubsystem::GetArmyDefense(const FString& ArmyId) const
{
	const FWLArmy* Army = FindArmy(ArmyId);
	return Army ? SumUnitStat(*Army, false) : 0;
}

EWLBattleResult UWLMilitarySubsystem::AutoResolveBattle(const FString& AttackerId, const FString& DefenderId, FString& OutReport)
{
	const UWLDataRegistry* Reg = GetRegistry();
	FWLArmy* Attacker = FindArmy(AttackerId);
	FWLArmy* Defender = FindArmy(DefenderId);
	if (!Reg || !Attacker || !Defender)
	{
		OutReport = TEXT("Ejercito atacante o defensor no encontrado.");
		return EWLBattleResult::Invalid;
	}
	if (Attacker->Id == Defender->Id)
	{
		OutReport = TEXT("Un ejercito no puede atacarse a si mismo.");
		return EWLBattleResult::Invalid;
	}
	if (Attacker->OwnerIso == Defender->OwnerIso)
	{
		OutReport = FString::Printf(TEXT("%s y %s pertenecen a la misma nacion (%s)."),
			*Attacker->Id, *Defender->Id, *Attacker->OwnerIso);
		return EWLBattleResult::Invalid;
	}

	FWLProvinceData AttackerProvince;
	if (!Reg->GetProvince(Attacker->ProvinceId, AttackerProvince))
	{
		OutReport = FString::Printf(TEXT("Provincia del atacante invalida: %s"), *Attacker->ProvinceId);
		return EWLBattleResult::Invalid;
	}
	if (Attacker->ProvinceId != Defender->ProvinceId && !AttackerProvince.Neighbors.Contains(Defender->ProvinceId))
	{
		OutReport = FString::Printf(TEXT("%s no puede atacar %s desde %s hasta %s: no hay adyacencia."),
			*Attacker->Id, *Defender->Id, *Attacker->ProvinceId, *Defender->ProvinceId);
		return EWLBattleResult::Invalid;
	}

	// Bonus de terreno para el defensor.
	float TerrainMult = 1.0f;
	FWLProvinceData DefProvince;
	if (Reg->GetProvince(Defender->ProvinceId, DefProvince))
	{
		TerrainMult = UWLMilitaryLibrary::TerrainDefenseMultiplier(DefProvince.Terrain);
	}

	const int32 AttackPower = SumUnitStat(*Attacker, true);
	const int32 DefensePower = FMath::RoundToInt(SumUnitStat(*Defender, false) * TerrainMult);
	const EWLBattleResult Result = UWLMilitaryLibrary::ResolveBattle(AttackPower, DefensePower);

	float AttackerLoss = 0.f;
	float DefenderLoss = 0.f;
	switch (Result)
	{
	case EWLBattleResult::AttackerDecisiveVictory: AttackerLoss = 0.10f; DefenderLoss = 1.00f; break;
	case EWLBattleResult::AttackerVictory:         AttackerLoss = 0.25f; DefenderLoss = 0.50f; break;
	case EWLBattleResult::Stalemate:               AttackerLoss = 0.30f; DefenderLoss = 0.30f; break;
	case EWLBattleResult::DefenderVictory:         AttackerLoss = 0.50f; DefenderLoss = 0.25f; break;
	case EWLBattleResult::DefenderDecisiveVictory: AttackerLoss = 1.00f; DefenderLoss = 0.10f; break;
	}

	ApplyCasualties(*Attacker, AttackerLoss);
	ApplyCasualties(*Defender, DefenderLoss);

	const bool bAttackerWins = (Result == EWLBattleResult::AttackerDecisiveVictory || Result == EWLBattleResult::AttackerVictory);
	const FString DefenderProvince = Defender->ProvinceId;
	const bool bDefenderDestroyed = (Defender->Units.Num() == 0);
	const bool bEnemyStillPresent = Armies.ContainsByPredicate(
		[&DefenderProvince, &Attacker, &Defender](const FWLArmy& Army)
		{
			return Army.Id != Defender->Id
				&& Army.ProvinceId == DefenderProvince
				&& Army.OwnerIso != Attacker->OwnerIso
				&& Army.Units.Num() > 0;
		});

	OutReport = FString::Printf(
		TEXT("Batalla en %s: %s (atk %d) vs %s (def %d, terreno x%.2f) -> %s. Quedan: %s=%d, %s=%d."),
		*DefenderProvince, *AttackerId, AttackPower, *DefenderId, DefensePower, TerrainMult,
		*UWLMilitaryLibrary::BattleResultToString(Result),
		*AttackerId, Attacker->Units.Num(), *DefenderId, Defender->Units.Num());

	// Ocupacion: si el atacante gana y aniquila al defensor, ocupa la provincia.
	if (bAttackerWins && bDefenderDestroyed && !bEnemyStillPresent)
	{
		Attacker->ProvinceId = DefenderProvince;
		if (UWLStrategicTickSubsystem* Tick = GetStrategicTick())
		{
			FString OccupationMessage;
			Tick->SetProvinceController(DefenderProvince, Attacker->OwnerIso, OccupationMessage);
			OutReport += FString::Printf(TEXT(" %s ocupa %s. %s"), *AttackerId, *DefenderProvince, *OccupationMessage);
		}
		else
		{
			OutReport += FString::Printf(TEXT(" %s ocupa %s."), *AttackerId, *DefenderProvince);
		}
	}
	else if (bAttackerWins && bDefenderDestroyed && bEnemyStillPresent)
	{
		Attacker->ProvinceId = DefenderProvince;
		OutReport += FString::Printf(TEXT(" %s entra en %s, pero quedan fuerzas enemigas."), *AttackerId, *DefenderProvince);
	}

	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutReport);

	// Eliminar ejercitos aniquilados (esto invalida los punteros anteriores).
	Armies.RemoveAll([](const FWLArmy& A) { return A.Units.Num() == 0; });

	return Result;
}

void UWLMilitarySubsystem::WriteSaveSnapshot(TArray<FWLArmy>& OutArmies, int32& OutNextArmyNumber) const
{
	OutArmies = Armies;
	OutArmies.Sort([](const FWLArmy& A, const FWLArmy& B)
	{
		return A.Id < B.Id;
	});
	OutNextArmyNumber = FMath::Max(1, NextArmyNumber);
}

bool UWLMilitarySubsystem::RestoreSaveSnapshot(
	const TArray<FWLArmy>& SavedArmies,
	int32 SavedNextArmyNumber,
	FString& OutMessage)
{
	const UWLDataRegistry* Reg = GetRegistry();
	if (!Reg)
	{
		OutMessage = TEXT("Registro de datos no disponible.");
		return false;
	}

	ResetMilitaryState();

	int32 Restored = 0;
	for (const FWLArmy& SavedArmy : SavedArmies)
	{
		FWLNationData Nation;
		FWLProvinceData Province;
		if (!Reg->GetNation(SavedArmy.OwnerIso, Nation) || !Reg->GetProvince(SavedArmy.ProvinceId, Province))
		{
			continue;
		}

		FWLArmy Army;
		Army.Id = NormalizeArmyId(SavedArmy.Id);
		Army.OwnerIso = Nation.Iso;
		Army.ProvinceId = Province.Id;
		Army.General = SavedArmy.General.IsEmpty() ? TEXT("Comandante") : SavedArmy.General;

		for (const FString& UnitId : SavedArmy.Units)
		{
			FWLUnitData Unit;
			if (Reg->GetUnit(UnitId, Unit))
			{
				Army.Units.Add(Unit.Id);
			}
		}

		if (!Army.Id.IsEmpty() && !Army.Units.IsEmpty() && !Armies.ContainsByPredicate(
			[&Army](const FWLArmy& Existing) { return Existing.Id == Army.Id; }))
		{
			Armies.Add(MoveTemp(Army));
			++Restored;
		}
	}

	NextArmyNumber = FMath::Max(1, SavedNextArmyNumber);
	for (const FWLArmy& Army : Armies)
	{
		if (Army.Id.StartsWith(TEXT("A")))
		{
			const int32 ParsedNumber = FCString::Atoi(*Army.Id.Mid(1));
			NextArmyNumber = FMath::Max(NextArmyNumber, ParsedNumber + 1);
		}
	}

	OutMessage = FString::Printf(TEXT("%d ejercitos restaurados."), Restored);
	return true;
}
