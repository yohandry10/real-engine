// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLTacticalBattleView.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AWLTacticalBattleView::AWLTacticalBattleView()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded()) { BaseMaterial = MatFinder.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> UnitFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (UnitFinder.Succeeded()) { UnitMesh = UnitFinder.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (RingFinder.Succeeded()) { RingMesh = RingFinder.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded()) { GroundMesh = PlaneFinder.Object; }
}

UMaterialInstanceDynamic* AWLTacticalBattleView::MakeColorMaterial(const FLinearColor& Color)
{
	if (!BaseMaterial)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Mat->SetVectorParameterValue(TEXT("Base Color"), Color);
	}
	return Mat;
}

FVector AWLTacticalBattleView::TacticalToWorld(const FVector2D& Tactical) const
{
	return FVector(Tactical.X * WorldScale, Tactical.Y * WorldScale, GroundZ);
}

FVector2D AWLTacticalBattleView::WorldToTactical(const FVector& World) const
{
	return FVector2D(World.X / WorldScale, World.Y / WorldScale);
}

FLinearColor AWLTacticalBattleView::ColorForUnit(const FWLTacticalUnitState& Unit) const
{
	// Bando del jugador en tonos calidos (dorado/verde), enemigo en rojo. La salud baja apaga el color.
	const bool bPlayer = Unit.OwnerIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
	const bool bRouting = Unit.Order == EWLTacticalUnitOrder::Routing || Unit.Morale <= 25.0;
	FLinearColor Base = bPlayer
		? (bRouting ? FLinearColor(0.55f, 0.50f, 0.22f) : FLinearColor(0.30f, 0.62f, 0.95f))
		: (bRouting ? FLinearColor(0.55f, 0.30f, 0.24f) : FLinearColor(0.92f, 0.28f, 0.22f));
	const float HealthT = FMath::Clamp(static_cast<float>(Unit.Health) / 100.f, 0.25f, 1.f);
	return Base * HealthT;
}

void AWLTacticalBattleView::Initialize(const FWLTacticalBattleState& Battle, const FString& InPlayerIso)
{
	PlayerIso = InPlayerIso.TrimStartAndEnd().ToUpper();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Suelo: plano llano centrado en el origen (el plano del Engine mide 100x100 -> escalar a metros).
	if (GroundMesh)
	{
		Ground = NewObject<UStaticMeshComponent>(this);
		Ground->SetupAttachment(Root);
		Ground->RegisterComponent();
		Ground->SetStaticMesh(GroundMesh);
		Ground->SetWorldLocation(FVector(0.f, 0.f, GroundZ - 2.f));
		Ground->SetWorldScale3D(FVector(220.f, 220.f, 1.f));   // ~220m de lado
		Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(FLinearColor(0.14f, 0.22f, 0.13f)))
		{
			Ground->SetMaterial(0, Mat);
		}
	}

	// Anillo de seleccion (oculto hasta seleccionar).
	if (RingMesh)
	{
		SelectionRing = NewObject<UStaticMeshComponent>(this);
		SelectionRing->SetupAttachment(Root);
		SelectionRing->RegisterComponent();
		SelectionRing->SetStaticMesh(RingMesh);
		SelectionRing->SetWorldScale3D(FVector(2.6f, 2.6f, 0.06f));
		SelectionRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SelectionRing->SetVisibility(false);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(FLinearColor(1.0f, 0.86f, 0.3f)))
		{
			SelectionRing->SetMaterial(0, Mat);
		}
	}

	// Objetivos: un anillo por punto de control.
	for (const FWLTacticalObjectiveState& Objective : Battle.Objectives)
	{
		if (!RingMesh)
		{
			break;
		}
		UStaticMeshComponent* Ring = NewObject<UStaticMeshComponent>(this);
		Ring->SetupAttachment(Root);
		Ring->RegisterComponent();
		Ring->SetStaticMesh(RingMesh);
		const FVector Loc = TacticalToWorld(Objective.Position) + FVector(0.f, 0.f, 4.f);
		Ring->SetWorldLocation(Loc);
		const float RingScale = static_cast<float>(Objective.Radius * WorldScale) / 50.f;   // cilindro base = 100cm diam
		Ring->SetWorldScale3D(FVector(RingScale, RingScale, 0.04f));
		Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(FLinearColor(0.85f, 0.82f, 0.30f)))
		{
			Ring->SetMaterial(0, Mat);
		}
		ObjectiveComponents.Add(Ring);
	}

	// Una malla (cubo) por unidad.
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (!UnitMesh)
		{
			break;
		}
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetupAttachment(Root);
		Comp->RegisterComponent();
		Comp->SetStaticMesh(UnitMesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(ColorForUnit(Unit)))
		{
			Comp->SetMaterial(0, Mat);
		}
		UnitComponents.Add(Unit.TacticalUnitId, Comp);
	}

	// Luces propias: la batalla se ve igual aunque la escena de campana se apague.
	BattleLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
		FVector::ZeroVector, FRotator(-52.f, 35.f, 0.f));
	if (BattleLight && BattleLight->GetComponent())
	{
		BattleLight->GetComponent()->SetIntensity(3.2f);
	}
	BattleSky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass());
	if (BattleSky && BattleSky->GetLightComponent())
	{
		BattleSky->GetLightComponent()->SetIntensity(1.1f);
	}

	// Camara: por encima del campo, mirando hacia abajo desde el lado del jugador.
	const bool bPlayerIsAttacker = Battle.AttackerIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
	const float SideSign = bPlayerIsAttacker ? -1.f : 1.f;   // atacante empieza a -X
	const FVector CamLoc(SideSign * 9000.f, 0.f, 8200.f);
	const FRotator CamRot((SideSign < 0.f ? -46.f : -46.f), (SideSign < 0.f ? 0.f : 180.f), 0.f);
	BattleCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CamLoc, CamRot);
	if (BattleCamera && BattleCamera->GetCameraComponent())
	{
		BattleCamera->GetCameraComponent()->SetFieldOfView(52.f);
	}

	RefreshFromState(Battle);
}

