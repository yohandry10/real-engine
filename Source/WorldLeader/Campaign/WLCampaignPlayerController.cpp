// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLCampaignGameMode.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Economy/WLEconomyLibrary.h"
#include "Map/WLWorldMap.h"
#include "Presentation/WLCampaign3DView.h"
#include "UI/WLCampaignSelectionPanelData.h"
#include "UI/WLMainMenuWidget.h"
#include "WorldLeader.h"
#include "Camera/CameraActor.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

namespace
{
	const FWLCampaignSelectionPanelData& GetCampaignControllerPanelData()
	{
		static FWLCampaignSelectionPanelData Data;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			FWLCampaignSelectionPanelDataLoader::Load(Data);
			bLoaded = true;
		}
		return Data;
	}

	const FWLCampaignBuildingSlotCatalog& GetCampaignControllerBuildingCatalog()
	{
		static FWLCampaignBuildingSlotCatalog Catalog;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			FWLCampaignBuildingSlotDataLoader::Load(Catalog);
			bLoaded = true;
		}
		return Catalog;
	}

	TArray<FString> GetCampaignControllerPanelSlots(const AWLCampaignPlayerController* PC)
	{
		TArray<FString> Slots;
		if (!PC || !PC->HasCampaignSelectionPanel())
		{
			return Slots;
		}

		const FWLCampaignSelectionPanelData& PanelData = GetCampaignControllerPanelData();
		const FString SelectionId = PC->GetCampaignSelectionId();
		const bool bCityMode = PC->GetCampaignSelectionKind() == EWLCampaignSelectionKind::City;
		if (bCityMode)
		{
			if (const FWLCampaignSelectionPanelEntry* City = PanelData.Cities.Find(SelectionId))
			{
				Slots = City->UrbanSlots;
			}
		}
		else
		{
			if (const FWLCampaignSelectionPanelEntry* Province = PanelData.Provinces.Find(SelectionId))
			{
				Slots = Province->BuildingSlots;
			}
		}

		if (Slots.IsEmpty())
		{
			Slots = bCityMode
				? TArray<FString>{ TEXT("Government"), TEXT("Services"), TEXT("Industry"), TEXT("Security") }
				: TArray<FString>{ TEXT("Infrastructure"), TEXT("Logistics"), TEXT("Security"), TEXT("Industry") };
		}
		return Slots;
	}

	float GetCampaignControllerPanelSlotStartY(float PanelY)
	{
		return PanelY + 383.f;
	}

	float GetCampaignControllerPanelDetailsY(float PanelY, int32 SlotCount)
	{
		const int32 SlotRows = FMath::Max(1, FMath::DivideAndRoundUp(FMath::Clamp(SlotCount, 1, 6), 2));
		return GetCampaignControllerPanelSlotStartY(PanelY) + static_cast<float>(SlotRows) * 56.f + 16.f;
	}
}

AWLCampaignPlayerController::AWLCampaignPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AWLCampaignPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (HasCampaignInput())
	{
		EnterCampaignInputMode();
		CachePresentationActors();
		ShowCampaign3DView();
		if (FParse::Param(FCommandLine::Get(), TEXT("WLAmericaView")))
		{
			FTimerHandle AmericaViewTimer;
			GetWorldTimerManager().SetTimer(AmericaViewTimer, this, &AWLCampaignPlayerController::FocusCampaignAmerica, 0.45f, false);
		}
	}
	else
	{
		ShowMainMenu();
	}
}

void AWLCampaignPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateMapCamera(DeltaSeconds);
}

void AWLCampaignPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AWLCampaignPlayerController::OnAdvanceMonth);
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AWLCampaignPlayerController::OnPrintState);
		InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &AWLCampaignPlayerController::OnSaveCampaign);
		InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AWLCampaignPlayerController::OnBuildRecommended);
		InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AWLCampaignPlayerController::OnToggleDiplomacyView);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AWLCampaignPlayerController::OnSelectCountry);
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AWLCampaignPlayerController::OnZoomIn);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AWLCampaignPlayerController::OnZoomOut);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AWLCampaignPlayerController::BeginDragPan);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AWLCampaignPlayerController::EndDragPan);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AWLCampaignPlayerController::BeginDragPan);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &AWLCampaignPlayerController::EndDragPan);
		InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AWLCampaignPlayerController::OnPanNorth);
		InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AWLCampaignPlayerController::OnPanSouth);
		InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AWLCampaignPlayerController::OnPanWest);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AWLCampaignPlayerController::OnPanEast);
		InputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &AWLCampaignPlayerController::OnZoomIn);
		InputComponent->BindKey(EKeys::Add, IE_Pressed, this, &AWLCampaignPlayerController::OnZoomIn);
		InputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &AWLCampaignPlayerController::OnZoomOut);
		InputComponent->BindKey(EKeys::Subtract, IE_Pressed, this, &AWLCampaignPlayerController::OnZoomOut);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AWLCampaignPlayerController::ResetCampaignCamera);
		InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AWLCampaignPlayerController::FocusCampaignTheater);
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AWLCampaignPlayerController::FocusCampaignAmerica);
	}
}

bool AWLCampaignPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (HandleCampaignInputKey(Params))
	{
		return true;
	}
	return Super::InputKey(Params);
}

UWLStrategicTickSubsystem* AWLCampaignPlayerController::GetTick() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}

UWLDataRegistry* AWLCampaignPlayerController::GetRegistry() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

FString AWLCampaignPlayerController::GetCampaignZoomLODLabel() const
{
	return Campaign3DView ? Campaign3DView->GetCurrentZoomLODLabel() : TEXT("Teatro");
}

float AWLCampaignPlayerController::GetCampaignCameraHeight() const
{
	const AActor* CameraTarget = GetViewTarget();
	return CameraTarget && ActivePresentationMode == EWLCampaignPresentationMode::Campaign3D
		? CameraTarget->GetActorLocation().Z
		: 0.f;
}

void AWLCampaignPlayerController::OnAdvanceMonth()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->AdvanceMonth();
		const int32 AIBuildCount = Tick->GetLastEconomicAIBuildCount();
		SetLastActionMessage(FString::Printf(TEXT("Mes avanzado a %02d/%d. IA economica: %d construcciones."),
			Tick->GetCurrentMonth(), Tick->GetCurrentYear(), AIBuildCount), true);
	}
}

void AWLCampaignPlayerController::OnPrintState()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		GI->WLPrintState();
	}
}

void AWLCampaignPlayerController::OnSaveCampaign()
{
	SaveActiveCampaign();
}

void AWLCampaignPlayerController::OnBuildRecommended()
{
	if (!HasCampaignInput())
	{
		return;
	}

	FString Message;
	if (BuildRecommendedInSelectedProvince(Message))
	{
		SetLastActionMessage(Message, true);
		UE_LOG(LogWorldLeader, Log, TEXT("BuildRecommended: %s"), *Message);
	}
	else
	{
		SetLastActionMessage(Message, false);
		UE_LOG(LogWorldLeader, Warning, TEXT("BuildRecommended: %s"), *Message);
	}
}

void AWLCampaignPlayerController::OnToggleDiplomacyView()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
	{
		ShowCampaign3DView();
	}
	else
	{
		ShowDiplomacyMapView();
	}
}

