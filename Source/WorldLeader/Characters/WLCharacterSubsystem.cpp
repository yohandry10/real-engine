// Copyright World Leader project. See ROADMAP.md.

#include "Characters/WLCharacterSubsystem.h"
#include "Campaign/WLDataRegistry.h"
#include "Military/WLMilitarySubsystem.h"
#include "WorldLeader.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr int32 InitialPoliticalCapital = 100;
	constexpr int32 AppointMinisterPoliticalCost = 10;
	constexpr int32 DismissMinisterPoliticalCost = 6;

	TArray<EWLMinisterOffice> AllCabinetOffices()
	{
		return {
			EWLMinisterOffice::Economy,
			EWLMinisterOffice::Defense,
			EWLMinisterOffice::Interior,
			EWLMinisterOffice::Foreign,
			EWLMinisterOffice::Intelligence
		};
	}

	int32 ClampStat(int32 Value)
	{
		return FMath::Clamp(Value, 0, 100);
	}

	const TArray<FString>& GeneratedGeneralSurnames()
	{
		static const TArray<FString> Names = {
			TEXT("Suarez"),
			TEXT("Mendoza"),
			TEXT("Herrera"),
			TEXT("Navarro"),
			TEXT("Rangel"),
			TEXT("Castillo"),
			TEXT("Delgado"),
			TEXT("Torres")
		};
		return Names;
	}
}

void UWLCharacterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UWLDataRegistry::StaticClass());
	ResetCharacterState();
}

UWLDataRegistry* UWLCharacterSubsystem::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLMilitarySubsystem* UWLCharacterSubsystem::GetMilitary() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLMilitarySubsystem>() : nullptr;
}

FString UWLCharacterSubsystem::NormalizeCharacterId(const FString& In)
{
	return In.TrimStartAndEnd().ToUpper();
}

FString UWLCharacterSubsystem::NormalizeIso(const FString& In)
{
	return In.TrimStartAndEnd().ToUpper();
}

FString UWLCharacterSubsystem::NormalizeTraitId(const FString& In)
{
	return In.TrimStartAndEnd().ToLower();
}

EWLCharacterRole UWLCharacterSubsystem::RoleFromString(const FString& In)
{
	const FString S = In.TrimStartAndEnd().ToLower();
	if (S == TEXT("minister"))       return EWLCharacterRole::Minister;
	if (S == TEXT("opposition"))     return EWLCharacterRole::Opposition;
	if (S == TEXT("foreign_leader")
		|| S == TEXT("foreignleader")
		|| S == TEXT("leader"))      return EWLCharacterRole::ForeignLeader;
	if (S == TEXT("spy"))            return EWLCharacterRole::Spy;
	return EWLCharacterRole::General;
}

EWLMilitaryRank UWLCharacterSubsystem::RankFromString(const FString& In)
{
	const FString S = In.TrimStartAndEnd().ToLower();
	if (S == TEXT("brigade_general") || S == TEXT("brigadegeneral"))   return EWLMilitaryRank::BrigadeGeneral;
	if (S == TEXT("division_general") || S == TEXT("divisiongeneral")) return EWLMilitaryRank::DivisionGeneral;
	if (S == TEXT("corps_general") || S == TEXT("corpsgeneral"))       return EWLMilitaryRank::CorpsGeneral;
	if (S == TEXT("field_marshal") || S == TEXT("fieldmarshal"))       return EWLMilitaryRank::FieldMarshal;
	return EWLMilitaryRank::Colonel;
}

EWLMinisterOffice UWLCharacterSubsystem::OfficeFromString(const FString& In)
{
	const FString S = In.TrimStartAndEnd().ToLower();
	if (S == TEXT("economy") || S == TEXT("economia"))           return EWLMinisterOffice::Economy;
	if (S == TEXT("defense") || S == TEXT("defensa"))            return EWLMinisterOffice::Defense;
	if (S == TEXT("interior"))                                   return EWLMinisterOffice::Interior;
	if (S == TEXT("foreign") || S == TEXT("exterior"))           return EWLMinisterOffice::Foreign;
	if (S == TEXT("intelligence") || S == TEXT("inteligencia"))  return EWLMinisterOffice::Intelligence;
	return EWLMinisterOffice::None;
}

