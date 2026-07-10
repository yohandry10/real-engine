// Copyright World Leader project. See ROADMAP.md.
//
// Reclutamiento de tropas por turnos (estilo Total War): una base encola unidades; cada turno
// (AdvanceDay) avanza la primera orden de la cola; al terminar, el lote entra a la guarnicion de
// esa base. La UI (panel de la base) llama QueueRecruit y lee GetRecruitQueue / GetGarrisonRecruited.

#include "Campaign/WLStrategicTickSubsystem.h"

#include "WorldLeader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UWLStrategicTickSubsystem::EnsureRecruitCatalog() const
{
	if (bRecruitCatalogLoaded)
	{
		return;
	}
	bRecruitCatalogLoaded = true;
	RecruitCatalog.Reset();

	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign3D") / TEXT("RecruitableUnits.json");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("RecruitableUnits.json no encontrado: %s"), *Path);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if (!Root->TryGetArrayField(TEXT("units"), Units) || !Units)
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Units)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Obj) || !Obj || !(*Obj).IsValid())
		{
			continue;
		}
		FWLRecruitOption Option;
		(*Obj)->TryGetStringField(TEXT("unit"), Option.UnitType);
		Option.UnitType = Option.UnitType.ToLower();
		(*Obj)->TryGetStringField(TEXT("label"), Option.Label);
		(*Obj)->TryGetStringField(TEXT("category"), Option.Category);
		double Batch = 1.0, Cost = 0.0, Turns = 1.0;
		(*Obj)->TryGetNumberField(TEXT("batch"), Batch);
		(*Obj)->TryGetNumberField(TEXT("cost"), Cost);
		(*Obj)->TryGetNumberField(TEXT("turns"), Turns);
		if (Option.UnitType.IsEmpty())
		{
			continue;
		}
		if (Option.Label.IsEmpty()) { Option.Label = Option.UnitType; }
		if (Option.Category.IsEmpty()) { Option.Category = TEXT("land"); }
		Option.Batch = FMath::Max(1, FMath::RoundToInt(Batch));
		Option.Cost = static_cast<int64>(Cost);
		Option.Turns = FMath::Max(1, FMath::RoundToInt(Turns));
		RecruitCatalog.Add(Option);
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Catalogo de reclutamiento cargado: %d unidades."), RecruitCatalog.Num());
}

void UWLStrategicTickSubsystem::EnsureMilitaryCatalog() const
{
	// FE1.1: efectivos DESPLEGADOS por nacion (MilitaryForces.json), para el upkeep militar mensual. Cache
	// perezoso (una sola lectura). Las guarniciones reclutadas se suman aparte en GetNationMilitaryStrength.
	if (bMilitaryCatalogLoaded)
	{
		return;
	}
	bMilitaryCatalogLoaded = true;
	PreplacedMilitaryStrength.Reset();

	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign3D") / TEXT("MilitaryForces.json");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Forces = nullptr;
	if (!Root->TryGetArrayField(TEXT("forces"), Forces) || !Forces)
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Forces)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Obj) || !Obj || !(*Obj).IsValid())
		{
			continue;
		}
		FString Iso;
		(*Obj)->TryGetStringField(TEXT("country_iso"), Iso);
		Iso = Iso.TrimStartAndEnd().ToUpper();
		double Strength = 0.0;
		(*Obj)->TryGetNumberField(TEXT("strength"), Strength);
		if (Iso.IsEmpty() || Strength <= 0.0)
		{
			continue;
		}
		PreplacedMilitaryStrength.FindOrAdd(Iso) += static_cast<int64>(Strength);
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Catalogo militar cargado: %d naciones con fuerzas desplegadas."), PreplacedMilitaryStrength.Num());
}

const FWLRecruitOption* UWLStrategicTickSubsystem::FindRecruitOption(const FString& UnitType) const
{
	EnsureRecruitCatalog();
	const FString Wanted = UnitType.ToLower();
	for (const FWLRecruitOption& Option : RecruitCatalog)
	{
		if (Option.UnitType == Wanted)
		{
			return &Option;
		}
	}
	return nullptr;
}

TArray<FWLRecruitOption> UWLStrategicTickSubsystem::GetRecruitOptions() const
{
	EnsureRecruitCatalog();
	return RecruitCatalog;
}