void AWLCampaignPlayerController::OnSelectCountry()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (TryHandleSelectionPanelClick())
	{
		return;
	}
	if (TryHandleViewToggleClick())
	{
		return;
	}

	auto TrySelectCampaign3DAtWorldLocation = [this](const FVector& WorldLocation, const TCHAR* SourceLabel) -> bool
	{
		if (ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D)
		{
			return false;
		}
		if (!Campaign3DView)
		{
			CachePresentationActors();
		}
		if (!Campaign3DView)
		{
			return false;
		}

		FWLCampaign3DCityView CityView;
		if (Campaign3DView->TryGetCityNearWorldLocation(WorldLocation, 18500.f, CityView))
		{
			SelectCampaignCity(CityView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Ciudad seleccionada en Campaign 3D: %s."), *SelectedCityName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected city by %s: %s (%s)"), SourceLabel, *SelectedCityName, *SelectedCityId);
			return true;
		}

		FWLCampaignTerritoryRegionView TerritoryView;
		if (Campaign3DView->TryGetTerritoryAtWorldLocation(WorldLocation, TerritoryView) && !TerritoryView.bIsCountry)
		{
			SelectCampaignTerritory(TerritoryView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Territorio seleccionado en Campaign 3D: %s."), *SelectedTerritoryName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected territory by %s: %s (%s)"), SourceLabel, *SelectedTerritoryName, *SelectedTerritoryId);
			return true;
		}

		return false;
	};

	auto TrySelectCampaign3DAtCursorPlane = [this, &TrySelectCampaign3DAtWorldLocation](const TCHAR* SourceLabel) -> bool
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (!GetMousePosition(MouseX, MouseY))
		{
			return false;
		}

		FVector GroundPoint = FVector::ZeroVector;
		if (!GetCampaignGroundPointFromScreen(MouseX, MouseY, GroundPoint))
		{
			return false;
		}

		return TrySelectCampaign3DAtWorldLocation(GroundPoint, SourceLabel);
	};

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		if (TrySelectCampaign3DAtCursorPlane(TEXT("cursor plane")))
		{
			return;
		}
		return;
	}

	if (ActivePresentationMode == EWLCampaignPresentationMode::Campaign3D)
	{
		if (!Campaign3DView)
		{
			CachePresentationActors();
		}
		FWLCampaign3DCityView CityView;
		if (Campaign3DView && Campaign3DView->TryGetCityForComponent(Hit.GetComponent(), CityView))
		{
			SelectCampaignCity(CityView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Ciudad seleccionada en Campaign 3D: %s."), *SelectedCityName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected city: %s (%s)"), *SelectedCityName, *SelectedCityId);
			return;
		}
		if (Campaign3DView && Campaign3DView->TryGetCityNearWorldLocation(Hit.Location, 18500.f, CityView))
		{
			SelectCampaignCity(CityView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Ciudad seleccionada en Campaign 3D: %s."), *SelectedCityName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected city by location: %s (%s)"), *SelectedCityName, *SelectedCityId);
			return;
		}

		FWLCampaignTerritoryRegionView TerritoryView;
		if (Campaign3DView && Campaign3DView->TryGetTerritoryForComponent(Hit.GetComponent(), TerritoryView) && !TerritoryView.bIsCountry)
		{
			SelectCampaignTerritory(TerritoryView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Territorio seleccionado en Campaign 3D: %s."), *SelectedTerritoryName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected territory: %s (%s)"), *SelectedTerritoryName, *SelectedTerritoryId);
			return;
		}
		if (Campaign3DView && Campaign3DView->TryGetTerritoryAtWorldLocation(Hit.Location, TerritoryView) && !TerritoryView.bIsCountry)
		{
			SelectCampaignTerritory(TerritoryView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Territorio seleccionado en Campaign 3D: %s."), *SelectedTerritoryName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected territory by location: %s (%s)"), *SelectedTerritoryName, *SelectedTerritoryId);
			return;
		}

		FWLCampaign3DProvinceView ProvinceView;
		if (Campaign3DView && Campaign3DView->TryGetProvinceForComponent(Hit.GetComponent(), ProvinceView))
		{
			if (SelectProvince(ProvinceView.Id))
			{
				ClearSelectedCity();
				ActiveSelectionKind = EWLCampaignSelectionKind::Province;
				SelectedPanelObjectId = SelectedProvinceId;
				SelectedTerritoryId = SelectedProvinceId;
				SelectedTerritoryName = SelectedProvinceName;
				SelectedTerritoryCountryIso = SelectedProvinceCountryIso;
				SelectedTerritoryType = TEXT("province");
				if (Campaign3DView)
				{
					Campaign3DView->SetSelectedProvinceHighlight(SelectedProvinceId);
				}
				ClearSelectedCountry();
				SetLastActionMessage(FString::Printf(TEXT("Provincia seleccionada en Campaign 3D: %s."), *SelectedProvinceName), true);
				UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected province: %s (%s)"), *SelectedProvinceName, *SelectedProvinceId);
			}
			return;
		}
		if (TrySelectCampaign3DAtCursorPlane(TEXT("cursor plane fallback")))
		{
			return;
		}
		return;
	}

	if (!WorldMap)
	{
		CacheWorldMap();
	}
	if (!WorldMap)
	{
		return;
	}

	FWLMapProvinceView ProvinceView;
	if (WorldMap->TryGetProvinceForComponent(Hit.GetComponent(), ProvinceView))
	{
		if (SelectProvince(ProvinceView.Id))
		{
			ClearSelectedCountry();
			UE_LOG(LogWorldLeader, Log, TEXT("Selected province: %s (%s)"), *SelectedProvinceName, *SelectedProvinceId);
		}
		return;
	}

	FWLMapCountryView Country;
	if (WorldMap->TryGetCountryForComponent(Hit.GetComponent(), Country))
	{
		ClearSelectedProvince();
		bHasSelectedCountry = true;
		SelectedCountryName = Country.Name;
		SelectedCountryIso = Country.Iso;
		SelectedCountryContinent = Country.Continent;
		UE_LOG(LogWorldLeader, Log, TEXT("Selected country: %s (%s)"), *SelectedCountryName, *SelectedCountryIso);
	}
}

void AWLCampaignPlayerController::OnZoomIn()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
	{
		ZoomMapCamera(0.82f);
	}
	else
	{
		ZoomCampaignCamera(0.82f);
	}
}

void AWLCampaignPlayerController::OnZoomOut()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
	{
		ZoomMapCamera(1.18f);
	}
	else
	{
		ZoomCampaignCamera(1.22f);
	}
}

void AWLCampaignPlayerController::OnPanNorth()
{
	if (!HasCampaignInput())
	{
		return;
	}

	const AActor* CameraTarget = GetViewTarget();
	const float Scale = CameraTarget ? FMath::Clamp(CameraTarget->GetActorLocation().Z / 120000.f, 0.45f, 5.0f) : 1.f;
	if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
	{
		MoveMapCamera(FVector2D(1.f, 0.f) * EdgePanSpeed * 0.18f * Scale);
	}
	else
	{
		MoveCampaignCamera(FVector2D(1.f, 0.f) * CampaignEdgePanSpeed * 0.30f * Scale);
	}
}

void AWLCampaignPlayerController::OnPanSouth()
{
	if (!HasCampaignInput())
	{
		return;
	}

	const AActor* CameraTarget = GetViewTarget();
	const float Scale = CameraTarget ? FMath::Clamp(CameraTarget->GetActorLocation().Z / 120000.f, 0.45f, 5.0f) : 1.f;
	if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
	{
		MoveMapCamera(FVector2D(-1.f, 0.f) * EdgePanSpeed * 0.18f * Scale);
	}
	else
	{
		MoveCampaignCamera(FVector2D(-1.f, 0.f) * CampaignEdgePanSpeed * 0.30f * Scale);
	}
}

