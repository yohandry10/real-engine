// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLGovernmentWidget.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Balance/WLBalanceSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Core/WLCharacterTypes.h"
#include "Core/WLFinancialTypes.h"
#include "Core/WLPoliticalTypes.h"
#include "Economy/WLEconomyLibrary.h"
#include "Military/WLMilitarySubsystem.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"

#include "UI/WLGovernmentWidgetShared.h"

using namespace WLGovUI;

void UWLGovActionButton::BindAction(UWLGovernmentWidget* InOwner, const FString& InActionId)
{
	Owner = InOwner;
	ActionId = InActionId;
	OnClicked.AddDynamic(this, &UWLGovActionButton::HandleClicked);
}

void UWLGovActionButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleAction(ActionId);
	}
}

TSharedRef<SWidget> UWLGovernmentWidget::RebuildWidget()
{
	// Widget 100% C++: hay que poblar el WidgetTree ANTES de que Slate lo tome aqui. Si se
	// construye en NativeConstruct (mas tarde), Slate ya se armo con un arbol vacio y no pinta nada.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildShell();
		SetActiveTab(EWLGovernmentTab::Overview);
	}
	return Super::RebuildWidget();
}

void UWLGovernmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UWLGovernmentWidget::NativeDestruct()
{
	for (UButton* B : TabButtons)
	{
		if (B)
		{
			B->OnClicked.RemoveAll(this);
		}
	}
	Super::NativeDestruct();
}

FReply UWLGovernmentWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::C)
	{
		OnCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UWLGovernmentWidget::BuildShell()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GovRoot"));
	WidgetTree->RootWidget = Root;

	// Fondo modal atenuado (cubre todo el mapa).
	UBorder* Dim = MakeBorder(WidgetTree, GovBackdrop, FMargin(0.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	// Marco dorado + panel interior, centrado.
	UBorder* Frame = MakeBorder(WidgetTree, GovFrame, FMargin(2.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Frame))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(FVector2D(0.f, 0.f));
		S->SetSize(FVector2D(1210.f, 860.f));
	}

	UBorder* Panel = MakeBorder(WidgetTree, GovPanel, FMargin(16.f));
	Frame->SetContent(Panel);

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GovMain"));
	Panel->SetContent(Main);

	BuildHeader(Main);
	BuildTabs(Main);
	BuildBody(Main);
	BuildFooter(Main);
}

void UWLGovernmentWidget::BuildHeader(UVerticalBox* Root)
{
	UWLCampaignGameInstance* GI = GetCampaignGI();
	FWLNationData Nation;
	const bool bHasNation = GI && GI->GetSelectedNation(Nation);
	const UWLStrategicTickSubsystem* Tick = GetTick();

	UBorder* Strip = MakeBorder(WidgetTree, GovHeaderStrip, FMargin(16.f, 12.f));
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Strip->SetContent(HB);

	// Emblema (color de la nacion + ISO).
	USizeBox* EmblemBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	EmblemBox->SetWidthOverride(56.f);
	EmblemBox->SetHeightOverride(56.f);
	UBorder* Emblem = MakeBorder(WidgetTree, bHasNation ? Nation.MapColor : FLinearColor::Gray, FMargin(0.f));
	Emblem->SetHorizontalAlignment(HAlign_Center);
	Emblem->SetVerticalAlignment(VAlign_Center);
	Emblem->SetContent(MakeText(WidgetTree, bHasNation ? Nation.Iso : TEXT("--"), 20, GovDarkInk, ETextJustify::Center));
	EmblemBox->SetContent(Emblem);
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(EmblemBox))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}

	// Titulo + subtitulo.
	const FString LeaderName = (bHasNation && !Nation.Leader.IsEmpty()) ? Nation.Leader : TEXT("Presidente de la Republica");
	const FString GovType = (bHasNation && !Nation.GovernmentType.IsEmpty()) ? Nation.GovernmentType : TEXT("Gobierno");
	UVerticalBox* TitleVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TitleVB->AddChildToVerticalBox(MakeText(WidgetTree, bHasNation ? Nation.Name.ToUpper() : TEXT("SIN NACION"), 30, GovText));
	if (UVerticalBoxSlot* S = TitleVB->AddChildToVerticalBox(
		MakeText(WidgetTree, FString::Printf(TEXT("Presidente: %s     %s"), *LeaderName, *GovType), 15, GovGold)))
	{
		S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
	}
	UBorder* TitlePad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(16.f, 0.f, 0.f, 0.f));
	TitlePad->SetContent(TitleVB);
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(TitlePad))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}

	// Fecha del turno.
	if (Tick)
	{
		UVerticalBox* DateVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		DateVB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("TURNO"), 11, GovMuted, ETextJustify::Right));
		if (UVerticalBoxSlot* S = DateVB->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%02d/%02d/%d"), Tick->GetCurrentDay(), Tick->GetCurrentMonth(), Tick->GetCurrentYear()),
			16, GovText, ETextJustify::Right)))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(DateVB))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}
	}

	// Boton cerrar (X).
	UButton* Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Close->SetBackgroundColor(FLinearColor(0.42f, 0.14f, 0.12f, 1.f));
	Close->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnCloseClicked);
	USizeBox* CloseBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CloseBox->SetWidthOverride(38.f);
	CloseBox->SetHeightOverride(38.f);
	Close->SetContent(MakeText(WidgetTree, TEXT("X"), 18, GovText, ETextJustify::Center));
	CloseBox->SetContent(Close);
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(CloseBox))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}

	AddColumnChild(Root, Strip, 0.f);
}

void UWLGovernmentWidget::BuildTabs(UVerticalBox* Root)
{
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	// AddDynamic estampa el NOMBRE de la funcion en tiempo de compilacion, asi que el handler NO puede
	// venir de una variable (puntero a miembro): cada pestana se enlaza explicitamente por indice.
	const TCHAR* Labels[] = { TEXT("RESUMEN"), TEXT("ECONOMIA"), TEXT("ALTO MANDO"), TEXT("POLITICA"), TEXT("DIPLOMACIA"), TEXT("REGISTROS") };

	TabButtons.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(Labels); ++i)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(GovTabIdle);
		switch (i)
		{
		case 0: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabOverview);    break;
		case 1: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabEconomy);     break;
		case 2: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabHighCommand); break;
		case 3: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabPolitics);    break;
		case 4: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabDiplomacy);   break;
		case 5: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabRecords);     break;
		default: break;
		}

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(42.f);
		Box->SetMinDesiredWidth(118.f);
		Box->SetContent(MakeText(WidgetTree, Labels[i], 15, GovText, ETextJustify::Center));
		Button->SetContent(Box);

		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Button))
		{
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		TabButtons.Add(Button);
	}

	AddColumnChild(Root, HB, 12.f);
}

void UWLGovernmentWidget::BuildBody(UVerticalBox* Root)
{
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	// --- Columna izquierda: GABINETE ---
	{
		USizeBox* ColBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ColBox->SetWidthOverride(266.f);
		UBorder* Col = MakeBorder(WidgetTree, GovPanelSoft, FMargin(14.f));
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("GABINETE"), 15, GovGold));

		// Gabinete REAL desde el backend de personajes (gestion en ALTO MANDO).
		const UWLCharacterSubsystem* Characters = GetCharacters();
		const TArray<FWLCabinetSeat> Cabinet = Characters
			? Characters->GetCabinet(PlayerIso())
			: TArray<FWLCabinetSeat>();
		if (Cabinet.Num() > 0)
		{
			for (const FWLCabinetSeat& Seat : Cabinet)
			{
				const bool bFilled = Seat.Minister.IsValid();
				AddColumnChild(VB, MakeListCard(WidgetTree, bFilled ? GovGoldDim : GovMuted,
					FString::Printf(TEXT("Min. de %s"), *UWLCharacterSubsystem::MinisterOfficeToString(Seat.Office)),
					bFilled ? Seat.Minister.Name : TEXT("Cargo vacante"),
					bFilled ? FString::Printf(TEXT("Skill %d"), Seat.Minister.Skill) : TEXT("Vacante"),
					bFilled ? GovGood : GovGold), 8.f);
			}
		}
		else
		{
			AddColumnChild(VB, MakeText(WidgetTree, TEXT("Sin datos de gabinete."), 13, GovMuted, ETextJustify::Left, true), 8.f);
		}
		Scroll->AddChild(VB);
		Col->SetContent(Scroll);
		ColBox->SetContent(Col);
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(ColBox))
		{
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// --- Centro: contenido por pestana (scroll) ---
	{
		UBorder* Col = MakeBorder(WidgetTree, GovPanelSoft, FMargin(6.f));
		CenterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		CenterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CenterScroll->AddChild(CenterBox);
		Col->SetContent(CenterScroll);
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(Col))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(12.f, 0.f, 12.f, 0.f));
		}
	}

	// --- Columna derecha: OTRAS POTENCIAS ---
	{
		USizeBox* ColBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ColBox->SetWidthOverride(266.f);
		UBorder* Col = MakeBorder(WidgetTree, GovPanelSoft, FMargin(14.f));
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("OTRAS POTENCIAS"), 15, GovGold));

		const UWLDataRegistry* Registry = GetRegistry();
		const UWLStrategicTickSubsystem* Tick = GetTick();
		const FString Iso = PlayerIso();
		int32 Shown = 0;
		if (Registry)
		{
			TArray<FWLNationData> Nations = Registry->GetAllNations();
			Nations.Sort([](const FWLNationData& A, const FWLNationData& B) { return A.Name < B.Name; });
			for (const FWLNationData& Other : Nations)
			{
				if (Other.Iso.Equals(Iso, ESearchCase::IgnoreCase))
				{
					continue;
				}
				const int64 Tre = Tick ? Tick->GetTreasury(Other.Iso) : 0;
				const int32 Prov = Registry->GetProvincesByNation(Other.Iso).Num();
				AddColumnChild(VB, MakeListCard(WidgetTree, Other.MapColor, Other.Name,
					FString::Printf(TEXT("Tesoro %s"), *GovGroupThousands(Tre)),
					FString::Printf(TEXT("%d prov"), Prov), GovMuted), 8.f);
				++Shown;
			}
		}
		if (Shown == 0)
		{
			AddColumnChild(VB, MakeText(WidgetTree, TEXT("Sin datos de otras naciones."), 13, GovMuted, ETextJustify::Left, true), 8.f);
		}
		Scroll->AddChild(VB);
		Col->SetContent(Scroll);
		ColBox->SetContent(Col);
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(ColBox))
		{
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Body))
	{
		S->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UWLGovernmentWidget::BuildFooter(UVerticalBox* Root)
{
	UWLCampaignGameInstance* GI = GetCampaignGI();
	FWLNationData Nation;
	const bool bHasNation = GI && GI->GetSelectedNation(Nation);
	const FString LeaderName = (bHasNation && !Nation.Leader.IsEmpty()) ? Nation.Leader : TEXT("Presidente de la Republica");
	const FString GovType = (bHasNation && !Nation.GovernmentType.IsEmpty()) ? Nation.GovernmentType : TEXT("Gobierno");

	UBorder* Footer = MakeBorder(WidgetTree, GovHeaderStrip, FMargin(16.f, 12.f));
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Footer->SetContent(HB);

	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Left->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("PRESIDENTE"), 12, GovGold));
	if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(MakeText(WidgetTree, LeaderName, 20, GovText)))
	{
		S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
	}
	if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(MakeText(WidgetTree, GovType, 13, GovMuted)))
	{
		S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Left))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Right->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("PODERES"), 12, GovGold, ETextJustify::Left));
	UHorizontalBox* Tags = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const TCHAR* Powers[] = { TEXT("Ejecutivo"), TEXT("Comandante en jefe"), TEXT("Decretos"), TEXT("Presupuesto") };
	for (const TCHAR* P : Powers)
	{
		if (UHorizontalBoxSlot* S = Tags->AddChildToHorizontalBox(MakeTag(WidgetTree, P)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
	}
	if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(Tags))
	{
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}
	Right->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Agenda nacional, reformas y crisis: pestana POLITICA."), 12, GovMuted));
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Right))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}

	AddColumnChild(Root, Footer, 12.f);
}

