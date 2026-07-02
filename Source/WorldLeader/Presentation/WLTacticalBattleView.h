// Copyright World Leader project. See ROADMAP.md.
//
// Vista 3D de BATALLA TACTICA (contrato "Vista Tactica" del roadmap UIX). Actor
// autonomo que renderiza el estado determinista de UWLTacticalBattleSubsystem:
// un campo llano, una malla por unidad (coloreada por bando y escalada por salud),
// anillos de objetivo y un anillo de seleccion. No decide dano/moral/victoria:
// solo lee FWLTacticalBattleState y traduce clics a coordenadas tacticas para que
// el PlayerController emita ordenes al backend.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/WLTacticalBattleTypes.h"
#include "WLTacticalBattleView.generated.h"

class ACameraActor;
class ADirectionalLight;
class ASkyLight;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;

UCLASS()
class WORLDLEADER_API AWLTacticalBattleView : public AActor
{
	GENERATED_BODY()

public:
	AWLTacticalBattleView();

	/** Prepara el escenario (suelo, camara, luces, objetivos) y las mallas de unidad. */
	void Initialize(const FWLTacticalBattleState& Battle, const FString& InPlayerIso);

	/** Actualiza posiciones/colores/visibilidad de unidades, objetivos y seleccion desde el estado. */
	void RefreshFromState(const FWLTacticalBattleState& Battle);

	ACameraActor* GetBattleCamera() const { return BattleCamera; }

	/** Coordenada tactica (FVector2D del backend) <-> mundo (el suelo esta a Z = GroundZ). */
	FVector TacticalToWorld(const FVector2D& Tactical) const;
	FVector2D WorldToTactical(const FVector& World) const;

	/** Unidad mas cercana a un punto del suelo (mundo) dentro del radio de seleccion. Vacio si ninguna. */
	FString FindUnitNearWorldPoint(const FVector& WorldPoint) const;

	/** Resalta la unidad seleccionada (vacio = ninguna). */
	void SetSelectedUnit(const FString& TacticalUnitId);

	double GetGroundZ() const { return GroundZ; }

private:
	UMaterialInstanceDynamic* MakeColorMaterial(const FLinearColor& Color);
	FLinearColor ColorForUnit(const FWLTacticalUnitState& Unit) const;

	UPROPERTY() USceneComponent* Root = nullptr;
	UPROPERTY() UStaticMeshComponent* Ground = nullptr;
	UPROPERTY() ACameraActor* BattleCamera = nullptr;
	UPROPERTY() ADirectionalLight* BattleLight = nullptr;
	UPROPERTY() ASkyLight* BattleSky = nullptr;

	UPROPERTY() UStaticMesh* UnitMesh = nullptr;
	UPROPERTY() UStaticMesh* RingMesh = nullptr;
	UPROPERTY() UStaticMesh* GroundMesh = nullptr;
	UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;

	UPROPERTY() TMap<FString, UStaticMeshComponent*> UnitComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> ObjectiveComponents;
	UPROPERTY() UStaticMeshComponent* SelectionRing = nullptr;

	FString PlayerIso;
	FString SelectedUnitId;

	// El campo se dibuja en el origen del mundo (lejos de la geometria de campana proyectada).
	static constexpr double WorldScale = 4.0;    // coord tactica -> cm de mundo
	static constexpr double GroundZ = 0.0;
	static constexpr float UnitPickRadius = 150.f * static_cast<float>(WorldScale);
};