FString UWLCharacterSubsystem::MinisterOfficeToString(EWLMinisterOffice Office)
{
	switch (Office)
	{
	case EWLMinisterOffice::Economy:      return TEXT("Economia");
	case EWLMinisterOffice::Defense:      return TEXT("Defensa");
	case EWLMinisterOffice::Interior:     return TEXT("Interior");
	case EWLMinisterOffice::Foreign:      return TEXT("Exterior");
	case EWLMinisterOffice::Intelligence: return TEXT("Inteligencia");
	default:                              return TEXT("Sin cargo");
	}
}

EWLMilitaryRank UWLCharacterSubsystem::NextRank(EWLMilitaryRank Rank) const
{
	switch (Rank)
	{
	case EWLMilitaryRank::Colonel:         return EWLMilitaryRank::BrigadeGeneral;
	case EWLMilitaryRank::BrigadeGeneral: return EWLMilitaryRank::DivisionGeneral;
	case EWLMilitaryRank::DivisionGeneral:return EWLMilitaryRank::CorpsGeneral;
	case EWLMilitaryRank::CorpsGeneral:   return EWLMilitaryRank::FieldMarshal;
	default:                              return EWLMilitaryRank::FieldMarshal;
	}
}

FString UWLCharacterSubsystem::BuildGeneralDisplayName(const FWLCharacter& Character) const
{
	switch (Character.Rank)
	{
	case EWLMilitaryRank::Colonel:         return FString::Printf(TEXT("Coronel %s"), *Character.Name);
	case EWLMilitaryRank::BrigadeGeneral: return FString::Printf(TEXT("General Brig. %s"), *Character.Name);
	case EWLMilitaryRank::DivisionGeneral:return FString::Printf(TEXT("General Div. %s"), *Character.Name);
	case EWLMilitaryRank::CorpsGeneral:   return FString::Printf(TEXT("General Cuerpo %s"), *Character.Name);
	case EWLMilitaryRank::FieldMarshal:   return FString::Printf(TEXT("Mariscal %s"), *Character.Name);
	default:                              return Character.Name;
	}
}

void UWLCharacterSubsystem::ResetCharacterState()
{
	Characters.Reset();
	PoliticalCapitalByNation.Reset();
	const FString FilePath = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Characters") / TEXT("Characters.json");
	LoadCharactersFromFile(FilePath);
	SeedPoliticalCapital();
}

bool UWLCharacterSubsystem::LoadCharactersFromFile(const FString& FilePath)
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLCharacterSubsystem: no se pudo leer %s"), *FilePath);
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		UE_LOG(LogWorldLeader, Error, TEXT("WLCharacterSubsystem: JSON de personajes invalido en %s"), *FilePath);
		return false;
	}

	const UWLDataRegistry* Registry = GetRegistry();
	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FWLCharacter Character;
		Obj->TryGetStringField(TEXT("id"), Character.Id);
		Character.Id = NormalizeCharacterId(Character.Id);
		Obj->TryGetStringField(TEXT("name"), Character.Name);
		Obj->TryGetStringField(TEXT("country_iso"), Character.CountryIso);
		Character.CountryIso = NormalizeIso(Character.CountryIso);

		FString Role;
		Obj->TryGetStringField(TEXT("role"), Role);
		Character.Role = RoleFromString(Role);

		FString Rank;
		Obj->TryGetStringField(TEXT("rank"), Rank);
		Character.Rank = RankFromString(Rank);

		int32 Tmp = 0;
		if (Obj->TryGetNumberField(TEXT("skill"), Tmp))      Character.Skill = ClampStat(Tmp);
		if (Obj->TryGetNumberField(TEXT("loyalty"), Tmp))    Character.Loyalty = ClampStat(Tmp);
		if (Obj->TryGetNumberField(TEXT("ambition"), Tmp))   Character.Ambition = ClampStat(Tmp);
		if (Obj->TryGetNumberField(TEXT("popularity"), Tmp)) Character.Popularity = ClampStat(Tmp);
		if (Obj->TryGetNumberField(TEXT("renown"), Tmp))     Character.Renown = FMath::Max(0, Tmp);

		FString Office;
		Obj->TryGetStringField(TEXT("preferred_office"), Office);
		Character.PreferredOffice = OfficeFromString(Office);
		Obj->TryGetStringField(TEXT("assigned_office"), Office);
		Character.AssignedOffice = OfficeFromString(Office);

		Obj->TryGetStringField(TEXT("assigned_army"), Character.AssignedArmyId);
		Character.AssignedArmyId = NormalizeCharacterId(Character.AssignedArmyId);
		Obj->TryGetBoolField(TEXT("active"), Character.bActive);

		const TArray<TSharedPtr<FJsonValue>>* Traits = nullptr;
		if (Obj->TryGetArrayField(TEXT("traits"), Traits) && Traits)
		{
			for (const TSharedPtr<FJsonValue>& TraitValue : *Traits)
			{
				FString TraitId;
				if (TraitValue.IsValid() && TraitValue->TryGetString(TraitId))
				{
					Character.Traits.Add(NormalizeTraitId(TraitId));
				}
			}
		}

		if (!Character.IsValid())
		{
			continue;
		}
		if (Characters.Contains(Character.Id))
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLCharacterSubsystem: personaje duplicado ignorado: %s"), *Character.Id);
			continue;
		}
		if (Registry)
		{
			FWLNationData Nation;
			if (!Registry->GetNation(Character.CountryIso, Nation))
			{
				UE_LOG(LogWorldLeader, Error, TEXT("WLCharacterSubsystem: personaje %s referencia nacion inexistente %s."),
					*Character.Id, *Character.CountryIso);
				continue;
			}
		}
		if (Character.Role != EWLCharacterRole::Minister)
		{
			Character.AssignedOffice = EWLMinisterOffice::None;
			Character.PreferredOffice = EWLMinisterOffice::None;
		}
		Characters.Add(Character.Id, MoveTemp(Character));
	}

	UE_LOG(LogWorldLeader, Log, TEXT("WLCharacterSubsystem: %d personajes cargados."), Characters.Num());
	return Characters.Num() > 0;
}