void UWLGovernmentWidget::RebuildCenter()
{
	if (!CenterBox)
	{
		return;
	}
	CenterBox->ClearChildren();
	if (CenterScroll)
	{
		CenterScroll->ScrollToStart();
	}
	// Feedback de la ultima accion (nombramientos, tratados, bonos...): visible en cualquier tab.
	if (!LastActionMessage.IsEmpty())
	{
		UBorder* Strip = MakeBorder(WidgetTree, GovHeaderStrip, FMargin(12.f, 8.f));
		Strip->SetContent(MakeText(WidgetTree, LastActionMessage, 13,
			bLastActionSucceeded ? GovGold : GovBad, ETextJustify::Left, true));
		AddColumnChild(CenterBox, Strip, 4.f);
	}

	switch (ActiveTab)
	{
	case EWLGovernmentTab::Overview:    BuildOverviewTab();    break;
	case EWLGovernmentTab::Economy:     BuildEconomyTab();     break;
	case EWLGovernmentTab::HighCommand: BuildHighCommandTab(); break;
	case EWLGovernmentTab::Politics:    BuildPoliticsTab();    break;
	case EWLGovernmentTab::Diplomacy:   BuildDiplomacyTab();   break;
	case EWLGovernmentTab::Records:     BuildRecordsTab();     break;
	case EWLGovernmentTab::Province:    BuildProvinceTab();    break;
	}
}

void UWLGovernmentWidget::BuildOverviewTab()
{
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	const FSummary Sum = BuildSummary();
	const int64 Treasury = Tick ? Tick->GetTreasury(Iso) : 0;
	const int64 Balance = Tick ? Tick->GetMonthlyBalance(Iso) : 0;

	// F5.3/F5.4: si la campania termino, el RESUMEN lo anuncia primero.
	if (const UWLPoliticalSubsystem* Political = GetPolitical())
	{
		const FWLCampaignOutcomeState Outcome = Political->GetCampaignOutcome();
		if (Outcome.bGameOver)
		{
			const bool bPlayerWon = Outcome.WinningNationIso.Equals(Iso, ESearchCase::IgnoreCase);
			UBorder* Banner = MakeBorder(WidgetTree, bPlayerWon ? GovGoldDim : FLinearColor(0.35f, 0.10f, 0.08f, 1.f), FMargin(14.f, 12.f));
			UVerticalBox* BVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			BVB->AddChildToVerticalBox(MakeText(WidgetTree,
				bPlayerWon ? TEXT("VICTORIA") : TEXT("FIN DE LA PARTIDA"), 22, GovText));
			if (UVerticalBoxSlot* S = BVB->AddChildToVerticalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("%s — %s"), *Outcome.OutcomeType, *Outcome.Reason), 13, GovText, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
			Banner->SetContent(BVB);
			AddColumnChild(CenterBox, Banner, 6.f);
		}
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("ESTADO DE LA NACION"), 17, GovGold), 6.f);

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(5.f));
	auto Place = [&](int32 R, int32 Col, UBorder* Card)
	{
		if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, R, Col))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	Place(0, 0, MakeMetricCard(WidgetTree, TEXT("Tesoro nacional"), GovGroupThousands(Treasury), GovText));
	Place(0, 1, MakeMetricCard(WidgetTree, TEXT("Balance mensual"),
		FString::Printf(TEXT("%s%s"), Balance >= 0 ? TEXT("+") : TEXT(""), *GovGroupThousands(Balance)), Balance >= 0 ? GovGood : GovBad));
	Place(1, 0, MakeMetricCard(WidgetTree, TEXT("Provincias"), FString::Printf(TEXT("%d"), Sum.ProvinceCount), GovText));
	Place(1, 1, MakeMetricCard(WidgetTree, TEXT("Poblacion"), GovGroupThousands(Sum.Population), GovText));
	Place(2, 0, MakeMetricCard(WidgetTree, TEXT("Ingreso / mes"), GovGroupThousands(Sum.MonthlyIncome), GovGood));
	Place(2, 1, MakeMetricCard(WidgetTree, TEXT("Mantenimiento / mes"), GovGroupThousands(Sum.MonthlyUpkeep), GovMuted));
	// FE1.5: PIB y su crecimiento entre ticks economicos.
	if (Tick)
	{
		const double Growth = Tick->GetNationGDPGrowth(Iso);
		Place(3, 0, MakeMetricCard(WidgetTree, TEXT("PIB / mes"), GovGroupThousands(Tick->GetNationGDP(Iso)), GovText));
		Place(3, 1, MakeMetricCard(WidgetTree, TEXT("Crecimiento"),
			FString::Printf(TEXT("%+.2f%%"), Growth * 100.0),
			Growth > 0.0 ? GovGood : (Growth < 0.0 ? GovBad : GovMuted)));
	}
	AddColumnChild(CenterBox, Grid, 10.f);

	// Gobierno P1/P2: pulso politico del gobierno (aprobacion, legitimidad, eleccion, coalicion...).
	BuildGovernanceOverviewCards();

	// Dificultad de la IA activa + selector (lee reglas del backend de balance).
	BuildDifficultyPanel();

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("CONDICIONES DE VICTORIA"), 17, GovGold), 20.f);
	UHorizontalBox* Tags = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const TCHAR* Conditions[] = { TEXT("Dominacion"), TEXT("Economica"), TEXT("Tecnologica"), TEXT("Diplomatica"), TEXT("Militar") };
	for (const TCHAR* C : Conditions)
	{
		if (UHorizontalBoxSlot* S = Tags->AddChildToHorizontalBox(MakeTag(WidgetTree, C)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
	}
	AddColumnChild(CenterBox, Tags, 8.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Dominacion y golpe/revolucion se chequean cada mes; el resto llega con sus fases."), 13, GovMuted, ETextJustify::Left, true), 8.f);

	// Territorio (antes tab NACION): provincias controladas con poblacion y balance real.
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("TERRITORIO  (%d provincias)"), Sum.ProvinceCount), 17, GovGold), 20.f);
	if (Sum.Controlled.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin provincias bajo control directo."), 14, GovMuted), 10.f);
		return;
	}

	auto MakeRow = [&](const FString& Name, const FString& Pop, const FString& Bal, const FLinearColor& BalColor,
		const FLinearColor& RowColor, int32 FontSize, const FLinearColor& NameColor)
	{
		UBorder* Row = MakeBorder(WidgetTree, RowColor, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, Name, FontSize, NameColor)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		USizeBox* PopBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PopBox->SetWidthOverride(160.f);
		PopBox->SetContent(MakeText(WidgetTree, Pop, FontSize, GovMuted, ETextJustify::Right));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(PopBox)) { S->SetVerticalAlignment(VAlign_Center); }
		USizeBox* BalBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BalBox->SetWidthOverride(140.f);
		BalBox->SetContent(MakeText(WidgetTree, Bal, FontSize, BalColor, ETextJustify::Right));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(BalBox)) { S->SetVerticalAlignment(VAlign_Center); }
		Row->SetContent(HB);
		return Row;
	};

	AddColumnChild(CenterBox, MakeRow(TEXT("Provincia"), TEXT("Poblacion"), TEXT("Balance/mes"),
		GovMuted, FLinearColor(0.f, 0.f, 0.f, 0.f), 12, GovMuted), 6.f);

	int32 Index = 0;
	for (const FWLProvinceData& P : Sum.Controlled)
	{
		const int64 Bal = Tick ? Tick->GetProvinceMonthlyBalance(P.Id) : 0;
		AddColumnChild(CenterBox, MakeRow(P.Name, GovGroupThousands(P.Population),
			FString::Printf(TEXT("%s%s"), Bal >= 0 ? TEXT("+") : TEXT(""), *GovGroupThousands(Bal)),
			Bal >= 0 ? GovGood : GovBad, (Index % 2 == 0) ? GovCard : GovCardAlt, 15, GovText), 4.f);
		++Index;
	}
}

