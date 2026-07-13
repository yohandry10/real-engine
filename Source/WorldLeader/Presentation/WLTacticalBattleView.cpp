// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLTacticalBattleView.h"
#include "Battle/WLTacticalBattleSubsystem.h"
#include "WorldLeader.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Campaign/WLDataRegistry.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
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
	if (UnitFinder.Succeeded()) { UtilityCubeMesh = UnitFinder.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (RingFinder.Succeeded()) { RingMesh = RingFinder.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded()) { BillboardPlaneMesh = PlaneFinder.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded()) { SphereMesh = SphereFinder.Object; }

	// F6: modelos low-poly reales (gen_vehicle.py -> /Game/GenVehicle), unlit vertex color.
	// Bando del jugador = camo verde; enemigo = desierto. Caza y buque son neutros (gris).
	// No hay fallback de cubo: una unidad sin modelo se registra como error y no se dibuja.
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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BattleMatFinder(TEXT("/Game/GenBattle/M_BattleUnlit.M_BattleUnlit"));
	if (BattleMatFinder.Succeeded()) { BattleMaterial = BattleMatFinder.Object; }

	auto LoadBattleMesh = [](const TCHAR* Path) -> UStaticMesh*
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(Path);
		return Finder.Succeeded() ? Finder.Object : nullptr;
	};
	auto AddMesh = [&LoadBattleMesh](TArray<UStaticMesh*>& Target, const TCHAR* Path)
	{
		if (UStaticMesh* Mesh = LoadBattleMesh(Path))
		{
			Target.Add(Mesh);
		}
	};
	auto AddNamedMesh = [&LoadBattleMesh](TMap<FString, UStaticMesh*>& Target, const TCHAR* Key, const TCHAR* Path)
	{
		if (UStaticMesh* Mesh = LoadBattleMesh(Path))
		{
			Target.Add(Key, Mesh);
		}
	};

	BattlefieldMesh = LoadBattleMesh(TEXT("/Game/GenBattle/battlefield_grassland.battlefield_grassland"));

	AddMesh(UrbanMeshes, TEXT("/Game/GenBattle/battle_house.battle_house"));
	AddMesh(UrbanMeshes, TEXT("/Game/GenBattle/battle_block.battle_block"));
	AddMesh(UrbanMeshes, TEXT("/Game/GenBattle/battle_warehouse.battle_warehouse"));
	AddMesh(UrbanMeshes, TEXT("/Game/GenBattle/battle_gas_station.battle_gas_station"));
	AddMesh(UrbanMeshes, TEXT("/Game/GenBattle/battle_house_ruin.battle_house_ruin"));
	AddMesh(UrbanMeshes, TEXT("/Game/GenBattle/battle_block_ruin.battle_block_ruin"));
	AddMesh(UrbanMeshes, TEXT("/Game/GenBattle/battle_warehouse_ruin.battle_warehouse_ruin"));
	AddMesh(UrbanMeshes, TEXT("/Game/GenBattle/battle_gas_station_ruin.battle_gas_station_ruin"));

	AddMesh(NatureMeshes, TEXT("/Game/GenBattle/battle_tree_broadleaf_a.battle_tree_broadleaf_a"));
	AddMesh(NatureMeshes, TEXT("/Game/GenBattle/battle_tree_broadleaf_b.battle_tree_broadleaf_b"));
	AddMesh(NatureMeshes, TEXT("/Game/GenBattle/battle_tree_conifer.battle_tree_conifer"));
	AddMesh(NatureMeshes, TEXT("/Game/GenBattle/battle_shrub_a.battle_shrub_a"));
	AddMesh(NatureMeshes, TEXT("/Game/GenBattle/battle_shrub_b.battle_shrub_b"));
	AddMesh(NatureMeshes, TEXT("/Game/GenBattle/battle_fallen_log.battle_fallen_log"));

	AddMesh(FortificationMeshes, TEXT("/Game/GenBattle/battle_fort_sandbags.battle_fort_sandbags"));
	AddMesh(FortificationMeshes, TEXT("/Game/GenBattle/battle_fort_trench_straight.battle_fort_trench_straight"));
	AddMesh(FortificationMeshes, TEXT("/Game/GenBattle/battle_fort_trench_corner.battle_fort_trench_corner"));
	AddMesh(FortificationMeshes, TEXT("/Game/GenBattle/battle_fort_bunker.battle_fort_bunker"));
	AddMesh(FortificationMeshes, TEXT("/Game/GenBattle/battle_fort_wire.battle_fort_wire"));
	AddMesh(FortificationMeshes, TEXT("/Game/GenBattle/battle_fort_checkpoint.battle_fort_checkpoint"));

	AddMesh(FieldPropMeshes, TEXT("/Game/GenBattle/battle_prop_rocks_a.battle_prop_rocks_a"));
	AddMesh(FieldPropMeshes, TEXT("/Game/GenBattle/battle_prop_rocks_b.battle_prop_rocks_b"));
	AddMesh(FieldPropMeshes, TEXT("/Game/GenBattle/battle_prop_fence.battle_prop_fence"));
	AddMesh(FieldPropMeshes, TEXT("/Game/GenBattle/battle_prop_utility_pole.battle_prop_utility_pole"));
	CraterMesh = LoadBattleMesh(TEXT("/Game/GenBattle/battle_prop_crater.battle_prop_crater"));
	if (CraterMesh) { FieldPropMeshes.Add(CraterMesh); }

	AddNamedMesh(WreckModels, TEXT("mbt"), TEXT("/Game/GenBattle/battle_wreck_mbt.battle_wreck_mbt"));
	AddNamedMesh(WreckModels, TEXT("ifv"), TEXT("/Game/GenBattle/battle_wreck_ifv.battle_wreck_ifv"));
	AddNamedMesh(WreckModels, TEXT("apc"), TEXT("/Game/GenBattle/battle_wreck_apc.battle_wreck_apc"));
	AddNamedMesh(WreckModels, TEXT("artillery"), TEXT("/Game/GenBattle/battle_wreck_artillery.battle_wreck_artillery"));
	AddNamedMesh(WreckModels, TEXT("sam"), TEXT("/Game/GenBattle/battle_wreck_sam.battle_wreck_sam"));
	AddNamedMesh(WreckModels, TEXT("heli"), TEXT("/Game/GenBattle/battle_wreck_heli.battle_wreck_heli"));

	AddNamedMesh(SoldierPoseModels, TEXT("kneeling"), TEXT("/Game/GenBattle/battle_soldier_kneeling.battle_soldier_kneeling"));
	AddNamedMesh(SoldierPoseModels, TEXT("prone"), TEXT("/Game/GenBattle/battle_soldier_prone.battle_soldier_prone"));
	AddNamedMesh(SoldierPoseModels, TEXT("kneeling_desert"), TEXT("/Game/GenBattle/battle_soldier_kneeling_desert.battle_soldier_kneeling_desert"));
	AddNamedMesh(SoldierPoseModels, TEXT("prone_desert"), TEXT("/Game/GenBattle/battle_soldier_prone_desert.battle_soldier_prone_desert"));

	AddNamedMesh(BannerModels, TEXT("VE"), TEXT("/Game/GenBattle/battle_banner_ve.battle_banner_ve"));
	AddNamedMesh(BannerModels, TEXT("CO"), TEXT("/Game/GenBattle/battle_banner_co.battle_banner_co"));
}

