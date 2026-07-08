// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLTacticalBattleView.h"
#include "Battle/WLTacticalBattleSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/WLDataRegistry.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/WLGovAssets.h"
#include "Engine/Texture2D.h"
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded()) { SphereMesh = SphereFinder.Object; }

	// F6: modelos low-poly reales (gen_vehicle.py -> /Game/GenVehicle), unlit vertex color.
	// Bando del jugador = camo verde; enemigo = desierto. Caza y buque son neutros (gris).
	// Si falta un asset, el contingente cae al cubo de reserva (degradacion elegante).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MSoldier(TEXT("/Game/GenVehicle/veh_soldier.veh_soldier"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MApc(TEXT("/Game/GenVehicle/veh_apc.veh_apc"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MIfv(TEXT("/Game/GenVehicle/veh_ifv.veh_ifv"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MMbt(TEXT("/Game/GenVehicle/veh_mbt.veh_mbt"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MArty(TEXT("/Game/GenVehicle/veh_artillery.veh_artillery"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MSam(TEXT("/Game/GenVehicle/veh_sam.veh_sam"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MHeli(TEXT("/Game/GenVehicle/veh_heli.veh_heli"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MAir(TEXT("/Game/GenVehicle/veh_aircraft.veh_aircraft"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MShip(TEXT("/Game/GenVehicle/veh_ship.veh_ship"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MSoldierD(TEXT("/Game/GenVehicle/veh_soldier_desert.veh_soldier_desert"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MApcD(TEXT("/Game/GenVehicle/veh_apc_desert.veh_apc_desert"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MIfvD(TEXT("/Game/GenVehicle/veh_ifv_desert.veh_ifv_desert"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MMbtD(TEXT("/Game/GenVehicle/veh_mbt_desert.veh_mbt_desert"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MArtyD(TEXT("/Game/GenVehicle/veh_artillery_desert.veh_artillery_desert"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MSamD(TEXT("/Game/GenVehicle/veh_sam_desert.veh_sam_desert"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MHeliD(TEXT("/Game/GenVehicle/veh_heli_desert.veh_heli_desert"));
	if (MSoldier.Succeeded()) { UnitModels.Add(TEXT("soldier"), MSoldier.Object); }
	if (MApc.Succeeded())     { UnitModels.Add(TEXT("apc"), MApc.Object); }
	if (MIfv.Succeeded())     { UnitModels.Add(TEXT("ifv"), MIfv.Object); }
	if (MMbt.Succeeded())     { UnitModels.Add(TEXT("mbt"), MMbt.Object); }
	if (MArty.Succeeded())    { UnitModels.Add(TEXT("artillery"), MArty.Object); }
	if (MSam.Succeeded())     { UnitModels.Add(TEXT("sam"), MSam.Object); }
	if (MHeli.Succeeded())    { UnitModels.Add(TEXT("heli"), MHeli.Object); }
	if (MAir.Succeeded())     { UnitModels.Add(TEXT("aircraft"), MAir.Object); }
	if (MShip.Succeeded())    { UnitModels.Add(TEXT("ship"), MShip.Object); }
	if (MSoldierD.Succeeded()) { UnitModels.Add(TEXT("soldier_desert"), MSoldierD.Object); }
	if (MApcD.Succeeded())     { UnitModels.Add(TEXT("apc_desert"), MApcD.Object); }
	if (MIfvD.Succeeded())     { UnitModels.Add(TEXT("ifv_desert"), MIfvD.Object); }
	if (MMbtD.Succeeded())     { UnitModels.Add(TEXT("mbt_desert"), MMbtD.Object); }
	if (MArtyD.Succeeded())    { UnitModels.Add(TEXT("artillery_desert"), MArtyD.Object); }
	if (MSamD.Succeeded())     { UnitModels.Add(TEXT("sam_desert"), MSamD.Object); }
	if (MHeliD.Succeeded())    { UnitModels.Add(TEXT("heli_desert"), MHeliD.Object); }

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VehMatFinder(TEXT("/Game/GenVehicle/M_VehicleUnlit.M_VehicleUnlit"));
	if (VehMatFinder.Succeeded()) { VehicleMaterial = VehMatFinder.Object; }

	// Material de sprites de combate (unlit translucido texturizado, creado por create_battle_material.py).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SpriteMatFinder(TEXT("/Game/UI/Battle/M_BattleSprite.M_BattleSprite"));
	if (SpriteMatFinder.Succeeded()) { SpriteMaterial = SpriteMatFinder.Object; }
}

UTexture2D* AWLTacticalBattleView::LoadBattleSprite(const FString& Name) const
{
	return WLGovAssetsNS::LoadExternalTexture(FString::Printf(TEXT("UI/Battle/%s.png"), *Name));
}