// FE1.3: panel ECONOMIA — presupuesto mensual desglosado por categorias + palanca de impuestos (FE1.2).
void UWLGovernmentWidget::BuildEconomyTab()
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	if (!Tick || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos economicos."), 14, GovMuted), 10.f);
		return;
	}

	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const FWLNationBudget Budget = Tick->GetNationBudget(Iso);

	// Fila de presupuesto: etiqueta (fill) + importe con signo a la derecha.
	auto AddBudgetRow = [&](const FString& Label, int64 Amount, bool bIsIncome, bool bTotal, int32 Index)
	{
		const FLinearColor RowColor = bTotal ? GovCardAlt : ((Index % 2 == 0) ? GovCard : GovCardAlt);
		UBorder* Row = MakeBorder(WidgetTree, RowColor, FMargin(12.f, bTotal ? 10.f : 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(
			MakeText(WidgetTree, Label, bTotal ? 15 : 14, bTotal ? GovGold : GovText)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		HB->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s%s"), bIsIncome ? TEXT("+") : TEXT("-"), *GovGroupThousands(Amount)),
			bTotal ? 15 : 14, bIsIncome ? GovGood : GovBad, ETextJustify::Right));
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, bTotal ? 8.f : 4.f);
	};

	// FE1.5: PIB y crecimiento arriba del presupuesto.
	{
		const double Growth = Tick->GetNationGDPGrowth(Iso);
		const double Inflation = Tick->GetNationInflationRate(Iso);
		const FWLNationLaborStats Labor = Tick->GetNationLaborStats(Iso);
		AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
			TEXT("PIB: %s / mes   ·   Crecimiento: %+.2f%%   ·   Inflacion: %+.2f%%   ·   Ciclo: %s"),
			*GovGroupThousands(Tick->GetNationGDP(Iso)), Growth * 100.0,
			Inflation * 100.0, *Tick->GetNationEconomicCycleLabel(Iso)),
			14, Growth < 0.0 ? GovBad : GovText), 6.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
			TEXT("Empleo: %s / %s   ·   Desempleo: %.1f%%   ·   Productividad: %.0f%%"),
			*GovGroupThousands(Labor.Employed), *GovGroupThousands(Labor.Workforce),
			Labor.UnemploymentRate * 100.0, Labor.Productivity * 100.0),
			12, Labor.UnemploymentRate > 0.15 ? GovBad : GovMuted), 4.f);
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("PRESUPUESTO MENSUAL"), 17, GovGold), 12.f);

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("INGRESOS"), 13, GovMuted), 12.f);
	AddBudgetRow(TEXT("Recursos y produccion"), Budget.ResourceIncome, true, false, 0);
	AddBudgetRow(TEXT("Impuestos"), Budget.TaxIncome, true, false, 1);
	if (Budget.ExportIncome > 0)
	{
		AddBudgetRow(TEXT("Exportaciones"), Budget.ExportIncome, true, false, 2);
	}
	if (Budget.TariffIncome > 0)
	{
		AddBudgetRow(TEXT("Aranceles"), Budget.TariffIncome, true, false, 3);   // FE4.3
	}
	if (Budget.ForeignAidIncome > 0)
	{
		AddBudgetRow(TEXT("Ayuda exterior recibida"), Budget.ForeignAidIncome, true, false, 4);   // FE5.3
	}
	AddBudgetRow(TEXT("TOTAL INGRESOS"), Budget.TotalIncome(), true, true, 0);

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("GASTOS"), 13, GovMuted), 14.f);
	AddBudgetRow(TEXT("Militar"), Budget.MilitaryUpkeep, false, false, 0);
	AddBudgetRow(TEXT("Infraestructura"), Budget.InfrastructureUpkeep, false, false, 1);
	AddBudgetRow(TEXT("Salarios publicos"), Budget.PublicWages, false, false, 2);
	AddBudgetRow(TEXT("Gasto social"), Budget.SocialSpending, false, false, 3);
	if (Budget.DebtInterest > 0)
	{
		AddBudgetRow(TEXT("Intereses de deuda"), Budget.DebtInterest, false, false, 4);   // FE1.4
	}
	if (Budget.DebtService > 0)
	{
		AddBudgetRow(TEXT("Servicio de deuda (bonos/FMI)"), Budget.DebtService, false, false, 5);   // FE5.1
	}
	if (Budget.ImportCost > 0)
	{
		AddBudgetRow(TEXT("Importaciones criticas"), Budget.ImportCost, false, false, 6);
	}
	if (Budget.CorruptionLoss > 0)
	{
		AddBudgetRow(TEXT("Perdida por corrupcion"), Budget.CorruptionLoss, false, false, 7);   // FE6.2
	}
	if (Budget.ForeignAidExpense > 0)
	{
		AddBudgetRow(TEXT("Ayuda exterior concedida"), Budget.ForeignAidExpense, false, false, 8);   // FE5.3
	}
	AddBudgetRow(TEXT("TOTAL GASTOS"), Budget.TotalSpending(), false, true, 0);

	// Balance neto (== GetMonthlyBalance).
	const int64 Net = Budget.Net();
	UBorder* NetCard = MakeBorder(WidgetTree, GovHeaderStrip, FMargin(12.f, 11.f));
	UHorizontalBox* NetHB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = NetHB->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("BALANCE MENSUAL"), 15, GovText)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	NetHB->AddChildToHorizontalBox(MakeText(WidgetTree,
		FString::Printf(TEXT("%s%s"), Net >= 0 ? TEXT("+") : TEXT(""), *GovGroupThousands(Net)),
		18, Net >= 0 ? GovGood : GovBad, ETextJustify::Right));
	NetCard->SetContent(NetHB);
	AddColumnChild(CenterBox, NetCard, 12.f);

	// FE1.4: deuda y linea de credito. Gastar por encima del tesoro endeuda (con interes mensual)
	// hasta el limite de credito; el tesoro negativo ademas penaliza el orden publico cada mes.
	{
		const int64 Treasury = Tick->GetTreasury(Iso);
		const int64 CreditLimit = Tick->GetCreditLimit(Iso);
		if (Treasury < 0)
		{
			UBorder* DebtCard = MakeBorder(WidgetTree, GovCard, FMargin(12.f, 10.f));
			UVerticalBox* DebtVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UHorizontalBox* DebtHB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = DebtHB->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("DEUDA"), 15, GovBad)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			DebtHB->AddChildToHorizontalBox(MakeText(WidgetTree,
				GovGroupThousands(-Treasury), 18, GovBad, ETextJustify::Right));
			DebtVB->AddChildToVerticalBox(DebtHB);
			if (UVerticalBoxSlot* S = DebtVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
				TEXT("Interes %.0f%%/mes (%s/mes)   ·   Credito restante: %s"),
				Rules.DebtMonthlyInterestRate * 100.0,
				*GovGroupThousands(Budget.DebtInterest),
				*GovGroupThousands(FMath::Max<int64>(0, CreditLimit + Treasury))),
				13, GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
			DebtCard->SetContent(DebtVB);
			AddColumnChild(CenterBox, DebtCard, 8.f);
		}
		else
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
				TEXT("Sin deuda. Linea de credito disponible: %s (interes %.0f%%/mes si el tesoro cae en negativo)."),
				*GovGroupThousands(CreditLimit), Rules.DebtMonthlyInterestRate * 100.0),
				12, GovMuted, ETextJustify::Left, true), 8.f);
		}
	}

	// FE2.3-FE4.1: produccion nacional por bien con insumos, demanda, precios y comercio.
	{
		const TArray<FWLGoodMarketBalance> Market = Tick->GetNationGoodMarketBalance(Iso);
		if (Market.Num() > 0)
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("MERCADO NACIONAL / MES"), 17, GovGold), 20.f);
			const UWLDataRegistry* Registry = GetRegistry();
			int32 Index = 0;
			for (const FWLGoodMarketBalance& Balance : Market)
			{
				FWLGoodData Good;
				const bool bHasGood = Registry && Registry->GetGood(Balance.GoodId, Good);
				const FString GoodName = bHasGood ? Good.Name : Balance.GoodId;
				const bool bManufactured = bHasGood && Good.Category == EWLGoodCategory::Manufactured;

				UBorder* Row = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 7.f));
				UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, GoodName, 14, GovText)))
				{
					S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					S->SetVerticalAlignment(VAlign_Center);
				}
				USizeBox* KindBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				KindBox->SetWidthOverride(104.f);
				KindBox->SetContent(MakeText(WidgetTree, bManufactured ? TEXT("Manufactura") : TEXT("Extraccion"),
					11, GovMuted, ETextJustify::Right));
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(KindBox)) { S->SetVerticalAlignment(VAlign_Center); }
				USizeBox* UnitsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				UnitsBox->SetWidthOverride(142.f);
				UnitsBox->SetContent(MakeText(WidgetTree,
					FString::Printf(TEXT("%s / %s"), *GovGroupThousands(Balance.Production), *GovGroupThousands(Balance.Demand)),
					13, GovGold, ETextJustify::Right));
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(UnitsBox)) { S->SetVerticalAlignment(VAlign_Center); }
				USizeBox* PriceBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				PriceBox->SetWidthOverride(86.f);
				PriceBox->SetContent(MakeText(WidgetTree,
					FString::Printf(TEXT("x%.2f"), Balance.PriceMultiplier),
					13, Balance.PriceMultiplier > 1.05 ? GovBad : GovMuted, ETextJustify::Right));
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(PriceBox)) { S->SetVerticalAlignment(VAlign_Center); }
				USizeBox* TradeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				TradeBox->SetWidthOverride(136.f);
				TradeBox->SetContent(MakeText(WidgetTree,
					FString::Printf(TEXT("I %s / E %s"),
						*GovGroupThousands(Balance.Imports), *GovGroupThousands(Balance.Exports)),
					12, (Balance.Deficit > 0 || Balance.Imports > 0) ? GovBad : GovMuted, ETextJustify::Right));
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(TradeBox)) { S->SetVerticalAlignment(VAlign_Center); }
				Row->SetContent(HB);
				AddColumnChild(CenterBox, Row, 4.f);
				++Index;
			}
			AddColumnChild(CenterBox, MakeText(WidgetTree,
				TEXT("Oferta final tras insumos, precios y comercio regional."),
				12, GovMuted, ETextJustify::Left, true), 6.f);
		}
	}

	// FE3.4: shocks de mercado activos (bien, multiplicador, duracion restante).
	{
		const TArray<FWLMarketShockState> Shocks = Tick->GetActiveMarketShocks();
		if (Shocks.Num() > 0)
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("SHOCKS DE MERCADO ACTIVOS"), 17, GovGold), 20.f);
			const UWLDataRegistry* Registry = GetRegistry();
			int32 Index = 0;
			for (const FWLMarketShockState& Shock : Shocks)
			{
				FWLGoodData Good;
				const FString GoodName = (Shock.GoodId == TEXT("*") || Shock.GoodId.Equals(TEXT("all"), ESearchCase::IgnoreCase))
					? TEXT("Todo el mercado")
					: ((Registry && Registry->GetGood(Shock.GoodId, Good)) ? Good.Name : Shock.GoodId);
				AddColumnChild(CenterBox, MakeStatRow(WidgetTree,
					FString::Printf(TEXT("%s — %s"), *Shock.Title, *GoodName),
					FString::Printf(TEXT("x%.2f · %d/%d meses"), Shock.PriceMultiplier, Shock.RemainingMonths, Shock.TotalMonths),
					Shock.PriceMultiplier >= 1.0 ? GovBad : GovGood,
					(Index % 2 == 0) ? GovCard : GovCardAlt), 4.f);
				++Index;
			}
		}
	}

	// FE4.3: arancel nacional (stepper) + resumen de comercio del presupuesto.
	{
		const int32 Tariff = Tick->GetTariffRate(Iso);
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("COMERCIO EXTERIOR"), 17, GovGold), 20.f);

		UBorder* TariffCard = MakeBorder(WidgetTree, GovCard, FMargin(14.f, 11.f));
		UHorizontalBox* TariffRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* TariffInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TariffInfo->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("ARANCEL A IMPORTACIONES"), 12, GovMuted));
		if (UVerticalBoxSlot* S = TariffInfo->AddChildToVerticalBox(
			MakeText(WidgetTree, FString::Printf(TEXT("%d%%"), Tariff), 27, GovGold)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = TariffInfo->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Ingreso por aranceles: %s/mes   ·   Exporta %s · Importa %s"),
			*GovGroupThousands(Budget.TariffIncome),
			*GovGroupThousands(Budget.ExportIncome), *GovGroupThousands(Budget.ImportCost)),
			13, GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = TariffRow->AddChildToHorizontalBox(TariffInfo))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		auto AddTariffButton = [&](const FString& ActionId, const TCHAR* Label)
		{
			if (UHorizontalBoxSlot* S = TariffRow->AddChildToHorizontalBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, GovTabIdle, 52.f, 20)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			}
		};
		AddTariffButton(TEXT("tariffdown"), TEXT("-"));
		AddTariffButton(TEXT("tariffup"), TEXT("+"));
		TariffCard->SetContent(TariffRow);
		AddColumnChild(CenterBox, TariffCard, 10.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Subir aranceles recauda y protege la industria local, pero encarece importaciones y molesta a los vecinos. Rutas y embargos: en DIPLOMACIA."),
			12, GovMuted, ETextJustify::Left, true), 6.f);
	}

	// FE5.1/FE5.3: finanzas soberanas — rating, deuda, bonos, FMI, default, apoyos externos.
	{
		const FWLFinancialProfile Profile = Tick->GetFinancialProfile(Iso);
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("FINANZAS SOBERANAS"), 17, GovGold), 20.f);

		const FLinearColor RatingColor = Profile.bInDefault ? GovBad : (Profile.CreditScore >= 60 ? GovGood : GovGold);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Calificacion crediticia"),
			FString::Printf(TEXT("%s (%d/100)%s"), *Profile.CreditRatingLabel, Profile.CreditScore,
				Profile.bInDefault ? TEXT(" · EN DEFAULT") : TEXT("")),
			RatingColor, GovCard), 8.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Deuda viva / servicio mensual"),
			FString::Printf(TEXT("%s / %s"), *GovGroupThousands(Profile.OutstandingDebt), *GovGroupThousands(Profile.MonthlyDebtService)),
			Profile.OutstandingDebt > 0 ? GovBad : GovMuted, GovCardAlt), 4.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Credito disponible"),
			GovGroupThousands(Profile.AvailableCredit), GovText, GovCard), 4.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Riesgo de default"),
			FString::Printf(TEXT("%.0f%%%s"), Profile.DefaultRisk * 100.0, Profile.bIMFEligible ? TEXT(" · elegible para FMI") : TEXT("")),
			Profile.DefaultRisk > 0.4 ? GovBad : GovMuted, GovCardAlt), 4.f);

		UHorizontalBox* FinActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddFinAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg)
		{
			if (UHorizontalBoxSlot* S = FinActions->AddChildToHorizontalBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 130.f)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
		};
		AddFinAction(TEXT("bond"), TEXT("EMITIR BONO"), GovGoldDim);
		if (Profile.bIMFEligible)
		{
			AddFinAction(TEXT("imf"), TEXT("PROGRAMA FMI"), GovTabIdle);
		}
		if (Profile.OutstandingDebt > 0 && !Profile.bInDefault)
		{
			AddFinAction(TEXT("default"), TEXT("DECLARAR DEFAULT"), FLinearColor(0.40f, 0.12f, 0.10f, 1.f));
		}
		AddColumnChild(CenterBox, FinActions, 10.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("El bono emite deuda a 24 meses segun tu rating. El FMI presta barato con condiciones. El default borra deuda pero hunde rating y orden."),
			12, GovMuted, ETextJustify::Left, true), 4.f);

		const TArray<FWLFinancialInstrumentState> Instruments = Tick->GetFinancialInstrumentsForNation(Iso);
		int32 ActiveCount = 0;
		for (const FWLFinancialInstrumentState& Inst : Instruments)
		{
			if (!Inst.IsActive())
			{
				continue;
			}
			if (ActiveCount == 0)
			{
				AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("INSTRUMENTOS ACTIVOS"), 13, GovMuted), 10.f);
			}
			AddColumnChild(CenterBox, MakeStatRow(WidgetTree, Inst.Title,
				FString::Printf(TEXT("%s restante · %s/mes · %d meses"),
					*GovGroupThousands(Inst.PrincipalRemaining), *GovGroupThousands(Inst.MonthlyPayment), Inst.RemainingMonths),
				GovMuted, (ActiveCount % 2 == 0) ? GovCard : GovCardAlt), 4.f);
			++ActiveCount;
		}

		const TArray<FWLForeignSupportState> Supports = Tick->GetForeignSupportForNation(Iso);
		int32 SupportCount = 0;
		for (const FWLForeignSupportState& Support : Supports)
		{
			if (Support.bCompleted)
			{
				continue;
			}
			if (SupportCount == 0)
			{
				AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("APOYOS EXTERNOS ACTIVOS"), 13, GovMuted), 10.f);
			}
			AddColumnChild(CenterBox, MakeStatRow(WidgetTree,
				FString::Printf(TEXT("%s -> %s"), *Support.SponsorIso, *Support.RecipientIso),
				FString::Printf(TEXT("%s/mes · %d meses"), *GovGroupThousands(Support.MonthlyAmount), Support.RemainingMonths),
				GovGood, (SupportCount % 2 == 0) ? GovCard : GovCardAlt), 4.f);
			++SupportCount;
		}
	}

	// FE6: gobernanza economica — ministro, corrupcion y tecnologia mueven la economia real.
	{
		const FWLEconomicGovernanceStats Gov = Tick->GetEconomicGovernanceStats(Iso);
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("GOBERNANZA ECONOMICA"), 17, GovGold), 20.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Ministro de Economia"),
			Gov.EconomyMinisterName.IsEmpty()
				? TEXT("Cargo vacante (nombra en ALTO MANDO)")
				: FString::Printf(TEXT("%s · skill %d"), *Gov.EconomyMinisterName, Gov.EconomyMinisterSkill),
			Gov.EconomyMinisterName.IsEmpty() ? GovGold : GovText, GovCard), 8.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Corrupcion sistemica"),
			FString::Printf(TEXT("%d/100 · skim %.1f%% (%s/mes)"),
				Gov.SystemicCorruption, Gov.CorruptionSkimRate * 100.0, *GovGroupThousands(Budget.CorruptionLoss)),
			Gov.SystemicCorruption >= 50 ? GovBad : GovMuted, GovCardAlt), 4.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Tecnologia / clima de inversion"),
			FString::Printf(TEXT("%d / %d"), Gov.TechnologyLevel, Gov.InvestmentClimate), GovText, GovCard), 4.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Eficiencia fiscal / productividad"),
			FString::Printf(TEXT("x%.2f / x%.2f"), Gov.TaxCollectionMultiplier, Gov.ProductivityMultiplier),
			GovText, GovCardAlt), 4.f);
	}

	// FE1.2: palanca de impuestos. Mover la tasa cambia la recaudacion (Laffer) y el orden publico mensual.
	{
		const int32 TaxRate = Tick->GetTaxRate(Iso);
		const double TaxMult = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(TaxRate, Rules);
		const int32 OrderPressure = Tick->GetTaxPublicOrderPressure(Iso);

		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("IMPUESTOS"), 17, GovGold), 20.f);

		UBorder* TaxCard = MakeBorder(WidgetTree, GovCard, FMargin(14.f, 11.f));
		UHorizontalBox* TaxRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UVerticalBox* TaxInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TaxInfo->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("TASA NACIONAL"), 12, GovMuted));
		if (UVerticalBoxSlot* S = TaxInfo->AddChildToVerticalBox(
			MakeText(WidgetTree, FString::Printf(TEXT("%d%%"), TaxRate), 27, GovGold)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = TaxInfo->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Recaudacion x%.2f   ·   Orden publico %+d/mes"), TaxMult, -OrderPressure),
			13, OrderPressure > 0 ? GovBad : GovMuted)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = TaxRow->AddChildToHorizontalBox(TaxInfo))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		auto AddTaxButton = [&](const TCHAR* Label, bool bUp)
		{
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			Button->SetBackgroundColor(GovTabIdle);
			if (bUp)
			{
				Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTaxUp);
			}
			else
			{
				Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTaxDown);
			}
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(52.f);
			Box->SetHeightOverride(46.f);
			Box->SetContent(MakeText(WidgetTree, Label, 20, GovText, ETextJustify::Center));
			Button->SetContent(Box);
			if (UHorizontalBoxSlot* S = TaxRow->AddChildToHorizontalBox(Button))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			}
		};
		AddTaxButton(TEXT("-"), false);
		AddTaxButton(TEXT("+"), true);

		TaxCard->SetContent(TaxRow);
		AddColumnChild(CenterBox, TaxCard, 10.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
			TEXT("Rango %d%%-%d%%. Subir impuestos recauda mas con rendimiento decreciente y drena orden publico cada mes."),
			Rules.TaxRateMinPercent, Rules.TaxRateMaxPercent), 12, GovMuted, ETextJustify::Left, true), 6.f);
	}
}

