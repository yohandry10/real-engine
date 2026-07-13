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
class AExponentialHeightFog;
class ASkyLight;
class ASkyAtmosphere;
class AVolumetricCloud;
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Prepara el escenario (suelo, camara, luces, objetivos) y las formaciones de contingente. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	void Initialize(const FWLTacticalBattleState& Battle, const FString& InPlayerIso);

	/** Actualiza formaciones, trazadoras, restos, objetivos y seleccion desde el estado. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
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
		FVector Scale = FVector(1.f, 1.f, 1.f);   // escala del CUBO de reserva (sin modelo)
		float SpacingCm = 260.f;
		float HoverZCm = 0.f;      // >0 = contingente aereo (flota sobre el campo)
		// F6: tamano objetivo del MODELO real (largo en X; alto si bScaleByHeight) -> escala por bounds.
		float TargetSizeCm = 0.f;
		bool bScaleByHeight = false;
	};

	UMaterialInstanceDynamic* MakeColorMaterial(const FLinearColor& Color);
	FLinearColor ColorForUnit(const FWLTacticalUnitState& Unit) const;
	FElementStyle StyleForUnitId(const FString& UnitId) const;
	FString UnitKind(const FWLTacticalUnitState& Unit) const;
	bool IsInfantryUnit(const FWLTacticalUnitState& Unit) const;
	/** F6: modelo low-poly real del contingente (/Game/GenVehicle): camo verde propio, desierto enemigo. */
	UStaticMesh* ModelForUnit(const FWLTacticalUnitState& Unit) const;
	UStaticMesh* WreckForUnit(const FWLTacticalUnitState& Unit) const;
	UStaticMesh* BannerForUnit(const FWLTacticalUnitState& Unit) const;
	/** F6: humo en contingentes danados y fogonazos de impacto de salvas. */
	void UpdateBattleEffects(const FWLTacticalBattleState& Battle);
	/** Sprites de combate (Codex, Content/UI/Battle) como billboards que encaran la camara. */
	UTexture2D* LoadBattleSprite(const FString& Name) const;
	UStaticMeshComponent* MakeSpriteBillboard();
	void UpdateSpriteBillboard(UStaticMeshComponent* Comp, const FString& Sprite, const FLinearColor& Tint, float Opacity, const FVector& WorldPos, float SizeCm);
	/** Offsets locales de la formacion (rejilla ancha centrada, primera fila al frente). */
	static void BuildFormationOffsets(int32 Count, float Spacing, TArray<FVector2D>& OutOffsets);
	void RebuildContingentInstances(const FWLTacticalUnitState& Unit);
	float BattlefieldHeightCmAtWorld(const FVector2D& WorldXY) const;
	UStaticMeshComponent* SpawnBattleAsset(UStaticMesh* Mesh, const FVector& WorldLocation,
		const FRotator& WorldRotation, const FVector& AssetScale, TArray<UStaticMeshComponent*>& Bucket);
	/** Densidad del campo abierto, parches tacticos y fortificaciones de objetivos. */
	void BuildFieldProps(const FWLTacticalBattleState& Battle);
	void BuildTerrainPatches(const FWLTacticalBattleState& Battle);
	void BuildObjectiveFortifications(const FWLTacticalBattleState& Battle);
	void SpawnWrecks(const FWLTacticalUnitState& Unit, const FVector& Center);
	void UpdateTracer(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit);
	/** F3: salvas indirectas en vuelo (arco balistico) y crater al impactar. */
	void UpdateShells(const FWLTacticalBattleState& Battle);

	UPROPERTY() USceneComponent* Root = nullptr;
	UPROPERTY() UStaticMeshComponent* Ground = nullptr;
	UPROPERTY() ACameraActor* BattleCamera = nullptr;
	UPROPERTY() ADirectionalLight* BattleLight = nullptr;
	UPROPERTY() ASkyLight* BattleSky = nullptr;
	UPROPERTY() ASkyAtmosphere* BattleAtmosphere = nullptr;
	UPROPERTY() AVolumetricCloud* BattleClouds = nullptr;
	UPROPERTY() AExponentialHeightFog* BattleFog = nullptr;

	// Primitivas tecnicas para VFX/seleccion; nunca se usan como fallback visual de contenido.
	UPROPERTY() UStaticMesh* UtilityCubeMesh = nullptr;
	UPROPERTY() UStaticMesh* RingMesh = nullptr;
	UPROPERTY() UStaticMesh* SphereMesh = nullptr;
	UPROPERTY() UStaticMesh* BillboardPlaneMesh = nullptr;
	UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;
	// F6: modelos de unidad reales (gen_vehicle.py) + material unlit vertex-color compartido.
	UPROPERTY() TMap<FString, UStaticMesh*> UnitModels;
	UPROPERTY() UMaterialInterface* VehicleMaterial = nullptr;
	// Paquete /Game/GenBattle: 38 mallas tacticas y material vertex-color opaco.
	UPROPERTY() UStaticMesh* BattlefieldMesh = nullptr;
	UPROPERTY() UMaterialInterface* BattleMaterial = nullptr;
	UPROPERTY() TArray<UStaticMesh*> UrbanMeshes;
	UPROPERTY() TArray<UStaticMesh*> NatureMeshes;
	UPROPERTY() TArray<UStaticMesh*> FortificationMeshes;
	UPROPERTY() TArray<UStaticMesh*> FieldPropMeshes;
	UPROPERTY() TMap<FString, UStaticMesh*> WreckModels;
	UPROPERTY() TMap<FString, UStaticMesh*> SoldierPoseModels;
	UPROPERTY() TMap<FString, UStaticMesh*> BannerModels;
	UPROPERTY() UStaticMesh* CraterMesh = nullptr;
	// Sprites de combate: material unlit translucido texturizado (/Game/UI/Battle/M_BattleSprite).
	UPROPERTY() UMaterialInterface* SpriteMaterial = nullptr;

	// F1b: una FORMACION instanciada por contingente (los elementos vivos se ven y caen).
	UPROPERTY() TMap<FString, UInstancedStaticMeshComponent*> ContingentMeshes;
	UPROPERTY() TMap<FString, UInstancedStaticMeshComponent*> KneelingContingentMeshes;
	UPROPERTY() TMap<FString, UInstancedStaticMeshComponent*> ProneContingentMeshes;
	UPROPERTY() TMap<FString, UStaticMeshComponent*> BannerComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> ObjectiveComponents;
	UPROPERTY() UStaticMeshComponent* SelectionRing = nullptr;
	UPROPERTY() TMap<FString, UStaticMeshComponent*> TracerComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> WreckComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> TerrainComponents;
	UPROPERTY() TMap<FString, UStaticMeshComponent*> ShellComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> ScorchComponents;
	// F6: VFX baratos — humo por contingente danado y fogonazos de impacto con vida corta.
	UPROPERTY() TMap<FString, UStaticMeshComponent*> SmokeComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> FlashComponents;
	TArray<double> FlashSpawnSeconds;
	UPROPERTY() TArray<UStaticMeshComponent*> ImpactComponents;
	TArray<double> ImpactSpawnSeconds;

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