void AWLCampaignPlayerController::OnPanWest()
{
	if (!HasCampaignInput())
	{
		return;
	}

	const AActor* CameraTarget = GetViewTarget();
	const float Scale = CameraTarget ? FMath::Clamp(CameraTarget->GetActorLocation().Z / 120000.f, 0.45f, 5.0f) : 1.f;
	if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
	{
		MoveMapCamera(FVector2D(0.f, -1.f) * EdgePanSpeed * 0.18f * Scale);
	}
	else
	{
		MoveCampaignCamera(FVector2D(0.f, -1.f) * CampaignEdgePanSpeed * 0.30f * Scale);
	}
}

void AWLCampaignPlayerController::OnPanEast()
{
	if (!HasCampaignInput())
	{
		return;
	}

	const AActor* CameraTarget = GetViewTarget();
	const float Scale = CameraTarget ? FMath::Clamp(CameraTarget->GetActorLocation().Z / 120000.f, 0.45f, 5.0f) : 1.f;
	if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
	{
		MoveMapCamera(FVector2D(0.f, 1.f) * EdgePanSpeed * 0.18f * Scale);
	}
	else
	{
		MoveCampaignCamera(FVector2D(0.f, 1.f) * CampaignEdgePanSpeed * 0.30f * Scale);
	}
}

void AWLCampaignPlayerController::ResetCampaignCamera()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D)
	{
		return;
	}

	if (!Campaign3DView)
	{
		CachePresentationActors();
	}
	ACameraActor* Camera = Campaign3DView ? Campaign3DView->GetViewCamera() : nullptr;
	if (!Camera)
	{
		return;
	}

	Camera->SetActorLocation(Campaign3DView->GetDefaultCameraLocation());
	Camera->SetActorRotation(Campaign3DView->GetDefaultCameraRotation());
	SetViewTargetWithBlend(Camera, 0.20f);
	Campaign3DView->ApplyZoomLOD(Camera->GetActorLocation().Z);
	SetLastActionMessage(TEXT("Camara restablecida en el teatro activo."), true);
}

void AWLCampaignPlayerController::FocusCampaignTheater()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D)
	{
		ShowCampaign3DView();
	}
	if (!Campaign3DView)
	{
		CachePresentationActors();
	}

	ACameraActor* Camera = Campaign3DView ? Campaign3DView->GetViewCamera() : nullptr;
	if (!Camera)
	{
		return;
	}

	const FVector FocusPoint = Campaign3DView->GetTheaterFocusPoint();
	const float DesiredZ = FMath::Clamp(320000.f, CampaignMinCameraHeight, CampaignMaxCameraHeight);
	Camera->SetActorLocation(FVector(FocusPoint.X, FocusPoint.Y, DesiredZ));
	Camera->SetActorRotation(FRotator(-90.f, 0.f, 0.f));
	SetViewTargetWithBlend(Camera, 0.20f);
	Campaign3DView->ApplyZoomLOD(Camera->GetActorLocation().Z);
	SetLastActionMessage(TEXT("Vista Teatro: Colombia/Venezuela."), true);
}

void AWLCampaignPlayerController::FocusCampaignAmerica()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D)
	{
		ShowCampaign3DView();
	}
	if (!Campaign3DView)
	{
		CachePresentationActors();
	}

	ACameraActor* Camera = Campaign3DView ? Campaign3DView->GetViewCamera() : nullptr;
	if (!Camera)
	{
		return;
	}

	const FVector FocusPoint = GetCampaignAmericaFocusPoint();
	const float DesiredZ = FMath::Clamp(4000000.f, CampaignMinCameraHeight, CampaignMaxCameraHeight);
	const FRotator Rotation(-90.f, 0.f, 0.f);
	const FVector Location(FocusPoint.X, FocusPoint.Y, DesiredZ);

	Camera->SetActorLocation(Location);
	Camera->SetActorRotation(Rotation);
	SetViewTargetWithBlend(Camera, 0.20f);
	Campaign3DView->ApplyZoomLOD(Camera->GetActorLocation().Z);
	SetLastActionMessage(TEXT("Vista America: paises y ciudades low-detail."), true);
}

void AWLCampaignPlayerController::BeginDragPan()
{
	if (!HasCampaignInput())
	{
		return;
	}

	bDragPanning = true;
	bHasLastDragMouse = false;
	DragPanAnchorWorld = FVector::ZeroVector;
}

void AWLCampaignPlayerController::EndDragPan()
{
	bDragPanning = false;
	bHasLastDragMouse = false;
	DragPanAnchorWorld = FVector::ZeroVector;
}

void AWLCampaignPlayerController::CacheWorldMap()
{
	WorldMap = nullptr;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AWLWorldMap> It(World); It; ++It)
	{
		WorldMap = *It;
		return;
	}
}

void AWLCampaignPlayerController::CachePresentationActors()
{
	Campaign3DView = nullptr;
	WorldMap = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (AWLCampaignGameMode* CampaignMode = World->GetAuthGameMode<AWLCampaignGameMode>())
	{
		Campaign3DView = CampaignMode->GetCampaign3DView();
		WorldMap = CampaignMode->GetDiplomacyMapView();
	}

	if (!Campaign3DView)
	{
		for (TActorIterator<AWLCampaign3DView> It(World); It; ++It)
		{
			Campaign3DView = *It;
			break;
		}
	}
	if (!WorldMap)
	{
		CacheWorldMap();
	}
}

void AWLCampaignPlayerController::ShowCampaign3DView()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (!Campaign3DView || !WorldMap)
	{
		CachePresentationActors();
	}

	if (WorldMap)
	{
		WorldMap->SetPresentationActive(false, false);
	}
	if (Campaign3DView)
	{
		Campaign3DView->SetPresentationActive(true, true);
		if (AActor* CameraTarget = Campaign3DView->GetViewCamera())
		{
			Campaign3DView->ApplyZoomLOD(CameraTarget->GetActorLocation().Z);
		}
	}
	ActivePresentationMode = EWLCampaignPresentationMode::Campaign3D;
	SetLastActionMessage(TEXT("Vista principal: Campaign 3D."), true);
}

void AWLCampaignPlayerController::ShowDiplomacyMapView()
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (!Campaign3DView || !WorldMap)
	{
		CachePresentationActors();
	}

	if (Campaign3DView)
	{
		Campaign3DView->SetPresentationActive(false, false);
	}
	if (WorldMap)
	{
		WorldMap->SetPresentationActive(true, true);
	}
	ActivePresentationMode = EWLCampaignPresentationMode::Diplomacy;
	SetLastActionMessage(TEXT("Vista de diplomacia: mapa politico/cartografico."), true);
}