// F1.6/F1.7: ALTO MANDO — gabinete gestionable + generales con ascenso/baja/recompensa.
void UWLGovernmentWidget::BuildHighCommandTab()
{
	const FString Iso = PlayerIso();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos de personajes."), 14, GovMuted), 10.f);
		return;
	}

	const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("ALTO MANDO"), 17, GovGold), 6.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
		TEXT("Capital politico: %d   ·   Estabilidad: %d   ·   Corrupcion: %d   ·   Riesgo de golpe: %d"),
		Stats.PoliticalCapital, Stats.Stability, Stats.Corruption, Stats.CoupRisk),
		13, Stats.CoupRisk >= 50 ? GovBad : GovMuted, ETextJustify::Left, true), 4.f);

	// Gobierno P1: gabinete vivo — rivalidad, faccionalismo y riesgos de escandalo/sabotaje/renuncia.
	BuildCabinetDynamicsCard();

	// Gabinete: cada cargo con su ministro o vacante + nombrar/destituir + su efecto REAL en el juego.
	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("GABINETE"), 15, GovGold), 14.f);
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FWLBalanceRules Rules = Tick ? Tick->GetBalanceRules() : FWLBalanceRules::Default();
	auto MinisterEffectText = [&](EWLMinisterOffice Office, double Factor) -> FString
	{
		if (Factor == 0.0)
		{
			return TEXT("Sin efecto (cargo vacante)");
		}
		switch (Office)
		{
		case EWLMinisterOffice::Economy:
			return TEXT("Eficiencia fiscal y productividad (ver ECONOMIA)");
		case EWLMinisterOffice::Defense:
			return FString::Printf(TEXT("Upkeep militar %+.0f%%"), -Factor * Rules.DefenseMinisterUpkeepEffect * 100.0);
		case EWLMinisterOffice::Interior:
			return FString::Printf(TEXT("Orden publico %+.1f/mes por provincia"), Factor * Rules.InteriorMinisterOrderPerMonth);
		case EWLMinisterOffice::Foreign:
			return FString::Printf(TEXT("Opinion %+.1f/mes con cada pais"), Factor * Rules.ForeignMinisterOpinionPerMonth);
		case EWLMinisterOffice::Intelligence:
			return FString::Printf(TEXT("Skill de espias %+.0f"), Factor * Rules.IntelligenceMinisterSpyBonus);
		default:
			return FString();
		}
	};
	const TArray<FWLCabinetSeat> Cabinet = Characters->GetCabinet(Iso);
	int32 Index = 0;
	for (const FWLCabinetSeat& Seat : Cabinet)
	{
		const bool bFilled = Seat.Minister.IsValid();
		const double Factor = Characters->GetMinisterEffectFactor(Iso, Seat.Office);
		UBorder* Row = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Info->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Ministerio de %s"), *UWLCharacterSubsystem::MinisterOfficeToString(Seat.Office)), 14, GovText));
		Info->AddChildToVerticalBox(MakeText(WidgetTree,
			bFilled
				? FString::Printf(TEXT("%s — skill %d · lealtad %d · ambicion %d · popularidad %d"),
					*Seat.Minister.Name, Seat.Minister.Skill, Seat.Minister.Loyalty,
					Seat.Minister.Ambition, Seat.Minister.Popularity)
				: TEXT("Cargo vacante"),
			12, bFilled ? GovMuted : GovGold, ETextJustify::Left, true));
		if (bFilled && Seat.Minister.Traits.Num() > 0)
		{
			Info->AddChildToVerticalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("Rasgos: %s"), *FString::Join(Seat.Minister.Traits, TEXT(" · "))),
				11, GovGoldDim, ETextJustify::Left, true));
		}
		Info->AddChildToVerticalBox(MakeText(WidgetTree, MinisterEffectText(Seat.Office, Factor), 11,
			Factor < 0.0 ? GovBad : (Factor > 0.0 ? GovGood : GovMuted), ETextJustify::Left, true));
		// Gobierno P2: ficha politica del ministro (corrupcion personal, escandalo, sucesion).
		if (bFilled)
		{
			FWLCharacterPoliticalProfile Profile;
			if (const UWLPoliticalSubsystem* Political = GetPolitical();
				Political && Political->GetCharacterPoliticalProfile(Seat.CharacterId, Profile))
			{
				Info->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
					TEXT("Corrupcion personal %d · Escandalo %d · Ambicion presidencial %d"),
					Profile.PersonalCorruption, Profile.ScandalHeat, Profile.PresidentialAmbition),
					11, (Profile.ScandalHeat >= 50 || Profile.PersonalCorruption >= 50) ? GovBad : GovMuted,
					ETextJustify::Left, true));
			}
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Info))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		// Flujos separados: NOMBRAR (comparador de candidatos) / DESTITUIR (confirmar) / CONTRATAR (crear candidato).
		UVerticalBox* SeatActions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		auto AddSeatAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg)
		{
			if (UVerticalBoxSlot* S = SeatActions->AddChildToVerticalBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 118.f, 11)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
			}
		};
		AddSeatAction(FString::Printf(TEXT("opencompare:%d"), static_cast<int32>(Seat.Office)),
			bFilled ? TEXT("CANDIDATOS") : TEXT("NOMBRAR"), GovGoldDim);
		if (bFilled)
		{
			AddSeatAction(FString::Printf(TEXT("dismiss:%d"), static_cast<int32>(Seat.Office)),
				TEXT("DESTITUIR"), GovDanger);
		}
		else
		{
			AddSeatAction(FString::Printf(TEXT("hire:%d"), static_cast<int32>(Seat.Office)),
				TEXT("CONTRATAR"), GovTabIdle);
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(SeatActions))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, 4.f);

		// Comparador de candidatos abierto justo debajo de la cartera que se esta cubriendo.
		if (CompareOfficeContext == static_cast<int32>(Seat.Office))
		{
			BuildMinisterComparator(Seat.Office);
		}
		++Index;
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("NOMBRAR/CANDIDATOS abre el comparador (skill, lealtad, ambicion, riesgos). CONTRATAR trae un candidato nuevo a la cartera. Nombrar y destituir cuestan capital politico."),
		12, GovMuted, ETextJustify::Left, true), 4.f);

	// Generales: tarjeta por general con stats + acciones F1.7/F2.4.
	const TArray<FWLCharacter> Generals = Characters->GetGenerals(Iso);
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("GENERALES  (%d)"), Generals.Num()), 15, GovGold), 16.f);
	Index = 0;
	for (const FWLCharacter& General : Generals)
	{
		if (!General.bActive)
		{
			continue;
		}
		UBorder* Card = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UVerticalBox* GVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s — %s"), *General.Name, *RankToText(General.Rank)), 14, GovText, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			General.AssignedArmyId.IsEmpty() ? TEXT("Sin mando") : FString::Printf(TEXT("Ejercito %s"), *General.AssignedArmyId),
			11, GovMuted, ETextJustify::Right));
		GVB->AddChildToVerticalBox(Head);
		const FLinearColor LoyaltyColor = General.Loyalty < 40 ? GovBad : (General.Loyalty < 60 ? GovGold : GovGood);
		if (UVerticalBoxSlot* S = GVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Skill %d · Lealtad %d · Ambicion %d · Popularidad %d · Renombre %d"),
			General.Skill, General.Loyalty, General.Ambition, General.Popularity, General.Renown),
			12, LoyaltyColor, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}
		UWrapBox* Actions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		auto AddGeneralAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg)
		{
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Actions->AddChildToWrapBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 108.f))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 5.f));
			}
		};
		AddGeneralAction(FString::Printf(TEXT("promote:%s"), *General.Id), TEXT("ASCENDER"), GovGoldDim);
		AddGeneralAction(FString::Printf(TEXT("reward:%s"), *General.Id), TEXT("RECOMPENSAR"), GovTabIdle);
		AddGeneralAction(FString::Printf(TEXT("purge:%s"), *General.Id), TEXT("PURGAR"), FLinearColor(0.34f, 0.13f, 0.11f, 1.f));
		AddGeneralAction(FString::Printf(TEXT("retire:%s"), *General.Id), TEXT("DAR DE BAJA"), GovTabIdle);
		if (UVerticalBoxSlot* S = GVB->AddChildToVerticalBox(Actions))
		{
			S->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
		}
		Card->SetContent(GVB);
		AddColumnChild(CenterBox, Card, 5.f);
		++Index;
	}
	AddColumnChild(CenterBox, MakeActionButton(WidgetTree, this, TEXT("creategeneral"), TEXT("+ CREAR GENERAL"), GovGoldDim, 160.f, 13), 10.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Recompensar sube lealtad (cuesta tesoro). Purgar elimina al general y asusta al resto (-lealtad). Lealtad baja + ambicion alta = golpe."),
		12, GovMuted, ETextJustify::Left, true), 4.f);

	// Militar: ejercitos con general asignado, composicion, reservas y reorganizar.
	BuildArmiesSection();

	// Gobierno P2: fichas politicas de todos los personajes (facciones, sucesion, escandalos).
	BuildPoliticalProfilesSection();
}

