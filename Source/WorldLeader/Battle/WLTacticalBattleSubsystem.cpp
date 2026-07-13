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

	// Fuerzas especiales combaten como infanteria; el drone legacy como vehiculo ligero fragil.
	EWLUnitType NormalizeCombatType(EWLUnitType T)
	{
		if (T == EWLUnitType::SpecialForces) { return EWLUnitType::Infantry; }
		if (T == EWLUnitType::Drone)         { return EWLUnitType::LightVehicle; }
		return T;
	}

	// --- F1: matriz de contras de armas combinadas (terreno abierto). Atacante -> defensor. ---
	// La "piedra-papel-tijera" moderna: el tanque revienta infanteria en abierto, la infanteria
	// apenas rasca blindaje, el SAM caza aviacion, la aviacion caza tanques, la artilleria
	// castiga objetivos blandos. Los matchups clave estan amarrados por tests de automation.
	double OpenFieldCounterMultiplier(EWLUnitType Attacker, EWLUnitType Defender)
	{
		const EWLUnitType A = NormalizeCombatType(Attacker);
		const EWLUnitType D = NormalizeCombatType(Defender);

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
			case EWLUnitType::Air:          return 0.0;   // F3: al aire solo le pegan SAM y cazas
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
			case EWLUnitType::Air:          return 0.0;   // F3
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
			case EWLUnitType::Air:          return 0.0;   // F3
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
			case EWLUnitType::Air:          return 0.0;   // F3
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

	// --- F2: el terreno del combate modifica la matriz ---
	// La EMBOSCADA nace de la cobertura del que dispara (infanteria con ATGM entre edificios
	// revienta blindados); la PROTECCION, de la cobertura del que recibe (no se desaloja
	// infanteria urbana a canonazos, y la aviacion/artilleria pierden ojos sobre cobertura).
	double TacticalCounterMultiplier(
		EWLUnitType Attacker,
		EWLUnitType Defender,
		EWLTacticalTerrain TerrainAtAttacker,
		EWLTacticalTerrain TerrainAtTarget)
	{
		const EWLUnitType A = NormalizeCombatType(Attacker);
		const EWLUnitType D = NormalizeCombatType(Defender);
		double Value = OpenFieldCounterMultiplier(A, D);

		if (TerrainAtTarget != EWLTacticalTerrain::Open)
		{
			const bool bUrban = TerrainAtTarget == EWLTacticalTerrain::Urban;
			if (A == EWLUnitType::Armor && D == EWLUnitType::Infantry)             { Value = bUrban ? 0.7 : 0.9; }
			else if (A == EWLUnitType::LightVehicle && D == EWLUnitType::Infantry) { Value = bUrban ? 0.8 : 1.1; }
			else if (A == EWLUnitType::Air)                                        { Value *= bUrban ? 0.6 : 0.7; }
			else if (A == EWLUnitType::Artillery)                                  { Value *= bUrban ? 0.75 : 0.85; }
		}
		if (TerrainAtAttacker != EWLTacticalTerrain::Open && A == EWLUnitType::Infantry)
		{
			const bool bUrban = TerrainAtAttacker == EWLTacticalTerrain::Urban;
			if (D == EWLUnitType::Armor)             { Value = FMath::Max(Value, bUrban ? 1.4 : 1.1); }
			else if (D == EWLUnitType::LightVehicle) { Value = FMath::Max(Value, bUrban ? 1.6 : 1.2); }
		}
		return Value;
	}

	// F2: contra un objetivo en cobertura el fuego directo ocurre en el borde del parche, no
	// desde 900 unidades (el tanque tiene que METERSE a la ciudad, y ahi lo emboscan).
	bool UsesIndirectFire(EWLUnitType Type)
	{
		const EWLUnitType T = NormalizeCombatType(Type);
		return T == EWLUnitType::Artillery || T == EWLUnitType::Naval;
	}

	// El dano usa el canal correcto: HARD attack contra blindaje, SOFT contra lo demas.
	bool IsArmoredTarget(EWLUnitType Type)
	{
		return Type == EWLUnitType::Armor
			|| Type == EWLUnitType::LightVehicle
			|| Type == EWLUnitType::Naval;
	}

	// F4: encaramiento del contingente — hacia su objetivo de ataque, su destino de
	// movimiento, o el frente por defecto (atacante mira +X, defensor -X).
	FVector2D ComputeUnitFacing(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit)
	{
		if (Unit.Order == EWLTacticalUnitOrder::Attacking && !Unit.AttackTargetUnitId.IsEmpty())
		{
			const FWLTacticalUnitState* Target = Battle.Units.FindByPredicate([&Unit](const FWLTacticalUnitState& U)
			{
				return U.TacticalUnitId == Unit.AttackTargetUnitId;
			});
			if (Target && !(Target->Position - Unit.Position).IsNearlyZero())
			{
				return (Target->Position - Unit.Position).GetSafeNormal();
			}
		}
		if ((Unit.Order == EWLTacticalUnitOrder::Moving || Unit.Order == EWLTacticalUnitOrder::Routing)
			&& !(Unit.MoveTarget - Unit.Position).IsNearlyZero())
		{
			return (Unit.MoveTarget - Unit.Position).GetSafeNormal();
		}
		return Unit.OwnerIso == Battle.AttackerIso ? FVector2D(1.0, 0.0) : FVector2D(-1.0, 0.0);
	}

	// F4: fuera del fuego los nervios se calman — la moral se recupera, y el que huia se
	// reagrupa al superar el umbral de derrota con margen.
	void RecoverMorale(
		FWLTacticalBattleState& Battle,
		const TArray<double>& HealthBeforeTick,
		double DeltaSeconds,
		const FWLBalanceRules& Rules,
		TArray<FString>& OutEvents)
	{
		for (int32 Index = 0; Index < Battle.Units.Num(); ++Index)
		{
			FWLTacticalUnitState& Unit = Battle.Units[Index];
			if (Unit.bDestroyed || Unit.Health <= 0.0 || Unit.Morale >= 100.0)
			{
				continue;
			}
			if (HealthBeforeTick.IsValidIndex(Index) && Unit.Health < HealthBeforeTick[Index])
			{
				continue;   // bajo fuego este tick: nada de calma
			}
			Unit.Morale = FMath::Min(100.0, Unit.Morale + Rules.TacticalMoraleRecoveryPerSecond * DeltaSeconds);
			if (Unit.Order == EWLTacticalUnitOrder::Routing
				&& Unit.Morale >= static_cast<double>(Rules.TacticalRoutMoraleThreshold + Rules.TacticalRallyMoraleMargin))
			{
				Unit.Order = EWLTacticalUnitOrder::Idle;
				Unit.MoveTarget = Unit.Position;
				OutEvents.Add(FString::Printf(TEXT("%s se reagrupa."), *Unit.TacticalUnitId));
			}
		}
	}

	// Aplica el dano de un ataque (fuego directo o salva) a un contingente: canal correcto
	// (AA contra aire, HARD contra blindaje, SOFT contra el resto), matriz con terreno,
	// mitigacion por blindaje y bono por mantener posicion; deriva bajas visibles y moral.
	void ApplyTacticalDamage(
		const FWLBalanceRules& Rules,
		const FWLUnitData& AttackerData,
		int32 AttackerElements,
		EWLTacticalTerrain TerrainAtAttacker,
		EWLTacticalTerrain TerrainAtTarget,
		double Seconds,
		const FWLUnitData& DefenderData,
		FWLTacticalUnitState& Target,
		const FString& SourceLabel,
		TArray<FString>& OutEvents,
		const FVector2D& AttackerPosition,
		const FVector2D& DefenderFacing,
		double MoraleFactor = 1.0)
	{
		// F4: FLANQUEO — pegar por el flanco o la retaguardia hace mas dano y rompe nervios.
		// El angulo se mide contra el encaramiento del defensor (los aviones no tienen flanco).
		double FlankDamage = 1.0;
		double FlankMorale = 1.0;
		if (DefenderData.Type != EWLUnitType::Air && !DefenderFacing.IsNearlyZero())
		{
			const FVector2D ToAttacker = (AttackerPosition - Target.Position).GetSafeNormal();
			if (!ToAttacker.IsNearlyZero())
			{
				const double Alignment = FVector2D::DotProduct(DefenderFacing, ToAttacker);
				if (Alignment < Rules.TacticalRearArcDotThreshold)
				{
					FlankDamage = Rules.TacticalRearDamageMultiplier;
					FlankMorale = Rules.TacticalRearMoraleMultiplier;
				}
				else if (Alignment <= Rules.TacticalFlankArcDotThreshold)
				{
					FlankDamage = Rules.TacticalFlankDamageMultiplier;
					FlankMorale = Rules.TacticalFlankMoraleMultiplier;
				}
			}
		}
		const double BaseAttack = DefenderData.Type == EWLUnitType::Air
			? static_cast<double>(AttackerData.EffectiveAAAttack())
			: (IsArmoredTarget(DefenderData.Type)
				? static_cast<double>(AttackerData.EffectiveHardAttack())
				: static_cast<double>(AttackerData.EffectiveSoftAttack()));
		const double Counter = TacticalCounterMultiplier(AttackerData.Type, DefenderData.Type, TerrainAtAttacker, TerrainAtTarget);
		const double Mitigation = 1.0 / (1.0 + static_cast<double>(DefenderData.EffectiveArmor()) * Rules.TacticalDefenseMitigationPerPoint);
		// F2: mantener posicion atrinchera — la unidad quieta recibe menos dano.
		const double HoldBonus = Target.Order == EWLTacticalUnitOrder::Idle ? 0.85 : 1.0;
		const double DamagePerSecond = BaseAttack
			* static_cast<double>(FMath::Max(1, AttackerElements))
			* Counter
			* Rules.TacticalDamagePerAttackPerSecond
			* Mitigation
			* HoldBonus
			* FlankDamage;
		const double DefenderPool = FMath::Max(1.0,
			static_cast<double>(FMath::Max(1, DefenderData.Strength)) * static_cast<double>(FMath::Max(1, Target.InitialElementCount)));
		const double HealthLossPercent = DamagePerSecond * Seconds / DefenderPool * 100.0;

		const double PreviousHealth = Target.Health;
		Target.Health = FMath::Max(0.0, Target.Health - HealthLossPercent);
		const double HealthLost = PreviousHealth - Target.Health;
		Target.Morale = FMath::Max(0.0, Target.Morale - HealthLost * Rules.TacticalMoraleDamagePerHealth * FlankMorale * MoraleFactor);

		// Las bajas se VEN: los elementos vivos siguen al % de salud del contingente.
		Target.ElementCount = Target.Health <= 0.0
			? 0
			: FMath::Clamp(FMath::CeilToInt(static_cast<double>(Target.InitialElementCount) * Target.Health / 100.0), 1, Target.InitialElementCount);

		if (Target.Health <= 0.0 && !Target.bDestroyed)
		{
			Target.bDestroyed = true;
			Target.ElementCount = 0;
			Target.Order = EWLTacticalUnitOrder::Idle;
			OutEvents.Add(FString::Printf(TEXT("%s destruida por %s."), *Target.TacticalUnitId, *SourceLabel));
		}
		else if (Target.Morale <= Rules.TacticalRoutMoraleThreshold && Target.Order != EWLTacticalUnitOrder::Routing)
		{
			Target.Order = EWLTacticalUnitOrder::Routing;
			Target.AttackTargetUnitId.Reset();
			OutEvents.Add(FString::Printf(TEXT("%s entra en retirada."), *Target.TacticalUnitId));
		}
	}
}

