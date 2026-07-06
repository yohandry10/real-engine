// Copyright World Leader project. See ROADMAP.md.

#include "Battle/WLTacticalBattleSubsystem.h"

#include "Balance/WLBalanceSubsystem.h"
#include "Campaign/WLDataRegistry.h"
#include "Engine/GameInstance.h"

namespace
{
	FString NormalizeBattleId(const FString& In)
	{
		return In.TrimStartAndEnd().ToUpper();
	}

	FString NormalizeTacticalIso(const FString& In)
	{
		return In.TrimStartAndEnd().ToUpper();
	}

	FString NormalizeTacticalDataId(const FString& In)
	{
		return In.TrimStartAndEnd().ToLower();
	}

	void MoveUnitToward(FWLTacticalUnitState& Unit, const FVector2D& Target, double Speed, double DeltaSeconds)
	{
		const FVector2D ToTarget = Target - Unit.Position;
		const double Distance = ToTarget.Size();
		if (Distance <= KINDA_SMALL_NUMBER)
		{
			Unit.Position = Target;
			return;
		}

		const double Step = FMath::Max(0.0, Speed * DeltaSeconds);
		if (Step >= Distance)
		{
			Unit.Position = Target;
			return;
		}
		Unit.Position += ToTarget / Distance * Step;
	}

	// --- F1: matriz de contras de armas combinadas (terreno abierto). Atacante -> defensor. ---
	// La "piedra-papel-tijera" moderna: el tanque revienta infanteria en abierto, la infanteria
	// apenas rasca blindaje, el SAM caza aviacion, la aviacion caza tanques, la artilleria
	// castiga objetivos blandos. Los matchups clave estan amarrados por tests de automation.
	double TacticalCounterMultiplier(EWLUnitType Attacker, EWLUnitType Defender)
	{
		// Fuerzas especiales combaten como infanteria; el drone legacy como vehiculo ligero fragil.
		auto Norm = [](EWLUnitType T)
		{
			if (T == EWLUnitType::SpecialForces) { return EWLUnitType::Infantry; }
			if (T == EWLUnitType::Drone)         { return EWLUnitType::LightVehicle; }
			return T;
		};
		const EWLUnitType A = Norm(Attacker);
		const EWLUnitType D = Norm(Defender);

		switch (A)
		{
		case EWLUnitType::Infantry:
			switch (D)
			{
			case EWLUnitType::Infantry:     return 1.0;
			case EWLUnitType::LightVehicle: return 0.8;
			case EWLUnitType::Armor:        return 0.3;   // en abierto el fusil no rasca al MBT
			case EWLUnitType::Artillery:    return 1.6;
			case EWLUnitType::AirDefense:   return 1.5;
			case EWLUnitType::Air:          return 0.2;
			case EWLUnitType::Naval:        return 0.2;
			default:                        return 1.0;
			}
		case EWLUnitType::LightVehicle:
			switch (D)
			{
			case EWLUnitType::Infantry:     return 1.6;
			case EWLUnitType::LightVehicle: return 1.0;
			case EWLUnitType::Armor:        return 0.4;
			case EWLUnitType::Artillery:    return 1.4;
			case EWLUnitType::AirDefense:   return 1.3;
			case EWLUnitType::Air:          return 0.3;
			case EWLUnitType::Naval:        return 0.2;
			default:                        return 1.0;
			}
		case EWLUnitType::Armor:
			switch (D)
			{
			case EWLUnitType::Infantry:     return 1.8;   // "caballeria vs arqueros" moderno
			case EWLUnitType::LightVehicle: return 1.6;
			case EWLUnitType::Armor:        return 1.0;
			case EWLUnitType::Artillery:    return 1.8;
			case EWLUnitType::AirDefense:   return 1.6;
			case EWLUnitType::Air:          return 0.2;
			case EWLUnitType::Naval:        return 0.3;
			default:                        return 1.0;
			}
		case EWLUnitType::Artillery:
			switch (D)
			{
			case EWLUnitType::Infantry:     return 1.8;
			case EWLUnitType::LightVehicle: return 1.2;
			case EWLUnitType::Armor:        return 0.8;
			case EWLUnitType::Artillery:    return 1.0;   // contrabateria
			case EWLUnitType::AirDefense:   return 1.4;
			case EWLUnitType::Air:          return 0.1;
			case EWLUnitType::Naval:        return 0.5;
			default:                        return 1.0;
			}
		case EWLUnitType::AirDefense:
			switch (D)
			{
			case EWLUnitType::Infantry:     return 0.2;
			case EWLUnitType::LightVehicle: return 0.2;
			case EWLUnitType::Armor:        return 0.1;
			case EWLUnitType::Artillery:    return 0.3;
			case EWLUnitType::AirDefense:   return 1.0;
			case EWLUnitType::Air:          return 2.2;   // el SAM caza aviacion
			case EWLUnitType::Naval:        return 0.1;
			default:                        return 1.0;
			}
		case EWLUnitType::Air:
			switch (D)
			{
			case EWLUnitType::Infantry:     return 1.2;
			case EWLUnitType::LightVehicle: return 1.5;
			case EWLUnitType::Armor:        return 1.9;   // helo/caza cazan blindados
			case EWLUnitType::Artillery:    return 1.6;
			case EWLUnitType::AirDefense:   return 0.35;  // entrar al paraguas SAM cuesta caro
			case EWLUnitType::Air:          return 1.0;
			case EWLUnitType::Naval:        return 1.0;
			default:                        return 1.0;
			}
		case EWLUnitType::Naval:
			switch (D)
			{
			case EWLUnitType::Infantry:     return 0.8;
			case EWLUnitType::LightVehicle: return 0.8;
			case EWLUnitType::Armor:        return 0.6;
			case EWLUnitType::Artillery:    return 0.8;
			case EWLUnitType::AirDefense:   return 0.6;
			case EWLUnitType::Air:          return 0.4;
			case EWLUnitType::Naval:        return 1.0;
			default:                        return 1.0;
			}
		default:
			return 1.0;
		}
	}