// F3.6 + F4: DIPLOMACIA CONTINENTAL — 38 naciones exigen filtros, busqueda y orden; el listado
// es compacto y las acciones (tratados, guerra, intriga, FDI) viven en el panel del pais GESTIONADO.
void UWLGovernmentWidget::BuildDiplomacyTab()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	const UWLDataRegistry* Registry = GetRegistry();
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Political || !Registry || !Tick || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos diplomaticos."), 14, GovMuted), 10.f);
		return;
	}

	// Snapshot de relaciones para cabecera, filtros y orden.
	struct FDiploRow
	{
		FWLNationData Nation;
		FWLDiplomaticRelationState Relation;
		int64 Treasury = 0;
		int32 Provinces = 0;
	};
	TArray<FDiploRow> Rows;
	int32 WarCount = 0, AllyCount = 0, EmbargoCount = 0, TreatyCount = 0;
	for (const FWLNationData& Other : Registry->GetAllNations())
	{
		if (Other.Iso.Equals(Iso, ESearchCase::IgnoreCase))
		{
			continue;
		}
		FDiploRow Row;
		Row.Nation = Other;
		Political->GetRelation(Iso, Other.Iso, Row.Relation);
		Row.Treasury = Tick->GetTreasury(Other.Iso);
		Row.Provinces = Registry->GetProvincesByNation(Other.Iso).Num();
		if (Row.Relation.Status == EWLDiplomaticStatus::War) { ++WarCount; }
		if (Row.Relation.Treaties.Contains(EWLTreatyType::Alliance)) { ++AllyCount; }
		if (Row.Relation.Treaties.Contains(EWLTreatyType::Embargo)) { ++EmbargoCount; }
		TreatyCount += Row.Relation.Treaties.Num();
		Rows.Add(MoveTemp(Row));
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("DIPLOMACIA CONTINENTAL  (%d naciones)"), Rows.Num()), 17, GovGold), 6.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
		TEXT("En guerra %d · Alianzas %d · Embargos %d · Tratados vigentes %d"),
		WarCount, AllyCount, EmbargoCount, TreatyCount),
		13, WarCount > 0 ? GovBad : GovMuted, ETextJustify::Left, true), 4.f);

	// Buscador por nombre/ISO (Enter confirma; el filtrado ocurre al reconstruir).
	{
		UBorder* SearchCard = MakeBorder(WidgetTree, GovCard, FMargin(10.f, 7.f));
		UHorizontalBox* SearchRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = SearchRow->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("BUSCAR"), 11, GovMuted)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}
		UEditableTextBox* SearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		SearchBox->SetText(FText::FromString(DiplomacySearchText));
		SearchBox->SetHintText(FText::FromString(TEXT("Nombre o ISO (Enter para filtrar; vacio muestra todo)")));
		SearchBox->OnTextCommitted.AddDynamic(this, &UWLGovernmentWidget::OnDiplomacySearchCommitted);
		if (UHorizontalBoxSlot* S = SearchRow->AddChildToHorizontalBox(SearchBox))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (!DiplomacySearchText.IsEmpty())
		{
			if (UHorizontalBoxSlot* S = SearchRow->AddChildToHorizontalBox(
				MakeActionButton(WidgetTree, this, TEXT("dipsearchclear"), TEXT("LIMPIAR"), GovTabIdle, 80.f, 10)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			}
		}
		SearchCard->SetContent(SearchRow);
		AddColumnChild(CenterBox, SearchCard, 8.f);
	}

	// Filtros de estado + orden.
	{
		const struct { int32 Value; const TCHAR* Label; } Filters[] = {
			{ 0, TEXT("TODOS") }, { 1, TEXT("GUERRA") }, { 2, TEXT("TENSION") }, { 3, TEXT("PAZ") },
			{ 4, TEXT("ALIADOS") }, { 5, TEXT("CON TRATADO") }, { 6, TEXT("EMBARGO") } };
		UWrapBox* FilterChips = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		for (const auto& Def : Filters)
		{
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(FilterChips->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("dipfilter:%d"), Def.Value), Def.Label,
				DiplomacyStatusFilter == Def.Value ? GovGoldDim : GovTabIdle, 0.f, 10))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 4.f, 4.f));
			}
		}
		AddColumnChild(CenterBox, FilterChips, 6.f);

		const struct { int32 Value; const TCHAR* Label; } Sorts[] = {
			{ 0, TEXT("A-Z") }, { 1, TEXT("OPINION +") }, { 2, TEXT("OPINION -") },
			{ 3, TEXT("TESORO") }, { 4, TEXT("PROVINCIAS") }, { 5, TEXT("ESTADO") } };
		UWrapBox* SortChips = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(SortChips->AddChildToWrapBox(MakeText(WidgetTree, TEXT("ORDENAR:"), 10, GovMuted))))
		{
			S->SetPadding(FMargin(0.f, 6.f, 6.f, 0.f));
		}
		for (const auto& Def : Sorts)
		{
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(SortChips->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("dipsort:%d"), Def.Value), Def.Label,
				DiplomacySortMode == Def.Value ? GovGoldDim : GovTabIdle, 0.f, 10))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 4.f, 4.f));
			}
		}
		AddColumnChild(CenterBox, SortChips, 2.f);
	}

	// Aplica busqueda + filtro.
	const FString Needle = DiplomacySearchText.TrimStartAndEnd();
	Rows.RemoveAll([&](const FDiploRow& Row)
	{
		if (!Needle.IsEmpty()
			&& !Row.Nation.Name.Contains(Needle, ESearchCase::IgnoreCase)
			&& !Row.Nation.Iso.Contains(Needle, ESearchCase::IgnoreCase))
		{
			return true;
		}
		switch (DiplomacyStatusFilter)
		{
		case 1: return Row.Relation.Status != EWLDiplomaticStatus::War;
		case 2: return Row.Relation.Status != EWLDiplomaticStatus::Tension;
		case 3: return Row.Relation.Status != EWLDiplomaticStatus::Peace;
		case 4: return !Row.Relation.Treaties.Contains(EWLTreatyType::Alliance);
		case 5: return Row.Relation.Treaties.Num() == 0;
		case 6: return !Row.Relation.Treaties.Contains(EWLTreatyType::Embargo);
		default: return false;
		}
	});

	// Orden.
	Rows.Sort([this](const FDiploRow& A, const FDiploRow& B)
	{
		switch (DiplomacySortMode)
		{
		case 1: return A.Relation.Opinion > B.Relation.Opinion;
		case 2: return A.Relation.Opinion < B.Relation.Opinion;
		case 3: return A.Treasury > B.Treasury;
		case 4: return A.Provinces > B.Provinces;
		case 5: return static_cast<int32>(A.Relation.Status) > static_cast<int32>(B.Relation.Status);
		default: return A.Nation.Name < B.Nation.Name;
		}
	});

	if (Rows.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Ningun pais cumple el filtro/busqueda actual."), 13, GovMuted), 10.f);
		return;
	}

	// Panel de gestion del pais seleccionado (arriba, con todas las acciones).
	for (const FDiploRow& Row : Rows)
	{
		if (Row.Nation.Iso.Equals(SelectedDiplomacyIso, ESearchCase::IgnoreCase))
		{
			BuildDiplomacyDetailPanel(Row.Nation);
			break;
		}
	}

	// Listado compacto: una fila por pais, GESTIONAR abre/cierra su panel.
	int32 Index = 0;
	for (const FDiploRow& Row : Rows)
	{
		const bool bSelected = Row.Nation.Iso.Equals(SelectedDiplomacyIso, ESearchCase::IgnoreCase);
		const bool bAtWar = Row.Relation.Status == EWLDiplomaticStatus::War;
		UBorder* Card = MakeBorder(WidgetTree, bSelected ? GovHeaderStrip : ((Index % 2 == 0) ? GovCard : GovCardAlt), FMargin(10.f, 6.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		USizeBox* EmblemBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		EmblemBox->SetWidthOverride(20.f);
		EmblemBox->SetHeightOverride(20.f);
		EmblemBox->SetContent(MakeBorder(WidgetTree, Row.Nation.MapColor, FMargin(0.f)));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(EmblemBox)) { S->SetVerticalAlignment(VAlign_Center); }

		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Info->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s (%s)"), *Row.Nation.Name, *Row.Nation.Iso), 13, GovText));
		FString TreatyLine;
		for (const EWLTreatyType Treaty : Row.Relation.Treaties)
		{
			TreatyLine += (TreatyLine.IsEmpty() ? TEXT("") : TEXT(" · ")) + TreatyToText(Treaty);
		}
		Info->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("%s · Tesoro %s · %d prov"),
			TreatyLine.IsEmpty() ? TEXT("Sin tratados") : *TreatyLine,
			*GovGroupThousands(Row.Treasury), Row.Provinces),
			10, GovMuted, ETextJustify::Left, true));
		UBorder* InfoPad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(8.f, 0.f, 0.f, 0.f));
		InfoPad->SetContent(Info);
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(InfoPad))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		const FLinearColor StatusColor = bAtWar ? GovBad
			: (Row.Relation.Status == EWLDiplomaticStatus::Tension ? GovGold : GovGood);
		USizeBox* StatusBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		StatusBox->SetWidthOverride(150.f);
		StatusBox->SetContent(MakeText(WidgetTree, FString::Printf(
			TEXT("%s · %+d"), *DiplomaticStatusToText(Row.Relation.Status), Row.Relation.Opinion),
			12, StatusColor, ETextJustify::Right));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(StatusBox))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}

		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("dipsel:%s"), *Row.Nation.Iso),
			bSelected ? TEXT("CERRAR") : TEXT("GESTIONAR"),
			bSelected ? GovTabIdle : GovGoldDim, 96.f, 10)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		Card->SetContent(HB);
		AddColumnChild(CenterBox, Card, 3.f);
		++Index;
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("GESTIONAR abre el panel del pais con tratados, guerra, ayuda, inversion e intriga. Sin guerra declarada no hay combate."),
		12, GovMuted, ETextJustify::Left, true), 8.f);
}

