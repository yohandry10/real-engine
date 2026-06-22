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
		if (Category.Contains(TEXT("mechanized")))
		{
			return FLinearColor(0.91f, 0.62f, 0.24f, 1.f);
		}
		if (Category.Contains(TEXT("frontier")))
		{
			return FLinearColor(0.86f, 0.74f, 0.32f, 1.f);
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
		if (Category.Contains(TEXT("mechanized")))
		{
			return FVector(9.5f, 9.5f, 14.0f);
		}
		if (Category.Contains(TEXT("frontier")))
		{
			return FVector(8.4f, 8.4f, 12.0f);
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
	if (Force.Id.IsEmpty() || !ConeMesh)
	{
		return;
	}

	const int32 ForceIndex = ForceViews.Num();
	const FLinearColor BaseColor = CampaignMilitaryForceColor(Force);
	const FVector BaseScale = CampaignMilitaryForceScale(Force);

	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
	if (!Marker)
	{
		return;
	}
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetStaticMesh(ConeMesh);
	Marker->SetWorldLocation(Force.WorldLocation);
	Marker->SetWorldRotation(FRotator(0.f, Force.bNaval ? 45.f : 0.f, 0.f));
	Marker->SetWorldScale3D(BaseScale);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DForce"));
	Marker->ComponentTags.Add(FName(*Force.Id));
	if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(BaseColor))
	{
		Marker->SetMaterial(0, Mat);
	}

	USphereComponent* SelectionProxy = NewObject<USphereComponent>(this);
	if (!SelectionProxy)
	{
		Marker->DestroyComponent();
		return;
	}
	SelectionProxy->SetupAttachment(SceneRoot);
	SelectionProxy->RegisterComponent();
	SelectionProxy->SetWorldLocation(Force.WorldLocation + FVector(0.f, 0.f, 1400.f));
	SelectionProxy->SetSphereRadius(Force.bNaval ? 14500.f : (Force.bAir ? 13200.f : 11800.f));
	SelectionProxy->SetVisibility(false, true);
	SelectionProxy->SetHiddenInGame(false, true);
	SelectionProxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectionProxy->SetCollisionObjectType(ECC_WorldDynamic);
	SelectionProxy->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionProxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SelectionProxy->ComponentTags.Add(TEXT("WorldLeaderCampaign3DForce"));
	SelectionProxy->ComponentTags.Add(FName(*Force.Id));

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	if (Label)
	{
		Label->SetupAttachment(SceneRoot);
		Label->RegisterComponent();
		Label->SetWorldLocation(Force.WorldLocation + FVector(0.f, 0.f, 2550.f));
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(Force.bNaval ? 430.f : 390.f);
		Label->SetText(FText::FromString(Force.NearbyCity.IsEmpty() ? Force.Name : Force.NearbyCity));
		Label->SetTextRenderColor(Force.bAir ? FColor(200, 225, 245) : FColor(230, 214, 148));
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ForceMarkerLabels.Add(Label);
	}

	ForceViews.Add(Force);
	ForceMarkerComponents.Add(Marker);
	ForceSelectionMarkers.Add(SelectionProxy);
	ForceMarkerBaseScales.Add(BaseScale);
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
