// Copyright World Leader project. See ROADMAP.md.
//
// Gobierno P1/P2 en la ventana GOBIERNO: la pestana POLITICA es un hub con
// subsecciones (PODER · AGENDA · PROGRAMAS · LEYES · CONGRESO · ELECCIONES ·
// MEDIOS · REGIONES · CRISIS) y ALTO MANDO/RESUMEN/REGISTROS ganan paneles de
// gabinete vivo, comparador de candidatos, perfiles politicos, IA politica y
// calibracion. Todo lee endpoints de UWLPoliticalSubsystem/UWLCharacterSubsystem;
// ninguna regla de simulacion vive aqui.

#include "UI/WLGovernmentWidget.h"
#include "Balance/WLBalanceSubsystem.h"
#include "Balance/WLBalanceTypes.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Core/WLCharacterTypes.h"
#include "Core/WLGameTypes.h"
#include "Core/WLPoliticalTypes.h"
#include "Military/WLMilitarySubsystem.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

#include "UI/WLGovernmentWidgetShared.h"

using namespace WLGovUI;

namespace
{
	// --- Texto de los enums de Gobierno P1/P2 (copy de UI, sin acentos como el resto del widget) ---

	FString PriorityToText(EWLGovernmentPriority Priority)
	{
		switch (Priority)
		{
		case EWLGovernmentPriority::Security:          return TEXT("Seguridad");
		case EWLGovernmentPriority::Growth:            return TEXT("Crecimiento");
		case EWLGovernmentPriority::Austerity:         return TEXT("Austeridad");
		case EWLGovernmentPriority::Industrialization: return TEXT("Industrializacion");
		case EWLGovernmentPriority::Diplomacy:         return TEXT("Diplomacia");
		case EWLGovernmentPriority::Control:           return TEXT("Control interno");
		default:                                       return TEXT("Prioridad");
		}
	}

	FString PriorityEffectText(EWLGovernmentPriority Priority)
	{
		switch (Priority)
		{
		case EWLGovernmentPriority::Security:
			return TEXT("Refuerza orden publico y contenta a los militares; presiona a sindicatos y estudiantes.");
		case EWLGovernmentPriority::Growth:
			return TEXT("Empuja crecimiento e inversion; contenta a empresarios y clase media.");
		case EWLGovernmentPriority::Austerity:
			return TEXT("Contiene gasto y deuda; drena apoyo de trabajadores y sindicatos.");
		case EWLGovernmentPriority::Industrialization:
			return TEXT("Acelera sectores industriales; exige capacidad estatal y presupuesto.");
		case EWLGovernmentPriority::Diplomacy:
			return TEXT("Mejora opinion exterior y comercio; los halcones internos desconfian.");
		case EWLGovernmentPriority::Control:
			return TEXT("Baja el riesgo de golpe y de oposicion; erosiona legitimidad si se abusa.");
		default:
			return FString();
		}
	}

	FString ReformAreaToText(EWLPolicyReformArea Area)
	{
		switch (Area)
		{
		case EWLPolicyReformArea::Tax:              return TEXT("Tributaria");
		case EWLPolicyReformArea::Labor:            return TEXT("Laboral");
		case EWLPolicyReformArea::Security:         return TEXT("Seguridad");
		case EWLPolicyReformArea::Education:        return TEXT("Educacion");
		case EWLPolicyReformArea::Health:           return TEXT("Salud");
		case EWLPolicyReformArea::Decentralization: return TEXT("Descentralizacion");
		case EWLPolicyReformArea::Military:         return TEXT("Militar");
		case EWLPolicyReformArea::Justice:          return TEXT("Justicia");
		case EWLPolicyReformArea::Media:            return TEXT("Medios");
		case EWLPolicyReformArea::Energy:           return TEXT("Energia");
		case EWLPolicyReformArea::Trade:            return TEXT("Comercio");
		case EWLPolicyReformArea::Constitution:     return TEXT("Constitucion");
		default:                                    return TEXT("Reforma");
		}
	}

	FString IdeologyToText(EWLPoliticalIdeology Ideology)
	{
		switch (Ideology)
		{
		case EWLPoliticalIdeology::Conservative:   return TEXT("Conservadora");
		case EWLPoliticalIdeology::Liberal:        return TEXT("Liberal");
		case EWLPoliticalIdeology::SocialDemocrat: return TEXT("Socialdemocrata");
		case EWLPoliticalIdeology::Nationalist:    return TEXT("Nacionalista");
		case EWLPoliticalIdeology::Technocratic:   return TEXT("Tecnocrata");
		case EWLPoliticalIdeology::Populist:       return TEXT("Populista");
		case EWLPoliticalIdeology::Regionalist:    return TEXT("Regionalista");
		default:                                   return TEXT("Ideologia");
		}
	}

	FString PartyRoleToText(EWLPartyRole Role)
	{
		switch (Role)
		{
		case EWLPartyRole::Ruling:         return TEXT("Oficialista");
		case EWLPartyRole::Ally:           return TEXT("Aliado");
		case EWLPartyRole::SoftOpposition: return TEXT("Oposicion blanda");
		case EWLPartyRole::HardOpposition: return TEXT("Oposicion dura");
		default:                           return TEXT("Bancada");
		}
	}

	FLinearColor PartyRoleColor(EWLPartyRole Role)
	{
		switch (Role)
		{
		case EWLPartyRole::Ruling:         return GovGold;
		case EWLPartyRole::Ally:           return GovGood;
		case EWLPartyRole::SoftOpposition: return GovGoldDim;
		case EWLPartyRole::HardOpposition: return GovBad;
		default:                           return GovMuted;
		}
	}

	FString ElectionPhaseToText(EWLElectionPhase Phase)
	{
		switch (Phase)
		{
		case EWLElectionPhase::Governing:   return TEXT("Gobierno");
		case EWLElectionPhase::PreCampaign: return TEXT("Precampania");
		case EWLElectionPhase::Campaign:    return TEXT("Campania");
		case EWLElectionPhase::Election:    return TEXT("Eleccion");
		case EWLElectionPhase::Transition:  return TEXT("Transicion");
		case EWLElectionPhase::Crisis:      return TEXT("Crisis constitucional");
		default:                            return TEXT("Fase");
		}
	}

	struct FPatronageActionUI { EWLPatronageActionType Action; const TCHAR* Label; const TCHAR* Preview; };
	const FPatronageActionUI PatronageActions[] = {
		{ EWLPatronageActionType::AppointLoyalist, TEXT("NOMBRAR LEAL"),
			TEXT("Coloca a un incondicional: sube tu control y la presion clientelista.") },
		{ EWLPatronageActionType::AwardContract, TEXT("CONCEDER CONTRATO"),
			TEXT("Compra apoyo empresarial con obra publica: sube corrupcion de contratos.") },
		{ EWLPatronageActionType::GrantFavor, TEXT("REPARTIR FAVOR"),
			TEXT("Un favor puntual a cambio de lealtad: alimenta la red clientelar.") },
		{ EWLPatronageActionType::FundGovernor, TEXT("FINANCIAR GOBERNADOR"),
			TEXT("Dinero a una maquinaria regional: obediencia hoy, escandalo manana.") },
		{ EWLPatronageActionType::GrantConcession, TEXT("OTORGAR CONCESION"),
			TEXT("Entrega una concesion estrategica: apoyo fuerte y backlash publico.") },
	};

	struct FMediaActionUI { EWLMediaActionType Action; const TCHAR* Label; const TCHAR* Preview; };
	const FMediaActionUI MediaActions[] = {
		{ EWLMediaActionType::PressBriefing, TEXT("RUEDA DE PRENSA"),
			TEXT("Explica tu gestion: aprobacion modesta sin coste de legitimidad.") },
		{ EWLMediaActionType::StateBroadcast, TEXT("CADENA NACIONAL"),
			TEXT("Mensaje directo al pais: mas alcance, riesgo de sobreexposicion.") },
		{ EWLMediaActionType::Propaganda, TEXT("PROPAGANDA INTERNA"),
			TEXT("Narrativa oficialista sostenida: sube alcance propagandistico y erosiona prensa libre.") },
		{ EWLMediaActionType::Censorship, TEXT("CENSURA"),
			TEXT("Silencia criticos: control mediatico a costa de legitimidad y backlash.") },
		{ EWLMediaActionType::CounterFakeNews, TEXT("CONTRA FAKE NEWS"),
			TEXT("Desmiente campanas de desinformacion: baja la presion de fake news.") },
	};

	struct FRegionActionUI { EWLRegionPolicyActionType Action; const TCHAR* Label; const TCHAR* Preview; };
	const FRegionActionUI RegionActions[] = {
		{ EWLRegionPolicyActionType::AppointGovernor, TEXT("NOMBRAR GOBERNADOR"),
			TEXT("Reemplaza al gobernador por uno alineado: obediencia sube, protesta local posible.") },
		{ EWLRegionPolicyActionType::RegionalInvestment, TEXT("INVERSION REGIONAL"),
			TEXT("Obras y presupuesto para la region: baja protesta y sube control central.") },
		{ EWLRegionPolicyActionType::AutonomyDeal, TEXT("PACTO AUTONOMICO"),
			TEXT("Cede autonomia a cambio de paz: baja secesion, baja autoridad central.") },
		{ EWLRegionPolicyActionType::SecurityOperation, TEXT("OPERACION DE SEGURIDAD"),
			TEXT("Fuerza del Estado en la region: control ahora, memoria de represion despues.") },
	};

	FString CrisisTypeToText(EWLCrisisChainType Type)
	{
		switch (Type)
		{
		case EWLCrisisChainType::NationalProtest:   return TEXT("Paro nacional");
		case EWLCrisisChainType::CorruptionScandal: return TEXT("Escandalo de corrupcion");
		case EWLCrisisChainType::MilitaryCrisis:    return TEXT("Crisis militar");
		case EWLCrisisChainType::DebtCrisis:        return TEXT("Crisis de deuda");
		case EWLCrisisChainType::StudentProtest:    return TEXT("Protesta estudiantil");
		case EWLCrisisChainType::OilStrike:         return TEXT("Huelga petrolera");
		case EWLCrisisChainType::BorderCrisis:      return TEXT("Crisis fronteriza");
		case EWLCrisisChainType::Impeachment:       return TEXT("Juicio politico");
		case EWLCrisisChainType::SoftCoup:          return TEXT("Golpe blando");
		case EWLCrisisChainType::StateOfException:  return TEXT("Estado de excepcion");
		default:                                    return TEXT("Crisis");
		}
	}

	FString AIObjectiveToText(EWLGovernmentAIObjective Objective)
	{
		switch (Objective)
		{
		case EWLGovernmentAIObjective::Stabilize:     return TEXT("Estabilizar");
		case EWLGovernmentAIObjective::Expand:        return TEXT("Expandirse");
		case EWLGovernmentAIObjective::Borrow:        return TEXT("Endeudarse");
		case EWLGovernmentAIObjective::Militarize:    return TEXT("Militarizar");
		case EWLGovernmentAIObjective::Align:         return TEXT("Alinear bloque");
		case EWLGovernmentAIObjective::Industrialize: return TEXT("Industrializar");
		default:                                      return TEXT("Objetivo");
		}
	}

	FString PublicGroupToText(EWLPublicGroup Group)
	{
		switch (Group)
		{
		case EWLPublicGroup::Business:    return TEXT("Empresarios");
		case EWLPublicGroup::Military:    return TEXT("Militares");
		case EWLPublicGroup::Workers:     return TEXT("Trabajadores");
		case EWLPublicGroup::Regions:     return TEXT("Regiones");
		case EWLPublicGroup::MiddleClass: return TEXT("Clase media");
		case EWLPublicGroup::Unions:      return TEXT("Sindicatos");
		default:                          return TEXT("Grupo");
		}
	}

	const EWLMinisterOffice AllOffices[] = {
		EWLMinisterOffice::Economy, EWLMinisterOffice::Defense, EWLMinisterOffice::Interior,
		EWLMinisterOffice::Foreign, EWLMinisterOffice::Intelligence };
}

// ---------------------------------------------------------------------------
// POLITICA: hub con subsecciones
// ---------------------------------------------------------------------------

void UWLGovernmentWidget::BuildPoliticsTab()
{
	BuildPoliticsHeader();
	switch (PoliticsSection)
	{
	case EWLPoliticsSection::Power:     BuildPoliticsPowerSection();     break;
	case EWLPoliticsSection::Agenda:    BuildPoliticsAgendaSection();    break;
	case EWLPoliticsSection::Programs:  BuildPoliticsProgramsSection();  break;
	case EWLPoliticsSection::Laws:      BuildPoliticsLawsSection();      break;
	case EWLPoliticsSection::Congress:  BuildPoliticsCongressSection();  break;
	case EWLPoliticsSection::Elections: BuildPoliticsElectionsSection(); break;
	case EWLPoliticsSection::Media:     BuildPoliticsMediaSection();     break;
	case EWLPoliticsSection::Regions:   BuildPoliticsRegionsSection();   break;
	case EWLPoliticsSection::Crisis:    BuildPoliticsCrisisSection();    break;
	}
}

// Pulso politico (capital, aprobacion, legitimidad, eleccion, crisis) + chips de subseccion.
void UWLGovernmentWidget::BuildPoliticsHeader()
{
	const FString Iso = PlayerIso();
	const UWLPoliticalSubsystem* Political = GetPolitical();
	const UWLCharacterSubsystem* Characters = GetCharacters();

	if (Political && Characters && !Iso.IsEmpty())
	{
		const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
		const FWLMediaPublicOpinionState Media = Political->GetMediaPublicOpinion(Iso);
		const FWLElectionState Election = Political->GetElectionState(Iso);
		int32 ActiveCrises = 0;
		for (const FWLCrisisChainState& Chain : Political->GetActiveCrisisChains(Iso))
		{
			if (!Chain.bResolved)
			{
				++ActiveCrises;
			}
		}

		// Pulso politico como tarjetas de metrica con icono (mismo lenguaje que RESUMEN y ALTO MANDO).
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		Grid->SetSlotPadding(FMargin(4.f));
		auto Place = [&](int32 C, UBorder* Card)
		{
			if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, 0, C)) { S->SetHorizontalAlignment(HAlign_Fill); }
		};
		Place(0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Capital, GovGold,
			TEXT("Capital politico"), FString::Printf(TEXT("%d"), Stats.PoliticalCapital), GovText));
		Place(1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Approval, Media.PresidentialApproval < 40 ? GovBad : GovGood,
			TEXT("Aprobacion"), FString::Printf(TEXT("%d%%"), Media.PresidentialApproval),
			Media.PresidentialApproval < 40 ? GovBad : GovGood));
		Place(2, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Order, Election.Legitimacy < 40 ? GovBad : GovGoldDim,
			TEXT("Legitimidad"), FString::Printf(TEXT("%d"), Election.Legitimacy),
			Election.Legitimacy < 40 ? GovBad : GovText));
		Place(3, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Politics, FLinearColor(0.55f, 0.68f, 0.95f),
			TEXT("Eleccion"), FString::Printf(TEXT("%d meses"), Election.MonthsToElection), GovText));
		Place(4, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Crisis, ActiveCrises > 0 ? GovBad : GovMuted,
			TEXT("Crisis activas"), FString::Printf(TEXT("%d"), ActiveCrises),
			ActiveCrises > 0 ? GovBad : GovText));
		AddColumnChild(CenterBox, Grid, 4.f);
	}

	// Chips de subseccion (WrapBox: en 1600x900 caben en una fila; en menos se parten sin solaparse).
	const struct { EWLPoliticsSection Section; const TCHAR* Label; } Sections[] = {
		{ EWLPoliticsSection::Power,     TEXT("PODER") },
		{ EWLPoliticsSection::Agenda,    TEXT("AGENDA") },
		{ EWLPoliticsSection::Programs,  TEXT("PROGRAMAS") },
		{ EWLPoliticsSection::Laws,      TEXT("LEYES") },
		{ EWLPoliticsSection::Congress,  TEXT("CONGRESO") },
		{ EWLPoliticsSection::Elections, TEXT("ELECCIONES") },
		{ EWLPoliticsSection::Media,     TEXT("MEDIOS") },
		{ EWLPoliticsSection::Regions,   TEXT("REGIONES") },
		{ EWLPoliticsSection::Crisis,    TEXT("CRISIS") },
	};
	UWrapBox* Chips = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	for (const auto& Def : Sections)
	{
		const bool bActive = PoliticsSection == Def.Section;
		if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Chips->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("polsec:%d"), static_cast<int32>(Def.Section)),
			Def.Label, bActive ? GovGold : GovTabIdle, 92.f, 11, true,
			bActive ? GovDarkInk : GovMuted))))
		{
			S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
		}
	}
	AddColumnChild(CenterBox, Chips, 8.f);
}

