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

void AWLCampaignPlayerController::UpdateCampaignForceHover()
{
	if (!HasCampaignInput() || ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D)
	{
		if (Campaign3DView)
		{
			Campaign3DView->SetHoveredForceHighlight(TEXT(""));
		}
		return;
	}

	if (!Campaign3DView)
	{
		CachePresentationActors();
	}
	if (!Campaign3DView)
	{
		return;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY) || IsScreenPointOverCampaignHud(MouseX, MouseY))
	{
		Campaign3DView->SetHoveredForceHighlight(TEXT(""));
		return;
	}

	FWLCampaign3DForceView ForceView;
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit)
		&& Campaign3DView->TryGetForceForComponent(Hit.GetComponent(), ForceView))
	{
		Campaign3DView->SetHoveredForceHighlight(ForceView.Id);
		return;
	}

	FVector GroundPoint = FVector::ZeroVector;
	if (GetCampaignGroundPointFromScreen(MouseX, MouseY, GroundPoint)
		&& Campaign3DView->TryGetForceNearWorldLocation(GroundPoint, 2800.f, ForceView))
	{
		Campaign3DView->SetHoveredForceHighlight(ForceView.Id);
		return;
	}

	Campaign3DView->SetHoveredForceHighlight(TEXT(""));
}

void AWLCampaignPlayerController::UpdateCampaignMovementDestinationHover()
{
	if (!HasCampaignInput()
		|| ActivePresentationMode != EWLCampaignPresentationMode::Campaign3D
		|| ActiveSelectionKind != EWLCampaignSelectionKind::Force
		|| ForceMovementOrderMode == EWLCampaignForceMovementOrderMode::DestinationSelected)
	{
		return;
	}

	if (!Campaign3DView)
	{
		CachePresentationActors();
	}
	if (!Campaign3DView)
	{
		return;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY) || IsScreenPointOverCampaignHud(MouseX, MouseY))
	{
		Campaign3DView->SetMovementDestinationPreview(SelectedForceId, TEXT(""));
		return;
	}

	FWLCampaign3DMovementNodeView Destination;
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit)
		&& Campaign3DView->TryGetMovementDestinationForComponent(Hit.GetComponent(), Destination))
	{
		Campaign3DView->SetMovementDestinationPreview(SelectedForceId, Destination.Id);
		return;
	}

	FVector GroundPoint = FVector::ZeroVector;
	if (GetCampaignGroundPointFromScreen(MouseX, MouseY, GroundPoint)
		&& Campaign3DView->TryGetMovementDestinationNearWorldLocation(GroundPoint, 15000.f, Destination))
	{
		Campaign3DView->SetMovementDestinationPreview(SelectedForceId, Destination.Id);
		return;
	}

	Campaign3DView->SetMovementDestinationPreview(SelectedForceId, TEXT(""));
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

bool AWLCampaignPlayerController::GetCampaignCanvasMetrics(float& OutWidth, float& OutHeight, float& OutOffsetX, float& OutOffsetY) const
{
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}

	// Por defecto, Canvas == viewport (sin letterbox).
	OutWidth = static_cast<float>(ViewportX);
	OutHeight = static_cast<float>(ViewportY);
	OutOffsetX = 0.f;
	OutOffsetY = 0.f;

	// El Canvas del HUD es el rect de la vista, que puede estar centrado/letterboxed
	// dentro del viewport cuando la camara fija el aspect ratio (p.ej. 16:9). El cursor
	// llega en espacio viewport; para pasarlo a espacio Canvas restamos el origen del
	// rect de vista (las barras de letterbox), NO escalamos: el pixel del Canvas es el
	// mismo pixel del viewport, solo desplazado.
	if (const AWLCampaignHUD* CampaignHud = Cast<AWLCampaignHUD>(GetHUD()))
	{
		const FVector2D CanvasSize = CampaignHud->GetLastCanvasSize();
		if (CanvasSize.X > 0.f && CanvasSize.Y > 0.f)
		{
			OutWidth = CanvasSize.X;
			OutHeight = CanvasSize.Y;
			OutOffsetX = (static_cast<float>(ViewportX) - CanvasSize.X) * 0.5f;
			OutOffsetY = (static_cast<float>(ViewportY) - CanvasSize.Y) * 0.5f;
		}
	}
	return true;
}

bool AWLCampaignPlayerController::IsScreenPointOverCampaignHud(float ScreenX, float ScreenY)
{
	float W = 0.f;
	float H = 0.f;
	float OffsetX = 0.f;
	float OffsetY = 0.f;
	if (!GetCampaignCanvasMetrics(W, H, OffsetX, OffsetY))
	{
		return false;
	}

	// ScreenX/ScreenY llegan en espacio de mouse/viewport; los convertimos al
	// espacio de Canvas restando el letterbox para compararlos con el layout del HUD.
	ScreenX -= OffsetX;
	ScreenY -= OffsetY;

	if (ScreenY <= 156.f || ScreenY >= H - 56.f)
	{
		return true;
	}

	if (ActivePresentationMode == EWLCampaignPresentationMode::Campaign3D && HasCampaignSelectionPanel())
	{
		const float PanelW = 430.f;
		const float PanelX = W - 468.f;
		const float PanelY = 154.f;
		const float PanelH = FMath::Clamp(H - PanelY - 54.f, 360.f, 690.f);
		if (ScreenX >= PanelX && ScreenX <= PanelX + PanelW && ScreenY >= PanelY && ScreenY <= PanelY + PanelH)
		{
			return true;
		}
	}

	return ScreenX >= W - 480.f && ScreenY <= 190.f;
}

void AWLCampaignPlayerController::ClampCampaignCameraLocation(FVector& Location) const
{
	Location.Z = FMath::Clamp(Location.Z, CampaignMinCameraHeight, CampaignMaxCameraHeight);
	if (!Campaign3DView)
	{
		return;
	}

	// Suelo de zoom RELATIVO al terreno: con un tope absoluto bajo (11000) la camara se enterraria en
	// el relieve alto (Andes ~48000u). Mantenemos al menos CampaignMinAboveGround sobre el suelo bajo
	// la camara -> acercamiento cercano y consistente en costa Y montana, sin enterrarse.
	const float GroundZ = Campaign3DView->GetGroundWorldZAtWorld(Location.X, Location.Y);
	Location.Z = FMath::Min(FMath::Max(Location.Z, GroundZ + CampaignMinAboveGround), CampaignMaxCameraHeight);

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
		// Con la ventana de Gobierno abierta (widget UMG modal) el input va al widget, no aqui.
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
	if (Params.Key == EKeys::C)
	{
		ToggleGovernmentWindow();
		return true;
	}
	if (Params.Key == EKeys::E)
	{
		// [E] abre el modal de eventos si hay decisiones pendientes (si no, no molesta).
		ShowEventModalIfPending();
		return true;
	}

	return false;
}