void AWLTacticalBattleView::RefreshFromState(const FWLTacticalBattleState& Battle)
{
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		UStaticMeshComponent** Found = UnitComponents.Find(Unit.TacticalUnitId);
		if (!Found || !*Found)
		{
			continue;
		}
		UStaticMeshComponent* Comp = *Found;
		if (Unit.bDestroyed || Unit.Health <= 0.0)
		{
			Comp->SetVisibility(false);
			continue;
		}
		Comp->SetVisibility(true);
		// Cubo base = 100cm; el token mide ~2m y se estira un poco con la salud para dar sensacion de "bloque".
		const float Health = FMath::Clamp(static_cast<float>(Unit.Health) / 100.f, 0.3f, 1.f);
		Comp->SetWorldScale3D(FVector(1.7f, 1.7f, 1.7f + Health * 1.6f));
		FVector Loc = TacticalToWorld(Unit.Position);
		Loc.Z = GroundZ + (1.7f + Health * 1.6f) * 50.f;   // apoya la base en el suelo
		Comp->SetWorldLocation(Loc);
		if (UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0)))
		{
			const FLinearColor Color = ColorForUnit(Unit);
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			Mat->SetVectorParameterValue(TEXT("Base Color"), Color);
		}
	}

	// Objetivos: color por controlador (jugador/enemigo/neutral).
	for (int32 i = 0; i < ObjectiveComponents.Num() && i < Battle.Objectives.Num(); ++i)
	{
		if (!ObjectiveComponents[i])
		{
			continue;
		}
		const FWLTacticalObjectiveState& Obj = Battle.Objectives[i];
		FLinearColor Color(0.72f, 0.70f, 0.28f);   // neutral
		if (!Obj.ControllerIso.IsEmpty())
		{
			Color = Obj.ControllerIso.Equals(PlayerIso, ESearchCase::IgnoreCase)
				? FLinearColor(0.30f, 0.70f, 0.95f)
				: FLinearColor(0.92f, 0.30f, 0.24f);
		}
		if (UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(ObjectiveComponents[i]->GetMaterial(0)))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			Mat->SetVectorParameterValue(TEXT("Base Color"), Color);
		}
	}

	// Anillo de seleccion sigue a su unidad (si sigue viva).
	if (SelectionRing)
	{
		const FWLTacticalUnitState* Sel = Battle.Units.FindByPredicate([this](const FWLTacticalUnitState& U)
		{
			return U.TacticalUnitId == SelectedUnitId && !U.bDestroyed && U.Health > 0.0;
		});
		if (Sel)
		{
			FVector Loc = TacticalToWorld(Sel->Position);
			Loc.Z = GroundZ + 6.f;
			SelectionRing->SetWorldLocation(Loc);
			SelectionRing->SetVisibility(true);
		}
		else
		{
			SelectionRing->SetVisibility(false);
		}
	}
}

FString AWLTacticalBattleView::FindUnitNearWorldPoint(const FVector& WorldPoint) const
{
	FString Best;
	float BestDistSq = UnitPickRadius * UnitPickRadius;
	for (const TPair<FString, UStaticMeshComponent*>& Pair : UnitComponents)
	{
		if (!Pair.Value || !Pair.Value->IsVisible())
		{
			continue;
		}
		const FVector Loc = Pair.Value->GetComponentLocation();
		const float DistSq = FVector2D::DistSquared(
			FVector2D(Loc.X, Loc.Y), FVector2D(WorldPoint.X, WorldPoint.Y));
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Pair.Key;
		}
	}
	return Best;
}

void AWLTacticalBattleView::SetSelectedUnit(const FString& TacticalUnitId)
{
	SelectedUnitId = TacticalUnitId;
	if (SelectionRing && TacticalUnitId.IsEmpty())
	{
		SelectionRing->SetVisibility(false);
	}
}