bool AWLCampaignPlayerController::TryHandleViewToggleClick()
{
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	const float Y = 58.f;
	const float Height = 38.f;
	const float CampaignX = static_cast<float>(ViewportX) - 386.f;
	const float DiplomacyX = static_cast<float>(ViewportX) - 210.f;
	const float Width = 158.f;
	const float TopButtonPaddingY = 12.f;
	const float TopButtonBottomPaddingY = 12.f;

	const bool bCampaignButton =
		MouseX >= CampaignX
		&& MouseX <= CampaignX + Width
		&& MouseY >= Y - TopButtonPaddingY
		&& MouseY <= Y + Height + TopButtonBottomPaddingY;
	const bool bDiplomacyButton =
		MouseX >= DiplomacyX
		&& MouseX <= DiplomacyX + Width
		&& MouseY >= Y - TopButtonPaddingY
		&& MouseY <= Y + Height + TopButtonBottomPaddingY;

	if (bCampaignButton)
	{
		ShowCampaign3DView();
		return true;
	}
	if (bDiplomacyButton)
	{
		ShowDiplomacyMapView();
		return true;
	}

	if (ActivePresentationMode == EWLCampaignPresentationMode::Campaign3D)
	{
		const float ControlY = 102.f;
		const float ControlH = 34.f;
		const float HitPaddingX = 9.f;
		const float HitPaddingTopY = 10.f;
		const float HitPaddingBottomY = 12.f;
		const float ZoomW = 44.f;
		const float Gap = 8.f;
		const float ZoomInX = static_cast<float>(ViewportX) - 444.f;
		const float ZoomOutX = ZoomInX + ZoomW + Gap;
		const float ResetX = ZoomOutX + ZoomW + Gap;
		const float ResetW = 84.f;
		const float FocusX = ResetX + ResetW + Gap;
		const float FocusW = 104.f;
		const float AmericaX = FocusX + FocusW + Gap;
		const float AmericaW = 112.f;

		const bool bInControlRow = MouseY >= ControlY - HitPaddingTopY && MouseY <= ControlY + ControlH + HitPaddingBottomY;
		if (bInControlRow && MouseX >= ZoomInX - HitPaddingX && MouseX <= ZoomInX + ZoomW + HitPaddingX)
		{
			OnZoomIn();
			return true;
		}
		if (bInControlRow && MouseX >= ZoomOutX - HitPaddingX && MouseX <= ZoomOutX + ZoomW + HitPaddingX)
		{
			OnZoomOut();
			return true;
		}
		if (bInControlRow && MouseX >= ResetX - HitPaddingX && MouseX <= ResetX + ResetW + HitPaddingX)
		{
			ResetCampaignCamera();
			return true;
		}
		if (bInControlRow && MouseX >= FocusX - HitPaddingX && MouseX <= FocusX + FocusW + HitPaddingX)
		{
			FocusCampaignTheater();
			return true;
		}
		if (bInControlRow && MouseX >= AmericaX - HitPaddingX && MouseX <= AmericaX + AmericaW + HitPaddingX)
		{
			FocusCampaignAmerica();
			return true;
		}
	}
	return false;
}

bool AWLCampaignPlayerController::TryHandleSelectionPanelClick()
{
	if (ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D || !HasCampaignSelectionPanel())
	{
		return false;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	const float PanelW = 430.f;
	const float PanelX = static_cast<float>(ViewportX) - 468.f;
	const float PanelY = 154.f;
	const float PanelH = FMath::Clamp(static_cast<float>(ViewportY) - PanelY - 54.f, 360.f, 690.f);
	const bool bInPanel = MouseX >= PanelX && MouseX <= PanelX + PanelW && MouseY >= PanelY && MouseY <= PanelY + PanelH;
	if (!bInPanel)
	{
		return false;
	}

	const float CloseX = PanelX + PanelW - 38.f;
	const float CloseY = PanelY + 14.f;
	const bool bClose = MouseX >= CloseX && MouseX <= CloseX + 24.f && MouseY >= CloseY && MouseY <= CloseY + 24.f;
	if (bClose)
	{
		ClearCampaignSelection();
		SetLastActionMessage(TEXT("Panel contextual cerrado."), true);
		return true;
	}

	const TArray<FString> SlotLabels = GetCampaignControllerPanelSlots(this);
	const bool bCityMode = ActiveSelectionKind == EWLCampaignSelectionKind::City;
	const EWLCampaignBuildingPanelContext Context = FWLCampaignBuildingSlotRules::ContextFromCityMode(bCityMode);
	const FWLCampaignBuildingSlotCatalog& Catalog = GetCampaignControllerBuildingCatalog();

	const float SlotStartY = GetCampaignControllerPanelSlotStartY(PanelY);
	const float SlotW = (PanelW - 48.f) * 0.5f;
	const float SlotH = 48.f;
	const float SlotGap = 8.f;
	const float SlotX0 = PanelX + 18.f;
	for (int32 Index = 0; Index < SlotLabels.Num() && Index < 6; ++Index)
	{
		const int32 Col = Index % 2;
		const int32 Row = Index / 2;
		const float SlotX = SlotX0 + static_cast<float>(Col) * (SlotW + SlotGap);
		const float SlotY = SlotStartY + static_cast<float>(Row) * (SlotH + SlotGap);
		if (MouseX >= SlotX && MouseX <= SlotX + SlotW && MouseY >= SlotY && MouseY <= SlotY + SlotH)
		{
			SelectCampaignBuildingSlot(SlotLabels[Index], Index, bCityMode);
			const EWLCampaignBuildingSlotState State = GetCampaignBuildingSlotState(SlotLabels[Index], Index, bCityMode);
			const FString DisplayLabel = FWLCampaignBuildingSlotRules::GetSlotDisplayLabel(Catalog, Context, SlotLabels[Index]);
			if (State == EWLCampaignBuildingSlotState::Locked)
			{
				SetLastActionMessage(FString::Printf(TEXT("Slot bloqueado para fases futuras: %s."), *DisplayLabel), true);
			}
			else if (State == EWLCampaignBuildingSlotState::Occupied)
			{
				SetLastActionMessage(FString::Printf(TEXT("Edificio seleccionado en slot: %s."), *DisplayLabel), true);
			}
			else
			{
				SetLastActionMessage(FString::Printf(TEXT("Slot vacio seleccionado: %s."), *DisplayLabel), true);
			}
			return true;
		}
	}

	if (HasSelectedBuildingSlot()
		&& bSelectedBuildingSlotCityMode == bCityMode
		&& GetCampaignBuildingSlotState(SelectedBuildingSlotLabel, SelectedBuildingSlotIndex, bCityMode) == EWLCampaignBuildingSlotState::Empty)
	{
		TArray<FWLCampaignBuildingDefinition> CompatibleBuildings;
		FWLCampaignBuildingSlotRules::GetCompatibleBuildings(Catalog, Context, SelectedBuildingSlotLabel, CompatibleBuildings);
		const int32 MaxOptions = FMath::Min(CompatibleBuildings.Num(), 3);
		const float DetailY = GetCampaignControllerPanelDetailsY(PanelY, SlotLabels.Num());
		const float OptionStartY = DetailY + 74.f;
		const float OptionH = 46.f;
		const float BuildW = 78.f;
		const float BuildH = 23.f;
		const float BuildX = PanelX + PanelW - 105.f;
		for (int32 Index = 0; Index < MaxOptions; ++Index)
		{
			const float OptionY = OptionStartY + static_cast<float>(Index) * 51.f;
			const float BuildY = OptionY + 12.f;
			if (MouseX >= BuildX && MouseX <= BuildX + BuildW && MouseY >= BuildY && MouseY <= BuildY + BuildH)
			{
				FString Message;
				if (TryBuildCampaignPlaceholderBuilding(CompatibleBuildings[Index].Id, Message))
				{
					SetLastActionMessage(Message, true);
				}
				else
				{
					SetLastActionMessage(Message, false);
				}
				return true;
			}
		}
	}
	return true;
}

void AWLCampaignPlayerController::UpdateMapCamera(float DeltaSeconds)
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy && !WorldMap)
	{
		CachePresentationActors();
	}
	else if (ActivePresentationMode == EWLCampaignPresentationMode::Campaign3D && !Campaign3DView)
	{
		CachePresentationActors();
	}

	AActor* CameraTarget = GetViewTarget();
	if (!CameraTarget || !CameraTarget->IsA<ACameraActor>())
	{
		return;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		bHasLastDragMouse = false;
		return;
	}

	const FVector CameraLocation = CameraTarget->GetActorLocation();
	const float ZoomSpeedScale = FMath::Clamp(CameraLocation.Z / 120000.f, 0.35f, 4.0f);

	if (bDragPanning)
	{
		const FVector2D CurrentMouse(MouseX, MouseY);
		if (ActivePresentationMode == EWLCampaignPresentationMode::Campaign3D)
		{
			FVector CurrentGroundPoint = FVector::ZeroVector;
			if (GetCampaignGroundPointFromScreen(MouseX, MouseY, CurrentGroundPoint))
			{
				if (!bHasLastDragMouse)
				{
					DragPanAnchorWorld = CurrentGroundPoint;
					bHasLastDragMouse = true;
					LastDragMouse = CurrentMouse;
					return;
				}

				FVector Location = CameraTarget->GetActorLocation();
				Location.X += DragPanAnchorWorld.X - CurrentGroundPoint.X;
				Location.Y += DragPanAnchorWorld.Y - CurrentGroundPoint.Y;
				ClampCampaignCameraLocation(Location);
				CameraTarget->SetActorLocation(Location);
				if (Campaign3DView)
				{
					Campaign3DView->ApplyZoomLOD(Location.Z);
				}
				LastDragMouse = CurrentMouse;
				return;
			}
		}

		if (bHasLastDragMouse)
		{
			const FVector2D MouseDelta = CurrentMouse - LastDragMouse;
			const float UnitsPerPixel = DragPanUnitsPerPixelAt100k * FMath::Clamp(CameraLocation.Z / 100000.f, 0.25f, 6.0f);
			if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
			{
				MoveMapCamera(FVector2D(-MouseDelta.Y, MouseDelta.X) * UnitsPerPixel);
			}
			else
			{
				MoveCampaignCamera(FVector2D(-MouseDelta.Y, MouseDelta.X) * UnitsPerPixel * 0.42f);
			}
		}
		LastDragMouse = CurrentMouse;
		bHasLastDragMouse = true;
		return;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return;
	}

	FVector2D PanDirection = FVector2D::ZeroVector;
	if (MouseX <= EdgePanMarginPx)
	{
		PanDirection.Y -= 1.f; // oeste
	}
	else if (MouseX >= static_cast<float>(ViewportX) - EdgePanMarginPx)
	{
		PanDirection.Y += 1.f; // este
	}

	if (MouseY <= EdgePanMarginPx)
	{
		PanDirection.X += 1.f; // norte
	}
	else if (MouseY >= static_cast<float>(ViewportY) - EdgePanMarginPx)
	{
		PanDirection.X -= 1.f; // sur
	}

	if (!PanDirection.IsNearlyZero())
	{
		PanDirection.Normalize();
		if (ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy)
		{
			MoveMapCamera(PanDirection * EdgePanSpeed * ZoomSpeedScale * DeltaSeconds);
		}
		else
		{
			MoveCampaignCamera(PanDirection * CampaignEdgePanSpeed * ZoomSpeedScale * DeltaSeconds);
		}
	}
}

