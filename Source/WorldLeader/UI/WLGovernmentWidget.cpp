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
#include "WorldLeader.h"

using namespace WLGovUI;

namespace
{
	struct FWLScopedGovernmentPerfLog
	{
		FWLScopedGovernmentPerfLog(const TCHAR* InLabel, double InThresholdMs, const FString& InContext = FString())
			: Label(InLabel)
			, Context(InContext)
			, ThresholdMs(InThresholdMs)
			, StartSeconds(FPlatformTime::Seconds())
		{
		}

		~FWLScopedGovernmentPerfLog()
		{
			const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
			if (ElapsedMs >= ThresholdMs)
			{
				const FString ContextText = Context.IsEmpty() ? FString() : FString::Printf(TEXT(" [%s]"), *Context);
				UE_LOG(LogWorldLeader, Log, TEXT("WLPerf Gobierno UI: %s%s %.2f ms"),
					Label,
					*ContextText,
					ElapsedMs);
			}
		}

		const TCHAR* Label = TEXT("");
		FString Context;
		double ThresholdMs = 0.0;
		double StartSeconds = 0.0;
	};

	FString GovernmentTabPerfName(EWLGovernmentTab Tab)
	{
		switch (Tab)
		{
		case EWLGovernmentTab::Overview:    return TEXT("Overview");
		case EWLGovernmentTab::Economy:     return TEXT("Economy");
		case EWLGovernmentTab::HighCommand: return TEXT("HighCommand");
		case EWLGovernmentTab::Politics:    return TEXT("Politics");
		case EWLGovernmentTab::Diplomacy:   return TEXT("Diplomacy");
		case EWLGovernmentTab::Records:     return TEXT("Records");
		case EWLGovernmentTab::Province:    return TEXT("Province");
		default:                            return TEXT("Unknown");
		}
	}

	int64 SuggestedDebtPrincipal(int64 AvailableCredit, int32 Divisor)
	{
		if (AvailableCredit <= 0)
		{
			return 0;
		}
		const int64 Suggested = FMath::Max<int64>(10000, AvailableCredit / FMath::Max(1, Divisor));
		return FMath::Clamp<int64>(Suggested, 1, AvailableCredit);
	}

	int64 EstimateDebtPayment(int64 Principal, int32 TermMonths, double MonthlyInterestRate)
	{
		if (Principal <= 0 || TermMonths <= 0)
		{
			return 0;
		}
		return static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(Principal) / static_cast<double>(TermMonths)
			+ static_cast<double>(Principal) * FMath::Max(0.0, MonthlyInterestRate)));
	}

	FString GovernmentLogCategoryToText(EWLGovernmentLogCategory Category)
	{
		switch (Category)
		{
		case EWLGovernmentLogCategory::Government: return TEXT("GOBIERNO");
		case EWLGovernmentLogCategory::Economy: return TEXT("ECONOMIA");
		case EWLGovernmentLogCategory::Diplomacy: return TEXT("DIPLOMACIA");
		case EWLGovernmentLogCategory::Military: return TEXT("MILITAR");
		case EWLGovernmentLogCategory::Intelligence: return TEXT("INTELIGENCIA");
		case EWLGovernmentLogCategory::Crisis: return TEXT("CRISIS");
		case EWLGovernmentLogCategory::Event: return TEXT("EVENTO");
		case EWLGovernmentLogCategory::AI: return TEXT("IA");
		case EWLGovernmentLogCategory::General:
		default: return TEXT("GENERAL");
		}
	}
}

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
	FWLScopedGovernmentPerfLog Perf(TEXT("RebuildWidget"), 0.10);
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

	// Juice: la ventana entra con fundido + escala desde el centro (se siente premium, no "de golpe").
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderOpacity(0.f);
	OpenAnimTime = 0.f;
	bOpenAnimating = true;
}

void UWLGovernmentWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Curva suave (easeOutCubic) para que arranque rapido y frene con elegancia.
	auto EaseOut = [](float T) { const float U = 1.f - FMath::Clamp(T, 0.f, 1.f); return 1.f - U * U * U; };

	if (bOpenAnimating)
	{
		OpenAnimTime += InDeltaTime;
		const float E = EaseOut(OpenAnimTime / 0.20f);
		SetRenderOpacity(E);
		FWidgetTransform X;
		X.Scale = FVector2D(FMath::Lerp(0.97f, 1.f, E), FMath::Lerp(0.97f, 1.f, E));
		X.Translation = FVector2D(0.f, FMath::Lerp(14.f, 0.f, E));   // sube ligeramente al aparecer
		SetRenderTransform(X);
		if (OpenAnimTime >= 0.20f)
		{
			bOpenAnimating = false;
			SetRenderOpacity(1.f);
			SetRenderTransform(FWidgetTransform());
		}
	}

	// Fade rapido del contenido al cambiar de pestana.
	if (bContentAnimating && CenterScroll)
	{
		ContentAnimTime += InDeltaTime;
		const float E = EaseOut(ContentAnimTime / 0.13f);
		CenterScroll->SetRenderOpacity(E);
		if (ContentAnimTime >= 0.13f)
		{
			bContentAnimating = false;
			CenterScroll->SetRenderOpacity(1.f);
		}
	}
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
	FWLScopedGovernmentPerfLog Perf(TEXT("BuildShell"), 0.10);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GovRoot"));
	WidgetTree->RootWidget = Root;

	// Fondo modal atenuado (cubre todo el mapa).
	UBorder* Dim = MakeBorder(WidgetTree, GovBackdrop, FMargin(0.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	// Marco dorado + panel interior. RESPONSIVE: anclas proporcionales (porcentaje del viewport),
	// asi la ventana ocupa ~93% x 94% de CUALQUIER resolucion (1080p, 1440p, 4K...) sin quedar
	// pequena ni desbordar. Estilo Football Manager: casi pantalla completa, dejando ver el mapa
	// atenuado en los bordes.
	UBorder* Frame = MakeRoundedSurface(WidgetTree, GovFrame, FMargin(2.f), 14.f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Frame))
	{
		S->SetAnchors(FAnchors(0.035f, 0.03f, 0.965f, 0.97f));
		S->SetOffsets(FMargin(0.f));
	}

	// Panel con fondo TEXTURIZADO (gradiente+viñeta): profundidad frente al relleno plano.
	UBorder* Panel = MakeTexturedPanel(WidgetTree, FMargin(16.f));
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

	// Franja translucida: si hay imagen de fondo (UI/gov_panel_bg.png) asoma tras el titulo (efecto banner).
	UBorder* Strip = MakeRoundedSurface(WidgetTree,
		FLinearColor(GovHeaderStrip.R, GovHeaderStrip.G, GovHeaderStrip.B, 0.66f), FMargin(16.f, 12.f), 10.f);
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Strip->SetContent(HB);

	// Emblema: bandera real del pais (UI/Flags/<ISO>.png) o chip de color con ISO de fallback.
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeFlag(WidgetTree,
		bHasNation ? Nation.Iso : FString(), bHasNation ? Nation.MapColor : FLinearColor::Gray,
		66.f, 46.f, bHasNation ? Nation.Iso : TEXT("--"))))
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

	// Boton cerrar (X), redondeado como el resto de la UI.
	UButton* Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	StyleRoundedButton(Close, 10.f);
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

	// Linea de acento dorada bajo la cabecera (identidad, ahora que la franja es pizarra oscura).
	USizeBox* Accent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Accent->SetHeightOverride(3.f);
	Accent->SetContent(MakeRoundedSurface(WidgetTree, GovGold, FMargin(0.f), 1.5f));
	AddColumnChild(Root, Accent, 0.f);
}

void UWLGovernmentWidget::BuildTabs(UVerticalBox* Root)
{
	// Control segmentado moderno: un riel oscuro redondeado contiene las pestanas; la activa se
	// pinta ORO SOLIDO con texto oscuro (senal inequivoca), las demas son texto sobre el riel.
	UBorder* Rail = MakeRoundedSurface(WidgetTree, GovBarTrack, FMargin(5.f), 12.f);
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Rail->SetContent(HB);

	// AddDynamic estampa el NOMBRE de la funcion en tiempo de compilacion, asi que el handler NO puede
	// venir de una variable (puntero a miembro): cada pestana se enlaza explicitamente por indice.
	const TCHAR* Labels[] = { TEXT("RESUMEN"), TEXT("ECONOMIA"), TEXT("ALTO MANDO"), TEXT("POLITICA"), TEXT("DIPLOMACIA"), TEXT("REGISTROS") };

	TabButtons.Reset();
	TabLabels.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(Labels); ++i)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		StyleRoundedButton(Button, 9.f);
		Button->SetBackgroundColor(GovBarTrack);
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

		UTextBlock* Label = MakeText(WidgetTree, Labels[i], 14, GovMuted, ETextJustify::Center);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(40.f);
		Box->SetContent(Label);
		Button->SetContent(Box);

		// Cada pestana ocupa el mismo ancho del riel: rejilla uniforme, no botones sueltos.
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Button))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(i == 0 ? 0.f : 3.f, 0.f, 0.f, 0.f));
		}
		TabButtons.Add(Button);
		TabLabels.Add(Label);
	}

	AddColumnChild(Root, Rail, 12.f);
}