// PODER: orden publico, golpe, grupos sociales, capacidad estatal y eventos pendientes.
void UWLGovernmentWidget::BuildPoliticsPowerSection()
{
	const FSummary Sum = BuildSummary();
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos politicos."), 14, GovMuted), 10.f);
		return;
	}

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("PODER INTERNO")), 6.f);

	// Los DOS numeros que deciden si sigues en el poder, como heroes lado a lado.
	const FWLInternalPowerState Power = Political->GetInternalPower(Iso);
	{
		UUniformGridPanel* HeroGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		HeroGrid->SetSlotPadding(FMargin(5.f, 0.f));
		if (UUniformGridSlot* S = HeroGrid->AddChildToUniformGrid(MakeHeroGauge(WidgetTree,
			TEXT("Orden publico nacional"), Sum.AveragePublicOrder, SupportColor(Sum.AveragePublicOrder),
			TEXT("Media de tus provincias. Bajo 35 el pais entra en zona de revuelta.")), 0, 0))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
		if (UUniformGridSlot* S = HeroGrid->AddChildToUniformGrid(MakeHeroGauge(WidgetTree,
			TEXT("Riesgo de golpe de estado"), Power.CoupRisk, RiskColor(Power.CoupRisk),
			FString::Printf(TEXT("Oposicion: fuerza %d · popularidad %d · financiacion externa %d"),
				Power.OppositionStrength, Power.OppositionPopularity, Power.ExternalCoupFunding)), 0, 1))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
		AddColumnChild(CenterBox, HeroGrid, 8.f);
	}

	if (!Power.LastCoupReport.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree,
			FString::Printf(TEXT("Ultimo golpe: %s"), *Power.LastCoupReport),
			Power.bLastCoupSucceeded ? GovBad : GovMuted), 6.f);
	}

	// F2.4: reprimir como fila de accion (titulo + consecuencias + boton), no un boton suelto.
	{
		UBorder* Row = MakeCard(WidgetTree, GovCardAlt, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Info->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Reprimir a la oposicion"), 13, GovText));
		Info->AddChildToVerticalBox(MakeText(WidgetTree,
			TEXT("Baja su fuerza a costa de orden publico, tesoro y memoria politica. Generales: en ALTO MANDO."),
			10, GovMuted, ETextJustify::Left, true));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Info))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(
			MakeActionButton(WidgetTree, this, TEXT("repress"), TEXT("REPRIMIR"), GovDanger, 170.f, 12)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		}
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, 8.f);
	}

	// F5.5: agenda de rasgos del lider como insignias (alteran opciones de eventos).
	{
		const TArray<FString> Agenda = Political->GetLeaderAgendaTraits(Iso);
		if (Agenda.Num() > 0)
		{
			UHorizontalBox* TraitRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = TraitRow->AddChildToHorizontalBox(MakePortrait(WidgetTree,
				FString::Printf(TEXT("%s-LEADER-INCUMBENT"), *Iso), GovGold, 40.f, 50.f)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 9.f, 0.f));
			}
			if (UHorizontalBoxSlot* S = TraitRow->AddChildToHorizontalBox(
				MakeText(WidgetTree, TEXT("Rasgos del lider:"), 12, GovMuted)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
			for (const FString& Trait : Agenda)
			{
				if (UHorizontalBoxSlot* S = TraitRow->AddChildToHorizontalBox(
					MakeBadge(WidgetTree, Trait.ToUpper(), GovHeaderStrip, GovGold)))
				{
					S->SetVerticalAlignment(VAlign_Center);
					S->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
				}
			}
			if (UHorizontalBoxSlot* S = TraitRow->AddChildToHorizontalBox(
				MakeText(WidgetTree, TEXT("— alteran las opciones de los eventos"), 11, GovMuted)))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			AddColumnChild(CenterBox, TraitRow, 8.f);
		}
	}

	// Gobierno P1: apoyo y presion de los grupos sociales.
	{
		const TArray<FWLPublicGroupSupportState> Groups = Political->GetPublicGroups(Iso);
		AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("GRUPOS SOCIALES")), 20.f);
		if (Groups.Num() == 0)
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos de grupos sociales."), 13, GovMuted), 8.f);
		}
		// Tablero 2 columnas: cada grupo es una celda compacta (nombre + apoyo + barras).
		// El motivo del ultimo cambio va en tooltip: la pantalla respira y el detalle sigue ahi.
		UBorder* Panel = MakeCard(WidgetTree, GovCard, FMargin(14.f, 11.f));
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		Grid->SetSlotPadding(FMargin(12.f, 8.f));
		int32 Cell = 0;
		for (const FWLPublicGroupSupportState& Group : Groups)
		{
			UVerticalBox* GVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* Name = MakeText(WidgetTree, PublicGroupToText(Group.Group), 12, GovText);
			if (!Group.LastShiftReason.IsEmpty())
			{
				Name->SetToolTipText(FText::FromString(Group.LastShiftReason));
			}
			if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(Name))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (Group.Pressure > 0)
			{
				if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeBadge(WidgetTree,
					FString::Printf(TEXT("PRESION %d"), Group.Pressure),
					Group.Pressure >= 60 ? GovBad : GovTabIdle,
					Group.Pressure >= 60 ? GovDarkInk : GovMuted)))
				{
					S->SetVerticalAlignment(VAlign_Center);
					S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				}
			}
			Head->AddChildToHorizontalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("%d"), Group.Support), 15, SupportColor(Group.Support), ETextJustify::Right));
			GVB->AddChildToVerticalBox(Head);
			if (UVerticalBoxSlot* S = GVB->AddChildToVerticalBox(MakeBar(WidgetTree, Group.Support / 100.f, SupportColor(Group.Support), 8.f)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
			if (Group.Pressure > 0)
			{
				if (UVerticalBoxSlot* S = GVB->AddChildToVerticalBox(MakeBar(WidgetTree, Group.Pressure / 100.f, GovBad, 4.f)))
				{
					S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
				}
			}
			if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(GVB, Cell / 2, Cell % 2))
			{
				S->SetHorizontalAlignment(HAlign_Fill);
			}
			++Cell;
		}
		Panel->SetContent(Grid);
		AddColumnChild(CenterBox, Panel, 8.f);
	}

	// Gobierno P1: capacidad estatal (burocracia, corrupcion, eficiencia, autoridad, riesgo de fallo).
	{
		const FWLStateCapacityState Capacity = Political->GetStateCapacity(Iso);
		AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("CAPACIDAD ESTATAL")), 20.f);
		AddColumnChild(CenterBox, MakeGaugePanel(WidgetTree, {
			{ TEXT("Burocracia"), Capacity.Bureaucracy, SupportColor(Capacity.Bureaucracy),
				TEXT("Cuanto Estado tienes para ejecutar. Bajo: los programas fallan.") },
			{ TEXT("Corrupcion"), Capacity.Corruption, RiskColor(Capacity.Corruption),
				TEXT("Se roba parte de lo que gastas y pudre la capacidad estatal.") },
			{ TEXT("Eficiencia administrativa"), Capacity.AdministrativeEfficiency, SupportColor(Capacity.AdministrativeEfficiency),
				TEXT("Requisito de muchas reformas P2. Sube con burocracia sana y baja corrupcion.") },
			{ TEXT("Autoridad central"), Capacity.CentralAuthority, SupportColor(Capacity.CentralAuthority),
				TEXT("Cuanto obedecen las regiones al gobierno central.") },
			{ TEXT("Riesgo de fallo de politicas"), Capacity.PolicyFailureRisk, RiskColor(Capacity.PolicyFailureRisk),
				TEXT("Probabilidad de que programas y reformas se ejecuten mal.") } }), 8.f);
		if (!Capacity.LastReport.IsEmpty())
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree, Capacity.LastReport, 12, GovMuted, ETextJustify::Left, true), 6.f);
		}
	}

	// F5: eventos politicos pendientes con opciones (coste/impacto por opcion + confirmacion).
	const TArray<FWLPoliticalEventInstance> Events = Political->GetQueuedEvents(Iso);
	int32 PendingCount = 0;
	for (const FWLPoliticalEventInstance& Event : Events)
	{
		if (!Event.bResolved)
		{
			++PendingCount;
		}
	}
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("EVENTOS  (%d pendientes)"), PendingCount)), 20.f);
	if (PendingCount == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin eventos pendientes. Se disparan al cierre mensual segun la situacion interna."),
			13, GovMuted, ETextJustify::Left, true), 8.f);
	}
	for (const FWLPoliticalEventInstance& Event : Events)
	{
		if (Event.bResolved)
		{
			continue;
		}
		UBorder* Card = MakeCard(WidgetTree, GovCard, FMargin(14.f, 12.f));
		UVerticalBox* EVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		EVB->AddChildToVerticalBox(MakeText(WidgetTree, Event.Title, 16, GovGold, ETextJustify::Left, true));
		if (UVerticalBoxSlot* S = EVB->AddChildToVerticalBox(MakeText(WidgetTree, Event.Body, 13, GovText, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		for (const FWLPoliticalEventOption& Option : Event.Options)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("event:%s:%s"), *Event.InstanceId, *Option.OptionId),
				Option.Label, GovGoldDim, 150.f, 13)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			}
			FString Effects;
			if (Option.PoliticalCapitalDelta != 0) Effects += FString::Printf(TEXT("Capital %+d  "), Option.PoliticalCapitalDelta);
			if (Option.TreasuryDelta != 0)         Effects += FString::Printf(TEXT("Tesoro %+lld  "), static_cast<long long>(Option.TreasuryDelta));
			if (Option.PublicOrderDelta != 0)      Effects += FString::Printf(TEXT("Orden %+d  "), Option.PublicOrderDelta);
			if (Option.OppositionDelta != 0)       Effects += FString::Printf(TEXT("Oposicion %+d  "), Option.OppositionDelta);
			if (Option.RelationDelta != 0)         Effects += FString::Printf(TEXT("Relacion %+d  "), Option.RelationDelta);
			if (!Option.MarketShockGoodId.IsEmpty()) Effects += TEXT("Shock de mercado");
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeText(WidgetTree, Effects, 12, GovMuted, ETextJustify::Left, true)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UVerticalBoxSlot* S = EVB->AddChildToVerticalBox(Row))
			{
				S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			}
		}
		Card->SetContent(EVB);
		AddColumnChild(CenterBox, Card, 8.f);
	}
}

// AGENDA: selector de hasta 3 prioridades nacionales con preview de efectos.
void UWLGovernmentWidget::BuildPoliticsAgendaSection()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos de agenda."), 14, GovMuted), 10.f);
		return;
	}

	const FWLGovernmentAgendaState Agenda = Political->GetGovernmentAgenda(Iso);
	if (!bDraftAgendaLoaded)
	{
		DraftAgenda = Agenda.Priorities;
		bDraftAgendaLoaded = true;
	}

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("AGENDA NACIONAL")), 6.f);

	// Agenda vigente segun el backend.
	FString CurrentText;
	for (const EWLGovernmentPriority AgendaPriority : Agenda.Priorities)
	{
		CurrentText += (CurrentText.IsEmpty() ? TEXT("") : TEXT(" · ")) + PriorityToText(AgendaPriority);
	}
	AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Agenda vigente"),
		CurrentText.IsEmpty() ? TEXT("Sin prioridades definidas") : CurrentText,
		CurrentText.IsEmpty() ? GovMuted : GovGold, GovCard), 8.f);
	AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Meses activa"),
		FString::Printf(TEXT("%d"), Agenda.MonthsActive), GovText, GovCardAlt), 4.f);
	if (!Agenda.LastAgendaReport.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree, Agenda.LastAgendaReport, GovGoldDim), 6.f);
	}

	// Selector: cada prioridad es un toggle; el backend valida duplicados y limite al confirmar.
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("NUEVA AGENDA  (%d/3 seleccionadas)"), DraftAgenda.Num())), 18.f);
	// Rejilla 2x3 de tarjetas seleccionables: la agenda se ELIGE mirando un tablero, no leyendo una lista.
	const EWLGovernmentPriority AllPriorities[] = {
		EWLGovernmentPriority::Security, EWLGovernmentPriority::Growth, EWLGovernmentPriority::Austerity,
		EWLGovernmentPriority::Industrialization, EWLGovernmentPriority::Diplomacy, EWLGovernmentPriority::Control };
	{
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		Grid->SetSlotPadding(FMargin(4.f));
		int32 Cell = 0;
		for (const EWLGovernmentPriority AvailablePriority : AllPriorities)
		{
			const bool bSelected = DraftAgenda.Contains(AvailablePriority);
			// Elegida = borde dorado y fondo elevado; se distingue de un vistazo.
			UBorder* Card = bSelected
				? MakeCard(WidgetTree, GovHeaderStrip, FMargin(12.f, 10.f), 8.f, GovGold, 1.8f)
				: MakeCard(WidgetTree, GovCard, FMargin(12.f, 10.f), 8.f);
			UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			// Icono de la prioridad: cada linea de gobierno tiene su simbolo.
			const EWLGovIcon PriorityIcon =
				AvailablePriority == EWLGovernmentPriority::Security          ? EWLGovIcon::Order :
				AvailablePriority == EWLGovernmentPriority::Growth            ? EWLGovIcon::Growth :
				AvailablePriority == EWLGovernmentPriority::Austerity         ? EWLGovIcon::Treasury :
				AvailablePriority == EWLGovernmentPriority::Industrialization ? EWLGovIcon::Provinces :
				AvailablePriority == EWLGovernmentPriority::Diplomacy         ? EWLGovIcon::Diplomacy :
				                                                                EWLGovIcon::Politics;
			UBorder* IconChip = MakeRoundedSurface(WidgetTree,
				FLinearColor(GovGold.R, GovGold.G, GovGold.B, bSelected ? 0.22f : 0.10f), FMargin(6.f), 8.f);
			IconChip->SetVerticalAlignment(VAlign_Center);
			IconChip->SetHorizontalAlignment(HAlign_Center);
			IconChip->SetContent(MakeIcon(WidgetTree, PriorityIcon, 20, bSelected ? GovGold : GovMuted));
			if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(IconChip))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
			if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
				PriorityToText(AvailablePriority), 14, bSelected ? GovGold : GovText)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (bSelected)
			{
				if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(
					MakeBadge(WidgetTree, TEXT("EN AGENDA"), GovGold, GovDarkInk)))
				{
					S->SetVerticalAlignment(VAlign_Center);
				}
			}
			Info->AddChildToVerticalBox(Head);
			if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(MakeText(WidgetTree,
				PriorityEffectText(AvailablePriority), 11, GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
			if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("agendatoggle:%d"), static_cast<int32>(AvailablePriority)),
				bSelected ? TEXT("QUITAR") : TEXT("ANADIR"),
				bSelected ? GovTabIdle : GovGoldDim, 0.f, 11)))
			{
				S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
				S->SetHorizontalAlignment(HAlign_Left);
			}
			Card->SetContent(Info);
			if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, Cell / 2, Cell % 2))
			{
				S->SetHorizontalAlignment(HAlign_Fill);
				S->SetVerticalAlignment(VAlign_Fill);
			}
			++Cell;
		}
		AddColumnChild(CenterBox, Grid, 6.f);
	}

	FWLPoliticalActionRequest AgendaPreviewRequest;
	AgendaPreviewRequest.NationIso = Iso;
	AgendaPreviewRequest.ActionType = EWLPoliticalActionType::SetAgenda;
	AgendaPreviewRequest.Priorities = DraftAgenda;
	const FWLPoliticalActionPreview AgendaPreview = Political->GetPoliticalActionPreview(AgendaPreviewRequest);
	AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(TEXT("AP %d · %s"),
		AgendaPreview.ActionPointCost,
		AgendaPreview.bCanExecute ? TEXT("Disponible") : *AgendaPreview.BlockReason),
		11, AgendaPreview.bCanExecute ? GovGold : GovBad, ETextJustify::Left, true), 10.f);
	AddColumnChild(CenterBox, MakeActionButton(WidgetTree, this, TEXT("agendaset"),
		TEXT("ESTABLECER AGENDA"), AgendaPreview.bCanExecute ? GovGoldDim : GovTabIdle, 190.f, 13), 4.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("La agenda guia eventos, presupuesto, opinion, ministerios y grupos sociales mes a mes. Cambiarla reinicia sus meses activos."),
		12, GovMuted, ETextJustify::Left, true), 4.f);
}

