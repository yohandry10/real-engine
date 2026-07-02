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
#include "Campaign/WLCampaignPlayerControllerPrivate.h"

using namespace WLCampaignPlayerControllerPrivate;

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

void AWLCampaignPlayerController::ClearSelectedForce()
{
	SelectedForceId.Reset();
	SelectedForceName.Reset();
	SelectedForceCountryIso.Reset();
	SelectedForceCountryName.Reset();
	SelectedForceType.Reset();
	SelectedForceLocation.Reset();
	SelectedForceProvinceId.Reset();
	SelectedForceProvinceName.Reset();
	SelectedForceNearbyCity.Reset();
	SelectedForceMobility.Reset();
	SelectedForceOperationalState.Reset();
	SelectedForceSupply.Reset();
	SelectedForceMorale.Reset();
	SelectedForcePosture.Reset();
	SelectedForceStrategicRole.Reset();
	SelectedForceDetailLevel.Reset();
	SelectedForceDisabledActions.Reset();
	SelectedForceEstimatedStrength = 0;
	bSelectedForceMovable = true;
	SelectedForceMovementNodeId.Reset();
	SelectedForceMovementStatus.Reset();
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

void AWLCampaignPlayerController::ClearForceMovementOrderState(bool bClearViewPreview)
{
	ForceMovementOrderMode = EWLCampaignForceMovementOrderMode::None;
	PendingForceMoveDestinationNodeId.Reset();
	PendingForceMoveDestinationName.Reset();
	PendingForceMoveRouteSummary.Reset();
	PendingForceMoveEstimatedTurns = 0;
	if (bClearViewPreview)
	{
		if (!Campaign3DView)
		{
			CachePresentationActors();
		}
		if (Campaign3DView)
		{
			Campaign3DView->ClearMovementPreview();
		}
	}
}

void AWLCampaignPlayerController::BeginForceMovementOrder()
{
	if (ActiveSelectionKind != EWLCampaignSelectionKind::Force || SelectedForceId.IsEmpty())
	{
		return;
	}
	if (!bSelectedForceMovable)
	{
		SetLastActionMessage(TEXT("Esta fuerza esta fija y no puede moverse en esta fase."), false);
		return;
	}
	if (!Campaign3DView)
	{
		CachePresentationActors();
	}
	if (!Campaign3DView)
	{
		SetLastActionMessage(TEXT("Campaign 3D no disponible para orden de movimiento."), false);
		return;
	}

	ForceMovementOrderMode = EWLCampaignForceMovementOrderMode::SelectingDestination;
	PendingForceMoveDestinationNodeId.Reset();
	PendingForceMoveDestinationName.Reset();
	PendingForceMoveRouteSummary.Reset();
	PendingForceMoveEstimatedTurns = 0;
	SelectedForceMovementStatus = TEXT("esperando destino");
	Campaign3DView->ShowMovementDestinationOptions(SelectedForceId);
	SetLastActionMessage(TEXT("Modo movimiento activo: elige un destino resaltado."), true);
}

void AWLCampaignPlayerController::SetForceMovementDestination(const FWLCampaign3DMovementNodeView& Destination)
{
	if (ActiveSelectionKind != EWLCampaignSelectionKind::Force || SelectedForceId.IsEmpty())
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

	TArray<FWLCampaign3DMovementNodeView> RouteNodes;
	int32 EstimatedTurns = 0;
	if (!Campaign3DView->BuildMovementRouteForForce(SelectedForceId, Destination.Id, RouteNodes, EstimatedTurns))
	{
		SetLastActionMessage(TEXT("No hay ruta valida para esa fuerza."), false);
		return;
	}

	TArray<FString> RouteNames;
	for (const FWLCampaign3DMovementNodeView& Node : RouteNodes)
	{
		RouteNames.Add(Node.Name);
	}

	ForceMovementOrderMode = EWLCampaignForceMovementOrderMode::DestinationSelected;
	PendingForceMoveDestinationNodeId = Destination.Id;
	PendingForceMoveDestinationName = Destination.Name;
	PendingForceMoveRouteSummary = FString::Join(RouteNames, TEXT(" -> "));
	PendingForceMoveEstimatedTurns = EstimatedTurns;
	SelectedForceMovementStatus = TEXT("destino seleccionado");
	Campaign3DView->SetMovementDestinationPreview(SelectedForceId, Destination.Id);
	SetLastActionMessage(FString::Printf(TEXT("Destino seleccionado: %s. Confirma o cancela el movimiento."), *Destination.Name), true);
}

void AWLCampaignPlayerController::ConfirmForceMovementOrder()
{
	if (ActiveSelectionKind != EWLCampaignSelectionKind::Force || SelectedForceId.IsEmpty())
	{
		return;
	}
	if (ForceMovementOrderMode != EWLCampaignForceMovementOrderMode::DestinationSelected || PendingForceMoveDestinationNodeId.IsEmpty())
	{
		SetLastActionMessage(TEXT("Selecciona un destino valido antes de confirmar."), false);
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

	FWLCampaign3DForceView UpdatedForce;
	if (!Campaign3DView->UpdateForceMovementLocation(SelectedForceId, PendingForceMoveDestinationNodeId, UpdatedForce))
	{
		SetLastActionMessage(TEXT("No se pudo ejecutar el movimiento."), false);
		return;
	}

	const FString DestinationName = PendingForceMoveDestinationName;
	SelectedForceLocation = UpdatedForce.LocationName;
	SelectedForceProvinceId = UpdatedForce.ProvinceId;
	SelectedForceProvinceName = UpdatedForce.ProvinceName;
	SelectedForceNearbyCity = UpdatedForce.NearbyCity;
	SelectedForceOperationalState = UpdatedForce.OperationalState;
	SelectedForcePosture = UpdatedForce.Posture;
	SelectedForceMovementNodeId = UpdatedForce.MovementNodeId;
	SelectedForceMovementStatus = UpdatedForce.MovementStatus;
	ClearForceMovementOrderState(true);
	SetLastActionMessage(FString::Printf(TEXT("Movimiento confirmado hacia %s."), *DestinationName), true);
}

void AWLCampaignPlayerController::CancelForceMovementOrder()
{
	if (!IsForceMovementModeActive())
	{
		return;
	}
	ClearForceMovementOrderState(true);
	SelectedForceMovementStatus = TEXT("detenido");
	SetLastActionMessage(TEXT("Movimiento cancelado."), true);
}

void AWLCampaignPlayerController::ClearCampaignSelection()
{
	ClearSelectedCountry();
	ClearSelectedProvince();
	ClearSelectedCity();
	ClearSelectedForce();
	ClearCampaignBuildingSelection();
	ClearForceMovementOrderState(true);
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
	ClearSelectedForce();
	ClearSelectedProvince();
	ClearCampaignBuildingSelection();
	ClearForceMovementOrderState(true);
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
	ClearSelectedForce();
	ClearCampaignBuildingSelection();
	ClearForceMovementOrderState(true);
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

void AWLCampaignPlayerController::SelectCampaignForce(const FWLCampaign3DForceView& Force)
{
	ClearSelectedProvince();
	ClearSelectedCity();
	ClearCampaignBuildingSelection();
	ClearForceMovementOrderState(true);
	ActiveSelectionKind = EWLCampaignSelectionKind::Force;
	SelectedPanelObjectId = Force.Id;
	SelectedTerritoryId.Reset();
	SelectedTerritoryName.Reset();
	SelectedTerritoryCountryIso.Reset();
	SelectedTerritoryType.Reset();
	SelectedForceId = Force.Id;
	SelectedForceName = Force.Name;
	SelectedForceCountryIso = Force.CountryIso;
	SelectedForceCountryName = Force.CountryName;
	SelectedForceType = Force.ForceType;
	SelectedForceLocation = Force.LocationName;
	SelectedForceProvinceId = Force.ProvinceId;
	SelectedForceProvinceName = Force.ProvinceName;
	SelectedForceNearbyCity = Force.NearbyCity;
	SelectedForceMobility = Force.Mobility;
	SelectedForceOperationalState = Force.OperationalState;
	SelectedForceSupply = Force.Supply;
	SelectedForceMorale = Force.Morale;
	SelectedForcePosture = Force.Posture;
	SelectedForceStrategicRole = Force.StrategicRole;
	SelectedForceDetailLevel = Force.DetailLevel;
	SelectedForceDisabledActions = Force.DisabledActions;
	SelectedForceEstimatedStrength = Force.EstimatedStrength;
	SelectedForceComposition.Reset();
	for (const FWLForceUnitGroup& Group : Force.Composition)
	{
		FWLCampaignForceCompositionEntry Entry;
		Entry.Label = Group.Label;
		Entry.Count = Group.Count;
		SelectedForceComposition.Add(Entry);
	}
	SelectedForceCategory = Force.MarkerCategory;
	bSelectedForceMovable = Force.bMovable;
	bSelectedForceIsRecruitmentBase = Force.bIsRecruitmentBase;
	SelectedForceMovementNodeId = Force.MovementNodeId;
	SelectedForceMovementStatus = Force.MovementStatus;

	if (!Campaign3DView)
	{
		CachePresentationActors();
	}
	if (Campaign3DView)
	{
		Campaign3DView->SetSelectedForceHighlight(Force.Id);
	}
}

TArray<FWLCampaignForceCompositionEntry> AWLCampaignPlayerController::GetSelectedForceTotalComposition() const
{
	TArray<FWLCampaignForceCompositionEntry> Out = SelectedForceComposition;  // base (del JSON)
	const UGameInstance* GI = GetGameInstance();
	const UWLStrategicTickSubsystem* Tick = GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
	if (Tick && !SelectedForceId.IsEmpty())
	{
		for (const FWLGarrisonGroup& Group : Tick->GetGarrisonRecruited(SelectedForceId))
		{
			bool bMerged = false;
			for (FWLCampaignForceCompositionEntry& Entry : Out)
			{
				if (Entry.Label.Equals(Group.Label, ESearchCase::IgnoreCase))
				{
					Entry.Count += Group.Count;
					bMerged = true;
					break;
				}
			}
			if (!bMerged)
			{
				FWLCampaignForceCompositionEntry Entry;
				Entry.Label = Group.Label;
				Entry.Count = Group.Count;
				Out.Add(Entry);
			}
		}
	}
	return Out;
}

bool AWLCampaignPlayerController::IsSelectedForeignFort() const
{
	if (!bSelectedForceIsRecruitmentBase)
	{
		return false;
	}
	const UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	const FString PlayerIso = GI ? GI->GetSelectedNationIso() : FString();
	return !PlayerIso.IsEmpty() && !SelectedForceCountryIso.Equals(PlayerIso, ESearchCase::IgnoreCase);
}

TArray<FWLCampaignRecruitButton> AWLCampaignPlayerController::GetSelectedForceRecruitOptions() const
{
	TArray<FWLCampaignRecruitButton> Out;
	// Solo los FUERTES (bases de reclutamiento) entrenan tropas. Un ejercito de campo seleccionado muestra
	// sus tropas y se mueve, pero NO recluta (modelo Total War: recluta en el edificio).
	if (!bSelectedForceIsRecruitmentBase)
	{
		return Out;
	}
	// Cada pais es INDEPENDIENTE: el jugador SOLO recluta en los fuertes de SU pais. Los de otros paises (VE,
	// etc.) son suyos -> sin opciones para el jugador. Asi un ejercito de CO jamas sale del tesoro de VE.
	if (IsSelectedForeignFort())
	{
		return Out;
	}
	const UGameInstance* GI = GetGameInstance();
	const UWLStrategicTickSubsystem* Tick = GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
	if (!Tick || SelectedForceId.IsEmpty())
	{
		return Out;
	}
	const FString Cat = SelectedForceCategory.ToLower();
	const bool bAir = Cat.Contains(TEXT("air"));
	const bool bNaval = Cat.Contains(TEXT("naval"));
	for (const FWLRecruitOption& Option : Tick->GetRecruitOptions())
	{
		const FString OptCat = Option.Category.ToLower();
		// Base aerea -> unidades aereas; naval -> buques + infanteria de marina; tierra/fuerte -> terrestres.
		const bool bMatch = bAir
			? OptCat.Contains(TEXT("air"))
			: (bNaval
				? (OptCat.Contains(TEXT("naval")) || Option.UnitType == TEXT("infantry"))
				: OptCat.Contains(TEXT("land")));
		if (!bMatch)
		{
			continue;
		}
		FWLCampaignRecruitButton Button;
		Button.UnitType = Option.UnitType;
		Button.Label = Option.Label;
		Button.Cost = Option.Cost;
		Button.Turns = Option.Turns;
		Out.Add(Button);
	}
	return Out;
}

FString AWLCampaignPlayerController::GetSelectedForceRecruitStatus() const
{
	const UGameInstance* GI = GetGameInstance();
	const UWLStrategicTickSubsystem* Tick = GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
	if (!Tick || SelectedForceId.IsEmpty())
	{
		return FString();
	}
	if (IsSelectedForeignFort())
	{
		return FString::Printf(TEXT("Fuerte de %s (otro pais). Cada pais recluta en sus propios fuertes."), *SelectedForceCountryIso);
	}
	const TArray<FWLRecruitOrder> Queue = Tick->GetRecruitQueue(SelectedForceId);
	if (Queue.Num() == 0)
	{
		return TEXT("Cola vacia. Pulsa Reclutar y avanza el mes [M].");
	}
	const FWLRecruitOrder& Front = Queue[0];
	return FString::Printf(TEXT("Reclutando: %s  faltan %d turno(s)  ·  en cola: %d"), *Front.Label, Front.TurnsRemaining, Queue.Num());
}

bool AWLCampaignPlayerController::QueueRecruitForSelectedForce(const FString& UnitType, FString& OutMessage)
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick || SelectedForceId.IsEmpty())
	{
		OutMessage = TEXT("Selecciona una fuerza primero.");
		return false;
	}
	// Independencia: solo reclutas en los fuertes de TU pais (su tesoro, su ejercito).
	if (IsSelectedForeignFort())
	{
		OutMessage = FString::Printf(TEXT("Este fuerte es de %s. Solo reclutas en los fuertes de tu pais."), *SelectedForceCountryIso);
		return false;
	}
	return Tick->QueueRecruit(SelectedForceId, SelectedForceCountryIso, UnitType, OutMessage);
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