	// El dano usa el canal correcto: HARD attack contra blindaje, SOFT contra lo demas.
	bool IsArmoredTarget(EWLUnitType Type)
	{
		return Type == EWLUnitType::Armor
			|| Type == EWLUnitType::LightVehicle
			|| Type == EWLUnitType::Naval;
	}
}

UWLDataRegistry* UWLTacticalBattleSubsystem::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

FWLBalanceRules UWLTacticalBattleSubsystem::GetBalanceRules() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWLBalanceSubsystem* Balance = GI->GetSubsystem<UWLBalanceSubsystem>())
		{
			return Balance->GetRules();
		}
	}
	return FWLBalanceRules::Default();
}

void UWLTacticalBattleSubsystem::ResetTacticalBattles()
{
	Battles.Reset();
	NextBattleNumber = 1;
}

FWLTacticalBattleState* UWLTacticalBattleSubsystem::FindBattle(const FString& BattleId)
{
	return Battles.Find(NormalizeBattleId(BattleId));
}

const FWLTacticalBattleState* UWLTacticalBattleSubsystem::FindBattle(const FString& BattleId) const
{
	return Battles.Find(NormalizeBattleId(BattleId));
}

FWLTacticalUnitState* UWLTacticalBattleSubsystem::FindUnit(FWLTacticalBattleState& Battle, const FString& TacticalUnitId)
{
	const FString UnitId = NormalizeBattleId(TacticalUnitId);
	return Battle.Units.FindByPredicate([&UnitId](const FWLTacticalUnitState& Unit)
	{
		return Unit.TacticalUnitId == UnitId;
	});
}

const FWLTacticalUnitState* UWLTacticalBattleSubsystem::FindUnit(const FWLTacticalBattleState& Battle, const FString& TacticalUnitId) const
{
	const FString UnitId = NormalizeBattleId(TacticalUnitId);
	return Battle.Units.FindByPredicate([&UnitId](const FWLTacticalUnitState& Unit)
	{
		return Unit.TacticalUnitId == UnitId;
	});
}