void UWLGovernmentWidget::BuildBody(UVerticalBox* Root)
{
	// Una sola columna de contenido a ancho completo. Antes habia dos rieles fijos (GABINETE y
	// OTRAS POTENCIAS) que se dibujaban en TODOS los tabs, apretujando el centro y repitiendo info
	// que ya vive en ALTO MANDO (gabinete) y DIPLOMACIA (otras potencias). Cada tab ahora es duenno
	// de su ancho: mas aire, mejor lectura, sin duplicacion.
	// Cuerpo TRANSLUCIDO: la imagen de fondo (mapa nocturno) asoma entre las tarjetas y detras de
	// los titulos, como en el menu. Las tarjetas siguen opacas, asi el texto denso se lee bien.
	// Si no hay imagen, el translucido sobre el fondo plano oscuro se ve igual de oscuro.
	UBorder* Col = MakeRoundedSurface(WidgetTree,
		FLinearColor(GovPanelSoft.R, GovPanelSoft.G, GovPanelSoft.B, 0.40f), FMargin(18.f, 14.f), 10.f);
	CenterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	CenterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CenterScroll->AddChild(CenterBox);
	Col->SetContent(CenterScroll);

	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Col))
	{
		S->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UWLGovernmentWidget::BuildFooter(UVerticalBox* Root)
{
	FWLScopedGovernmentPerfLog Perf(TEXT("BuildFooter"), 0.10);
	// Barra de estado inferior: vitales nacionales de un vistazo (estandar en juegos de estrategia).
	// Ya NO repite al presidente (eso vive en la cabecera): aqui van tesoro, balance, orden y capital politico.
	const UWLCharacterSubsystem* Characters = GetCharacters();
	const FString Iso = PlayerIso();
	const int64 Treasury = !Iso.IsEmpty() ? GetCachedTreasury() : 0;
	const int64 Balance = !Iso.IsEmpty() ? GetCachedMonthlyBalance() : 0;
	const FSummary Sum = BuildSummary();
	const int32 PolCapital = (Characters && !Iso.IsEmpty()) ? Characters->GetGovernmentStats(Iso).PoliticalCapital : 0;

	UBorder* Footer = MakeRoundedSurface(WidgetTree,
		FLinearColor(GovHeaderStrip.R, GovHeaderStrip.G, GovHeaderStrip.B, 0.66f), FMargin(12.f, 9.f), 10.f);
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Footer->SetContent(HB);

	// Vitales como chips con icono (misma familia que las tarjetas de metrica de los tabs).
	auto AddStat = [&](EWLGovIcon Icon, const FLinearColor& Accent,
		const FString& Label, const FString& Value, const FLinearColor& ValueColor)
	{
		UHorizontalBox* Chip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder* IconChip = MakeRoundedSurface(WidgetTree,
			FLinearColor(Accent.R, Accent.G, Accent.B, 0.14f), FMargin(6.f), 8.f);
		IconChip->SetVerticalAlignment(VAlign_Center);
		IconChip->SetHorizontalAlignment(HAlign_Center);
		IconChip->SetContent(MakeIcon(WidgetTree, Icon, 20, Accent));
		if (UHorizontalBoxSlot* S = Chip->AddChildToHorizontalBox(IconChip))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Cell->AddChildToVerticalBox(MakeText(WidgetTree, Label, 10, GovMuted));
		if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(MakeText(WidgetTree, Value, 17, ValueColor)))
		{
			S->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = Chip->AddChildToHorizontalBox(Cell))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Chip))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 30.f, 0.f));
		}
	};
	AddStat(EWLGovIcon::Treasury, GovGold, TEXT("TESORO NACIONAL"), GovGroupThousands(Treasury), GovText);
	AddStat(EWLGovIcon::Balance, Balance >= 0 ? GovGood : GovBad, TEXT("BALANCE / MES"),
		FString::Printf(TEXT("%s%s"), Balance >= 0 ? TEXT("+") : TEXT(""), *GovGroupThousands(Balance)),
		Balance >= 0 ? GovGood : GovBad);
	const FLinearColor OrderColor = Sum.AveragePublicOrder >= 60 ? GovGood : (Sum.AveragePublicOrder >= 35 ? GovGold : GovBad);
	AddStat(EWLGovIcon::Order, OrderColor, TEXT("ORDEN PUBLICO"),
		FString::Printf(TEXT("%d"), Sum.AveragePublicOrder), OrderColor);
	AddStat(EWLGovIcon::Capital, GovGold, TEXT("CAPITAL POLITICO"), FString::Printf(TEXT("%d"), PolCapital), GovGold);

	AddColumnChild(Root, Footer, 12.f);
}

void UWLGovernmentWidget::RebuildCenter(bool bPreserveScrollOffset)
{
	FWLScopedGovernmentPerfLog Perf(TEXT("RebuildCenter"), 0.10, GovernmentTabPerfName(ActiveTab));
	if (!CenterBox)
	{
		return;
	}
	const float PreviousScrollOffset = (bPreserveScrollOffset && CenterScroll)
		? CenterScroll->GetScrollOffset()
		: 0.f;
	CenterBox->ClearChildren();
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
	if (CenterScroll)
	{
		if (bPreserveScrollOffset)
		{
			CenterScroll->SetScrollOffset(PreviousScrollOffset);
		}
		else
		{
			CenterScroll->ScrollToStart();
		}
	}
}

void UWLGovernmentWidget::InvalidateDataSnapshot() const
{
	DataSnapshot = FDataSnapshot();
}

void UWLGovernmentWidget::EnsureDataSnapshotContext() const
{
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	const int32 Day = Tick ? Tick->GetCurrentDay() : 0;
	const int32 Month = Tick ? Tick->GetCurrentMonth() : 0;
	const int32 Year = Tick ? Tick->GetCurrentYear() : 0;

	if (!DataSnapshot.bContextValid
		|| !DataSnapshot.NationIso.Equals(Iso, ESearchCase::IgnoreCase)
		|| DataSnapshot.Day != Day
		|| DataSnapshot.Month != Month
		|| DataSnapshot.Year != Year)
	{
		DataSnapshot = FDataSnapshot();
		DataSnapshot.bContextValid = true;
		DataSnapshot.NationIso = Iso;
		DataSnapshot.Day = Day;
		DataSnapshot.Month = Month;
		DataSnapshot.Year = Year;
	}
}

const UWLGovernmentWidget::FSummary& UWLGovernmentWidget::GetCachedSummary() const
{
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bSummaryValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedSummary miss"), 0.10, DataSnapshot.NationIso);
		FSummary S;
		const UWLDataRegistry* Registry = GetRegistry();
		const UWLStrategicTickSubsystem* Tick = GetTick();
		if (Registry && Tick && !DataSnapshot.NationIso.IsEmpty())
		{
			const int32 InitOrder = Tick->GetBalanceRules().InitialPublicOrder;
			int64 OrderSum = 0;
			for (const FWLProvinceData& P : Registry->GetAllProvinces())
			{
				if (!Tick->GetProvinceControllerIso(P.Id).Equals(DataSnapshot.NationIso, ESearchCase::IgnoreCase))
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
			const FWLNationBudget& Budget = GetCachedNationBudget();
			S.MonthlyIncome = Budget.TotalIncome();
			S.MonthlyUpkeep = Budget.TotalSpending();
			S.AveragePublicOrder = S.ProvinceCount > 0 ? static_cast<int32>(OrderSum / S.ProvinceCount) : 0;
			S.Controlled.Sort([](const FWLProvinceData& A, const FWLProvinceData& B) { return A.Population > B.Population; });
		}
		DataSnapshot.Summary = MoveTemp(S);
		DataSnapshot.bSummaryValid = true;
	}
	return DataSnapshot.Summary;
}

const FWLNationBudget& UWLGovernmentWidget::GetCachedNationBudget() const
{
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bBudgetValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedNationBudget miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.Budget = FWLNationBudget();
		if (const UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (!DataSnapshot.NationIso.IsEmpty())
			{
				DataSnapshot.Budget = Tick->GetNationBudget(DataSnapshot.NationIso);
			}
		}
		DataSnapshot.bBudgetValid = true;
	}
	return DataSnapshot.Budget;
}

const TArray<FWLGoodMarketBalance>& UWLGovernmentWidget::GetCachedNationGoodMarketBalance() const
{
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bMarketValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedNationGoodMarketBalance miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.Market.Reset();
		if (const UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (!DataSnapshot.NationIso.IsEmpty())
			{
				DataSnapshot.Market = Tick->GetNationGoodMarketBalance(DataSnapshot.NationIso);
			}
		}
		DataSnapshot.bMarketValid = true;
	}
	return DataSnapshot.Market;
}

int64 UWLGovernmentWidget::GetCachedMonthlyBalance() const
{
	return GetCachedNationBudget().Net();
}

int64 UWLGovernmentWidget::GetCachedTreasury() const
{
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bTreasuryValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedTreasury miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.Treasury = 0;
		if (const UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (!DataSnapshot.NationIso.IsEmpty())
			{
				DataSnapshot.Treasury = Tick->GetTreasury(DataSnapshot.NationIso);
			}
		}
		DataSnapshot.bTreasuryValid = true;
	}
	return DataSnapshot.Treasury;
}

int64 UWLGovernmentWidget::GetCachedNationGDP() const
{
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bNationGDPValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedNationGDP miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.NationGDP = 0;
		if (const UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (!DataSnapshot.NationIso.IsEmpty())
			{
				DataSnapshot.NationGDP = Tick->GetNationGDP(DataSnapshot.NationIso);
			}
		}
		DataSnapshot.bNationGDPValid = true;
	}
	return DataSnapshot.NationGDP;
}

double UWLGovernmentWidget::GetCachedNationGDPGrowth() const
{
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bNationGDPGrowthValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedNationGDPGrowth miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.NationGDPGrowth = 0.0;
		if (const UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (!DataSnapshot.NationIso.IsEmpty())
			{
				DataSnapshot.NationGDPGrowth = Tick->GetNationGDPGrowth(DataSnapshot.NationIso);
			}
		}
		DataSnapshot.bNationGDPGrowthValid = true;
	}
	return DataSnapshot.NationGDPGrowth;
}

