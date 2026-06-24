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
		ZoomCampaignCamera(0.66f);
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
		ZoomCampaignCamera(1.52f);
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
	// La rueda vive en el rango jugable (ciudad -> teatro): tope CampaignWheelMaxHeight, no
	// CampaignMaxCameraHeight. El continente se ve con el boton "America".
	const float DesiredZ = FMath::Clamp(Location.Z * ZoomFactor, CampaignMinCameraHeight, CampaignWheelMaxHeight);
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