UStaticMeshComponent* AWLTacticalBattleView::MakeSpriteBillboard()
{
	if (!GroundMesh || !SpriteMaterial)
	{
		return nullptr;
	}
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetupAttachment(Root);
	Comp->RegisterComponent();
	Comp->SetStaticMesh(GroundMesh);   // el Plane 100x100 del Engine, orientado a la camara por frame
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCastShadow(false);
	if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(SpriteMaterial, this))
	{
		Comp->SetMaterial(0, Mid);
	}
	return Comp;
}

void AWLTacticalBattleView::UpdateSpriteBillboard(UStaticMeshComponent* Comp, const FString& Sprite,
	const FLinearColor& Tint, float Opacity, const FVector& WorldPos, float SizeCm)
{
	if (!Comp)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0)))
	{
		if (UTexture2D* Tex = LoadBattleSprite(Sprite))
		{
			Mid->SetTextureParameterValue(TEXT("Sprite"), Tex);
		}
		Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
		Mid->SetScalarParameterValue(TEXT("OpacityScale"), Opacity);
	}
	Comp->SetWorldLocation(WorldPos);
	const float S = FMath::Max(0.01f, SizeCm) / 100.f;
	Comp->SetWorldScale3D(FVector(S, S, S));
	// Encara la camara: el Plane mira a +Z local; alinear +Z con la direccion hacia la camara.
	FVector ToCamera(0.f, 0.f, 1.f);
	if (BattleCamera)
	{
		const FVector Dir = BattleCamera->GetActorLocation() - WorldPos;
		if (!Dir.IsNearlyZero())
		{
			ToCamera = Dir.GetSafeNormal();
		}
	}
	Comp->SetWorldRotation(FRotationMatrix::MakeFromZ(ToCamera).Rotator());
	Comp->SetVisibility(true);
}

UStaticMesh* AWLTacticalBattleView::ModelForUnit(const FWLTacticalUnitState& Unit) const
{
	const FString Id = Unit.UnitId.ToLower();
	FString Kind;
	if (Id == TEXT("infantry"))                              { Kind = TEXT("soldier"); }
	else if (Id == TEXT("apc"))                              { Kind = TEXT("apc"); }
	else if (Id == TEXT("ifv"))                              { Kind = TEXT("ifv"); }
	else if (Id == TEXT("mbt") || Id == TEXT("tank"))        { Kind = TEXT("mbt"); }
	else if (Id == TEXT("artillery"))                        { Kind = TEXT("artillery"); }
	else if (Id == TEXT("sam"))                              { Kind = TEXT("sam"); }
	else if (Id == TEXT("heli"))                             { Kind = TEXT("heli"); }
	else if (Id == TEXT("aircraft") || Id == TEXT("drone"))  { Kind = TEXT("aircraft"); }
	else if (Id == TEXT("ship"))                             { Kind = TEXT("ship"); }
	else                                                     { Kind = TEXT("soldier"); }

	const bool bPlayer = Unit.OwnerIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
	if (!bPlayer && Kind != TEXT("aircraft") && Kind != TEXT("ship"))
	{
		if (UStaticMesh* Desert = UnitModels.FindRef(Kind + TEXT("_desert")))
		{
			return Desert;
		}
	}
	return UnitModels.FindRef(Kind);
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
	// Bando del jugador en azul, enemigo en rojo. Derrota/moral rota apaga el color.
	const bool bPlayer = Unit.OwnerIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
	const bool bRouting = Unit.Order == EWLTacticalUnitOrder::Routing || Unit.Morale <= 25.0;
	FLinearColor Base = bPlayer
		? (bRouting ? FLinearColor(0.55f, 0.50f, 0.22f) : FLinearColor(0.30f, 0.62f, 0.95f))
		: (bRouting ? FLinearColor(0.55f, 0.30f, 0.24f) : FLinearColor(0.92f, 0.28f, 0.22f));
	const float HealthT = FMath::Clamp(static_cast<float>(Unit.Health) / 100.f, 0.35f, 1.f);
	return Base * HealthT;
}