void UWLTacticalBattleSubsystem::AddArmyUnits(
	FWLTacticalBattleState& Battle,
	const FWLArmy& Army,
	const FVector2D& Origin,
	double DirectionSign)
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}

	const FString OwnerIso = NormalizeTacticalIso(Army.OwnerIso);

	// F1: CONTINGENTES. Las entradas identicas del ejercito (50 x "infantry") se agrupan en UNA
	// unidad tactica con ElementCount, en orden de primera aparicion (determinista). Es el modelo
	// Total War: se comandan grupos, no individuos.
	TArray<TPair<FString, int32>> Contingents;
	for (const FString& RawUnitId : Army.Units)
	{
		const FString UnitId = NormalizeTacticalDataId(RawUnitId);
		bool bFound = false;
		for (TPair<FString, int32>& Existing : Contingents)
		{
			if (Existing.Key == UnitId)
			{
				++Existing.Value;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Contingents.Add(TPair<FString, int32>(UnitId, 1));
		}
	}

	const int32 StartIndex = Battle.Units.Num();
	int32 ContingentIndex = 0;
	for (const TPair<FString, int32>& Contingent : Contingents)
	{
		FWLUnitData UnitData;
		if (!Registry->GetUnit(Contingent.Key, UnitData))
		{
			continue;
		}

		FWLTacticalUnitState Unit;
		Unit.TacticalUnitId = FString::Printf(TEXT("%s-%s-%03d"), *Battle.BattleId, *OwnerIso, StartIndex + ContingentIndex + 1);
		Unit.SourceArmyId = Army.Id;
		Unit.OwnerIso = OwnerIso;
		Unit.UnitId = UnitData.Id;
		Unit.DisplayName = FString::Printf(TEXT("%s x%d"), *UnitData.Name, Contingent.Value);
		Unit.ElementCount = Contingent.Value;
		Unit.InitialElementCount = Contingent.Value;
		Unit.Position = Origin + FVector2D(0.0, static_cast<double>(ContingentIndex) * 420.0 * DirectionSign);
		Unit.MoveTarget = Unit.Position;
		Battle.Units.Add(MoveTemp(Unit));
		++ContingentIndex;
	}
}

bool UWLTacticalBattleSubsystem::StartTacticalBattleFromArmies(
	const FWLArmy& Attacker,
	const FWLArmy& Defender,
	const FString& ProvinceId,
	FWLTacticalBattleState& OutBattle,
	FString& OutMessage)
{
	if (Attacker.OwnerIso.IsEmpty() || Defender.OwnerIso.IsEmpty() || Attacker.OwnerIso == Defender.OwnerIso)
	{
		OutMessage = TEXT("Batalla tactica invalida: paises iguales o vacios.");
		return false;
	}
	if (Attacker.Units.IsEmpty() || Defender.Units.IsEmpty())
	{
		OutMessage = TEXT("Batalla tactica invalida: ejercito sin unidades.");
		return false;
	}
	if (!GetRegistry())
	{
		OutMessage = TEXT("Registro de datos no disponible para batalla tactica.");
		return false;
	}

	FWLTacticalBattleState Battle;
	Battle.BattleId = FString::Printf(TEXT("TB-%04d"), NextBattleNumber++);
	Battle.ProvinceId = ProvinceId.TrimStartAndEnd().ToUpper();
	Battle.AttackerArmyId = Attacker.Id;
	Battle.DefenderArmyId = Defender.Id;
	Battle.AttackerIso = NormalizeTacticalIso(Attacker.OwnerIso);
	Battle.DefenderIso = NormalizeTacticalIso(Defender.OwnerIso);
	Battle.bActive = true;

	AddArmyUnits(Battle, Attacker, FVector2D(-500.0, -260.0), 1.0);
	AddArmyUnits(Battle, Defender, FVector2D(500.0, 260.0), -1.0);

	FWLTacticalObjectiveState Objective;
	Objective.ObjectiveId = FString::Printf(TEXT("%s-OBJ-1"), *Battle.BattleId);
	Objective.Position = FVector2D::ZeroVector;
	Battle.Objectives.Add(Objective);

	if (Battle.Units.Num() < 2)
	{
		OutMessage = TEXT("Batalla tactica invalida: no se pudieron crear unidades tacticas.");
		return false;
	}

	Battles.Add(Battle.BattleId, Battle);
	OutBattle = Battle;
	OutMessage = FString::Printf(TEXT("Batalla tactica %s iniciada: %s vs %s (%d unidades)."),
		*Battle.BattleId, *Battle.AttackerIso, *Battle.DefenderIso, Battle.Units.Num());
	return true;
}