double UWLGovernmentWidget::GetCachedNationInflationRate(const FWLBalanceRules& Rules) const
{
	(void)Rules;
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bNationInflationRateValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedNationInflationRate miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.NationInflationRate = 0.0;
		if (const UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (!DataSnapshot.NationIso.IsEmpty())
			{
				DataSnapshot.NationInflationRate = Tick->GetNationInflationRate(DataSnapshot.NationIso);
			}
		}
		DataSnapshot.bNationInflationRateValid = true;
	}
	return DataSnapshot.NationInflationRate;
}

const FWLNationLaborStats& UWLGovernmentWidget::GetCachedNationLaborStats() const
{
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bNationLaborStatsValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedNationLaborStats miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.NationLaborStats = FWLNationLaborStats();
		if (const UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (!DataSnapshot.NationIso.IsEmpty())
			{
				DataSnapshot.NationLaborStats = Tick->GetNationLaborStats(DataSnapshot.NationIso);
			}
		}
		DataSnapshot.bNationLaborStatsValid = true;
	}
	return DataSnapshot.NationLaborStats;
}

const FString& UWLGovernmentWidget::GetCachedNationEconomicCycleLabel(const FWLBalanceRules& Rules) const
{
	(void)Rules;
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bNationEconomicCycleLabelValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedNationEconomicCycleLabel miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.NationEconomicCycleLabel = TEXT("Estable");
		if (const UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (!DataSnapshot.NationIso.IsEmpty())
			{
				DataSnapshot.NationEconomicCycleLabel = Tick->GetNationEconomicCycleLabel(DataSnapshot.NationIso);
			}
		}
		DataSnapshot.bNationEconomicCycleLabelValid = true;
	}
	return DataSnapshot.NationEconomicCycleLabel;
}

int64 UWLGovernmentWidget::GetCachedCreditLimit(const FWLBalanceRules& Rules) const
{
	EnsureDataSnapshotContext();
	if (!DataSnapshot.bCreditLimitValid)
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedCreditLimit miss"), 0.10, DataSnapshot.NationIso);
		DataSnapshot.CreditLimit = static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(GetCachedNationBudget().TotalIncome()) * Rules.DebtCreditLimitIncomeMonths));
		DataSnapshot.bCreditLimitValid = true;
	}
	return DataSnapshot.CreditLimit;
}

int64 UWLGovernmentWidget::GetCachedProvinceMonthlyBalance(const FString& ProvinceId) const
{
	EnsureDataSnapshotContext();
	if (const int64* Found = DataSnapshot.ProvinceMonthlyBalanceById.Find(ProvinceId))
	{
		return *Found;
	}

	int64 Balance = 0;
	if (const UWLStrategicTickSubsystem* Tick = GetTick())
	{
		FWLScopedGovernmentPerfLog Perf(TEXT("GetCachedProvinceMonthlyBalance miss"), 0.20, ProvinceId);
		Balance = Tick->GetProvinceMonthlyBalance(ProvinceId);
	}
	DataSnapshot.ProvinceMonthlyBalanceById.Add(ProvinceId, Balance);
	return Balance;
}

void UWLGovernmentWidget::BuildOverviewTab()
{
	FWLScopedGovernmentPerfLog Perf(TEXT("BuildOverviewTab"), 0.10);
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	const FSummary Sum = BuildSummary();
	// Tesoro y balance ya viven en la barra de estado inferior (siempre visible); no se repiten en el grid.

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

	// INFORME PRESIDENCIAL: rostro del lider + identidad + rasgos + aprobacion como titular.
	// Da un punto focal humano al RESUMEN (el retrato no vive en la cabecera, solo la bandera).
	{
		UWLCampaignGameInstance* GI = GetCampaignGI();
		FWLNationData Nation;
		const bool bHasNation = GI && GI->GetSelectedNation(Nation);
		const UWLPoliticalSubsystem* Political = GetPolitical();
		const int32 Approval = Political ? Political->GetMediaPublicOpinion(Iso).PresidentialApproval : 0;
		const FString Leader = (bHasNation && !Nation.Leader.IsEmpty()) ? Nation.Leader : TEXT("Presidente de la Republica");

		UBorder* Hero = MakeCard(WidgetTree, GovHeaderStrip, FMargin(16.f, 14.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakePortrait(WidgetTree,
			FString::Printf(TEXT("%s-LEADER-INCUMBENT"), *Iso), GovGold, 94.f, 116.f, InferGender(Leader))))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		UVerticalBox* IdVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		IdVB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("INFORME PRESIDENCIAL"), 11, GovMuted));
		if (UVerticalBoxSlot* S = IdVB->AddChildToVerticalBox(MakeText(WidgetTree, Leader, 21, GovText)))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		if (bHasNation && !Nation.GovernmentType.IsEmpty())
		{
			if (UVerticalBoxSlot* S = IdVB->AddChildToVerticalBox(MakeText(WidgetTree, Nation.GovernmentType, 13, GovGold)))
			{
				S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
			}
		}
		if (Political)
		{
			const TArray<FString> Traits = Political->GetLeaderAgendaTraits(Iso);
			if (Traits.Num() > 0)
			{
				UHorizontalBox* TraitRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				for (const FString& T : Traits)
				{
					if (UHorizontalBoxSlot* S = TraitRow->AddChildToHorizontalBox(
						MakeBadge(WidgetTree, T.ToUpper(), GovGoldDim, GovDarkInk)))
					{
						S->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
					}
				}
				if (UVerticalBoxSlot* S = IdVB->AddChildToVerticalBox(TraitRow))
				{
					S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
				}
			}
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(IdVB))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		// Aprobacion presidencial como titular a la derecha: numero grande + barra.
		UVerticalBox* ApprVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ApprVB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("APROBACION"), 11, GovMuted, ETextJustify::Right));
		if (UVerticalBoxSlot* S = ApprVB->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%d%%"), Approval), 36, SupportColor(Approval), ETextJustify::Right)))
		{
			S->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}
		USizeBox* ApprBar = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ApprBar->SetWidthOverride(158.f);
		ApprBar->SetContent(MakeBar(WidgetTree, Approval / 100.f, SupportColor(Approval), 9.f));
		if (UVerticalBoxSlot* S = ApprVB->AddChildToVerticalBox(ApprBar))
		{
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			S->SetHorizontalAlignment(HAlign_Right);
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(ApprVB))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}

		Hero->SetContent(HB);
		AddColumnChild(CenterBox, Hero, 6.f);
	}

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("ESTADO DE LA NACION")), 16.f);

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(5.f));
	auto Place = [&](int32 R, int32 Col, UBorder* Card)
	{
		if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, R, Col))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	// Tiles con icono, barra de acento de color y buen contraste. Tesoro y Balance NO se repiten aqui:
	// viven en la barra de estado inferior (siempre visible). 6 tiles = 2 filas de 3, sin celda huerfana.
	const double Growth = Tick ? GetCachedNationGDPGrowth() : 0.0;
	Place(0, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Provinces, GovGold,
		TEXT("Provincias"), FString::Printf(TEXT("%d"), Sum.ProvinceCount), GovText));
	Place(0, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Population, FLinearColor(0.45f, 0.68f, 0.95f),
		TEXT("Poblacion"), GovGroupThousands(Sum.Population), GovText));
	Place(0, 2, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Balance, GovGood,
		TEXT("Ingreso / mes"), GovGroupThousands(Sum.MonthlyIncome), GovGood));
	Place(1, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Order, FLinearColor(0.85f, 0.55f, 0.40f),
		TEXT("Mantenimiento / mes"), GovGroupThousands(Sum.MonthlyUpkeep), GovMuted));
	Place(1, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Treasury, GovGold,
		TEXT("PIB / mes"), Tick ? GovGroupThousands(GetCachedNationGDP()) : TEXT("--"), GovText));
	Place(1, 2, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Growth,
		Growth > 0.0 ? GovGood : (Growth < 0.0 ? GovBad : GovMuted),
		TEXT("Crecimiento"), Tick ? FString::Printf(TEXT("%+.2f%%"), Growth * 100.0) : TEXT("--"),
		Growth > 0.0 ? GovGood : (Growth < 0.0 ? GovBad : GovMuted)));
	AddColumnChild(CenterBox, Grid, 10.f);

	// Gobierno P1/P2: pulso politico del gobierno (aprobacion, legitimidad, eleccion, coalicion...).
	BuildGovernanceOverviewCards();

	// Dificultad de la IA activa + selector (lee reglas del backend de balance).
	BuildDifficultyPanel();

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("CONDICIONES DE VICTORIA")), 20.f);
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
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("TERRITORIO  (%d provincias)"), Sum.ProvinceCount)), 20.f);
	if (Sum.Controlled.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin provincias bajo control directo."), 14, GovMuted), 10.f);
		return;
	}

	// Layout FM: provincias en DOS columnas (mitad de scroll en paises grandes). Cada ficha
	// es autoexplicativa: nombre + poblacion + balance como insignia de color.
	UVerticalBox* TerrLeft; UVerticalBox* TerrRight;
	UHorizontalBox* TerrCols = MakeTwoColumnRow(WidgetTree, TerrLeft, TerrRight);
	int32 Index = 0;
	for (const FWLProvinceData& P : Sum.Controlled)
	{
		const int64 Bal = Tick ? GetCachedProvinceMonthlyBalance(P.Id) : 0;
		UBorder* Row = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* NamePop = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		NamePop->AddChildToVerticalBox(MakeText(WidgetTree, P.Name, 15, GovText));
		NamePop->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Poblacion %s"), *GovGroupThousands(P.Population)), 11, GovMuted));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(NamePop))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeBadge(WidgetTree,
			FString::Printf(TEXT("%s%s"), Bal >= 0 ? TEXT("+") : TEXT(""), *GovGroupThousands(Bal)),
			Bal >= 0 ? GovGood : GovBad, GovDarkInk)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Row->SetContent(HB);
		AddColumnChild((Index % 2 == 0) ? TerrLeft : TerrRight, Row, 4.f);
		++Index;
	}
	AddColumnChild(CenterBox, TerrCols, 4.f);
}

