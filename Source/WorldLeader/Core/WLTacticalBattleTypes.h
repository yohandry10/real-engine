// Copyright World Leader project. See ROADMAP.md.
//
// Backend B2: estado tactico determinista. UI/3D debe leer estos structs y emitir
// ordenes al subsystem; no debe decidir dano, moral ni victoria.

#pragma once

#include "CoreMinimal.h"
#include "WLTacticalBattleTypes.generated.h"

UENUM(BlueprintType)
enum class EWLTacticalUnitOrder : uint8
{
	Idle      UMETA(DisplayName = "Idle"),
	Moving    UMETA(DisplayName = "Moving"),
	Attacking UMETA(DisplayName = "Attacking"),
	Routing   UMETA(DisplayName = "Routing")
};

UENUM(BlueprintType)
enum class EWLTacticalTerrain : uint8
{
	Open   UMETA(DisplayName = "Open"),
	Urban  UMETA(DisplayName = "Urban"),
	Forest UMETA(DisplayName = "Forest")
};

UENUM(BlueprintType)
enum class EWLTacticalBattleResult : uint8
{
	Ongoing          UMETA(DisplayName = "Ongoing"),
	AttackerVictory UMETA(DisplayName = "Attacker Victory"),
	DefenderVictory UMETA(DisplayName = "Defender Victory"),
	Draw             UMETA(DisplayName = "Draw")
};

USTRUCT(BlueprintType)
struct FWLTacticalUnitState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString TacticalUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString SourceArmyId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString OwnerIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString UnitId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FVector2D MoveTarget = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString AttackTargetUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double Health = 100.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double Morale = 100.0;

	// F1 armas combinadas: una unidad tactica es un CONTINGENTE (grupo de N elementos del mismo
	// tipo: "50 de infanteria", "4 tanques"). Health es el % agregado del contingente; los
	// elementos vivos se derivan de el (las bajas se VEN: la formacion se reduce).
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	int32 ElementCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	int32 InitialElementCount = 1;

	// F3: cadencia de fuego indirecto (segundos hasta la proxima salva de artilleria/naval).
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double IndirectCooldownSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	bool bDestroyed = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	EWLTacticalUnitOrder Order = EWLTacticalUnitOrder::Idle;

	bool IsCombatEffective(int32 RoutMoraleThreshold) const
	{
		return !bDestroyed && Health > 0.0 && Morale > static_cast<double>(RoutMoraleThreshold);
	}
};

USTRUCT(BlueprintType)
struct FWLTacticalObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString ObjectiveId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double Radius = 600.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString ControllerIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double CaptureProgressSeconds = 0.0;
};

// F2: parche circular de terreno. El terreno de un punto = primer parche que lo contiene
// (orden deterministico de creacion); fuera de todo parche el campo es abierto.
USTRUCT(BlueprintType)
struct FWLTacticalTerrainPatch
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString PatchId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	EWLTacticalTerrain Terrain = EWLTacticalTerrain::Open;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double Radius = 500.0;
};

// F3: salva de fuego INDIRECTO en vuelo. Se dispara contra la POSICION del objetivo al
// momento del disparo — mata estaticos, falla contra moviles (que ya no estan alli).
USTRUCT(BlueprintType)
struct FWLTacticalShellState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString ShellId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString OwnerIso;

	/** Id de DATOS del que dispara (artillery/ship): decide canal de dano y contras al impactar. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString SourceUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	int32 Elements = 1;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FVector2D FirePosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FVector2D ImpactPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double FiredAtSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double ImpactAtSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double Radius = 160.0;
};

USTRUCT(BlueprintType)
struct FWLTacticalBattleState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString BattleId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString ProvinceId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString AttackerArmyId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString DefenderArmyId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString AttackerIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString DefenderIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	double ElapsedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	EWLTacticalBattleResult Result = EWLTacticalBattleResult::Ongoing;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	FString WinnerIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	TArray<FString> AIControlledOwnerIsos;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	TArray<FWLTacticalUnitState> Units;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	TArray<FWLTacticalObjectiveState> Objectives;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	TArray<FWLTacticalTerrainPatch> TerrainPatches;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	TArray<FWLTacticalShellState> Shells;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle")
	int32 NextShellNumber = 1;

	bool IsOwnerAIControlled(const FString& OwnerIso) const
	{
		const FString NormalizedOwner = OwnerIso.TrimStartAndEnd().ToUpper();
		return AIControlledOwnerIsos.ContainsByPredicate([&NormalizedOwner](const FString& Candidate)
		{
			return Candidate.Equals(NormalizedOwner, ESearchCase::IgnoreCase);
		});
	}
};
