// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaign3DView.h"

#include "WorldLeader.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Core/WLCharacterTypes.h"
#include "Military/WLMilitarySubsystem.h"
#include "Presentation/WLCampaignOverviewBuilder.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "Presentation/WLCampaignRouteBuilder.h"
#include "Presentation/WLCampaignSettlementBuilder.h"
#include "Presentation/WLCampaignTerrainBuilder.h"
#include "Presentation/WLCampaignVisualStyle.h"
#include "Presentation/WLCampaignWaterBuilder.h"
#include "ProceduralMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/ConstructorHelpers.h"
#include "UnrealClient.h"

namespace
{
	FString ReadCampaignMilitaryForceStringField(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName,
		const FString& DefaultValue = TEXT(""))
	{
		FString Value;
		return Obj.IsValid() && Obj->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
	}

	float ReadCampaignMilitaryForceNumberField(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName,
		float DefaultValue = 0.f)
	{
		double Value = static_cast<double>(DefaultValue);
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(FieldName, Value);
		}
		return static_cast<float>(Value);
	}

	int32 ReadCampaignMilitaryForceIntField(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName,
		int32 DefaultValue = 0)
	{
		double Value = static_cast<double>(DefaultValue);
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(FieldName, Value);
		}
		return FMath::RoundToInt(Value);
	}

	bool ReadCampaignMilitaryForceBoolField(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName,
		bool bDefaultValue = false)
	{
		bool bValue = bDefaultValue;
		if (Obj.IsValid())
		{
			Obj->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}

	TArray<FString> ReadCampaignMilitaryForceStringArray(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName)
	{
		TArray<FString> Result;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(FieldName, Values) || !Values)
		{
			return Result;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
			{
				Result.Add(Text);
			}
		}
		return Result;
	}

	FString ForceUnitLabel(const FString& UnitType)
	{
		const FString U = UnitType.ToLower();
		if (U == TEXT("mbt"))       return TEXT("Tanques");
		if (U == TEXT("ifv"))       return TEXT("Veh. combate");
		if (U == TEXT("apc"))       return TEXT("Transportes");
		if (U == TEXT("infantry"))  return TEXT("Infanteria");
		if (U == TEXT("aircraft"))  return TEXT("Cazas");
		if (U == TEXT("heli"))      return TEXT("Helicopteros");
		if (U == TEXT("ship"))      return TEXT("Buques");
		if (U == TEXT("artillery")) return TEXT("Artilleria");
		return UnitType;
	}

	// Lee "composition": [ {"unit":"mbt","count":20}, ... ] del JSON de la fuerza.
	TArray<FWLForceUnitGroup> ReadForceComposition(const TSharedPtr<FJsonObject>& Obj)
	{
		TArray<FWLForceUnitGroup> Result;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(TEXT("composition"), Values) || !Values)
		{
			return Result;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry || !(*Entry).IsValid())
			{
				continue;
			}
			FWLForceUnitGroup Group;
			Group.UnitType = ReadCampaignMilitaryForceStringField(*Entry, TEXT("unit")).ToLower();
			Group.Count = FMath::Max(0, ReadCampaignMilitaryForceIntField(*Entry, TEXT("count")));
			if (Group.UnitType.IsEmpty() || Group.Count <= 0)
			{
				continue;
			}
			Group.Label = ForceUnitLabel(Group.UnitType);
			Result.Add(MoveTemp(Group));
		}
		return Result;
	}

	// Composicion plausible derivada de la categoria + efectivos cuando el JSON no la trae (asi TODA
	// fuerza muestra cartas de tropas sin tener que editar cada entrada a mano).
	TArray<FWLForceUnitGroup> DeriveDefaultComposition(const FString& Category, int32 Strength)
	{
		auto Add = [](TArray<FWLForceUnitGroup>& Out, const TCHAR* Type, int32 Count, int32 Cap)
		{
			Count = FMath::Clamp(Count, 0, Cap);
			if (Count <= 0) { return; }
			FWLForceUnitGroup G;
			G.UnitType = Type;
			G.Label = ForceUnitLabel(Type);
			G.Count = Count;
			Out.Add(G);
		};
		TArray<FWLForceUnitGroup> Out;
		const int32 S = FMath::Max(0, Strength);
		const FString C = Category.ToLower();
		if (C.Contains(TEXT("naval")))
		{
			Add(Out, TEXT("ship"), FMath::RoundToInt(S / 700.f), 40);
			Add(Out, TEXT("infantry"), FMath::RoundToInt(S / 120.f), 200);
		}
		else if (C.Contains(TEXT("air")))
		{
			Add(Out, TEXT("aircraft"), FMath::RoundToInt(S / 130.f), 200);
			Add(Out, TEXT("heli"), FMath::RoundToInt(S / 360.f), 120);
		}
		else
		{
			Add(Out, TEXT("infantry"), FMath::RoundToInt(S / 85.f), 400);
			Add(Out, TEXT("mbt"), FMath::RoundToInt(S / 1400.f), 60);
			Add(Out, TEXT("ifv"), FMath::RoundToInt(S / 2000.f), 50);
			Add(Out, TEXT("apc"), FMath::RoundToInt(S / 2400.f), 50);
			Add(Out, TEXT("artillery"), FMath::RoundToInt(S / 3500.f), 30);
		}
		return Out;
	}

	FLinearColor CampaignMilitaryForceColor(const FWLCampaign3DForceView& Force)
	{
		const FString Category = Force.MarkerCategory.ToLower();
		if (Category.Contains(TEXT("naval")))
		{
			return FLinearColor(0.20f, 0.70f, 0.88f, 1.f);
		}
		if (Category.Contains(TEXT("air")))
		{
			return FLinearColor(0.72f, 0.86f, 0.96f, 1.f);
		}
		if (Category.Contains(TEXT("urban")))
		{
			return FLinearColor(0.58f, 0.66f, 0.68f, 1.f);
		}
		return Force.CountryIso.Equals(TEXT("VE"), ESearchCase::IgnoreCase)
			? FLinearColor(0.84f, 0.50f, 0.30f, 1.f)
			: FLinearColor(0.74f, 0.68f, 0.36f, 1.f);
	}

	FVector CampaignMilitaryForceScale(const FWLCampaign3DForceView& Force)
	{
		const FString Category = Force.MarkerCategory.ToLower();
		if (Category.Contains(TEXT("naval")))
		{
			return FVector(10.5f, 10.5f, 8.6f);
		}
		if (Category.Contains(TEXT("air")))
		{
			return FVector(9.2f, 9.2f, 10.8f);
		}
		if (Category.Contains(TEXT("urban")))
		{
			return FVector(8.0f, 8.0f, 10.8f);
		}
		return FVector(9.0f, 9.0f, 13.0f);
	}

	void LoadCampaignMilitaryForceDefinitions(TArray<FWLCampaign3DForceView>& OutForces)
	{
		OutForces.Reset();
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign3D") / TEXT("MilitaryForces.json");
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *Path))
		{
			return;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root->TryGetArrayField(TEXT("forces"), Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
			{
				continue;
			}

			FWLCampaign3DForceView Force;
			Force.Id = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("id"));
			Force.Name = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("name"));
			Force.CountryIso = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("country_iso")).ToUpper();
			Force.CountryName = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("country_name"));
			Force.ForceType = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("force_type"));
			Force.MarkerCategory = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("marker_category"), TEXT("land"));
			Force.MovementNodeId = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("movement_node_id"));
			Force.MovementStatus = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("movement_status"), TEXT("detenido"));
			Force.LocationName = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("location"));
			Force.ProvinceId = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("province_id"));
			Force.ProvinceName = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("province"));
			Force.NearbyCity = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("nearby_city"));
			Force.Lon = ReadCampaignMilitaryForceNumberField(*ObjPtr, TEXT("lon"));
			Force.Lat = ReadCampaignMilitaryForceNumberField(*ObjPtr, TEXT("lat"));
			Force.EstimatedStrength = FMath::Max(0, ReadCampaignMilitaryForceIntField(*ObjPtr, TEXT("strength")));
			Force.Mobility = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("mobility"), TEXT("media"));
			Force.OperationalState = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("operational_state"), TEXT("placeholder"));
			Force.Supply = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("supply"), TEXT("sin datos"));
			Force.Morale = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("morale"), TEXT("sin datos"));
			Force.Posture = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("posture"), TEXT("observacion"));
			Force.StrategicRole = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("strategic_role"), TEXT("presencia militar placeholder"));
			Force.DetailLevel = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("detail_level"), TEXT("placeholder force marker"));
			Force.DisabledActions = ReadCampaignMilitaryForceStringArray(*ObjPtr, TEXT("disabled_actions"));
			if (Force.DisabledActions.IsEmpty())
			{
				Force.DisabledActions = {
					TEXT("Mover"),
					TEXT("Atacar"),
					TEXT("Reorganizar"),
					TEXT("Reforzar"),
					TEXT("Ver composicion"),
					TEXT("Abrir logistica"),
					TEXT("Auto-resolve")
				};
			}

			const FString Category = Force.MarkerCategory.ToLower();
			Force.bAir = Category.Contains(TEXT("air"));
			Force.bNaval = Category.Contains(TEXT("naval"));
			Force.bMovable = ReadCampaignMilitaryForceBoolField(*ObjPtr, TEXT("movable"), !Force.bAir);
			Force.Composition = ReadForceComposition(*ObjPtr);
			if (Force.Composition.IsEmpty())
			{
				Force.Composition = DeriveDefaultComposition(Force.MarkerCategory, Force.EstimatedStrength);
			}
			if (!Force.Id.IsEmpty()
				&& !Force.Name.IsEmpty()
				&& FWLCampaignRegionGeometry::IsTheaterIso(Force.CountryIso)
				&& !FMath::IsNearlyZero(Force.Lon)
				&& !FMath::IsNearlyZero(Force.Lat))
			{
				OutForces.Add(MoveTemp(Force));
			}
		}

}
}