// FE1.3: panel ECONOMIA — presupuesto mensual desglosado por categorias + palanca de impuestos (FE1.2).
void UWLGovernmentWidget::BuildEconomyTab()
{
	FWLScopedGovernmentPerfLog Perf(TEXT("BuildEconomyTab"), 0.10);
	UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	if (!Tick || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos economicos."), 14, GovMuted), 10.f);
		return;
	}

	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const FWLNationBudget Budget = GetCachedNationBudget();

	// Fila de presupuesto: etiqueta (fill) + importe con signo a la derecha. Target = columna destino.
	auto AddBudgetRow = [&](UVerticalBox* Target, const FString& Label, int64 Amount, bool bIsIncome, bool bTotal, int32 Index)
	{
		const FLinearColor RowColor = bTotal ? GovCardAlt : ((Index % 2 == 0) ? GovCard : GovCardAlt);
		UBorder* Row = MakeCard(WidgetTree, RowColor, FMargin(12.f, bTotal ? 10.f : 8.f));
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
		AddColumnChild(Target, Row, bTotal ? 8.f : 4.f);
	};

	// FE1.5: macro en tiles con icono (antes eran dos lineas de texto apretadas e ilegibles).
	{
		const double Growth = GetCachedNationGDPGrowth();
		const double Inflation = GetCachedNationInflationRate(Rules);
		const FWLNationLaborStats& Labor = GetCachedNationLaborStats();
		const FString& CycleLabel = GetCachedNationEconomicCycleLabel(Rules);

		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		Grid->SetSlotPadding(FMargin(5.f));
		auto Place = [&](int32 R, int32 C, UBorder* Card)
		{
			if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, R, C)) { S->SetHorizontalAlignment(HAlign_Fill); }
		};
		Place(0, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Treasury, GovGold,
			TEXT("PIB / mes"), GovGroupThousands(GetCachedNationGDP()), GovText));
		Place(0, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Growth, Growth < 0.0 ? GovBad : GovGood,
			TEXT("Crecimiento"), FString::Printf(TEXT("%+.2f%%"), Growth * 100.0), Growth < 0.0 ? GovBad : GovGood));
		Place(0, 2, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Balance, GovGoldDim,
			TEXT("Inflacion"), FString::Printf(TEXT("%+.2f%%"), Inflation * 100.0), Inflation > 0.05 ? GovBad : GovText));
		Place(1, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Politics, FLinearColor(0.55f, 0.68f, 0.95f),
			TEXT("Ciclo economico"), CycleLabel, GovText));
		Place(1, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Crisis, Labor.UnemploymentRate > 0.15 ? GovBad : GovGood,
			TEXT("Desempleo"), FString::Printf(TEXT("%.1f%%"), Labor.UnemploymentRate * 100.0),
			Labor.UnemploymentRate > 0.15 ? GovBad : GovGood));
		Place(1, 2, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Order, FLinearColor(0.85f, 0.55f, 0.40f),
			TEXT("Productividad"), FString::Printf(TEXT("%.0f%%"), Labor.Productivity * 100.0), GovText));
		AddColumnChild(CenterBox, Grid, 6.f);
	}

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("PRESUPUESTO MENSUAL")), 12.f);

	// Layout FM: INGRESOS y GASTOS lado a lado (contabilidad de dos columnas), no una pila larga.
	UVerticalBox* IncomeCol; UVerticalBox* ExpenseCol;
	UHorizontalBox* BudgetCols = MakeTwoColumnRow(WidgetTree, IncomeCol, ExpenseCol);

	AddColumnChild(IncomeCol, MakeText(WidgetTree, TEXT("INGRESOS"), 13, GovGood), 0.f);
	AddBudgetRow(IncomeCol, TEXT("Recursos y produccion"), Budget.ResourceIncome, true, false, 0);
	AddBudgetRow(IncomeCol, TEXT("Impuestos"), Budget.TaxIncome, true, false, 1);
	if (Budget.ExportIncome > 0)
	{
		AddBudgetRow(IncomeCol, TEXT("Exportaciones"), Budget.ExportIncome, true, false, 2);
	}
	if (Budget.TariffIncome > 0)
	{
		AddBudgetRow(IncomeCol, TEXT("Aranceles"), Budget.TariffIncome, true, false, 3);   // FE4.3
	}
	if (Budget.ForeignAidIncome > 0)
	{
		AddBudgetRow(IncomeCol, TEXT("Ayuda exterior recibida"), Budget.ForeignAidIncome, true, false, 4);   // FE5.3
	}
	AddBudgetRow(IncomeCol, TEXT("TOTAL INGRESOS"), Budget.TotalIncome(), true, true, 0);

	AddColumnChild(ExpenseCol, MakeText(WidgetTree, TEXT("GASTOS"), 13, GovBad), 0.f);
	AddBudgetRow(ExpenseCol, TEXT("Militar"), Budget.MilitaryUpkeep, false, false, 0);
	AddBudgetRow(ExpenseCol, TEXT("Infraestructura"), Budget.InfrastructureUpkeep, false, false, 1);
	AddBudgetRow(ExpenseCol, TEXT("Salarios publicos"), Budget.PublicWages, false, false, 2);
	AddBudgetRow(ExpenseCol, TEXT("Gasto social"), Budget.SocialSpending, false, false, 3);
	if (Budget.DebtInterest > 0)
	{
		AddBudgetRow(ExpenseCol, TEXT("Intereses de deuda"), Budget.DebtInterest, false, false, 4);   // FE1.4
	}
	if (Budget.DebtService > 0)
	{
		AddBudgetRow(ExpenseCol, TEXT("Servicio de deuda (bonos/FMI)"), Budget.DebtService, false, false, 5);   // FE5.1
	}
	if (Budget.ImportCost > 0)
	{
		AddBudgetRow(ExpenseCol, TEXT("Importaciones criticas"), Budget.ImportCost, false, false, 6);
	}
	if (Budget.CorruptionLoss > 0)
	{
		AddBudgetRow(ExpenseCol, TEXT("Perdida por corrupcion"), Budget.CorruptionLoss, false, false, 7);   // FE6.2
	}
	if (Budget.ForeignAidExpense > 0)
	{
		AddBudgetRow(ExpenseCol, TEXT("Ayuda exterior concedida"), Budget.ForeignAidExpense, false, false, 8);   // FE5.3
	}
	AddBudgetRow(ExpenseCol, TEXT("TOTAL GASTOS"), Budget.TotalSpending(), false, true, 0);

	AddColumnChild(CenterBox, BudgetCols, 8.f);

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
	// El balance neto vive bajo INGRESOS (columna izquierda): equilibra las alturas (ingresos suele
	// tener menos filas que gastos) y lee natural — "lo que entra y lo que queda al final".
	AddColumnChild(IncomeCol, NetCard, 8.f);

	// FE1.4: deuda y linea de credito. Gastar por encima del tesoro endeuda (con interes mensual)
	// hasta el limite de credito; el tesoro negativo ademas penaliza el orden publico cada mes.
	{
		const int64 Treasury = GetCachedTreasury();
		const int64 CreditLimit = GetCachedCreditLimit(Rules);
		if (Treasury < 0)
		{
			UBorder* DebtCard = MakeCard(WidgetTree, GovCard, FMargin(12.f, 10.f));
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
			AddColumnChild(IncomeCol, DebtCard, 8.f);
		}
		else
		{
			AddColumnChild(IncomeCol, MakeText(WidgetTree, FString::Printf(
				TEXT("Sin deuda. Linea de credito disponible: %s (interes %.0f%%/mes si el tesoro cae en negativo)."),
				*GovGroupThousands(CreditLimit), Rules.DebtMonthlyInterestRate * 100.0),
				12, GovMuted, ETextJustify::Left, true), 8.f);
		}
	}

	// FE2.3-FE4.1: produccion nacional por bien con insumos, demanda, precios y comercio.
	{
		const TArray<FWLGoodMarketBalance>& Market = GetCachedNationGoodMarketBalance();
		if (Market.Num() > 0)
		{
			AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("MERCADO NACIONAL / MES")), 20.f);
			const UWLDataRegistry* Registry = GetRegistry();
			int32 Index = 0;
			for (const FWLGoodMarketBalance& Balance : Market)
			{
				FWLGoodData Good;
				const bool bHasGood = Registry && Registry->GetGood(Balance.GoodId, Good);
				const FString GoodName = bHasGood ? Good.Name : Balance.GoodId;
				const bool bManufactured = bHasGood && Good.Category == EWLGoodCategory::Manufactured;

				UBorder* Row = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 7.f));
				UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				// Icono a color del bien (petroleo, cafe, acero...) si existe UI/Goods/<id>.png.
				if (UWidget* GoodIcon = MakeGoodIcon(WidgetTree, Balance.GoodId, 26.f))
				{
					if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(GoodIcon))
					{
						S->SetVerticalAlignment(VAlign_Center);
						S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
					}
				}
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
			AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("SHOCKS DE MERCADO ACTIVOS")), 20.f);
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
		AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("COMERCIO EXTERIOR")), 20.f);

		UBorder* TariffCard = MakeCard(WidgetTree, GovCard, FMargin(14.f, 11.f));
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
		AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("FINANZAS SOBERANAS")), 20.f);

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

		constexpr int32 BondTermMonths = 24;
		constexpr int32 IMFTermMonths = 36;
		const int64 BondPrincipal = SuggestedDebtPrincipal(Profile.AvailableCredit, 4);
		const double BondRate = Tick->GetInterestRateForInstrument(Iso, EWLFinancialInstrumentType::Bond);
		const int64 BondPayment = EstimateDebtPayment(BondPrincipal, BondTermMonths, BondRate);
		const bool bBondEnabled = !Profile.bInDefault && BondPrincipal > 0;
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Bono previsto"),
			bBondEnabled
				? FString::Printf(TEXT("%s a %d meses · tasa %.2f%%/mes · pago %s/mes"),
					*GovGroupThousands(BondPrincipal), BondTermMonths, BondRate * 100.0, *GovGroupThousands(BondPayment))
				: (Profile.bInDefault ? TEXT("Bloqueado: pais en default") : TEXT("Bloqueado: sin credito disponible")),
			bBondEnabled ? GovText : GovBad, GovCard), 4.f);

		int64 IMFPrincipal = 0;
		double IMFRate = 0.0;
		int64 IMFPayment = 0;
		bool bIMFEnabled = false;
		if (Profile.bIMFEligible)
		{
			IMFPrincipal = SuggestedDebtPrincipal(Profile.AvailableCredit, 3);
			IMFRate = Tick->GetInterestRateForInstrument(Iso, EWLFinancialInstrumentType::IMFProgram);
			IMFPayment = EstimateDebtPayment(IMFPrincipal, IMFTermMonths, IMFRate);
			bIMFEnabled = IMFPrincipal > 0;
			AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Programa FMI previsto"),
				bIMFEnabled
					? FString::Printf(TEXT("%s a %d meses · tasa %.2f%%/mes · pago %s/mes · orden -2"),
						*GovGroupThousands(IMFPrincipal), IMFTermMonths, IMFRate * 100.0, *GovGroupThousands(IMFPayment))
					: TEXT("Bloqueado: sin credito disponible"),
				bIMFEnabled ? GovText : GovBad, GovCardAlt), 4.f);
		}
		if (Profile.OutstandingDebt > 0 && !Profile.bInDefault)
		{
			AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Default previsto"),
				FString::Printf(TEXT("Elimina %s de deuda · orden publico -%d · rating Default"),
					*GovGroupThousands(Profile.OutstandingDebt), Tick->GetBalanceRules().DefaultPublicOrderPenalty),
				GovBad, GovCardAlt), 4.f);
		}

		UHorizontalBox* FinActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddFinAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg, bool bEnabled = true)
		{
			if (UHorizontalBoxSlot* S = FinActions->AddChildToHorizontalBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 130.f, 12, bEnabled)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
		};
		AddFinAction(TEXT("bond"), bBondEnabled ? TEXT("EMITIR BONO") : TEXT("BONO BLOQUEADO"), GovGoldDim, bBondEnabled);
		if (Profile.bIMFEligible)
		{
			AddFinAction(TEXT("imf"), bIMFEnabled ? TEXT("PROGRAMA FMI") : TEXT("FMI BLOQUEADO"), GovTabIdle, bIMFEnabled);
		}
		if (Profile.OutstandingDebt > 0 && !Profile.bInDefault)
		{
			AddFinAction(TEXT("default"), TEXT("DECLARAR DEFAULT"), FLinearColor(0.40f, 0.12f, 0.10f, 1.f));
		}
		AddColumnChild(CenterBox, FinActions, 10.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("El bono y el FMI usan el monto previsto arriba. El default elimina deuda viva, pero hunde rating y orden publico."),
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
		AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("GOBERNANZA ECONOMICA")), 20.f);
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

		AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("IMPUESTOS")), 20.f);

		UBorder* TaxCard = MakeCard(WidgetTree, GovCard, FMargin(14.f, 11.f));
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
	const UWLPoliticalSubsystem* PoliticalSubsystem = GetPolitical();
	const FWLInternalPowerState InternalPower = PoliticalSubsystem ? PoliticalSubsystem->GetInternalPower(Iso) : FWLInternalPowerState();
	const int32 RealCoupRisk = InternalPower.CoupRisk;
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("ALTO MANDO")), 6.f);

	// Indicadores de gobierno como tarjetas de metrica (mismo lenguaje que RESUMEN), no una linea de texto.
	{
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		Grid->SetSlotPadding(FMargin(5.f));
		auto Place = [&](int32 C, UBorder* Card)
		{
			if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, 0, C)) { S->SetHorizontalAlignment(HAlign_Fill); }
		};
		Place(0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Capital, GovGold,
			TEXT("Capital politico"), FString::Printf(TEXT("%d"), Stats.PoliticalCapital), GovText));
		Place(1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Order, Stats.Stability < 40 ? GovBad : GovGood,
			TEXT("Estabilidad"), FString::Printf(TEXT("%d"), Stats.Stability),
			Stats.Stability < 40 ? GovBad : GovGood));
		Place(2, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Politics, Stats.Corruption >= 50 ? GovBad : GovGoldDim,
			TEXT("Corrupcion"), FString::Printf(TEXT("%d"), Stats.Corruption),
			Stats.Corruption >= 50 ? GovBad : GovText));
		Place(3, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Crisis, RealCoupRisk >= 50 ? GovBad : GovMuted,
			TEXT("Riesgo de golpe"), FString::Printf(TEXT("%d"), RealCoupRisk),
			RealCoupRisk >= 50 ? GovBad : GovText));
		AddColumnChild(CenterBox, Grid, 6.f);
	}

	// Gobierno P1: gabinete vivo — rivalidad, faccionalismo y riesgos de escandalo/sabotaje/renuncia.
	BuildCabinetDynamicsCard();

	// Gabinete: cada cargo con su ministro o vacante + nombrar/destituir + su efecto REAL en el juego.
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("GABINETE")), 14.f);
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
		UBorder* Row = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// Retrato del ministro (arte de personaje) con color de acento por cartera.
		const FLinearColor OfficeAccent =
			Seat.Office == EWLMinisterOffice::Economy     ? GovGold :
			Seat.Office == EWLMinisterOffice::Defense     ? FLinearColor(0.88f, 0.38f, 0.32f) :
			Seat.Office == EWLMinisterOffice::Interior    ? FLinearColor(0.42f, 0.78f, 0.52f) :
			Seat.Office == EWLMinisterOffice::Foreign     ? FLinearColor(0.45f, 0.68f, 0.95f) :
			                                                FLinearColor(0.72f, 0.56f, 0.90f);
		const FString PortraitSeed = bFilled ? Seat.CharacterId : FString::Printf(TEXT("VAC-%d"), static_cast<int32>(Seat.Office));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakePortrait(WidgetTree, PortraitSeed, OfficeAccent, 58.f, 72.f,
			bFilled ? InferGender(Seat.Minister.Name) : '?')))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}

		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		// Jerarquia clara: cartera pequena arriba en su color, NOMBRE del ministro grande debajo.
		Info->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("MINISTERIO DE %s"), *UWLCharacterSubsystem::MinisterOfficeToString(Seat.Office).ToUpper()),
			10, OfficeAccent));
		Info->AddChildToVerticalBox(MakeText(WidgetTree,
			bFilled ? Seat.Minister.Name : TEXT("Cargo vacante"), 15, bFilled ? GovText : GovGold));
		if (bFilled)
		{
			Info->AddChildToVerticalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("Skill %d · Lealtad %d · Ambicion %d · Popularidad %d"),
					Seat.Minister.Skill, Seat.Minister.Loyalty, Seat.Minister.Ambition, Seat.Minister.Popularity),
				11, Seat.Minister.Loyalty < 40 ? GovBad : GovMuted, ETextJustify::Left, true));
		}
		// Rasgos como insignias, no texto corrido.
		if (bFilled && Seat.Minister.Traits.Num() > 0)
		{
			UHorizontalBox* TraitRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			for (const FString& Trait : Seat.Minister.Traits)
			{
				if (UHorizontalBoxSlot* S = TraitRow->AddChildToHorizontalBox(
					MakeBadge(WidgetTree, Trait.ToUpper(), GovHeaderStrip, GovGoldDim)))
				{
					S->SetPadding(FMargin(0.f, 3.f, 5.f, 2.f));
				}
			}
			Info->AddChildToVerticalBox(TraitRow);
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
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("GENERALES  (%d)"), Generals.Num())), 16.f);
	if (Generals.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin generales en plantilla. Crea uno para poder dar mando a tus ejercitos."),
			12, GovMuted, ETextJustify::Left, true), 4.f);
	}
	Index = 0;
	for (const FWLCharacter& General : Generals)
	{
		if (!General.bActive)
		{
			continue;
		}
		UBorder* Card = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UVerticalBox* GVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree, General.Name, 15, GovText)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(
			MakeBadge(WidgetTree, RankToText(General.Rank).ToUpper(), GovHeaderStrip, GovGold)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(General.AssignedArmyId.IsEmpty()
			? MakeBadge(WidgetTree, TEXT("SIN MANDO"), GovTabIdle, GovMuted)
			: MakeBadge(WidgetTree, FString::Printf(TEXT("EJERCITO %s"), *General.AssignedArmyId.ToUpper()), GovGoldDim, GovDarkInk)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		GVB->AddChildToVerticalBox(Head);
		const FLinearColor LoyaltyColor = General.Loyalty < 40 ? GovBad : (General.Loyalty < 60 ? GovGold : GovGood);
		if (UVerticalBoxSlot* S = GVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Skill %d · Ambicion %d · Popularidad %d · Renombre %d"),
			General.Skill, General.Ambition, General.Popularity, General.Renown),
			12, GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}
		// Lealtad como barra: es EL numero que decide golpes de estado.
		{
			UHorizontalBox* LoyaltyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = LoyaltyRow->AddChildToHorizontalBox(
				MakeText(WidgetTree, FString::Printf(TEXT("Lealtad %d"), General.Loyalty), 11, LoyaltyColor)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
			if (UHorizontalBoxSlot* S = LoyaltyRow->AddChildToHorizontalBox(
				MakeBar(WidgetTree, General.Loyalty / 100.f, LoyaltyColor, 7.f)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UVerticalBoxSlot* S = GVB->AddChildToVerticalBox(LoyaltyRow))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
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
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(MakePortrait(WidgetTree, General.Id, GovGoldDim, 52.f, 64.f,
			InferGender(General.Name))))
		{
			S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(GVB))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Card->SetContent(CardRow);
		AddColumnChild(CenterBox, Card, 5.f);
		++Index;
	}
	AddColumnChild(CenterBox, MakeActionButton(WidgetTree, this, TEXT("creategeneral"), TEXT("+ CREAR GENERAL"), GovGoldDim, 160.f, 13), 10.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Crear generales cuesta capital y tesoro. Recompensar sube lealtad (cuesta tesoro). Purgar elimina al general, sube tension y baja lealtad militar."),
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

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("DIPLOMACIA CONTINENTAL  (%d naciones)"), Rows.Num())), 6.f);

	// Master/detalle: si hay un pais en gestion mostramos SOLO su ficha limpia (sin buscador ni lista).
	if (!SelectedDiplomacyIso.IsEmpty())
	{
		for (const FDiploRow& Row : Rows)
		{
			if (Row.Nation.Iso.Equals(SelectedDiplomacyIso, ESearchCase::IgnoreCase))
			{
				BuildDiplomacyDetailPanel(Row.Nation);
				return;
			}
		}
		SelectedDiplomacyIso.Reset();   // el pais ya no existe: volvemos a la lista
	}

	// Resumen del continente como insignias de color (se apagan cuando estan a cero).
	{
		UHorizontalBox* Badges = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddSummaryBadge = [&](const FString& Label, int32 Count, const FLinearColor& ActiveBg)
		{
			const bool bActive = Count > 0;
			if (UHorizontalBoxSlot* S = Badges->AddChildToHorizontalBox(MakeBadge(WidgetTree,
				FString::Printf(TEXT("%s  %d"), *Label, Count),
				bActive ? ActiveBg : GovTabIdle,
				bActive ? GovDarkInk : GovMuted)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
		};
		AddSummaryBadge(TEXT("EN GUERRA"), WarCount, GovBad);
		AddSummaryBadge(TEXT("ALIANZAS"), AllyCount, GovGood);
		AddSummaryBadge(TEXT("EMBARGOS"), EmbargoCount, GovGold);
		AddSummaryBadge(TEXT("TRATADOS"), TreatyCount, GovGold);
		AddColumnChild(CenterBox, Badges, 6.f);
	}

	// Barra de herramientas unificada: buscador + filtros + orden dentro de UNA tarjeta,
	// con etiquetas alineadas en columna. Antes eran tres filas sueltas flotando.
	{
		UBorder* Toolbar = MakeCard(WidgetTree, GovHeaderStrip, FMargin(12.f, 10.f));
		UVerticalBox* TVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// Etiqueta de columna fija para que BUSCAR / FILTRO / ORDEN queden alineados.
		auto MakeRowLabel = [&](const TCHAR* Label) -> UWidget*
		{
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(76.f);
			Box->SetContent(MakeText(WidgetTree, Label, 11, GovGold));
			return Box;
		};

		// Fila 1: buscador a lo ancho, campo hundido oscuro.
		{
			UHorizontalBox* SearchRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = SearchRow->AddChildToHorizontalBox(MakeRowLabel(TEXT("BUSCAR"))))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			UBorder* Field = MakeRoundedSurface(WidgetTree, GovBarTrack, FMargin(10.f, 5.f), 6.f);
			UEditableTextBox* SearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			SearchBox->SetText(FText::FromString(DiplomacySearchText));
			SearchBox->SetHintText(FText::FromString(TEXT("Escribe el nombre o ISO del pais y pulsa Enter...")));
			SearchBox->WidgetStyle.TextStyle.Font.Size = 13;
			SearchBox->OnTextCommitted.AddDynamic(this, &UWLGovernmentWidget::OnDiplomacySearchCommitted);
			Field->SetContent(SearchBox);
			if (UHorizontalBoxSlot* S = SearchRow->AddChildToHorizontalBox(Field))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (!DiplomacySearchText.IsEmpty())
			{
				if (UHorizontalBoxSlot* S = SearchRow->AddChildToHorizontalBox(
					MakeActionButton(WidgetTree, this, TEXT("dipsearchclear"), TEXT("LIMPIAR"), GovTabIdle, 84.f, 10)))
				{
					S->SetVerticalAlignment(VAlign_Center);
					S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
				}
			}
			TVB->AddChildToVerticalBox(SearchRow);
		}

		// Filas 2 y 3: chips de filtro y orden con etiqueta alineada; el activo va en dorado.
		auto AddChipRow = [&](const TCHAR* Label, const TCHAR* Verb,
			std::initializer_list<TPair<int32, const TCHAR*>> Defs, int32 Active)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeRowLabel(Label)))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			UWrapBox* Chips = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
			for (const auto& Def : Defs)
			{
				const bool bChipActive = Active == Def.Key;
				if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Chips->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
					FString::Printf(TEXT("%s:%d"), Verb, Def.Key), Def.Value,
					bChipActive ? GovGold : GovTabIdle, 0.f, 10, true,
					bChipActive ? GovDarkInk : GovMuted))))
				{
					S->SetPadding(FMargin(0.f, 0.f, 4.f, 4.f));
				}
			}
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Chips))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			UBorder* RowPad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(0.f, 8.f, 0.f, 0.f));
			RowPad->SetContent(Row);
			TVB->AddChildToVerticalBox(RowPad);
		};
		AddChipRow(TEXT("FILTRO"), TEXT("dipfilter"), {
			{ 0, TEXT("TODOS") }, { 1, TEXT("GUERRA") }, { 2, TEXT("TENSION") }, { 3, TEXT("PAZ") },
			{ 4, TEXT("ALIADOS") }, { 5, TEXT("CON TRATADO") }, { 6, TEXT("EMBARGO") } }, DiplomacyStatusFilter);
		AddChipRow(TEXT("ORDEN"), TEXT("dipsort"), {
			{ 0, TEXT("A-Z") }, { 1, TEXT("OPINION +") }, { 2, TEXT("OPINION -") },
			{ 3, TEXT("TESORO") }, { 4, TEXT("PROVINCIAS") }, { 5, TEXT("ESTADO") } }, DiplomacySortMode);

		Toolbar->SetContent(TVB);
		AddColumnChild(CenterBox, Toolbar, 6.f);
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

	// Listado en DOS columnas (layout FM): 37 naciones ocupan la mitad de scroll.
	UVerticalBox* DipLeft; UVerticalBox* DipRight;
	UHorizontalBox* DipCols = MakeTwoColumnRow(WidgetTree, DipLeft, DipRight);
	int32 Index = 0;
	for (const FDiploRow& Row : Rows)
	{
		const bool bAtWar = Row.Relation.Status == EWLDiplomaticStatus::War;
		UBorder* Card = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(10.f, 6.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeFlag(WidgetTree,
			Row.Nation.Iso, Row.Nation.MapColor, 30.f, 20.f))) { S->SetVerticalAlignment(VAlign_Center); }

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
			TEXT("GESTIONAR"), GovGoldDim, 96.f, 10)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		Card->SetContent(HB);
		AddColumnChild((Index % 2 == 0) ? DipLeft : DipRight, Card, 3.f);
		++Index;
	}
	AddColumnChild(CenterBox, DipCols, 3.f);

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

	const FLinearColor StatusColor = bAtWar ? GovBad : (Relation.Status == EWLDiplomaticStatus::Tension ? GovGold : GovGood);

	// --- Barra de navegacion: VOLVER a la lista. Master/detalle claro. ---
	{
		UHorizontalBox* NavBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = NavBar->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("dipsel:%s"), *Other.Iso), TEXT("←  VOLVER A LA LISTA"), GovTabIdle, 210.f, 12)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		AddColumnChild(CenterBox, NavBar, 8.f);
	}

	// --- Cabecera heroica: bandera grande + nombre + insignia de estado. ---
	{
		UBorder* Hero = MakeCard(WidgetTree, GovHeaderStrip, FMargin(14.f, 12.f));
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeFlag(WidgetTree,
			Other.Iso, Other.MapColor, 66.f, 44.f))) { S->SetVerticalAlignment(VAlign_Center); }

		UVerticalBox* Title = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Title->AddChildToVerticalBox(MakeText(WidgetTree, Other.Name.ToUpper(), 20, GovText));
		Title->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s  ·  %d provincias"), *Other.Iso, Registry->GetProvincesByNation(Other.Iso).Num()),
			12, GovMuted, ETextJustify::Left, true));
		UBorder* TitlePad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(12.f, 0.f, 0.f, 0.f));
		TitlePad->SetContent(Title);
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(TitlePad))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeBadge(WidgetTree,
			FString::Printf(TEXT("%s · %+d"), *DiplomaticStatusToText(Relation.Status), Relation.Opinion),
			StatusColor, GovDarkInk)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Hero->SetContent(Head);
		AddColumnChild(CenterBox, Hero, 10.f);
	}

	// ===== RELACION: estado, tratados y ruta comercial en filas legibles. =====
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("RELACION")), 6.f);
	{
		FString TreatyLine;
		for (const EWLTreatyType Treaty : Relation.Treaties)
		{
			TreatyLine += (TreatyLine.IsEmpty() ? TEXT("") : TEXT(" · ")) + TreatyToText(Treaty);
		}
		if (TreatyLine.IsEmpty()) { TreatyLine = TEXT("Sin tratados"); }

		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Estado"),
			DiplomaticStatusToText(Relation.Status), StatusColor, GovCard), 3.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Opinion"),
			FString::Printf(TEXT("%+d"), Relation.Opinion),
			Relation.Opinion >= 0 ? GovGood : GovBad, GovCardAlt), 3.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Tratados"), TreatyLine, GovText, GovCard), 3.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Ruta comercial"),
			Route.bOpen ? FString::Printf(TEXT("Abierta (x%.2f)"), Route.AccessMultiplier)
			            : FString::Printf(TEXT("Cerrada — %s"), *Route.Reason),
			Route.bOpen ? GovGood : GovBad, GovCardAlt), 3.f);
		if (!Relation.CasusBelli.IsEmpty())
		{
			AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Casus belli"),
				Relation.CasusBelli, GovGold, GovCard), 3.f);
		}
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			bAtWar ? TEXT("EN GUERRA: tus ejercitos pueden atacar; las rutas mutuas estan cortadas.")
			       : TEXT("Sin guerra declarada el combate contra este pais esta bloqueado."),
			11, bAtWar ? GovBad : GovMuted, ETextJustify::Left, true), 5.f);
	}

	// Fila de accion con el MISMO lenguaje visual que MakeStatRow: nombre + descripcion
	// a la izquierda y el boton a la derecha. Todo el panel queda uniforme y legible.
	int32 ActionRowIndex = 0;
	auto AddActionRow = [&](const FString& Title, const FString& Desc,
		const FString& ActionId, const FString& ButtonLabel, const FLinearColor& ButtonBg)
	{
		UBorder* Row = MakeCard(WidgetTree, (ActionRowIndex++ % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 7.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Info->AddChildToVerticalBox(MakeText(WidgetTree, Title, 13, GovText));
		if (!Desc.IsEmpty())
		{
			Info->AddChildToVerticalBox(MakeText(WidgetTree, Desc, 10, GovMuted, ETextJustify::Left, true));
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Info))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(
			MakeActionButton(WidgetTree, this, ActionId, ButtonLabel, ButtonBg, 190.f, 11)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		}
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, 3.f);
	};
	auto HasTreaty = [&Relation](EWLTreatyType Type) { return Relation.Treaties.Contains(Type); };

	// ===== DIPLOMACIA: guerra/paz + tratados. =====
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("DIPLOMACIA")), 10.f);
	ActionRowIndex = 0;
	if (bAtWar)
	{
		AddActionRow(TEXT("Negociar la paz"),
			TEXT("Termina la guerra; se reabren las rutas comerciales mutuas."),
			FString::Printf(TEXT("peace:%s"), *Other.Iso), TEXT("NEGOCIAR PAZ"), GovGoldDim);
	}
	else
	{
		AddActionRow(TEXT("Declarar la guerra"),
			TEXT("Habilita atacar con tus ejercitos; corta rutas y hunde la opinion."),
			FString::Printf(TEXT("war:%s"), *Other.Iso), TEXT("DECLARAR GUERRA"), GovDanger);
	}
	{
		const struct { EWLTreatyType Type; const TCHAR* Title; const TCHAR* Label; const TCHAR* Desc; } TreatyDefs[] = {
			{ EWLTreatyType::TradeAgreement, TEXT("Tratado de comercio"), TEXT("COMERCIO"),
				TEXT("Mejora el acceso comercial mutuo y los ingresos de ruta.") },
			{ EWLTreatyType::NonAggression,  TEXT("Pacto de no agresion"), TEXT("NO AGRESION"),
				TEXT("Compromiso de no atacar; estabiliza la relacion.") },
			{ EWLTreatyType::Alliance,       TEXT("Alianza"), TEXT("ALIANZA"),
				TEXT("Defensa mutua; el mejor tratado, requiere buena opinion.") },
			{ EWLTreatyType::Embargo,        TEXT("Embargo"), TEXT("EMBARGO"),
				TEXT("Corta el comercio con este pais; lo presiona y lo enemista.") },
		};
		for (const auto& Def : TreatyDefs)
		{
			if (HasTreaty(Def.Type))
			{
				AddActionRow(FString::Printf(TEXT("%s  —  VIGENTE"), Def.Title), Def.Desc,
					FString::Printf(TEXT("breaktreaty:%d:%s"), static_cast<int32>(Def.Type), *Other.Iso),
					FString::Printf(TEXT("ROMPER %s"), Def.Label), GovTabIdle);
			}
			else
			{
				AddActionRow(Def.Title, Def.Desc,
					FString::Printf(TEXT("treaty:%d:%s"), static_cast<int32>(Def.Type), *Other.Iso),
					FString::Printf(TEXT("FIRMAR %s"), Def.Label), GovGoldDim);
			}
		}
	}

	// ===== ECONOMIA: ayuda financiera + inversion extranjera. =====
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("ECONOMIA")), 10.f);
	ActionRowIndex = 0;
	AddActionRow(TEXT("Ayuda financiera"),
		TEXT("Envia fondos de tu tesoro para mejorar la opinion de este pais."),
		FString::Printf(TEXT("aid:%s"), *Other.Iso), TEXT("ENVIAR AYUDA"), GovGoldDim);
	{
		int32 FdiShown = 0;
		for (const FWLProvinceData& TargetProvince : Registry->GetProvincesByNation(Other.Iso))
		{
			if (FdiShown >= 2) { break; }
			for (const FWLBuildingData& Candidate : Registry->GetAllBuildings())
			{
				if (!Tick->IsBuildingSupportedInProvince(TargetProvince.Id, Candidate.Id)
					|| Tick->GetProvinceBuildingLevel(TargetProvince.Id, Candidate.Id) > 0)
				{
					continue;
				}
				AddActionRow(FString::Printf(TEXT("Inversion: %s"), *Candidate.Name),
					FString::Printf(TEXT("Construye en %s; genera influencia y retorno economico."), *TargetProvince.Name),
					FString::Printf(TEXT("fdi:%s:%s:%s"), *Other.Iso, *TargetProvince.Id, *Candidate.Id),
					TEXT("INVERTIR"), GovFuture);
				++FdiShown;
				break;   // un candidato por provincia
			}
		}
	}

	// ===== INTELIGENCIA: red de espias + operaciones de intriga. =====
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("INTELIGENCIA")), 10.f);
	AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Red de espionaje"),
		FString::Printf(TEXT("Fuerza %d  ·  Exposicion %d"), Network.NetworkStrength, Network.Exposure),
		Network.Exposure >= 60 ? GovBad : GovText, GovCard), 3.f);
	if (!Network.LastOperationReport.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Ultima operacion"),
			Network.LastOperationReport, GovMuted, GovCardAlt), 3.f);
	}
	if (SpyId.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin espias activos disponibles. Recluta un espia para operar aqui."), 12, GovMuted,
			ETextJustify::Left, true), 5.f);
	}
	else
	{
		ActionRowIndex = 1;
		AddActionRow(TEXT("Ampliar la red"),
			TEXT("Refuerza tu red local: mas fuerza = mas exito en operaciones."),
			FString::Printf(TEXT("spynet:%s"), *Other.Iso), TEXT("AMPLIAR RED"), GovGoldDim);
		const struct { EWLSpyOperationType Type; const TCHAR* Title; const TCHAR* Label; const TCHAR* Desc; } SpyDefs[] = {
			{ EWLSpyOperationType::SabotageEconomy, TEXT("Sabotear la economia"), TEXT("SABOTEAR ECO"),
				TEXT("Dana los ingresos del pais objetivo durante un tiempo.") },
			{ EWLSpyOperationType::SabotageArmy, TEXT("Sabotear el ejercito"), TEXT("SABOTEAR EJERCITO"),
				TEXT("Reduce la moral y la fuerza de sus tropas.") },
			{ EWLSpyOperationType::FundCoup, TEXT("Financiar un golpe"), TEXT("FINANCIAR GOLPE"),
				TEXT("Arriesgado: intenta derribar a su gobierno desde dentro.") },
			{ EWLSpyOperationType::Propaganda, TEXT("Propaganda"), TEXT("PROPAGANDA"),
				TEXT("Mueve la opinion publica del objetivo a tu favor.") },
			{ EWLSpyOperationType::CounterIntelligence, TEXT("Contraespionaje"), TEXT("CONTRAESPIONAJE"),
				TEXT("Caza espias enemigos y baja tu exposicion.") },
		};
		for (const auto& Def : SpyDefs)
		{
			AddActionRow(Def.Title, Def.Desc,
				FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(Def.Type), *Other.Iso),
				Def.Label, GovFuture);
		}
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("La intriga sube tu exposicion; si te descubren, la relacion se hunde y hay incidente diplomatico."),
			11, GovMuted, ETextJustify::Left, true), 5.f);
	}
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

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("PROVINCIA: %s (%s)"), *Province.Name.ToUpper(), *Province.Id)), 6.f);
	FWLProvinceRuntimeState State;
	Tick->GetProvinceState(Province.Id, State);
	AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
		TEXT("Control: %s   ·   Poblacion: %s   ·   Orden publico: %d   ·   Balance: %+lld/mes"),
		*ControllerIso, *GovGroupThousands(State.Population), State.PublicOrder,
		static_cast<long long>(GetCachedProvinceMonthlyBalance(Province.Id))),
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

		UBorder* Card = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UVerticalBox* SVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		if (bHasBuilt)
		{
			const int32 Level = Tick->GetProvinceBuildingLevel(Province.Id, BuiltBuilding.Id);
			UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UWidget* BIcon = MakeBuildingIcon(WidgetTree, BuiltBuilding.Id, 30.f))
			{
				if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(BIcon))
				{
					S->SetVerticalAlignment(VAlign_Center);
					S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
				}
			}
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
	const UWLPoliticalSubsystem* Political = GetPolitical();
	const FString ViewerIso = PlayerIso();

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("REGISTRO DE GOBIERNO")), 6.f);
	if (Political)
	{
		const struct { const TCHAR* Label; int32 Value; } Filters[] = {
			{ TEXT("TODO"), -1 },
			{ TEXT("GOBIERNO"), static_cast<int32>(EWLGovernmentLogCategory::Government) },
			{ TEXT("ECONOMIA"), static_cast<int32>(EWLGovernmentLogCategory::Economy) },
			{ TEXT("DIPLOMACIA"), static_cast<int32>(EWLGovernmentLogCategory::Diplomacy) },
			{ TEXT("MILITAR"), static_cast<int32>(EWLGovernmentLogCategory::Military) },
			{ TEXT("INTEL"), static_cast<int32>(EWLGovernmentLogCategory::Intelligence) },
			{ TEXT("CRISIS"), static_cast<int32>(EWLGovernmentLogCategory::Crisis) },
			{ TEXT("EVENTOS"), static_cast<int32>(EWLGovernmentLogCategory::Event) },
		};
		UWrapBox* FilterRow = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		for (const auto& Filter : Filters)
		{
			const bool bActive = RecordsCategoryFilter == Filter.Value;
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(FilterRow->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("recordsfilter:%d"), Filter.Value), Filter.Label,
				bActive ? GovGold : GovTabIdle, 0.f, 10, true,
				bActive ? GovDarkInk : GovMuted))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
			}
		}
		AddColumnChild(CenterBox, FilterRow, 6.f);

		TArray<FWLGovernmentLogEntry> LogEntries = Political->GetGovernmentLog(ViewerIso);
		const int32 TotalVisibleEntries = LogEntries.Num();
		if (RecordsCategoryFilter >= 0)
		{
			LogEntries.RemoveAll([this](const FWLGovernmentLogEntry& Entry)
			{
				return static_cast<int32>(Entry.Category) != RecordsCategoryFilter;
			});
		}
		if (LogEntries.Num() == 0)
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree,
				RecordsCategoryFilter < 0
					? TEXT("Sin registros todavia. Las decisiones, eventos, economia y crisis quedaran fechadas aqui.")
					: TEXT("Sin registros en esta categoria."),
				13, GovMuted, ETextJustify::Left, true), 8.f);
		}
		else
		{
			int32 GovernmentCount = 0;
			int32 EconomyCount = 0;
			int32 DiplomacyCount = 0;
			int32 CrisisCount = 0;
			for (const FWLGovernmentLogEntry& Entry : LogEntries)
			{
				GovernmentCount += Entry.Category == EWLGovernmentLogCategory::Government || Entry.Category == EWLGovernmentLogCategory::Event ? 1 : 0;
				EconomyCount += Entry.Category == EWLGovernmentLogCategory::Economy ? 1 : 0;
				DiplomacyCount += Entry.Category == EWLGovernmentLogCategory::Diplomacy ? 1 : 0;
				CrisisCount += Entry.Category == EWLGovernmentLogCategory::Crisis ? 1 : 0;
			}
			AddColumnChild(CenterBox, MakeText(WidgetTree,
				FString::Printf(TEXT("Historial persistente: mostrando %d de %d entradas · gobierno %d · economia %d · diplomacia %d · crisis %d"),
					LogEntries.Num(), TotalVisibleEntries, GovernmentCount, EconomyCount, DiplomacyCount, CrisisCount),
				12, GovMuted, ETextJustify::Left, true), 8.f);

			int32 Index = 0;
			for (const FWLGovernmentLogEntry& Entry : LogEntries)
			{
				if (Index >= 30)
				{
					break;
				}
				UBorder* Row = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
				UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
				UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				const FString Date = FString::Printf(TEXT("%02d/%02d/%04d"), Entry.Day, Entry.Month, Entry.Year);
				if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
					FString::Printf(TEXT("%s  ·  %s"), *Date, *Entry.Title), 13, GovText, ETextJustify::Left, true)))
				{
					S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					S->SetVerticalAlignment(VAlign_Center);
				}
				const bool bEconomyEntry = Entry.Category == EWLGovernmentLogCategory::Economy;
				const FLinearColor BadgeColor = Entry.Severity >= 8 ? GovDanger
					: bEconomyEntry ? GovGood
					: Entry.Category == EWLGovernmentLogCategory::Crisis ? GovBad
					: GovHeaderStrip;
				Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, GovernmentLogCategoryToText(Entry.Category), BadgeColor,
					bEconomyEntry ? GovDarkInk : GovGold));
				VB->AddChildToVerticalBox(Head);
				if (!Entry.Body.IsEmpty() && Entry.Body != Entry.Title)
				{
					VB->AddChildToVerticalBox(MakeText(WidgetTree, Entry.Body, 12, GovMuted, ETextJustify::Left, true));
				}
				FString Meta;
				if (!Entry.NationIso.IsEmpty())
				{
					Meta += FString::Printf(TEXT("Nacion %s"), *Entry.NationIso);
				}
				if (!Entry.TargetIso.IsEmpty())
				{
					Meta += (Meta.IsEmpty() ? TEXT("") : TEXT(" · ")) + FString::Printf(TEXT("Objetivo %s"), *Entry.TargetIso);
				}
				if (!Entry.Source.IsEmpty())
				{
					Meta += (Meta.IsEmpty() ? TEXT("") : TEXT(" · ")) + Entry.Source;
				}
				if (!Meta.IsEmpty())
				{
					VB->AddChildToVerticalBox(MakeText(WidgetTree, Meta, 11, GovGoldDim, ETextJustify::Left, true));
				}
				Row->SetContent(VB);
				AddColumnChild(CenterBox, Row, 5.f);
				++Index;
			}
		}
	}

	const TArray<FString> Reports = Tick ? Tick->GetLastEconomicAIReports() : TArray<FString>();
	if (Reports.Num() > 0)
	{
		AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("ULTIMO CIERRE ECONOMICO IA")), 18.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Estos movimientos tambien quedan archivados en el registro persistente."),
			12, GovMuted, ETextJustify::Left, true), 6.f);
		int32 Index = 0;
		for (const FString& R : Reports)
		{
			if (Index >= 8)
			{
				break;
			}
			UBorder* Row = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
			Row->SetContent(MakeText(WidgetTree, R, 14, GovText, ETextJustify::Left, true));
			AddColumnChild(CenterBox, Row, 5.f);
			++Index;
		}
	}

	// Gobierno P2: estimaciones de inteligencia sobre lo que persigue cada gobierno IA de America.
	BuildAIPlansPanel();

	// Diagnostico jugable de dilemas sistemicos del regimen.
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
	RebuildCenter(false);
	// Fade rapido del contenido SOLO al cambiar de pestana (no en cada accion, para no parpadear).
	if (CenterScroll)
	{
		CenterScroll->SetRenderOpacity(0.f);
		ContentAnimTime = 0.f;
		bContentAnimating = true;
	}
}