void UWLCharacterSubsystem::SeedPoliticalCapital()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		PoliticalCapitalByNation.Add(Nation.Iso, InitialPoliticalCapital);
	}
}

void UWLCharacterSubsystem::EnsureNationPoliticalCapital(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!PoliticalCapitalByNation.Contains(Iso))
	{
		PoliticalCapitalByNation.Add(Iso, InitialPoliticalCapital);
	}
}

bool UWLCharacterSubsystem::ValidateNation(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetRegistry();
	FWLNationData Nation;
	return Registry && Registry->GetNation(NormalizeIso(NationIso), Nation);
}

FWLCharacter* UWLCharacterSubsystem::FindMutableCharacter(const FString& CharacterId)
{
	return Characters.Find(NormalizeCharacterId(CharacterId));
}

const FWLCharacter* UWLCharacterSubsystem::FindCharacter(const FString& CharacterId) const
{
	return Characters.Find(NormalizeCharacterId(CharacterId));
}

TArray<FWLCharacter> UWLCharacterSubsystem::GetAllCharacters() const
{
	TArray<FWLCharacter> Out;
	Characters.GenerateValueArray(Out);
	Out.Sort([](const FWLCharacter& A, const FWLCharacter& B) { return A.Id < B.Id; });
	return Out;
}

bool UWLCharacterSubsystem::GetCharacter(const FString& CharacterId, FWLCharacter& OutCharacter) const
{
	if (const FWLCharacter* Found = FindCharacter(CharacterId))
	{
		OutCharacter = *Found;
		return true;
	}
	return false;
}

TArray<FWLCharacter> UWLCharacterSubsystem::GetCharactersByNation(const FString& NationIso) const
{
	TArray<FWLCharacter> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLCharacter>& Pair : Characters)
	{
		if (Pair.Value.CountryIso == Iso && Pair.Value.bActive)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLCharacter& A, const FWLCharacter& B) { return A.Id < B.Id; });
	return Out;
}

TArray<FWLCharacter> UWLCharacterSubsystem::GetCharactersByRole(const FString& NationIso, EWLCharacterRole Role) const
{
	TArray<FWLCharacter> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLCharacter>& Pair : Characters)
	{
		if (Pair.Value.CountryIso == Iso && Pair.Value.Role == Role && Pair.Value.bActive)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLCharacter& A, const FWLCharacter& B) { return A.Id < B.Id; });
	return Out;
}

TArray<FWLCharacter> UWLCharacterSubsystem::GetGenerals(const FString& NationIso) const
{
	return GetCharactersByRole(NationIso, EWLCharacterRole::General);
}

