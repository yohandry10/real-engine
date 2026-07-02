// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLCampaignGameMode.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Economy/WLEconomyLibrary.h"
#include "Map/WLWorldMap.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "Presentation/WLCampaign3DView.h"
#include "UI/WLCampaignHUD.h"
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
	if (IsForceMovementModeActive())
	{
		UpdateCampaignMovementDestinationHover();
	}
	else
	{
		UpdateCampaignForceHover();
	}
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

UWLPoliticalSubsystem* AWLCampaignPlayerController::GetPolitics() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLPoliticalSubsystem>() : nullptr;
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

	// F5.3/F5.4: con la campana terminada el tiempo no avanza mas (fin de partida REAL).
	if (UWLPoliticalSubsystem* Politics = GetPolitics())
	{
		const FWLCampaignOutcomeState Outcome = Politics->GetCampaignOutcome();
		if (Outcome.bGameOver)
		{
			SetLastActionMessage(FString::Printf(TEXT("La partida ha terminado: %s Abre GOBIERNO [C] para el resumen."),
				*Outcome.Reason), false);
			return;
		}
	}

	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		const int32 PreviousMonth = Tick->GetCurrentMonth();
		const int32 PreviousYear = Tick->GetCurrentYear();
		Tick->AdvanceDay();   // avanza UN DIA (economia/reclutamiento cada dia; IA al cerrar mes)
		if ((Tick->GetCurrentMonth() != PreviousMonth || Tick->GetCurrentYear() != PreviousYear))
		{
			if (UWLPoliticalSubsystem* Politics = GetPolitics())
			{
				Politics->ProcessPoliticalMonth();
			}
			// F5: al cerrar el mes, si el nuevo estado dejo eventos sin resolver, el juego los
			// pone en primer plano con el popup modal (no hay que adivinar que abrir GOBIERNO).
			ShowEventModalIfPending();
		}
		SetLastActionMessage(FString::Printf(TEXT("Dia avanzado: %02d/%02d/%d."),
			Tick->GetCurrentDay(), Tick->GetCurrentMonth(), Tick->GetCurrentYear()), true);

		// Tras avanzar el mes: (1) materializa/actualiza el ejercito movible de cada fuerte que termino de
		// reclutar, y (2) re-evalua el LOD a la altura de camara actual para que el token de ejercito (tanque)
		// aparezca de inmediato. Sin esto, el ejercito no se mostraria hasta el siguiente zoom/pan.
		if (Campaign3DView)
		{
			Campaign3DView->AdvanceArmyMovements();   // ejercitos en marcha avanzan su cargador por la carretera
			Campaign3DView->SyncRecruitedArmyTokens();
			Campaign3DView->ApplyZoomLOD(GetCampaignCameraHeight());
		}
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
	if (IsForceMovementModeActive())
	{
		TryHandleForceMovementDestinationClick();
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

		FWLCampaign3DForceView ForceView;
		if (Campaign3DView->TryGetForceNearWorldLocation(WorldLocation, 2800.f, ForceView))
		{
			SelectCampaignForce(ForceView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Fuerza seleccionada en Campaign 3D: %s."), *SelectedForceName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected force by %s: %s (%s)"), SourceLabel, *SelectedForceName, *SelectedForceId);
			return true;
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

		// NADA de territorio/provincia: clicar terreno vacio no selecciona la provincia (antes salia el
		// "circulo que nombra la provincia"). Solo fuerza o ciudad; la provincia se ve en el panel de ciudad.
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
		FWLCampaign3DForceView ForceView;
		if (Campaign3DView && Campaign3DView->TryGetForceForComponent(Hit.GetComponent(), ForceView))
		{
			SelectCampaignForce(ForceView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Fuerza seleccionada en Campaign 3D: %s."), *SelectedForceName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected force: %s (%s)"), *SelectedForceName, *SelectedForceId);
			return;
		}
		if (Campaign3DView && Campaign3DView->TryGetForceNearWorldLocation(Hit.Location, 2800.f, ForceView))
		{
			SelectCampaignForce(ForceView);
			ClearSelectedCountry();
			SetLastActionMessage(FString::Printf(TEXT("Fuerza seleccionada en Campaign 3D: %s."), *SelectedForceName), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D selected force by location: %s (%s)"), *SelectedForceName, *SelectedForceId);
			return;
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

		// Clic = solo FUERZA o CIUDAD. NO se selecciona territorio/provincia (eso dibujaba el "circulo que
		// nombra la provincia" en cualquier zoom). La info de la provincia se muestra DENTRO del panel de la
		// ciudad (campo Territorio). Fallback: intenta fuerza/ciudad en el plano del cursor.
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
