// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignGameMode.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "UI/WLCampaignHUD.h"
#include "Map/WLWorldMap.h"
#include "Presentation/WLCampaign3DView.h"
#include "Camera/CameraActor.h"
#include "GameFramework/DefaultPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AWLCampaignGameMode::AWLCampaignGameMode()
{
	PlayerControllerClass = AWLCampaignPlayerController::StaticClass();
	HUDClass = AWLCampaignHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
}

void AWLCampaignGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	FString AutoStartIso;
	if (GI && !GI->HasActiveCampaign() && FParse::Value(FCommandLine::Get(), TEXT("WLAutoStart="), AutoStartIso))
	{
		GI->StartNewCampaign(AutoStartIso);
	}
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

	if (Campaign3DView)
	{
		Campaign3DView->Destroy();
		Campaign3DView = nullptr;
	}
	if (DiplomacyMapView)
	{
		DiplomacyMapView->Destroy();
		DiplomacyMapView = nullptr;
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

	AWLCampaign3DView* NewCampaign3D = World->SpawnActorDeferred<AWLCampaign3DView>(
		AWLCampaign3DView::StaticClass(),
		FTransform::Identity);
	if (NewCampaign3D)
	{
		UGameplayStatics::FinishSpawningActor(NewCampaign3D, FTransform::Identity);
		NewCampaign3D->BuildView(NationIso);
		if (FParse::Param(FCommandLine::Get(), TEXT("WLAmericaView")))
		{
			if (ACameraActor* Camera = NewCampaign3D->GetViewCamera())
			{
				FVector FocusPoint = NewCampaign3D->GetTheaterFocusPoint();
				const FBox2D AmericaBounds = NewCampaign3D->GetCameraBounds2D(3200000.f);
				if (AmericaBounds.bIsValid)
				{
					const FVector2D Center = AmericaBounds.GetCenter();
					FocusPoint.X = Center.X;
					FocusPoint.Y = Center.Y;
				}
				Camera->SetActorLocation(FVector(FocusPoint.X, FocusPoint.Y, 4000000.f));
				Camera->SetActorRotation(FRotator(-90.f, 0.f, 0.f));
				NewCampaign3D->ApplyZoomLOD(4000000.f);
			}
		}
		NewCampaign3D->SetPresentationActive(true, true);
		Campaign3DView = NewCampaign3D;
	}

	AWLWorldMap* NewDiplomacyMap = World->SpawnActorDeferred<AWLWorldMap>(
		AWLWorldMap::StaticClass(),
		FTransform::Identity);
	if (!NewDiplomacyMap)
	{
		return;
	}

	NewDiplomacyMap->InitialCameraLonLat = InitialLonLat;
	NewDiplomacyMap->InitialCameraHeight = 165000.f;
	NewDiplomacyMap->bActivateCameraOnBuild = false;
	UGameplayStatics::FinishSpawningActor(NewDiplomacyMap, FTransform::Identity);
	NewDiplomacyMap->SetPresentationActive(false, false);
	DiplomacyMapView = NewDiplomacyMap;
}