TArray<FWLCabinetSeat> UWLCharacterSubsystem::GetCabinet(const FString& NationIso) const
{
	TArray<FWLCabinetSeat> Seats;
	const FString Iso = NormalizeIso(NationIso);
	for (EWLMinisterOffice Office : AllCabinetOffices())
	{
		FWLCabinetSeat Seat;
		Seat.Office = Office;
		for (const TPair<FString, FWLCharacter>& Pair : Characters)
		{
			const FWLCharacter& Character = Pair.Value;
			if (Character.CountryIso == Iso
				&& Character.Role == EWLCharacterRole::Minister
				&& Character.AssignedOffice == Office
				&& Character.bActive)
			{
				Seat.CharacterId = Character.Id;
				Seat.Minister = Character;
				break;
			}
		}
		Seats.Add(MoveTemp(Seat));
	}
	return Seats;
}

bool UWLCharacterSubsystem::GetCabinetMinister(
	const FString& NationIso,
	EWLMinisterOffice Office,
	FWLCharacter& OutMinister) const
{
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLCharacter>& Pair : Characters)
	{
		const FWLCharacter& Character = Pair.Value;
		if (Character.CountryIso == Iso
			&& Character.Role == EWLCharacterRole::Minister
			&& Character.AssignedOffice == Office
			&& Character.bActive)
		{
			OutMinister = Character;
			return true;
		}
	}
	return false;
}