AWLTacticalBattleView::FElementStyle AWLTacticalBattleView::StyleForUnitId(const FString& UnitId) const
{
	FElementStyle Style;
	EWLUnitType Type = EWLUnitType::Infantry;
	if (const FWLUnitData* Data = UnitDataById.Find(UnitId.ToLower()))
	{
		Type = Data->Type;
	}
	switch (Type)
	{
	case EWLUnitType::Armor:
		Style.Scale = FVector(1.9f, 1.15f, 0.85f); Style.SpacingCm = 460.f; Style.TargetSizeCm = 400.f; break;
	case EWLUnitType::LightVehicle:
	case EWLUnitType::Drone:
		Style.Scale = FVector(1.5f, 0.95f, 0.75f); Style.SpacingCm = 410.f; Style.TargetSizeCm = 350.f; break;
	case EWLUnitType::Artillery:
		Style.Scale = FVector(1.7f, 1.00f, 0.80f); Style.SpacingCm = 480.f; Style.TargetSizeCm = 430.f; break;
	case EWLUnitType::AirDefense:
		Style.Scale = FVector(1.3f, 1.30f, 1.00f); Style.SpacingCm = 450.f; Style.TargetSizeCm = 390.f; break;
	case EWLUnitType::Air:
		Style.Scale = FVector(1.9f, 1.40f, 0.45f); Style.SpacingCm = 620.f; Style.TargetSizeCm = 540.f; Style.HoverZCm = 900.f; break;
	case EWLUnitType::Naval:
		Style.Scale = FVector(3.2f, 1.10f, 0.90f); Style.SpacingCm = 1050.f; Style.TargetSizeCm = 950.f; break;
	case EWLUnitType::Infantry:
	case EWLUnitType::SpecialForces:
	default:
		Style.Scale = FVector(0.42f, 0.42f, 1.05f); Style.SpacingCm = 115.f; Style.TargetSizeCm = 110.f; Style.bScaleByHeight = true; break;
	}
	return Style;
}

void AWLTacticalBattleView::BuildFormationOffsets(int32 Count, float Spacing, TArray<FVector2D>& OutOffsets)
{
	// Rejilla ANCHA (mas columnas que filas, como una linea de batalla), centrada en el
	// contingente. Local: +X = frente, +Y = derecha. Al morir elementos desaparecen las
	// ultimas posiciones (las filas traseras se vacian primero).
	OutOffsets.Reset();
	if (Count <= 0)
	{
		return;
	}
	const int32 Cols = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Count) * 2.4f)));
	const int32 Rows = FMath::Max(1, FMath::DivideAndRoundUp(Count, Cols));
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Row = i / Cols;
		const int32 Col = i % Cols;
		const int32 ColsInRow = (Row == Rows - 1) ? (Count - Row * Cols) : Cols;
		const float Y = (static_cast<float>(Col) - static_cast<float>(ColsInRow - 1) * 0.5f) * Spacing;
		const float X = -static_cast<float>(Row) * Spacing * 0.95f;   // filas hacia atras
		OutOffsets.Add(FVector2D(X, Y));
	}
}

void AWLTacticalBattleView::RebuildContingentInstances(UInstancedStaticMeshComponent* Mesh, const FWLTacticalUnitState& Unit)
{
	const FElementStyle Style = StyleForUnitId(Unit.UnitId);
	TArray<FVector2D> Offsets;
	BuildFormationOffsets(Unit.ElementCount, Style.SpacingCm, Offsets);

	// F6: con modelo real la escala sale de sus bounds hacia el tamano objetivo del tipo
	// (largo X para vehiculos, alto para el soldado); el origen del modelo es z=0 = suelo.
	FVector InstanceScale = Style.Scale;
	float BaseZ = Style.Scale.Z * 50.f;   // cubo de reserva: centro apoyado
	const UStaticMesh* Model = Mesh->GetStaticMesh();
	if (Model && Model != UnitMesh)
	{
		const FBoxSphereBounds Bounds = Model->GetBounds();
		const float ModelSize = 2.f * (Style.bScaleByHeight ? Bounds.BoxExtent.Z : Bounds.BoxExtent.X);
		const float Uniform = Style.TargetSizeCm / FMath::Max(1.f, ModelSize);
		InstanceScale = FVector(Uniform);
		BaseZ = -(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Uniform;   // punto mas bajo al suelo
	}

	Mesh->ClearInstances();
	for (const FVector2D& Offset : Offsets)
	{
		FTransform Xform;
		Xform.SetScale3D(InstanceScale);
		Xform.SetLocation(FVector(Offset.X, Offset.Y, BaseZ));
		Mesh->AddInstance(Xform);
	}
	ContingentShownElements.Add(Unit.TacticalUnitId, Unit.ElementCount);
}

void AWLTacticalBattleView::BuildTerrainPatches(const FWLTacticalBattleState& Battle)
{
	if (!RingMesh || !UnitMesh)
	{
		return;
	}

	for (const FWLTacticalTerrainPatch& Patch : Battle.TerrainPatches)
	{
		const bool bUrban = Patch.Terrain == EWLTacticalTerrain::Urban;

		// Disco apenas sobre el suelo: la zona se LEE desde la camara (asfalto gris / sotobosque).
		UStaticMeshComponent* Disc = NewObject<UStaticMeshComponent>(this);
		Disc->SetupAttachment(Root);
		Disc->RegisterComponent();
		Disc->SetStaticMesh(RingMesh);
		Disc->SetWorldLocation(TacticalToWorld(Patch.Position) + FVector(0.f, 0.f, 1.5f));
		const float DiscScale = static_cast<float>(Patch.Radius * WorldScale) / 50.f;
		Disc->SetWorldScale3D(FVector(DiscScale, DiscScale, 0.02f));
		Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(
			bUrban ? FLinearColor(0.115f, 0.112f, 0.118f) : FLinearColor(0.05f, 0.115f, 0.045f)))
		{
			Disc->SetMaterial(0, Mat);
		}
		TerrainComponents.Add(Disc);

		// Props dispersos deterministas: bloques grises (edificios) o pilares verdes (arboles).
		const int32 PropCount = bUrban ? 10 : 16;
		const uint32 Hash = GetTypeHash(Patch.PatchId);
		const float MaxOffset = static_cast<float>(Patch.Radius * WorldScale) * 0.72f;
		const FVector PatchCenter = TacticalToWorld(Patch.Position);
		for (int32 i = 0; i < PropCount; ++i)
		{
			const uint32 Seed = Hash + static_cast<uint32>(i) * 2654435761u;
			UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(this);
			Prop->SetupAttachment(Root);
			Prop->RegisterComponent();
			Prop->SetStaticMesh(UnitMesh);
			Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			const float OffX = (static_cast<float>(Seed % 1000) / 500.f - 1.f) * MaxOffset;
			const float OffY = (static_cast<float>((Seed / 1000u) % 1000) / 500.f - 1.f) * MaxOffset;
			FVector Scale;
			FLinearColor Color;
			if (bUrban)
			{
				Scale = FVector(
					1.6f + static_cast<float>((Seed / 7u) % 17) * 0.1f,
					1.6f + static_cast<float>((Seed / 11u) % 17) * 0.1f,
					2.5f + static_cast<float>((Seed / 13u) % 36) * 0.1f);
				const float Tint = 0.30f + static_cast<float>((Seed / 17u) % 8) * 0.01f;
				Color = FLinearColor(Tint, Tint + 0.01f, Tint + 0.03f);
			}
			else
			{
				Scale = FVector(
					0.7f + static_cast<float>((Seed / 7u) % 4) * 0.1f,
					0.7f + static_cast<float>((Seed / 11u) % 4) * 0.1f,
					2.0f + static_cast<float>((Seed / 13u) % 15) * 0.1f);
				Color = FLinearColor(0.055f, 0.16f + static_cast<float>((Seed / 17u) % 6) * 0.008f, 0.05f);
			}
			Prop->SetWorldLocation(PatchCenter + FVector(OffX, OffY, Scale.Z * 50.f));
			Prop->SetWorldRotation(FRotator(0.f, static_cast<float>(Seed % 90), 0.f));
			Prop->SetWorldScale3D(Scale);
			if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(Color))
			{
				Prop->SetMaterial(0, Mat);
			}
			TerrainComponents.Add(Prop);
		}
	}
}

