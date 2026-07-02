// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLCharacterTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WLCharacterSubsystem.generated.h"

class UWLDataRegistry;
class UWLMilitarySubsystem;

USTRUCT(BlueprintType)
struct FWLCabinetSeat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLMinisterOffice Office = EWLMinisterOffice::None;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString CharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FWLCharacter Minister;
};

USTRUCT(BlueprintType)
struct FWLGovernmentStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PoliticalCapital = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 FilledOffices = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 AverageSkill = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 AverageLoyalty = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 AverageAmbition = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Stability = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Corruption = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 CoupRisk = 0;
};

/**
 * F1.2: backend de personajes/gobierno.
 *
 * Gestiona el roster mutable de personajes por nacion: gabinete, generales,
 * oposicion, lideres y espias. La UI de gobierno debe consumir este subsystem,
 * no inventar placeholders.
 */
UCLASS()
class WORLDLEADER_API UWLCharacterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	void ResetCharacterState();

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLCharacter> GetAllCharacters() const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	bool GetCharacter(const FString& CharacterId, FWLCharacter& OutCharacter) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLCharacter> GetCharactersByNation(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLCharacter> GetCharactersByRole(const FString& NationIso, EWLCharacterRole Role) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLCharacter> GetGenerals(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLCabinetSeat> GetCabinet(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	bool GetCabinetMinister(const FString& NationIso, EWLMinisterOffice Office, FWLCharacter& OutMinister) const;

	/**
	 * Fase 3 auditoria: factor de efecto del ministro en el cargo, (skill-50)/50 en -1..+1.
	 * Cargo vacante = 0 (neutro). Un ministro con skill < 50 tiene factor NEGATIVO (perjudica).
	 */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	double GetMinisterEffectFactor(const FString& NationIso, EWLMinisterOffice Office) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLGovernmentStats GetGovernmentStats(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	int32 GetPoliticalCapital(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	int32 AdjustPoliticalCapital(const FString& NationIso, int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool AppointMinister(const FString& NationIso, EWLMinisterOffice Office, const FString& CharacterId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool DismissMinister(const FString& NationIso, EWLMinisterOffice Office, FString& OutMessage);

	/** Backend de contratacion: crea un candidato civil para una cartera, sin nombrarlo todavia. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool CreateMinister(const FString& NationIso, EWLMinisterOffice PreferredOffice, FWLCharacter& OutMinister, FString& OutMessage);

	/** Backend de contratacion: crea un candidato y lo nombra en la cartera indicada si hay capital politico. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool HireMinister(const FString& NationIso, EWLMinisterOffice Office, FWLCharacter& OutMinister, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool AssignGeneralToArmy(const FString& CharacterId, const FString& ArmyId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool CreateGeneral(const FString& NationIso, FWLCharacter& OutGeneral, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool CreateAndAssignGeneralToArmy(const FString& NationIso, const FString& ArmyId, FWLCharacter& OutGeneral, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool PromoteGeneral(const FString& CharacterId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool RetireCharacter(const FString& CharacterId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool AddRenownToGeneral(const FString& CharacterId, int32 RenownDelta, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	int32 AddMonthlyRenownToGenerals(const FString& NationIso, int32 RenownDelta);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool AdjustCharacterLoyalty(const FString& CharacterId, int32 LoyaltyDelta, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	bool GetAssignedGeneralForArmy(const FString& ArmyId, FWLCharacter& OutGeneral) const;

	void WriteSaveSnapshot(
		TArray<FWLCharacter>& OutCharacters,
		TArray<FWLPoliticalCapitalSave>& OutPoliticalCapital) const;

	bool RestoreSaveSnapshot(
		const TArray<FWLCharacter>& SavedCharacters,
		const TArray<FWLPoliticalCapitalSave>& SavedPoliticalCapital,
		FString& OutMessage);

	static FString MinisterOfficeToString(EWLMinisterOffice Office);

private:
	UPROPERTY()
	TMap<FString, FWLCharacter> Characters;

	UPROPERTY()
	TMap<FString, int32> PoliticalCapitalByNation;

	bool LoadCharactersFromFile(const FString& FilePath);
	void SeedGeneratedCharactersForAllNations();
	void SeedPoliticalCapital();
	void EnsureNationPoliticalCapital(const FString& NationIso);

	static FString NormalizeCharacterId(const FString& In);
	static FString NormalizeIso(const FString& In);
	static FString NormalizeTraitId(const FString& In);
	static EWLCharacterRole RoleFromString(const FString& In);
	static EWLMilitaryRank RankFromString(const FString& In);
	static EWLMinisterOffice OfficeFromString(const FString& In);

	UWLDataRegistry* GetRegistry() const;
	UWLMilitarySubsystem* GetMilitary() const;
	FWLCharacter* FindMutableCharacter(const FString& CharacterId);
	const FWLCharacter* FindCharacter(const FString& CharacterId) const;
	bool ValidateNation(const FString& NationIso) const;
	void UnassignOffice(const FString& NationIso, EWLMinisterOffice Office);
	FString BuildGeneralDisplayName(const FWLCharacter& Character) const;
	EWLMilitaryRank NextRank(EWLMilitaryRank Rank) const;
};
