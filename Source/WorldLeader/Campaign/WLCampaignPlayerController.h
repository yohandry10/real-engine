// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "UI/WLCampaignBuildingSlotData.h"
#include "GameFramework/PlayerController.h"
#include "WLCampaignPlayerController.generated.h"

class UWLStrategicTickSubsystem;
class UWLDataRegistry;
class AWLWorldMap;
class AWLCampaign3DView;
class UWLMainMenuWidget;
struct FWLCampaign3DCityView;
struct FWLCampaign3DForceView;
struct FWLCampaign3DMovementNodeView;
struct FWLCampaignTerritoryRegionView;
struct FInputKeyEventArgs;

UENUM(BlueprintType)
enum class EWLCampaignPresentationMode : uint8
{
	Campaign3D,
	Diplomacy
};

UENUM(BlueprintType)
enum class EWLCampaignSelectionKind : uint8
{
	None,
	Province,
	City,
	Force
};

enum class EWLCampaignForceMovementOrderMode : uint8
{
	None,
	SelectingDestination,
	DestinationSelected
};

/**
 * PlayerController de campania. Input del jugador (regla del roadmap).
 * Por ahora, atajos de teclado para la vertical slice:
 *   M -> avanzar un mes (tick estrategico)
 *   P -> imprimir el estado al log
 *   Mouse en bordes / arrastre / rueda -> navegar el mapa estrategico
 */