void AWLCampaignPlayerController::MoveMapCamera(const FVector2D& Delta)
{
	AActor* CameraTarget = GetViewTarget();
	if (!CameraTarget)
	{
		return;
	}

	FVector Location = CameraTarget->GetActorLocation();
	Location.X += Delta.X;
	Location.Y += Delta.Y;

	if (WorldMap)
	{
		const FBox2D Bounds = WorldMap->GetMapBounds2D();
		if (Bounds.bIsValid)
		{
			Location.X = FMath::Clamp(Location.X, Bounds.Min.X, Bounds.Max.X);
			Location.Y = FMath::Clamp(Location.Y, Bounds.Min.Y, Bounds.Max.Y);
		}
	}

	CameraTarget->SetActorLocation(Location);
}

void AWLCampaignPlayerController::MoveCampaignCamera(const FVector2D& Delta)
{
	AActor* CameraTarget = GetViewTarget();
	if (!CameraTarget)
	{
		return;
	}

	FVector Location = CameraTarget->GetActorLocation();
	Location.X += Delta.X;
	Location.Y += Delta.Y;

	ClampCampaignCameraLocation(Location);
	CameraTarget->SetActorLocation(Location);
	if (Campaign3DView)
	{
		Campaign3DView->ApplyZoomLOD(Location.Z);
	}
}

void AWLCampaignPlayerController::ZoomMapCamera(float ZoomFactor)
{
	AActor* CameraTarget = GetViewTarget();
	if (!CameraTarget || !CameraTarget->IsA<ACameraActor>())
	{
		return;
	}

	FVector Location = CameraTarget->GetActorLocation();
	Location.Z = FMath::Clamp(Location.Z * ZoomFactor, MinCameraHeight, MaxCameraHeight);
	CameraTarget->SetActorLocation(Location);
}

void AWLCampaignPlayerController::ZoomCampaignCamera(float ZoomFactor)
{
	AActor* CameraTarget = GetViewTarget();
	if (!CameraTarget || !CameraTarget->IsA<ACameraActor>())
	{
		return;
	}

	FVector AnchorBefore = FVector::ZeroVector;
	FVector2D AnchorScreenPoint = FVector2D::ZeroVector;
	const bool bHasAnchor = GetCampaignZoomAnchor(AnchorBefore, AnchorScreenPoint);

	FVector Location = CameraTarget->GetActorLocation();
	const float DesiredZ = FMath::Clamp(Location.Z * ZoomFactor, CampaignMinCameraHeight, CampaignMaxCameraHeight);
	if (FMath::IsNearlyEqual(Location.Z, DesiredZ, 1.f))
	{
		return;
	}

	Location.Z = DesiredZ;
	CameraTarget->SetActorLocation(Location);

	if (bHasAnchor)
	{
		FVector AnchorAfter = FVector::ZeroVector;
		if (GetCampaignGroundPointFromScreen(AnchorScreenPoint.X, AnchorScreenPoint.Y, AnchorAfter))
		{
			Location = CameraTarget->GetActorLocation();
			Location.X += AnchorBefore.X - AnchorAfter.X;
			Location.Y += AnchorBefore.Y - AnchorAfter.Y;
		}
	}

	ClampCampaignCameraLocation(Location);
	CameraTarget->SetActorLocation(Location);
	if (Campaign3DView)
	{
		Campaign3DView->ApplyZoomLOD(Location.Z);
		SetLastActionMessage(FString::Printf(TEXT("Zoom Campaign 3D: %s %.0fk."),
			*Campaign3DView->GetCurrentZoomLODLabel(), Location.Z / 1000.f), true);
	}
}

FVector AWLCampaignPlayerController::GetCampaignAmericaFocusPoint() const
{
	FVector FocusPoint = Campaign3DView ? Campaign3DView->GetTheaterFocusPoint() : FVector::ZeroVector;
	if (!Campaign3DView)
	{
		return FocusPoint;
	}

	const FBox2D AmericaBounds = Campaign3DView->GetCameraBounds2D(CampaignMaxCameraHeight);
	if (AmericaBounds.bIsValid)
	{
		const FVector2D Center = AmericaBounds.GetCenter();
		FocusPoint.X = Center.X;
		FocusPoint.Y = Center.Y;
	}
	return FocusPoint;
}