// PROGRAMAS: activos por cartera + catalogo filtrable con bloqueos legibles.
void UWLGovernmentWidget::BuildPoliticsProgramsSection()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	UWLCharacterSubsystem* Characters = GetCharacters();
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Political || !Characters || !Tick || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos de programas."), 14, GovMuted), 10.f);
		return;
	}

	const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
	const int64 Treasury = Tick->GetTreasury(Iso);
	const FWLStateCapacityState Capacity = Political->GetStateCapacity(Iso);
	const TArray<FWLMinistryProgramState> Active = Political->GetActiveMinistryPrograms(Iso);
	const TArray<FWLMinistryProgramDefinition> Catalog = Political->GetAvailableMinistryPrograms(Iso);

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("PROGRAMAS MINISTERIALES")), 6.f);
	// Recursos disponibles como insignias (lo que te limita, visible de un vistazo).
	{
		UHorizontalBox* Badges = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddResBadge = [&](const FString& Text, const FLinearColor& Bg, const FLinearColor& Fg)
		{
			if (UHorizontalBoxSlot* S = Badges->AddChildToHorizontalBox(MakeBadge(WidgetTree, Text, Bg, Fg)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
		};
		AddResBadge(FString::Printf(TEXT("CAPITAL %d"), Stats.PoliticalCapital), GovGoldDim, GovDarkInk);
		AddResBadge(FString::Printf(TEXT("TESORO %s"), *GovGroupThousands(Treasury)), GovHeaderStrip, GovGold);
		AddResBadge(FString::Printf(TEXT("RIESGO DE FALLO %d%%"), Capacity.PolicyFailureRisk),
			Capacity.PolicyFailureRisk >= 40 ? GovBad : GovTabIdle,
			Capacity.PolicyFailureRisk >= 40 ? GovDarkInk : GovMuted);
		AddColumnChild(CenterBox, Badges, 4.f);
	}

	// Filtro por cartera.
	UWrapBox* Filters = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	auto AddFilterChip = [&](int32 Value, const FString& Label)
	{
		const bool bActive = ProgramOfficeFilter == Value;
		if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Filters->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("progoffice:%d"), Value), Label,
			bActive ? GovGold : GovTabIdle, 96.f, 11, true,
			bActive ? GovDarkInk : GovMuted))))
		{
			S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
		}
	};
	AddFilterChip(0, TEXT("TODAS"));
	for (const EWLMinisterOffice Office : AllOffices)
	{
		AddFilterChip(static_cast<int32>(Office), UWLCharacterSubsystem::MinisterOfficeToString(Office));
	}
	AddColumnChild(CenterBox, Filters, 10.f);

	auto PassesFilter = [&](EWLMinisterOffice Office)
	{
		return ProgramOfficeFilter == 0 || ProgramOfficeFilter == static_cast<int32>(Office);
	};

	// Programa activo por cartera (para catalogar bloqueos de "cartera ocupada").
	TMap<EWLMinisterOffice, FWLMinistryProgramState> ActiveByOffice;
	for (const FWLMinistryProgramState& Program : Active)
	{
		ActiveByOffice.Add(Program.Office, Program);
	}

	// --- Programas en curso ---
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("PROGRAMAS EN CURSO")), 12.f);
	int32 ShownActive = 0;
	for (const FWLMinistryProgramState& Program : Active)
	{
		if (!PassesFilter(Program.Office))
		{
			continue;
		}
		UBorder* Card = MakeCard(WidgetTree, (ShownActive % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UVerticalBox* PVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s — %s"), *UWLCharacterSubsystem::MinisterOfficeToString(Program.Office), *Program.Name),
			14, GovText, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			Program.bBlocked ? TEXT("BLOQUEADO") : FString::Printf(TEXT("%d meses restantes"), Program.RemainingMonths),
			12, Program.bBlocked ? GovBad : GovGold, ETextJustify::Right));
		PVB->AddChildToVerticalBox(Head);
		if (UVerticalBoxSlot* S = PVB->AddChildToVerticalBox(MakeBar(WidgetTree, Program.Progress / 100.f,
			Program.bBlocked ? GovBad : GovGood, 8.f)))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		if (!Program.LastReport.IsEmpty())
		{
			if (UVerticalBoxSlot* S = PVB->AddChildToVerticalBox(MakeText(WidgetTree, Program.LastReport, 11, GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
		}
		Card->SetContent(PVB);
		AddColumnChild(CenterBox, Card, 4.f);
		++ShownActive;
	}
	if (ShownActive == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Ningun programa en curso para este filtro."), 13, GovMuted), 6.f);
	}

	// --- Catalogo ---
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("CATALOGO DE PROGRAMAS  (%d)"), Catalog.Num())), 16.f);
	int32 ShownCatalog = 0;
	for (const FWLMinistryProgramDefinition& Definition : Catalog)
	{
		if (!PassesFilter(Definition.Office))
		{
			continue;
		}
		UBorder* Card = MakeCard(WidgetTree, (ShownCatalog % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		// El programa lo ejecuta UNA persona: el ministro de la cartera, con su retrato.
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		{
			FWLCharacter Minister;
			const bool bHasMinister = Characters->GetCabinetMinister(Iso, Definition.Office, Minister) && Minister.IsValid();
			if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(MakePortrait(WidgetTree,
				bHasMinister ? Minister.Id : FString::Printf(TEXT("%s-MIN-VAC-%d"), *Iso, static_cast<int32>(Definition.Office)),
				GovGoldDim, 48.f, 58.f)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 11.f, 0.f));
			}
		}
		UVerticalBox* DVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree, Definition.Name, 14, GovText, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Head->AddChildToHorizontalBox(MakeBadge(WidgetTree,
			UWLCharacterSubsystem::MinisterOfficeToString(Definition.Office).ToUpper(), GovHeaderStrip, GovGold));
		DVB->AddChildToVerticalBox(Head);
		if (!Definition.Description.IsEmpty())
		{
			if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree, Definition.Description, 11, GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
		}
		FWLPoliticalActionRequest PreviewRequest;
		PreviewRequest.NationIso = Iso;
		PreviewRequest.ActionType = EWLPoliticalActionType::StartProgram;
		PreviewRequest.PrimaryId = Definition.ProgramId;
		const FWLPoliticalActionPreview Preview = Political->GetPoliticalActionPreview(PreviewRequest);
		if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Duracion %d meses · AP %d · Capital %d · Tesoro %s%s"),
			Definition.DurationMonths,
			Preview.ActionPointCost,
			Preview.PoliticalCapitalCost,
			*GovGroupThousands(Preview.TreasuryCost),
			Definition.bRequiresLegislation ? TEXT(" · Requiere ley del Congreso") : TEXT("")),
			11, Preview.bCanExecute ? GovGoldDim : GovBad, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}
		if (!Preview.EffectsPreview.IsEmpty())
		{
			if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree, Preview.EffectsPreview, 11, GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
		}

		// Bloqueos legibles desde el mismo preview que valida el backend.
		TArray<FString> Blockers;
		if (!Preview.bCanExecute)
		{
			Blockers.Add(Preview.BlockReason);
		}
		if (Blockers.Num() > 0)
		{
			for (const FString& Blocker : Blockers)
			{
				if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree,
					FString::Printf(TEXT("Bloqueado: %s"), *Blocker), 11, GovBad, ETextJustify::Left, true)))
				{
					S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
				}
			}
		}
		else
		{
			UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = Actions->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("program:%s"), *Definition.ProgramId), TEXT("INICIAR PROGRAMA"), GovGoldDim, 150.f, 11)))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(Actions))
			{
				S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			}
		}
		if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(DVB))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Card->SetContent(CardRow);
		AddColumnChild(CenterBox, Card, 4.f);
		++ShownCatalog;
	}
	if (ShownCatalog == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin programas disponibles para este filtro."), 13, GovMuted), 6.f);
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Un programa es ejecucion del ministerio (meses, presupuesto, ministro). Una reforma es ley con efecto largo: pestana LEYES."),
		12, GovMuted, ETextJustify::Left, true), 8.f);
}