UCLASS()
class WORLDLEADER_API AWLCampaignPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWLCampaignPlayerController();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	void StartCampaignFromMenu(const FString& NationIso);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Save")
	void LoadCampaignFromMenu();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Save")
	void SaveActiveCampaign();

	bool HasSelectedCountry() const { return bHasSelectedCountry; }
	const FString& GetSelectedCountryName() const { return SelectedCountryName; }
	const FString& GetSelectedCountryIso() const { return SelectedCountryIso; }
	const FString& GetSelectedCountryContinent() const { return SelectedCountryContinent; }
	bool HasSelectedProvince() const { return bHasSelectedProvince; }
	const FString& GetSelectedProvinceId() const { return SelectedProvinceId; }
	const FString& GetSelectedProvinceName() const { return SelectedProvinceName; }
	const FString& GetSelectedProvinceCountryIso() const { return SelectedProvinceCountryIso; }
	const FString& GetSelectedProvinceRegion() const { return SelectedProvinceRegion; }
	EWLCampaignSelectionKind GetCampaignSelectionKind() const { return ActiveSelectionKind; }
	bool HasCampaignSelectionPanel() const { return ActiveSelectionKind != EWLCampaignSelectionKind::None; }
	const FString& GetCampaignSelectionId() const { return SelectedPanelObjectId; }
	const FString& GetSelectedTerritoryId() const { return SelectedTerritoryId; }
	const FString& GetSelectedTerritoryName() const { return SelectedTerritoryName; }
	const FString& GetSelectedTerritoryCountryIso() const { return SelectedTerritoryCountryIso; }
	const FString& GetSelectedTerritoryType() const { return SelectedTerritoryType; }
	const FString& GetSelectedCityId() const { return SelectedCityId; }
	const FString& GetSelectedCityName() const { return SelectedCityName; }
	const FString& GetSelectedCityCountryIso() const { return SelectedCityCountryIso; }
	const FString& GetSelectedCityTerritoryId() const { return SelectedCityTerritoryId; }
	const FString& GetSelectedCityTerritoryName() const { return SelectedCityTerritoryName; }
	const FString& GetSelectedCityType() const { return SelectedCityType; }
	const FString& GetSelectedForceId() const { return SelectedForceId; }
	const FString& GetSelectedForceName() const { return SelectedForceName; }
	const FString& GetSelectedForceCountryIso() const { return SelectedForceCountryIso; }
	const FString& GetSelectedForceCountryName() const { return SelectedForceCountryName; }
	const FString& GetSelectedForceType() const { return SelectedForceType; }
	const FString& GetSelectedForceLocation() const { return SelectedForceLocation; }
	const FString& GetSelectedForceProvinceId() const { return SelectedForceProvinceId; }
	const FString& GetSelectedForceProvinceName() const { return SelectedForceProvinceName; }
	const FString& GetSelectedForceNearbyCity() const { return SelectedForceNearbyCity; }
	const FString& GetSelectedForceMobility() const { return SelectedForceMobility; }
	const FString& GetSelectedForceOperationalState() const { return SelectedForceOperationalState; }
	const FString& GetSelectedForceSupply() const { return SelectedForceSupply; }
	const FString& GetSelectedForceMorale() const { return SelectedForceMorale; }
	const FString& GetSelectedForcePosture() const { return SelectedForcePosture; }
	const FString& GetSelectedForceStrategicRole() const { return SelectedForceStrategicRole; }
	const FString& GetSelectedForceDetailLevel() const { return SelectedForceDetailLevel; }
	const TArray<FString>& GetSelectedForceDisabledActions() const { return SelectedForceDisabledActions; }
	int32 GetSelectedForceEstimatedStrength() const { return SelectedForceEstimatedStrength; }
	bool CanSelectedForceMove() const { return bSelectedForceMovable; }
	bool IsForceMovementModeActive() const { return ForceMovementOrderMode != EWLCampaignForceMovementOrderMode::None; }
	bool HasForceMovementDestination() const { return ForceMovementOrderMode == EWLCampaignForceMovementOrderMode::DestinationSelected; }
	const FString& GetForceMovementDestinationName() const { return PendingForceMoveDestinationName; }
	const FString& GetForceMovementRouteSummary() const { return PendingForceMoveRouteSummary; }
	const FString& GetForceMovementStatus() const { return SelectedForceMovementStatus; }
	int32 GetForceMovementEstimatedTurns() const { return PendingForceMoveEstimatedTurns; }
	bool HasSelectedBuildingSlot() const { return !SelectedBuildingSlotKey.IsEmpty(); }
	const FString& GetSelectedBuildingSlotKey() const { return SelectedBuildingSlotKey; }
	const FString& GetSelectedBuildingSlotLabel() const { return SelectedBuildingSlotLabel; }
	const FString& GetSelectedCampaignBuildingId() const { return SelectedCampaignBuildingId; }
	bool IsSelectedCampaignBuildingCandidate() const { return bSelectedCampaignBuildingCandidate; }
	EWLCampaignBuildingSlotState GetCampaignBuildingSlotState(const FString& SlotLabel, int32 SlotIndex, bool bCityMode) const;
	FString GetCampaignBuildingIdForSlot(const FString& SlotLabel, int32 SlotIndex, bool bCityMode) const;
	bool HasLastActionMessage() const { return !LastActionMessage.IsEmpty(); }
	const FString& GetLastActionMessage() const { return LastActionMessage; }
	bool WasLastActionSuccessful() const { return bLastActionSucceeded; }
	EWLCampaignPresentationMode GetPresentationMode() const { return ActivePresentationMode; }
	bool IsDiplomacyViewActive() const { return ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy; }
	FString GetCampaignZoomLODLabel() const;
	float GetCampaignCameraHeight() const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|CampaignView")
	void ShowCampaign3DView();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|CampaignView")
	void ShowDiplomacyMapView();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void OnAdvanceMonth();
	void OnPrintState();
	void OnSaveCampaign();
	void OnBuildRecommended();
	void OnToggleDiplomacyView();
	void OnSelectCountry();
	void OnZoomIn();
	void OnZoomOut();
	void OnPanNorth();
	void OnPanSouth();
	void OnPanWest();
	void OnPanEast();
	void ResetCampaignCamera();
	void FocusCampaignTheater();
	void FocusCampaignAmerica();
	void BeginDragPan();
	void EndDragPan();
	void UpdateMapCamera(float DeltaSeconds);
	void MoveMapCamera(const FVector2D& Delta);
	void MoveCampaignCamera(const FVector2D& Delta);
	void ZoomMapCamera(float ZoomFactor);
	void ZoomCampaignCamera(float ZoomFactor);
	void UpdateCampaignForceHover();
	void UpdateCampaignMovementDestinationHover();
	FVector GetCampaignAmericaFocusPoint() const;
	bool GetCampaignGroundPointFromScreen(float ScreenX, float ScreenY, FVector& OutWorldPoint);
	bool GetCampaignZoomAnchor(FVector& OutAnchor, FVector2D& OutScreenPoint);
	bool IsScreenPointOverCampaignHud(float ScreenX, float ScreenY);
	bool GetCampaignCanvasMetrics(float& OutWidth, float& OutHeight, float& OutOffsetX, float& OutOffsetY) const;
	void ClampCampaignCameraLocation(FVector& Location) const;
	bool HandleCampaignInputKey(const FInputKeyEventArgs& Params);
	void CacheWorldMap();
	void CachePresentationActors();
	bool TryHandleViewToggleClick();
	bool TryHandleSelectionPanelClick();
	bool TryHandleForceMovementDestinationClick();
	bool HasCampaignInput() const;
	void EnterCampaignInputMode();
	void ClearSelectedCountry();
	void ClearSelectedProvince();
	void ClearSelectedCity();
	void ClearSelectedForce();
	void ClearCampaignSelection();
	void ClearCampaignBuildingSelection();
	void ClearForceMovementOrderState(bool bClearViewPreview = true);
	void BeginForceMovementOrder();
	void SetForceMovementDestination(const FWLCampaign3DMovementNodeView& Destination);
	void ConfirmForceMovementOrder();
	void CancelForceMovementOrder();
	void SelectCampaignBuildingSlot(const FString& SlotLabel, int32 SlotIndex, bool bCityMode);
	bool TryBuildCampaignPlaceholderBuilding(const FString& BuildingId, FString& OutMessage);
	bool SelectProvince(const FString& ProvinceId);
	void SelectCampaignTerritory(const FWLCampaignTerritoryRegionView& Territory);
	void SelectCampaignCity(const FWLCampaign3DCityView& City);
	void SelectCampaignForce(const FWLCampaign3DForceView& Force);
	bool BuildRecommendedInSelectedProvince(FString& OutMessage);
	bool PickRecommendedBuilding(const FWLProvinceData& Province, FWLBuildingData& OutBuilding) const;
	void SetLastActionMessage(const FString& Message, bool bSucceeded);

	UWLStrategicTickSubsystem* GetTick() const;
	UWLDataRegistry* GetRegistry() const;

	UPROPERTY(EditAnywhere, Category = "WorldLeader|UI")
	TSubclassOf<UWLMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY()
	UWLMainMenuWidget* MainMenuWidget = nullptr;

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float EdgePanMarginPx = 22.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float EdgePanSpeed = 90000.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float MinCameraHeight = 25000.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float MaxCameraHeight = 620000.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float DragPanUnitsPerPixelAt100k = 120.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float CampaignEdgePanSpeed = 36000.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float CampaignMinCameraHeight = 42000.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float CampaignMaxCameraHeight = 4200000.f;

	UPROPERTY() AWLCampaign3DView* Campaign3DView = nullptr;
	UPROPERTY() AWLWorldMap* WorldMap = nullptr;
	EWLCampaignPresentationMode ActivePresentationMode = EWLCampaignPresentationMode::Campaign3D;

	bool bDragPanning = false;
	bool bHasLastDragMouse = false;
	FVector2D LastDragMouse = FVector2D::ZeroVector;
	FVector DragPanAnchorWorld = FVector::ZeroVector;

	bool bHasSelectedCountry = false;
	FString SelectedCountryName;
	FString SelectedCountryIso;
	FString SelectedCountryContinent;

	bool bHasSelectedProvince = false;
	FString SelectedProvinceId;
	FString SelectedProvinceName;
	FString SelectedProvinceCountryIso;
	FString SelectedProvinceRegion;

	EWLCampaignSelectionKind ActiveSelectionKind = EWLCampaignSelectionKind::None;
	FString SelectedPanelObjectId;
	FString SelectedTerritoryId;
	FString SelectedTerritoryName;
	FString SelectedTerritoryCountryIso;
	FString SelectedTerritoryType;
	FString SelectedCityId;
	FString SelectedCityName;
	FString SelectedCityCountryIso;
	FString SelectedCityTerritoryId;
	FString SelectedCityTerritoryName;
	FString SelectedCityType;
	FString SelectedForceId;
	FString SelectedForceName;
	FString SelectedForceCountryIso;
	FString SelectedForceCountryName;
	FString SelectedForceType;
	FString SelectedForceLocation;
	FString SelectedForceProvinceId;
	FString SelectedForceProvinceName;
	FString SelectedForceNearbyCity;
	FString SelectedForceMobility;
	FString SelectedForceOperationalState;
	FString SelectedForceSupply;
	FString SelectedForceMorale;
	FString SelectedForcePosture;
	FString SelectedForceStrategicRole;
	FString SelectedForceDetailLevel;
	TArray<FString> SelectedForceDisabledActions;
	int32 SelectedForceEstimatedStrength = 0;
	bool bSelectedForceMovable = true;
	FString SelectedForceMovementNodeId;
	FString SelectedForceMovementStatus;
	EWLCampaignForceMovementOrderMode ForceMovementOrderMode = EWLCampaignForceMovementOrderMode::None;
	FString PendingForceMoveDestinationNodeId;
	FString PendingForceMoveDestinationName;
	FString PendingForceMoveRouteSummary;
	int32 PendingForceMoveEstimatedTurns = 0;

	TMap<FString, FString> CampaignPlaceholderBuildingsBySlot;
	FString SelectedBuildingSlotKey;
	FString SelectedBuildingSlotLabel;
	FString SelectedCampaignBuildingId;
	int32 SelectedBuildingSlotIndex = INDEX_NONE;
	bool bSelectedBuildingSlotCityMode = false;
	bool bSelectedCampaignBuildingCandidate = false;

	FString LastActionMessage;
	bool bLastActionSucceeded = true;
};
