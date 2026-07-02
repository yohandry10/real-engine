// Copyright World Leader project. See ROADMAP.md.
//
// Ventana de GOBIERNO / PRESIDENCIA construida en UMG/C++ (mismo patron que
// WLMainMenuWidget: sin assets binarios, fuentes Slate reales, UBorder con
// padding). Es el equivalente moderno al panel de faccion de Total War,
// adaptado al roadmap: RESUMEN · ECONOMIA · ALTO MANDO · POLITICA ·
// DIPLOMACIA · REGISTROS. Toda la logica vive en los subsystems
// (UWLStrategicTickSubsystem, UWLCharacterSubsystem, UWLPoliticalSubsystem);
// este widget solo lee endpoints y dispara acciones.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "WLGovernmentWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;
class UScrollBox;
class UWidget;
class UWLDataRegistry;
class UWLStrategicTickSubsystem;
class UWLCampaignGameInstance;
class UWLCharacterSubsystem;
class UWLPoliticalSubsystem;
class UWLGovernmentWidget;

/**
 * Boton con payload: OnClicked dinamico no identifica al emisor, asi que cada
 * boton de accion (ascender general X, declarar guerra a Y...) guarda su
 * ActionId y lo reenvia al dispatcher central del widget (HandleAction).
 */
UCLASS()
class WORLDLEADER_API UWLGovActionButton : public UButton
{
	GENERATED_BODY()

public:
	void BindAction(UWLGovernmentWidget* InOwner, const FString& InActionId);

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY() TObjectPtr<UWLGovernmentWidget> Owner = nullptr;
	FString ActionId;
};

/** Pestanas de la ventana de Gobierno (cambian el contenido central). */
UENUM()
enum class EWLGovernmentTab : uint8
{
	Overview,     // RESUMEN    -> metricas + territorio + outcome
	Economy,      // ECONOMIA   -> presupuesto, mercado, comercio, finanzas, gobernanza, impuestos
	HighCommand,  // ALTO MANDO -> gabinete + generales (F1.6/F1.7)
	Politics,     // POLITICA   -> orden publico, golpe/oposicion (F2) + eventos (F5)
	Diplomacy,    // DIPLOMACIA -> relaciones, tratados, guerra (F3) + intriga (F4)
	Records       // REGISTROS  -> IA economica / reportes
};

UCLASS()
class WORLDLEADER_API UWLGovernmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Dispatcher central de acciones de los UWLGovActionButton ("verbo:arg1:arg2"). */
	void HandleAction(const FString& ActionId);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// Agregados de la nacion del jugador (calculados de datos reales).
	struct FSummary
	{
		int32 ProvinceCount = 0;
		int64 Population = 0;
		int64 MonthlyIncome = 0;
		int64 MonthlyUpkeep = 0;
		int32 AveragePublicOrder = 0;
		TArray<FWLProvinceData> Controlled;
	};

	void BuildShell();
	void BuildHeader(UVerticalBox* Root);
	void BuildTabs(UVerticalBox* Root);
	void BuildBody(UVerticalBox* Root);
	void BuildFooter(UVerticalBox* Root);
	void RebuildCenter();

	void BuildOverviewTab();
	void BuildEconomyTab();       // FE: presupuesto/mercado/comercio/finanzas/gobernanza/impuestos
	void BuildHighCommandTab();   // F1.6/F1.7: gabinete + generales
	void BuildPoliticsTab();      // F2: poder interno + F5: eventos
	void BuildDiplomacyTab();     // F3: relaciones/tratados/guerra + F4: intriga
	void BuildRecordsTab();

	void SetActiveTab(EWLGovernmentTab Tab);
	void RefreshTabButtonStyles();

	UFUNCTION() void OnTabOverview();
	UFUNCTION() void OnTabEconomy();
	UFUNCTION() void OnTabHighCommand();
	UFUNCTION() void OnTabPolitics();
	UFUNCTION() void OnTabDiplomacy();
	UFUNCTION() void OnTabRecords();
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnTaxDown();   // FE1.2: palanca de impuestos
	UFUNCTION() void OnTaxUp();

	void AdjustTaxRate(int32 DeltaPercent);   // FE1.2

	FSummary BuildSummary() const;
	FString PlayerIso() const;

	UWLCampaignGameInstance* GetCampaignGI() const;
	UWLDataRegistry* GetRegistry() const;
	UWLStrategicTickSubsystem* GetTick() const;
	UWLCharacterSubsystem* GetCharacters() const;
	UWLPoliticalSubsystem* GetPolitical() const;

	/** Primer espia activo del jugador (las operaciones de intriga lo requieren). Vacio si no hay. */
	FString FindPlayerSpyId() const;

	UPROPERTY() UScrollBox* CenterScroll = nullptr;
	UPROPERTY() UVerticalBox* CenterBox = nullptr;
	UPROPERTY() TArray<UButton*> TabButtons;

	EWLGovernmentTab ActiveTab = EWLGovernmentTab::Overview;

	/** Resultado de la ultima accion (feedback del backend), mostrado arriba del contenido. */
	FString LastActionMessage;
	bool bLastActionSucceeded = true;
};
