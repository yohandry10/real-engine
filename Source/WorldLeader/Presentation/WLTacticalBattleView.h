// Copyright World Leader project. See ROADMAP.md.
//
// Vista 3D de BATALLA TACTICA (contrato "Vista Tactica" del roadmap UIX). Actor
// autonomo que renderiza el estado determinista de UWLTacticalBattleSubsystem.
// F1b: cada CONTINGENTE es una FORMACION instanciada (N elementos visibles que
// desaparecen con las bajas), con trazadoras de fuego, restos en el campo y
// encaramiento hacia el objetivo. No decide dano/moral/victoria: solo lee
// FWLTacticalBattleState y traduce clics a coordenadas tacticas.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/WLTacticalBattleTypes.h"
#include "Core/WLGameTypes.h"
#include "WLTacticalBattleView.generated.h"

class ACameraActor;
class ADirectionalLight;
class ASkyLight;
class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;

UCLASS()
class WORLDLEADER_API AWLTacticalBattleView : public AActor
{
	GENERATED_BODY()

public:
	AWLTacticalBattleView();

	/** Prepara el escenario (suelo, camara, luces, objetivos) y las formaciones de contingente. */
	void Initialize(const FWLTacticalBattleState& Battle, const FString& InPlayerIso);

	/** Actualiza formaciones, trazadoras, restos, objetivos y seleccion desde el estado. */
	void RefreshFromState(const FWLTacticalBattleState& Battle);

	ACameraActor* GetBattleCamera() const { return BattleCamera; }

	/** Coordenada tactica (FVector2D del backend) <-> mundo (el suelo esta a Z = GroundZ). */
	FVector TacticalToWorld(const FVector2D& Tactical) const;
	FVector2D WorldToTactical(const FVector& World) const;

	/** Contingente mas cercano a un punto del suelo (mundo) dentro de su radio. Vacio si ninguno. */
	FString FindUnitNearWorldPoint(const FVector& WorldPoint) const;

	/** Resalta el contingente seleccionado (vacio = ninguno). */
	void SetSelectedUnit(const FString& TacticalUnitId);

	double GetGroundZ() const { return GroundZ; }

private:
	/** Forma/espaciado/altura de cada elemento segun el tipo de unidad (tanque, soldado, caza...). */
	struct FElementStyle
	{
		FVector Scale = FVector(1.f, 1.f, 1.f);
		float SpacingCm = 260.f;
		float HoverZCm = 0.f;      // >0 = contingente aereo (flota sobre el campo)
	};

	UMaterialInstanceDynamic* MakeColorMaterial(const FLinearColor& Color);
	FLinearColor ColorForUnit(const FWLTacticalUnitState& Unit) const;
	FElementStyle StyleForUnitId(const FString& UnitId) const;
	/** Offsets locales de la formacion (rejilla ancha centrada, primera fila al frente). */
	static void BuildFormationOffsets(int32 Count, float Spacing, TArray<FVector2D>& OutOffsets);
	void RebuildContingentInstances(UInstancedStaticMeshComponent* Mesh, const FWLTacticalUnitState& Unit);
	void SpawnWrecks(const FWLTacticalUnitState& Unit, const FVector& Center);
	void UpdateTracer(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit);

	UPROPERTY() USceneComponent* Root = nullptr;
	UPROPERTY() UStaticMeshComponent* Ground = nullptr;
	UPROPERTY() ACameraActor* BattleCamera = nullptr;
	UPROPERTY() ADirectionalLight* BattleLight = nullptr;
	UPROPERTY() ASkyLight* BattleSky = nullptr;

	UPROPERTY() UStaticMesh* UnitMesh = nullptr;
	UPROPERTY() UStaticMesh* RingMesh = nullptr;
	UPROPERTY() UStaticMesh* GroundMesh = nullptr;
	UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;

	// F1b: una FORMACION instanciada por contingente (los elementos vivos se ven y caen).
	UPROPERTY() TMap<FString, UInstancedStaticMeshComponent*> ContingentMeshes;
	UPROPERTY() TArray<UStaticMeshComponent*> ObjectiveComponents;
	UPROPERTY() UStaticMeshComponent* SelectionRing = nullptr;
	UPROPERTY() TMap<FString, UStaticMeshComponent*> TracerComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> WreckComponents;

	// Estado de presentacion por contingente (centro, encaramiento, elementos dibujados).
	TMap<FString, FVector> ContingentCenters;
	TMap<FString, float> ContingentYaw;
	TMap<FString, int32> ContingentShownElements;
	TMap<FString, float> ContingentPickRadius;
	TSet<FString> WreckedContingents;
	// Datos de unidad cacheados al iniciar (estilo y alcance para las trazadoras).
	TMap<FString, FWLUnitData> UnitDataById;

	FString PlayerIso;
	FString AttackerIso;
	FString SelectedUnitId;
	double LastElapsedSeconds = 0.0;

	// El campo se dibuja en el origen del mundo (lejos de la geometria de campana proyectada).
	static constexpr double WorldScale = 4.0;    // coord tactica -> cm de mundo
	static constexpr double GroundZ = 0.0;
	static constexpr float UnitPickRadius = 150.f * static_cast<float>(WorldScale);
};
