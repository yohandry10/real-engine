// Copyright World Leader project. See ROADMAP.md.

#include "Military/WLMilitarySubsystem.h"
#include "Battle/WLTacticalBattleSubsystem.h"
#include "Military/WLMilitaryLibrary.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

namespace
{
	FString NormalizeArmyId(const FString& In)
	{
		return In.TrimStartAndEnd().ToUpper();
	}

	FString NormalizeMilitaryIso(const FString& In)
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

bool UWLMilitarySubsystem::ValidateBattlePair(const FWLArmy& Attacker, const FWLArmy& Defender, FString& OutMessage) const
{
	const UWLDataRegistry* Reg = GetRegistry();
	if (!Reg)
	{
		OutMessage = TEXT("Registro de datos no disponible.");
		return false;
	}
	if (Attacker.Id == Defender.Id)
	{
		OutMessage = TEXT("Un ejercito no puede atacarse a si mismo.");
		return false;
	}
	if (Attacker.OwnerIso == Defender.OwnerIso)
	{
		OutMessage = FString::Printf(TEXT("%s y %s pertenecen a la misma nacion (%s)."),
			*Attacker.Id, *Defender.Id, *Attacker.OwnerIso);
		return false;
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWLPoliticalSubsystem* Politics = GI->GetSubsystem<UWLPoliticalSubsystem>())
		{
			FWLDiplomaticRelationState Relation;
			if (Politics->GetRelation(Attacker.OwnerIso, Defender.OwnerIso, Relation)
				&& Relation.Status != EWLDiplomaticStatus::War)
			{
				OutMessage = FString::Printf(TEXT("%s y %s no estan en guerra."),
					*Attacker.OwnerIso, *Defender.OwnerIso);
				return false;
			}
		}
	}

	FWLProvinceData AttackerProvince;
	if (!Reg->GetProvince(Attacker.ProvinceId, AttackerProvince))
	{
		OutMessage = FString::Printf(TEXT("Provincia del atacante invalida: %s"), *Attacker.ProvinceId);
		return false;
	}
	FWLProvinceData DefenderProvince;
	if (!Reg->GetProvince(Defender.ProvinceId, DefenderProvince))
	{
		OutMessage = FString::Printf(TEXT("Provincia del defensor invalida: %s"), *Defender.ProvinceId);
		return false;
	}
	if (Attacker.ProvinceId != Defender.ProvinceId && !AttackerProvince.Neighbors.Contains(Defender.ProvinceId))
	{
		OutMessage = FString::Printf(TEXT("%s no puede atacar %s desde %s hasta %s: no hay adyacencia."),
			*Attacker.Id, *Defender.Id, *Attacker.ProvinceId, *Defender.ProvinceId);
		return false;
	}
	return true;
}