void AWLTacticalBattleView::SpawnWrecks(const FWLTacticalUnitState& Unit, const FVector& Center)
{
	if (WreckedContingents.Contains(Unit.TacticalUnitId) || !UnitMesh)
	{
		return;
	}
	WreckedContingents.Add(Unit.TacticalUnitId);

	// Restos oscuros y ladeados: el campo cuenta la historia de la batalla.
	const FElementStyle Style = StyleForUnitId(Unit.UnitId);
	const int32 WreckCount = FMath::Clamp(Unit.InitialElementCount, 1, 5);
	const uint32 Hash = GetTypeHash(Unit.TacticalUnitId);
	for (int32 i = 0; i < WreckCount; ++i)
	{
		UStaticMeshComponent* Wreck = NewObject<UStaticMeshComponent>(this);
		Wreck->SetupAttachment(Root);
		Wreck->RegisterComponent();
		Wreck->SetStaticMesh(UnitMesh);
		Wreck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		const float OffX = static_cast<float>(((Hash >> (i * 3)) % 7)) * 90.f - 270.f;
		const float OffY = static_cast<float>(((Hash >> (i * 5)) % 9)) * 90.f - 360.f;
		const float Yaw = static_cast<float>((Hash >> (i * 2)) % 360);
		FVector Loc = Center + FVector(OffX, OffY, 0.f);
		Loc.Z = GroundZ + Style.Scale.Z * 26.f;   // medio hundido: chatarra, no unidad viva
		Wreck->SetWorldLocation(Loc);
		Wreck->SetWorldRotation(FRotator(0.f, Yaw, 8.f));
		Wreck->SetWorldScale3D(Style.Scale * 0.92f);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(FLinearColor(0.055f, 0.048f, 0.042f)))
		{
			Wreck->SetMaterial(0, Mat);
		}
		WreckComponents.Add(Wreck);
	}
}