// LEYES: arbol de reformas P2 por area con requisitos, efectos y bloqueos legibles.
void UWLGovernmentWidget::BuildPoliticsLawsSection()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	UWLCharacterSubsystem* Characters = GetCharacters();
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Political || !Characters || !Tick || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos de reformas."), 14, GovMuted), 10.f);
		return;
	}

	const TArray<FWLPolicyReformDefinition> Definitions = Political->GetAvailablePolicyReforms(Iso);
	const TArray<FWLActiveReformState> ActiveReforms = Political->GetActivePolicyReforms(Iso);
	const FWLInstitutionalPowerState Institutions = Political->GetInstitutionalPower(Iso);
	const FWLStateCapacityState Capacity = Political->GetStateCapacity(Iso);
	const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
	const FWLElectionState Election = Political->GetElectionState(Iso);
	const int64 Treasury = Tick->GetTreasury(Iso);

	// Reformas ya aprobadas o en implementacion (activas + memoria politica "reform_<id>").
	TSet<FString> EnactedIds;
	for (const FWLActiveReformState& Reform : ActiveReforms)
	{
		EnactedIds.Add(Reform.ReformId);
	}
	for (const FWLPoliticalMemoryRecord& Memory : Political->GetPoliticalMemory(Iso))
	{
		if (Memory.Value > 0 && Memory.MemoryKey.StartsWith(TEXT("reform_")))
		{
			EnactedIds.Add(Memory.MemoryKey.RightChop(7));
		}
	}
	TMap<FString, FString> NameById;
	for (const FWLPolicyReformDefinition& Definition : Definitions)
	{
		NameById.Add(Definition.ReformId, Definition.Name);
	}

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("ARBOL DE LEYES Y REFORMAS")), 6.f);
	// Lo que exigen las reformas, como insignias: coalicion, capacidad, capital y tesoro.
	{
		UHorizontalBox* Badges = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddResBadge = [&](const FString& Text, const FLinearColor& Bg, const FLinearColor& Fg)
		{
			if (UHorizontalBoxSlot* S = Badges->AddChildToHorizontalBox(MakeBadge(WidgetTree, Text, Bg, Fg)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
		};
		AddResBadge(FString::Printf(TEXT("COALICION %d"), Institutions.RulingCoalitionSupport),
			Institutions.RulingCoalitionSupport < 40 ? GovBad : GovGoldDim,
			GovDarkInk);
		AddResBadge(FString::Printf(TEXT("CAPACIDAD %d"), Capacity.AdministrativeEfficiency), GovHeaderStrip, GovGold);
		AddResBadge(FString::Printf(TEXT("CAPITAL %d"), Stats.PoliticalCapital), GovGoldDim, GovDarkInk);
		AddResBadge(FString::Printf(TEXT("TESORO %s"), *GovGroupThousands(Treasury)), GovHeaderStrip, GovGold);
		AddColumnChild(CenterBox, Badges, 4.f);
	}

	// Filtro por area.
	UWrapBox* Filters = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	auto AddAreaChip = [&](int32 Value, const FString& Label)
	{
		const bool bActive = ReformAreaFilter == Value;
		if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Filters->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("reformarea:%d"), Value), Label,
			bActive ? GovGold : GovTabIdle, 0.f, 10, true,
			bActive ? GovDarkInk : GovMuted))))
		{
			S->SetPadding(FMargin(0.f, 0.f, 4.f, 4.f));
		}
	};
	AddAreaChip(-1, TEXT("TODAS"));
	const EWLPolicyReformArea AllAreas[] = {
		EWLPolicyReformArea::Tax, EWLPolicyReformArea::Labor, EWLPolicyReformArea::Security,
		EWLPolicyReformArea::Education, EWLPolicyReformArea::Health, EWLPolicyReformArea::Decentralization,
		EWLPolicyReformArea::Military, EWLPolicyReformArea::Justice, EWLPolicyReformArea::Media,
		EWLPolicyReformArea::Energy, EWLPolicyReformArea::Trade, EWLPolicyReformArea::Constitution };
	for (const EWLPolicyReformArea Area : AllAreas)
	{
		AddAreaChip(static_cast<int32>(Area), ReformAreaToText(Area));
	}
	AddColumnChild(CenterBox, Filters, 10.f);

	auto PassesArea = [&](EWLPolicyReformArea Area)
	{
		return ReformAreaFilter < 0 || ReformAreaFilter == static_cast<int32>(Area);
	};

	// --- Reformas en implementacion (timeline) ---
	int32 ShownActive = 0;
	for (const FWLActiveReformState& Reform : ActiveReforms)
	{
		if (!PassesArea(Reform.Area))
		{
			continue;
		}
		if (ShownActive == 0)
		{
			AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("REFORMAS EN IMPLEMENTACION")), 12.f);
		}
		UBorder* Card = MakeCard(WidgetTree, (ShownActive % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UVerticalBox* RVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree, Reform.Name, 14, GovText, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (Election.CampaignPromiseReformId == Reform.ReformId)
		{
			Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, TEXT("PROMESA"), GovGoldDim, GovDarkInk));
		}
		Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, ReformAreaToText(Reform.Area).ToUpper(), GovHeaderStrip, GovGold));
		RVB->AddChildToVerticalBox(Head);
		if (UVerticalBoxSlot* S = RVB->AddChildToVerticalBox(MakeBar(WidgetTree, Reform.ImplementationProgress / 100.f, GovGood, 8.f)))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = RVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Progreso %d%% · %d meses restantes · Backlash %d"),
			Reform.ImplementationProgress, Reform.MonthsRemaining, Reform.Backlash),
			11, Reform.Backlash >= 40 ? GovBad : GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (!Reform.LastReport.IsEmpty())
		{
			RVB->AddChildToVerticalBox(MakeText(WidgetTree, Reform.LastReport, 11, GovMuted, ETextJustify::Left, true));
		}
		Card->SetContent(RVB);
		AddColumnChild(CenterBox, Card, 4.f);
		++ShownActive;
	}

	// --- Reformas disponibles ---
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("REFORMAS DISPONIBLES")), 16.f);
	int32 Shown = 0;
	for (const FWLPolicyReformDefinition& Definition : Definitions)
	{
		if (!PassesArea(Definition.Area))
		{
			continue;
		}
		const bool bEnacted = EnactedIds.Contains(Definition.ReformId);

		UBorder* Card = MakeCard(WidgetTree, (Shown % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UVerticalBox* DVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree, Definition.Name, 14,
			bEnacted ? GovMuted : GovText, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (bEnacted)
		{
			Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, TEXT("APROBADA"), GovGood, GovDarkInk));
		}
		if (Election.CampaignPromiseReformId == Definition.ReformId)
		{
			Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, TEXT("PROMESA"), GovGoldDim, GovDarkInk));
		}
		Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, ReformAreaToText(Definition.Area).ToUpper(), GovHeaderStrip, GovGold));
		DVB->AddChildToVerticalBox(Head);
		if (!Definition.Description.IsEmpty())
		{
			if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree, Definition.Description, 11, GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
		}
		FWLPoliticalActionRequest PreviewRequest;
		PreviewRequest.NationIso = Iso;
		PreviewRequest.ActionType = EWLPoliticalActionType::EnactReform;
		PreviewRequest.PrimaryId = Definition.ReformId;
		const FWLPoliticalActionPreview Preview = Political->GetPoliticalActionPreview(PreviewRequest);

		// Coste, requisitos y riesgo.
		if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("AP %d · Capital %d · Tesoro %s · Coalicion %d · Capacidad %d · Riesgo de protesta %d%%"),
			Preview.ActionPointCost,
			Preview.PoliticalCapitalCost,
			*GovGroupThousands(Preview.TreasuryCost),
			Definition.RequiredCoalitionSupport, Definition.RequiredStateCapacity, Definition.ProtestRisk),
			11, (!Preview.bCanExecute || Definition.ProtestRisk >= 40) ? GovBad : GovGoldDim, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}
		if (!Preview.EffectsPreview.IsEmpty())
		{
			if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree, Preview.EffectsPreview, 11, GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
		}

		// Efectos previstos: inmediatos + de largo plazo + grupos afectados.
		{
			FString Effects;
			auto AddEffect = [&Effects](const TCHAR* Label, int32 Value)
			{
				if (Value != 0)
				{
					Effects += FString::Printf(TEXT("%s%s %+d"), Effects.IsEmpty() ? TEXT("") : TEXT(" · "), Label, Value);
				}
			};
			AddEffect(TEXT("Orden"), Definition.PublicOrderDelta);
			AddEffect(TEXT("Oposicion"), Definition.OppositionDelta);
			AddEffect(TEXT("Legitimidad"), Definition.LegitimacyDelta);
			AddEffect(TEXT("Capacidad"), Definition.CapacityDelta);
			AddEffect(TEXT("Corrupcion"), Definition.CorruptionDelta);
			if (Definition.MonthlyTreasuryDelta != 0)
			{
				Effects += FString::Printf(TEXT("%sIngreso %+lld/mes x%d meses"),
					Effects.IsEmpty() ? TEXT("") : TEXT(" · "),
					static_cast<long long>(Definition.MonthlyTreasuryDelta), Definition.LongTermMonths);
			}
			FString Groups;
			for (const EWLPublicGroup Group : Definition.AffectedGroups)
			{
				Groups += (Groups.IsEmpty() ? TEXT("") : TEXT(", ")) + PublicGroupToText(Group);
			}
			if (!Groups.IsEmpty())
			{
				Effects += FString::Printf(TEXT("%sGrupos: %s (%+d)"),
					Effects.IsEmpty() ? TEXT("") : TEXT(" · "), *Groups, Definition.PublicGroupSupportDelta);
			}
			if (!Effects.IsEmpty())
			{
				if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree,
					FString::Printf(TEXT("Efectos: %s"), *Effects), 11, GovMuted, ETextJustify::Left, true)))
				{
					S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
				}
			}
		}

		// Bloqueos legibles (mismos datos que valida el backend; este re-valida al ejecutar).
		if (!bEnacted)
		{
			TArray<FString> Blockers;
			if (!Preview.bCanExecute)
			{
				Blockers.Add(Preview.BlockReason);
			}
			if (Blockers.Num() > 0)
			{
				for (const FString& Blocker : Blockers)
				{
					if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(MakeText(WidgetTree,
						FString::Printf(TEXT("Bloqueado: %s"), *Blocker), 11, GovBad, ETextJustify::Left, true)))
					{
						S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
					}
				}
			}
			else
			{
				UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				if (UHorizontalBoxSlot* S = Actions->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
					FString::Printf(TEXT("reform:%s"), *Definition.ReformId), TEXT("APROBAR REFORMA"), GovGoldDim, 150.f, 11)))
				{
					S->SetVerticalAlignment(VAlign_Center);
				}
				if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(Actions))
				{
					S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
				}
			}
		}
		Card->SetContent(DVB);
		AddColumnChild(CenterBox, Card, 4.f);
		++Shown;
	}
	if (Shown == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin reformas en esta area."), 13, GovMuted), 6.f);
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Aprobar pasa la votacion en el Congreso (coste segun gridlock), cobra tesoro/capital y puede detonar protesta. El efecto largo corre por meses."),
		12, GovMuted, ETextJustify::Left, true), 8.f);
}

// CONGRESO: instituciones, bancadas por rol y red clientelar (patronazgo).
void UWLGovernmentWidget::BuildPoliticsCongressSection()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos del Congreso."), 14, GovMuted), 10.f);
		return;
	}

	const FWLInstitutionalPowerState Institutions = Political->GetInstitutionalPower(Iso);
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("CONGRESO Y BASE POLITICA")), 6.f);
	// Heroe: la coalicion es EL numero del Congreso (las reformas viven o mueren por el).
	{
		UUniformGridPanel* HeroGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		HeroGrid->SetSlotPadding(FMargin(5.f, 0.f));
		if (UUniformGridSlot* S = HeroGrid->AddChildToUniformGrid(MakeHeroGauge(WidgetTree,
			TEXT("Coalicion oficialista"), Institutions.RulingCoalitionSupport,
			SupportColor(Institutions.RulingCoalitionSupport),
			TEXT("Apoyo legislativo del gobierno. Las reformas exigen un minimo de coalicion.")), 0, 0))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
		if (UUniformGridSlot* S = HeroGrid->AddChildToUniformGrid(MakeHeroGauge(WidgetTree,
			TEXT("Riesgo de gridlock"), Institutions.GridlockRisk, RiskColor(Institutions.GridlockRisk),
			FString::Printf(TEXT("Oposicion legislativa %d · Coste de reforma: %d de capital"),
				Institutions.LegislativeOpposition, Institutions.ReformCost)), 0, 1))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
		AddColumnChild(CenterBox, HeroGrid, 8.f);
	}
	if (!Institutions.LastVoteReport.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree,
			FString::Printf(TEXT("Ultima votacion: %s"), *Institutions.LastVoteReport), GovGoldDim), 6.f);
	}

	// Bancadas agrupadas por rol parlamentario.
	const TArray<FWLPartyState> Parties = Political->GetPoliticalParties(Iso);
	const EWLPartyRole RoleOrder[] = {
		EWLPartyRole::Ruling, EWLPartyRole::Ally, EWLPartyRole::SoftOpposition, EWLPartyRole::HardOpposition };
	for (const EWLPartyRole Role : RoleOrder)
	{
		bool bHeader = false;
		int32 Index = 0;
		for (const FWLPartyState& Party : Parties)
		{
			if (Party.Role != Role)
			{
				continue;
			}
			if (!bHeader)
			{
				// Franja con el color del rol parlamentario + linea fina del mismo color.
				UVerticalBox* HeaderVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
				UBorder* Strip = MakeRoundedSurface(WidgetTree, GovHeaderStrip, FMargin(11.f, 7.f), 5.f);
				Strip->SetContent(MakeText(WidgetTree, PartyRoleToText(Role).ToUpper(), 14, PartyRoleColor(Role)));
				HeaderVB->AddChildToVerticalBox(Strip);
				USizeBox* Line = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				Line->SetHeightOverride(2.f);
				Line->SetContent(MakeRoundedSurface(WidgetTree, PartyRoleColor(Role), FMargin(0.f), 1.f));
				HeaderVB->AddChildToVerticalBox(Line);
				AddColumnChild(CenterBox, HeaderVB, 16.f);
				bHeader = true;
			}
			UBorder* Card = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
			// Cara del jefe de bancada (pool de retratos por rol): el Congreso son PERSONAS, no filas.
			UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			const FString PartySeed = FString::Printf(TEXT("%s-%s-%s"), *Iso,
				Role == EWLPartyRole::Ruling ? TEXT("LEADER") : TEXT("OPP"), *Party.PartyId);
			if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(MakePortrait(WidgetTree,
				PartySeed, PartyRoleColor(Role), 52.f, 64.f)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 11.f, 0.f));
			}
			UVerticalBox* PVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree, Party.Name, 14, GovText, ETextJustify::Left, true)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeBadge(WidgetTree,
				FString::Printf(TEXT("%d ESCANOS"), Party.Seats), GovGoldDim, GovDarkInk)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
			}
			if (Party.bInCoalition)
			{
				if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeBadge(WidgetTree,
					TEXT("COALICION"), GovGood, GovDarkInk)))
				{
					S->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
				}
			}
			Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, IdeologyToText(Party.Ideology).ToUpper(), GovHeaderStrip, GovGold));
			PVB->AddChildToVerticalBox(Head);
			if (UVerticalBoxSlot* S = PVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
				TEXT("Disciplina %d · Lealtad al gobierno %d · Corrupcion %d"),
				Party.Discipline, Party.LoyaltyToGovernment, Party.Corruption),
				11, Party.LoyaltyToGovernment < 35 ? GovBad : GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
			if (Party.bInCoalition && Party.LoyaltyToGovernment < 35)
			{
				PVB->AddChildToVerticalBox(MakeText(WidgetTree,
					TEXT("Alerta: lealtad critica — riesgo de ruptura de coalicion o traicion en votaciones."),
					11, GovBad, ETextJustify::Left, true));
			}
			if (!Party.LastIncident.IsEmpty())
			{
				PVB->AddChildToVerticalBox(MakeText(WidgetTree, Party.LastIncident, 11, GovMuted, ETextJustify::Left, true));
			}
			FWLPoliticalActionRequest NegotiatePreviewRequest;
			NegotiatePreviewRequest.NationIso = Iso;
			NegotiatePreviewRequest.ActionType = EWLPoliticalActionType::NegotiatePartySupport;
			NegotiatePreviewRequest.PrimaryId = Party.PartyId;
			const FWLPoliticalActionPreview NegotiatePreview = Political->GetPoliticalActionPreview(NegotiatePreviewRequest);
			FWLPoliticalActionRequest ElectionPreviewRequest;
			ElectionPreviewRequest.NationIso = Iso;
			ElectionPreviewRequest.ActionType = EWLPoliticalActionType::HoldPartyInternalElection;
			ElectionPreviewRequest.PrimaryId = Party.PartyId;
			const FWLPoliticalActionPreview ElectionPreview = Political->GetPoliticalActionPreview(ElectionPreviewRequest);
			const FString PartyActionPreview = FString::Printf(TEXT("Negociar: AP %d capital %d (%s) · Interna: AP %d capital %d (%s)"),
				NegotiatePreview.ActionPointCost,
				NegotiatePreview.PoliticalCapitalCost,
				NegotiatePreview.bCanExecute ? TEXT("ok") : *NegotiatePreview.BlockReason,
				ElectionPreview.ActionPointCost,
				ElectionPreview.PoliticalCapitalCost,
				ElectionPreview.bCanExecute ? TEXT("ok") : *ElectionPreview.BlockReason);
			if (UVerticalBoxSlot* S = PVB->AddChildToVerticalBox(MakeText(WidgetTree, PartyActionPreview, 10,
				(NegotiatePreview.bCanExecute || ElectionPreview.bCanExecute) ? GovGold : GovBad, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
			UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = Actions->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("negotiate:%s"), *Party.PartyId), TEXT("NEGOCIAR APOYO"),
				NegotiatePreview.bCanExecute ? GovGoldDim : GovTabIdle, 140.f, 11)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UHorizontalBoxSlot* S = Actions->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("partyelect:%s"), *Party.PartyId), TEXT("ELECCION INTERNA"),
				ElectionPreview.bCanExecute ? GovTabIdle : GovCardAlt, 140.f, 11)))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UVerticalBoxSlot* S = PVB->AddChildToVerticalBox(Actions))
			{
				S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			}
			if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(PVB))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			Card->SetContent(CardRow);
			AddColumnChild(CenterBox, Card, 4.f);
			++Index;
		}
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Negociar apoyo cuesta capital politico y alimenta el clientelismo. La eleccion interna renueva disciplina/lealtad de la bancada."),
		12, GovMuted, ETextJustify::Left, true), 6.f);

	// Gobierno P2: red clientelar (patronazgo).
	const FWLPatronageState Patronage = Political->GetPatronageState(Iso);
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("RED CLIENTELAR (PATRONAZGO)")), 20.f);
	AddColumnChild(CenterBox, MakeGaugePanel(WidgetTree, {
		{ TEXT("Poder de patronazgo"), Patronage.PatronagePower, SupportColor(Patronage.PatronagePower, 50, 20),
			TEXT("Cuantos favores puedes repartir sin romper el Estado.") },
		{ TEXT("Presion clientelista"), Patronage.ClientelistPressure, RiskColor(Patronage.ClientelistPressure) },
		{ TEXT("Corrupcion de contratos"), Patronage.ContractCorruption, RiskColor(Patronage.ContractCorruption) },
		{ TEXT("Maquinaria regional"), Patronage.RegionalMachines, GovGoldDim },
		{ TEXT("Backlash por concesiones"), Patronage.ConcessionBacklash, RiskColor(Patronage.ConcessionBacklash) } }), 8.f);
	if (!Patronage.LastDeal.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree,
			FString::Printf(TEXT("Ultimo trato: %s"), *Patronage.LastDeal), GovGoldDim), 6.f);
	}
	if (Patronage.ContractCorruption >= 60 || Patronage.ConcessionBacklash >= 60)
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree,
			TEXT("La red clientelar esta al limite: mas favores pueden detonar un escandalo de corrupcion."), GovBad), 4.f);
	}
	int32 PatronageIndex = 0;
	for (const FPatronageActionUI& Def : PatronageActions)
	{
		FWLPoliticalActionRequest PreviewRequest;
		PreviewRequest.NationIso = Iso;
		PreviewRequest.ActionType = EWLPoliticalActionType::UsePatronage;
		PreviewRequest.NumericValue = static_cast<int32>(Def.Action);
		const FWLPoliticalActionPreview Preview = Political->GetPoliticalActionPreview(PreviewRequest);
		UBorder* Row = MakeCard(WidgetTree, (PatronageIndex % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, Def.Preview, 11, GovMuted, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		const FString PreviewLine = FString::Printf(TEXT("AP %d · Capital %d · Tesoro %s · %s"),
			Preview.ActionPointCost,
			Preview.PoliticalCapitalCost,
			*GovGroupThousands(Preview.TreasuryCost),
			Preview.bCanExecute ? TEXT("Disponible") : *Preview.BlockReason);
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, PreviewLine, 10,
			Preview.bCanExecute ? GovGold : GovBad, ETextJustify::Left, true)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("patronage:%d"), static_cast<int32>(Def.Action)), Def.Label,
			Preview.bCanExecute ? GovGoldDim : GovTabIdle, 170.f, 11)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, 4.f);
		++PatronageIndex;
	}
}

