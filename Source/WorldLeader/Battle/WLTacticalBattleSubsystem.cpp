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
	const int32 StartIndex = Battle.Units.Num();
	for (int32 Index = 0; Index < Army.Units.Num(); ++Index)
	{
		FWLUnitData UnitData;
		const FString UnitId = NormalizeTacticalDataId(Army.Units[Index]);
		if (!Registry->GetUnit(UnitId, UnitData))
		{
			continue;
		}

		FWLTacticalUnitState Unit;
		Unit.TacticalUnitId = FString::Printf(TEXT("%s-%s-%03d"), *Battle.BattleId, *OwnerIso, StartIndex + Index + 1);
		Unit.SourceArmyId = Army.Id;
		Unit.OwnerIso = OwnerIso;
		Unit.UnitId = UnitData.Id;
		Unit.DisplayName = UnitData.Name;
		Unit.Position = Origin + FVector2D(0.0, static_cast<double>(Index) * 260.0 * DirectionSign);
		Unit.MoveTarget = Unit.Position;
		Battle.Units.Add(MoveTemp(Unit));
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
		if (Unit.Morale <= Rules.TacticalRoutMoraleThreshold)
		{
			Unit.Order = EWLTacticalUnitOrder::Routing;
			Unit.AttackTargetUnitId.Reset();
			const double RetreatDirection = Unit.OwnerIso == Battle.AttackerIso ? -1.0 : 1.0;
			MoveUnitToward(Unit, Unit.Position + FVector2D(600.0 * RetreatDirection, 0.0), Rules.TacticalMoveSpeedUnitsPerSecond * 0.5, DeltaSeconds);
			continue;
		}

		if (Unit.Order == EWLTacticalUnitOrder::Moving)
		{
			MoveUnitToward(Unit, Unit.MoveTarget, Rules.TacticalMoveSpeedUnitsPerSecond, DeltaSeconds);
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
		if (Distance > Rules.TacticalAttackRangeUnits)
		{
			MoveUnitToward(Unit, Target->Position, Rules.TacticalMoveSpeedUnitsPerSecond, DeltaSeconds);
			continue;
		}

		FWLUnitData AttackerData;
		FWLUnitData DefenderData;
		if (!Registry->GetUnit(Unit.UnitId, AttackerData) || !Registry->GetUnit(Target->UnitId, DefenderData))
		{
			continue;
		}

		const double Mitigation = 1.0 / (1.0 + static_cast<double>(DefenderData.Defense) * Rules.TacticalDefenseMitigationPerPoint);
		const double Damage = static_cast<double>(AttackerData.Attack) * Rules.TacticalDamagePerAttackPerSecond * Mitigation * DeltaSeconds;
		const double PreviousHealth = Target->Health;
		Target->Health = FMath::Max(0.0, Target->Health - Damage);
		const double HealthLost = PreviousHealth - Target->Health;
		Target->Morale = FMath::Max(0.0, Target->Morale - HealthLost * Rules.TacticalMoraleDamagePerHealth);

		if (Target->Health <= 0.0 && !Target->bDestroyed)
		{
			Target->bDestroyed = true;
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