void AWLTacticalBattleView::UpdateTracer(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit)
{
	UStaticMeshComponent** FoundTracer = TracerComponents.Find(Unit.TacticalUnitId);
	UStaticMeshComponent* Tracer = FoundTracer ? *FoundTracer : nullptr;

	auto HideTracer = [&Tracer]()
	{
		if (Tracer) { Tracer->SetVisibility(false); }
	};

	if (Unit.bDestroyed || Unit.Order != EWLTacticalUnitOrder::Attacking || Unit.AttackTargetUnitId.IsEmpty())
	{
		HideTracer();
		return;
	}
	const FWLTacticalUnitState* Target = Battle.Units.FindByPredicate([&Unit](const FWLTacticalUnitState& U)
	{
		return U.TacticalUnitId == Unit.AttackTargetUnitId && !U.bDestroyed && U.Health > 0.0;
	});
	if (!Target)
	{
		HideTracer();
		return;
	}

	// Solo dispara (y traza) dentro de su alcance — el mismo criterio que el backend,
	// incluido el recorte F2 por cobertura (sin fuego directo lejano contra urbano/bosque).
	double Range = 1200.0;
	if (const FWLUnitData* Data = UnitDataById.Find(Unit.UnitId.ToLower()))
	{
		if (Data->RangeUnits > 0.0) { Range = Data->RangeUnits; }
		if (Data->Type != EWLUnitType::Artillery && Data->Type != EWLUnitType::Naval)
		{
			Range = FMath::Min(Range, UWLTacticalBattleSubsystem::GetCoverEngageRange(
				UWLTacticalBattleSubsystem::TerrainAtPosition(Battle, Target->Position)));
		}
	}
	const double Distance = FVector2D::Distance(Unit.Position, Target->Position);
	if (Distance > Range)
	{
		HideTracer();
		return;
	}

	// Parpadeo: rafagas, no un laser continuo. Fase estable por contingente.
	const double Phase = static_cast<double>(GetTypeHash(Unit.TacticalUnitId) % 100) / 100.0;
	if (FMath::Fmod(Battle.ElapsedSeconds * 2.6 + Phase, 1.0) > 0.62)
	{
		HideTracer();
		return;
	}

	if (!Tracer)
	{
		if (!UnitMesh)
		{
			return;
		}
		Tracer = NewObject<UStaticMeshComponent>(this);
		Tracer->SetupAttachment(Root);
		Tracer->RegisterComponent();
		Tracer->SetStaticMesh(UnitMesh);
		Tracer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		const bool bPlayer = Unit.OwnerIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(
			bPlayer ? FLinearColor(1.0f, 0.88f, 0.42f) : FLinearColor(1.0f, 0.40f, 0.22f)))
		{
			Tracer->SetMaterial(0, Mat);
		}
		TracerComponents.Add(Unit.TacticalUnitId, Tracer);
	}

	const FElementStyle FromStyle = StyleForUnitId(Unit.UnitId);
	const FElementStyle ToStyle = StyleForUnitId(Target->UnitId);
	FVector From = TacticalToWorld(Unit.Position);
	From.Z = GroundZ + FromStyle.HoverZCm + FromStyle.Scale.Z * 60.f;
	FVector To = TacticalToWorld(Target->Position);
	To.Z = GroundZ + ToStyle.HoverZCm + ToStyle.Scale.Z * 60.f;

	const FVector Mid = (From + To) * 0.5f;
	const FVector Dir = To - From;
	const float Length = static_cast<float>(Dir.Size());
	if (Length < 10.f)
	{
		HideTracer();
		return;
	}
	Tracer->SetWorldLocation(Mid);
	Tracer->SetWorldRotation(Dir.Rotation());
	Tracer->SetWorldScale3D(FVector(Length / 100.f, 0.09f, 0.09f));
	Tracer->SetVisibility(true);
}