void UWLMilitarySubsystem::ReconcileArmyFromTacticalBattle(
	FWLArmy& Army,
	const FWLTacticalBattleState& Battle,
	int32& OutDestroyedLosses,
	int32& OutRoutedUnits) const
{
	const FWLBalanceRules Rules = GetStrategicTick() ? GetStrategicTick()->GetBalanceRules() : FWLBalanceRules::Default();
	TArray<FString> EffectiveUnits;
	TArray<FString> RoutedUnits;
	OutDestroyedLosses = 0;
	OutRoutedUnits = 0;

	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (!Unit.SourceArmyId.Equals(Army.Id, ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (Unit.bDestroyed || Unit.Health <= 0.0)
		{
			++OutDestroyedLosses;
		}
		else if (Unit.IsCombatEffective(Rules.TacticalRoutMoraleThreshold))
		{
			EffectiveUnits.Add(Unit.UnitId);
		}
		else
		{
			RoutedUnits.Add(Unit.UnitId);
		}
	}

	OutRoutedUnits = RoutedUnits.Num();
	Army.Units = MoveTemp(EffectiveUnits);
	Army.RecoveringUnits.Append(MoveTemp(RoutedUnits));
}

bool UWLMilitarySubsystem::FindRetreatProvinceForArmy(
	const FWLArmy& Army,
	const FString& FromProvinceId,
	FString& OutRetreatProvinceId) const
{
	const UWLDataRegistry* Reg = GetRegistry();
	if (!Reg)
	{
		return false;
	}

	FWLProvinceData FromProvince;
	if (!Reg->GetProvince(FromProvinceId, FromProvince))
	{
		return false;
	}

	TArray<FString> Candidates = FromProvince.Neighbors;
	Candidates.Sort();
	for (const FString& CandidateId : Candidates)
	{
		FWLProvinceData Candidate;
		if (!Reg->GetProvince(CandidateId, Candidate))
		{
			continue;
		}

		const FString ControllerIso = GetStrategicTick()
			? GetStrategicTick()->GetProvinceControllerIso(Candidate.Id)
			: Candidate.CountryIso;
		if (ControllerIso == Army.OwnerIso)
		{
			OutRetreatProvinceId = Candidate.Id;
			return true;
		}
	}

	return false;
}

void UWLMilitarySubsystem::AwardBattleRenown(const FWLArmy& Attacker, const FWLArmy& Defender, bool bAttackerWins)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWLCharacterSubsystem* Characters = GI->GetSubsystem<UWLCharacterSubsystem>())
		{
			FWLCharacter AttackerGeneral;
			if (Characters->GetAssignedGeneralForArmy(Attacker.Id, AttackerGeneral))
			{
				FString RenownMessage;
				Characters->AddRenownToGeneral(AttackerGeneral.Id, bAttackerWins ? 8 : 4, RenownMessage);
			}

			FWLCharacter DefenderGeneral;
			if (Characters->GetAssignedGeneralForArmy(Defender.Id, DefenderGeneral))
			{
				FString RenownMessage;
				Characters->AddRenownToGeneral(DefenderGeneral.Id, bAttackerWins ? 4 : 8, RenownMessage);
			}
		}
	}
}

FString UWLMilitarySubsystem::CreateArmy(const FString& OwnerIso, const FString& ProvinceId, const FString& UnitId, int32 Count, const FString& General)
{
	const UWLDataRegistry* Reg = GetRegistry();
	if (!Reg) return FString();

	const FString NormalizedOwner = NormalizeMilitaryIso(OwnerIso);
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
	if (General.IsEmpty())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UWLCharacterSubsystem* Characters = GI->GetSubsystem<UWLCharacterSubsystem>())
			{
				FWLCharacter GeneratedGeneral;
				FString AssignMessage;
				Characters->CreateAndAssignGeneralToArmy(Owner.Iso, Army.Id, GeneratedGeneral, AssignMessage);
			}
		}
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Ejercito %s creado: %s x%d en %s (%s)"),
		*Army.Id, *Unit.Name, Count, *Province.Name, *Army.OwnerIso);
	return Army.Id;
}

FString UWLMilitarySubsystem::FindArmyIdByBase(const FString& BaseId) const
{
	const FString Normalized = BaseId.TrimStartAndEnd().ToUpper();
	for (const FWLArmy& Army : Armies)
	{
		if (Army.SourceBaseId.Equals(Normalized, ESearchCase::IgnoreCase))
		{
			return Army.Id;
		}
	}
	return FString();
}

// Tipos del catalogo de reclutamiento (RecruitableUnits.json) -> unidades con stats (Units.json).
static FString MapRecruitTypeToUnitId(const FString& RecruitType)
{
	const FString T = RecruitType.TrimStartAndEnd().ToLower();
	if (T == TEXT("mbt") || T == TEXT("ifv") || T == TEXT("apc"))       return TEXT("tank");
	if (T == TEXT("artillery"))                                          return TEXT("artillery");
	if (T == TEXT("heli") || T == TEXT("aircraft") || T == TEXT("ship")) return TEXT("drone");
	return TEXT("infantry");
}

