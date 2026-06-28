// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaign3DView.h"

#include "WorldLeader.h"
#include "Campaign/WLDataRegistry.h"
#include "Presentation/WLCampaignOverviewBuilder.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "Presentation/WLCampaignRouteBuilder.h"
#include "Presentation/WLCampaignSettlementBuilder.h"
#include "Presentation/WLCampaignTerrainBuilder.h"
#include "Presentation/WLCampaignVisualStyle.h"
#include "Presentation/WLCampaignWaterBuilder.h"
#include "ProceduralMeshComponent.h"
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
}

void AWLCampaign3DView::AddMilitaryForceMarker(const FWLCampaign3DForceView& Force)
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

	// Token de ejercito (NPC) = modelo de unidad por categoria (tanque/buque/caza). Respaldo: cono.
	UStaticMesh* TokenMesh = Force.bNaval ? ForceTokenNavalMesh
		: (Force.bAir ? ForceTokenAirMesh : ForceTokenLandMesh);
	if (!TokenMesh)
	{
		TokenMesh = ConeMesh;
	}
	if (!TokenMesh)
	{
		return;
	}

	const int32 ForceIndex = ForceViews.Num();
	const FLinearColor BaseColor = CampaignMilitaryForceColor(Force);

	// El modelo es 1u~=1m; el mapa mide decenas de km. Escalamos a un ANCHO DE TOKEN visible (icono de
	// ejercito tipo Total War, no a escala real).
	const float TargetTokenWidth = Force.bNaval ? 5200.f : (Force.bAir ? 4400.f : 3600.f);
	const FVector Ext = TokenMesh->GetBounds().BoxExtent;
	const float ModelW = FMath::Max(1.f, FMath::Max(Ext.X, Ext.Y) * 2.f);
	const float TokenScaleU = TargetTokenWidth / ModelW;
	const FVector TokenScale(TokenScaleU, TokenScaleU, TokenScaleU);
	const float LabelZ = TargetTokenWidth * 0.95f;

	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
	if (!Marker)
	{
		return;
	}
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetStaticMesh(TokenMesh);
	Marker->SetWorldLocation(Force.WorldLocation);
	Marker->SetWorldRotation(FRotator(0.f, Force.bNaval ? 45.f : 0.f, 0.f));
	Marker->SetWorldScale3D(TokenScale);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DForce"));
	Marker->ComponentTags.Add(FName(*Force.Id));
	// Material UNLIT vertex-color (igual que ciudades/vehiculos): muestra el camuflaje del modelo y
	// queda coherente con el mapa (un material LIT saldria oscuro -> doc #4).
	if (VertexColorMaterial)
	{
		const int32 NumMats = FMath::Max(1, Marker->GetNumMaterials());
		for (int32 i = 0; i < NumMats; ++i)
		{
			Marker->SetMaterial(i, VertexColorMaterial);
		}
	}

	// Hitbox PEQUENO sobre el token. Antes era enorme (11800u): tapaba la ciudad y se tragaba su click
	// (doc #10). Ahora solo el area del token selecciona la fuerza; el clic en la ciudad sigue yendo a
	// la ciudad.
	USphereComponent* SelectionProxy = NewObject<USphereComponent>(this);
	if (!SelectionProxy)
	{
		Marker->DestroyComponent();
		return;
	}
	SelectionProxy->SetupAttachment(SceneRoot);
	SelectionProxy->RegisterComponent();
	SelectionProxy->SetWorldLocation(Force.WorldLocation + FVector(0.f, 0.f, TargetTokenWidth * 0.30f));
	SelectionProxy->SetSphereRadius(TargetTokenWidth * 0.72f);
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
		Label->SetWorldLocation(Force.WorldLocation + FVector(0.f, 0.f, LabelZ));
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

	if (ForceMarkerComponents.IsValidIndex(ForceIndex))
	{
		ForceMarkerComponents[ForceIndex]->SetVisibility(!IsHidden(), true);
	}
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
		const bool bSelected = !SelectedForceHighlightId.IsEmpty() && Force.Id.Equals(SelectedForceHighlightId, ESearchCase::IgnoreCase);
		const bool bHovered = !HoveredForceHighlightId.IsEmpty() && Force.Id.Equals(HoveredForceHighlightId, ESearchCase::IgnoreCase);
		const FVector BaseScale = ForceMarkerBaseScales.IsValidIndex(Index) ? ForceMarkerBaseScales[Index] : CampaignMilitaryForceScale(Force);
		const FLinearColor BaseColor = ForceMarkerBaseColors.IsValidIndex(Index) ? ForceMarkerBaseColors[Index] : CampaignMilitaryForceColor(Force);
		Marker->SetWorldScale3D(BaseScale * (bSelected ? 1.42f : (bHovered ? 1.22f : 1.0f)));

		FLinearColor DisplayColor = BaseColor;
		if (bSelected)
		{
			DisplayColor = (BaseColor * 0.58f) + FLinearColor(0.96f, 0.78f, 0.28f, 1.f) * 0.42f;
			DisplayColor.A = 1.f;
		}
		else if (bHovered)
		{
			DisplayColor = (BaseColor * 0.72f) + FLinearColor(0.82f, 0.96f, 1.0f, 1.f) * 0.28f;
			DisplayColor.A = 1.f;
		}

		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(DisplayColor))
		{
			Marker->SetMaterial(0, Mat);
		}
	}
}