void AWLTacticalBattleView::UpdateShells(const FWLTacticalBattleState& Battle)
{
	if (!SphereMesh || !RingMesh)
	{
		return;
	}

	// Salvas vivas: sincronizar componentes (arco balistico origen -> punto de impacto).
	TSet<FString> AliveShells;
	for (const FWLTacticalShellState& Shell : Battle.Shells)
	{
		AliveShells.Add(Shell.ShellId);
		UStaticMeshComponent** Found = ShellComponents.Find(Shell.ShellId);
		UStaticMeshComponent* Comp = Found ? *Found : nullptr;
		if (!Comp)
		{
			Comp = NewObject<UStaticMeshComponent>(this);
			Comp->SetupAttachment(Root);
			Comp->RegisterComponent();
			Comp->SetStaticMesh(SphereMesh);
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Comp->SetWorldScale3D(FVector(0.34f, 0.34f, 0.34f));
			const bool bPlayer = Shell.OwnerIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
			if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(
				bPlayer ? FLinearColor(0.95f, 0.82f, 0.35f) : FLinearColor(0.95f, 0.38f, 0.20f)))
			{
				Comp->SetMaterial(0, Mat);
			}
			ShellComponents.Add(Shell.ShellId, Comp);
		}

		const double Flight = FMath::Max(0.1, Shell.ImpactAtSeconds - Shell.FiredAtSeconds);
		const float T = FMath::Clamp(static_cast<float>((Battle.ElapsedSeconds - Shell.FiredAtSeconds) / Flight), 0.f, 1.f);
		const FVector From = TacticalToWorld(Shell.FirePosition);
		const FVector To = TacticalToWorld(Shell.ImpactPosition);
		FVector Pos = FMath::Lerp(From, To, T);
		const float ArcPeak = FMath::Min(2800.f, static_cast<float>(FVector::Dist2D(From, To)) * 0.30f);
		Pos.Z = GroundZ + 120.f + ArcPeak * FMath::Sin(T * PI);
		Comp->SetWorldLocation(Pos);
		Comp->SetVisibility(true);
	}

	// Salvas que ya impactaron: quitar el proyectil, dejar CRATER y encender un FOGONAZO breve.
	for (auto It = ShellComponents.CreateIterator(); It; ++It)
	{
		if (AliveShells.Contains(It->Key))
		{
			continue;
		}
		if (UStaticMeshComponent* Comp = It->Value)
		{
			if (FlashComponents.Num() < 24)
			{
				if (UStaticMeshComponent* Flash = MakeSpriteBillboard())
				{
					FVector FlashLoc = Comp->GetComponentLocation();
					FlashLoc.Z = GroundZ + 180.f;   // el sprite de explosion se anima en UpdateBattleEffects
					Flash->SetWorldLocation(FlashLoc);
					FlashComponents.Add(Flash);
					FlashSpawnSeconds.Add(Battle.ElapsedSeconds);
				}
			}
			if (ImpactComponents.Num() < 24)
			{
				if (UStaticMeshComponent* Dust = MakeSpriteBillboard())
				{
					FVector DustLoc = Comp->GetComponentLocation();
					DustLoc.Z = GroundZ + 70.f;   // polvo a ras de suelo; se anima en UpdateBattleEffects
					Dust->SetWorldLocation(DustLoc);
					ImpactComponents.Add(Dust);
					ImpactSpawnSeconds.Add(Battle.ElapsedSeconds);
				}
			}
			if (ScorchComponents.Num() < 60)
			{
				UStaticMeshComponent* Scorch = NewObject<UStaticMeshComponent>(this);
				Scorch->SetupAttachment(Root);
				Scorch->RegisterComponent();
				Scorch->SetStaticMesh(RingMesh);
				FVector Loc = Comp->GetComponentLocation();
				Loc.Z = GroundZ + 2.5f;
				Scorch->SetWorldLocation(Loc);
				Scorch->SetWorldScale3D(FVector(7.5f, 7.5f, 0.02f));
				Scorch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(FLinearColor(0.05f, 0.045f, 0.04f)))
				{
					Scorch->SetMaterial(0, Mat);
				}
				ScorchComponents.Add(Scorch);
			}
			Comp->DestroyComponent();
		}
		It.RemoveCurrent();
	}
}