FString UWLMilitarySubsystem::SyncArmyFromGarrison(
	const FString& BaseId,
	const FString& OwnerIso,
	const FString& ProvinceId,
	const TArray<TPair<FString, int32>>& GarrisonUnits)
{
	const UWLDataRegistry* Reg = GetRegistry();
	if (!Reg || BaseId.TrimStartAndEnd().IsEmpty())
	{
		return FString();
	}

	// Composicion real: cada lote reclutado se traduce a unidades con stats.
	TArray<FString> Units;
	for (const TPair<FString, int32>& Group : GarrisonUnits)
	{
		const FString UnitId = MapRecruitTypeToUnitId(Group.Key);
		FWLUnitData Unit;
		if (!Reg->GetUnit(UnitId, Unit))
		{
			continue;
		}
		for (int32 i = 0; i < FMath::Max(0, Group.Value); ++i)
		{
			Units.Add(Unit.Id);
		}
	}
	if (Units.IsEmpty())
	{
		return FString();
	}

	const FString NormalizedBase = BaseId.TrimStartAndEnd().ToUpper();
	if (FWLArmy* Existing = FindArmy(FindArmyIdByBase(NormalizedBase)))
	{
		Existing->Units = MoveTemp(Units);   // la guarnicion crecio: el ejercito real refleja la nueva tropa
		Existing->RecoveringUnits.Reset();
		return Existing->Id;
	}

	FWLNationData Owner;
	if (!Reg->GetNation(OwnerIso, Owner))
	{
		return FString();
	}
	FWLProvinceData Province;
	if (!Reg->GetProvince(ProvinceId, Province))
	{
		// Sin provincia valida (nodo de carretera sin mapear): usa la capital como asiento fiscal/legal.
		if (!Reg->GetProvince(Owner.CapitalProvinceId, Province))
		{
			return FString();
		}
	}

	FWLArmy Army;
	Army.Id = FString::Printf(TEXT("A%d"), NextArmyNumber++);
	Army.OwnerIso = Owner.Iso;
	Army.ProvinceId = Province.Id;
	Army.General = TEXT("Comandante");
	Army.SourceBaseId = NormalizedBase;
	Army.Units = MoveTemp(Units);
	Armies.Add(Army);

	// F1.4: el ejercito recien desplegado recibe su general nombrado.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWLCharacterSubsystem* Characters = GI->GetSubsystem<UWLCharacterSubsystem>())
		{
			FWLCharacter GeneratedGeneral;
			FString AssignMessage;
			Characters->CreateAndAssignGeneralToArmy(Owner.Iso, Army.Id, GeneratedGeneral, AssignMessage);
		}
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Ejercito %s desplegado desde %s (%d unidades)."),
		*Army.Id, *NormalizedBase, Army.Units.Num());
	return Army.Id;
}