// ELECCIONES: fase, encuestas, legitimidad y promesas de campania ligadas a reformas reales.
void UWLGovernmentWidget::BuildPoliticsElectionsSection()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos electorales."), 14, GovMuted), 10.f);
		return;
	}

	const FWLElectionState Election = Political->GetElectionState(Iso);
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("ELECCIONES Y LEGITIMIDAD")), 6.f);

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(5.f));
	auto Place = [&](int32 Row, int32 Col, UBorder* Card)
	{
		if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, Row, Col))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	Place(0, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Politics,
		Election.MonthsToElection <= 6 ? GovGold : FLinearColor(0.55f, 0.68f, 0.95f),
		TEXT("Meses a la eleccion"), FString::Printf(TEXT("%d"), Election.MonthsToElection),
		Election.MonthsToElection <= 6 ? GovGold : GovText));
	Place(0, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Crisis,
		Election.Phase == EWLElectionPhase::Crisis ? GovBad : GovGoldDim,
		TEXT("Fase"), ElectionPhaseToText(Election.Phase),
		Election.Phase == EWLElectionPhase::Crisis ? GovBad : GovText));
	Place(1, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Approval, SupportColor(Election.IncumbentApproval),
		TEXT("Aprobacion presidencial"),
		FString::Printf(TEXT("%d%%"), Election.IncumbentApproval), SupportColor(Election.IncumbentApproval)));
	Place(1, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Order, SupportColor(Election.Legitimacy),
		TEXT("Legitimidad"),
		FString::Printf(TEXT("%d"), Election.Legitimacy), SupportColor(Election.Legitimacy)));
	AddColumnChild(CenterBox, Grid, 10.f);

	// Encuesta como DUELO: una sola barra gobierno (oro) contra oposicion (rojo), estilo noche electoral.
	{
		UBorder* Duel = MakeCard(WidgetTree, GovCard, FMargin(14.f, 11.f));
		UVerticalBox* DVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Labels = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Labels->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("GOBIERNO  %d%%"), Election.PollingGovernment), 13, GovGold)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		Labels->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%d%%  OPOSICION"), Election.PollingOpposition), 13, GovBad, ETextJustify::Right));
		DVB->AddChildToVerticalBox(Labels);
		// Barra partida: cada mitad pesa lo que su encuesta (el hueco central es indecisos).
		UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const float GovFrac = FMath::Max(Election.PollingGovernment / 100.f, 0.02f);
		const float OppFrac = FMath::Max(Election.PollingOpposition / 100.f, 0.02f);
		const float RestFrac = FMath::Max(1.f - GovFrac - OppFrac, 0.001f);
		UBorder* GovSeg = MakeRoundedSurface(WidgetTree, GovGold, FMargin(0.f), 5.f);
		UBorder* Gap = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(0.f));
		UBorder* OppSeg = MakeRoundedSurface(WidgetTree, GovBad, FMargin(0.f), 5.f);
		auto AddSeg = [&](UWidget* W, float Weight)
		{
			if (UHorizontalBoxSlot* S = Split->AddChildToHorizontalBox(W))
			{
				FSlateChildSize Size(ESlateSizeRule::Fill);
				Size.Value = Weight;
				S->SetSize(Size);
			}
		};
		AddSeg(GovSeg, GovFrac);
		AddSeg(Gap, RestFrac);
		AddSeg(OppSeg, OppFrac);
		USizeBox* SplitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SplitBox->SetHeightOverride(14.f);
		SplitBox->SetContent(Split);
		UBorder* SplitTrack = MakeRoundedSurface(WidgetTree, GovBarTrack, FMargin(2.f), 7.f);
		SplitTrack->SetContent(SplitBox);
		if (UVerticalBoxSlot* S = DVB->AddChildToVerticalBox(SplitTrack))
		{
			S->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
		}
		DVB->AddChildToVerticalBox(MakeText(WidgetTree,
			TEXT("El hueco central son los indecisos: se los lleva quien gobierna mejor los ultimos meses."),
			10, GovMuted, ETextJustify::Left, true));
		// Duelo con CARAS: presidente vs rival de la oposicion, como una portada de noche electoral.
		UHorizontalBox* DuelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = DuelRow->AddChildToHorizontalBox(MakePortrait(WidgetTree,
			FString::Printf(TEXT("%s-LEADER-INCUMBENT"), *Iso), GovGold, 58.f, 72.f)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = DuelRow->AddChildToHorizontalBox(DVB))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = DuelRow->AddChildToHorizontalBox(MakePortrait(WidgetTree,
			FString::Printf(TEXT("%s-OPP-RIVAL"), *Iso), GovBad, 58.f, 72.f)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}
		Duel->SetContent(DuelRow);
		AddColumnChild(CenterBox, Duel, 10.f);
	}
	AddColumnChild(CenterBox, MakeGaugePanel(WidgetTree, {
		{ TEXT("Riesgo de abstencion"), Election.AbstentionRisk, RiskColor(Election.AbstentionRisk, 35, 55) },
		{ TEXT("Riesgo de fraude percibido"), Election.FraudRisk, RiskColor(Election.FraudRisk, 25, 50),
			TEXT("Sube con censura, patronazgo y control mediatico. Erosiona la legitimidad del resultado.") } }), 6.f);

	if (Election.FraudRisk >= 30)
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree,
			TEXT("El fraude percibido esta erosionando la legitimidad: la victoria puede no ser reconocida."), GovBad), 6.f);
	}
	if (Election.bTermLimited)
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree,
			TEXT("Limite de mandato: no puedes presentarte a la reeleccion sin reforma constitucional."), GovGold), 4.f);
	}

	// Promesa de campania ligada a una reforma real del arbol de leyes.
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("PROMESA DE CAMPANIA")), 16.f);
	const TArray<FWLPolicyReformDefinition> Definitions = Political->GetAvailablePolicyReforms(Iso);
	const TArray<FWLActiveReformState> ActiveReforms = Political->GetActivePolicyReforms(Iso);
	TSet<FString> EnactedIds;
	for (const FWLActiveReformState& Reform : ActiveReforms)
	{
		EnactedIds.Add(Reform.ReformId);
	}
	for (const FWLPoliticalMemoryRecord& Memory : Political->GetPoliticalMemory(Iso))
	{
		if (Memory.Value > 0 && Memory.MemoryKey.StartsWith(TEXT("reform_")))
		{
			EnactedIds.Add(Memory.MemoryKey.RightChop(7));
		}
	}
	if (!Election.CampaignPromiseReformId.IsEmpty())
	{
		FString PromiseName = Election.CampaignPromiseReformId;
		for (const FWLPolicyReformDefinition& Definition : Definitions)
		{
			if (Definition.ReformId == Election.CampaignPromiseReformId)
			{
				PromiseName = Definition.Name;
				break;
			}
		}
		const bool bKept = EnactedIds.Contains(Election.CampaignPromiseReformId);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree,
			FString::Printf(TEXT("Prometido: %s"), *PromiseName),
			bKept ? TEXT("CUMPLIDA / EN MARCHA") : TEXT("PENDIENTE"),
			bKept ? GovGood : GovGold, GovCard), 8.f);
		if (!bKept)
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree,
				TEXT("Incumplir la promesa el dia de la eleccion castiga encuestas y legitimidad. Apruebala en LEYES."),
				12, GovMuted, ETextJustify::Left, true), 4.f);
		}
	}
	else
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin promesa activa. Promete una reforma para ganar encuestas ahora... y pagar el precio si no cumples."),
			12, GovMuted, ETextJustify::Left, true), 6.f);
		int32 PromiseShown = 0;
		UWrapBox* Options = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		for (const FWLPolicyReformDefinition& Definition : Definitions)
		{
			if (EnactedIds.Contains(Definition.ReformId))
			{
				continue;
			}
			FWLPoliticalActionRequest PreviewRequest;
			PreviewRequest.NationIso = Iso;
			PreviewRequest.ActionType = EWLPoliticalActionType::MakeCampaignPromise;
			PreviewRequest.PrimaryId = Definition.ReformId;
			const FWLPoliticalActionPreview Preview = Political->GetPoliticalActionPreview(PreviewRequest);
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Options->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("promise:%s"), *Definition.ReformId),
				Preview.bCanExecute
					? FString::Printf(TEXT("PROMETER: %s · AP %d"), *Definition.Name, Preview.ActionPointCost)
					: FString::Printf(TEXT("BLOQUEADO: %s"), *Definition.Name),
				Preview.bCanExecute ? GovTabIdle : GovCardAlt, 0.f, 10))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
			}
			if (++PromiseShown >= 8)
			{
				break;
			}
		}
		if (PromiseShown > 0)
		{
			AddColumnChild(CenterBox, Options, 6.f);
		}
	}

	if (!Election.LastElectionReport.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("ULTIMA ELECCION")), 16.f);
		AddColumnChild(CenterBox, MakeAlert(WidgetTree, Election.LastElectionReport,
			Election.bLastElectionWon ? GovGood : GovBad), 6.f);
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("La eleccion se resuelve sola al llegar el mes: reeleccion, derrota con transicion o crisis constitucional segun legitimidad y fraude."),
		12, GovMuted, ETextJustify::Left, true), 8.f);
}

// MEDIOS: prensa, opinion publica, narrativa y acciones mediaticas con preview.
void UWLGovernmentWidget::BuildPoliticsMediaSection()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos de medios."), 14, GovMuted), 10.f);
		return;
	}

	const FWLMediaPublicOpinionState Media = Political->GetMediaPublicOpinion(Iso);
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("MEDIOS Y OPINION PUBLICA")), 6.f);
	// Heroe: la aprobacion es EL numero de esta pantalla — con la cara del presidente al lado.
	{
		UHorizontalBox* HeroRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HeroRow->AddChildToHorizontalBox(MakePortrait(WidgetTree,
			FString::Printf(TEXT("%s-LEADER-INCUMBENT"), *Iso),
			SupportColor(Media.PresidentialApproval), 74.f, 92.f)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = HeroRow->AddChildToHorizontalBox(MakeHeroGauge(WidgetTree,
			TEXT("Aprobacion presidencial"), Media.PresidentialApproval, SupportColor(Media.PresidentialApproval),
			Media.LastNarrative.IsEmpty() ? TEXT("Lo que el pais opina de ti este mes.")
			                              : FString::Printf(TEXT("Narrativa del momento: %s"), *Media.LastNarrative))))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		AddColumnChild(CenterBox, HeroRow, 8.f);
	}
	AddColumnChild(CenterBox, MakeGaugePanel(WidgetTree, {
		{ TEXT("Libertad de prensa"), Media.PressFreedom, SupportColor(Media.PressFreedom, 50, 25),
			TEXT("Prensa libre critica pero legitima. Censurarla da control y quita legitimidad.") },
		{ TEXT("Control mediatico"), Media.MediaControl, RiskColor(Media.MediaControl, 40, 70) },
		{ TEXT("Alcance propagandistico"), Media.PropagandaReach, GovGoldDim },
		{ TEXT("Backlash de censura"), Media.CensorshipBacklash, RiskColor(Media.CensorshipBacklash) },
		{ TEXT("Presion de fake news"), Media.FakeNewsPressure, RiskColor(Media.FakeNewsPressure) },
		{ TEXT("Riesgo de crisis mediatica"), Media.MediaCrisisRisk, RiskColor(Media.MediaCrisisRisk) } }), 6.f);

	if (Media.MediaCrisisRisk >= 50)
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree,
			TEXT("Riesgo alto de crisis mediatica: censura y fake news pueden escalar a protesta nacional."), GovBad), 6.f);
	}
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("ACCIONES DE MEDIOS")), 16.f);
	int32 Index = 0;
	for (const FMediaActionUI& Def : MediaActions)
	{
		FWLPoliticalActionRequest PreviewRequest;
		PreviewRequest.NationIso = Iso;
		PreviewRequest.ActionType = EWLPoliticalActionType::RunMediaAction;
		PreviewRequest.NumericValue = static_cast<int32>(Def.Action);
		const FWLPoliticalActionPreview Preview = Political->GetPoliticalActionPreview(PreviewRequest);
		UBorder* Row = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, Def.Preview, 11, GovMuted, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		const FString PreviewLine = FString::Printf(TEXT("AP %d · Capital %d · Tesoro %s · %s"),
			Preview.ActionPointCost,
			Preview.PoliticalCapitalCost,
			*GovGroupThousands(Preview.TreasuryCost),
			Preview.bCanExecute ? TEXT("Disponible") : *Preview.BlockReason);
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, PreviewLine, 10,
			Preview.bCanExecute ? GovGold : GovBad, ETextJustify::Left, true)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		const bool bHardline = Def.Action == EWLMediaActionType::Censorship || Def.Action == EWLMediaActionType::Propaganda;
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("media:%d"), static_cast<int32>(Def.Action)), Def.Label,
			!Preview.bCanExecute ? GovTabIdle : (bHardline ? GovDanger : GovGoldDim), 170.f, 11)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, 4.f);
		++Index;
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Censura y propaganda piden confirmacion: el preview muestra coste/AP y la memoria politica registra el backlash."),
		12, GovMuted, ETextJustify::Left, true), 6.f);
}