int32 UWLCharacterSubsystem::GetPoliticalCapital(const FString& NationIso) const
{
	if (const int32* Found = PoliticalCapitalByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	return InitialPoliticalCapital;
}

int32 UWLCharacterSubsystem::AdjustPoliticalCapital(const FString& NationIso, int32 Delta)
{
	const FString Iso = NormalizeIso(NationIso);
	EnsureNationPoliticalCapital(Iso);
	int32& PoliticalCapital = PoliticalCapitalByNation.FindOrAdd(Iso);
	PoliticalCapital = FMath::Max(0, PoliticalCapital + Delta);
	return PoliticalCapital;
}

FWLGovernmentStats UWLCharacterSubsystem::GetGovernmentStats(const FString& NationIso) const
{
	FWLGovernmentStats Stats;
	Stats.NationIso = NormalizeIso(NationIso);
	Stats.PoliticalCapital = GetPoliticalCapital(Stats.NationIso);

	int32 SkillSum = 0;
	int32 LoyaltySum = 0;
	int32 AmbitionSum = 0;
	int32 PopularitySum = 0;
	int32 CorruptTraitCount = 0;
	int32 Ministers = 0;

	for (const FWLCabinetSeat& Seat : GetCabinet(Stats.NationIso))
	{
		if (Seat.CharacterId.IsEmpty())
		{
			continue;
		}
		++Stats.FilledOffices;
		++Ministers;
		SkillSum += Seat.Minister.Skill;
		LoyaltySum += Seat.Minister.Loyalty;
		AmbitionSum += Seat.Minister.Ambition;
		PopularitySum += Seat.Minister.Popularity;
		if (Seat.Minister.Traits.Contains(TEXT("corrupto")) || Seat.Minister.Traits.Contains(TEXT("clientelista")))
		{
			++CorruptTraitCount;
		}
	}

	if (Ministers > 0)
	{
		Stats.AverageSkill = SkillSum / Ministers;
		Stats.AverageLoyalty = LoyaltySum / Ministers;
		Stats.AverageAmbition = AmbitionSum / Ministers;
		const int32 AveragePopularity = PopularitySum / Ministers;
		Stats.Corruption = FMath::Clamp(100 - Stats.AverageSkill + CorruptTraitCount * 12, 0, 100);
		Stats.Stability = FMath::Clamp(
			40 + Stats.AverageLoyalty / 3 + AveragePopularity / 4 + Stats.PoliticalCapital / 5 - Stats.Corruption / 4,
			0,
			100);
		Stats.CoupRisk = FMath::Clamp(
			Stats.AverageAmbition + (100 - Stats.AverageLoyalty) / 2 + (100 - Stats.Stability) / 3 - 35,
			0,
			100);
	}
	return Stats;
}

void UWLCharacterSubsystem::UnassignOffice(const FString& NationIso, EWLMinisterOffice Office)
{
	const FString Iso = NormalizeIso(NationIso);
	for (TPair<FString, FWLCharacter>& Pair : Characters)
	{
		FWLCharacter& Character = Pair.Value;
		if (Character.CountryIso == Iso && Character.AssignedOffice == Office)
		{
			Character.AssignedOffice = EWLMinisterOffice::None;
		}
	}
}

bool UWLCharacterSubsystem::AppointMinister(
	const FString& NationIso,
	EWLMinisterOffice Office,
	const FString& CharacterId,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (Office == EWLMinisterOffice::None)
	{
		OutMessage = TEXT("Cargo de gabinete invalido.");
		return false;
	}
	if (!ValidateNation(Iso))
	{
		OutMessage = FString::Printf(TEXT("Nacion no disponible: %s"), *NationIso);
		return false;
	}

	FWLCharacter* Character = FindMutableCharacter(CharacterId);
	if (!Character)
	{
		OutMessage = FString::Printf(TEXT("Personaje desconocido: %s"), *CharacterId);
		return false;
	}
	if (!Character->bActive || Character->CountryIso != Iso || Character->Role != EWLCharacterRole::Minister)
	{
		OutMessage = FString::Printf(TEXT("%s no puede ocupar un ministerio de %s."), *Character->Name, *Iso);
		return false;
	}

	EnsureNationPoliticalCapital(Iso);
	int32& PoliticalCapital = PoliticalCapitalByNation.FindOrAdd(Iso);
	if (PoliticalCapital < AppointMinisterPoliticalCost)
	{
		OutMessage = TEXT("Capital politico insuficiente para nombrar ministro.");
		return false;
	}

	UnassignOffice(Iso, Office);
	for (TPair<FString, FWLCharacter>& Pair : Characters)
	{
		if (Pair.Value.CountryIso == Iso && Pair.Value.AssignedOffice != EWLMinisterOffice::None
			&& Pair.Value.Id == Character->Id)
		{
			Pair.Value.AssignedOffice = EWLMinisterOffice::None;
		}
	}
	Character = FindMutableCharacter(CharacterId);
	if (!Character)
	{
		OutMessage = TEXT("No se pudo revalidar el ministro.");
		return false;
	}
	Character->AssignedOffice = Office;
	PoliticalCapital = FMath::Max(0, PoliticalCapital - AppointMinisterPoliticalCost);
	OutMessage = FString::Printf(TEXT("%s nombrado en %s. Capital politico: %d."),
		*Character->Name, *MinisterOfficeToString(Office), PoliticalCapital);
	return true;
}

bool UWLCharacterSubsystem::DismissMinister(const FString& NationIso, EWLMinisterOffice Office, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (Office == EWLMinisterOffice::None)
	{
		OutMessage = TEXT("Cargo de gabinete invalido.");
		return false;
	}

	FWLCharacter Current;
	if (!GetCabinetMinister(Iso, Office, Current))
	{
		OutMessage = FString::Printf(TEXT("No hay ministro en %s."), *MinisterOfficeToString(Office));
		return false;
	}

	EnsureNationPoliticalCapital(Iso);
	int32& PoliticalCapital = PoliticalCapitalByNation.FindOrAdd(Iso);
	if (PoliticalCapital < DismissMinisterPoliticalCost)
	{
		OutMessage = TEXT("Capital politico insuficiente para remover ministro.");
		return false;
	}

	UnassignOffice(Iso, Office);
	PoliticalCapital = FMath::Max(0, PoliticalCapital - DismissMinisterPoliticalCost);
	OutMessage = FString::Printf(TEXT("%s removido de %s. Capital politico: %d."),
		*Current.Name, *MinisterOfficeToString(Office), PoliticalCapital);
	return true;
}

bool UWLCharacterSubsystem::AssignGeneralToArmy(
	const FString& CharacterId,
	const FString& ArmyId,
	FString& OutMessage)
{
	FWLCharacter* Character = FindMutableCharacter(CharacterId);
	if (!Character)
	{
		OutMessage = FString::Printf(TEXT("Personaje desconocido: %s"), *CharacterId);
		return false;
	}
	if (!Character->bActive || Character->Role != EWLCharacterRole::General)
	{
		OutMessage = FString::Printf(TEXT("%s no es un general activo."), *Character->Name);
		return false;
	}

	const FString NormalizedArmyId = NormalizeCharacterId(ArmyId);
	if (const UWLMilitarySubsystem* Military = GetMilitary())
	{
		FWLArmy Army;
		if (!Military->GetArmy(NormalizedArmyId, Army))
		{
			OutMessage = FString::Printf(TEXT("Ejercito desconocido: %s"), *ArmyId);
			return false;
		}
		if (Army.OwnerIso != Character->CountryIso)
		{
			OutMessage = FString::Printf(TEXT("%s no puede comandar un ejercito de %s."),
				*Character->Name, *Army.OwnerIso);
			return false;
		}
	}

	for (TPair<FString, FWLCharacter>& Pair : Characters)
	{
		if (Pair.Value.AssignedArmyId == NormalizedArmyId)
		{
			Pair.Value.AssignedArmyId.Reset();
		}
		if (Pair.Value.Id == Character->Id)
		{
			Pair.Value.AssignedArmyId.Reset();
		}
	}

	Character = FindMutableCharacter(CharacterId);
	if (!Character)
	{
		OutMessage = TEXT("No se pudo revalidar el general.");
		return false;
	}
	Character->AssignedArmyId = NormalizedArmyId;
	if (UWLMilitarySubsystem* Military = GetMilitary())
	{
		FString ArmyMessage;
		Military->SetArmyGeneral(NormalizedArmyId, BuildGeneralDisplayName(*Character), ArmyMessage);
	}
	OutMessage = FString::Printf(TEXT("%s asignado al ejercito %s."), *Character->Name, *NormalizedArmyId);
	return true;
}

bool UWLCharacterSubsystem::CreateGeneral(
	const FString& NationIso,
	FWLCharacter& OutGeneral,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = FString::Printf(TEXT("Nacion no disponible: %s"), *NationIso);
		return false;
	}

	int32 ExistingGenerals = 0;
	for (const TPair<FString, FWLCharacter>& Pair : Characters)
	{
		if (Pair.Value.CountryIso == Iso && Pair.Value.Role == EWLCharacterRole::General)
		{
			++ExistingGenerals;
		}
	}

	FWLCharacter General;
	General.Id = FString::Printf(TEXT("%s-GEN-AUTO-%02d"), *Iso, ExistingGenerals + 1);
	while (Characters.Contains(General.Id))
	{
		++ExistingGenerals;
		General.Id = FString::Printf(TEXT("%s-GEN-AUTO-%02d"), *Iso, ExistingGenerals + 1);
	}

	const TArray<FString>& Surnames = GeneratedGeneralSurnames();
	General.Name = Surnames.Num() > 0
		? Surnames[ExistingGenerals % Surnames.Num()]
		: FString::Printf(TEXT("General %02d"), ExistingGenerals + 1);
	General.CountryIso = Iso;
	General.Role = EWLCharacterRole::General;
	General.Rank = EWLMilitaryRank::Colonel;
	General.Skill = ClampStat(58 + (ExistingGenerals * 7) % 25);
	General.Loyalty = ClampStat(55 + (ExistingGenerals * 5) % 25);
	General.Ambition = ClampStat(35 + (ExistingGenerals * 9) % 35);
	General.Popularity = ClampStat(35 + (ExistingGenerals * 6) % 25);
	General.Renown = 0;
	General.Traits = { TEXT("emergente") };
	General.bActive = true;

	OutGeneral = General;
	Characters.Add(General.Id, MoveTemp(General));
	OutMessage = FString::Printf(TEXT("General creado: %s (%s)."), *OutGeneral.Name, *OutGeneral.Id);
	return true;
}