void AWLCampaign3DView::AddMilitaryForceMarkers()
{
	TArray<FWLCampaign3DForceView> LoadedForces;
	LoadCampaignMilitaryForceDefinitions(LoadedForces);
	LoadedForces.Sort([](const FWLCampaign3DForceView& A, const FWLCampaign3DForceView& B)
	{
		return A.Id < B.Id;
	});

	for (FWLCampaign3DForceView Force : LoadedForces)
	{
		Force.WorldLocation = ProjectLonLat(Force.Lon, Force.Lat) + FVector(0.f, 0.f, Force.bNaval ? 2350.f : 2600.f);
		AddMilitaryForceMarker(Force);
	}

	// Materializa ejercitos para guarniciones ya reclutadas (p.ej. al cargar partida). En juego nuevo no
	// hay ninguna, asi que no crea nada.
	SyncRecruitedArmyTokens();
}

void AWLCampaign3DView::AddMilitaryForceMarker(const FWLCampaign3DForceView& Force, bool bSpawnTokenMesh)
{
	if (Force.Id.IsEmpty())
	{
		return;
	}

	// Marcadores de fuerza OCULTOS por ahora: son placeholder (sin gameplay activo) y ensuciaban las
	// ciudades con conos dorados ("triangulos amarillos") + una etiqueta que repetia el nombre de la
	// ciudad (el segundo "Caracas"). Registramos los datos en ForceViews (paneles/reactivacion) pero
	// NO creamos geometria ni hitbox (que ademas se tragaba el click de la ciudad). Para reactivarlos
	// cuando las fuerzas sean jugables: bShowMilitaryForceMarkers = true. Es todo-o-nada, asi que las
	// arrays paralelas de marcadores quedan vacias de forma consistente (Queries usa IsValidIndex).
	if (!bShowMilitaryForceMarkers)
	{
		ForceViews.Add(Force);
		return;
	}

	const int32 ForceIndex = ForceViews.Num();
	const FLinearColor BaseColor = CampaignMilitaryForceColor(Force);
	const float TargetTokenWidth = Force.bNaval ? 4200.f : (Force.bAir ? 3600.f : 2900.f);

	// Token de UNIDAD (NPC) SOLO si se pide. Los FUERTES NO llevan token: son un edificio clicable (la
	// guarnicion se ve en el panel; las tropas como icono movil son un paso aparte). Sin token -> Marker
	// nullptr (las arrays paralelas lo toleran; Queries/LOD null-chequean).
	UStaticMeshComponent* Marker = nullptr;
	FVector TokenScale(1.f, 1.f, 1.f);
	if (bSpawnTokenMesh)
	{
		UStaticMesh* TokenMesh = Force.bNaval ? ForceTokenNavalMesh
			: (Force.bAir ? ForceTokenAirMesh : ForceTokenLandMesh);
		if (!TokenMesh)
		{
			TokenMesh = ConeMesh;
		}
		if (TokenMesh)
		{
			const FVector Ext = TokenMesh->GetBounds().BoxExtent;
			const float ModelW = FMath::Max(1.f, FMath::Max(Ext.X, Ext.Y) * 2.f);
			const float TokenScaleU = TargetTokenWidth / ModelW;
			TokenScale = FVector(TokenScaleU, TokenScaleU, TokenScaleU);
			Marker = NewObject<UStaticMeshComponent>(this);
			if (Marker)
			{
				Marker->SetupAttachment(SceneRoot);
				Marker->RegisterComponent();
				Marker->SetStaticMesh(TokenMesh);
				Marker->SetWorldLocation(Force.WorldLocation);
				Marker->SetWorldRotation(FRotator(0.f, Force.bNaval ? 45.f : 0.f, 0.f));
				Marker->SetWorldScale3D(TokenScale);
				// El TOKEN (la unidad visible) es CLICABLE: bloquea el trazo de seleccion. Asi "si ves el
				// tanque, lo puedes clicar" (el LOD activa/desactiva esta colision junto con la visibilidad,
				// evitando el bug de "visible pero no seleccionable"). TryGetForceForComponent lo reconoce.
				Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Marker->SetCollisionObjectType(ECC_WorldDynamic);
				Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
				Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
				Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DForce"));
				Marker->ComponentTags.Add(FName(*Force.Id));
				// Material UNLIT (M_VehicleUnlit) -> el tanque muestra su vertex-color tan SIEMPRE igual, sin
				// oscurecerse por luz/sombra (el VertexColorMaterial del Engine es LIT). Fallback al lit.
				UMaterialInterface* TokenMat = ForceTokenMaterial ? ForceTokenMaterial : VertexColorMaterial;
				if (TokenMat)
				{
					const int32 NumMats = FMath::Max(1, Marker->GetNumMaterials());
					for (int32 i = 0; i < NumMats; ++i)
					{
						Marker->SetMaterial(i, TokenMat);
					}
				}
			}
		}
	}

	// Hitbox PEQUENO sobre el token. Antes era enorme (11800u): tapaba la ciudad y se tragaba su click
	// (doc #10). Ahora solo el area del token selecciona la fuerza; el clic en la ciudad sigue yendo a
	// la ciudad.
	USphereComponent* SelectionProxy = NewObject<USphereComponent>(this);
	if (!SelectionProxy)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
		return;
	}
	SelectionProxy->SetupAttachment(SceneRoot);
	SelectionProxy->RegisterComponent();
	SelectionProxy->SetWorldLocation(GetForceSelectionProxyLocation(Force, Force.WorldLocation));
	// El fuerte y el tanque quedan cerca, pero sus hitboxes NO deben solaparse: clicar fuerte selecciona
	// fuerte; clicar tanque selecciona ejercito. El fallback por proximidad se mantiene todavia mas chico.
	SelectionProxy->SetSphereRadius(bSpawnTokenMesh ? TargetTokenWidth * 0.64f : 2200.f);
	SelectionProxy->SetVisibility(false, true);
	SelectionProxy->SetHiddenInGame(false, true);
	SelectionProxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectionProxy->SetCollisionObjectType(ECC_WorldDynamic);
	SelectionProxy->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionProxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SelectionProxy->ComponentTags.Add(TEXT("WorldLeaderCampaign3DForce"));
	SelectionProxy->ComponentTags.Add(FName(*Force.Id));

	// Etiqueta = NOMBRE DE LA FUERZA (no la ciudad: antes duplicaba el nombre de la ciudad -> doc #10).
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	if (Label)
	{
		Label->SetupAttachment(SceneRoot);
		Label->RegisterComponent();
		Label->SetWorldLocation(GetForceLabelLocation(Force, Force.WorldLocation));
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(Force.bNaval ? 430.f : 390.f);
		Label->SetText(FText::FromString(Force.Name));
		Label->SetTextRenderColor(Force.bAir ? FColor(200, 225, 245) : FColor(230, 214, 148));
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ForceMarkerLabels.Add(Label);
	}

	ForceViews.Add(Force);
	ForceMarkerComponents.Add(Marker);
	ForceSelectionMarkers.Add(SelectionProxy);
	ForceMarkerBaseScales.Add(TokenScale);
	ForceMarkerBaseColors.Add(BaseColor);

	if (ForceMarkerComponents.IsValidIndex(ForceIndex) && ForceMarkerComponents[ForceIndex])
	{
		ForceMarkerComponents[ForceIndex]->SetVisibility(!IsHidden(), true);
	}
}