bool UWLMilitarySubsystem::SetArmyProvince(const FString& ArmyId, const FString& ProvinceId, FString& OutMessage)
{
	const UWLDataRegistry* Reg = GetRegistry();
	FWLArmy* Army = FindArmy(ArmyId);
	FWLProvinceData Province;
	if (!Reg || !Army || !Reg->GetProvince(ProvinceId, Province))
	{
		OutMessage = TEXT("Ejercito o provincia invalidos para sincronizar posicion.");
		return false;
	}
	Army->ProvinceId = Province.Id;
	OutMessage = FString::Printf(TEXT("%s ahora opera en %s."), *Army->Id, *Province.Id);
	return true;
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

bool UWLMilitarySubsystem::SetArmyGeneral(const FString& ArmyId, const FString& GeneralName, FString& OutMessage)
{
	FWLArmy* Army = FindArmy(ArmyId);
	if (!Army)
	{
		OutMessage = FString::Printf(TEXT("Ejercito desconocido: %s"), *ArmyId);
		return false;
	}
	Army->General = GeneralName.TrimStartAndEnd().IsEmpty() ? TEXT("Comandante") : GeneralName.TrimStartAndEnd();
	OutMessage = FString::Printf(TEXT("%s ahora esta bajo %s."), *Army->Id, *Army->General);
	return true;
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

void UWLMilitarySubsystem::ComputeBattlePower(const FWLArmy& Attacker, const FWLArmy& Defender, FWLBattlePreview& OutPreview) const
{
	const UWLDataRegistry* Reg = GetRegistry();
	OutPreview.AttackerArmyId = Attacker.Id;
	OutPreview.DefenderArmyId = Defender.Id;
	OutPreview.DefenderIso = Defender.OwnerIso;
	OutPreview.AttackerUnits = Attacker.Units.Num();
	OutPreview.DefenderUnits = Defender.Units.Num();
	OutPreview.AttackerBaseAttack = SumUnitStat(Attacker, true);
	OutPreview.DefenderBaseDefense = SumUnitStat(Defender, false);

	// Bonus de terreno para el defensor.
	float TerrainMult = 1.0f;
	FWLProvinceData DefProvince;
	if (Reg && Reg->GetProvince(Defender.ProvinceId, DefProvince))
	{
		TerrainMult = UWLMilitaryLibrary::TerrainDefenseMultiplier(DefProvince.Terrain);
		switch (DefProvince.Terrain)
		{
		case EWLTerrainType::Mountain: OutPreview.TerrainLabel = TEXT("montana"); break;
		case EWLTerrainType::Desert:   OutPreview.TerrainLabel = TEXT("desierto"); break;
		case EWLTerrainType::Jungle:   OutPreview.TerrainLabel = TEXT("selva"); break;
		case EWLTerrainType::Coastal:  OutPreview.TerrainLabel = TEXT("costa"); break;
		case EWLTerrainType::Urban:    OutPreview.TerrainLabel = TEXT("urbana"); break;
		case EWLTerrainType::Arctic:   OutPreview.TerrainLabel = TEXT("artica"); break;
		case EWLTerrainType::Maritime: OutPreview.TerrainLabel = TEXT("maritima"); break;
		default:                       OutPreview.TerrainLabel = TEXT("llano"); break;
		}
	}
	float BuildingDefenseMult = 1.0f;
	FWLBalanceRules Rules = FWLBalanceRules::Default();
	if (const UWLStrategicTickSubsystem* Tick = GetStrategicTick())
	{
		Rules = Tick->GetBalanceRules();
		const FWLProvinceBuildingEffects Effects = Tick->GetProvinceBuildingEffects(Defender.ProvinceId);
		BuildingDefenseMult += static_cast<float>(FMath::Max(0, Effects.BonusDefense)) / 100.0f;
	}
	OutPreview.TerrainMultiplier = TerrainMult;
	OutPreview.DefenderBuildingMultiplier = BuildingDefenseMult;

	// El skill del general pesa en el combate segun balance: skill 50 = neutro.
	float AttackerSkillMult = 1.0f;
	float DefenderSkillMult = 1.0f;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWLCharacterSubsystem* Characters = GI->GetSubsystem<UWLCharacterSubsystem>())
		{
			FWLCharacter AttackerGeneral;
			FWLCharacter DefenderGeneral;
			if (Characters->GetAssignedGeneralForArmy(Attacker.Id, AttackerGeneral))
			{
				AttackerSkillMult = UWLMilitaryLibrary::GeneralSkillCombatMultiplier(AttackerGeneral.Skill, Rules);
				OutPreview.AttackerGeneral = FString::Printf(TEXT("%s (skill %d)"), *AttackerGeneral.Name, AttackerGeneral.Skill);
			}
			if (Characters->GetAssignedGeneralForArmy(Defender.Id, DefenderGeneral))
			{
				DefenderSkillMult = UWLMilitaryLibrary::GeneralSkillCombatMultiplier(DefenderGeneral.Skill, Rules);
				OutPreview.DefenderGeneral = FString::Printf(TEXT("%s (skill %d)"), *DefenderGeneral.Name, DefenderGeneral.Skill);
			}
		}
	}

	OutPreview.AttackerPower = FMath::RoundToInt(OutPreview.AttackerBaseAttack * AttackerSkillMult);
	OutPreview.DefenderPower = FMath::RoundToInt(OutPreview.DefenderBaseDefense * TerrainMult * BuildingDefenseMult * DefenderSkillMult);

	const double Ratio = OutPreview.DefenderPower > 0
		? static_cast<double>(OutPreview.AttackerPower) / static_cast<double>(OutPreview.DefenderPower)
		: 2.0;
	OutPreview.OddsLabel = Ratio >= 1.25 ? TEXT("Favorable") : (Ratio <= 0.8 ? TEXT("Desfavorable") : TEXT("Parejo"));
}