bool UWLCharacterSubsystem::CreateAndAssignGeneralToArmy(
	const FString& NationIso,
	const FString& ArmyId,
	FWLCharacter& OutGeneral,
	FString& OutMessage)
{
	FWLArmy Army;
	if (const UWLMilitarySubsystem* Military = GetMilitary())
	{
		if (!Military->GetArmy(ArmyId, Army))
		{
			OutMessage = FString::Printf(TEXT("Ejercito desconocido: %s"), *ArmyId);
			return false;
		}
		if (Army.OwnerIso != NormalizeIso(NationIso))
		{
			OutMessage = FString::Printf(TEXT("El ejercito %s pertenece a %s, no a %s."),
				*Army.Id, *Army.OwnerIso, *NationIso);
			return false;
		}
	}

	if (!CreateGeneral(NationIso, OutGeneral, OutMessage))
	{
		return false;
	}
	FString AssignMessage;
	if (!AssignGeneralToArmy(OutGeneral.Id, ArmyId, AssignMessage))
	{
		return false;
	}
	OutMessage = FString::Printf(TEXT("%s %s"), *OutMessage, *AssignMessage);
	return true;
}

bool UWLCharacterSubsystem::PromoteGeneral(const FString& CharacterId, FString& OutMessage)
{
	FWLCharacter* Character = FindMutableCharacter(CharacterId);
	if (!Character || Character->Role != EWLCharacterRole::General || !Character->bActive)
	{
		OutMessage = FString::Printf(TEXT("General no disponible: %s"), *CharacterId);
		return false;
	}
	const EWLMilitaryRank PreviousRank = Character->Rank;
	Character->Rank = NextRank(Character->Rank);
	if (Character->Rank == PreviousRank)
	{
		OutMessage = FString::Printf(TEXT("%s ya tiene el rango maximo."), *Character->Name);
		return false;
	}
	Character->Loyalty = ClampStat(Character->Loyalty + 6);
	Character->Ambition = ClampStat(Character->Ambition + 4);
	if (!Character->AssignedArmyId.IsEmpty())
	{
		if (UWLMilitarySubsystem* Military = GetMilitary())
		{
			FString ArmyMessage;
			Military->SetArmyGeneral(Character->AssignedArmyId, BuildGeneralDisplayName(*Character), ArmyMessage);
		}
	}
	OutMessage = FString::Printf(TEXT("%s ascendido. Lealtad %d, ambicion %d."),
		*Character->Name, Character->Loyalty, Character->Ambition);
	return true;
}