bool UWLTacticalBattleSubsystem::GetTacticalBattleState(const FString& BattleId, FWLTacticalBattleState& OutBattle) const
{
	if (const FWLTacticalBattleState* Battle = FindBattle(BattleId))
	{
		OutBattle = *Battle;
		return true;
	}
	return false;
}

bool UWLTacticalBattleSubsystem::IssueMoveOrder(const FString& BattleId, const FString& TacticalUnitId, FVector2D Target, FString& OutMessage)
{
	FWLTacticalBattleState* Battle = FindBattle(BattleId);
	if (!Battle || !Battle->bActive)
	{
		OutMessage = FString::Printf(TEXT("Batalla tactica no activa: %s"), *BattleId);
		return false;
	}
	FWLTacticalUnitState* Unit = FindUnit(*Battle, TacticalUnitId);
	if (!Unit || Unit->bDestroyed)
	{
		OutMessage = FString::Printf(TEXT("Unidad tactica no disponible: %s"), *TacticalUnitId);
		return false;
	}

	Unit->MoveTarget = Target;
	Unit->AttackTargetUnitId.Reset();
	Unit->Order = EWLTacticalUnitOrder::Moving;
	OutMessage = FString::Printf(TEXT("%s mueve a %.0f, %.0f."), *Unit->TacticalUnitId, Target.X, Target.Y);
	return true;
}

bool UWLTacticalBattleSubsystem::IssueAttackOrder(const FString& BattleId, const FString& TacticalUnitId, const FString& TargetUnitId, FString& OutMessage)
{
	FWLTacticalBattleState* Battle = FindBattle(BattleId);
	if (!Battle || !Battle->bActive)
	{
		OutMessage = FString::Printf(TEXT("Batalla tactica no activa: %s"), *BattleId);
		return false;
	}
	FWLTacticalUnitState* Unit = FindUnit(*Battle, TacticalUnitId);
	const FWLTacticalUnitState* Target = FindUnit(*Battle, TargetUnitId);
	if (!Unit || Unit->bDestroyed || !Target || Target->bDestroyed || Unit->OwnerIso == Target->OwnerIso)
	{
		OutMessage = FString::Printf(TEXT("Orden de ataque tactica invalida: %s -> %s"), *TacticalUnitId, *TargetUnitId);
		return false;
	}

	Unit->AttackTargetUnitId = Target->TacticalUnitId;
	Unit->Order = EWLTacticalUnitOrder::Attacking;
	OutMessage = FString::Printf(TEXT("%s ataca a %s."), *Unit->TacticalUnitId, *Target->TacticalUnitId);
	return true;
}

bool UWLTacticalBattleSubsystem::SetTacticalAIControl(
	const FString& BattleId,
	const FString& OwnerIso,
	bool bEnabled,
	FString& OutMessage)
{
	FWLTacticalBattleState* Battle = FindBattle(BattleId);
	if (!Battle)
	{
		OutMessage = FString::Printf(TEXT("Batalla tactica desconocida: %s"), *BattleId);
		return false;
	}

	const FString NormalizedOwner = NormalizeTacticalIso(OwnerIso);
	if (NormalizedOwner.IsEmpty() || (NormalizedOwner != Battle->AttackerIso && NormalizedOwner != Battle->DefenderIso))
	{
		OutMessage = FString::Printf(TEXT("Pais tactico invalido para IA: %s"), *OwnerIso);
		return false;
	}

	Battle->AIControlledOwnerIsos.RemoveAll([&NormalizedOwner](const FString& Existing)
	{
		return Existing.Equals(NormalizedOwner, ESearchCase::IgnoreCase);
	});
	if (bEnabled)
	{
		Battle->AIControlledOwnerIsos.Add(NormalizedOwner);
	}

	OutMessage = FString::Printf(TEXT("IA tactica %s para %s en %s."),
		bEnabled ? TEXT("activada") : TEXT("desactivada"),
		*NormalizedOwner,
		*Battle->BattleId);
	return true;
}

