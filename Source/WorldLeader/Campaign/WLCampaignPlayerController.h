// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "GameFramework/PlayerController.h"
#include "WLCampaignPlayerController.generated.h"

class UWLStrategicTickSubsystem;
class UWLDataRegistry;
class AWLWorldMap;
class AWLCampaign3DView;
class UWLMainMenuWidget;

UENUM(BlueprintType)
enum class EWLCampaignPresentationMode : uint8
{
	Campaign3D,
	Diplomacy
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
	bool HasLastActionMessage() const { return !LastActionMessage.IsEmpty(); }
	const FString& GetLastActionMessage() const { return LastActionMessage; }
	bool WasLastActionSuccessful() const { return bLastActionSucceeded; }
	EWLCampaignPresentationMode GetPresentationMode() const { return ActivePresentationMode; }
	bool IsDiplomacyViewActive() const { return ActivePresentationMode == EWLCampaignPresentationMode::Diplomacy; }

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|CampaignView")
	void ShowCampaign3DView();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|CampaignView")
	void ShowDiplomacyMapView();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;

private:
	void OnAdvanceMonth();
	void OnPrintState();
	void OnSaveCampaign();
	void OnBuildRecommended();
	void OnToggleDiplomacyView();
	void OnSelectCountry();
	void OnZoomIn();
	void OnZoomOut();
	void BeginDragPan();
	void EndDragPan();
	void UpdateMapCamera(float DeltaSeconds);
	void MoveMapCamera(const FVector2D& Delta);
	void MoveCampaignCamera(const FVector2D& Delta);
	void ZoomMapCamera(float ZoomFactor);
	void ZoomCampaignCamera(float ZoomFactor);
	void CacheWorldMap();
	void CachePresentationActors();
	bool TryHandleViewToggleClick();
	bool HasCampaignInput() const;
	void EnterCampaignInputMode();
	void ClearSelectedCountry();
	void ClearSelectedProvince();
	bool SelectProvince(const FString& ProvinceId);
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
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Camera") float CampaignMaxCameraHeight = 170000.f;

	UPROPERTY() AWLCampaign3DView* Campaign3DView = nullptr;
	UPROPERTY() AWLWorldMap* WorldMap = nullptr;
	EWLCampaignPresentationMode ActivePresentationMode = EWLCampaignPresentationMode::Campaign3D;

	bool bDragPanning = false;
	bool bHasLastDragMouse = false;
	FVector2D LastDragMouse = FVector2D::ZeroVector;

	bool bHasSelectedCountry = false;
	FString SelectedCountryName;
	FString SelectedCountryIso;
	FString SelectedCountryContinent;

	bool bHasSelectedProvince = false;
	FString SelectedProvinceId;
	FString SelectedProvinceName;
	FString SelectedProvinceCountryIso;
	FString SelectedProvinceRegion;

	FString LastActionMessage;
	bool bLastActionSucceeded = true;
};