bool UWLCharacterSubsystem::RetireCharacter(const FString& CharacterId, FString& OutMessage)
{
	FWLCharacter* Character = FindMutableCharacter(CharacterId);
	if (!Character || !Character->bActive)
	{
		OutMessage = FString::Printf(TEXT("Personaje no disponible: %s"), *CharacterId);
		return false;
	}
	const FString ArmyId = Character->AssignedArmyId;
	Character->bActive = false;
	Character->AssignedOffice = EWLMinisterOffice::None;
	Character->AssignedArmyId.Reset();
	if (!ArmyId.IsEmpty())
	{
		if (UWLMilitarySubsystem* Military = GetMilitary())
		{
			FString ArmyMessage;
			Military->SetArmyGeneral(ArmyId, TEXT("Comandante"), ArmyMessage);
		}
	}
	OutMessage = FString::Printf(TEXT("%s retirado del servicio activo."), *Character->Name);
	return true;
}

bool UWLCharacterSubsystem::AddRenownToGeneral(
	const FString& CharacterId,
	int32 RenownDelta,
	FString& OutMessage)
{
	FWLCharacter* Character = FindMutableCharacter(CharacterId);
	if (!Character || Character->Role != EWLCharacterRole::General || !Character->bActive)
	{
		OutMessage = FString::Printf(TEXT("General no disponible: %s"), *CharacterId);
		return false;
	}
	const int32 PreviousRenown = Character->Renown;
	Character->Renown = FMath::Max(0, Character->Renown + RenownDelta);
	if (Character->Renown / 25 > PreviousRenown / 25)
	{
		Character->Skill = ClampStat(Character->Skill + 1);
		if (!Character->Traits.Contains(TEXT("veterano")))
		{
			Character->Traits.Add(TEXT("veterano"));
		}
	}
	OutMessage = FString::Printf(TEXT("%s renombre %d -> %d."),
		*Character->Name, PreviousRenown, Character->Renown);
	return true;
}

int32 UWLCharacterSubsystem::AddMonthlyRenownToGenerals(const FString& NationIso, int32 RenownDelta)
{
	const FString Iso = NormalizeIso(NationIso);
	int32 Updated = 0;
	for (TPair<FString, FWLCharacter>& Pair : Characters)
	{
		FWLCharacter& Character = Pair.Value;
		if (Character.CountryIso == Iso && Character.Role == EWLCharacterRole::General && Character.bActive)
		{
			const int32 PreviousRenown = Character.Renown;
			Character.Renown = FMath::Max(0, Character.Renown + RenownDelta);
			if (Character.Renown / 25 > PreviousRenown / 25)
			{
				Character.Skill = ClampStat(Character.Skill + 1);
				if (!Character.Traits.Contains(TEXT("veterano")))
				{
					Character.Traits.Add(TEXT("veterano"));
				}
			}
			++Updated;
		}
	}
	return Updated;
}