void AWLTacticalBattleView::UpdateBattleEffects(const FWLTacticalBattleState& Battle)
{
	if (!SpriteMaterial || !GroundMesh)
	{
		return;
	}

	// HUMO: columna de sprites (Codex) que sube, pasa de denso a disperso y se desvanece, reciclando.
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		const bool bSmoking = !Unit.bDestroyed && Unit.Health > 0.0 && Unit.Health < 55.0;
		UStaticMeshComponent* Smoke = SmokeComponents.FindRef(Unit.TacticalUnitId);
		const FVector* Center = ContingentCenters.Find(Unit.TacticalUnitId);
		if (!bSmoking || !Center)
		{
			if (Smoke) { Smoke->SetVisibility(false); }
			continue;
		}
		if (!Smoke)
		{
			Smoke = MakeSpriteBillboard();
			if (!Smoke)
			{
				continue;
			}
			SmokeComponents.Add(Unit.TacticalUnitId, Smoke);
		}
		const double Phase = static_cast<double>(GetTypeHash(Unit.TacticalUnitId) % 97) / 97.0;
		const float Cycle = static_cast<float>(FMath::Fmod(Battle.ElapsedSeconds * 0.5 + Phase, 1.0));
		const int32 Frame = FMath::Clamp(1 + FMath::FloorToInt(Cycle * 4.f), 1, 4);   // smoke_01 denso -> _04 disperso
		const FVector Pos = *Center + FVector(0.f, 0.f, 220.f + 520.f * Cycle);
		const float Size = 360.f + 560.f * Cycle;
		const float Opacity = FMath::Clamp(1.15f - Cycle, 0.15f, 1.0f);
		UpdateSpriteBillboard(Smoke, FString::Printf(TEXT("smoke_0%d"), Frame),
			FLinearColor(1.f, 1.f, 1.f, 1.f), Opacity, Pos, Size);
	}

	// EXPLOSIONES: secuencia de 6 fotogramas (fuego -> humo) en ~0.55 s sobre el punto de impacto.
	for (int32 Index = FlashComponents.Num() - 1; Index >= 0; --Index)
	{
		UStaticMeshComponent* Flash = FlashComponents[Index];
		const double Age = FlashSpawnSeconds.IsValidIndex(Index)
			? Battle.ElapsedSeconds - FlashSpawnSeconds[Index] : 1.0;
		if (!Flash || Age > 0.55)
		{
			if (Flash) { Flash->DestroyComponent(); }
			FlashComponents.RemoveAt(Index);
			FlashSpawnSeconds.RemoveAt(Index);
			continue;
		}
		const float T = FMath::Clamp(static_cast<float>(Age / 0.55), 0.f, 0.999f);
		const int32 Frame = FMath::Clamp(1 + FMath::FloorToInt(T * 6.f), 1, 6);
		const float Size = 460.f + 980.f * T;
		const float Opacity = FMath::Clamp(1.2f - T * 0.5f, 0.25f, 1.0f);
		UpdateSpriteBillboard(Flash, FString::Printf(TEXT("explosion_0%d"), Frame),
			FLinearColor(1.f, 1.f, 1.f, 1.f), Opacity, Flash->GetComponentLocation(), Size);
	}

	// POLVO DE IMPACTO: 3 fotogramas a ras de suelo (impact_01..03) en ~0.45 s.
	for (int32 Index = ImpactComponents.Num() - 1; Index >= 0; --Index)
	{
		UStaticMeshComponent* Dust = ImpactComponents[Index];
		const double Age = ImpactSpawnSeconds.IsValidIndex(Index)
			? Battle.ElapsedSeconds - ImpactSpawnSeconds[Index] : 1.0;
		if (!Dust || Age > 0.45)
		{
			if (Dust) { Dust->DestroyComponent(); }
			ImpactComponents.RemoveAt(Index);
			ImpactSpawnSeconds.RemoveAt(Index);
			continue;
		}
		const float T = FMath::Clamp(static_cast<float>(Age / 0.45), 0.f, 0.999f);
		const int32 Frame = FMath::Clamp(1 + FMath::FloorToInt(T * 3.f), 1, 3);
		const float Size = 360.f + 520.f * T;
		const float Opacity = FMath::Clamp(1.0f - T, 0.2f, 1.0f);
		UpdateSpriteBillboard(Dust, FString::Printf(TEXT("impact_0%d"), Frame),
			FLinearColor(1.f, 1.f, 1.f, 1.f), Opacity, Dust->GetComponentLocation(), Size);
	}
}

