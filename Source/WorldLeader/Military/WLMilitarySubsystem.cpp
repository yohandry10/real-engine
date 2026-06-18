// Copyright World Leader project. See ROADMAP.md.

#include "Military/WLMilitarySubsystem.h"
#include "Military/WLMilitaryLibrary.h"
#include "Campaign/WLDataRegistry.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

UWLDataRegistry* UWLMilitarySubsystem::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

FWLArmy* UWLMilitarySubsystem::FindArmy(const FString& ArmyId)
{
	return Armies.FindByPredicate([&ArmyId](const FWLArmy& A) { return A.Id == ArmyId; });
}

const FWLArmy* UWLMilitarySubsystem::FindArmy(const FString& ArmyId) const
{
	return Armies.FindByPredicate([&ArmyId](const FWLArmy& A) { return A.Id == ArmyId; });
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

	FWLUnitData Unit;
	if (!Reg->GetUnit(UnitId, Unit))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CreateArmy: unidad desconocida %s"), *UnitId);
		return FString();
	}

	FWLProvinceData Province;
	if (!Reg->GetProvince(ProvinceId, Province))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CreateArmy: provincia desconocida %s"), *ProvinceId);
		return FString();
	}

	FWLArmy Army;
	Army.Id = FString::Printf(TEXT("A%d"), NextArmyNumber++);
	Army.OwnerIso = OwnerIso;
	Army.ProvinceId = ProvinceId;
	Army.General = General.IsEmpty() ? TEXT("Comandante") : General;

	const int32 N = FMath::Max(1, Count);
	for (int32 i = 0; i < N; ++i)
	{
		Army.Units.Add(UnitId);
	}

	Armies.Add(Army);
	UE_LOG(LogWorldLeader, Log, TEXT("Ejercito %s creado: %s x%d en %s (%s)"),
		*Army.Id, *Unit.Name, N, *Province.Name, *OwnerIso);
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

	FWLProvinceData From;
	if (!Reg->GetProvince(Army->ProvinceId, From))
	{
		OutMessage = TEXT("Provincia de origen invalida.");
		return false;
	}

	if (!From.Neighbors.Contains(ToProvinceId))
	{
		OutMessage = FString::Printf(TEXT("%s no es adyacente a %s."), *ToProvinceId, *Army->ProvinceId);
		return false;
	}

	Army->ProvinceId = ToProvinceId;
	OutMessage = FString::Printf(TEXT("%s se movio a %s."), *ArmyId, *ToProvinceId);
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
		return EWLBattleResult::Stalemate;
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

	OutReport = FString::Printf(
		TEXT("Batalla en %s: %s (atk %d) vs %s (def %d, terreno x%.2f) -> %s. Quedan: %s=%d, %s=%d."),
		*DefenderProvince, *AttackerId, AttackPower, *DefenderId, DefensePower, TerrainMult,
		*UWLMilitaryLibrary::BattleResultToString(Result),
		*AttackerId, Attacker->Units.Num(), *DefenderId, Defender->Units.Num());

	// Ocupacion: si el atacante gana y aniquila al defensor, ocupa la provincia.
	if (bAttackerWins && bDefenderDestroyed)
	{
		Attacker->ProvinceId = DefenderProvince;
		OutReport += FString::Printf(TEXT(" %s ocupa %s."), *AttackerId, *DefenderProvince);
	}

	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutReport);

	// Eliminar ejercitos aniquilados (esto invalida los punteros anteriores).
	Armies.RemoveAll([](const FWLArmy& A) { return A.Units.Num() == 0; });

	return Result;
}