bool UWLCharacterSubsystem::AdjustCharacterLoyalty(
	const FString& CharacterId,
	int32 LoyaltyDelta,
	FString& OutMessage)
{
	FWLCharacter* Character = FindMutableCharacter(CharacterId);
	if (!Character || !Character->bActive)
	{
		OutMessage = FString::Printf(TEXT("Personaje no disponible: %s"), *CharacterId);
		return false;
	}
	const int32 Previous = Character->Loyalty;
	Character->Loyalty = ClampStat(Character->Loyalty + LoyaltyDelta);
	OutMessage = FString::Printf(TEXT("%s lealtad %d -> %d."), *Character->Name, Previous, Character->Loyalty);
	return true;
}

bool UWLCharacterSubsystem::GetAssignedGeneralForArmy(const FString& ArmyId, FWLCharacter& OutGeneral) const
{
	const FString NormalizedArmyId = NormalizeCharacterId(ArmyId);
	for (const TPair<FString, FWLCharacter>& Pair : Characters)
	{
		if (Pair.Value.Role == EWLCharacterRole::General
			&& Pair.Value.AssignedArmyId == NormalizedArmyId
			&& Pair.Value.bActive)
		{
			OutGeneral = Pair.Value;
			return true;
		}
	}
	return false;
}

void UWLCharacterSubsystem::WriteSaveSnapshot(
	TArray<FWLCharacter>& OutCharacters,
	TArray<FWLPoliticalCapitalSave>& OutPoliticalCapital) const
{
	OutCharacters = GetAllCharacters();
	OutPoliticalCapital.Reset();
	for (const TPair<FString, int32>& Pair : PoliticalCapitalByNation)
	{
		FWLPoliticalCapitalSave Saved;
		Saved.NationIso = Pair.Key;
		Saved.PoliticalCapital = Pair.Value;
		OutPoliticalCapital.Add(MoveTemp(Saved));
	}
	OutPoliticalCapital.Sort([](const FWLPoliticalCapitalSave& A, const FWLPoliticalCapitalSave& B)
	{
		return A.NationIso < B.NationIso;
	});
}

bool UWLCharacterSubsystem::RestoreSaveSnapshot(
	const TArray<FWLCharacter>& SavedCharacters,
	const TArray<FWLPoliticalCapitalSave>& SavedPoliticalCapital,
	FString& OutMessage)
{
	ResetCharacterState();
	if (SavedCharacters.Num() > 0)
	{
		Characters.Reset();
		for (FWLCharacter Character : SavedCharacters)
		{
			Character.Id = NormalizeCharacterId(Character.Id);
			Character.CountryIso = NormalizeIso(Character.CountryIso);
			Character.Skill = ClampStat(Character.Skill);
			Character.Loyalty = ClampStat(Character.Loyalty);
			Character.Ambition = ClampStat(Character.Ambition);
			Character.Popularity = ClampStat(Character.Popularity);
			Character.Renown = FMath::Max(0, Character.Renown);
			if (Character.Role != EWLCharacterRole::Minister)
			{
				Character.AssignedOffice = EWLMinisterOffice::None;
				Character.PreferredOffice = EWLMinisterOffice::None;
			}
			if (Character.IsValid() && ValidateNation(Character.CountryIso) && !Characters.Contains(Character.Id))
			{
				Characters.Add(Character.Id, MoveTemp(Character));
			}
		}
	}

	PoliticalCapitalByNation.Reset();
	SeedPoliticalCapital();
	for (const FWLPoliticalCapitalSave& Saved : SavedPoliticalCapital)
	{
		const FString Iso = NormalizeIso(Saved.NationIso);
		if (ValidateNation(Iso))
		{
			PoliticalCapitalByNation.Add(Iso, ClampStat(Saved.PoliticalCapital));
		}
	}

	OutMessage = FString::Printf(TEXT("%d personajes restaurados."), Characters.Num());
	return true;
}