bool UWLStrategicTickSubsystem::QueueRecruit(const FString& BaseId, const FString& NationIso, const FString& UnitType, FString& OutMessage)
{
	if (BaseId.IsEmpty())
	{
		OutMessage = TEXT("Base invalida.");
		return false;
	}
	const FWLRecruitOption* Option = FindRecruitOption(UnitType);
	if (!Option)
	{
		OutMessage = FString::Printf(TEXT("Unidad no reclutable: %s"), *UnitType);
		return false;
	}
	const FString Iso = NationIso.TrimStartAndEnd().ToUpper();
	int64* Treasury = Treasuries.Find(Iso);
	if (!Treasury)
	{
		// TODOS los fuertes deben poder reclutar. En los datos (Nations.json) solo existen CO y VE, asi que los
		// fuertes de los demas paises (EC/PE/CL/AR/BO/UY/PY/BR — el pais sale del id del fuerte) no tenian
		// tesoro y fallaban con "Nacion sin tesoro". Les sembramos un tesoro por defecto la primera vez para
		// que el edificio sea funcional (cada fuerte recluta para SU pais).
		Treasury = &Treasuries.Add(Iso, 50000);
		UE_LOG(LogWorldLeader, Log, TEXT("Tesoro por defecto sembrado para %s (fuerte sin nacion en datos)."), *Iso);
	}
	// FE1.4: se puede reclutar en deficit (deuda con interes) hasta el limite de credito de la nacion.
	if (*Treasury - Option->Cost < -GetCreditLimit(Iso))
	{
		OutMessage = FString::Printf(TEXT("Credito agotado para %s (tesoro %lld, cuesta %lld, limite %lld)."),
			*Option->Label, static_cast<long long>(*Treasury), static_cast<long long>(Option->Cost),
			static_cast<long long>(GetCreditLimit(Iso)));
		return false;
	}

	*Treasury -= Option->Cost;
	FWLRecruitOrder Order;
	Order.UnitType = Option->UnitType;
	Order.Label = Option->Label;
	Order.Batch = Option->Batch;
	Order.TurnsRemaining = Option->Turns;
	Order.TurnsTotal = Option->Turns;
	RecruitQueues.FindOrAdd(BaseId).Add(Order);

	OutMessage = FString::Printf(TEXT("Reclutando %s (+%d) en %d turno(s)."), *Option->Label, Option->Batch, Option->Turns);
	return true;
}

TArray<FWLRecruitOrder> UWLStrategicTickSubsystem::GetRecruitQueue(const FString& BaseId) const
{
	if (const TArray<FWLRecruitOrder>* Queue = RecruitQueues.Find(BaseId))
	{
		return *Queue;
	}
	return TArray<FWLRecruitOrder>();
}

TArray<FWLGarrisonGroup> UWLStrategicTickSubsystem::GetGarrisonRecruited(const FString& BaseId) const
{
	TArray<FWLGarrisonGroup> Out;
	const TMap<FString, int32>* Garrison = GarrisonRecruited.Find(BaseId);
	if (!Garrison)
	{
		return Out;
	}
	for (const TPair<FString, int32>& Pair : *Garrison)
	{
		if (Pair.Value <= 0)
		{
			continue;
		}
		FWLGarrisonGroup Group;
		Group.UnitType = Pair.Key;
		const FWLRecruitOption* Option = FindRecruitOption(Pair.Key);
		Group.Label = Option ? Option->Label : Pair.Key;
		Group.Count = Pair.Value;
		Out.Add(Group);
	}
	return Out;
}

int32 UWLStrategicTickSubsystem::ConsumeGarrisonUnits(const FString& BaseId, const FString& UnitType, int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}
	TMap<FString, int32>* Garrison = GarrisonRecruited.Find(BaseId);
	if (!Garrison)
	{
		// Las claves llegan del despliegue en mayusculas normalizadas; intenta esa variante.
		Garrison = GarrisonRecruited.Find(BaseId.TrimStartAndEnd().ToUpper());
	}
	if (!Garrison)
	{
		return 0;
	}
	int32* Stock = Garrison->Find(UnitType);
	if (!Stock)
	{
		// El mapeo tactico es 1:1, pero los ids de Units.json van en minusculas.
		Stock = Garrison->Find(UnitType.ToLower());
	}
	if (!Stock || *Stock <= 0)
	{
		return 0;
	}
	const int32 Consumed = FMath::Min(*Stock, Count);
	*Stock -= Consumed;
	return Consumed;
}