bool AWLCampaignPlayerController::GetCampaignGroundPointFromScreen(float ScreenX, float ScreenY, FVector& OutWorldPoint)
{
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(ScreenX, ScreenY, RayOrigin, RayDirection))
	{
		return false;
	}

	if (FMath::Abs(RayDirection.Z) < UE_SMALL_NUMBER)
	{
		return false;
	}

	const float PlaneZ = Campaign3DView ? Campaign3DView->GetTheaterFocusPoint().Z : 0.f;
	const float T = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
	if (T <= 0.f)
	{
		return false;
	}

	OutWorldPoint = RayOrigin + RayDirection * T;
	OutWorldPoint.Z = PlaneZ;
	return true;
}

bool AWLCampaignPlayerController::GetCampaignZoomAnchor(FVector& OutAnchor, FVector2D& OutScreenPoint)
{
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (GetMousePosition(MouseX, MouseY)
		&& MouseX >= 0.f && MouseX <= static_cast<float>(ViewportX)
		&& MouseY >= 0.f && MouseY <= static_cast<float>(ViewportY)
		&& !IsScreenPointOverCampaignHud(MouseX, MouseY))
	{
		OutScreenPoint = FVector2D(MouseX, MouseY);
	}
	else
	{
		OutScreenPoint = FVector2D(static_cast<float>(ViewportX) * 0.5f, static_cast<float>(ViewportY) * 0.5f);
	}

	if (GetCampaignGroundPointFromScreen(OutScreenPoint.X, OutScreenPoint.Y, OutAnchor))
	{
		return true;
	}

	OutScreenPoint = FVector2D(static_cast<float>(ViewportX) * 0.5f, static_cast<float>(ViewportY) * 0.5f);
	return GetCampaignGroundPointFromScreen(OutScreenPoint.X, OutScreenPoint.Y, OutAnchor);
}

bool AWLCampaignPlayerController::IsScreenPointOverCampaignHud(float ScreenX, float ScreenY)
{
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}

	if (ScreenY <= 156.f || ScreenY >= static_cast<float>(ViewportY) - 56.f)
	{
		return true;
	}

	if (ActivePresentationMode == EWLCampaignPresentationMode::Campaign3D && HasCampaignSelectionPanel())
	{
		const float PanelW = 430.f;
		const float PanelX = static_cast<float>(ViewportX) - 468.f;
		const float PanelY = 154.f;
		const float PanelH = FMath::Clamp(static_cast<float>(ViewportY) - PanelY - 54.f, 360.f, 690.f);
		if (ScreenX >= PanelX && ScreenX <= PanelX + PanelW && ScreenY >= PanelY && ScreenY <= PanelY + PanelH)
		{
			return true;
		}
	}

	return ScreenX >= static_cast<float>(ViewportX) - 480.f && ScreenY <= 190.f;
}

void AWLCampaignPlayerController::ClampCampaignCameraLocation(FVector& Location) const
{
	Location.Z = FMath::Clamp(Location.Z, CampaignMinCameraHeight, CampaignMaxCameraHeight);
	if (!Campaign3DView)
	{
		return;
	}

	const FBox2D Bounds = Campaign3DView->GetCameraBounds2D(Location.Z);
	if (!Bounds.bIsValid)
	{
		return;
	}

	const float HeightRange = FMath::Max(1.f, CampaignMaxCameraHeight - CampaignMinCameraHeight);
	const float ZoomAlpha = FMath::Clamp((Location.Z - CampaignMinCameraHeight) / HeightRange, 0.f, 1.f);
	const FVector2D Padding(
		FMath::Lerp(52000.f, 260000.f, ZoomAlpha),
		FMath::Lerp(70000.f, 380000.f, ZoomAlpha));
	Location.X = FMath::Clamp(Location.X, Bounds.Min.X - Padding.X, Bounds.Max.X + Padding.X);
	Location.Y = FMath::Clamp(Location.Y, Bounds.Min.Y - Padding.Y, Bounds.Max.Y + Padding.Y);
}

bool AWLCampaignPlayerController::HandleCampaignInputKey(const FInputKeyEventArgs& Params)
{
	if (!HasCampaignInput())
	{
		return false;
	}

	if (Params.Key == EKeys::MouseWheelAxis && Params.Event == IE_Axis && !FMath::IsNearlyZero(Params.AmountDepressed))
	{
		Params.AmountDepressed > 0.f ? OnZoomIn() : OnZoomOut();
		return true;
	}

	const bool bPressed = Params.Event == IE_Pressed || Params.Event == IE_Repeat;
	if (!bPressed)
	{
		return false;
	}

	if (Params.Key == EKeys::LeftMouseButton)
	{
		if (TryHandleSelectionPanelClick())
		{
			return true;
		}
		if (TryHandleViewToggleClick())
		{
			return true;
		}
		OnSelectCountry();
		return true;
	}
	if (Params.Key == EKeys::MouseScrollUp || Params.Key == EKeys::Equals || Params.Key == EKeys::Add || Params.Key == EKeys::PageUp)
	{
		OnZoomIn();
		return true;
	}
	if (Params.Key == EKeys::MouseScrollDown || Params.Key == EKeys::Hyphen || Params.Key == EKeys::Subtract || Params.Key == EKeys::PageDown)
	{
		OnZoomOut();
		return true;
	}
	if (Params.Key == EKeys::R)
	{
		ResetCampaignCamera();
		return true;
	}
	if (Params.Key == EKeys::F)
	{
		FocusCampaignTheater();
		return true;
	}
	if (Params.Key == EKeys::G)
	{
		FocusCampaignAmerica();
		return true;
	}

	return false;
}

void AWLCampaignPlayerController::ShowMainMenu()
{
	if (MainMenuWidget)
	{
		MainMenuWidget->RemoveFromParent();
		MainMenuWidget = nullptr;
	}

	TSubclassOf<UWLMainMenuWidget> WidgetClass = MainMenuWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UWLMainMenuWidget::StaticClass();
	}
	MainMenuWidget = CreateWidget<UWLMainMenuWidget>(this, WidgetClass);
	if (MainMenuWidget)
	{
		MainMenuWidget->AddToViewport(100);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
		SetInputMode(InputMode);
	}

	bShowMouseCursor = true;
}

void AWLCampaignPlayerController::StartCampaignFromMenu(const FString& NationIso)
{
	UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI || !GI->StartNewCampaign(NationIso))
	{
		SetLastActionMessage(FString::Printf(TEXT("No se pudo iniciar campania con %s."), *NationIso), false);
		return;
	}

	if (MainMenuWidget)
	{
		MainMenuWidget->RemoveFromParent();
		MainMenuWidget = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		if (AWLCampaignGameMode* CampaignMode = World->GetAuthGameMode<AWLCampaignGameMode>())
		{
			CampaignMode->StartCampaignWorld(GI->GetSelectedNationIso());
		}
	}

	ClearCampaignSelection();
	CachePresentationActors();
	ShowCampaign3DView();
	EnterCampaignInputMode();
	SetLastActionMessage(FString::Printf(TEXT("Campania iniciada con %s."), *GI->GetSelectedNationIso()), true);
}

void AWLCampaignPlayerController::LoadCampaignFromMenu()
{
	UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI)
	{
		SetLastActionMessage(TEXT("GameInstance de campania no disponible."), false);
		return;
	}

	FString Message;
	if (!GI->LoadLocalCampaign(Message))
	{
		SetLastActionMessage(Message, false);
		UE_LOG(LogWorldLeader, Warning, TEXT("LoadCampaignFromMenu: %s"), *Message);
		return;
	}

	if (MainMenuWidget)
	{
		MainMenuWidget->RemoveFromParent();
		MainMenuWidget = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		if (AWLCampaignGameMode* CampaignMode = World->GetAuthGameMode<AWLCampaignGameMode>())
		{
			CampaignMode->StartCampaignWorld(GI->GetSelectedNationIso());
		}
	}

	ClearCampaignSelection();
	CachePresentationActors();
	ShowCampaign3DView();
	EnterCampaignInputMode();
	SetLastActionMessage(Message, true);
	UE_LOG(LogWorldLeader, Log, TEXT("LoadCampaignFromMenu: %s"), *Message);
}