FVector AWLCampaign3DView::GroundedLandTokenLocationAtWorld(float WorldX, float WorldY) const
{
	// Superficie visible EXACTA: raycast hacia abajo contra SOLO el TerrainMesh (su colision = los vertices
	// dibujados, con bioma/pads ya incluidos). Ignora edificios/proxies -> el tanque apoya en el terreno.
	// Sirve para cualquier punto del mundo (incluido a MITAD de carretera, no solo en ciudades).
	float SurfaceZ = GetGroundWorldZAtWorld(WorldX, WorldY);
	if (TerrainMesh)
	{
		const FVector Start(WorldX, WorldY, SurfaceZ + 600000.f);
		const FVector End(WorldX, WorldY, SurfaceZ - 600000.f);
		FHitResult Hit;
		FCollisionQueryParams Q(FName(TEXT("WLArmyGround")), true);
		if (TerrainMesh->LineTraceComponent(Hit, Start, End, Q))
		{
			SurfaceZ = Hit.ImpactPoint.Z;
		}
		else
		{
			SurfaceZ += 120.f;   // respaldo si el raycast no impacta
		}
	}

	// Asiento: lleva el punto MAS BAJO del modelo a la superficie (admite cualquier pivote del FBX).
	float Seat = 0.f;
	if (ForceTokenLandMesh)
	{
		const FVector Ext = ForceTokenLandMesh->GetBounds().BoxExtent;
		const FVector Orig = ForceTokenLandMesh->GetBounds().Origin;
		const float ModelW = FMath::Max(1.f, FMath::Max(Ext.X, Ext.Y) * 2.f);
		const float Scale = 3600.f / ModelW;   // = TargetTokenWidth terrestre en AddMilitaryForceMarker
		Seat = (Ext.Z - Orig.Z) * Scale;
	}

	return FVector(WorldX, WorldY, SurfaceZ + Seat);
}