bool UWLTacticalBattleSubsystem::AdvanceTacticalBattle(
	const FString& BattleId,
	double DeltaSeconds,
	FWLTacticalBattleState& OutBattle,
	TArray<FString>& OutEvents)
{
	OutEvents.Reset();
	FWLTacticalBattleState* Battle = FindBattle(BattleId);
	if (!Battle)
	{
		return false;
	}
	if (!Battle->bActive)
	{
		OutBattle = *Battle;
		return true;
	}

	const double StepSeconds = FMath::Clamp(DeltaSeconds, 0.0, 60.0);
	Battle->ElapsedSeconds += StepSeconds;
	IssueTacticalAIOrders(*Battle, OutEvents);
	AdvanceUnitOrders(*Battle, StepSeconds, OutEvents);
	AdvanceObjectives(*Battle, StepSeconds, OutEvents);
	UpdateBattleResult(*Battle, OutEvents);
	OutBattle = *Battle;
	return true;
}

bool UWLTacticalBattleSubsystem::IsValidAttackTarget(
	const FWLTacticalBattleState& Battle,
	const FWLTacticalUnitState& Unit,
	const FString& TargetUnitId,
	int32 RoutMoraleThreshold) const
{
	const FWLTacticalUnitState* Target = FindUnit(Battle, TargetUnitId);
	return Target
		&& Target->OwnerIso != Unit.OwnerIso
		&& Target->IsCombatEffective(RoutMoraleThreshold);
}

const FWLTacticalUnitState* UWLTacticalBattleSubsystem::FindNearestEffectiveEnemy(
	const FWLTacticalBattleState& Battle,
	const FWLTacticalUnitState& Unit,
	int32 RoutMoraleThreshold) const
{
	const FWLTacticalUnitState* BestTarget = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();
	for (const FWLTacticalUnitState& Candidate : Battle.Units)
	{
		if (Candidate.OwnerIso == Unit.OwnerIso || !Candidate.IsCombatEffective(RoutMoraleThreshold))
		{
			continue;
		}

		const double DistanceSq = FVector2D::DistSquared(Unit.Position, Candidate.Position);
		if (DistanceSq < BestDistanceSq
			|| (FMath::IsNearlyEqual(DistanceSq, BestDistanceSq) && BestTarget && Candidate.TacticalUnitId < BestTarget->TacticalUnitId))
		{
			BestDistanceSq = DistanceSq;
			BestTarget = &Candidate;
		}
	}
	return BestTarget;
}

const FWLTacticalObjectiveState* UWLTacticalBattleSubsystem::FindBestObjectiveForUnit(
	const FWLTacticalBattleState& Battle,
	const FWLTacticalUnitState& Unit) const
{
	const FWLTacticalObjectiveState* BestObjective = nullptr;
	double BestScore = TNumericLimits<double>::Max();
	for (const FWLTacticalObjectiveState& Objective : Battle.Objectives)
	{
		const bool bAlreadyControlled = Objective.ControllerIso.Equals(Unit.OwnerIso, ESearchCase::IgnoreCase);
		const double ControlPenalty = bAlreadyControlled ? 1000000000.0 : 0.0;
		const double Score = FVector2D::DistSquared(Unit.Position, Objective.Position) + ControlPenalty;
		if (Score < BestScore
			|| (FMath::IsNearlyEqual(Score, BestScore) && BestObjective && Objective.ObjectiveId < BestObjective->ObjectiveId))
		{
			BestScore = Score;
			BestObjective = &Objective;
		}
	}
	return BestObjective;
}