void AWLCampaignPlayerController::SaveActiveCampaign()
{
	if (!HasCampaignInput())
	{
		return;
	}

	UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI)
	{
		SetLastActionMessage(TEXT("GameInstance de campania no disponible."), false);
		return;
	}

	FString Message;
	if (GI->SaveLocalCampaign(Message))
	{
		SetLastActionMessage(Message, true);
		UE_LOG(LogWorldLeader, Log, TEXT("SaveActiveCampaign: %s"), *Message);
	}
	else
	{
		SetLastActionMessage(Message, false);
		UE_LOG(LogWorldLeader, Warning, TEXT("SaveActiveCampaign: %s"), *Message);
	}
}

bool AWLCampaignPlayerController::HasCampaignInput() const
{
	const UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	return GI && GI->HasActiveCampaign();
}

void AWLCampaignPlayerController::EnterCampaignInputMode()
{
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AWLCampaignPlayerController::ClearSelectedCountry()
{
	bHasSelectedCountry = false;
	SelectedCountryName.Reset();
	SelectedCountryIso.Reset();
	SelectedCountryContinent.Reset();
}

void AWLCampaignPlayerController::ClearSelectedProvince()
{
	bHasSelectedProvince = false;
	SelectedProvinceId.Reset();
	SelectedProvinceName.Reset();
	SelectedProvinceCountryIso.Reset();
	SelectedProvinceRegion.Reset();
}

void AWLCampaignPlayerController::ClearSelectedCity()
{
	SelectedCityId.Reset();
	SelectedCityName.Reset();
	SelectedCityCountryIso.Reset();
	SelectedCityTerritoryId.Reset();
	SelectedCityTerritoryName.Reset();
	SelectedCityType.Reset();
}

void AWLCampaignPlayerController::ClearCampaignBuildingSelection()
{
	SelectedBuildingSlotKey.Reset();
	SelectedBuildingSlotLabel.Reset();
	SelectedCampaignBuildingId.Reset();
	SelectedBuildingSlotIndex = INDEX_NONE;
	bSelectedBuildingSlotCityMode = false;
	bSelectedCampaignBuildingCandidate = false;
}

void AWLCampaignPlayerController::ClearCampaignSelection()
{
	ClearSelectedCountry();
	ClearSelectedProvince();
	ClearSelectedCity();
	ClearCampaignBuildingSelection();
	ActiveSelectionKind = EWLCampaignSelectionKind::None;
	SelectedPanelObjectId.Reset();
	SelectedTerritoryId.Reset();
	SelectedTerritoryName.Reset();
	SelectedTerritoryCountryIso.Reset();
	SelectedTerritoryType.Reset();
	if (!Campaign3DView)
	{
		CachePresentationActors();
	}
	if (Campaign3DView)
	{
		Campaign3DView->ClearSelectionHighlight();
	}
}

EWLCampaignBuildingSlotState AWLCampaignPlayerController::GetCampaignBuildingSlotState(
	const FString& SlotLabel,
	int32 SlotIndex,
	bool bCityMode) const
{
	const FWLCampaignBuildingSlotCatalog& Catalog = GetCampaignControllerBuildingCatalog();
	const EWLCampaignBuildingPanelContext Context = FWLCampaignBuildingSlotRules::ContextFromCityMode(bCityMode);
	if (FWLCampaignBuildingSlotRules::IsSlotLocked(Catalog, Context, SlotLabel))
	{
		return EWLCampaignBuildingSlotState::Locked;
	}
	return GetCampaignBuildingIdForSlot(SlotLabel, SlotIndex, bCityMode).IsEmpty()
		? EWLCampaignBuildingSlotState::Empty
		: EWLCampaignBuildingSlotState::Occupied;
}

FString AWLCampaignPlayerController::GetCampaignBuildingIdForSlot(
	const FString& SlotLabel,
	int32 SlotIndex,
	bool bCityMode) const
{
	if (!HasCampaignSelectionPanel())
	{
		return FString();
	}

	const FString ObjectId = GetCampaignSelectionId();
	const EWLCampaignBuildingPanelContext Context = FWLCampaignBuildingSlotRules::ContextFromCityMode(bCityMode);
	const FString SlotKey = FWLCampaignBuildingSlotRules::MakeSlotKey(ObjectId, Context, SlotLabel, SlotIndex);
	if (const FString* BuiltBuilding = CampaignPlaceholderBuildingsBySlot.Find(SlotKey))
	{
		return *BuiltBuilding;
	}

	const FWLCampaignBuildingSlotCatalog& Catalog = GetCampaignControllerBuildingCatalog();
	return FWLCampaignBuildingSlotRules::GetInitialBuildingId(Catalog, Context, SlotLabel, SlotIndex);
}

void AWLCampaignPlayerController::SelectCampaignBuildingSlot(const FString& SlotLabel, int32 SlotIndex, bool bCityMode)
{
	if (!HasCampaignSelectionPanel())
	{
		ClearCampaignBuildingSelection();
		return;
	}

	const EWLCampaignBuildingPanelContext Context = FWLCampaignBuildingSlotRules::ContextFromCityMode(bCityMode);
	SelectedBuildingSlotKey = FWLCampaignBuildingSlotRules::MakeSlotKey(GetCampaignSelectionId(), Context, SlotLabel, SlotIndex);
	SelectedBuildingSlotLabel = SlotLabel;
	SelectedBuildingSlotIndex = SlotIndex;
	bSelectedBuildingSlotCityMode = bCityMode;
	bSelectedCampaignBuildingCandidate = false;

	const EWLCampaignBuildingSlotState State = GetCampaignBuildingSlotState(SlotLabel, SlotIndex, bCityMode);
	SelectedCampaignBuildingId = State == EWLCampaignBuildingSlotState::Occupied
		? GetCampaignBuildingIdForSlot(SlotLabel, SlotIndex, bCityMode)
		: FString();
}

bool AWLCampaignPlayerController::TryBuildCampaignPlaceholderBuilding(const FString& BuildingId, FString& OutMessage)
{
	if (!HasSelectedBuildingSlot() || SelectedBuildingSlotIndex == INDEX_NONE)
	{
		OutMessage = TEXT("Selecciona un slot primero.");
		return false;
	}

	const bool bCityMode = ActiveSelectionKind == EWLCampaignSelectionKind::City;
	if (bSelectedBuildingSlotCityMode != bCityMode)
	{
		OutMessage = TEXT("El slot seleccionado ya no pertenece al panel actual.");
		return false;
	}

	const FWLCampaignBuildingSlotCatalog& Catalog = GetCampaignControllerBuildingCatalog();
	const EWLCampaignBuildingPanelContext Context = FWLCampaignBuildingSlotRules::ContextFromCityMode(bCityMode);
	if (FWLCampaignBuildingSlotRules::IsSlotLocked(Catalog, Context, SelectedBuildingSlotLabel))
	{
		OutMessage = TEXT("Este slot esta bloqueado para fases futuras.");
		return false;
	}
	if (!GetCampaignBuildingIdForSlot(SelectedBuildingSlotLabel, SelectedBuildingSlotIndex, bCityMode).IsEmpty())
	{
		OutMessage = TEXT("Este slot ya tiene un edificio.");
		return false;
	}

	const FWLCampaignBuildingDefinition* Building = Catalog.FindBuilding(BuildingId);
	if (!Building)
	{
		OutMessage = FString::Printf(TEXT("Edificio placeholder desconocido: %s."), *BuildingId);
		return false;
	}
	if (!FWLCampaignBuildingSlotRules::IsBuildingCompatible(*Building, Context, SelectedBuildingSlotLabel))
	{
		OutMessage = FString::Printf(TEXT("%s no es compatible con este slot."), *Building->Name);
		return false;
	}

	CampaignPlaceholderBuildingsBySlot.Add(SelectedBuildingSlotKey, Building->Id);
	SelectedCampaignBuildingId = Building->Id;
	bSelectedCampaignBuildingCandidate = false;
	OutMessage = FString::Printf(TEXT("Construccion placeholder completada: %s."), *Building->Name);
	return true;
}

bool AWLCampaignPlayerController::SelectProvince(const FString& ProvinceId)
{
	const UWLDataRegistry* Registry = GetRegistry();
	FWLProvinceData Province;
	if (!Registry || !Registry->GetProvince(ProvinceId, Province))
	{
		return false;
	}

	bHasSelectedProvince = true;
	SelectedProvinceId = Province.Id;
	SelectedProvinceName = Province.Name;
	SelectedProvinceCountryIso = Province.CountryIso;
	SelectedProvinceRegion = Province.Region;
	return true;
}

void AWLCampaignPlayerController::SelectCampaignTerritory(const FWLCampaignTerritoryRegionView& Territory)
{
	ClearSelectedCity();
	ClearSelectedProvince();
	ClearCampaignBuildingSelection();
	ActiveSelectionKind = EWLCampaignSelectionKind::Province;
	SelectedPanelObjectId = Territory.Id;
	SelectedTerritoryId = Territory.Id;
	SelectedTerritoryName = Territory.Name;
	SelectedTerritoryCountryIso = Territory.CountryIso;
	SelectedTerritoryType = Territory.ProvinceType.IsEmpty() ? TEXT("province") : Territory.ProvinceType;

	// Keep legacy province state available when this territory also exists in the core registry.
	SelectProvince(Territory.Id);

	if (!Campaign3DView)
	{
		CachePresentationActors();
	}
	if (Campaign3DView)
	{
		Campaign3DView->SetSelectedProvinceHighlight(Territory.Id);
	}
}

void AWLCampaignPlayerController::SelectCampaignCity(const FWLCampaign3DCityView& City)
{
	ClearSelectedProvince();
	ClearCampaignBuildingSelection();
	ActiveSelectionKind = EWLCampaignSelectionKind::City;
	SelectedPanelObjectId = City.Id;
	SelectedTerritoryId.Reset();
	SelectedTerritoryName.Reset();
	SelectedTerritoryCountryIso.Reset();
	SelectedTerritoryType.Reset();
	SelectedCityId = City.Id;
	SelectedCityName = City.Name;
	SelectedCityCountryIso = City.CountryIso;
	SelectedCityTerritoryId = City.TerritoryId;
	SelectedCityTerritoryName = City.TerritoryName;
	SelectedCityType = City.CityType;

	if (!Campaign3DView)
	{
		CachePresentationActors();
	}
	if (Campaign3DView)
	{
		Campaign3DView->SetSelectedCityHighlight(City.Id);
	}
}

bool AWLCampaignPlayerController::BuildRecommendedInSelectedProvince(FString& OutMessage)
{
	if (!bHasSelectedProvince)
	{
		OutMessage = TEXT("Selecciona una provincia primero.");
		return false;
	}

	const UWLDataRegistry* Registry = GetRegistry();
	UWLStrategicTickSubsystem* Tick = GetTick();
	const UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!Registry || !Tick || !GI || !GI->HasActiveCampaign())
	{
		OutMessage = TEXT("Campania no disponible.");
		return false;
	}

	FWLProvinceData Province;
	if (!Registry->GetProvince(SelectedProvinceId, Province))
	{
		OutMessage = FString::Printf(TEXT("Provincia desconocida: %s"), *SelectedProvinceId);
		return false;
	}

	const FString ControllerIso = Tick->GetProvinceControllerIso(Province.Id);
	if (ControllerIso != GI->GetSelectedNationIso())
	{
		OutMessage = FString::Printf(TEXT("Solo puedes construir en provincias propias. %s esta controlada por %s."),
			*Province.Name, *ControllerIso);
		return false;
	}

	FWLBuildingData Building;
	if (!PickRecommendedBuilding(Province, Building))
	{
		bool bHasUnbuilt = false;
		const TArray<FString> Built = Tick->GetProvinceBuildings(Province.Id);
		for (const FWLBuildingData& Candidate : Registry->GetAllBuildings())
		{
			if (!Built.Contains(Candidate.Id))
			{
				bHasUnbuilt = true;
				break;
			}
		}
		OutMessage = bHasUnbuilt
			? FString::Printf(TEXT("Fondos insuficientes para nuevos edificios en %s."), *Province.Name)
			: FString::Printf(TEXT("Todos los edificios disponibles ya existen en %s."), *Province.Name);
		return false;
	}

	return Tick->BuildBuilding(Province.Id, Building.Id, OutMessage);
}