void UWLStrategicTickSubsystem::WriteRecruitmentSnapshot(
	TArray<FWLGarrisonUnitSave>& OutGarrison,
	TArray<FWLRecruitOrderSave>& OutOrders) const
{
	OutGarrison.Reset();
	OutOrders.Reset();
	for (const TPair<FString, TMap<FString, int32>>& Base : GarrisonRecruited)
	{
		for (const TPair<FString, int32>& Unit : Base.Value)
		{
			if (Unit.Value <= 0)
			{
				continue;
			}
			FWLGarrisonUnitSave Row;
			Row.BaseId = Base.Key;
			Row.UnitType = Unit.Key;
			Row.Count = Unit.Value;
			OutGarrison.Add(MoveTemp(Row));
		}
	}
	for (const TPair<FString, TArray<FWLRecruitOrder>>& Queue : RecruitQueues)
	{
		for (const FWLRecruitOrder& Order : Queue.Value)
		{
			FWLRecruitOrderSave Row;
			Row.BaseId = Queue.Key;
			Row.UnitType = Order.UnitType;
			Row.Batch = Order.Batch;
			Row.TurnsRemaining = Order.TurnsRemaining;
			Row.TurnsTotal = Order.TurnsTotal;
			OutOrders.Add(MoveTemp(Row));
		}
	}
}

void UWLStrategicTickSubsystem::RestoreRecruitmentSnapshot(
	const TArray<FWLGarrisonUnitSave>& SavedGarrison,
	const TArray<FWLRecruitOrderSave>& SavedOrders)
{
	GarrisonRecruited.Reset();
	RecruitQueues.Reset();
	for (const FWLGarrisonUnitSave& Row : SavedGarrison)
	{
		if (Row.BaseId.IsEmpty() || Row.UnitType.IsEmpty() || Row.Count <= 0)
		{
			continue;
		}
		GarrisonRecruited.FindOrAdd(Row.BaseId).FindOrAdd(Row.UnitType) += Row.Count;
	}
	for (const FWLRecruitOrderSave& Row : SavedOrders)
	{
		if (Row.BaseId.IsEmpty() || Row.UnitType.IsEmpty() || Row.Batch <= 0)
		{
			continue;
		}
		FWLRecruitOrder Order;
		Order.UnitType = Row.UnitType;
		const FWLRecruitOption* Option = FindRecruitOption(Row.UnitType);
		Order.Label = Option ? Option->Label : Row.UnitType;
		Order.Batch = Row.Batch;
		Order.TurnsTotal = FMath::Max(1, Row.TurnsTotal);
		Order.TurnsRemaining = FMath::Clamp(Row.TurnsRemaining, 0, Order.TurnsTotal);
		RecruitQueues.FindOrAdd(Row.BaseId).Add(MoveTemp(Order));
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Reclutamiento restaurado: %d bases con guarnicion, %d colas."),
		GarrisonRecruited.Num(), RecruitQueues.Num());
}

void UWLStrategicTickSubsystem::AdvanceRecruitment()
{
	// Construccion SECUENCIAL (como Total War): solo avanza la PRIMERA orden de cada cola por turno.
	for (TPair<FString, TArray<FWLRecruitOrder>>& Pair : RecruitQueues)
	{
		TArray<FWLRecruitOrder>& Queue = Pair.Value;
		if (Queue.Num() == 0)
		{
			continue;
		}
		FWLRecruitOrder& Front = Queue[0];
		Front.TurnsRemaining = FMath::Max(0, Front.TurnsRemaining - 1);
		if (Front.TurnsRemaining <= 0)
		{
			GarrisonRecruited.FindOrAdd(Pair.Key).FindOrAdd(Front.UnitType) += Front.Batch;
			UE_LOG(LogWorldLeader, Log, TEXT("Reclutamiento completado en %s: +%d %s"), *Pair.Key, Front.Batch, *Front.Label);
			Queue.RemoveAt(0);
		}
	}
}