bool UWLMilitarySubsystem::PreviewBattle(const FString& AttackerId, const FString& DefenderId, FWLBattlePreview& OutPreview) const
{
	OutPreview = FWLBattlePreview();
	const FWLArmy* Attacker = FindArmy(AttackerId);
	const FWLArmy* Defender = FindArmy(DefenderId);
	if (!Attacker || !Defender)
	{
		OutPreview.Reason = TEXT("Ejercito atacante o defensor no encontrado.");
		return false;
	}
	FString ValidateMessage;
	OutPreview.bValid = ValidateBattlePair(*Attacker, *Defender, ValidateMessage);
	OutPreview.Reason = ValidateMessage;
	ComputeBattlePower(*Attacker, *Defender, OutPreview);
	return OutPreview.bValid;
}

TArray<FString> UWLMilitarySubsystem::GetAttackableTargetIds(const FString& AttackerId) const
{
	TArray<FString> Targets;
	const FWLArmy* Attacker = FindArmy(AttackerId);
	if (!Attacker || Attacker->Units.Num() == 0)
	{
		return Targets;
	}
	for (const FWLArmy& Other : Armies)
	{
		if (Other.Id == Attacker->Id || Other.Units.Num() == 0)
		{
			continue;
		}
		FString ValidateMessage;
		if (ValidateBattlePair(*Attacker, Other, ValidateMessage))
		{
			Targets.Add(Other.Id);
		}
	}
	return Targets;
}