bool AWLCampaignPlayerController::PickRecommendedBuilding(const FWLProvinceData& Province, FWLBuildingData& OutBuilding) const
{
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick)
	{
		return false;
	}

	const TArray<FString> Built = Tick->GetProvinceBuildings(Province.Id);
	const int64 Treasury = Tick->GetTreasury(Tick->GetProvinceControllerIso(Province.Id));
	const FWLBalanceRules BalanceRules = Tick->GetBalanceRules();
	TArray<FWLBuildingData> Buildings = Registry->GetAllBuildings();
	Buildings.Sort([](const FWLBuildingData& A, const FWLBuildingData& B)
	{
		return A.Id < B.Id;
	});

	bool bFound = false;
	int64 BestFit = TNumericLimits<int64>::Lowest();
	int64 BestIncome = TNumericLimits<int64>::Lowest();
	int64 BestCost = TNumericLimits<int64>::Max();

	for (const FWLBuildingData& Building : Buildings)
	{
		if (Built.Contains(Building.Id)
			|| Building.Cost < 0
			|| Building.Cost > Treasury
			|| !Tick->IsBuildingSupportedInProvince(Province.Id, Building.Id))
		{
			continue;
		}

		const int64 Income = UWLEconomyLibrary::CalculateBuildingIncomeWithRules(Building, BalanceRules);
		if (Income <= 0)
		{
			continue;
		}
		const int64 Fit =
			  static_cast<int64>(Province.BaseOil)      * Building.BonusOil
			+ static_cast<int64>(Province.BaseGas)      * Building.BonusGas
			+ static_cast<int64>(Province.BaseFood)     * Building.BonusFood
			+ static_cast<int64>(Province.BaseMinerals) * Building.BonusMinerals
			+ static_cast<int64>(Province.BaseIndustry) * Building.BonusIndustry;

		if (!bFound
			|| Fit > BestFit
			|| (Fit == BestFit && Income > BestIncome)
			|| (Fit == BestFit && Income == BestIncome && Building.Cost < BestCost))
		{
			bFound = true;
			BestFit = Fit;
			BestIncome = Income;
			BestCost = Building.Cost;
			OutBuilding = Building;
		}
	}

	return bFound;
}

void AWLCampaignPlayerController::SetLastActionMessage(const FString& Message, bool bSucceeded)
{
	LastActionMessage = Message;
	bLastActionSucceeded = bSucceeded;
}