// REGIONES: gobernadores con obediencia/autonomia/protesta y politicas regionales.
void UWLGovernmentWidget::BuildPoliticsRegionsSection()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos regionales."), 14, GovMuted), 10.f);
		return;
	}

	const TArray<FWLRegionGovernorState> Regions = Political->GetRegionGovernors(Iso);
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("REGIONES Y GOBERNADORES  (%d)"), Regions.Num())), 6.f);
	if (Regions.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin regiones registradas para esta nacion."), 13, GovMuted), 8.f);
		return;
	}

	int32 Index = 0;
	for (const FWLRegionGovernorState& Region : Regions)
	{
		UBorder* Card = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 10.f));
		// Cara del gobernador (pool de retratos): cada region tiene un rostro que rendir cuentas.
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(MakePortrait(WidgetTree,
			FString::Printf(TEXT("%s-MIN-GOV-%s"), *Iso, *Region.RegionId),
			PartyRoleColor(Region.Alignment), 52.f, 64.f)))
		{
			S->SetVerticalAlignment(VAlign_Top);
			S->SetPadding(FMargin(0.f, 0.f, 11.f, 0.f));
		}
		UVerticalBox* RVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s — Gob. %s"), *Region.RegionName.ToUpper(), *Region.GovernorName),
			14, GovText, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, PartyRoleToText(Region.Alignment).ToUpper(),
			GovHeaderStrip, PartyRoleColor(Region.Alignment)));
		RVB->AddChildToVerticalBox(Head);

		{
			UHorizontalBox* Badges = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			auto AddRegionBadge = [&](const FString& Text, bool bDanger)
			{
				if (UHorizontalBoxSlot* S = Badges->AddChildToHorizontalBox(MakeBadge(WidgetTree, Text,
					bDanger ? GovBad : GovTabIdle, bDanger ? GovDarkInk : GovMuted)))
				{
					S->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
					S->SetVerticalAlignment(VAlign_Center);
				}
			};
			AddRegionBadge(FString::Printf(TEXT("AUTONOMIA %d"), Region.Autonomy), false);
			AddRegionBadge(FString::Printf(TEXT("PROTESTA %d"), Region.ProtestRisk), Region.ProtestRisk >= 50);
			AddRegionBadge(FString::Printf(TEXT("CONTROL %d"), Region.CenterControl), false);
			AddRegionBadge(FString::Printf(TEXT("INVERSION NV %d"), Region.InvestmentLevel), false);
			if (UVerticalBoxSlot* S = RVB->AddChildToVerticalBox(Badges))
			{
				S->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
			}
		}
		// Obediencia: la cifra que decide si la region te hace caso, con su barra.
		{
			UHorizontalBox* ObedienceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = ObedienceRow->AddChildToHorizontalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("Obediencia %d"), Region.Obedience), 11,
				SupportColor(Region.Obedience))))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
			// La barra vive en el slot fill de abajo.
			if (UVerticalBoxSlot* S = RVB->AddChildToVerticalBox(ObedienceRow))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
		}
		if (UVerticalBoxSlot* S = RVB->AddChildToVerticalBox(MakeBar(WidgetTree, Region.Obedience / 100.f,
			SupportColor(Region.Obedience), 7.f)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (Region.SecessionRisk >= 25 || Region.RebellionRisk >= 25)
		{
			if (UVerticalBoxSlot* S = RVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
				TEXT("ALERTA: secesion %d · rebelion %d"), Region.SecessionRisk, Region.RebellionRisk),
				11, GovBad, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
		}
		if (!Region.LastReport.IsEmpty())
		{
			RVB->AddChildToVerticalBox(MakeText(WidgetTree, Region.LastReport, 11, GovMuted, ETextJustify::Left, true));
		}

		UWrapBox* Actions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		for (const FRegionActionUI& Def : RegionActions)
		{
			FWLPoliticalActionRequest PreviewRequest;
			PreviewRequest.NationIso = Iso;
			PreviewRequest.ActionType = EWLPoliticalActionType::RunRegionPolicy;
			PreviewRequest.PrimaryId = Region.RegionId;
			PreviewRequest.NumericValue = static_cast<int32>(Def.Action);
			const FWLPoliticalActionPreview Preview = Political->GetPoliticalActionPreview(PreviewRequest);
			UWLGovActionButton* Button = MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("region:%d:%s"), static_cast<int32>(Def.Action), *Region.RegionId),
				Def.Label,
				!Preview.bCanExecute ? GovCardAlt : (Def.Action == EWLRegionPolicyActionType::SecurityOperation ? GovDanger : GovTabIdle),
				0.f,
				10);
			Button->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s\nAP %d · Capital %d · Tesoro %s\n%s"),
				Def.Preview,
				Preview.ActionPointCost,
				Preview.PoliticalCapitalCost,
				*GovGroupThousands(Preview.TreasuryCost),
				Preview.bCanExecute ? TEXT("Disponible") : *Preview.BlockReason)));
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Actions->AddChildToWrapBox(Button)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 5.f, 4.f));
			}
		}
		if (UVerticalBoxSlot* S = RVB->AddChildToVerticalBox(Actions))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(RVB))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		Card->SetContent(CardRow);
		AddColumnChild(CenterBox, Card, 5.f);
		++Index;
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Nombrar gobernador y la operacion de seguridad piden confirmacion. Invertir y pactar autonomia mueven obediencia, protesta y control central."),
		12, GovMuted, ETextJustify::Left, true), 6.f);
}

// CRISIS: cadenas activas con escalada + memoria politica que las alimenta.
void UWLGovernmentWidget::BuildPoliticsCrisisSection()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos de crisis."), 14, GovMuted), 10.f);
		return;
	}

	const TArray<FWLCrisisChainState> Chains = Political->GetActiveCrisisChains(Iso);
	int32 ActiveCount = 0;
	for (const FWLCrisisChainState& Chain : Chains)
	{
		if (!Chain.bResolved)
		{
			++ActiveCount;
		}
	}
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("CRISIS ENCADENADAS  (%d activas)"), ActiveCount)), 6.f);
	if (ActiveCount == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin crisis en curso. Las decisiones duras (represion, censura, deuda, patronazgo) las incuban con el tiempo."),
			13, GovMuted, ETextJustify::Left, true), 8.f);
	}
	int32 Index = 0;
	for (const FWLCrisisChainState& Chain : Chains)
	{
		if (Chain.bResolved)
		{
			continue;
		}
		UBorder* Card = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 10.f));
		UVerticalBox* CVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			CrisisTypeToText(Chain.Type).ToUpper(), 14, GovBad)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Etapa %d/5 · %d meses activa"), Chain.Stage, Chain.MonthsActive),
			12, GovGold, ETextJustify::Right));
		CVB->AddChildToVerticalBox(Head);

		// Escalera de escalada: 5 segmentos, encendidos hasta la etapa actual.
		UHorizontalBox* Ladder = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (int32 Stage = 1; Stage <= 5; ++Stage)
		{
			USizeBox* Segment = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Segment->SetHeightOverride(10.f);
			const FLinearColor SegmentColor = Stage <= Chain.Stage
				? (Stage >= 4 ? GovBad : GovGold)
				: GovBarTrack;
			Segment->SetContent(MakeBorder(WidgetTree, SegmentColor, FMargin(0.f)));
			if (UHorizontalBoxSlot* S = Ladder->AddChildToHorizontalBox(Segment))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetPadding(FMargin(0.f, 0.f, Stage < 5 ? 3.f : 0.f, 0.f));
			}
		}
		if (UVerticalBoxSlot* S = CVB->AddChildToVerticalBox(Ladder))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = CVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Intensidad %d/100 · Escalada tipica: protesta -> huelga -> represion -> escandalo -> golpe"),
			Chain.Intensity), 11, GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (!Chain.LastReport.IsEmpty())
		{
			CVB->AddChildToVerticalBox(MakeText(WidgetTree, Chain.LastReport, 11, GovText, ETextJustify::Left, true));
		}
		Card->SetContent(CVB);
		AddColumnChild(CenterBox, Card, 5.f);
		++Index;
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Las crisis avanzan cada mes y disparan eventos con opciones en PODER. Resolverlos bien las desescala; ignorarlos las empuja al golpe."),
		12, GovMuted, ETextJustify::Left, true), 6.f);

	// Memoria politica: el pais no olvida.
	const TArray<FWLPoliticalMemoryRecord> Memory = Political->GetPoliticalMemory(Iso);
	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("MEMORIA POLITICA  (%d recuerdos)"), Memory.Num())), 18.f);
	if (Memory.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin memoria acumulada. Cada represion, promesa o reforma queda registrada aqui y pesa durante meses."),
			13, GovMuted, ETextJustify::Left, true), 8.f);
		return;
	}
	TArray<FWLPoliticalMemoryRecord> Sorted = Memory;
	Sorted.Sort([](const FWLPoliticalMemoryRecord& A, const FWLPoliticalMemoryRecord& B)
	{
		return A.MonthsRemaining > B.MonthsRemaining;
	});
	int32 MemoryIndex = 0;
	for (const FWLPoliticalMemoryRecord& Record : Sorted)
	{
		if (MemoryIndex >= 15)
		{
			break;
		}
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree,
			Record.LastReason.IsEmpty() ? Record.MemoryKey : Record.LastReason,
			FString::Printf(TEXT("%+d · %d meses"), Record.Value, Record.MonthsRemaining),
			Record.Value < 0 ? GovBad : GovMuted,
			(MemoryIndex % 2 == 0) ? GovCard : GovCardAlt), 4.f);
		++MemoryIndex;
	}
}

// ---------------------------------------------------------------------------
// RESUMEN / ALTO MANDO / REGISTROS: paneles de Gobierno P1/P2 fuera de POLITICA
// ---------------------------------------------------------------------------

// RESUMEN: grid de pulso de gobierno debajo de las metricas economicas.
void UWLGovernmentWidget::BuildGovernanceOverviewCards()
{
	const FString Iso = PlayerIso();
	const UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		return;
	}
	const FWLMediaPublicOpinionState Media = Political->GetMediaPublicOpinion(Iso);
	const FWLElectionState Election = Political->GetElectionState(Iso);
	const FWLInstitutionalPowerState Institutions = Political->GetInstitutionalPower(Iso);
	const FWLStateCapacityState Capacity = Political->GetStateCapacity(Iso);
	int32 ActiveCrises = 0;
	for (const FWLCrisisChainState& Chain : Political->GetActiveCrisisChains(Iso))
	{
		if (!Chain.bResolved)
		{
			++ActiveCrises;
		}
	}

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("PULSO DE GOBIERNO")), 20.f);
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(5.f));
	auto Place = [&](int32 Row, int32 Col, UBorder* Card)
	{
		if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, Row, Col))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	const FLinearColor ApprColor = SupportColor(Media.PresidentialApproval);
	const FLinearColor LegitColor = SupportColor(Election.Legitimacy);
	const FLinearColor ElecColor = Election.MonthsToElection <= 6 ? GovWarn : GovText;
	const FLinearColor CoalColor = SupportColor(Institutions.RulingCoalitionSupport);
	const FLinearColor CapColor = SupportColor(Capacity.AdministrativeEfficiency);
	const FLinearColor CrisisColor = ActiveCrises > 0 ? GovBad : GovGood;
	Place(0, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Approval, ApprColor, TEXT("Aprobacion"),
		FString::Printf(TEXT("%d%%"), Media.PresidentialApproval), ApprColor));
	Place(0, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Capital, LegitColor, TEXT("Legitimidad"),
		FString::Printf(TEXT("%d"), Election.Legitimacy), LegitColor));
	Place(1, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Politics, ElecColor, TEXT("Eleccion"),
		FString::Printf(TEXT("%d meses"), Election.MonthsToElection), ElecColor));
	Place(1, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Population, CoalColor, TEXT("Coalicion"),
		FString::Printf(TEXT("%d"), Institutions.RulingCoalitionSupport), CoalColor));
	Place(2, 0, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Order, CapColor, TEXT("Capacidad estatal"),
		FString::Printf(TEXT("%d"), Capacity.AdministrativeEfficiency), CapColor));
	Place(2, 1, MakeMetricCardIcon(WidgetTree, EWLGovIcon::Crisis, CrisisColor, TEXT("Crisis activas"),
		FString::Printf(TEXT("%d"), ActiveCrises), CrisisColor));
	AddColumnChild(CenterBox, Grid, 10.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("El detalle vive en POLITICA: agenda, programas, leyes, Congreso, elecciones, medios, regiones y crisis."),
		12, GovMuted, ETextJustify::Left, true), 4.f);
}

// ALTO MANDO: gabinete vivo — riesgos internos del propio gobierno.
void UWLGovernmentWidget::BuildCabinetDynamicsCard()
{
	const FString Iso = PlayerIso();
	const UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		return;
	}
	const FWLCabinetDynamicsState Dynamics = Political->GetCabinetDynamics(Iso);

	UBorder* Card = MakeCard(WidgetTree, GovCard, FMargin(14.f, 11.f));
	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Title = MakeText(WidgetTree, TEXT("GABINETE VIVO"), 13, GovGold);
	Title->SetToolTipText(FText::FromString(
		TEXT("Tu gabinete tiene vida propia: rivalidades y ambicion generan escandalos, sabotajes y renuncias.")));
	VB->AddChildToVerticalBox(Title);

	// Cada tension como mini-medidor (etiqueta + valor + barra de color), no una linea de texto corrido.
	{
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		Grid->SetSlotPadding(FMargin(6.f, 4.f));
		int32 Cell = 0;
		auto AddRiskGauge = [&](const TCHAR* Label, int32 Value)
		{
			UVerticalBox* GaugeVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(MakeText(WidgetTree, Label, 11, GovMuted)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			Line->AddChildToHorizontalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("%d"), Value), 11, RiskColor(Value), ETextJustify::Right));
			GaugeVB->AddChildToVerticalBox(Line);
			if (UVerticalBoxSlot* S = GaugeVB->AddChildToVerticalBox(MakeBar(WidgetTree, Value / 100.f, RiskColor(Value), 6.f)))
			{
				S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
			}
			if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(GaugeVB, Cell / 3, Cell % 3))
			{
				S->SetHorizontalAlignment(HAlign_Fill);
			}
			++Cell;
		};
		AddRiskGauge(TEXT("Rivalidad"), Dynamics.RivalryPressure);
		AddRiskGauge(TEXT("Faccionalismo"), Dynamics.Factionalism);
		AddRiskGauge(TEXT("Escandalo"), Dynamics.ScandalRisk);
		AddRiskGauge(TEXT("Sabotaje"), Dynamics.SabotageRisk);
		AddRiskGauge(TEXT("Renuncia"), Dynamics.ResignationRisk);
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(Grid))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
	}
	if (!Dynamics.LastIncident.IsEmpty())
	{
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Ultimo incidente: %s"), *Dynamics.LastIncident), 11, GovBad, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	}
	Card->SetContent(VB);
	AddColumnChild(CenterBox, Card, 10.f);
}