EWLBattleResult UWLMilitarySubsystem::ResolveTacticalBattleToEnd(const FString& AttackerId, const FString& DefenderId, FString& OutReport)
{
	FWLTacticalBattleState Battle;
	FString Message;
	if (!StartTacticalBattle(AttackerId, DefenderId, Battle, Message))
	{
		OutReport = Message;
		return EWLBattleResult::Invalid;
	}

	UWLTacticalBattleSubsystem* Tactical = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Tactical = GI->GetSubsystem<UWLTacticalBattleSubsystem>();
	}
	if (!Tactical)
	{
		OutReport = TEXT("Subsystem tactico no disponible.");
		return EWLBattleResult::Invalid;
	}

	// IA en ambos bandos: sin vista 3D interactiva, la batalla se juega sola de forma determinista.
	Tactical->SetTacticalAIControl(Battle.BattleId, Battle.AttackerIso, true, Message);
	Tactical->SetTacticalAIControl(Battle.BattleId, Battle.DefenderIso, true, Message);

	// Avanza en pasos fijos hasta que la batalla resuelve (tope de seguridad para no colgar).
	TArray<FString> Events;
	constexpr int32 MaxSteps = 600;   // ~10 min de batalla a 1s/paso
	for (int32 Step = 0; Step < MaxSteps; ++Step)
	{
		TArray<FString> StepEvents;
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, StepEvents);
		Events.Append(StepEvents);
		if (!Battle.bActive && Battle.Result != EWLTacticalBattleResult::Ongoing)
		{
			break;
		}
	}

	FString ApplyMessage;
	ApplyTacticalBattleResult(Battle.BattleId, ApplyMessage);

	// Reporte: ultimos hitos + resumen de aplicacion.
	FString EventTail;
	const int32 TailStart = FMath::Max(0, Events.Num() - 4);
	for (int32 i = TailStart; i < Events.Num(); ++i)
	{
		EventTail += (EventTail.IsEmpty() ? TEXT("") : TEXT(" · ")) + Events[i];
	}
	OutReport = FString::Printf(TEXT("Batalla tactica %s. %s%s"),
		*Battle.BattleId, *ApplyMessage,
		EventTail.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  [%s]"), *EventTail));

	switch (Battle.Result)
	{
	case EWLTacticalBattleResult::AttackerVictory: return EWLBattleResult::AttackerVictory;
	case EWLTacticalBattleResult::DefenderVictory: return EWLBattleResult::DefenderVictory;
	default:                                       return EWLBattleResult::Stalemate;
	}
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
	if (!ValidateBattlePair(*Attacker, *Defender, OutReport))
	{
		return EWLBattleResult::Invalid;
	}

	// Poderes desde la fuente unica (terreno + edificios + skill del general).
	FWLBattlePreview Preview;
	ComputeBattlePower(*Attacker, *Defender, Preview);
	const int32 AttackPower = Preview.AttackerPower;
	const int32 DefensePower = Preview.DefenderPower;
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
		TEXT("Batalla en %s: %s (atk %d, gen. %s) vs %s (def %d, terreno x%.2f, defensas x%.2f, gen. %s) -> %s. Quedan: %s=%d, %s=%d."),
		*DefenderProvince, *AttackerId, AttackPower,
		Preview.AttackerGeneral.IsEmpty() ? TEXT("sin mando") : *Preview.AttackerGeneral,
		*DefenderId, DefensePower, Preview.TerrainMultiplier, Preview.DefenderBuildingMultiplier,
		Preview.DefenderGeneral.IsEmpty() ? TEXT("sin mando") : *Preview.DefenderGeneral,
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

	// Renombre a los generales de ambos bandos (misma regla que la via tactica).
	AwardBattleRenown(*Attacker, *Defender, bAttackerWins);

	// Eliminar ejercitos aniquilados (esto invalida los punteros anteriores).
	Armies.RemoveAll([](const FWLArmy& A) { return A.Units.Num() == 0 && A.RecoveringUnits.Num() == 0; });

	return Result;
}

bool UWLMilitarySubsystem::StartTacticalBattle(
	const FString& AttackerId,
	const FString& DefenderId,
	FWLTacticalBattleState& OutBattle,
	FString& OutMessage)
{
	const UWLDataRegistry* Reg = GetRegistry();
	const FWLArmy* Attacker = FindArmy(AttackerId);
	const FWLArmy* Defender = FindArmy(DefenderId);
	if (!Reg || !Attacker || !Defender)
	{
		OutMessage = TEXT("Ejercito atacante o defensor no encontrado.");
		return false;
	}
	if (!ValidateBattlePair(*Attacker, *Defender, OutMessage))
	{
		return false;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWLTacticalBattleSubsystem* Tactical = GI->GetSubsystem<UWLTacticalBattleSubsystem>())
		{
			return Tactical->StartTacticalBattleFromArmies(*Attacker, *Defender, Defender->ProvinceId, OutBattle, OutMessage);
		}
	}

	OutMessage = TEXT("Subsystem tactico no disponible.");
	return false;
}

