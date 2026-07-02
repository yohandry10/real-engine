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
#include "Core/WLCharacterTypes.h"
#include "Core/WLPoliticalTypes.h"
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
class UWLMilitarySubsystem;
class UWLBalanceSubsystem;
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
	Politics,     // POLITICA   -> hub de Gobierno P1/P2 con subsecciones
	Diplomacy,    // DIPLOMACIA -> relaciones, tratados, guerra (F3) + intriga (F4)
	Records,      // REGISTROS  -> noticias del mes + IA economica/politica + calibracion
	Province      // PROVINCIA  -> slots de edificios de la provincia seleccionada (sin boton de tab; se abre con [B])
};

/** Subsecciones de la pestana POLITICA (Gobierno P1/P2 real). */
enum class EWLPoliticsSection : uint8
{
	Power,      // PODER      -> orden, golpe, grupos sociales, capacidad estatal, eventos
	Agenda,     // AGENDA     -> prioridades nacionales (SetGovernmentAgenda)
	Programs,   // PROGRAMAS  -> catalogo/activos por cartera (StartMinistryProgram)
	Laws,       // LEYES      -> arbol de reformas P2 (EnactPolicyReform)
	Congress,   // CONGRESO   -> instituciones, partidos y patronazgo
	Elections,  // ELECCIONES -> fase electoral, encuestas y promesas
	Media,      // MEDIOS     -> prensa, opinion publica y narrativa
	Regions,    // REGIONES   -> gobernadores y politica regional
	Crisis      // CRISIS     -> cadenas de crisis y memoria politica
};