// ALTO MANDO: comparador de candidatos a una cartera (skill/lealtad/ambicion + ficha politica).
void UWLGovernmentWidget::BuildMinisterComparator(EWLMinisterOffice Office)
{
	const FString Iso = PlayerIso();
	UWLCharacterSubsystem* Characters = GetCharacters();
	const UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Characters || Iso.IsEmpty())
	{
		return;
	}

	UBorder* Panel = MakeBorder(WidgetTree, GovPanelSoft, FMargin(12.f, 10.f));
	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(
		TEXT("CANDIDATOS — Ministerio de %s"), *UWLCharacterSubsystem::MinisterOfficeToString(Office)),
		14, GovGold)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	Head->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this, TEXT("closecompare"), TEXT("CERRAR"), GovTabIdle, 80.f, 11));
	VB->AddChildToVerticalBox(Head);

	// Coste politico de nombrar (leido del backend, no hardcodeado en la UI).
	const int32 AppointCost = Characters->GetMinisterAppointmentCost();
	const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
		TEXT("Nombrar cuesta %d de capital politico (tienes %d)."), AppointCost, Stats.PoliticalCapital),
		11, Stats.PoliticalCapital < AppointCost ? GovBad : GovMuted, ETextJustify::Left, true)))
	{
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	FWLCharacter Current;
	if (Characters->GetCabinetMinister(Iso, Office, Current) && Current.IsValid())
	{
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Nombrar aqui reemplaza a %s (skill %d, lealtad %d). Se pedira confirmacion."),
			*Current.Name, Current.Skill, Current.Loyalty), 11, GovGold, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	}

	// Candidatos: ministros activos sin cargo; primero los de la cartera, luego por skill.
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

	if (Candidates.Num() == 0)
	{
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(WidgetTree,
			TEXT("No hay candidatos libres. CONTRATAR trae un candidato nuevo para esta cartera (cuesta tesoro y capital)."),
			12, GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
	}
	int32 Shown = 0;
	for (const FWLCharacter& Candidate : Candidates)
	{
		if (Shown >= 6)
		{
			break;
		}
		UBorder* Row = MakeCard(WidgetTree, (Shown % 2 == 0) ? GovCard : GovCardAlt, FMargin(11.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakePortrait(WidgetTree, Candidate.Id, GovGoldDim, 44.f, 54.f)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(MakeText(WidgetTree, Candidate.Name, 13, GovText)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (Candidate.PreferredOffice == Office)
		{
			NameRow->AddChildToHorizontalBox(MakeBadge(WidgetTree, TEXT("SU CARTERA"), GovGoldDim, GovDarkInk));
		}
		Info->AddChildToVerticalBox(NameRow);
		Info->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Skill %d · Lealtad %d · Ambicion %d · Popularidad %d"),
			Candidate.Skill, Candidate.Loyalty, Candidate.Ambition, Candidate.Popularity),
			11, Candidate.Loyalty < 40 ? GovBad : GovMuted, ETextJustify::Left, true));
		if (Candidate.Traits.Num() > 0)
		{
			Info->AddChildToVerticalBox(MakeText(WidgetTree,
				FString::Join(Candidate.Traits, TEXT(" · ")), 10, GovGoldDim, ETextJustify::Left, true));
		}
		FWLCharacterPoliticalProfile Profile;
		if (Political && Political->GetCharacterPoliticalProfile(Candidate.Id, Profile))
		{
			Info->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
				TEXT("Corrupcion personal %d · Escandalo %d · Ambicion presidencial %d"),
				Profile.PersonalCorruption, Profile.ScandalHeat, Profile.PresidentialAmbition),
				10, (Profile.PersonalCorruption >= 50 || Profile.ScandalHeat >= 50) ? GovBad : GovMuted,
				ETextJustify::Left, true));
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Info))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("appointc:%d:%s"), static_cast<int32>(Office), *Candidate.Id),
			TEXT("NOMBRAR"), GovGoldDim, 100.f, 11)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Row->SetContent(HB);
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(Row))
		{
			S->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
		}
		++Shown;
	}

	Panel->SetContent(VB);
	AddColumnChild(CenterBox, Panel, 6.f);
}

// ALTO MANDO: fichas politicas de personajes — facciones, sucesion, escandalos y vinculos.
void UWLGovernmentWidget::BuildPoliticalProfilesSection()
{
	const FString Iso = PlayerIso();
	const UWLPoliticalSubsystem* Political = GetPolitical();
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Political || !Characters || Iso.IsEmpty())
	{
		return;
	}
	TArray<FWLCharacterPoliticalProfile> Profiles = Political->GetCharacterPoliticalProfiles(Iso);
	if (Profiles.Num() == 0)
	{
		return;
	}

	auto ResolveName = [&](const FString& CharacterId) -> FString
	{
		FWLCharacter Character;
		return Characters->GetCharacter(CharacterId, Character) ? Character.Name : CharacterId;
	};
	auto LoyaltyOf = [&](const FString& CharacterId) -> int32
	{
		FWLCharacter Character;
		return Characters->GetCharacter(CharacterId, Character) ? Character.Loyalty : 0;
	};

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("PERFILES POLITICOS  (%d)"), Profiles.Num())), 18.f);

	// Chips de ordenamiento.
	const struct { int32 Mode; const TCHAR* Label; } SortModes[] = {
		{ 0, TEXT("SUCESION") }, { 1, TEXT("AMBICION") }, { 2, TEXT("CORRUPCION") }, { 3, TEXT("LEALTAD") } };
	UHorizontalBox* SortChips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const auto& Def : SortModes)
	{
		const bool bActive = ProfileSortMode == Def.Mode;
		if (UHorizontalBoxSlot* S = SortChips->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("profsort:%d"), Def.Mode), Def.Label,
			bActive ? GovGold : GovTabIdle, 96.f, 10, true,
			bActive ? GovDarkInk : GovMuted)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
		}
	}
	AddColumnChild(CenterBox, SortChips, 6.f);

	Profiles.Sort([this, &LoyaltyOf](const FWLCharacterPoliticalProfile& A, const FWLCharacterPoliticalProfile& B)
	{
		switch (ProfileSortMode)
		{
		case 1:  return A.PresidentialAmbition > B.PresidentialAmbition;
		case 2:  return A.PersonalCorruption > B.PersonalCorruption;
		case 3:  return LoyaltyOf(A.CharacterId) < LoyaltyOf(B.CharacterId);   // los menos leales primero
		default: return A.SuccessionScore > B.SuccessionScore;
		}
	});

	int32 Shown = 0;
	for (const FWLCharacterPoliticalProfile& Profile : Profiles)
	{
		if (Shown >= 12)
		{
			break;
		}
		FWLCharacter Character;
		if (!Characters->GetCharacter(Profile.CharacterId, Character) || !Character.bActive)
		{
			continue;
		}
		UBorder* Card = MakeCard(WidgetTree, (Shown % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(MakePortrait(WidgetTree, Profile.CharacterId,
			Character.Loyalty < 40 ? GovBad : GovGoldDim, 48.f, 58.f)))
		{
			S->SetVerticalAlignment(VAlign_Top);
			S->SetPadding(FMargin(0.f, 0.f, 11.f, 0.f));
		}
		UVerticalBox* PVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree, Character.Name, 14, GovText)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (!Profile.FactionId.IsEmpty())
		{
			Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, Profile.FactionId.ToUpper(), GovHeaderStrip, GovGold));
		}
		PVB->AddChildToVerticalBox(Head);
		if (!Profile.Biography.IsEmpty())
		{
			PVB->AddChildToVerticalBox(MakeText(WidgetTree, Profile.Biography, 11, GovMuted, ETextJustify::Left, true));
		}
		if (UVerticalBoxSlot* S = PVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Sucesion %d · Ambicion presidencial %d · Corrupcion %d · Escandalo %d · Lealtad %d"),
			Profile.SuccessionScore, Profile.PresidentialAmbition, Profile.PersonalCorruption,
			Profile.ScandalHeat, Character.Loyalty),
			11, (Profile.ScandalHeat >= 50 || Character.Loyalty < 40) ? GovBad : GovGoldDim,
			ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}
		FString Links;
		if (!Profile.PatronCharacterId.IsEmpty())
		{
			Links += FString::Printf(TEXT("Patron: %s"), *ResolveName(Profile.PatronCharacterId));
		}
		if (Profile.AllyCharacterIds.Num() > 0)
		{
			TArray<FString> Names;
			for (const FString& Id : Profile.AllyCharacterIds)
			{
				Names.Add(ResolveName(Id));
			}
			Links += FString::Printf(TEXT("%sAliados: %s"), Links.IsEmpty() ? TEXT("") : TEXT(" · "), *FString::Join(Names, TEXT(", ")));
		}
		if (Profile.RivalCharacterIds.Num() > 0)
		{
			TArray<FString> Names;
			for (const FString& Id : Profile.RivalCharacterIds)
			{
				Names.Add(ResolveName(Id));
			}
			Links += FString::Printf(TEXT("%sRivales: %s"), Links.IsEmpty() ? TEXT("") : TEXT(" · "), *FString::Join(Names, TEXT(", ")));
		}
		if (!Links.IsEmpty())
		{
			PVB->AddChildToVerticalBox(MakeText(WidgetTree, Links, 11, GovMuted, ETextJustify::Left, true));
		}
		if (Profile.AgendaTags.Num() > 0)
		{
			PVB->AddChildToVerticalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("Agenda: %s"), *FString::Join(Profile.AgendaTags, TEXT(" · "))),
				10, GovGoldDim, ETextJustify::Left, true));
		}
		if (!Profile.LastProfileEvent.IsEmpty())
		{
			PVB->AddChildToVerticalBox(MakeText(WidgetTree, Profile.LastProfileEvent, 11, GovBad, ETextJustify::Left, true));
		}
		if (UHorizontalBoxSlot* S = CardRow->AddChildToHorizontalBox(PVB))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Card->SetContent(CardRow);
		AddColumnChild(CenterBox, Card, 4.f);
		++Shown;
	}
}

// REGISTROS: plan de la IA politica de cada pais de America (solo expone GetGovernmentAIPlan).
void UWLGovernmentWidget::BuildAIPlansPanel()
{
	const FString Iso = PlayerIso();
	const UWLPoliticalSubsystem* Political = GetPolitical();
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Political || !Registry)
	{
		return;
	}

	// Recolecta estimaciones visibles; no expone planes internos crudos de todos los paises.
	struct FAIPlanRow { FString Name; FWLPoliticalAIPlanState Plan; bool bHighConfidence = false; };
	TArray<FAIPlanRow> Notable;
	int32 ObjectiveHistogram[6] = { 0, 0, 0, 0, 0, 0 };   // indexado por (int32)EWLGovernmentAIObjective
	int32 UnknownCount = 0;
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		if (Nation.Iso.Equals(Iso, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const FWLPoliticalAIPlanState Plan = Political->GetGovernmentAIPlan(Nation.Iso);
		FWLDiplomaticRelationState Relation;
		const bool bHasRelation = Political->GetRelation(Iso, Nation.Iso, Relation);
		const FWLIntelligenceNetworkState Network = Political->GetIntelligenceNetwork(Iso, Nation.Iso);
		const bool bVisibleEstimate =
			(Plan.TargetIso == Iso)
			|| Network.NetworkStrength >= 25
			|| (bHasRelation && Relation.Opinion >= 45)
			|| (bHasRelation && Relation.Status == EWLDiplomaticStatus::War);
		if (!bVisibleEstimate)
		{
			++UnknownCount;
			continue;
		}
		const int32 ObjIndex = static_cast<int32>(Plan.Objective);
		if (ObjIndex >= 0 && ObjIndex < 6)
		{
			++ObjectiveHistogram[ObjIndex];
		}
		const bool bNotable = Plan.Objective != EWLGovernmentAIObjective::Stabilize
			|| !Plan.TargetIso.IsEmpty()
			|| !Plan.CurrentProgramId.IsEmpty();
		if (bNotable)
		{
			const bool bHighConfidence = Network.NetworkStrength >= 50 || (bHasRelation && Relation.Status == EWLDiplomaticStatus::War);
			Notable.Add({ Nation.Name, Plan, bHighConfidence });
		}
	}

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("PANORAMA CONTINENTAL")), 18.f);

	// Resumen del continente: cuantos gobiernos persiguen cada objetivo (solo cubos con datos).
	{
		const EWLGovernmentAIObjective Order[] = {
			EWLGovernmentAIObjective::Stabilize, EWLGovernmentAIObjective::Expand,
			EWLGovernmentAIObjective::Militarize, EWLGovernmentAIObjective::Borrow,
			EWLGovernmentAIObjective::Align, EWLGovernmentAIObjective::Industrialize };
		FString Summary;
		for (const EWLGovernmentAIObjective Obj : Order)
		{
			const int32 Count = ObjectiveHistogram[static_cast<int32>(Obj)];
			if (Count > 0)
			{
				Summary += (Summary.IsEmpty() ? TEXT("") : TEXT("   ·   ")) + FString::Printf(TEXT("%d %s"), Count, *AIObjectiveToText(Obj).ToLower());
			}
		}
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			Summary.IsEmpty()
				? FString::Printf(TEXT("Sin estimaciones firmes. %d gobiernos sin inteligencia suficiente."), UnknownCount)
				: FString::Printf(TEXT("Postura estimada: %s   ·   %d sin inteligencia suficiente"), *Summary, UnknownCount),
			12, GovMuted, ETextJustify::Left, true), 4.f);
	}

	if (Notable.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("No hay movimientos externos confirmados. Mejora relaciones o construye redes de inteligencia para convertir rumores en estimaciones utiles."),
			12, GovMuted, ETextJustify::Left, true), 6.f);
		return;
	}

	// Los planes mas asentados primero (mas meses = movimiento sostenido, mas relevante).
	Notable.Sort([](const FAIPlanRow& A, const FAIPlanRow& B) { return A.Plan.MonthsOnPlan > B.Plan.MonthsOnPlan; });
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("MOVIMIENTOS NOTABLES  (%d)"), Notable.Num()), 13, GovMuted), 8.f);
	int32 Index = 0;
	for (const FAIPlanRow& Entry : Notable)
	{
		const FWLPoliticalAIPlanState& Plan = Entry.Plan;
		UBorder* Row = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
		UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree, Entry.Name, 13, GovText)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		const bool bAggressive = Plan.Objective == EWLGovernmentAIObjective::Militarize
			|| Plan.Objective == EWLGovernmentAIObjective::Expand;
		Head->AddChildToHorizontalBox(MakeBadge(WidgetTree, AIObjectiveToText(Plan.Objective).ToUpper(),
			bAggressive ? GovDanger : GovHeaderStrip, bAggressive ? GovText : GovGold));
		VB->AddChildToVerticalBox(Head);
		FString Detail = FString::Printf(TEXT("%d meses en plan"), Plan.MonthsOnPlan);
		if (!Plan.TargetIso.IsEmpty())
		{
			Detail += FString::Printf(TEXT(" · objetivo: %s"), *Plan.TargetIso);
		}
		if (!Plan.CurrentProgramId.IsEmpty())
		{
			Detail += FString::Printf(TEXT(" · programa: %s"), *Plan.CurrentProgramId);
		}
		VB->AddChildToVerticalBox(MakeText(WidgetTree, Detail, 11, GovGoldDim, ETextJustify::Left, true));
		if (Entry.bHighConfidence && !Plan.LastPlanReason.IsEmpty())
		{
			VB->AddChildToVerticalBox(MakeText(WidgetTree, Plan.LastPlanReason, 11, GovMuted, ETextJustify::Left, true));
		}
		else
		{
			VB->AddChildToVerticalBox(MakeText(WidgetTree,
				TEXT("Estimacion basada en relacion, conflicto o inteligencia disponible; la razon interna exacta no esta confirmada."),
				11, GovMuted, ETextJustify::Left, true));
		}
		Row->SetContent(VB);
		AddColumnChild(CenterBox, Row, 4.f);
		++Index;
	}
}