void AWLTacticalBattleView::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BattleCamera) { BattleCamera->Destroy(); BattleCamera = nullptr; }
	if (BattleLight) { BattleLight->Destroy(); BattleLight = nullptr; }
	if (BattleSky) { BattleSky->Destroy(); BattleSky = nullptr; }
	if (BattleAtmosphere) { BattleAtmosphere->Destroy(); BattleAtmosphere = nullptr; }
	if (BattleClouds) { BattleClouds->Destroy(); BattleClouds = nullptr; }
	if (BattleFog) { BattleFog->Destroy(); BattleFog = nullptr; }
	Super::EndPlay(EndPlayReason);
}

UTexture2D* AWLTacticalBattleView::LoadBattleSprite(const FString& Name) const
{
	return WLGovAssetsNS::LoadExternalTexture(FString::Printf(TEXT("UI/Battle/%s.png"), *Name));
}

UStaticMeshComponent* AWLTacticalBattleView::MakeSpriteBillboard()
{
	if (!BillboardPlaneMesh || !SpriteMaterial)
	{
		return nullptr;
	}
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetupAttachment(Root);
	Comp->RegisterComponent();
	Comp->SetStaticMesh(BillboardPlaneMesh);   // plano tecnico 100x100, orientado a la camara por frame
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

FString AWLTacticalBattleView::UnitKind(const FWLTacticalUnitState& Unit) const
{
	const FString Id = Unit.UnitId.ToLower();
	if (Id == TEXT("infantry"))                             { return TEXT("soldier"); }
	if (Id == TEXT("apc"))                                  { return TEXT("apc"); }
	if (Id == TEXT("ifv"))                                  { return TEXT("ifv"); }
	if (Id == TEXT("mbt") || Id == TEXT("tank"))            { return TEXT("mbt"); }
	if (Id == TEXT("artillery"))                            { return TEXT("artillery"); }
	if (Id == TEXT("sam"))                                  { return TEXT("sam"); }
	if (Id == TEXT("heli"))                                 { return TEXT("heli"); }
	if (Id == TEXT("aircraft") || Id == TEXT("drone"))      { return TEXT("aircraft"); }
	if (Id == TEXT("ship"))                                 { return TEXT("ship"); }
	return TEXT("soldier");
}

bool AWLTacticalBattleView::IsInfantryUnit(const FWLTacticalUnitState& Unit) const
{
	if (const FWLUnitData* Data = UnitDataById.Find(Unit.UnitId.ToLower()))
	{
		return Data->Type == EWLUnitType::Infantry || Data->Type == EWLUnitType::SpecialForces;
	}
	return UnitKind(Unit) == TEXT("soldier");
}

UStaticMesh* AWLTacticalBattleView::ModelForUnit(const FWLTacticalUnitState& Unit) const
{
	const FString Kind = UnitKind(Unit);

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

UStaticMesh* AWLTacticalBattleView::WreckForUnit(const FWLTacticalUnitState& Unit) const
{
	const FString Kind = UnitKind(Unit);
	if (Kind == TEXT("soldier"))
	{
		const bool bPlayer = Unit.OwnerIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
		return SoldierPoseModels.FindRef(bPlayer ? TEXT("prone") : TEXT("prone_desert"));
	}
	return WreckModels.FindRef(Kind);
}

UStaticMesh* AWLTacticalBattleView::BannerForUnit(const FWLTacticalUnitState& Unit) const
{
	const FString Iso = Unit.OwnerIso.TrimStartAndEnd().ToUpper();
	if (UStaticMesh* Exact = BannerModels.FindRef(Iso))
	{
		return Exact;
	}
	// El paquete actual trae VE/CO. Para otros paises conserva identificacion por lado sin usar placeholders.
	return BannerModels.FindRef(Unit.OwnerIso.Equals(AttackerIso, ESearchCase::IgnoreCase) ? TEXT("VE") : TEXT("CO"));
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
	FVector Result(Tactical.X * WorldScale, Tactical.Y * WorldScale, GroundZ);
	Result.Z = BattlefieldHeightCmAtWorld(FVector2D(Result.X, Result.Y));
	return Result;
}

FVector2D AWLTacticalBattleView::WorldToTactical(const FVector& World) const
{
	return FVector2D(World.X / WorldScale, World.Y / WorldScale);
}

float AWLTacticalBattleView::BattlefieldHeightCmAtWorld(const FVector2D& WorldXY) const
{
	// Debe mantenerse alineado con terrain_height() de gen_battlefield.py.
	const double X = WorldXY.X / 100.0;
	const double Y = WorldXY.Y / 100.0;
	const double Broad = 0.30 * FMath::Sin((X + 18.0) / 35.0) + 0.24 * FMath::Cos((Y - 9.0) / 29.0);
	const double HillA = 0.55 * FMath::Exp(-((X + 56.0) * (X + 56.0) + (Y - 44.0) * (Y - 44.0)) / 1800.0);
	const double HillB = 0.42 * FMath::Exp(-((X - 61.0) * (X - 61.0) + (Y + 48.0) * (Y + 48.0)) / 1500.0);
	const double Hollow = -0.28 * FMath::Exp(-((X - 12.0) * (X - 12.0) + (Y - 16.0) * (Y - 16.0)) / 900.0);
	const double Micro = 0.08 * FMath::Sin(X * 0.19 + Y * 0.11) * FMath::Cos(Y * 0.17);
	const double HeightMeters = Broad + HillA + HillB + Hollow + Micro - 0.28;
	// El FBX se coloca +28 cm para que su altura media coincida con GroundZ.
	return static_cast<float>(GroundZ + (HeightMeters + 0.28) * 100.0);
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

void AWLTacticalBattleView::RebuildContingentInstances(const FWLTacticalUnitState& Unit)
{
	UInstancedStaticMeshComponent* MainMesh = ContingentMeshes.FindRef(Unit.TacticalUnitId);
	if (!MainMesh || !MainMesh->GetStaticMesh())
	{
		return;
	}
	UInstancedStaticMeshComponent* KneelingMesh = KneelingContingentMeshes.FindRef(Unit.TacticalUnitId);
	UInstancedStaticMeshComponent* ProneMesh = ProneContingentMeshes.FindRef(Unit.TacticalUnitId);
	MainMesh->ClearInstances();
	if (KneelingMesh) { KneelingMesh->ClearInstances(); }
	if (ProneMesh) { ProneMesh->ClearInstances(); }

	const FElementStyle Style = StyleForUnitId(Unit.UnitId);
	TArray<FVector2D> Offsets;
	BuildFormationOffsets(Unit.ElementCount, Style.SpacingCm, Offsets);

	const FBoxSphereBounds MainBounds = MainMesh->GetStaticMesh()->GetBounds();
	const float MainSize = 2.f * (Style.bScaleByHeight ? MainBounds.BoxExtent.Z : MainBounds.BoxExtent.X);
	const float Uniform = Style.TargetSizeCm / FMath::Max(1.f, MainSize);

	auto AddInstance = [Uniform](UInstancedStaticMeshComponent* Target, const FVector2D& Offset)
	{
		if (!Target || !Target->GetStaticMesh())
		{
			return;
		}
		const FBoxSphereBounds Bounds = Target->GetStaticMesh()->GetBounds();
		const float BaseZ = -(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Uniform;
		FTransform Xform;
		Xform.SetScale3D(FVector(Uniform));
		Xform.SetLocation(FVector(Offset.X, Offset.Y, BaseZ));
		Target->AddInstance(Xform);
	};

	for (int32 Index = 0; Index < Offsets.Num(); ++Index)
	{
		// La primera linea mantiene lectura de pie; filas posteriores mezclan rodilla y cuerpo a tierra.
		if (ProneMesh && Index % 7 == 3)
		{
			AddInstance(ProneMesh, Offsets[Index]);
		}
		else if (KneelingMesh && Index % 4 == 1)
		{
			AddInstance(KneelingMesh, Offsets[Index]);
		}
		else
		{
			AddInstance(MainMesh, Offsets[Index]);
		}
	}
	ContingentShownElements.Add(Unit.TacticalUnitId, Unit.ElementCount);
}

UStaticMeshComponent* AWLTacticalBattleView::SpawnBattleAsset(UStaticMesh* Mesh, const FVector& WorldLocation,
	const FRotator& WorldRotation, const FVector& AssetScale, TArray<UStaticMeshComponent*>& Bucket)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetupAttachment(Root);
	Component->RegisterComponent();
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetWorldRotation(WorldRotation);
	Component->SetWorldScale3D(AssetScale);
	FVector AnchoredLocation = WorldLocation;
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	AnchoredLocation.Z -= (Bounds.Origin.Z - Bounds.BoxExtent.Z) * AssetScale.Z;
	Component->SetWorldLocation(AnchoredLocation);
	if (BattleMaterial)
	{
		Component->SetMaterial(0, BattleMaterial);
	}
	Bucket.Add(Component);
	return Component;
}

void AWLTacticalBattleView::BuildFieldProps(const FWLTacticalBattleState& Battle)
{
	if (FieldPropMeshes.Num() < 5)
	{
		return;
	}
	struct FFieldPlacement
	{
		int32 MeshIndex;
		FVector2D WorldXY;
		float Yaw;
		float Scale;
	};
	const FFieldPlacement Placements[] = {
		{0, FVector2D(-8200.f, -6700.f), 18.f, 0.95f}, {1, FVector2D(7600.f, 6900.f), 72.f, 0.90f},
		{0, FVector2D(-7200.f, 6100.f), 126.f, 0.80f}, {1, FVector2D(8300.f, -5900.f), 33.f, 0.85f},
		{2, FVector2D(-6500.f, -2500.f), 12.f, 0.95f}, {2, FVector2D(5900.f, 3100.f), 168.f, 0.90f},
		{2, FVector2D(-2400.f, 7600.f), 94.f, 0.85f}, {3, FVector2D(-9200.f, 1200.f), 0.f, 0.88f},
		{3, FVector2D(9100.f, -600.f), 0.f, 0.92f}, {3, FVector2D(1600.f, 9000.f), 0.f, 0.84f},
		{4, FVector2D(-4900.f, 3900.f), 0.f, 0.68f}, {4, FVector2D(4500.f, -4200.f), 24.f, 0.74f},
		{4, FVector2D(-1200.f, -8200.f), 61.f, 0.62f}
	};

	for (const FFieldPlacement& Placement : Placements)
	{
		const FVector2D Tactical = Placement.WorldXY / static_cast<float>(WorldScale);
		const bool bInsidePatch = Battle.TerrainPatches.ContainsByPredicate([&Tactical](const FWLTacticalTerrainPatch& Patch)
		{
			return FVector2D::Distance(Tactical, Patch.Position) < Patch.Radius * 0.9;
		});
		if (bInsidePatch)
		{
			continue;
		}
		const FVector Location(Placement.WorldXY.X, Placement.WorldXY.Y, BattlefieldHeightCmAtWorld(Placement.WorldXY));
		SpawnBattleAsset(FieldPropMeshes[Placement.MeshIndex], Location, FRotator(0.f, Placement.Yaw, 0.f),
			FVector(Placement.Scale), TerrainComponents);
	}
}

void AWLTacticalBattleView::BuildTerrainPatches(const FWLTacticalBattleState& Battle)
{
	for (const FWLTacticalTerrainPatch& Patch : Battle.TerrainPatches)
	{
		const bool bUrban = Patch.Terrain == EWLTacticalTerrain::Urban;
		const TArray<UStaticMesh*>& Palette = bUrban ? UrbanMeshes : NatureMeshes;
		if (Patch.Terrain == EWLTacticalTerrain::Open || Palette.IsEmpty())
		{
			continue;
		}

		const int32 PropCount = bUrban ? FMath::Min(8, Palette.Num()) : 18;
		const uint32 Hash = GetTypeHash(Patch.PatchId);
		const float MaxOffset = static_cast<float>(Patch.Radius * WorldScale) * (bUrban ? 0.66f : 0.78f);
		const FVector PatchCenter = TacticalToWorld(Patch.Position);
		for (int32 i = 0; i < PropCount; ++i)
		{
			const uint32 Seed = Hash + static_cast<uint32>(i) * 2654435761u;
			const float Angle = FMath::DegreesToRadians(static_cast<float>(Seed % 360u) + i * 137.5f);
			const float RadiusAlpha = 0.24f + 0.70f * (static_cast<float>((Seed / 997u) % 1000u) / 999.f);
			const FVector2D XY(PatchCenter.X + FMath::Cos(Angle) * MaxOffset * RadiusAlpha,
				PatchCenter.Y + FMath::Sin(Angle) * MaxOffset * RadiusAlpha);
			const float Uniform = bUrban
				? 0.68f + static_cast<float>((Seed / 13u) % 18u) * 0.01f
				: 0.78f + static_cast<float>((Seed / 17u) % 35u) * 0.01f;
			const FVector Location(XY.X, XY.Y, BattlefieldHeightCmAtWorld(XY));
			SpawnBattleAsset(Palette[i % Palette.Num()], Location,
				FRotator(0.f, static_cast<float>(Seed % 360u), 0.f), FVector(Uniform), TerrainComponents);
		}
	}
}

void AWLTacticalBattleView::BuildObjectiveFortifications(const FWLTacticalBattleState& Battle)
{
	if (FortificationMeshes.Num() < 6)
	{
		return;
	}
	const FVector2D LocalOffsets[] = {
		FVector2D(-900.f, 0.f), FVector2D(0.f, -950.f), FVector2D(850.f, 720.f),
		FVector2D(0.f, 850.f), FVector2D(0.f, -1500.f), FVector2D(1500.f, 0.f)
	};
	const float Scales[] = {0.86f, 0.88f, 0.72f, 0.82f, 0.88f, 0.84f};
	const float LocalYaws[] = {0.f, 0.f, 90.f, 180.f, 0.f, 90.f};
	for (const FWLTacticalObjectiveState& Objective : Battle.Objectives)
	{
		const FVector Center = TacticalToWorld(Objective.Position);
		const float BaseYaw = static_cast<float>((GetTypeHash(Objective.ObjectiveId) % 4u) * 90u);
		const FRotator Rotation(0.f, BaseYaw, 0.f);
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const FVector Rotated = Rotation.RotateVector(FVector(LocalOffsets[Index].X, LocalOffsets[Index].Y, 0.f));
			const FVector2D XY(Center.X + Rotated.X, Center.Y + Rotated.Y);
			const FVector Location(XY.X, XY.Y, BattlefieldHeightCmAtWorld(XY));
			SpawnBattleAsset(FortificationMeshes[Index], Location,
				FRotator(0.f, BaseYaw + LocalYaws[Index], 0.f), FVector(Scales[Index]), TerrainComponents);
		}
	}
}

void AWLTacticalBattleView::SpawnWrecks(const FWLTacticalUnitState& Unit, const FVector& Center)
{
	if (WreckedContingents.Contains(Unit.TacticalUnitId))
	{
		return;
	}
	WreckedContingents.Add(Unit.TacticalUnitId);
	UStaticMesh* WreckModel = WreckForUnit(Unit);
	if (!WreckModel)
	{
		return;   // aeronaves/buques no dejan un cubo sustituto sobre el campo terrestre
	}

	const FElementStyle Style = StyleForUnitId(Unit.UnitId);
	const bool bInfantry = IsInfantryUnit(Unit);
	const int32 WreckCount = FMath::Clamp(Unit.InitialElementCount, 1, bInfantry ? 5 : 3);
	const FBoxSphereBounds Bounds = WreckModel->GetBounds();
	float ReferenceSize = 2.f * Bounds.BoxExtent.X;
	if (bInfantry)
	{
		if (UStaticMesh* Standing = ModelForUnit(Unit))
		{
			ReferenceSize = 2.f * Standing->GetBounds().BoxExtent.Z;
		}
	}
	const float Uniform = Style.TargetSizeCm / FMath::Max(1.f, ReferenceSize);
	const uint32 Hash = GetTypeHash(Unit.TacticalUnitId);
	for (int32 i = 0; i < WreckCount; ++i)
	{
		const float OffX = static_cast<float>(((Hash >> (i * 3)) % 7)) * 90.f - 270.f;
		const float OffY = static_cast<float>(((Hash >> (i * 5)) % 9)) * 90.f - 360.f;
		const float Yaw = static_cast<float>((Hash >> (i * 2)) % 360);
		FVector Loc = Center + FVector(OffX, OffY, 0.f);
		Loc.Z = BattlefieldHeightCmAtWorld(FVector2D(Loc.X, Loc.Y));
		SpawnBattleAsset(WreckModel, Loc, FRotator(0.f, Yaw, 0.f), FVector(Uniform), WreckComponents);
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
		if (!UtilityCubeMesh)
		{
			return;
		}
		Tracer = NewObject<UStaticMeshComponent>(this);
		Tracer->SetupAttachment(Root);
		Tracer->RegisterComponent();
		Tracer->SetStaticMesh(UtilityCubeMesh);
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
	From.Z += FromStyle.HoverZCm + FromStyle.Scale.Z * 60.f;
	FVector To = TacticalToWorld(Target->Position);
	To.Z += ToStyle.HoverZCm + ToStyle.Scale.Z * 60.f;

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
	if (!SphereMesh)
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
		Pos.Z = FMath::Lerp(From.Z, To.Z, T) + 120.f + ArcPeak * FMath::Sin(T * PI);
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
			if (CraterMesh && ScorchComponents.Num() < 60)
			{
				FVector Loc = Comp->GetComponentLocation();
				Loc.Z = BattlefieldHeightCmAtWorld(FVector2D(Loc.X, Loc.Y));
				SpawnBattleAsset(CraterMesh, Loc, FRotator(0.f, static_cast<float>(ScorchComponents.Num() * 37), 0.f),
					FVector(0.72f), ScorchComponents);
			}
			Comp->DestroyComponent();
		}
		It.RemoveCurrent();
	}
}

void AWLTacticalBattleView::UpdateBattleEffects(const FWLTacticalBattleState& Battle)
{
	if (!SpriteMaterial || !BillboardPlaneMesh)
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

	UE_LOG(LogWorldLeader, Display,
		TEXT("TacticalBattle generated assets: ground=%s urban=%d nature=%d forts=%d props=%d wrecks=%d poses=%d banners=%d"),
		BattlefieldMesh ? TEXT("ok") : TEXT("missing"), UrbanMeshes.Num(), NatureMeshes.Num(),
		FortificationMeshes.Num(), FieldPropMeshes.Num(), WreckModels.Num(), SoldierPoseModels.Num(), BannerModels.Num());

	// Campo Blender 220x220 m. El offset compensa el -0.28 m medio del generador.
	if (BattlefieldMesh)
	{
		Ground = NewObject<UStaticMeshComponent>(this);
		Ground->SetupAttachment(Root);
		Ground->RegisterComponent();
		Ground->SetStaticMesh(BattlefieldMesh);
		Ground->SetWorldLocation(FVector(0.f, 0.f, GroundZ + 28.f));
		Ground->SetWorldScale3D(FVector::OneVector);
		Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (BattleMaterial) { Ground->SetMaterial(0, BattleMaterial); }
	}
	else
	{
		UE_LOG(LogWorldLeader, Error, TEXT("TacticalBattle battlefield_grassland is missing; no placeholder ground will be used."));
	}

	BuildFieldProps(Battle);
	BuildTerrainPatches(Battle);
	BuildObjectiveFortifications(Battle);

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
		UStaticMesh* Model = ModelForUnit(Unit);
		if (!Model)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("TacticalBattle unit model missing: unit=%s type=%s"),
				*Unit.TacticalUnitId, *Unit.UnitId);
			continue;
		}
		UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(this);
		Mesh->SetupAttachment(Root);
		Mesh->RegisterComponent();
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetStaticMesh(Model);
		if (VehicleMaterial) { Mesh->SetMaterial(0, VehicleMaterial); }
		ContingentMeshes.Add(Unit.TacticalUnitId, Mesh);

		if (IsInfantryUnit(Unit))
		{
			const bool bPlayer = Unit.OwnerIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
			auto AddPoseComponent = [this, &Unit](UStaticMesh* PoseModel,
				TMap<FString, UInstancedStaticMeshComponent*>& TargetMap)
			{
				if (!PoseModel)
				{
					return;
				}
				UInstancedStaticMeshComponent* Pose = NewObject<UInstancedStaticMeshComponent>(this);
				Pose->SetupAttachment(Root);
				Pose->RegisterComponent();
				Pose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Pose->SetStaticMesh(PoseModel);
				if (VehicleMaterial) { Pose->SetMaterial(0, VehicleMaterial); }
				TargetMap.Add(Unit.TacticalUnitId, Pose);
			};
			AddPoseComponent(SoldierPoseModels.FindRef(bPlayer ? TEXT("kneeling") : TEXT("kneeling_desert")),
				KneelingContingentMeshes);
			AddPoseComponent(SoldierPoseModels.FindRef(bPlayer ? TEXT("prone") : TEXT("prone_desert")),
				ProneContingentMeshes);
		}

		if (UStaticMesh* BannerModel = BannerForUnit(Unit))
		{
			UStaticMeshComponent* Banner = NewObject<UStaticMeshComponent>(this);
			Banner->SetupAttachment(Root);
			Banner->RegisterComponent();
			Banner->SetStaticMesh(BannerModel);
			Banner->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Banner->SetCastShadow(false);
			if (BattleMaterial) { Banner->SetMaterial(0, BattleMaterial); }
			BannerComponents.Add(Unit.TacticalUnitId, Banner);
		}

		// Encaramiento inicial: los bandos se miran (atacante desde -X).
		ContingentYaw.Add(Unit.TacticalUnitId,
			Unit.OwnerIso.Equals(Battle.AttackerIso, ESearchCase::IgnoreCase) ? 0.f : 180.f);
		RebuildContingentInstances(Unit);
	}

	// Atmosfera y luz calida propias: la batalla no flota sobre el fondo vacio de campana.
	BattleLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
		FVector::ZeroVector, FRotator(-52.f, 35.f, 0.f));
	if (UDirectionalLightComponent* LightComponent = BattleLight
		? Cast<UDirectionalLightComponent>(BattleLight->GetLightComponent())
		: nullptr)
	{
		LightComponent->SetIntensity(4.2f);
		LightComponent->SetLightColor(FLinearColor(1.0f, 0.89f, 0.73f));
		LightComponent->SetAtmosphereSunLight(true);
	}
	BattleSky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass());
	if (USkyLightComponent* SkyComp = BattleSky ? Cast<USkyLightComponent>(BattleSky->GetLightComponent()) : nullptr)
	{
		if (UTextureCube* Ambient = LoadObject<UTextureCube>(nullptr,
			TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap")))
		{
			SkyComp->SourceType = SLS_SpecifiedCubemap;
			SkyComp->Cubemap = Ambient;
		}
		SkyComp->bLowerHemisphereIsBlack = false;
		SkyComp->SetLowerHemisphereColor(FLinearColor(0.20f, 0.24f, 0.20f));
		SkyComp->SetIntensity(1.25f);
		SkyComp->RecaptureSky();
	}
	BattleAtmosphere = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass());
	BattleClouds = World->SpawnActor<AVolumetricCloud>(AVolumetricCloud::StaticClass());
	BattleFog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(),
		FVector(0.f, 0.f, 800.f), FRotator::ZeroRotator);
	if (BattleFog && BattleFog->GetComponent())
	{
		BattleFog->GetComponent()->SetFogDensity(0.0018f);
		BattleFog->GetComponent()->SetFogHeightFalloff(0.22f);
		BattleFog->GetComponent()->SetFogMaxOpacity(0.55f);
		BattleFog->GetComponent()->SetStartDistance(7000.f);
		BattleFog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.48f, 0.58f, 0.62f));
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
	UE_LOG(LogWorldLeader, Display,
		TEXT("TacticalBattle scene ready: terrain_assets=%d formations=%d kneeling=%d prone=%d banners=%d objectives=%d atmosphere=%s"),
		TerrainComponents.Num(), ContingentMeshes.Num(), KneelingContingentMeshes.Num(),
		ProneContingentMeshes.Num(), BannerComponents.Num(), ObjectiveComponents.Num(),
		(BattleAtmosphere && BattleClouds && BattleFog) ? TEXT("ok") : TEXT("missing"));
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
			if (UInstancedStaticMeshComponent* Pose = KneelingContingentMeshes.FindRef(Unit.TacticalUnitId))
			{
				Pose->SetVisibility(false);
			}
			if (UInstancedStaticMeshComponent* Pose = ProneContingentMeshes.FindRef(Unit.TacticalUnitId))
			{
				Pose->SetVisibility(false);
			}
			if (UStaticMeshComponent* Banner = BannerComponents.FindRef(Unit.TacticalUnitId))
			{
				Banner->SetVisibility(false);
			}
			ContingentCenters.Remove(Unit.TacticalUnitId);
			UpdateTracer(Battle, Unit);
			continue;
		}
		Mesh->SetVisibility(true);
		if (UInstancedStaticMeshComponent* Pose = KneelingContingentMeshes.FindRef(Unit.TacticalUnitId))
		{
			Pose->SetVisibility(true);
		}
		if (UInstancedStaticMeshComponent* Pose = ProneContingentMeshes.FindRef(Unit.TacticalUnitId))
		{
			Pose->SetVisibility(true);
		}

		const FElementStyle Style = StyleForUnitId(Unit.UnitId);

		// Encaramiento: hacia el objetivo de ataque, o hacia el destino de movimiento.
		float Yaw = ContingentYaw.FindRef(Unit.TacticalUnitId);
		FVector2D Facing = FVector2D::ZeroVector;
		const bool bFixedWingAir = Style.HoverZCm > 0.f && !Unit.UnitId.Equals(TEXT("heli"), ESearchCase::IgnoreCase);
		if (bFixedWingAir)
		{
			// Un ALA FIJA encara su DIRECCION DE VUELO (el backend lo hace orbitar en pasadas);
			// apuntar el morro al objetivo mientras vuela tangencialmente = volar de costado.
			if (const FVector* PrevCenter = ContingentCenters.Find(Unit.TacticalUnitId))
			{
				const FVector NewCenter = TacticalToWorld(Unit.Position);
				const FVector2D Delta(NewCenter.X - PrevCenter->X, NewCenter.Y - PrevCenter->Y);
				if (Delta.SizeSquared() > 1.0f)
				{
					Facing = Delta;
				}
			}
		}
		if (Facing.IsNearlyZero() && Unit.Order == EWLTacticalUnitOrder::Attacking && !Unit.AttackTargetUnitId.IsEmpty())
		{
			if (const FWLTacticalUnitState* Target = Battle.Units.FindByPredicate(
				[&Unit](const FWLTacticalUnitState& U) { return U.TacticalUnitId == Unit.AttackTargetUnitId; }))
			{
				Facing = Target->Position - Unit.Position;
			}
		}
		else if (Facing.IsNearlyZero() && (Unit.Order == EWLTacticalUnitOrder::Moving || Unit.Order == EWLTacticalUnitOrder::Routing))
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
		Center.Z += Style.HoverZCm;
		Mesh->SetWorldLocationAndRotation(Center, FRotator(0.f, Yaw, 0.f));
		if (UInstancedStaticMeshComponent* Pose = KneelingContingentMeshes.FindRef(Unit.TacticalUnitId))
		{
			Pose->SetWorldLocationAndRotation(Center, FRotator(0.f, Yaw, 0.f));
		}
		if (UInstancedStaticMeshComponent* Pose = ProneContingentMeshes.FindRef(Unit.TacticalUnitId))
		{
			Pose->SetWorldLocationAndRotation(Center, FRotator(0.f, Yaw, 0.f));
		}
		ContingentCenters.Add(Unit.TacticalUnitId, Center);
		const float FormationExtent = FMath::Sqrt(static_cast<float>(FMath::Max(1, Unit.ElementCount))) * Style.SpacingCm;
		ContingentPickRadius.Add(Unit.TacticalUnitId, FMath::Max(UnitPickRadius, FormationExtent * 0.75f));
		if (UStaticMeshComponent* Banner = BannerComponents.FindRef(Unit.TacticalUnitId))
		{
			const FVector LocalOffset(-FormationExtent * 0.30f, -FormationExtent * 0.38f, 0.f);
			FVector BannerLocation = Center + FRotator(0.f, Yaw, 0.f).RotateVector(LocalOffset);
			BannerLocation.Z = BattlefieldHeightCmAtWorld(FVector2D(BannerLocation.X, BannerLocation.Y));
			Banner->SetWorldLocationAndRotation(BannerLocation, FRotator(0.f, Yaw, 0.f));
			Banner->SetWorldScale3D(FVector(0.72f));
			Banner->SetVisibility(true);
		}

		// Las BAJAS se ven: reconstruir instancias cuando cambian los elementos vivos.
		if (ContingentShownElements.FindRef(Unit.TacticalUnitId) != Unit.ElementCount)
		{
			RebuildContingentInstances(Unit);
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
			Loc.Z += 6.f;
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
