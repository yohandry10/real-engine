// Copyright World Leader project. See ROADMAP.md.
//
// Reclutamiento de tropas por turnos (estilo Total War): una base encola unidades; cada turno
// (AdvanceMonth) avanza la primera orden de la cola; al terminar, el lote entra a la guarnicion de
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
		OutMessage = TEXT("Nacion sin tesoro.");
		return false;
	}
	if (*Treasury < Option->Cost)
	{
		OutMessage = FString::Printf(TEXT("Fondos insuficientes para %s (%lld de %lld)."), *Option->Label, static_cast<long long>(*Treasury), static_cast<long long>(Option->Cost));
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