void UWLGovernmentWidget::RefreshTabButtonStyles()
{
	// Segmentado: activa = oro solido + texto oscuro; inactivas = texto apagado sobre el riel.
	for (int32 i = 0; i < TabButtons.Num(); ++i)
	{
		const bool bActive = static_cast<int32>(ActiveTab) == i;
		if (TabButtons[i])
		{
			TabButtons[i]->SetBackgroundColor(bActive ? GovGold : GovBarTrack);
		}
		if (TabLabels.IsValidIndex(i) && TabLabels[i])
		{
			TabLabels[i]->SetColorAndOpacity(FSlateColor(bActive ? GovDarkInk : GovMuted));
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
	UWLPoliticalSubsystem* Political = GetPolitical();
	InvalidateDataSnapshot();

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
	if (Verb == TEXT("recordsfilter"))
	{
		RecordsCategoryFilter = FCString::Atoi(*Arg1);
		PendingConfirmId.Reset();
		LastActionMessage.Reset();
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

	if (Verb == TEXT("difficulty"))
	{
		if (UWLBalanceSubsystem* Balance = GetBalance())
		{
			const EWLAIDifficulty Level = static_cast<EWLAIDifficulty>(FCString::Atoi(*Arg1));
			Balance->SetAIDifficulty(Level);
			bOk = true;
			Message = FString::Printf(TEXT("Dificultad fijada en %s."),
				Level == EWLAIDifficulty::Easy ? TEXT("Facil") : (Level == EWLAIDifficulty::Hard ? TEXT("Dificil") : TEXT("Medio")));
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
	else if (Political)
	{
		FWLGovernmentActionRequest Request;
		Request.NationIso = Iso;
		Request.ActionId = ActionId;
		Request.ContextId = ProvinceContextId;
		Request.Priorities = DraftAgenda;
		if (Verb == TEXT("spynet") || Verb == TEXT("spy"))
		{
			Request.AgentCharacterId = FindPlayerSpyId();
		}

		bOk = Political->ExecuteGovernmentAction(Request, Message);
		if (bOk && Verb == TEXT("agendaset"))
		{
			bDraftAgendaLoaded = false;   // re-sincroniza el borrador con lo que confirmo el backend
		}
		if (bOk && Verb == TEXT("appointc"))
		{
			CompareOfficeContext = -1;
		}
		if (Verb == TEXT("autoresolve"))
		{
			BattleAttackerId.Reset();
			BattleDefenderId.Reset();
		}
	}
	else
	{
		Message = FString::Printf(TEXT("Sistema politico no disponible para accion: %s"), *ActionId);
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

void UWLGovernmentWidget::OnTaxDown() { HandleAction(TEXT("taxdown")); }
void UWLGovernmentWidget::OnTaxUp()   { HandleAction(TEXT("taxup")); }

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
	HandleAction(DeltaPercent >= 0 ? TEXT("taxup") : TEXT("taxdown"));
}

UWLGovernmentWidget::FSummary UWLGovernmentWidget::BuildSummary() const
{
	return GetCachedSummary();
}

FString UWLGovernmentWidget::PlayerIso() const
{
	const UWLCampaignGameInstance* GI = GetCampaignGI();
	return GI ? GI->GetSelectedNationIso() : FString();
}

FString UWLGovernmentWidget::PlayerLeaderName() const
{
	UWLCampaignGameInstance* GI = GetCampaignGI();
	FWLNationData Nation;
	return (GI && GI->GetSelectedNation(Nation)) ? Nation.Leader : FString();
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