void UWLTacticalBattleSubsystem::IssueTacticalAIOrders(FWLTacticalBattleState& Battle, TArray<FString>& OutEvents)
{
	if (Battle.AIControlledOwnerIsos.IsEmpty())
	{
		return;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	for (FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (!Battle.IsOwnerAIControlled(Unit.OwnerIso) || !Unit.IsCombatEffective(Rules.TacticalRoutMoraleThreshold))
		{
			continue;
		}
		if (Unit.Order == EWLTacticalUnitOrder::Attacking
			&& IsValidAttackTarget(Battle, Unit, Unit.AttackTargetUnitId, Rules.TacticalRoutMoraleThreshold))
		{
			continue;
		}
		if (Unit.Order == EWLTacticalUnitOrder::Moving
			&& FVector2D::Distance(Unit.Position, Unit.MoveTarget) > 1.0)
		{
			const FWLTacticalUnitState* EnemyInTransit = FindNearestEffectiveEnemy(Battle, Unit, Rules.TacticalRoutMoraleThreshold);
			if (!EnemyInTransit || FVector2D::Distance(Unit.Position, EnemyInTransit->Position) > Rules.TacticalAttackRangeUnits)
			{
				continue;
			}
		}

		const FWLTacticalUnitState* Target = FindNearestEffectiveEnemy(Battle, Unit, Rules.TacticalRoutMoraleThreshold);
		if (Target)
		{
			if (FVector2D::Distance(Unit.Position, Target->Position) <= Rules.TacticalAttackRangeUnits)
			{
				Unit.AttackTargetUnitId = Target->TacticalUnitId;
				Unit.Order = EWLTacticalUnitOrder::Attacking;
				OutEvents.Add(FString::Printf(TEXT("IA tactica: %s ataca a %s."),
					*Unit.TacticalUnitId, *Target->TacticalUnitId));
				continue;
			}
		}

		const FWLTacticalObjectiveState* Objective = FindBestObjectiveForUnit(Battle, Unit);
		if (Objective)
		{
			Unit.MoveTarget = Objective->Position;
			Unit.AttackTargetUnitId.Reset();
			Unit.Order = EWLTacticalUnitOrder::Moving;
			OutEvents.Add(FString::Printf(TEXT("IA tactica: %s avanza hacia %s."),
				*Unit.TacticalUnitId, *Objective->ObjectiveId));
		}
		else if (Target)
		{
			Unit.MoveTarget = Target->Position;
			Unit.AttackTargetUnitId.Reset();
			Unit.Order = EWLTacticalUnitOrder::Moving;
			OutEvents.Add(FString::Printf(TEXT("IA tactica: %s persigue a %s."),
				*Unit.TacticalUnitId, *Target->TacticalUnitId));
		}
	}
}

void UWLTacticalBattleSubsystem::AdvanceUnitOrders(FWLTacticalBattleState& Battle, double DeltaSeconds, TArray<FString>& OutEvents)
{
	const FWLBalanceRules Rules = GetBalanceRules();
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}

	for (FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.bDestroyed)
		{
			continue;
		}

		// Velocidad y alcance POR TIPO de unidad (0 en datos = usar el global de reglas).
		FWLUnitData AttackerData;
		const bool bHasAttackerData = Registry->GetUnit(Unit.UnitId, AttackerData);
		const double UnitSpeed = (bHasAttackerData && AttackerData.SpeedUnits > 0.0)
			? AttackerData.SpeedUnits : Rules.TacticalMoveSpeedUnitsPerSecond;
		const double UnitRange = (bHasAttackerData && AttackerData.RangeUnits > 0.0)
			? AttackerData.RangeUnits : Rules.TacticalAttackRangeUnits;

		if (Unit.Morale <= Rules.TacticalRoutMoraleThreshold)
		{
			Unit.Order = EWLTacticalUnitOrder::Routing;
			Unit.AttackTargetUnitId.Reset();
			const double RetreatDirection = Unit.OwnerIso == Battle.AttackerIso ? -1.0 : 1.0;
			MoveUnitToward(Unit, Unit.Position + FVector2D(600.0 * RetreatDirection, 0.0), UnitSpeed * 0.5, DeltaSeconds);
			continue;
		}

		if (Unit.Order == EWLTacticalUnitOrder::Moving)
		{
			MoveUnitToward(Unit, Unit.MoveTarget, UnitSpeed, DeltaSeconds);
			if (FVector2D::Distance(Unit.Position, Unit.MoveTarget) <= 1.0)
			{
				Unit.Order = EWLTacticalUnitOrder::Idle;
			}
			continue;
		}

		if (Unit.Order != EWLTacticalUnitOrder::Attacking || Unit.AttackTargetUnitId.IsEmpty())
		{
			continue;
		}

		FWLTacticalUnitState* Target = FindUnit(Battle, Unit.AttackTargetUnitId);
		if (!Target || Target->bDestroyed)
		{
			Unit.Order = EWLTacticalUnitOrder::Idle;
			Unit.AttackTargetUnitId.Reset();
			continue;
		}

		const double Distance = FVector2D::Distance(Unit.Position, Target->Position);
		if (Distance > UnitRange)
		{
			MoveUnitToward(Unit, Target->Position, UnitSpeed, DeltaSeconds);
			continue;
		}

		FWLUnitData DefenderData;
		if (!bHasAttackerData || !Registry->GetUnit(Target->UnitId, DefenderData))
		{
			continue;
		}

		// --- F1 armas combinadas ---
		// Canal de dano segun el objetivo (HARD vs blindaje, SOFT vs blandos), escalado por los
		// elementos VIVOS del contingente atacante, multiplicado por la matriz de contras y
		// mitigado por el blindaje del defensor. El dano se normaliza contra el pool del
		// contingente defensor (HP por elemento x elementos iniciales) para que 50 fusileros
		// no tengan la misma vida que 4 tanques.
		const double BaseAttack = IsArmoredTarget(DefenderData.Type)
			? static_cast<double>(AttackerData.EffectiveHardAttack())
			: static_cast<double>(AttackerData.EffectiveSoftAttack());
		const double Counter = TacticalCounterMultiplier(AttackerData.Type, DefenderData.Type);
		const double Mitigation = 1.0 / (1.0 + static_cast<double>(DefenderData.EffectiveArmor()) * Rules.TacticalDefenseMitigationPerPoint);
		const double DamagePerSecond = BaseAttack
			* static_cast<double>(FMath::Max(1, Unit.ElementCount))
			* Counter
			* Rules.TacticalDamagePerAttackPerSecond
			* Mitigation;
		const double DefenderPool = FMath::Max(1.0,
			static_cast<double>(FMath::Max(1, DefenderData.Strength)) * static_cast<double>(FMath::Max(1, Target->InitialElementCount)));
		const double HealthLossPercent = DamagePerSecond * DeltaSeconds / DefenderPool * 100.0;

		const double PreviousHealth = Target->Health;
		Target->Health = FMath::Max(0.0, Target->Health - HealthLossPercent);
		const double HealthLost = PreviousHealth - Target->Health;
		Target->Morale = FMath::Max(0.0, Target->Morale - HealthLost * Rules.TacticalMoraleDamagePerHealth);

		// Las bajas se VEN: los elementos vivos siguen al % de salud del contingente.
		Target->ElementCount = Target->Health <= 0.0
			? 0
			: FMath::Clamp(FMath::CeilToInt(static_cast<double>(Target->InitialElementCount) * Target->Health / 100.0), 1, Target->InitialElementCount);

		if (Target->Health <= 0.0 && !Target->bDestroyed)
		{
			Target->bDestroyed = true;
			Target->ElementCount = 0;
			Target->Order = EWLTacticalUnitOrder::Idle;
			OutEvents.Add(FString::Printf(TEXT("%s destruida por %s."), *Target->TacticalUnitId, *Unit.TacticalUnitId));
		}
		else if (Target->Morale <= Rules.TacticalRoutMoraleThreshold && Target->Order != EWLTacticalUnitOrder::Routing)
		{
			Target->Order = EWLTacticalUnitOrder::Routing;
			Target->AttackTargetUnitId.Reset();
			OutEvents.Add(FString::Printf(TEXT("%s entra en retirada."), *Target->TacticalUnitId));
		}
	}
}