bool UWLMilitarySubsystem::ApplyTacticalBattleResult(const FString& BattleId, FString& OutMessage)
{
	UWLTacticalBattleSubsystem* Tactical = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Tactical = GI->GetSubsystem<UWLTacticalBattleSubsystem>();
	}
	if (!Tactical)
	{
		OutMessage = TEXT("Subsystem tactico no disponible.");
		return false;
	}

	FWLTacticalBattleState Battle;
	if (!Tactical->GetTacticalBattleState(BattleId, Battle))
	{
		OutMessage = FString::Printf(TEXT("Batalla tactica desconocida: %s"), *BattleId);
		return false;
	}
	if (Battle.bActive || Battle.Result == EWLTacticalBattleResult::Ongoing)
	{
		OutMessage = FString::Printf(TEXT("Batalla tactica aun activa: %s"), *Battle.BattleId);
		return false;
	}
	if (Battle.AttackerArmyId.IsEmpty() || Battle.DefenderArmyId.IsEmpty())
	{
		OutMessage = TEXT("Batalla tactica sin enlace a ejercitos de campania.");
		return false;
	}

	FWLArmy* Attacker = FindArmy(Battle.AttackerArmyId);
	FWLArmy* Defender = FindArmy(Battle.DefenderArmyId);
	if (!Attacker || !Defender)
	{
		OutMessage = TEXT("No se pudo aplicar resultado tactico: ejercito de campania ausente.");
		return false;
	}

	const FString AttackerArmyId = Attacker->Id;
	const FString DefenderArmyId = Defender->Id;
	const FString DefenderProvince = Defender->ProvinceId;
	const int32 AttackerBefore = Attacker->Units.Num();
	const int32 DefenderBefore = Defender->Units.Num();
	int32 AttackerDestroyedLosses = 0;
	int32 AttackerRoutedUnits = 0;
	int32 DefenderDestroyedLosses = 0;
	int32 DefenderRoutedUnits = 0;
	ReconcileArmyFromTacticalBattle(*Attacker, Battle, AttackerDestroyedLosses, AttackerRoutedUnits);
	ReconcileArmyFromTacticalBattle(*Defender, Battle, DefenderDestroyedLosses, DefenderRoutedUnits);

	const bool bAttackerWins = Battle.Result == EWLTacticalBattleResult::AttackerVictory;
	const bool bDefenderWins = Battle.Result == EWLTacticalBattleResult::DefenderVictory;
	const bool bAttackerCombatEffective = !Attacker->Units.IsEmpty();
	const bool bDefenderCombatEffective = !Defender->Units.IsEmpty();
	const bool bEnemyStillPresent = Armies.ContainsByPredicate(
		[&DefenderProvince, &AttackerArmyId, &DefenderArmyId, &Attacker](const FWLArmy& Army)
		{
			return Army.Id != AttackerArmyId
				&& Army.Id != DefenderArmyId
				&& Army.ProvinceId == DefenderProvince
				&& Army.OwnerIso != Attacker->OwnerIso
				&& Army.Units.Num() > 0;
		});

	OutMessage = FString::Printf(
		TEXT("Resultado tactico %s en %s: %s destruidas %d, retiradas %d/%d; %s destruidas %d, retiradas %d/%d."),
		*Battle.BattleId,
		*DefenderProvince,
		*AttackerArmyId,
		AttackerDestroyedLosses,
		AttackerRoutedUnits,
		AttackerBefore,
		*DefenderArmyId,
		DefenderDestroyedLosses,
		DefenderRoutedUnits,
		DefenderBefore);

	if (bAttackerWins && !bDefenderCombatEffective && Defender->RecoveringUnits.Num() > 0)
	{
		FString RetreatProvince;
		if (FindRetreatProvinceForArmy(*Defender, DefenderProvince, RetreatProvince))
		{
			Defender->ProvinceId = RetreatProvince;
			OutMessage += FString::Printf(TEXT(" %s se repliega a %s con %d unidades desorganizadas."),
				*DefenderArmyId, *RetreatProvince, Defender->RecoveringUnits.Num());
		}
		else
		{
			const int32 Captured = Defender->RecoveringUnits.Num();
			Defender->RecoveringUnits.Reset();
			OutMessage += FString::Printf(TEXT(" %s no tiene ruta de repliegue; %d unidades son capturadas/perdidas."),
				*DefenderArmyId, Captured);
		}
	}

	if (bDefenderWins && !bAttackerCombatEffective && Attacker->RecoveringUnits.Num() > 0 && Attacker->ProvinceId == DefenderProvince)
	{
		FString RetreatProvince;
		if (FindRetreatProvinceForArmy(*Attacker, DefenderProvince, RetreatProvince))
		{
			Attacker->ProvinceId = RetreatProvince;
			OutMessage += FString::Printf(TEXT(" %s se repliega a %s con %d unidades desorganizadas."),
				*AttackerArmyId, *RetreatProvince, Attacker->RecoveringUnits.Num());
		}
	}

	if (bAttackerWins && bAttackerCombatEffective && !bDefenderCombatEffective && !bEnemyStillPresent)
	{
		Attacker->ProvinceId = DefenderProvince;
		if (UWLStrategicTickSubsystem* Tick = GetStrategicTick())
		{
			FString OccupationMessage;
			Tick->SetProvinceController(DefenderProvince, Attacker->OwnerIso, OccupationMessage);
			OutMessage += FString::Printf(TEXT(" %s ocupa %s. %s"), *AttackerArmyId, *DefenderProvince, *OccupationMessage);
		}
		else
		{
			OutMessage += FString::Printf(TEXT(" %s ocupa %s."), *AttackerArmyId, *DefenderProvince);
		}
	}
	else if (bAttackerWins && bAttackerCombatEffective && !bDefenderCombatEffective && bEnemyStillPresent)
	{
		Attacker->ProvinceId = DefenderProvince;
		OutMessage += FString::Printf(TEXT(" %s entra en %s, pero quedan fuerzas enemigas."), *AttackerArmyId, *DefenderProvince);
	}
	else if (bAttackerWins && bAttackerCombatEffective)
	{
		OutMessage += TEXT(" Victoria tactica atacante sin ocupacion: quedan defensores efectivos.");
	}
	else if (bDefenderWins)
	{
		OutMessage += TEXT(" Victoria tactica defensora: no cambia el control provincial.");
	}
	else
	{
		OutMessage += TEXT(" Empate tactico: no cambia el control provincial.");
	}

	if (bAttackerWins || bDefenderWins)
	{
		AwardBattleRenown(*Attacker, *Defender, bAttackerWins);
	}

	Armies.RemoveAll([](const FWLArmy& A) { return A.Units.Num() == 0 && A.RecoveringUnits.Num() == 0; });

	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutMessage);
	return true;
}