UCLASS()
class WORLDLEADER_API UWLGovernmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Dispatcher central de acciones de los UWLGovActionButton ("verbo:arg1:arg2"). */
	void HandleAction(const FString& ActionId);

	/** Abre la ventana en modo PROVINCIA: panel de slots de edificios (niveles, upgrade, efectos). */
	void OpenProvince(const FString& ProvinceId);

	/**
	 * Confirmacion en dos clics: las acciones sensibles (guerra, reforma, purga, censura...)
	 * quedan pendientes al primer clic y el boton se repinta como "CONFIRMAR?". True si
	 * ActionId es la accion pendiente (lo consulta MakeActionButton al construir cada boton).
	 */
	bool IsPendingConfirm(const FString& ActionId) const { return !PendingConfirmId.IsEmpty() && PendingConfirmId == ActionId; }

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
	void BuildPoliticsTab();      // hub Gobierno P1/P2 (subsecciones abajo)
	void BuildDiplomacyTab();     // F3: relaciones/tratados/guerra + F4: intriga
	void BuildRecordsTab();
	void BuildProvinceTab();      // slots de edificios de ProvinceContextId

	// --- Gobierno P1/P2 (WLGovernmentWidgetGovernance.cpp). Solo leen endpoints y disparan acciones. ---
	void BuildPoliticsHeader();          // pulso politico + chips de subseccion
	void BuildPoliticsPowerSection();    // orden, golpe, grupos sociales, capacidad estatal, eventos
	void BuildPoliticsAgendaSection();   // selector de prioridades (max 3)
	void BuildPoliticsProgramsSection(); // programas ministeriales activos + catalogo por cartera
	void BuildPoliticsLawsSection();     // arbol de reformas por area + reformas activas
	void BuildPoliticsCongressSection(); // instituciones, partidos, patronazgo
	void BuildPoliticsElectionsSection();// fase, encuestas, legitimidad, promesas
	void BuildPoliticsMediaSection();    // prensa, opinion, acciones de medios
	void BuildPoliticsRegionsSection();  // gobernadores regionales y sus politicas
	void BuildPoliticsCrisisSection();   // cadenas de crisis + memoria politica
	void BuildGovernanceOverviewCards(); // RESUMEN: aprobacion/legitimidad/eleccion/coalicion
	void BuildMinisterComparator(EWLMinisterOffice Office);   // ALTO MANDO: comparador de candidatos
	void BuildCabinetDynamicsCard();     // ALTO MANDO: gabinete vivo (rivalidad/escandalo/renuncia)
	void BuildPoliticalProfilesSection();// ALTO MANDO: fichas politicas de personajes
	void BuildAIPlansPanel();            // REGISTROS: plan de IA politica por pais
	void BuildCalibrationPanel();        // REGISTROS: telemetria de dilemas (playtest)

	/** DIPLOMACIA: panel de gestion del pais seleccionado (tratados, guerra, FDI, intriga). */
	void BuildDiplomacyDetailPanel(const FWLNationData& Other);

	void BuildArmiesSection();      // ALTO MANDO: ejercitos, general asignado, reorganizar (ReorganizeArmy)
	void BuildDifficultyPanel();    // RESUMEN: nivel de IA activo + selector (UWLBalanceSubsystem)

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
	UFUNCTION() void OnDiplomacySearchCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void AdjustTaxRate(int32 DeltaPercent);   // FE1.2

	FSummary BuildSummary() const;
	FString PlayerIso() const;

	UWLCampaignGameInstance* GetCampaignGI() const;
	UWLDataRegistry* GetRegistry() const;
	UWLStrategicTickSubsystem* GetTick() const;
	UWLCharacterSubsystem* GetCharacters() const;
	UWLPoliticalSubsystem* GetPolitical() const;
	UWLMilitarySubsystem* GetMilitary() const;
	UWLBalanceSubsystem* GetBalance() const;

	/** Primer espia activo del jugador (las operaciones de intriga lo requieren). Vacio si no hay. */
	FString FindPlayerSpyId() const;

	UPROPERTY() UScrollBox* CenterScroll = nullptr;
	UPROPERTY() UVerticalBox* CenterBox = nullptr;
	UPROPERTY() TArray<UButton*> TabButtons;

	EWLGovernmentTab ActiveTab = EWLGovernmentTab::Overview;

	/** Resultado de la ultima accion (feedback del backend), mostrado arriba del contenido. */
	FString LastActionMessage;
	bool bLastActionSucceeded = true;

	/** Provincia mostrada por el modo PROVINCIA (destino de build:/upgrade:). */
	FString ProvinceContextId;

	// --- Estado de navegacion de Gobierno P1/P2 (solo UI; las reglas viven en el backend) ---

	/** Subseccion activa dentro de POLITICA. */
	EWLPoliticsSection PoliticsSection = EWLPoliticsSection::Power;

	/** Accion sensible esperando el segundo clic de confirmacion (vacio = ninguna). */
	FString PendingConfirmId;

	/** Borrador del selector de agenda (se sincroniza desde backend al entrar a AGENDA). */
	TArray<EWLGovernmentPriority> DraftAgenda;
	bool bDraftAgendaLoaded = false;

	/** Filtro de cartera en PROGRAMAS: 0 = todas, si no un EWLMinisterOffice. */
	int32 ProgramOfficeFilter = 0;

	/** Filtro de area en LEYES: -1 = todas, si no un EWLPolicyReformArea. */
	int32 ReformAreaFilter = -1;

	/** Cartera con el comparador de candidatos abierto en ALTO MANDO (-1 = cerrado). */
	int32 CompareOfficeContext = -1;

	/** Orden del listado de perfiles politicos: 0 sucesion, 1 ambicion, 2 corrupcion, 3 lealtad. */
	int32 ProfileSortMode = 0;

	// --- Estado de navegacion de DIPLOMACIA continental (38 naciones exigen filtros, no lista plana) ---

	/** Pais con el panel de gestion abierto (vacio = solo listado compacto). */
	FString SelectedDiplomacyIso;

	/** Filtro de estado: 0 todos, 1 guerra, 2 tension, 3 paz, 4 aliados, 5 con tratado, 6 embargo. */
	int32 DiplomacyStatusFilter = 0;

	/** Orden: 0 nombre, 1 opinion desc, 2 opinion asc, 3 tesoro, 4 provincias, 5 estado (guerra primero). */
	int32 DiplomacySortMode = 0;

	/** Busqueda por nombre o ISO (se confirma con Enter). */
	FString DiplomacySearchText;
};
