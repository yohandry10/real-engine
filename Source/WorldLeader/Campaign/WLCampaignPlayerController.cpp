// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLCampaignGameMode.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Economy/WLEconomyLibrary.h"
#include "Map/WLWorldMap.h"
#include "UI/WLMainMenuWidget.h"
#include "WorldLeader.h"
#include "Camera/CameraActor.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AWLCampaignPlayerController::AWLCampaignPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
}

void AWLCampaignPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (HasCampaignInput())
	{
		EnterCampaignInputMode();
		CacheWorldMap();
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
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AWLCampaignPlayerController::OnSelectCountry);
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AWLCampaignPlayerController::OnZoomIn);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AWLCampaignPlayerController::OnZoomOut);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AWLCampaignPlayerController::BeginDragPan);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AWLCampaignPlayerController::EndDragPan);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AWLCampaignPlayerController::BeginDragPan);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &AWLCampaignPlayerController::EndDragPan);
	}
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

void AWLCampaignPlayerController::OnSelectCountry()
{
	if (!HasCampaignInput())
	{
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

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
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

	ZoomMapCamera(0.82f);
}

void AWLCampaignPlayerController::OnZoomOut()
{
	if (!HasCampaignInput())
	{
		return;
	}

	ZoomMapCamera(1.18f);
}

void AWLCampaignPlayerController::BeginDragPan()
{
	if (!HasCampaignInput())
	{
		return;
	}

	bDragPanning = true;
	bHasLastDragMouse = false;
}

void AWLCampaignPlayerController::EndDragPan()
{
	bDragPanning = false;
	bHasLastDragMouse = false;
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

void AWLCampaignPlayerController::UpdateMapCamera(float DeltaSeconds)
{
	if (!HasCampaignInput())
	{
		return;
	}

	if (!WorldMap)
	{
		CacheWorldMap();
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
		if (bHasLastDragMouse)
		{
			const FVector2D MouseDelta = CurrentMouse - LastDragMouse;
			const float UnitsPerPixel = DragPanUnitsPerPixelAt100k * FMath::Clamp(CameraLocation.Z / 100000.f, 0.25f, 6.0f);
			MoveMapCamera(FVector2D(-MouseDelta.Y, MouseDelta.X) * UnitsPerPixel);
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
		MoveMapCamera(PanDirection * EdgePanSpeed * ZoomSpeedScale * DeltaSeconds);
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

	ClearSelectedCountry();
	ClearSelectedProvince();
	CacheWorldMap();
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

	ClearSelectedCountry();
	ClearSelectedProvince();
	CacheWorldMap();
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
	SetInputMode(InputMode);
	bShowMouseCursor = true;
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
