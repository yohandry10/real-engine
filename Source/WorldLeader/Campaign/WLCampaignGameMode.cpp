// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignGameMode.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "UI/WLCampaignHUD.h"
#include "Map/WLWorldMap.h"
#include "GameFramework/DefaultPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AWLCampaignGameMode::AWLCampaignGameMode()
{
	PlayerControllerClass = AWLCampaignPlayerController::StaticClass();
	HUDClass = AWLCampaignHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
}

void AWLCampaignGameMode::BeginPlay()
{
	Super::BeginPlay();

	const UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (GI && GI->HasActiveCampaign())
	{
		StartCampaignWorld(GI->GetSelectedNationIso());
	}
}

void AWLCampaignGameMode::StartCampaignWorld(const FString& NationIso)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (CampaignMap)
	{
		CampaignMap->Destroy();
		CampaignMap = nullptr;
	}

	FVector2D InitialLonLat(-75.f, 12.f);
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	const UWLDataRegistry* Registry = GameInstance ? GameInstance->GetSubsystem<UWLDataRegistry>() : nullptr;
	FWLNationData Nation;
	FWLProvinceData Capital;
	if (Registry && Registry->GetNation(NationIso, Nation) && Registry->GetProvince(Nation.CapitalProvinceId, Capital))
	{
		InitialLonLat = FVector2D(Capital.MapLon, Capital.MapLat);
	}

	AWLWorldMap* NewMap = World->SpawnActorDeferred<AWLWorldMap>(
		AWLWorldMap::StaticClass(),
		FTransform::Identity);
	if (!NewMap)
	{
		return;
	}

	NewMap->InitialCameraLonLat = InitialLonLat;
	UGameplayStatics::FinishSpawningActor(NewMap, FTransform::Identity);
	CampaignMap = NewMap;
}