FVector AWLCampaign3DView::GroundedLandTokenLocation(float Lon, float Lat) const
{
	const FVector Projected = ProjectLonLat(Lon, Lat);
	return GroundedLandTokenLocationAtWorld(Projected.X, Projected.Y);
}

void AWLCampaign3DView::SyncRecruitedArmyTokens()
{
	const UWLStrategicTickSubsystem* Tick = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
	if (!Tick)
	{
		return;
	}

	// Recolecta los FUERTES primero: abajo se ANADEN ejercitos a ForceViews, lo que invalidaria las
	// referencias si iteraramos el array en vivo.
	struct FFortInfo { FString Id; FString Name; FString Iso; FVector Loc; float Lon; float Lat; };
	TArray<FFortInfo> Forts;
	const FString PlayerIso = ActivePlayerNationIso.TrimStartAndEnd().ToUpper();
	for (const FWLCampaign3DForceView& F : ForceViews)
	{
		if (!F.bIsRecruitmentBase)
		{
			continue;
		}
		// SOLO los fuertes de TU pais despliegan ejercito: un fuerte del otro lado de la frontera no crea
		// tropa para el jugador. (Si no hay nacion activa por algun motivo, no filtramos para no dejar el
		// mapa sin ejercitos.) El reclutamiento ya se bloquea en fuertes ajenos; esto lo garantiza tambien
		// en el despliegue del token.
		if (!PlayerIso.IsEmpty() && !F.CountryIso.Equals(PlayerIso, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Forts.Add({ F.Id, F.Name, F.CountryIso, F.WorldLocation, F.Lon, F.Lat });
	}

	UWLMilitarySubsystem* Military = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWLMilitarySubsystem>() : nullptr;
	UWLCharacterSubsystem* Characters = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWLCharacterSubsystem>() : nullptr;

	for (const FFortInfo& Fort : Forts)
	{
		const TArray<FWLGarrisonGroup> Garrison = Tick->GetGarrisonRecruited(Fort.Id);
		if (Garrison.Num() == 0)
		{
			continue;   // nada reclutado todavia -> el fuerte aun no despliega ejercito
		}

		TArray<FWLForceUnitGroup> Comp;
		TArray<TPair<FString, int32>> GarrisonUnits;
		for (const FWLGarrisonGroup& G : Garrison)
		{
			FWLForceUnitGroup U;
			U.UnitType = G.UnitType;
			U.Label = G.Label;
			U.Count = G.Count;
			Comp.Add(U);
			GarrisonUnits.Add(TPair<FString, int32>(G.UnitType, G.Count));
		}

		const FString ArmyId = TEXT("ARMY-") + Fort.Id;

		// Si el ejercito ya existe, solo sincroniza su composicion (puede haberse movido: no tocar posicion)
		// y refresca el ejercito REAL del backend con la nueva tropa.
		bool bExists = false;
		for (FWLCampaign3DForceView& F : ForceViews)
		{
			if (F.Id.Equals(ArmyId, ESearchCase::IgnoreCase))
			{
				F.Composition = Comp;
				if (Military)
				{
					Military->SyncArmyFromGarrison(Fort.Id, Fort.Iso, F.ProvinceId, GarrisonUnits);
				}
				bExists = true;
				break;
			}
		}
		if (bExists)
		{
			continue;
		}

		// Crea el ejercito movible cerca del fuerte, desplazado hacia el nodo de carretera mas cercano (asi
		// queda "sobre la ruta" y su hitbox no se solapa con la del fuerte).
		FWLCampaign3DForceView Army;
		Army.Id = ArmyId;
		// Nombre limpio: "Ejercito de <region>" (quita "border fort"/"fronterizo"). Cada fuerte (Este, Sur,
		// Norte...) produce su PROPIO ejercito, asi que un pais puede tener muchos desplegados a la vez.
		FString Region = Fort.Name;
		Region.ReplaceInline(TEXT(" border fort"), TEXT(""), ESearchCase::IgnoreCase);
		Region.ReplaceInline(TEXT("Fuerte fronterizo"), TEXT(""), ESearchCase::IgnoreCase);
		Region.ReplaceInline(TEXT(" fronterizo"), TEXT(""), ESearchCase::IgnoreCase);
		Region.TrimStartAndEndInline();
		Army.Name = FString::Printf(TEXT("Ejercito de %s"), Region.IsEmpty() ? *Fort.Name : *Region);
		Army.CountryIso = Fort.Iso;
		Army.CountryName = Fort.Iso;
		Army.ForceType = TEXT("Ejercito de campo");
		Army.MarkerCategory = TEXT("land");
		Army.Mobility = TEXT("movil");
		Army.OperationalState = TEXT("desplegado");
		Army.Posture = TEXT("en marcha");
		Army.bMovable = true;
		Army.bIsRecruitmentBase = false;
		Army.Composition = Comp;

		// Posiciona el ejercito junto al fuerte, desplazado ~5000u hacia el nodo de carretera mas cercano
		// (para no solaparse con el edificio) y APOYADO en el terreno via raycast (GroundedLandTokenLocation
		// -> imposible que flote). El bug anterior anclaba la Z del fuerte en una posicion lejana con otro
		// relieve.
		float ArmyLon = Fort.Lon;
		float ArmyLat = Fort.Lat;
		FWLCampaign3DForceView Probe;
		Probe.WorldLocation = ProjectLonLat(Fort.Lon, Fort.Lat);
		const FString NearestId = FindNearestMovementNodeId(Probe);
		if (const FWLCampaign3DMovementNodeView* Node = FindMovementNodeById(NearestId))
		{
			FVector2D Dir(Node->Lon - Fort.Lon, Node->Lat - Fort.Lat);
			if (Dir.SizeSquared() > KINDA_SMALL_NUMBER)
			{
				Dir.Normalize();
				// JUSTO al lado del fuerte (no en la ciudad vecina): ~2850u hacia la carretera, fuera del
				// hitbox del fuerte (2200) para que cada uno se clique por separado.
				ArmyLon = Fort.Lon + Dir.X * 0.08f;
				ArmyLat = Fort.Lat + Dir.Y * 0.08f;
			}
			Army.MovementNodeId = NearestId;
			Army.LocationName = Node->Name;
			Army.NearbyCity = Node->Name;
			Army.ProvinceId = Node->ProvinceId;
			Army.ProvinceName = Node->ProvinceName;
		}
		// Nace SOBRE la carretera mas cercana al fuerte: asi el ejercito entra/sale por la via que tiene ENFRENTE.
		// PERO solo si esa via esta DE VERDAD junto al fuerte: las polilineas de conduccion son solo las rutas
		// curadas del teatro, y algunos cruces (p.ej. La Guajira/Maicao) no tienen una, asi que la "mas cercana"
		// podia caer a >300km, en OTRA frontera -> el token aparecia lejos (bug "reclutas en Maicao y el ejercito
		// sale en Cucuta"). Con el limite, si no hay via al lado, lo dejamos pegado al fuerte (se une a la red al
		// moverlo). Si hay via cercana, cae al desplazamiento hacia el nodo.
		const FVector FortWorld = ProjectLonLat(Fort.Lon, Fort.Lat);
		const float MaxRoadSnapWorld = 45.f * DetailWorldUnitsPerKm;   // 45 km: al lado del fuerte, no otra frontera
		FVector RoadSpawn;
		if (NearestRoadPointWorld(FortWorld, RoadSpawn)
			&& FVector::Dist2D(FortWorld, RoadSpawn) <= MaxRoadSnapWorld)
		{
			Army.WorldLocation = GroundedLandTokenLocationAtWorld(RoadSpawn.X, RoadSpawn.Y);
		}
		else
		{
			Army.WorldLocation = GroundedLandTokenLocation(ArmyLon, ArmyLat);
		}
		Army.Lon = ArmyLon;
		Army.Lat = ArmyLat;

		int32 Total = 0;
		for (const FWLForceUnitGroup& U : Comp)
		{
			Total += U.Count;
		}
		Army.EstimatedStrength = Total;

		// Enlace con el backend militar: crea el FWLArmy REAL (con general nombrado, F1.4) y el token
		// pasa a llamarse por su general. Sin esto, lo que se ve en el mapa no existia para el juego.
		if (Military)
		{
			const FString BackendArmyId = Military->SyncArmyFromGarrison(Fort.Id, Fort.Iso, Army.ProvinceId, GarrisonUnits);
			FWLCharacter General;
			if (Characters && !BackendArmyId.IsEmpty()
				&& Characters->GetAssignedGeneralForArmy(BackendArmyId, General))
			{
				Army.ForceType = Army.Name;   // "Ejercito de <region>" pasa a subtitulo
				Army.Name = General.Name;     // el token se llama por su general
				Army.StrategicRole = FString::Printf(TEXT("Al mando: %s (skill %d, renombre %d)"),
					*General.Name, General.Skill, General.Renown);
			}
		}

		AddMilitaryForceMarker(Army, true);
	}
}

bool AWLCampaign3DView::ForceHasTroopsForToken(int32 Index) const
{
	if (!ForceViews.IsValidIndex(Index))
	{
		return false;
	}
	const FWLCampaign3DForceView& Force = ForceViews[Index];
	if (Force.Composition.Num() > 0)
	{
		return true;   // fuerzas de ciudad: composicion base -> siempre visibles
	}
	// Fuertes (guarnicion vacia al inicio): el token aparece cuando se ha reclutado algo.
	if (const UWLStrategicTickSubsystem* Tick = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr)
	{
		return Tick->GetGarrisonRecruited(Force.Id).Num() > 0;
	}
	return false;
}

void AWLCampaign3DView::RefreshMilitaryForceMarkerVisuals()
{
	for (int32 Index = 0; Index < ForceMarkerComponents.Num(); ++Index)
	{
		UStaticMeshComponent* Marker = ForceMarkerComponents[Index];
		if (!Marker || !ForceViews.IsValidIndex(Index))
		{
			continue;
		}

		const FWLCampaign3DForceView& Force = ForceViews[Index];

		// Token visible solo si la fuerza TIENE TROPAS (un fuerte vacio no muestra tanque hasta reclutar).
		Marker->SetVisibility(ForceHasTroopsForToken(Index) && !IsHidden(), true);

		const bool bSelected = !SelectedForceHighlightId.IsEmpty() && Force.Id.Equals(SelectedForceHighlightId, ESearchCase::IgnoreCase);
		const bool bHovered = !HoveredForceHighlightId.IsEmpty() && Force.Id.Equals(HoveredForceHighlightId, ESearchCase::IgnoreCase);
		const FVector BaseScale = ForceMarkerBaseScales.IsValidIndex(Index) ? ForceMarkerBaseScales[Index] : CampaignMilitaryForceScale(Force);
		// Seleccion/hover = solo MAS GRANDE. NO se cambia el material: el token conserva su material UNLIT de
		// vertex-color (tan constante). Antes se sobrescribia con un color solido LIT que oscurecia el tanque.
		Marker->SetWorldScale3D(BaseScale * (bSelected ? 1.42f : (bHovered ? 1.22f : 1.0f)));
	}
}