void UWLTacticalBattleSubsystem::AdvanceObjectives(FWLTacticalBattleState& Battle, double DeltaSeconds, TArray<FString>& OutEvents)
{
	const FWLBalanceRules Rules = GetBalanceRules();
	for (FWLTacticalObjectiveState& Objective : Battle.Objectives)
	{
		bool bAttackerPresent = false;
		bool bDefenderPresent = false;
		for (const FWLTacticalUnitState& Unit : Battle.Units)
		{
			if (!Unit.IsCombatEffective(Rules.TacticalRoutMoraleThreshold))
			{
				continue;
			}
			if (FVector2D::Distance(Unit.Position, Objective.Position) > Objective.Radius)
			{
				continue;
			}
			if (Unit.OwnerIso == Battle.AttackerIso)
			{
				bAttackerPresent = true;
			}
			else if (Unit.OwnerIso == Battle.DefenderIso)
			{
				bDefenderPresent = true;
			}
		}

		FString CapturingIso;
		if (bAttackerPresent && !bDefenderPresent)
		{
			CapturingIso = Battle.AttackerIso;
		}
		else if (bDefenderPresent && !bAttackerPresent)
		{
			CapturingIso = Battle.DefenderIso;
		}

		if (CapturingIso.IsEmpty())
		{
			Objective.CaptureProgressSeconds = FMath::Max(0.0, Objective.CaptureProgressSeconds - DeltaSeconds);
			continue;
		}

		Objective.CaptureProgressSeconds += DeltaSeconds;
		if (Objective.CaptureProgressSeconds >= Rules.TacticalObjectiveCaptureSeconds
			&& Objective.ControllerIso != CapturingIso)
		{
			Objective.ControllerIso = CapturingIso;
			Objective.CaptureProgressSeconds = 0.0;
			OutEvents.Add(FString::Printf(TEXT("%s captura %s."), *CapturingIso, *Objective.ObjectiveId));
		}
	}
}