// Panel de gestion de UN pais: relacion, tratados, rutas, acciones, FDI e intriga.
void UWLGovernmentWidget::BuildDiplomacyDetailPanel(const FWLNationData& Other)
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	const UWLDataRegistry* Registry = GetRegistry();
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Political || !Registry || !Tick)
	{
		return;
	}

	FWLDiplomaticRelationState Relation;
	Political->GetRelation(Iso, Other.Iso, Relation);
	const FWLTradeRouteState Route = Tick->GetTradeRouteBetween(Iso, Other.Iso);
	const FWLIntelligenceNetworkState Network = Political->GetIntelligenceNetwork(Iso, Other.Iso);
	const bool bAtWar = Relation.Status == EWLDiplomaticStatus::War;
	const FString SpyId = FindPlayerSpyId();

	UBorder* Panel = MakeBorder(WidgetTree, GovPanelSoft, FMargin(12.f, 10.f));
	UVerticalBox* NVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	// Cabecera: nombre + estado + opinion.
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* EmblemBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	EmblemBox->SetWidthOverride(26.f);
	EmblemBox->SetHeightOverride(26.f);
	EmblemBox->SetContent(MakeBorder(WidgetTree, Other.MapColor, FMargin(0.f)));
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(EmblemBox)) { S->SetVerticalAlignment(VAlign_Center); }
	UBorder* NamePad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(9.f, 0.f, 0.f, 0.f));
	NamePad->SetContent(MakeText(WidgetTree, Other.Name.ToUpper(), 15, GovText));
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(NamePad))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	const FLinearColor StatusColor = bAtWar ? GovBad : (Relation.Status == EWLDiplomaticStatus::Tension ? GovGold : GovGood);
	Head->AddChildToHorizontalBox(MakeText(WidgetTree,
		FString::Printf(TEXT("%s · Opinion %+d"), *DiplomaticStatusToText(Relation.Status), Relation.Opinion),
		13, StatusColor, ETextJustify::Right));
	NVB->AddChildToVerticalBox(Head);

	// Tratados + ruta comercial (con multiplicador y razon de bloqueo).
	FString TreatyLine;
	for (const EWLTreatyType Treaty : Relation.Treaties)
	{
		TreatyLine += (TreatyLine.IsEmpty() ? TEXT("") : TEXT(" · ")) + TreatyToText(Treaty);
	}
	if (TreatyLine.IsEmpty())
	{
		TreatyLine = TEXT("Sin tratados");
	}
	if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
		TEXT("%s   ·   Ruta comercial: %s (x%.2f)"),
		*TreatyLine, Route.bOpen ? TEXT("abierta") : *FString::Printf(TEXT("CERRADA — %s"), *Route.Reason), Route.AccessMultiplier),
		12, Route.bOpen ? GovMuted : GovBad, ETextJustify::Left, true)))
	{
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}
	if (!Relation.CasusBelli.IsEmpty())
	{
		NVB->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Casus belli: %s"), *Relation.CasusBelli), 12, GovGold, ETextJustify::Left, true));
	}
	NVB->AddChildToVerticalBox(MakeText(WidgetTree,
		bAtWar ? TEXT("EN GUERRA: tus ejercitos pueden atacar; las rutas mutuas estan cortadas.")
		       : TEXT("Sin guerra declarada: el combate contra este pais esta bloqueado."),
		11, bAtWar ? GovBad : GovMuted, ETextJustify::Left, true));

	// Acciones diplomaticas (todas con confirmacion en dos clics).
	UWrapBox* Actions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	auto AddDiploAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg)
	{
		if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Actions->AddChildToWrapBox(
			MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 0.f, 11))))
		{
			S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
		}
	};
	auto HasTreaty = [&Relation](EWLTreatyType Type) { return Relation.Treaties.Contains(Type); };
	if (bAtWar)
	{
		AddDiploAction(FString::Printf(TEXT("peace:%s"), *Other.Iso), TEXT("NEGOCIAR PAZ"), GovGoldDim);
	}
	else
	{
		AddDiploAction(FString::Printf(TEXT("war:%s"), *Other.Iso), TEXT("DECLARAR GUERRA"), GovDanger);
	}
	const struct { EWLTreatyType Type; const TCHAR* Label; } TreatyDefs[] = {
		{ EWLTreatyType::TradeAgreement, TEXT("COMERCIO") },
		{ EWLTreatyType::NonAggression,  TEXT("NO AGRESION") },
		{ EWLTreatyType::Alliance,       TEXT("ALIANZA") },
		{ EWLTreatyType::Embargo,        TEXT("EMBARGO") },
	};
	for (const auto& Def : TreatyDefs)
	{
		if (HasTreaty(Def.Type))
		{
			AddDiploAction(FString::Printf(TEXT("breaktreaty:%d:%s"), static_cast<int32>(Def.Type), *Other.Iso),
				FString::Printf(TEXT("ROMPER %s"), Def.Label), GovTabIdle);
		}
		else
		{
			AddDiploAction(FString::Printf(TEXT("treaty:%d:%s"), static_cast<int32>(Def.Type), *Other.Iso),
				Def.Label, GovGoldDim);
		}
	}
	AddDiploAction(FString::Printf(TEXT("aid:%s"), *Other.Iso), TEXT("AYUDA"), GovTabIdle);
	// FE5.3: FDI con selector — cada candidato (provincia+edificio construible del vecino) es un boton.
	{
		int32 FdiShown = 0;
		for (const FWLProvinceData& TargetProvince : Registry->GetProvincesByNation(Other.Iso))
		{
			if (FdiShown >= 2)
			{
				break;
			}
			for (const FWLBuildingData& Candidate : Registry->GetAllBuildings())
			{
				if (!Tick->IsBuildingSupportedInProvince(TargetProvince.Id, Candidate.Id)
					|| Tick->GetProvinceBuildingLevel(TargetProvince.Id, Candidate.Id) > 0)
				{
					continue;
				}
				AddDiploAction(FString::Printf(TEXT("fdi:%s:%s:%s"), *Other.Iso, *TargetProvince.Id, *Candidate.Id),
					FString::Printf(TEXT("INVERTIR: %s en %s"), *Candidate.Name, *TargetProvince.Name), GovFuture);
				++FdiShown;
				break;   // un candidato por provincia
			}
		}
	}
	if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(Actions))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	// F4: intriga contra este pais.
	if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
		TEXT("INTELIGENCIA — red %d · exposicion %d%s"),
		Network.NetworkStrength, Network.Exposure,
		Network.LastOperationReport.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  ·  %s"), *Network.LastOperationReport)),
		12, Network.Exposure >= 60 ? GovBad : GovMuted, ETextJustify::Left, true)))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	if (SpyId.IsEmpty())
	{
		if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(MakeText(WidgetTree,
			TEXT("Sin espias activos disponibles."), 12, GovMuted)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	}
	else
	{
		UWrapBox* SpyActions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		auto AddSpyAction = [&](const FString& ActionId, const FString& Label)
		{
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(SpyActions->AddChildToWrapBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, GovFuture, 0.f, 11))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
			}
		};
		AddSpyAction(FString::Printf(TEXT("spynet:%s"), *Other.Iso), TEXT("RED +"));
		AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::SabotageEconomy), *Other.Iso), TEXT("SABOTEAR ECO"));
		AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::SabotageArmy), *Other.Iso), TEXT("SABOTEAR EJERCITO"));
		AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::FundCoup), *Other.Iso), TEXT("FINANCIAR GOLPE"));
		AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::Propaganda), *Other.Iso), TEXT("PROPAGANDA"));
		AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::CounterIntelligence), *Other.Iso), TEXT("CONTRAESP."));
		if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(SpyActions))
		{
			S->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
		}
		NVB->AddChildToVerticalBox(MakeText(WidgetTree,
			TEXT("La intriga sube tu exposicion; si te descubren, la relacion se hunde y hay incidente diplomatico."),
			11, GovMuted, ETextJustify::Left, true));
	}

	Panel->SetContent(NVB);
	AddColumnChild(CenterBox, Panel, 8.f);
}

void UWLGovernmentWidget::OpenProvince(const FString& ProvinceId)
{
	ProvinceContextId = ProvinceId.TrimStartAndEnd().ToUpper();
	SetActiveTab(EWLGovernmentTab::Province);
}

// Panel de slots de edificios de la provincia (contrato "edificios provinciales"): nivel, upgrade,
// upkeep y efectos reales; construir/mejorar cobra del tesoro via BuildBuilding/UpgradeBuilding.
void UWLGovernmentWidget::BuildProvinceTab()
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	const UWLDataRegistry* Registry = GetRegistry();
	FWLProvinceData Province;
	if (!Tick || !Registry || !Registry->GetProvince(ProvinceContextId, Province))
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Selecciona una provincia en el mapa y pulsa [B]."), 14, GovMuted), 10.f);
		return;
	}

	const FString ControllerIso = Tick->GetProvinceControllerIso(Province.Id);
	const bool bOwn = ControllerIso.Equals(PlayerIso(), ESearchCase::IgnoreCase);

	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("PROVINCIA: %s (%s)"), *Province.Name.ToUpper(), *Province.Id), 17, GovGold), 6.f);
	FWLProvinceRuntimeState State;
	Tick->GetProvinceState(Province.Id, State);
	AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
		TEXT("Control: %s   ·   Poblacion: %s   ·   Orden publico: %d   ·   Balance: %+lld/mes"),
		*ControllerIso, *GovGroupThousands(State.Population), State.PublicOrder,
		static_cast<long long>(Tick->GetProvinceMonthlyBalance(Province.Id))),
		13, GovMuted, ETextJustify::Left, true), 4.f);
	if (!bOwn)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Provincia bajo control extranjero: solo lectura."), 12, GovBad), 4.f);
	}

	// Texto de efectos de un edificio (solo los bonus distintos de cero).
	auto EffectsText = [](const FWLBuildingData& B) -> FString
	{
		FString Out;
		auto Add = [&Out](const TCHAR* Label, int64 Value)
		{
			if (Value != 0)
			{
				Out += FString::Printf(TEXT("%s%s %+lld"), Out.IsEmpty() ? TEXT("") : TEXT(" · "), Label, static_cast<long long>(Value));
			}
		};
		Add(TEXT("Oil"), B.BonusOil); Add(TEXT("Gas"), B.BonusGas); Add(TEXT("Food"), B.BonusFood);
		Add(TEXT("Min"), B.BonusMinerals); Add(TEXT("Ind"), B.BonusIndustry);
		Add(TEXT("Finanzas"), B.BonusFinancialIncome); Add(TEXT("Infra"), B.BonusInfrastructure);
		Add(TEXT("Orden"), B.BonusPublicOrder); Add(TEXT("Recluta"), B.BonusRecruitmentCapacity);
		Add(TEXT("Poder mil."), B.BonusMilitaryPower); Add(TEXT("Defensa"), B.BonusDefense);
		Add(TEXT("Aereo"), B.BonusAirCapacity); Add(TEXT("Naval"), B.BonusNavalCapacity);
		Add(TEXT("Tec"), B.BonusTechnology);
		return Out.IsEmpty() ? TEXT("Sin efectos directos") : Out;
	};
	auto SlotToText = [](EWLBuildingSlot BuildingSlot) -> FString
	{
		switch (BuildingSlot)
		{
		case EWLBuildingSlot::Economic:       return TEXT("ECONOMICO");
		case EWLBuildingSlot::Industrial:     return TEXT("INDUSTRIAL");
		case EWLBuildingSlot::Military:       return TEXT("MILITAR");
		case EWLBuildingSlot::Naval:          return TEXT("NAVAL");
		case EWLBuildingSlot::Air:            return TEXT("AEREO");
		case EWLBuildingSlot::Tech:           return TEXT("TECNOLOGICO");
		case EWLBuildingSlot::Financial:      return TEXT("FINANCIERO");
		case EWLBuildingSlot::Infrastructure: return TEXT("INFRAESTRUCTURA");
		case EWLBuildingSlot::Defensive:      return TEXT("DEFENSIVO");
		default:                              return TEXT("SLOT");
		}
	};

	const TArray<FString> Built = Tick->GetProvinceBuildings(Province.Id);
	const TArray<FWLBuildingData> AllBuildings = Registry->GetAllBuildings();
	const EWLBuildingSlot Slots[] = {
		EWLBuildingSlot::Economic, EWLBuildingSlot::Industrial, EWLBuildingSlot::Military,
		EWLBuildingSlot::Naval, EWLBuildingSlot::Air, EWLBuildingSlot::Tech,
		EWLBuildingSlot::Financial, EWLBuildingSlot::Infrastructure, EWLBuildingSlot::Defensive };

	int32 Index = 0;
	for (const EWLBuildingSlot BuildingSlot : Slots)
	{
		// Edificio construido de este slot (si hay).
		FWLBuildingData BuiltBuilding;
		bool bHasBuilt = false;
		for (const FString& BuildingId : Built)
		{
			FWLBuildingData Candidate;
			if (Registry->GetBuilding(BuildingId, Candidate) && Candidate.Slot == BuildingSlot)
			{
				BuiltBuilding = Candidate;
				bHasBuilt = true;
				break;
			}
		}

		UBorder* Card = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UVerticalBox* SVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		if (bHasBuilt)
		{
			const int32 Level = Tick->GetProvinceBuildingLevel(Province.Id, BuiltBuilding.Id);
			UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("%s — %s"), *SlotToText(BuildingSlot), *BuiltBuilding.Name), 14, GovText)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			Head->AddChildToHorizontalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("Nv %d/%d"), Level, BuiltBuilding.MaxLevel), 14, GovGold, ETextJustify::Right));
			SVB->AddChildToVerticalBox(Head);
			SVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(TEXT("%s   ·   Upkeep %s/mes"),
				*EffectsText(BuiltBuilding), *GovGroupThousands(BuiltBuilding.MonthlyUpkeep * Level)),
				11, GovMuted, ETextJustify::Left, true));
			if (bOwn && Level < BuiltBuilding.MaxLevel)
			{
				UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
					FString::Printf(TEXT("upgrade:%s"), *BuiltBuilding.Id),
					FString::Printf(TEXT("MEJORAR a Nv %d (%s)"), Level + 1,
						*GovGroupThousands(Tick->GetProvinceBuildingUpgradeCost(Province.Id, BuiltBuilding.Id))),
					GovGoldDim, 0.f, 12)))
				{
					S->SetVerticalAlignment(VAlign_Center);
				}
				if (UVerticalBoxSlot* S = SVB->AddChildToVerticalBox(Row))
				{
					S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
				}
			}
		}
		else
		{
			SVB->AddChildToVerticalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("%s — slot vacio"), *SlotToText(BuildingSlot)), 14, GovMuted));
			// Opciones construibles de este slot con base economica en la provincia.
			UWrapBox* Options = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
			int32 Shown = 0;
			for (const FWLBuildingData& Candidate : AllBuildings)
			{
				if (Candidate.Slot != BuildingSlot || !Tick->IsBuildingSupportedInProvince(Province.Id, Candidate.Id))
				{
					continue;
				}
				if (bOwn)
				{
					if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Options->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
						FString::Printf(TEXT("build:%s"), *Candidate.Id),
						FString::Printf(TEXT("CONSTRUIR %s (%s)"), *Candidate.Name, *GovGroupThousands(Candidate.Cost)),
						GovTabIdle, 0.f, 11))))
					{
						S->SetPadding(FMargin(0.f, 4.f, 5.f, 0.f));
					}
				}
				if (++Shown >= 3)
				{
					break;
				}
			}
			if (Shown == 0)
			{
				SVB->AddChildToVerticalBox(MakeText(WidgetTree,
					TEXT("Sin edificios compatibles con la base economica de esta provincia."), 11, GovMuted, ETextJustify::Left, true));
			}
			else if (bOwn)
			{
				SVB->AddChildToVerticalBox(Options);
			}
		}

		Card->SetContent(SVB);
		AddColumnChild(CenterBox, Card, 5.f);
		++Index;
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Construir/mejorar cobra del tesoro (endeuda hasta el limite de credito). Los efectos entran al ingreso, orden y defensa de la provincia."),
		12, GovMuted, ETextJustify::Left, true), 8.f);
}

