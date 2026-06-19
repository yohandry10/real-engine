// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

enum class EWLCampaignSettlementType : uint8
{
	Capital,
	LargeCity,
	Port,
	Frontier,
	Industrial,
	MediumCity
};

struct FWLCampaignSettlementSpec
{
	FString Name;
	float Lon = 0.f;
	float Lat = 0.f;
	EWLCampaignSettlementType Type = EWLCampaignSettlementType::MediumCity;
	FLinearColor AccentColor = FLinearColor::White;
	float Scale = 1.f;
	float YawDegrees = 0.f;
};

class FWLCampaignSettlementBuilder
{
public:
	static void AddSettlement(
		UProceduralMeshComponent* SettlementMesh,
		UMaterialInterface* SettlementMaterial,
		const FWLCampaignSettlementSpec& Spec,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);
};