bool UWLMilitarySubsystem::ReorganizeArmy(const FString& ArmyId, int32 MaxUnits, FString& OutMessage)
{
	FWLArmy* Army = FindArmy(ArmyId);
	if (!Army)
	{
		OutMessage = FString::Printf(TEXT("Ejercito desconocido: %s"), *ArmyId);
		return false;
	}
	if (Army->RecoveringUnits.IsEmpty())
	{
		OutMessage = FString::Printf(TEXT("%s no tiene unidades desorganizadas."), *Army->Id);
		return false;
	}

	const int32 UnitsToRecover = MaxUnits <= 0
		? Army->RecoveringUnits.Num()
		: FMath::Min(MaxUnits, Army->RecoveringUnits.Num());
	for (int32 Index = 0; Index < UnitsToRecover; ++Index)
	{
		Army->Units.Add(Army->RecoveringUnits[0]);
		Army->RecoveringUnits.RemoveAt(0);
	}

	OutMessage = FString::Printf(TEXT("%s reorganiza %d unidades. Efectivas=%d, desorganizadas=%d."),
		*Army->Id, UnitsToRecover, Army->Units.Num(), Army->RecoveringUnits.Num());
	return true;
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
		Army.SourceBaseId = SavedArmy.SourceBaseId.TrimStartAndEnd().ToUpper();

		for (const FString& UnitId : SavedArmy.Units)
		{
			FWLUnitData Unit;
			if (Reg->GetUnit(UnitId, Unit))
			{
				Army.Units.Add(Unit.Id);
			}
		}
		for (const FString& UnitId : SavedArmy.RecoveringUnits)
		{
			FWLUnitData Unit;
			if (Reg->GetUnit(UnitId, Unit))
			{
				Army.RecoveringUnits.Add(Unit.Id);
			}
		}

		if (!Army.Id.IsEmpty() && (!Army.Units.IsEmpty() || !Army.RecoveringUnits.IsEmpty()) && !Armies.ContainsByPredicate(
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