void UWLGovernmentWidget::BuildRecordsTab()
{
	const UWLStrategicTickSubsystem* Tick = GetTick();

	// Feed de noticias del mundo (guerras, golpes, espias detectados, IA...). Nuevas primero.
	if (const UWLPoliticalSubsystem* Political = GetPolitical())
	{
		const TArray<FString> News = Political->GetNewsLog();
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("NOTICIAS DEL MUNDO"), 17, GovGold), 6.f);
		if (News.Num() == 0)
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree,
				TEXT("Sin noticias todavia. El mundo se mueve al avanzar los meses."), 13, GovMuted, ETextJustify::Left, true), 8.f);
		}
		int32 NewsIndex = 0;
		for (const FString& Item : News)
		{
			if (NewsIndex >= 15)
			{
				break;
			}
			UBorder* Row = MakeBorder(WidgetTree, (NewsIndex % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 7.f));
			Row->SetContent(MakeText(WidgetTree, Item, 13, GovText, ETextJustify::Left, true));
			AddColumnChild(CenterBox, Row, 4.f);
			++NewsIndex;
		}
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("REGISTROS RECIENTES"), 17, GovGold), 18.f);

	const TArray<FString> Reports = Tick ? Tick->GetLastEconomicAIReports() : TArray<FString>();
	if (Reports.Num() > 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Actividad economica de la IA:"), 13, GovMuted), 10.f);
		int32 Index = 0;
		for (const FString& R : Reports)
		{
			UBorder* Row = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
			Row->SetContent(MakeText(WidgetTree, R, 14, GovText, ETextJustify::Left, true));
			AddColumnChild(CenterBox, Row, 5.f);
			++Index;
		}
	}
	else
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Aun no hay eventos. Avanza el tiempo con [M] o el boton AVANZAR DIA."), 14, GovMuted, ETextJustify::Left, true), 10.f);
	}

	// Gobierno P2: que persigue cada gobierno IA de America y por que.
	BuildAIPlansPanel();

	// Telemetria de dilemas para playtest/calibracion (herramienta de debug, no pantalla de jugador).
	BuildCalibrationPanel();
}

void UWLGovernmentWidget::SetActiveTab(EWLGovernmentTab Tab)
{
	ActiveTab = Tab;
	LastActionMessage.Reset();   // el feedback de acciones es del tab donde ocurrio
	PendingConfirmId.Reset();    // cambiar de tab cancela cualquier confirmacion pendiente
	CompareOfficeContext = -1;   // y cierra el comparador de candidatos
	BattleAttackerId.Reset();    // y cierra el preview de combate
	BattleDefenderId.Reset();
	bDraftAgendaLoaded = false;  // AGENDA vuelve a leer las prioridades reales del backend
	RefreshTabButtonStyles();
	RebuildCenter();
}

void UWLGovernmentWidget::RefreshTabButtonStyles()
{
	for (int32 i = 0; i < TabButtons.Num(); ++i)
	{
		if (TabButtons[i])
		{
			TabButtons[i]->SetBackgroundColor(static_cast<int32>(ActiveTab) == i ? GovGoldDim : GovTabIdle);
		}
	}
}

void UWLGovernmentWidget::OnTabOverview()    { SetActiveTab(EWLGovernmentTab::Overview); }
void UWLGovernmentWidget::OnTabEconomy()     { SetActiveTab(EWLGovernmentTab::Economy); }
void UWLGovernmentWidget::OnTabHighCommand() { SetActiveTab(EWLGovernmentTab::HighCommand); }
void UWLGovernmentWidget::OnTabPolitics()    { SetActiveTab(EWLGovernmentTab::Politics); }
void UWLGovernmentWidget::OnTabDiplomacy()   { SetActiveTab(EWLGovernmentTab::Diplomacy); }
void UWLGovernmentWidget::OnTabRecords()     { SetActiveTab(EWLGovernmentTab::Records); }

