// Copyright World Leader project. See ROADMAP.md.
//
// Ventana de GOBIERNO / PRESIDENCIA construida en UMG/C++ (mismo patron que
// WLMainMenuWidget: sin assets binarios, fuentes Slate reales, UBorder con
// padding). Es el equivalente moderno al panel de faccion de Total War
// (Resumen / Politica / Personajes / Registros), adaptado al roadmap de
// World Leader: presidencia, gabinete de ministerios, otras potencias y
// registros. Reemplaza al overlay de Canvas (ilegible) por UI de verdad.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "Blueprint/UserWidget.h"
#include "WLGovernmentWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UScrollBox;
class UWidget;
class UWLDataRegistry;
class UWLStrategicTickSubsystem;
class UWLCampaignGameInstance;

/** Pestanas de la ventana de Gobierno (cambian el contenido central). */
UENUM()
enum class EWLGovernmentTab : uint8
{
	Overview,  // RESUMEN  -> metricas de la nacion
	Economy,   // ECONOMIA -> presupuesto por categorias + palanca de impuestos (FE1.3)
	Politics,  // POLITICA -> orden publico (real) + sistemas del roadmap (fase futura)
	Nation,    // NACION   -> provincias controladas (datos reales)
	Records    // REGISTROS-> eventos recientes / IA economica
};

UCLASS()
class WORLDLEADER_API UWLGovernmentWidget : public UUserWidget
{
	GENERATED_BODY()

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
	void BuildEconomyTab();   // FE1.3
	void BuildPoliticsTab();
	void BuildNationTab();
	void BuildRecordsTab();

	void SetActiveTab(EWLGovernmentTab Tab);
	void RefreshTabButtonStyles();

	UFUNCTION() void OnTabOverview();
	UFUNCTION() void OnTabEconomy();   // FE1.3
	UFUNCTION() void OnTabPolitics();
	UFUNCTION() void OnTabNation();
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

	UPROPERTY() UScrollBox* CenterScroll = nullptr;
	UPROPERTY() UVerticalBox* CenterBox = nullptr;
	UPROPERTY() TArray<UButton*> TabButtons;

	EWLGovernmentTab ActiveTab = EWLGovernmentTab::Overview;
};
