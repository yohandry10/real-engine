// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLCampaignGameMode.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Economy/WLEconomyLibrary.h"
#include "Map/WLWorldMap.h"
#include "Presentation/WLCampaign3DView.h"
#include "UI/WLCampaignHUD.h"
#include "UI/WLCampaignSelectionPanelData.h"
#include "UI/WLGovernmentWidget.h"
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
#include "Campaign/WLCampaignPlayerControllerPrivate.h"

using namespace WLCampaignPlayerControllerPrivate;

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
	float ViewportX = 0.f;
	float ViewportY = 0.f;
	float OffsetX = 0.f;
	float OffsetY = 0.f;
	if (!GetCampaignCanvasMetrics(ViewportX, ViewportY, OffsetX, OffsetY))
	{
		return false;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	// Convertimos el cursor de espacio viewport a espacio Canvas restando el letterbox.
	MouseX -= OffsetX;
	MouseY -= OffsetY;

	// Boton "GOBIERNO" (barra superior, izquierda de "Campana 3D"): abre/cierra la ventana de presidencia.
	{
		const FBox2D GovButton = WLGovernmentLayout::GovernmentButton(ViewportX);
		if (GovButton.IsInside(FVector2D(MouseX, MouseY)))
		{
			ToggleGovernmentWindow();
			return true;
		}
	}

	const float Y = 58.f;
	const float Height = 38.f;
	const float CampaignX = ViewportX - 386.f;
	const float DiplomacyX = ViewportX - 210.f;
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

	// Boton grande "Avanzar dia" abajo-derecha (estilo "Finalizar turno" de Total War). MISMO rect que el HUD.
	if (ActivePresentationMode == EWLCampaignPresentationMode::Campaign3D)
	{
		const float AdvW = 220.f;
		const float AdvH = 50.f;
		const float AdvX = ViewportX - AdvW - 26.f;
		const float AdvY = ViewportY - AdvH - 28.f;
		if (MouseX >= AdvX && MouseX <= AdvX + AdvW && MouseY >= AdvY && MouseY <= AdvY + AdvH)
		{
			OnAdvanceMonth();
			return true;
		}
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
		const float ZoomInX = ViewportX - 444.f;
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

void AWLCampaignPlayerController::ToggleGovernmentWindow()
{
	SetGovernmentWindowOpen(!bGovernmentWindowOpen);
}

void AWLCampaignPlayerController::SetGovernmentWindowOpen(bool bOpen)
{
	if (bGovernmentWindowOpen == bOpen)
	{
		return;
	}
	bGovernmentWindowOpen = bOpen;

	if (bOpen)
	{
		// La ventana es un widget UMG modal (mismo patron que el menu principal): se crea al abrir y
		// captura el input (UIOnly) mientras esta arriba. El propio widget gestiona pestanas y cierre.
		if (!GovernmentWidget)
		{
			GovernmentWidget = CreateWidget<UWLGovernmentWidget>(this, UWLGovernmentWidget::StaticClass());
		}
		if (GovernmentWidget)
		{
			GovernmentWidget->AddToViewport(120);
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(GovernmentWidget->TakeWidget());
			SetInputMode(InputMode);
			bShowMouseCursor = true;
		}
		SetLastActionMessage(TEXT("Gobierno: consejo presidencial abierto."), true);
	}
	else
	{
		if (GovernmentWidget)
		{
			GovernmentWidget->RemoveFromParent();
			GovernmentWidget = nullptr;
		}
		// Restauramos el input de campania (el mapa vuelve a ser navegable).
		EnterCampaignInputMode();
		SetLastActionMessage(TEXT("Gobierno: consejo presidencial cerrado."), true);
	}
}

bool AWLCampaignPlayerController::TryHandleSelectionPanelClick()
{
	if (ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D || !HasCampaignSelectionPanel())
	{
		return false;
	}

	float ViewportX = 0.f;
	float ViewportY = 0.f;
	float OffsetX = 0.f;
	float OffsetY = 0.f;
	if (!GetCampaignCanvasMetrics(ViewportX, ViewportY, OffsetX, OffsetY))
	{
		return false;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	// Convertimos el cursor de espacio viewport a espacio Canvas restando el letterbox.
	MouseX -= OffsetX;
	MouseY -= OffsetY;

	const float PanelW = 430.f;
	const float PanelX = ViewportX - 468.f;
	const float PanelY = 154.f;
	const float PanelH = FMath::Clamp(ViewportY - PanelY - 54.f, 360.f, 690.f);
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

	if (ActiveSelectionKind == EWLCampaignSelectionKind::Force)
	{
		const float ButtonW = (PanelW - 48.f) * 0.5f;
		const float ButtonH = 28.f;
		const float ButtonGap = 8.f;
		const float ActionY = GetCampaignControllerForceActionStartY(PanelY);
		const float ButtonX0 = PanelX + 18.f;
		const float ButtonX1 = ButtonX0 + ButtonW + ButtonGap;

		if (IsPointInControllerRect(MouseX, MouseY, ButtonX0, ActionY, ButtonW, ButtonH))
		{
			BeginForceMovementOrder();
			return true;
		}

		if (IsForceMovementModeActive())
		{
			const float ConfirmY = ActionY + 86.f;
			if (IsPointInControllerRect(MouseX, MouseY, ButtonX0, ConfirmY, ButtonW, ButtonH))
			{
				ConfirmForceMovementOrder();
				return true;
			}
			if (IsPointInControllerRect(MouseX, MouseY, ButtonX1, ConfirmY, ButtonW, ButtonH))
			{
				CancelForceMovementOrder();
				return true;
			}
		}

		// Boton RECLUTAR (solo en modo no-movimiento). MISMO Y/anchura que el dibujo del panel
		// (DisabledStartY = ActionY+40 en WLCampaignHUDPanels.inl) -> mantener en sync.
		if (!IsForceMovementModeActive())
		{
			const TArray<FWLCampaignRecruitButton> Options = GetSelectedForceRecruitOptions();
			const float RBtnW = (PanelW - 48.f) * 0.5f;
			const float RBtnH = 60.f;                       // cards grandes (icono+nombre+coste+dias) -> sync con el .inl
			const float RBtnGap = 8.f;
			const float RGridY = (ActionY + 40.f) + 22.f;   // = DisabledStartY(no-mov)+22 (sync con el .inl)
			const int32 MaxOpt = FMath::Min(Options.Num(), 6);
			for (int32 i = 0; i < MaxOpt; ++i)
			{
				const float bx = PanelX + 18.f + static_cast<float>(i % 2) * (RBtnW + RBtnGap);
				const float by = RGridY + static_cast<float>(i / 2) * (RBtnH + RBtnGap);
				if (IsPointInControllerRect(MouseX, MouseY, bx, by, RBtnW, RBtnH))
				{
					FString Msg;
					const bool bOk = QueueRecruitForSelectedForce(Options[i].UnitType, Msg);
					SetLastActionMessage(Msg, bOk);
					return true;
				}
			}
		}

		SetLastActionMessage(TEXT("Fuerza seleccionada. Pulsa Mover o Reclutar."), true);
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

bool AWLCampaignPlayerController::TryHandleForceMovementDestinationClick()
{
	if (ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D
		|| ActiveSelectionKind != EWLCampaignSelectionKind::Force
		|| !IsForceMovementModeActive())
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

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY) || IsScreenPointOverCampaignHud(MouseX, MouseY))
	{
		return false;
	}

	FWLCampaign3DMovementNodeView Destination;
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit)
		&& Campaign3DView->TryGetMovementDestinationForComponent(Hit.GetComponent(), Destination))
	{
		SetForceMovementDestination(Destination);
		return true;
	}

	FVector GroundPoint = FVector::ZeroVector;
	if (GetCampaignGroundPointFromScreen(MouseX, MouseY, GroundPoint)
		&& Campaign3DView->TryGetMovementDestinationNearWorldLocation(GroundPoint, 15000.f, Destination))
	{
		SetForceMovementDestination(Destination);
		return true;
	}

	SetLastActionMessage(TEXT("Destino invalido. Elige un nodo resaltado del teatro Colombia/Venezuela."), false);
	return true;
}

bool AWLCampaignPlayerController::TryHandleArmyMoveClick()
{
	// Solo si hay un EJERCITO movil seleccionado (no un fuerte fijo) y no estamos en el modo de orden por
	// botones. Clic en una ciudad => marcha directa hacia ella.
	if (ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D
		|| ActiveSelectionKind != EWLCampaignSelectionKind::Force
		|| SelectedForceId.IsEmpty()
		|| !bSelectedForceMovable
		|| IsForceMovementModeActive())
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

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY) || IsScreenPointOverCampaignHud(MouseX, MouseY))
	{
		return false;
	}

	// Punto EXACTO bajo el cursor (terreno/carretera) via raycast real, SIN el parallax del plano plano que
	// usaba GetCampaignGroundPointFromScreen (con camara oblicua el ejercito iba a otra X,Y que la clicada).
	FVector ClickWorld = FVector::ZeroVector;
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit) && Hit.bBlockingHit)
	{
		ClickWorld = Hit.Location;
	}
	else if (!GetCampaignGroundPointFromScreen(MouseX, MouseY, ClickWorld))
	{
		return false;
	}

	// Movimiento LIBRE por carretera: el ejercito recorre la VIA hasta el punto clicado y se DETIENE ahi, a
	// mitad de camino si quieres. NO salta a ciudades; avanza su alcance por turno (varios turnos si lejos).
	FWLCampaign3DForceView Updated;
	if (!Campaign3DView->SetForceFreeMoveTarget(SelectedForceId, ClickWorld, Updated))
	{
		return false;
	}

	SelectedForceLocation = Updated.LocationName;
	SelectedForceNearbyCity = Updated.NearbyCity;
	SelectedForceProvinceId = Updated.ProvinceId;
	SelectedForceProvinceName = Updated.ProvinceName;
	SelectedForceMovementNodeId = Updated.MovementNodeId;
	SelectedForceMovementStatus = Updated.MovementStatus;
	SelectedForceOperationalState = Updated.OperationalState;
	SelectedForcePosture = Updated.Posture;
	SetLastActionMessage(TEXT("Ejercito en marcha por la carretera."), true);
	return true;
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