// ALTO MANDO: ejercitos con general asignado, composicion, reservas y reorganizar (ReorganizeArmy).
void UWLGovernmentWidget::BuildArmiesSection()
{
	const FString Iso = PlayerIso();
	UWLMilitarySubsystem* Military = GetMilitary();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Military || !Characters || Iso.IsEmpty())
	{
		return;
	}

	TArray<FWLArmy> Armies = Military->GetArmies();
	Armies.RemoveAll([&Iso](const FWLArmy& Army)
	{
		return !Army.OwnerIso.Equals(Iso, ESearchCase::IgnoreCase);
	});

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree,
		FString::Printf(TEXT("EJERCITOS  (%d)"), Armies.Num())), 18.f);
	if (Armies.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin ejercitos en campo. Recluta en un fuerte para desplegar una fuerza movil."),
			12, GovMuted, ETextJustify::Left, true), 6.f);
		return;
	}

	int32 Index = 0;
	for (const FWLArmy& Army : Armies)
	{
		UBorder* Card = MakeCard(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 10.f));
		UVerticalBox* AVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// Cabecera: id + ataque/defensa + provincia.
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Ejercito %s"), *Army.Id), 14, GovText)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Head->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("ATK %d · DEF %d"), Military->GetArmyAttack(Army.Id), Military->GetArmyDefense(Army.Id)),
			12, GovGold, ETextJustify::Right));
		AVB->AddChildToVerticalBox(Head);

		// Composicion: efectivas + desorganizadas (RecoveringUnits) + provincia.
		if (UVerticalBoxSlot* S = AVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Unidades efectivas: %d   ·   Desorganizadas: %d   ·   Provincia: %s"),
			Army.Units.Num(), Army.RecoveringUnits.Num(),
			Army.ProvinceId.IsEmpty() ? TEXT("en transito") : *Army.ProvinceId),
			12, Army.RecoveringUnits.Num() > 0 ? GovBad : GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		// General asignado (ficha real desde el subsystem de personajes).
		FWLCharacter General;
		const bool bHasGeneral = Characters->GetAssignedGeneralForArmy(Army.Id, General) && General.IsValid();
		if (bHasGeneral)
		{
			const FLinearColor LoyaltyColor = General.Loyalty < 40 ? GovBad : (General.Loyalty < 60 ? GovGold : GovGood);
			if (UVerticalBoxSlot* S = AVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
				TEXT("General: %s (%s) · Skill %d · Lealtad %d · Renombre %d%s"),
				*General.Name, *RankToText(General.Rank), General.Skill, General.Loyalty, General.Renown,
				General.Traits.Num() > 0 ? *FString::Printf(TEXT(" · %s"), *FString::Join(General.Traits, TEXT(", "))) : TEXT("")),
				12, LoyaltyColor, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
		}
		else
		{
			if (UVerticalBoxSlot* S = AVB->AddChildToVerticalBox(MakeText(WidgetTree,
				TEXT("General: sin mando asignado (penaliza el rendimiento en batalla)."), 12, GovGold, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
		}

		// Acciones: reorganizar (si hay reservas) + asignar/reasignar general.
		UWrapBox* Actions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		auto AddArmyAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg)
		{
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Actions->AddChildToWrapBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 0.f, 11))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
			}
		};
		if (Army.RecoveringUnits.Num() > 0)
		{
			AddArmyAction(FString::Printf(TEXT("reorg:%s"), *Army.Id),
				FString::Printf(TEXT("REORGANIZAR (%d)"), Army.RecoveringUnits.Num()), GovGoldDim);
		}
		// Reasignacion/rotacion: permite mover generales ya asignados a otros ejercitos.
		TArray<FWLCharacter> AssignableGenerals = Characters->GetGenerals(Iso);
		AssignableGenerals.RemoveAll([&Army](const FWLCharacter& G)
		{
			return !G.bActive || G.AssignedArmyId == Army.Id;
		});
		AssignableGenerals.Sort([](const FWLCharacter& A, const FWLCharacter& B)
		{
			const bool bAFree = A.AssignedArmyId.IsEmpty();
			const bool bBFree = B.AssignedArmyId.IsEmpty();
			if (bAFree != bBFree)
			{
				return bAFree;
			}
			return A.Skill > B.Skill;
		});
		int32 Shown = 0;
		for (const FWLCharacter& Candidate : AssignableGenerals)
		{
			if (Shown >= 4)
			{
				break;
			}
			const FString Verb = Candidate.AssignedArmyId.IsEmpty()
				? (bHasGeneral ? FString(TEXT("CAMBIAR A")) : FString(TEXT("ASIGNAR")))
				: FString::Printf(TEXT("ROTAR %s"), *Candidate.AssignedArmyId);
			AddArmyAction(FString::Printf(TEXT("assigngen:%s:%s"), *Candidate.Id, *Army.Id),
				FString::Printf(TEXT("%s: %s (sk %d)"), *Verb, *Candidate.Name, Candidate.Skill), GovTabIdle);
			++Shown;
		}
		if (UVerticalBoxSlot* S = AVB->AddChildToVerticalBox(Actions))
		{
			S->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
		}

		// Flujo de combate: objetivos enemigos atacables ahora (en guerra + misma/adyacente provincia).
		const TArray<FString> TargetIds = Military->GetAttackableTargetIds(Army.Id);
		if (TargetIds.Num() > 0)
		{
			if (UVerticalBoxSlot* S = AVB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Objetivos al alcance:"), 11, GovBad)))
			{
				S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			}
			UWrapBox* AttackRow = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
			for (const FString& TargetId : TargetIds)
			{
				FWLArmy Target;
				const FString TargetLabel = Military->GetArmy(TargetId, Target)
					? FString::Printf(TEXT("ATACAR %s (%s)"), *TargetId, *Target.OwnerIso)
					: FString::Printf(TEXT("ATACAR %s"), *TargetId);
				if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(AttackRow->AddChildToWrapBox(MakeActionButton(WidgetTree, this,
					FString::Printf(TEXT("battlepick:%s:%s"), *Army.Id, *TargetId), TargetLabel, GovDanger, 0.f, 11))))
				{
					S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
				}
			}
			if (UVerticalBoxSlot* S = AVB->AddChildToVerticalBox(AttackRow))
			{
				S->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
			}
		}

		Card->SetContent(AVB);
		AddColumnChild(CenterBox, Card, 5.f);

		// Preview de combate abierto justo debajo del ejercito atacante elegido.
		if (BattleAttackerId == Army.Id && !BattleDefenderId.IsEmpty())
		{
			BuildBattlePreviewPanel();
		}
		++Index;
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Reorganizar devuelve las unidades desorganizadas al frente. ATACAR abre el desglose de poder antes de resolver. Solo hay objetivos si declaraste la guerra y los ejercitos estan en la misma provincia o adyacentes."),
		12, GovMuted, ETextJustify::Left, true), 4.f);
}

// ALTO MANDO: desglose de poder del combate elegido + eleccion auto-resolve vs tactica (con confirmacion).
void UWLGovernmentWidget::BuildBattlePreviewPanel()
{
	UWLMilitarySubsystem* Military = GetMilitary();
	if (!Military || BattleAttackerId.IsEmpty() || BattleDefenderId.IsEmpty())
	{
		return;
	}
	FWLBattlePreview Preview;
	Military->PreviewBattle(BattleAttackerId, BattleDefenderId, Preview);

	UBorder* Panel = MakeBorder(WidgetTree, GovPanelSoft, FMargin(12.f, 10.f));
	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
		FString::Printf(TEXT("BATALLA: %s  vs  %s"), *BattleAttackerId, *BattleDefenderId), 14, GovGold)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	Head->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this, TEXT("battlecancel"), TEXT("CERRAR"), GovTabIdle, 80.f, 11));
	VB->AddChildToVerticalBox(Head);

	if (!Preview.bValid)
	{
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeAlert(WidgetTree,
			Preview.Reason.IsEmpty() ? TEXT("Este combate no es valido ahora mismo.") : Preview.Reason, GovBad)))
		{
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}
		Panel->SetContent(VB);
		AddColumnChild(CenterBox, Panel, 6.f);
		return;
	}

	// Balanza de poder: ATACANTE vs DEFENSOR con barra proporcional.
	const int32 Total = FMath::Max(1, Preview.AttackerPower + Preview.DefenderPower);
	const float AtkFrac = static_cast<float>(Preview.AttackerPower) / static_cast<float>(Total);
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
		TEXT("Poder de ataque %d  ·  Poder de defensa %d  ·  Pronostico: %s"),
		Preview.AttackerPower, Preview.DefenderPower, *Preview.OddsLabel),
		13, Preview.OddsLabel == TEXT("Favorable") ? GovGood : (Preview.OddsLabel == TEXT("Desfavorable") ? GovBad : GovGold),
		ETextJustify::Left, true)))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeBar(WidgetTree, AtkFrac, GovGold, 12.f)))
	{
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	// Desglose de modificadores.
	AddColumnChild(VB, MakeStatRow(WidgetTree, TEXT("Composicion (unidades)"),
		FString::Printf(TEXT("%d  vs  %d"), Preview.AttackerUnits, Preview.DefenderUnits), GovText, GovCard), 8.f);
	AddColumnChild(VB, MakeStatRow(WidgetTree, TEXT("Poder base (atk / def)"),
		FString::Printf(TEXT("%d  /  %d"), Preview.AttackerBaseAttack, Preview.DefenderBaseDefense), GovText, GovCardAlt), 4.f);
	AddColumnChild(VB, MakeStatRow(WidgetTree, TEXT("General atacante"),
		Preview.AttackerGeneral.IsEmpty() ? TEXT("sin mando") : Preview.AttackerGeneral,
		Preview.AttackerGeneral.IsEmpty() ? GovBad : GovText, GovCard), 4.f);
	AddColumnChild(VB, MakeStatRow(WidgetTree, TEXT("General defensor"),
		Preview.DefenderGeneral.IsEmpty() ? TEXT("sin mando") : Preview.DefenderGeneral,
		Preview.DefenderGeneral.IsEmpty() ? GovMuted : GovText, GovCardAlt), 4.f);
	AddColumnChild(VB, MakeStatRow(WidgetTree, TEXT("Defensa por terreno"),
		FString::Printf(TEXT("%s  x%.2f"), Preview.TerrainLabel.IsEmpty() ? TEXT("llano") : *Preview.TerrainLabel, Preview.TerrainMultiplier),
		Preview.TerrainMultiplier > 1.0 ? GovBad : GovMuted, GovCard), 4.f);
	if (Preview.DefenderBuildingMultiplier > 1.001)
	{
		AddColumnChild(VB, MakeStatRow(WidgetTree, TEXT("Defensa por edificios"),
			FString::Printf(TEXT("x%.2f"), Preview.DefenderBuildingMultiplier), GovBad, GovCardAlt), 4.f);
	}

	// Eleccion de resolucion (ambas con confirmacion en dos clics).
	UHorizontalBox* ResolveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = ResolveRow->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
		FString::Printf(TEXT("autoresolve:%s:%s"), *BattleAttackerId, *BattleDefenderId),
		TEXT("AUTO-RESOLVER"), GovGoldDim, 170.f, 12)))
	{
		S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* S = ResolveRow->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
		FString::Printf(TEXT("tacticalresolve:%s:%s"), *BattleAttackerId, *BattleDefenderId),
		TEXT("BATALLA TACTICA"), GovDanger, 170.f, 12)))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(ResolveRow))
	{
		S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(WidgetTree,
		TEXT("Auto-resolver calcula el resultado por poderes al instante. Batalla tactica abre el campo de batalla 3D donde comandas tu bando y el enemigo responde solo, y aplica bajas/ocupacion al volver. Ambas requieren guerra declarada."),
		11, GovMuted, ETextJustify::Left, true)))
	{
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	Panel->SetContent(VB);
	AddColumnChild(CenterBox, Panel, 6.f);
}

// RESUMEN: nivel de dificultad de IA activo + selector (lee/escribe UWLBalanceSubsystem).
void UWLGovernmentWidget::BuildDifficultyPanel()
{
	UWLBalanceSubsystem* Balance = GetBalance();
	if (!Balance)
	{
		return;
	}
	const EWLAIDifficulty Active = Balance->GetAIDifficulty();

	auto DifficultyLabel = [](EWLAIDifficulty D)
	{
		switch (D)
		{
		case EWLAIDifficulty::Easy: return TEXT("FACIL");
		case EWLAIDifficulty::Hard: return TEXT("DIFICIL");
		default:                    return TEXT("MEDIO");
		}
	};
	auto DifficultyBlurb = [](EWLAIDifficulty D) -> FString
	{
		switch (D)
		{
		case EWLAIDifficulty::Easy:
			return TEXT("La IA es pasiva: economia holgada, poca presion diplomatica, golpes e invasiones raros.");
		case EWLAIDifficulty::Hard:
			return TEXT("La IA es agresiva: mejor economia rival, mas guerra e intriga, golpes probables, reclutamiento acelerado.");
		default:
			return TEXT("Equilibrio de referencia: economia, diplomacia, guerra e intriga a ritmo estandar.");
		}
	};

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("DIFICULTAD")), 20.f);

	UBorder* Card = MakeCard(WidgetTree, GovCard, FMargin(14.f, 11.f));
	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Info->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("NIVEL ACTIVO"), 12, GovMuted));
	if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(MakeText(WidgetTree, DifficultyLabel(Active), 24, GovGold)))
	{
		S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
	}
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Info))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	// Selector: 3 botones; el activo va resaltado.
	const EWLAIDifficulty Levels[] = { EWLAIDifficulty::Easy, EWLAIDifficulty::Medium, EWLAIDifficulty::Hard };
	for (const EWLAIDifficulty Level : Levels)
	{
		const bool bLevelActive = Level == Active;
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
			FString::Printf(TEXT("difficulty:%d"), static_cast<int32>(Level)),
			DifficultyLabel(Level), bLevelActive ? GovGold : GovTabIdle, 84.f, 12, true,
			bLevelActive ? GovDarkInk : GovMuted)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(5.f, 0.f, 0.f, 0.f));
		}
	}
	VB->AddChildToVerticalBox(Row);
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(WidgetTree, DifficultyBlurb(Active), 12, GovMuted, ETextJustify::Left, true)))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	Card->SetContent(VB);
	AddColumnChild(CenterBox, Card, 8.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Cambia como juegan los gobiernos rivales (economia, fisco, diplomacia, guerra, intriga, reclutamiento). El efecto entra en el proximo cierre de mes."),
		12, GovMuted, ETextJustify::Left, true), 4.f);
}

// REGISTROS: diagnostico jugable de dilemas nacionales.
void UWLGovernmentWidget::BuildCalibrationPanel()
{
	const FString Iso = PlayerIso();
	const UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		return;
	}
	const FWLGovernmentCalibrationState Calibration = Political->GetGovernmentCalibration(Iso);

	AddColumnChild(CenterBox, MakeSectionTitle(WidgetTree, TEXT("DIAGNOSTICO DEL REGIMEN")), 18.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
		TEXT("Lectura estrategica de tensiones acumuladas. %d meses observados."),
		Calibration.MonthsObserved), 12, GovMuted, ETextJustify::Left, true), 4.f);

	const struct { const TCHAR* Label; int32 Value; } Dilemmas[] = {
		{ TEXT("Deuda vs crecimiento"),        Calibration.DebtVsGrowthPressure },
		{ TEXT("Represion vs legitimidad"),    Calibration.RepressionLegitimacyPressure },
		{ TEXT("Subsidios vs inflacion"),      Calibration.SubsidyInflationPressure },
		{ TEXT("Reformas vs gridlock"),        Calibration.ReformGridlockPressure },
		{ TEXT("Militares vs civiles"),        Calibration.CivilMilitaryTension },
		{ TEXT("Aliados vs soberania"),        Calibration.SovereigntyAlliancePressure },
	};
	int32 Index = 0;
	bool bAnyDominant = false;
	for (const auto& Dilemma : Dilemmas)
	{
		AddColumnChild(CenterBox, MakeGaugeRow(WidgetTree, Dilemma.Label, Dilemma.Value,
			RiskColor(Dilemma.Value, 50, 75), (Index % 2 == 0) ? GovCard : GovCardAlt), Index == 0 ? 8.f : 4.f);
		bAnyDominant |= Dilemma.Value >= 75;
		++Index;
	}
	if (bAnyDominant)
	{
		AddColumnChild(CenterBox, MakeAlert(WidgetTree,
			TEXT("Un dilema domina la agenda nacional (>=75): actua pronto o se convertira en crisis recurrente."), GovBad), 6.f);
	}
	if (!Calibration.LastReport.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, Calibration.LastReport, 12, GovMuted, ETextJustify::Left, true), 6.f);
	}
}