void AWLTacticalBattleView::Initialize(const FWLTacticalBattleState& Battle, const FString& InPlayerIso)
{
	PlayerIso = InPlayerIso.TrimStartAndEnd().ToUpper();
	AttackerIso = Battle.AttackerIso;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Datos de unidad cacheados (estilo de formacion + alcance de trazadora).
	if (const UGameInstance* GI = World->GetGameInstance())
	{
		if (const UWLDataRegistry* Registry = GI->GetSubsystem<UWLDataRegistry>())
		{
			for (const FWLTacticalUnitState& Unit : Battle.Units)
			{
				const FString Key = Unit.UnitId.ToLower();
				if (!UnitDataById.Contains(Key))
				{
					FWLUnitData Data;
					if (Registry->GetUnit(Key, Data))
					{
						UnitDataById.Add(Key, Data);
					}
				}
			}
		}
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

	// F2: los parches de terreno se dibujan antes que nada (quedan bajo unidades y anillos).
	BuildTerrainPatches(Battle);

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

	// Una FORMACION instanciada por contingente: N elementos visibles que caen con las bajas.
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (!UnitMesh)
		{
			break;
		}
		UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(this);
		Mesh->SetupAttachment(Root);
		Mesh->RegisterComponent();
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// F6: modelo real con material unlit vertex-color (el camo distingue los bandos);
		// sin modelo, cubo tintado por bando/salud como antes.
		UStaticMesh* Model = ModelForUnit(Unit);
		if (Model && VehicleMaterial)
		{
			Mesh->SetStaticMesh(Model);
			Mesh->SetMaterial(0, VehicleMaterial);
		}
		else
		{
			Mesh->SetStaticMesh(UnitMesh);
			if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(ColorForUnit(Unit)))
			{
				Mesh->SetMaterial(0, Mat);
			}
		}
		ContingentMeshes.Add(Unit.TacticalUnitId, Mesh);

		// Encaramiento inicial: los bandos se miran (atacante desde -X).
		ContingentYaw.Add(Unit.TacticalUnitId,
			Unit.OwnerIso.Equals(Battle.AttackerIso, ESearchCase::IgnoreCase) ? 0.f : 180.f);
		RebuildContingentInstances(Mesh, Unit);
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
	LastElapsedSeconds = Battle.ElapsedSeconds;

	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		UInstancedStaticMeshComponent** Found = ContingentMeshes.Find(Unit.TacticalUnitId);
		if (!Found || !*Found)
		{
			continue;
		}
		UInstancedStaticMeshComponent* Mesh = *Found;

		if (Unit.bDestroyed || Unit.Health <= 0.0)
		{
			// El contingente cae: la formacion desaparece y quedan RESTOS en su ultima posicion.
			if (const FVector* LastCenter = ContingentCenters.Find(Unit.TacticalUnitId))
			{
				SpawnWrecks(Unit, *LastCenter);
			}
			Mesh->SetVisibility(false);
			ContingentCenters.Remove(Unit.TacticalUnitId);
			UpdateTracer(Battle, Unit);
			continue;
		}
		Mesh->SetVisibility(true);

		const FElementStyle Style = StyleForUnitId(Unit.UnitId);

		// Encaramiento: hacia el objetivo de ataque, o hacia el destino de movimiento.
		float Yaw = ContingentYaw.FindRef(Unit.TacticalUnitId);
		FVector2D Facing = FVector2D::ZeroVector;
		if (Unit.Order == EWLTacticalUnitOrder::Attacking && !Unit.AttackTargetUnitId.IsEmpty())
		{
			if (const FWLTacticalUnitState* Target = Battle.Units.FindByPredicate(
				[&Unit](const FWLTacticalUnitState& U) { return U.TacticalUnitId == Unit.AttackTargetUnitId; }))
			{
				Facing = Target->Position - Unit.Position;
			}
		}
		else if (Unit.Order == EWLTacticalUnitOrder::Moving || Unit.Order == EWLTacticalUnitOrder::Routing)
		{
			Facing = Unit.MoveTarget - Unit.Position;
		}
		if (Facing.SizeSquared() > 1.0)
		{
			Yaw = FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X));
			ContingentYaw.Add(Unit.TacticalUnitId, Yaw);
		}

		// Mover TODA la formacion (las instancias son relativas al componente).
		FVector Center = TacticalToWorld(Unit.Position);
		Center.Z = GroundZ + Style.HoverZCm;
		Mesh->SetWorldLocationAndRotation(Center, FRotator(0.f, Yaw, 0.f));
		ContingentCenters.Add(Unit.TacticalUnitId, Center);
		const float FormationExtent = FMath::Sqrt(static_cast<float>(FMath::Max(1, Unit.ElementCount))) * Style.SpacingCm;
		ContingentPickRadius.Add(Unit.TacticalUnitId, FMath::Max(UnitPickRadius, FormationExtent * 0.75f));

		// Las BAJAS se ven: reconstruir instancias cuando cambian los elementos vivos.
		if (ContingentShownElements.FindRef(Unit.TacticalUnitId) != Unit.ElementCount)
		{
			RebuildContingentInstances(Mesh, Unit);
		}

		if (UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
		{
			const FLinearColor Color = ColorForUnit(Unit);
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			Mat->SetVectorParameterValue(TEXT("Base Color"), Color);
		}

		UpdateTracer(Battle, Unit);
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

	// Anillo de seleccion: rodea la FORMACION seleccionada (escala con su extension).
	if (SelectionRing)
	{
		const FWLTacticalUnitState* Sel = Battle.Units.FindByPredicate([this](const FWLTacticalUnitState& U)
		{
			return U.TacticalUnitId == SelectedUnitId && !U.bDestroyed && U.Health > 0.0;
		});
		if (Sel)
		{
			const FElementStyle Style = StyleForUnitId(Sel->UnitId);
			FVector Loc = TacticalToWorld(Sel->Position);
			Loc.Z = GroundZ + 6.f;
			SelectionRing->SetWorldLocation(Loc);
			const float Extent = FMath::Sqrt(static_cast<float>(FMath::Max(1, Sel->ElementCount))) * Style.SpacingCm;
			const float RingScale = FMath::Max(2.6f, (Extent * 1.35f) / 50.f);
			SelectionRing->SetWorldScale3D(FVector(RingScale, RingScale, 0.06f));
			SelectionRing->SetVisibility(true);
		}
		else
		{
			SelectionRing->SetVisibility(false);
		}
	}

	// F3: salvas indirectas en vuelo y crateres de impacto.
	UpdateShells(Battle);

	// F6: humo en danados y fogonazos de impacto.
	UpdateBattleEffects(Battle);
}

FString AWLTacticalBattleView::FindUnitNearWorldPoint(const FVector& WorldPoint) const
{
	FString Best;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const TPair<FString, FVector>& Pair : ContingentCenters)
	{
		const float PickRadius = ContingentPickRadius.FindRef(Pair.Key) > 0.f
			? ContingentPickRadius.FindRef(Pair.Key)
			: UnitPickRadius;
		const float DistSq = FVector2D::DistSquared(
			FVector2D(Pair.Value.X, Pair.Value.Y), FVector2D(WorldPoint.X, WorldPoint.Y));
		if (DistSq < PickRadius * PickRadius && DistSq < BestDistSq)
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