EWLTacticalTerrain UWLTacticalBattleSubsystem::TerrainAtPosition(const FWLTacticalBattleState& Battle, const FVector2D& Position)
{
	for (const FWLTacticalTerrainPatch& Patch : Battle.TerrainPatches)
	{
		if (FVector2D::Distance(Position, Patch.Position) <= Patch.Radius)
		{
			return Patch.Terrain;
		}
	}
	return EWLTacticalTerrain::Open;
}

double UWLTacticalBattleSubsystem::GetCoverEngageRange(EWLTacticalTerrain TerrainAtTarget)
{
	switch (TerrainAtTarget)
	{
	case EWLTacticalTerrain::Urban:  return 380.0;
	case EWLTacticalTerrain::Forest: return 500.0;
	default:                         return TNumericLimits<double>::Max();
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

TArray<FWLTacticalBattleState> UWLTacticalBattleSubsystem::GetTacticalBattleStates() const
{
	TArray<FWLTacticalBattleState> Result;
	Battles.GenerateValueArray(Result);
	Result.Sort([](const FWLTacticalBattleState& A, const FWLTacticalBattleState& B)
	{
		return A.BattleId < B.BattleId;
	});
	return Result;
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

	// F5: no se aceptan ordenes IMPOSIBLES (fusiles contra un caza): perseguirian para
	// siempre haciendo dano cero. La matriz decide que puede danar a que.
	if (const UWLDataRegistry* Registry = GetRegistry())
	{
		FWLUnitData UnitData, TargetData;
		if (Registry->GetUnit(Unit->UnitId, UnitData) && Registry->GetUnit(Target->UnitId, TargetData)
			&& OpenFieldCounterMultiplier(UnitData.Type, TargetData.Type) <= 0.0)
		{
			OutMessage = FString::Printf(TEXT("%s no puede danar a %s (objetivo aereo: usa SAM o cazas)."),
				*Unit->DisplayName, *Target->DisplayName);
			return false;
		}
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

bool UWLTacticalBattleSubsystem::AddTacticalTerrainPatch(
	const FString& BattleId,
	EWLTacticalTerrain Terrain,
	FVector2D Position,
	double Radius,
	FString& OutMessage)
{
	FWLTacticalBattleState* Battle = FindBattle(BattleId);
	if (!Battle)
	{
		OutMessage = FString::Printf(TEXT("Batalla tactica desconocida: %s"), *BattleId);
		return false;
	}

	FWLTacticalTerrainPatch Patch;
	Patch.PatchId = FString::Printf(TEXT("%s-TER-%d"), *Battle->BattleId, Battle->TerrainPatches.Num() + 1);
	Patch.Terrain = Terrain;
	Patch.Position = Position;
	Patch.Radius = FMath::Max(50.0, Radius);
	Battle->TerrainPatches.Add(Patch);
	OutMessage = FString::Printf(TEXT("Parche de terreno %s creado en %.0f, %.0f."),
		*Patch.PatchId, Position.X, Position.Y);
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
	// F4: foto de salud al inicio del tick — solo recupera moral quien NO recibio fuego.
	TArray<double> HealthBeforeTick;
	HealthBeforeTick.Reserve(Battle->Units.Num());
	for (const FWLTacticalUnitState& Unit : Battle->Units)
	{
		HealthBeforeTick.Add(Unit.Health);
	}
	IssueTacticalAIOrders(*Battle, OutEvents);
	AdvanceUnitOrders(*Battle, StepSeconds, OutEvents);
	AdvanceShells(*Battle, OutEvents);
	AdvanceAutoAirDefense(*Battle, StepSeconds, OutEvents);
	RecoverMorale(*Battle, HealthBeforeTick, StepSeconds, GetBalanceRules(), OutEvents);
	AdvanceObjectives(*Battle, StepSeconds, HealthBeforeTick, OutEvents);
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
	// F3: la IA no persigue lo que no puede danar (un tanque apuntando a un caza). Prefiere
	// el enemigo DANABLE mas cercano; solo si no hay ninguno cae al mas cercano a secas
	// (para seguir maniobrando hacia el frente).
	const UWLDataRegistry* Registry = GetRegistry();
	FWLUnitData UnitData;
	const bool bHasUnitData = Registry && Registry->GetUnit(Unit.UnitId, UnitData);

	const FWLTacticalUnitState* BestDamageable = nullptr;
	double BestDamageableDistSq = TNumericLimits<double>::Max();
	const FWLTacticalUnitState* BestAny = nullptr;
	double BestAnyDistSq = TNumericLimits<double>::Max();
	for (const FWLTacticalUnitState& Candidate : Battle.Units)
	{
		if (Candidate.OwnerIso == Unit.OwnerIso || !Candidate.IsCombatEffective(RoutMoraleThreshold))
		{
			continue;
		}

		const double DistanceSq = FVector2D::DistSquared(Unit.Position, Candidate.Position);
		if (DistanceSq < BestAnyDistSq
			|| (FMath::IsNearlyEqual(DistanceSq, BestAnyDistSq) && BestAny && Candidate.TacticalUnitId < BestAny->TacticalUnitId))
		{
			BestAnyDistSq = DistanceSq;
			BestAny = &Candidate;
		}

		bool bDamageable = true;
		FWLUnitData CandidateData;
		if (bHasUnitData && Registry->GetUnit(Candidate.UnitId, CandidateData))
		{
			bDamageable = OpenFieldCounterMultiplier(UnitData.Type, CandidateData.Type) > 0.0;
		}
		if (bDamageable
			&& (DistanceSq < BestDamageableDistSq
				|| (FMath::IsNearlyEqual(DistanceSq, BestDamageableDistSq) && BestDamageable && Candidate.TacticalUnitId < BestDamageable->TacticalUnitId)))
		{
			BestDamageableDistSq = DistanceSq;
			BestDamageable = &Candidate;
		}
	}
	return BestDamageable ? BestDamageable : BestAny;
}

const FWLTacticalUnitState* UWLTacticalBattleSubsystem::FindBestAITarget(
	const FWLTacticalBattleState& Battle,
	const FWLTacticalUnitState& Unit,
	int32 RoutMoraleThreshold) const
{
	// F5: la IA elige por MATRIZ, no por cercania: el mejor matchup (con terreno actual)
	// ponderado por distancia. La aviacion evita entrar a un paraguas SAM si tiene opcion.
	const UWLDataRegistry* Registry = GetRegistry();
	FWLUnitData MyData;
	if (!Registry || !Registry->GetUnit(Unit.UnitId, MyData))
	{
		return FindNearestEffectiveEnemy(Battle, Unit, RoutMoraleThreshold);
	}
	const EWLTacticalTerrain MyTerrain = TerrainAtPosition(Battle, Unit.Position);

	auto UnderEnemyUmbrella = [&](const FVector2D& Position) -> bool
	{
		for (const FWLTacticalUnitState& Sam : Battle.Units)
		{
			if (Sam.OwnerIso == Unit.OwnerIso || !Sam.IsCombatEffective(RoutMoraleThreshold))
			{
				continue;
			}
			FWLUnitData SamData;
			if (!Registry->GetUnit(Sam.UnitId, SamData) || NormalizeCombatType(SamData.Type) != EWLUnitType::AirDefense)
			{
				continue;
			}
			const double Range = SamData.RangeUnits > 0.0 ? SamData.RangeUnits : 1200.0;
			if (FVector2D::Distance(Sam.Position, Position) <= Range)
			{
				return true;
			}
		}
		return false;
	};

	const FWLTacticalUnitState* Best = nullptr;
	double BestScore = 0.0;
	for (const FWLTacticalUnitState& Candidate : Battle.Units)
	{
		if (Candidate.OwnerIso == Unit.OwnerIso || !Candidate.IsCombatEffective(RoutMoraleThreshold))
		{
			continue;
		}
		FWLUnitData CandidateData;
		if (!Registry->GetUnit(Candidate.UnitId, CandidateData))
		{
			continue;
		}
		double Score = TacticalCounterMultiplier(MyData.Type, CandidateData.Type,
			MyTerrain, TerrainAtPosition(Battle, Candidate.Position));
		if (Score <= 0.0)
		{
			continue;   // no puede danarlo: no es un objetivo
		}
		if (MyData.Type == EWLUnitType::Air && UnderEnemyUmbrella(Candidate.Position))
		{
			Score *= 0.25;   // volar al paraguas SAM cuesta caro: solo si no hay nada mejor
		}
		Score /= 1.0 + FVector2D::Distance(Unit.Position, Candidate.Position) / 1200.0;
		if (Score > BestScore
			|| (FMath::IsNearlyEqual(Score, BestScore) && Best && Candidate.TacticalUnitId < Best->TacticalUnitId))
		{
			BestScore = Score;
			Best = &Candidate;
		}
	}
	return Best;
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
	const UWLDataRegistry* Registry = GetRegistry();
	for (FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (!Battle.IsOwnerAIControlled(Unit.OwnerIso) || !Unit.IsCombatEffective(Rules.TacticalRoutMoraleThreshold))
		{
			continue;
		}

		FWLUnitData UnitData;
		const bool bHasUnitData = Registry && Registry->GetUnit(Unit.UnitId, UnitData);

		// F5: el SAM no maniobra por su cuenta — su trabajo es el paraguas (fuego automatico
		// contra lo aereo); moverlo al frente es regalarlo.
		if (bHasUnitData && NormalizeCombatType(UnitData.Type) == EWLUnitType::AirDefense)
		{
			continue;
		}

		if (Unit.Order == EWLTacticalUnitOrder::Attacking
			&& IsValidAttackTarget(Battle, Unit, Unit.AttackTargetUnitId, Rules.TacticalRoutMoraleThreshold))
		{
			continue;
		}

		// F5: la eleccion de objetivo es por MATRIZ (null = no puede danar a nadie).
		const FWLTacticalUnitState* Target = FindBestAITarget(Battle, Unit, Rules.TacticalRoutMoraleThreshold);

		if (Unit.Order == EWLTacticalUnitOrder::Moving
			&& FVector2D::Distance(Unit.Position, Unit.MoveTarget) > 1.0)
		{
			if (!Target || FVector2D::Distance(Unit.Position, Target->Position) > Rules.TacticalAttackRangeUnits)
			{
				continue;   // en transito y sin presa buena a tiro: seguir marchando
			}
		}

		if (Target && FVector2D::Distance(Unit.Position, Target->Position) <= Rules.TacticalAttackRangeUnits)
		{
			Unit.AttackTargetUnitId = Target->TacticalUnitId;
			Unit.Order = EWLTacticalUnitOrder::Attacking;
			OutEvents.Add(FString::Printf(TEXT("IA tactica: %s ataca a %s."),
				*Unit.TacticalUnitId, *Target->TacticalUnitId));
			continue;
		}

		const FWLTacticalObjectiveState* Objective = FindBestObjectiveForUnit(Battle, Unit);
		if (Objective)
		{
			// F5: la infanteria busca COBERTURA que domine el objetivo (ATGM al bosque o a
			// la ciudad) en vez de plantarse en campo abierto: el parche MAS CERCANO a la
			// unidad de entre los que quedan a tiro del objetivo.
			FVector2D MoveTarget = Objective->Position;
			if (bHasUnitData && NormalizeCombatType(UnitData.Type) == EWLUnitType::Infantry)
			{
				double BestPatchDistance = TNumericLimits<double>::Max();
				for (const FWLTacticalTerrainPatch& Patch : Battle.TerrainPatches)
				{
					if (FVector2D::Distance(Patch.Position, Objective->Position) > 700.0)
					{
						continue;
					}
					const double PatchDistance = FVector2D::Distance(Patch.Position, Unit.Position);
					if (PatchDistance < BestPatchDistance)
					{
						BestPatchDistance = PatchDistance;
						MoveTarget = Patch.Position;
					}
				}
			}
			// Ya en posicion: QUEDARSE (Idle atrinchera y conserva el encaramiento); nada
			// de re-ordenar cada tick — eso anulaba el bono defensivo de la propia IA.
			if (FVector2D::Distance(Unit.Position, MoveTarget) <= 40.0)
			{
				continue;
			}
			Unit.MoveTarget = MoveTarget;
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

		FWLUnitData DefenderData;
		if (!bHasAttackerData || !Registry->GetUnit(Target->UnitId, DefenderData))
		{
			continue;
		}

		// F2: contra cobertura no hay francotirador de tanques: el fuego directo obliga a
		// acercarse al borde del parche. Artilleria/naval tiran por elevacion y no se
		// acercan; y los objetivos AEREOS no se esconden en edificios (sin recorte).
		const bool bIndirectFire = UsesIndirectFire(AttackerData.Type);
		const EWLTacticalTerrain TerrainAtTarget = TerrainAtPosition(Battle, Target->Position);
		const double EngageRange = (bIndirectFire || DefenderData.Type == EWLUnitType::Air)
			? UnitRange
			: FMath::Min(UnitRange, GetCoverEngageRange(TerrainAtTarget));

		const double Distance = FVector2D::Distance(Unit.Position, Target->Position);
		if (Distance > EngageRange)
		{
			MoveUnitToward(Unit, Target->Position, UnitSpeed, DeltaSeconds);
			continue;
		}

		// Un ALA FIJA no se queda suspendida disparando (eso es un helicoptero): el caza ORBITA
		// a su objetivo dentro del alcance — pasadas continuas — y dispara mientras vuela. El
		// sentido de giro es estable por contingente; una componente radial suave lo mantiene a
		// ~70% del alcance sin perder nunca la ventana de fuego.
		if (AttackerData.Type == EWLUnitType::Air && !Unit.UnitId.Equals(TEXT("heli"), ESearchCase::IgnoreCase)
			&& Distance > KINDA_SMALL_NUMBER)
		{
			const FVector2D ToTarget = (Target->Position - Unit.Position).GetSafeNormal();
			const double Spin = (GetTypeHash(Unit.TacticalUnitId) % 2 == 0) ? 1.0 : -1.0;
			const FVector2D Tangent(-ToTarget.Y * Spin, ToTarget.X * Spin);
			const double RadialError = Distance - EngageRange * 0.7;
			const FVector2D FlightDir = (Tangent + ToTarget * FMath::Clamp(RadialError / 250.0, -0.6, 0.6)).GetSafeNormal();
			Unit.Position += FlightDir * UnitSpeed * DeltaSeconds;
		}

		// F3: la artilleria/naval no hace dano directo continuo — dispara SALVAS contra la
		// POSICION actual del objetivo, con tiempo de vuelo: mata estaticos, falla contra
		// moviles (que al impacto ya no estan alli).
		if (bIndirectFire)
		{
			Unit.IndirectCooldownSeconds -= DeltaSeconds;
			if (Unit.IndirectCooldownSeconds <= 0.0)
			{
				FWLTacticalShellState Shell;
				Shell.ShellId = FString::Printf(TEXT("%s-SH-%d"), *Battle.BattleId, Battle.NextShellNumber++);
				Shell.OwnerIso = Unit.OwnerIso;
				Shell.SourceUnitId = Unit.UnitId;
				Shell.Elements = Unit.ElementCount;
				Shell.FirePosition = Unit.Position;
				Shell.ImpactPosition = Target->Position;
				Shell.FiredAtSeconds = Battle.ElapsedSeconds;
				Shell.ImpactAtSeconds = Battle.ElapsedSeconds + 1.2 + Distance / Rules.TacticalIndirectShellSpeedUnits;
				Battle.Shells.Add(Shell);
				Unit.IndirectCooldownSeconds = Rules.TacticalIndirectVolleyPeriodSeconds;
				OutEvents.Add(FString::Printf(TEXT("%s dispara una salva sobre %.0f, %.0f."),
					*Unit.TacticalUnitId, Shell.ImpactPosition.X, Shell.ImpactPosition.Y));
			}
			continue;
		}

		// --- F1 armas combinadas: fuego directo continuo ---
		const EWLTacticalTerrain TerrainAtAttacker = TerrainAtPosition(Battle, Unit.Position);
		ApplyTacticalDamage(Rules, AttackerData, Unit.ElementCount,
			TerrainAtAttacker, TerrainAtTarget, DeltaSeconds,
			DefenderData, *Target, Unit.TacticalUnitId, OutEvents,
			Unit.Position, ComputeUnitFacing(Battle, *Target));
	}
}

void UWLTacticalBattleSubsystem::AdvanceShells(FWLTacticalBattleState& Battle, TArray<FString>& OutEvents)
{
	if (Battle.Shells.IsEmpty())
	{
		return;
	}
	const FWLBalanceRules Rules = GetBalanceRules();
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}

	for (int32 Index = 0; Index < Battle.Shells.Num(); )
	{
		if (Battle.ElapsedSeconds < Battle.Shells[Index].ImpactAtSeconds)
		{
			++Index;
			continue;
		}
		const FWLTacticalShellState Shell = Battle.Shells[Index];

		FWLUnitData AttackerData;
		if (Registry->GetUnit(Shell.SourceUnitId, AttackerData))
		{
			// Area: dana a todo contingente ENEMIGO dentro del radio al momento del impacto.
			// El que se movio ya no esta. La salva concentra el periodo configurado de fuego.
			for (FWLTacticalUnitState& Victim : Battle.Units)
			{
				if (Victim.bDestroyed || Victim.OwnerIso == Shell.OwnerIso)
				{
					continue;
				}
				if (FVector2D::Distance(Victim.Position, Shell.ImpactPosition) > Shell.Radius)
				{
					continue;
				}
				FWLUnitData DefenderData;
				if (!Registry->GetUnit(Victim.UnitId, DefenderData))
				{
					continue;
				}
				// F4: el bombardeo SUPRIME — moral castigada muy por encima del dano fisico.
				// Una explosion de area no tiene angulo: sin bono de flanqueo (facing nulo).
				ApplyTacticalDamage(Rules, AttackerData, Shell.Elements,
					EWLTacticalTerrain::Open, TerrainAtPosition(Battle, Victim.Position),
					Rules.TacticalIndirectVolleyPeriodSeconds, DefenderData, Victim, Shell.ShellId, OutEvents,
					Shell.FirePosition, FVector2D::ZeroVector, Rules.TacticalIndirectSuppressionMoraleFactor);
			}
		}
		Battle.Shells.RemoveAt(Index);
	}
}

void UWLTacticalBattleSubsystem::AdvanceAutoAirDefense(FWLTacticalBattleState& Battle, double DeltaSeconds, TArray<FString>& OutEvents)
{
	const FWLBalanceRules Rules = GetBalanceRules();
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}

	for (FWLTacticalUnitState& Sam : Battle.Units)
	{
		if (!Sam.IsCombatEffective(Rules.TacticalRoutMoraleThreshold))
		{
			continue;
		}
		FWLUnitData SamData;
		if (!Registry->GetUnit(Sam.UnitId, SamData) || NormalizeCombatType(SamData.Type) != EWLUnitType::AirDefense)
		{
			continue;
		}
		const double Range = SamData.RangeUnits > 0.0 ? SamData.RangeUnits : Rules.TacticalAttackRangeUnits;

		// Objetivo aereo enemigo mas cercano dentro del paraguas (desempate deterministico).
		FWLTacticalUnitState* AirTarget = nullptr;
		FWLUnitData AirData;
		double BestDistance = TNumericLimits<double>::Max();
		for (FWLTacticalUnitState& Candidate : Battle.Units)
		{
			if (Candidate.OwnerIso == Sam.OwnerIso || !Candidate.IsCombatEffective(Rules.TacticalRoutMoraleThreshold))
			{
				continue;
			}
			FWLUnitData CandidateData;
			if (!Registry->GetUnit(Candidate.UnitId, CandidateData) || CandidateData.Type != EWLUnitType::Air)
			{
				continue;
			}
			const double CandidateDistance = FVector2D::Distance(Sam.Position, Candidate.Position);
			if (CandidateDistance > Range)
			{
				continue;
			}
			if (CandidateDistance < BestDistance
				|| (FMath::IsNearlyEqual(CandidateDistance, BestDistance) && AirTarget && Candidate.TacticalUnitId < AirTarget->TacticalUnitId))
			{
				BestDistance = CandidateDistance;
				AirTarget = &Candidate;
				AirData = CandidateData;
			}
		}
		if (!AirTarget)
		{
			continue;
		}
		// UN canal de tiro: si ya ataca a un AEREO por orden explicita, ese fuego ya se
		// resolvio este tick — el automatico no duplica la salida del SAM.
		if (Sam.Order == EWLTacticalUnitOrder::Attacking && !Sam.AttackTargetUnitId.IsEmpty())
		{
			if (const FWLTacticalUnitState* Ordered = FindUnit(Battle, Sam.AttackTargetUnitId))
			{
				FWLUnitData OrderedData;
				if (Registry->GetUnit(Ordered->UnitId, OrderedData) && OrderedData.Type == EWLUnitType::Air)
				{
					continue;
				}
			}
		}
		ApplyTacticalDamage(Rules, SamData, Sam.ElementCount,
			TerrainAtPosition(Battle, Sam.Position), TerrainAtPosition(Battle, AirTarget->Position),
			DeltaSeconds, AirData, *AirTarget, Sam.TacticalUnitId, OutEvents,
			Sam.Position, ComputeUnitFacing(Battle, *AirTarget));
	}
}

void UWLTacticalBattleSubsystem::AdvanceObjectives(FWLTacticalBattleState& Battle, double DeltaSeconds, const TArray<double>& HealthBeforeTick, TArray<FString>& OutEvents)
{
	const FWLBalanceRules Rules = GetBalanceRules();
	const UWLDataRegistry* Registry = GetRegistry();
	for (FWLTacticalObjectiveState& Objective : Battle.Objectives)
	{
		bool bAttackerPresent = false;
		bool bDefenderPresent = false;
		bool bAttackerCalm = false;   // presente y SIN recibir fuego este tick (puede capturar)
		bool bDefenderCalm = false;
		for (int32 Index = 0; Index < Battle.Units.Num(); ++Index)
		{
			const FWLTacticalUnitState& Unit = Battle.Units[Index];
			if (!Unit.IsCombatEffective(Rules.TacticalRoutMoraleThreshold))
			{
				continue;
			}
			if (FVector2D::Distance(Unit.Position, Objective.Position) > Objective.Radius)
			{
				continue;
			}
			// F5: el terreno lo toman botas y blindados — la aviacion sobrevolando ni
			// captura ni disputa el objetivo.
			if (Registry)
			{
				FWLUnitData UnitData;
				if (Registry->GetUnit(Unit.UnitId, UnitData) && UnitData.Type == EWLUnitType::Air)
				{
					continue;
				}
			}
			// Bajo fuego se DISPUTA pero no se CAPTURA: nadie iza bandera mientras lo
			// estan destrozando (y ganar "por puntos" bajo bombardeo seria un exploit).
			const bool bUnderFire = HealthBeforeTick.IsValidIndex(Index) && Unit.Health < HealthBeforeTick[Index];
			if (Unit.OwnerIso == Battle.AttackerIso)
			{
				bAttackerPresent = true;
				bAttackerCalm = bAttackerCalm || !bUnderFire;
			}
			else if (Unit.OwnerIso == Battle.DefenderIso)
			{
				bDefenderPresent = true;
				bDefenderCalm = bDefenderCalm || !bUnderFire;
			}
		}

		FString CapturingIso;
		if (bAttackerPresent && !bDefenderPresent && bAttackerCalm)
		{
			CapturingIso = Battle.AttackerIso;
		}
		else if (bDefenderPresent && !bAttackerPresent && bDefenderCalm)
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