void UWLTacticalBattleSubsystem::UpdateBattleResult(FWLTacticalBattleState& Battle, TArray<FString>& OutEvents)
{
	const FWLBalanceRules Rules = GetBalanceRules();
	bool bAttackerEffective = false;
	bool bDefenderEffective = false;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (!Unit.IsCombatEffective(Rules.TacticalRoutMoraleThreshold))
		{
			continue;
		}
		if (Unit.OwnerIso == Battle.AttackerIso)
		{
			bAttackerEffective = true;
		}
		else if (Unit.OwnerIso == Battle.DefenderIso)
		{
			bDefenderEffective = true;
		}
	}

	if (!bAttackerEffective && !bDefenderEffective)
	{
		Battle.Result = EWLTacticalBattleResult::Draw;
		Battle.bActive = false;
		OutEvents.Add(TEXT("Batalla tactica termina en empate."));
		return;
	}
	if (bAttackerEffective && !bDefenderEffective)
	{
		Battle.Result = EWLTacticalBattleResult::AttackerVictory;
		Battle.WinnerIso = Battle.AttackerIso;
		Battle.bActive = false;
		OutEvents.Add(FString::Printf(TEXT("%s gana la batalla tactica."), *Battle.AttackerIso));
		return;
	}
	if (!bAttackerEffective && bDefenderEffective)
	{
		Battle.Result = EWLTacticalBattleResult::DefenderVictory;
		Battle.WinnerIso = Battle.DefenderIso;
		Battle.bActive = false;
		OutEvents.Add(FString::Printf(TEXT("%s gana la batalla tactica."), *Battle.DefenderIso));
		return;
	}

	if (!Battle.Objectives.IsEmpty())
	{
		const bool bAllAttacker = Battle.Objectives.ContainsByPredicate([&Battle](const FWLTacticalObjectiveState& Objective)
		{
			return Objective.ControllerIso == Battle.AttackerIso;
		}) && !Battle.Objectives.ContainsByPredicate([&Battle](const FWLTacticalObjectiveState& Objective)
		{
			return Objective.ControllerIso != Battle.AttackerIso;
		});
		const bool bAllDefender = Battle.Objectives.ContainsByPredicate([&Battle](const FWLTacticalObjectiveState& Objective)
		{
			return Objective.ControllerIso == Battle.DefenderIso;
		}) && !Battle.Objectives.ContainsByPredicate([&Battle](const FWLTacticalObjectiveState& Objective)
		{
			return Objective.ControllerIso != Battle.DefenderIso;
		});

		if (bAllAttacker)
		{
			Battle.Result = EWLTacticalBattleResult::AttackerVictory;
			Battle.WinnerIso = Battle.AttackerIso;
			Battle.bActive = false;
			OutEvents.Add(FString::Printf(TEXT("%s gana por objetivo tactico."), *Battle.AttackerIso));
		}
		else if (bAllDefender)
		{
			Battle.Result = EWLTacticalBattleResult::DefenderVictory;
			Battle.WinnerIso = Battle.DefenderIso;
			Battle.bActive = false;
			OutEvents.Add(FString::Printf(TEXT("%s gana por objetivo tactico."), *Battle.DefenderIso));
		}
	}
}