// Dispatcher central: todos los UWLGovActionButton llegan aqui con "verbo[:arg1[:arg2]]".
// Cada rama llama al endpoint backend correspondiente y refresca el tab con el mensaje resultante.
void UWLGovernmentWidget::HandleAction(const FString& ActionId)
{
	TArray<FString> Parts;
	ActionId.ParseIntoArray(Parts, TEXT(":"), true);
	if (Parts.Num() == 0)
	{
		return;
	}
	const FString Verb = Parts[0];
	const FString Arg1 = Parts.Num() > 1 ? Parts[1] : FString();
	const FString Arg2 = Parts.Num() > 2 ? Parts[2] : FString();

	const FString Iso = PlayerIso();
	UWLStrategicTickSubsystem* Tick = GetTick();
	UWLCharacterSubsystem* Characters = GetCharacters();
	UWLPoliticalSubsystem* Political = GetPolitical();

	// --- Navegacion interna de la UI (sin backend y sin franja de feedback) ---
	if (Verb == TEXT("polsec"))
	{
		PoliticsSection = static_cast<EWLPoliticsSection>(FCString::Atoi(*Arg1));
		PendingConfirmId.Reset();
		LastActionMessage.Reset();
		RebuildCenter();
		return;
	}
	if (Verb == TEXT("progoffice") || Verb == TEXT("reformarea") || Verb == TEXT("profsort"))
	{
		const int32 Value = FCString::Atoi(*Arg1);
		if (Verb == TEXT("progoffice"))      { ProgramOfficeFilter = Value; }
		else if (Verb == TEXT("reformarea")) { ReformAreaFilter = Value; }
		else                                 { ProfileSortMode = Value; }
		PendingConfirmId.Reset();
		RebuildCenter();
		return;
	}
	if (Verb == TEXT("opencompare") || Verb == TEXT("closecompare"))
	{
		CompareOfficeContext = Verb == TEXT("opencompare") ? FCString::Atoi(*Arg1) : -1;
		PendingConfirmId.Reset();
		LastActionMessage.Reset();
		RebuildCenter();
		return;
	}
	if (Verb == TEXT("dipsel"))
	{
		// GESTIONAR alterna el panel de detalle del pais en DIPLOMACIA.
		SelectedDiplomacyIso = SelectedDiplomacyIso.Equals(Arg1, ESearchCase::IgnoreCase) ? FString() : Arg1;
		PendingConfirmId.Reset();
		LastActionMessage.Reset();
		RebuildCenter();
		return;
	}
	if (Verb == TEXT("dipfilter") || Verb == TEXT("dipsort") || Verb == TEXT("dipsearchclear"))
	{
		if (Verb == TEXT("dipfilter"))      { DiplomacyStatusFilter = FCString::Atoi(*Arg1); }
		else if (Verb == TEXT("dipsort"))   { DiplomacySortMode = FCString::Atoi(*Arg1); }
		else                                { DiplomacySearchText.Reset(); }
		PendingConfirmId.Reset();
		RebuildCenter();
		return;
	}
	if (Verb == TEXT("battlepick") || Verb == TEXT("battlecancel"))
	{
		// battlepick:<attackerId>:<defenderId> abre el preview; battlecancel lo cierra.
		if (Verb == TEXT("battlepick"))
		{
			BattleAttackerId = Arg1;
			BattleDefenderId = Arg2;
		}
		else
		{
			BattleAttackerId.Reset();
			BattleDefenderId.Reset();
		}
		PendingConfirmId.Reset();
		LastActionMessage.Reset();
		RebuildCenter();
		return;
	}
	if (Verb == TEXT("agendatoggle"))
	{
		const EWLGovernmentPriority AgendaPriority = static_cast<EWLGovernmentPriority>(FCString::Atoi(*Arg1));
		PendingConfirmId.Reset();
		if (DraftAgenda.Contains(AgendaPriority))
		{
			DraftAgenda.Remove(AgendaPriority);
			LastActionMessage.Reset();
		}
		else if (DraftAgenda.Num() >= 3)
		{
			LastActionMessage = TEXT("Maximo 3 prioridades: quita una antes de anadir otra.");
			bLastActionSucceeded = false;
		}
		else
		{
			DraftAgenda.Add(AgendaPriority);
			LastActionMessage.Reset();
		}
		RebuildCenter();
		return;
	}

	// --- Confirmacion en dos clics para acciones sensibles ---
	// El primer clic deja la accion pendiente y repinta su boton como "CONFIRMAR?";
	// el segundo clic (mismo boton) ejecuta. Cualquier otro clic cancela la pendiente.
	auto RequiresConfirm = [&]() -> bool
	{
		static const TCHAR* AlwaysConfirm[] = {
			TEXT("war"), TEXT("peace"), TEXT("treaty"), TEXT("breaktreaty"), TEXT("aid"), TEXT("fdi"),
			TEXT("reward"), TEXT("purge"), TEXT("repress"), TEXT("event"), TEXT("retire"),
			TEXT("dismiss"), TEXT("appointc"), TEXT("hire"),
			TEXT("bond"), TEXT("imf"), TEXT("default"),
			TEXT("agendaset"), TEXT("program"), TEXT("reform"), TEXT("promise"),
			TEXT("negotiate"), TEXT("patronage"),
			TEXT("autoresolve"), TEXT("tacticalresolve") };
		for (const TCHAR* Confirmable : AlwaysConfirm)
		{
			if (Verb == Confirmable)
			{
				return true;
			}
		}
		if (Verb == TEXT("media"))
		{
			const EWLMediaActionType Action = static_cast<EWLMediaActionType>(FCString::Atoi(*Arg1));
			return Action == EWLMediaActionType::Propaganda || Action == EWLMediaActionType::Censorship;
		}
		if (Verb == TEXT("region"))
		{
			const EWLRegionPolicyActionType Action = static_cast<EWLRegionPolicyActionType>(FCString::Atoi(*Arg1));
			return Action == EWLRegionPolicyActionType::AppointGovernor
				|| Action == EWLRegionPolicyActionType::SecurityOperation;
		}
		if (Verb == TEXT("spy"))
		{
			return static_cast<EWLSpyOperationType>(FCString::Atoi(*Arg1)) == EWLSpyOperationType::FundCoup;
		}
		return false;
	};
	if (RequiresConfirm())
	{
		if (!PendingConfirmId.Equals(ActionId))
		{
			PendingConfirmId = ActionId;
			LastActionMessage = TEXT("Accion sensible: pulsa el boton naranja CONFIRMAR? para ejecutarla. Cualquier otro clic cancela.");
			bLastActionSucceeded = true;
			RebuildCenter();
			return;
		}
		PendingConfirmId.Reset();
	}
	else
	{
		PendingConfirmId.Reset();
	}

	FString Message;
	bool bOk = false;

	if (Verb == TEXT("tariffup") || Verb == TEXT("tariffdown"))
	{
		if (Political && Tick)
		{
			const int32 Delta = Verb == TEXT("tariffup") ? 5 : -5;
			bOk = Political->SetNationTariffRate(Iso, Tick->GetTariffRate(Iso) + Delta, Message);
		}
	}
	else if (Verb == TEXT("creategeneral"))
	{
		FWLCharacter NewGeneral;
		bOk = Characters && Characters->CreateGeneral(Iso, NewGeneral, Message);
	}
	else if (Verb == TEXT("promote"))
	{
		bOk = Characters && Characters->PromoteGeneral(Arg1, Message);
	}
	else if (Verb == TEXT("retire"))
	{
		bOk = Characters && Characters->RetireCharacter(Arg1, Message);
	}
	else if (Verb == TEXT("reward"))
	{
		bOk = Political && Political->RewardGeneral(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("purge"))
	{
		bOk = Political && Political->PurgeCharacter(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("repress"))
	{
		bOk = Political && Political->RepressOpposition(Iso, Message);
	}
	else if (Verb == TEXT("appoint"))
	{
		// Elige al mejor candidato disponible: rol ministro, activo y sin cargo; prefiere su cartera, luego skill.
		if (Characters)
		{
			const EWLMinisterOffice Office = static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1));
			TArray<FWLCharacter> Candidates = Characters->GetCharactersByRole(Iso, EWLCharacterRole::Minister);
			Candidates.RemoveAll([](const FWLCharacter& C)
			{
				return !C.bActive || C.AssignedOffice != EWLMinisterOffice::None;
			});
			Candidates.Sort([Office](const FWLCharacter& A, const FWLCharacter& B)
			{
				const bool bAPref = A.PreferredOffice == Office;
				const bool bBPref = B.PreferredOffice == Office;
				if (bAPref != bBPref)
				{
					return bAPref;
				}
				return A.Skill > B.Skill;
			});
			if (Candidates.Num() > 0)
			{
				bOk = Characters->AppointMinister(Iso, Office, Candidates[0].Id, Message);
			}
			else
			{
				Message = TEXT("No hay candidatos a ministro disponibles.");
			}
		}
	}
	else if (Verb == TEXT("dismiss"))
	{
		bOk = Characters && Characters->DismissMinister(Iso, static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("war"))
	{
		bOk = Political && Political->DeclareWar(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("peace"))
	{
		bOk = Political && Political->MakePeace(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("treaty"))
	{
		bOk = Political && Political->SignTreaty(Iso, Arg2, static_cast<EWLTreatyType>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("breaktreaty"))
	{
		bOk = Political && Political->BreakTreaty(Iso, Arg2, static_cast<EWLTreatyType>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("spynet"))
	{
		const FString SpyId = FindPlayerSpyId();
		bOk = Political && !SpyId.IsEmpty() && Political->BuildSpyNetwork(Iso, Arg1, SpyId, Message);
		if (SpyId.IsEmpty())
		{
			Message = TEXT("Sin espias activos disponibles.");
		}
	}
	else if (Verb == TEXT("spy"))
	{
		const FString SpyId = FindPlayerSpyId();
		bOk = Political && !SpyId.IsEmpty() && Political->RunSpyOperation(
			Iso, Arg2, SpyId, static_cast<EWLSpyOperationType>(FCString::Atoi(*Arg1)), Message);
		if (SpyId.IsEmpty())
		{
			Message = TEXT("Sin espias activos disponibles.");
		}
	}
	else if (Verb == TEXT("event"))
	{
		bOk = Political && Political->ResolveEvent(Arg1, Arg2, Message);
	}
	else if (Verb == TEXT("bond"))
	{
		if (Tick)
		{
			const int64 Principal = FMath::Max<int64>(10000, Tick->GetFinancialProfile(Iso).AvailableCredit / 4);
			bOk = Tick->IssueBond(Iso, Principal, 24, Message);
		}
	}
	else if (Verb == TEXT("imf"))
	{
		if (Tick)
		{
			const int64 Principal = FMath::Max<int64>(10000, Tick->GetFinancialProfile(Iso).AvailableCredit / 3);
			bOk = Tick->RequestIMFProgram(Iso, Principal, 36, Message);
		}
	}
	else if (Verb == TEXT("default"))
	{
		bOk = Tick && Tick->MarkDebtDefault(Iso, Message);
	}
	else if (Verb == TEXT("aid"))
	{
		bOk = Tick && Tick->GrantForeignAid(Iso, Arg1, 1000, 12, Message);
	}
	else if (Verb == TEXT("build"))
	{
		bOk = Tick && Tick->BuildBuilding(ProvinceContextId, Arg1, Message);
	}
	else if (Verb == TEXT("upgrade"))
	{
		bOk = Tick && Tick->UpgradeBuilding(ProvinceContextId, Arg1, Message);
	}
	else if (Verb == TEXT("fdi"))
	{
		// fdi:<iso>:<provincia>:<edificio> — invertir en el extranjero (FE5.3); construye al completarse.
		const FString Arg3 = Parts.Num() > 3 ? Parts[3] : FString();
		bOk = Tick && Tick->StartForeignInvestment(Iso, Arg1, Arg2, Arg3, 1500, 12, Message);
	}
	// --- Gobierno P1/P2: agenda, programas, reformas, partidos, elecciones, patronazgo, medios, regiones ---
	else if (Verb == TEXT("agendaset"))
	{
		bOk = Political && Political->SetGovernmentAgenda(Iso, DraftAgenda, Message);
		if (bOk)
		{
			bDraftAgendaLoaded = false;   // re-sincroniza el borrador con lo que confirmo el backend
		}
	}
	else if (Verb == TEXT("program"))
	{
		bOk = Political && Political->StartMinistryProgram(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("reform"))
	{
		bOk = Political && Political->EnactPolicyReform(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("promise"))
	{
		bOk = Political && Political->MakeCampaignPromise(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("negotiate"))
	{
		bOk = Political && Political->NegotiatePartySupport(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("partyelect"))
	{
		bOk = Political && Political->HoldPartyInternalElection(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("patronage"))
	{
		bOk = Political && Political->UsePatronage(Iso, static_cast<EWLPatronageActionType>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("media"))
	{
		bOk = Political && Political->RunMediaAction(Iso, static_cast<EWLMediaActionType>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("region"))
	{
		// region:<accion>:<regionId>
		bOk = Political && Political->RunRegionPolicy(Iso, Arg2, static_cast<EWLRegionPolicyActionType>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("appointc"))
	{
		// appointc:<office>:<characterId> — nombramiento elegido en el comparador de candidatos.
		bOk = Characters && Characters->AppointMinister(Iso, static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1)), Arg2, Message);
		if (bOk)
		{
			CompareOfficeContext = -1;
		}
	}
	else if (Verb == TEXT("hire"))
	{
		FWLCharacter NewMinister;
		bOk = Characters && Characters->HireMinister(Iso, static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1)), NewMinister, Message);
	}
	// --- Militar: reorganizar ejercito, asignar general; y dificultad de IA ---
	else if (Verb == TEXT("reorg"))
	{
		UWLMilitarySubsystem* Military = GetMilitary();
		bOk = Military && Military->ReorganizeArmy(Arg1, 0, Message);   // 0 = reincorpora todas las reservas
	}
	else if (Verb == TEXT("assigngen"))
	{
		// assigngen:<characterId>:<armyId>
		bOk = Characters && Characters->AssignGeneralToArmy(Arg1, Arg2, Message);
	}
	else if (Verb == TEXT("difficulty"))
	{
		if (UWLBalanceSubsystem* Balance = GetBalance())
		{
			const EWLAIDifficulty Level = static_cast<EWLAIDifficulty>(FCString::Atoi(*Arg1));
			Balance->SetAIDifficulty(Level);
			bOk = true;
			Message = FString::Printf(TEXT("Dificultad de la IA fijada en %s."),
				Level == EWLAIDifficulty::Easy ? TEXT("Facil") : (Level == EWLAIDifficulty::Hard ? TEXT("Dificil") : TEXT("Medio")));
		}
	}
	else if (Verb == TEXT("autoresolve"))
	{
		// autoresolve:<attackerId>:<defenderId> — resultado instantaneo por calculo de poderes.
		if (UWLMilitarySubsystem* Military = GetMilitary())
		{
			const EWLBattleResult Result = Military->AutoResolveBattle(Arg1, Arg2, Message);
			bOk = Result != EWLBattleResult::Invalid;
			BattleAttackerId.Reset();
			BattleDefenderId.Reset();
		}
	}
	else if (Verb == TEXT("tacticalresolve"))
	{
		// Cierra la ventana de gobierno y entra a la BATALLA TACTICA 3D interactiva.
		if (AWLCampaignPlayerController* PC = GetOwningPlayer<AWLCampaignPlayerController>())
		{
			const FString Atk = Arg1;
			const FString Def = Arg2;
			BattleAttackerId.Reset();
			BattleDefenderId.Reset();
			PC->SetGovernmentWindowOpen(false);   // esto destruye/oculta esta ventana
			PC->EnterTacticalBattle(Atk, Def);
			return;   // 'this' ya no esta en pantalla: no toques mas estado del widget
		}
	}
	else
	{
		Message = FString::Printf(TEXT("Accion desconocida: %s"), *ActionId);
	}

	LastActionMessage = Message.IsEmpty()
		? FString::Printf(TEXT("%s: %s"), *Verb, bOk ? TEXT("hecho.") : TEXT("no se pudo."))
		: Message;
	bLastActionSucceeded = bOk;
	RebuildCenter();
}

void UWLGovernmentWidget::OnCloseClicked()
{
	if (AWLCampaignPlayerController* PC = GetOwningPlayer<AWLCampaignPlayerController>())
	{
		PC->SetGovernmentWindowOpen(false);
	}
}

void UWLGovernmentWidget::OnTaxDown() { AdjustTaxRate(-5); }
void UWLGovernmentWidget::OnTaxUp()   { AdjustTaxRate(+5); }

// Buscador de DIPLOMACIA: filtra al confirmar con Enter (reconstruir en cada tecla robaria el foco).
void UWLGovernmentWidget::OnDiplomacySearchCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}
	DiplomacySearchText = Text.ToString();
	RebuildCenter();
}

void UWLGovernmentWidget::AdjustTaxRate(int32 DeltaPercent)
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	if (!Tick || Iso.IsEmpty())
	{
		return;
	}
	Tick->SetTaxRate(Iso, Tick->GetTaxRate(Iso) + DeltaPercent);
	RebuildCenter();   // refresca tasa, recaudacion y balance mensual en la misma pestana
}

UWLGovernmentWidget::FSummary UWLGovernmentWidget::BuildSummary() const
{
	FSummary S;
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	if (!Registry || !Tick || Iso.IsEmpty())
	{
		return S;
	}

	const int32 InitOrder = Tick->GetBalanceRules().InitialPublicOrder;
	int64 OrderSum = 0;
	for (const FWLProvinceData& P : Registry->GetAllProvinces())
	{
		if (!Tick->GetProvinceControllerIso(P.Id).Equals(Iso, ESearchCase::IgnoreCase))
		{
			continue;
		}
		S.ProvinceCount++;
		FWLProvinceRuntimeState State;
		const bool bHasState = Tick->GetProvinceState(P.Id, State);
		S.Population += bHasState ? State.Population : P.Population;
		OrderSum += bHasState ? State.PublicOrder : InitOrder;
		S.Controlled.Add(P);
	}
	// FE1.3: ingreso y gasto desde el presupuesto por categorias (incluye militar, salarios y social).
	const FWLNationBudget Budget = Tick->GetNationBudget(Iso);
	S.MonthlyIncome = Budget.TotalIncome();
	S.MonthlyUpkeep = Budget.TotalSpending();
	S.AveragePublicOrder = S.ProvinceCount > 0 ? static_cast<int32>(OrderSum / S.ProvinceCount) : 0;
	S.Controlled.Sort([](const FWLProvinceData& A, const FWLProvinceData& B) { return A.Population > B.Population; });
	return S;
}

FString UWLGovernmentWidget::PlayerIso() const
{
	const UWLCampaignGameInstance* GI = GetCampaignGI();
	return GI ? GI->GetSelectedNationIso() : FString();
}

UWLCampaignGameInstance* UWLGovernmentWidget::GetCampaignGI() const
{
	return Cast<UWLCampaignGameInstance>(GetGameInstance());
}

UWLDataRegistry* UWLGovernmentWidget::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLStrategicTickSubsystem* UWLGovernmentWidget::GetTick() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}

UWLCharacterSubsystem* UWLGovernmentWidget::GetCharacters() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLCharacterSubsystem>() : nullptr;
}

UWLPoliticalSubsystem* UWLGovernmentWidget::GetPolitical() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLPoliticalSubsystem>() : nullptr;
}

UWLMilitarySubsystem* UWLGovernmentWidget::GetMilitary() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLMilitarySubsystem>() : nullptr;
}

UWLBalanceSubsystem* UWLGovernmentWidget::GetBalance() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLBalanceSubsystem>() : nullptr;
}

FString UWLGovernmentWidget::FindPlayerSpyId() const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters)
	{
		return FString();
	}
	for (const FWLCharacter& Spy : Characters->GetCharactersByRole(PlayerIso(), EWLCharacterRole::Spy))
	{
		if (Spy.bActive)
		{
			return Spy.Id;
		}
	}
	return FString();
}
