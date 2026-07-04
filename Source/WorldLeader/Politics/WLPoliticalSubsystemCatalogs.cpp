// Copyright World Leader project. See ROADMAP.md.

#include "Politics/WLPoliticalSubsystemPrivate.h"
#include "Military/WLMilitarySubsystem.h"
#include "WorldLeader.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace WLPoliticsPrivate
{
	const TArray<EWLPublicGroup>& AllPublicGroups()
	{
		static const TArray<EWLPublicGroup> Groups = {
			EWLPublicGroup::Business,
			EWLPublicGroup::Military,
			EWLPublicGroup::Workers,
			EWLPublicGroup::Regions,
			EWLPublicGroup::MiddleClass,
			EWLPublicGroup::Unions
		};
		return Groups;
	}

	int32 MilitaryRankInfluence(EWLMilitaryRank Rank)
	{
		switch (Rank)
		{
		case EWLMilitaryRank::FieldMarshal:     return 5;
		case EWLMilitaryRank::CorpsGeneral:     return 4;
		case EWLMilitaryRank::DivisionGeneral:  return 3;
		case EWLMilitaryRank::BrigadeGeneral:   return 2;
		default:                                return 1;
		}
	}

	int32 GeneralPoliticalWeight(const FWLCharacter& General, const UWLMilitarySubsystem* Military)
	{
		const bool bHasCommand = !General.AssignedArmyId.IsEmpty();
		const bool bHasIndependentStanding =
			General.Rank != EWLMilitaryRank::Colonel
			|| General.Renown >= 25
			|| General.Popularity >= 65;
		if (!bHasCommand && !bHasIndependentStanding)
		{
			return 0;
		}

		int32 Weight = bHasCommand ? 4 : 1;
		Weight += MilitaryRankInfluence(General.Rank);
		Weight += FMath::Clamp(General.Renown / 25, 0, 4);
		Weight += FMath::Clamp((General.Popularity - 50) / 15, 0, 3);

		if (bHasCommand && Military)
		{
			FWLArmy Army;
			if (Military->GetArmy(General.AssignedArmyId, Army))
			{
				Weight += FMath::Clamp(Army.Units.Num() + Army.RecoveringUnits.Num(), 1, 10);
			}
		}
		return FMath::Max(0, Weight);
	}

	FString PoliticalActionTypeToString(EWLPoliticalActionType Type)
	{
		switch (Type)
		{
		case EWLPoliticalActionType::SetAgenda: return TEXT("agenda");
		case EWLPoliticalActionType::StartProgram: return TEXT("programa");
		case EWLPoliticalActionType::EnactReform: return TEXT("reforma");
		case EWLPoliticalActionType::ResolveEvent: return TEXT("evento");
		case EWLPoliticalActionType::RepressOpposition: return TEXT("represion");
		case EWLPoliticalActionType::NegotiatePartySupport: return TEXT("negociacion_partido");
		case EWLPoliticalActionType::HoldPartyInternalElection: return TEXT("eleccion_interna");
		case EWLPoliticalActionType::MakeCampaignPromise: return TEXT("promesa");
		case EWLPoliticalActionType::UsePatronage: return TEXT("patronazgo");
		case EWLPoliticalActionType::RunMediaAction: return TEXT("medios");
		case EWLPoliticalActionType::RunRegionPolicy: return TEXT("region");
		case EWLPoliticalActionType::GovernmentCommand: return TEXT("comando_gobierno");
		default: return TEXT("accion");
		}
	}

	EWLGovernmentLogCategory PoliticalActionLogCategory(EWLPoliticalActionType Type)
	{
		switch (Type)
		{
		case EWLPoliticalActionType::ResolveEvent:
			return EWLGovernmentLogCategory::Event;
		case EWLPoliticalActionType::RepressOpposition:
		case EWLPoliticalActionType::RunRegionPolicy:
			return EWLGovernmentLogCategory::Crisis;
		case EWLPoliticalActionType::RunMediaAction:
		case EWLPoliticalActionType::UsePatronage:
		case EWLPoliticalActionType::NegotiatePartySupport:
		case EWLPoliticalActionType::HoldPartyInternalElection:
		case EWLPoliticalActionType::MakeCampaignPromise:
		case EWLPoliticalActionType::SetAgenda:
		case EWLPoliticalActionType::StartProgram:
		case EWLPoliticalActionType::EnactReform:
		default:
			return EWLGovernmentLogCategory::Government;
		}
	}

	FString PoliticalActionLogTitle(EWLPoliticalActionType Type)
	{
		switch (Type)
		{
		case EWLPoliticalActionType::SetAgenda: return TEXT("Agenda de gobierno");
		case EWLPoliticalActionType::StartProgram: return TEXT("Programa ministerial");
		case EWLPoliticalActionType::EnactReform: return TEXT("Reforma aprobada");
		case EWLPoliticalActionType::ResolveEvent: return TEXT("Evento resuelto");
		case EWLPoliticalActionType::RepressOpposition: return TEXT("Represion interna");
		case EWLPoliticalActionType::NegotiatePartySupport: return TEXT("Negociacion parlamentaria");
		case EWLPoliticalActionType::HoldPartyInternalElection: return TEXT("Eleccion interna de partido");
		case EWLPoliticalActionType::MakeCampaignPromise: return TEXT("Promesa electoral");
		case EWLPoliticalActionType::UsePatronage: return TEXT("Patronazgo politico");
		case EWLPoliticalActionType::RunMediaAction: return TEXT("Accion de medios");
		case EWLPoliticalActionType::RunRegionPolicy: return TEXT("Politica regional");
		default: return TEXT("Accion politica");
		}
	}

	FWLMinistryProgramDefinition MakeProgramDefinition(
		const FString& ProgramId,
		const FString& Name,
		EWLMinisterOffice Office,
		int32 DurationMonths,
		int32 PoliticalCapitalCost,
		int64 TreasuryCost,
		bool bRequiresLegislation,
		const FString& Description)
	{
		FWLMinistryProgramDefinition Definition;
		Definition.ProgramId = ProgramId;
		Definition.Name = Name;
		Definition.Office = Office;
		Definition.DurationMonths = DurationMonths;
		Definition.PoliticalCapitalCost = PoliticalCapitalCost;
		Definition.TreasuryCost = TreasuryCost;
		Definition.bRequiresLegislation = bRequiresLegislation;
		Definition.Description = Description;
		return Definition;
	}

	bool ParseMinisterOffice(const FString& Raw, EWLMinisterOffice& OutOffice)
	{
		const FString S = Raw.TrimStartAndEnd().ToLower();
		if (S == TEXT("economy") || S == TEXT("economia")) { OutOffice = EWLMinisterOffice::Economy; return true; }
		if (S == TEXT("defense") || S == TEXT("defensa")) { OutOffice = EWLMinisterOffice::Defense; return true; }
		if (S == TEXT("interior")) { OutOffice = EWLMinisterOffice::Interior; return true; }
		if (S == TEXT("foreign") || S == TEXT("exterior")) { OutOffice = EWLMinisterOffice::Foreign; return true; }
		if (S == TEXT("intelligence") || S == TEXT("inteligencia")) { OutOffice = EWLMinisterOffice::Intelligence; return true; }
		return false;
	}

	bool LoadGovernmentProgramDefinitionsFromJson(TArray<FWLMinistryProgramDefinition>& OutDefinitions)
	{
		const FString FilePath = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Political") / TEXT("GovernmentPrograms.json");
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *FilePath))
		{
			return false;
		}

		TArray<TSharedPtr<FJsonValue>> Array;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Array))
		{
			UE_LOG(LogWorldLeader, Warning, TEXT("WLPoliticalSubsystem: GovernmentPrograms.json invalido; usando catalogo C++."));
			return false;
		}

		TSet<FString> ProgramIds;
		bool bInvalidCatalog = false;
		for (const TSharedPtr<FJsonValue>& Value : Array)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& Obj = *ObjPtr;
			FWLMinistryProgramDefinition Definition;
			FString Office;
			Obj->TryGetStringField(TEXT("program_id"), Definition.ProgramId);
			Obj->TryGetStringField(TEXT("name"), Definition.Name);
			Obj->TryGetStringField(TEXT("office"), Office);
			Obj->TryGetNumberField(TEXT("duration_months"), Definition.DurationMonths);
			Obj->TryGetNumberField(TEXT("political_capital_cost"), Definition.PoliticalCapitalCost);
			double TreasuryCost = 0.0;
			if (Obj->TryGetNumberField(TEXT("treasury_cost"), TreasuryCost))
			{
				Definition.TreasuryCost = static_cast<int64>(TreasuryCost);
			}
			Obj->TryGetBoolField(TEXT("requires_legislation"), Definition.bRequiresLegislation);
			Obj->TryGetStringField(TEXT("description"), Definition.Description);
			Definition.ProgramId = Definition.ProgramId.TrimStartAndEnd().ToLower();
			if (!Definition.ProgramId.IsEmpty()
				&& !Definition.Name.IsEmpty()
				&& ParseMinisterOffice(Office, Definition.Office)
				&& Definition.DurationMonths > 0)
			{
				if (ProgramIds.Contains(Definition.ProgramId))
				{
					UE_LOG(LogWorldLeader, Warning,
						TEXT("WLPoliticalSubsystem: GovernmentPrograms.json duplica program_id '%s'."),
						*Definition.ProgramId);
					bInvalidCatalog = true;
					continue;
				}
				ProgramIds.Add(Definition.ProgramId);
				OutDefinitions.Add(MoveTemp(Definition));
			}
		}

		if (bInvalidCatalog || OutDefinitions.Num() < 50)
		{
			UE_LOG(LogWorldLeader, Warning,
				TEXT("WLPoliticalSubsystem: GovernmentPrograms.json invalido o incompleto (%d/50); usando catalogo C++."),
				OutDefinitions.Num());
			OutDefinitions.Reset();
			return false;
		}
		return true;
	}

	const TArray<FWLMinistryProgramDefinition>& GovernmentProgramDefinitions()
	{
		static const TArray<FWLMinistryProgramDefinition> Definitions = []()
		{
			TArray<FWLMinistryProgramDefinition> Loaded;
			if (LoadGovernmentProgramDefinitionsFromJson(Loaded))
			{
				return Loaded;
			}
			TArray<FWLMinistryProgramDefinition> Items;
			Items.Reserve(50);
			auto Add = [&Items](
				const TCHAR* Id,
				const TCHAR* Name,
				EWLMinisterOffice Office,
				int32 Months,
				int32 PoliticalCost,
				int64 TreasuryCost,
				bool bLegislation,
				const TCHAR* Description)
			{
				Items.Add(MakeProgramDefinition(Id, Name, Office, Months, PoliticalCost, TreasuryCost, bLegislation, Description));
			};

			Add(TEXT("econ_tax_reform"), TEXT("Reforma tributaria"), EWLMinisterOffice::Economy, 3, 14, 0, true,
				TEXT("Ajusta impuestos y mejora recaudacion; tensiona trabajadores y sindicatos."));
			Add(TEXT("econ_public_investment"), TEXT("Inversion publica"), EWLMinisterOffice::Economy, 4, 8, 4500, false,
				TEXT("Gasto en obra publica y empleo; mejora apoyo social y capacidad estatal."));
			Add(TEXT("econ_subsidies"), TEXT("Subsidios focalizados"), EWLMinisterOffice::Economy, 2, 6, 2500, false,
				TEXT("Reduce presion social a costa del tesoro."));
			Add(TEXT("econ_privatization"), TEXT("Privatizacion selectiva"), EWLMinisterOffice::Economy, 4, 12, 0, true,
				TEXT("Vende activos estatales; mejora caja y empresarios, pero golpea sindicatos."));
			Add(TEXT("econ_price_controls"), TEXT("Control de precios"), EWLMinisterOffice::Economy, 3, 9, 1800, true,
				TEXT("Contiene costo de vida a costa de eficiencia y presion empresarial."));
			Add(TEXT("econ_debt_swap"), TEXT("Canje de deuda"), EWLMinisterOffice::Economy, 3, 10, 900, false,
				TEXT("Reordena vencimientos y compra tiempo fiscal."));
			Add(TEXT("econ_industrial_credit"), TEXT("Credito industrial"), EWLMinisterOffice::Economy, 4, 9, 3600, false,
				TEXT("Financia manufactura nacional y empleo urbano."));
			Add(TEXT("econ_anti_corruption_audit"), TEXT("Auditoria anticorrupcion"), EWLMinisterOffice::Economy, 3, 11, 1200, false,
				TEXT("Persigue filtraciones de presupuesto; incomoda redes clientelares."));
			Add(TEXT("econ_food_stabilization"), TEXT("Estabilizacion alimentaria"), EWLMinisterOffice::Economy, 2, 7, 2200, false,
				TEXT("Compra alimentos y baja presion de trabajadores."));
			Add(TEXT("econ_emergency_budget"), TEXT("Presupuesto de emergencia"), EWLMinisterOffice::Economy, 2, 15, 0, true,
				TEXT("Reasigna gasto para sobrevivir una crisis fiscal."));

			Add(TEXT("def_doctrine"), TEXT("Doctrina de defensa"), EWLMinisterOffice::Defense, 3, 8, 1800, false,
				TEXT("Ordena mandos, sube apoyo militar y baja rivalidad del alto mando."));
			Add(TEXT("def_procurement"), TEXT("Compras militares"), EWLMinisterOffice::Defense, 3, 10, 4200, false,
				TEXT("Procura equipo y prepara reclutamiento."));
			Add(TEXT("def_mobilization"), TEXT("Movilizacion nacional"), EWLMinisterOffice::Defense, 2, 16, 2000, true,
				TEXT("Moviliza reservas; sube poder militar pero tensiona sociedad."));
			Add(TEXT("def_border_command"), TEXT("Comando de frontera"), EWLMinisterOffice::Defense, 3, 9, 2400, false,
				TEXT("Refuerza provincias fronterizas y reduce crisis militares."));
			Add(TEXT("def_service_law"), TEXT("Ley de servicio militar"), EWLMinisterOffice::Defense, 4, 15, 1400, true,
				TEXT("Amplia la base de reclutamiento y tensiona juventud/trabajadores."));
			Add(TEXT("def_military_industry"), TEXT("Industria militar"), EWLMinisterOffice::Defense, 5, 13, 5200, false,
				TEXT("Crea proveedores nacionales y sube apoyo castrense."));
			Add(TEXT("def_veterans_package"), TEXT("Paquete de veteranos"), EWLMinisterOffice::Defense, 2, 6, 1600, false,
				TEXT("Compra lealtad militar y legitimidad patriotica."));
			Add(TEXT("def_counterinsurgency"), TEXT("Plan contrainsurgente"), EWLMinisterOffice::Defense, 3, 12, 2200, false,
				TEXT("Reduce rebelion regional con coste de derechos civiles."));
			Add(TEXT("def_joint_training"), TEXT("Entrenamiento conjunto"), EWLMinisterOffice::Defense, 3, 8, 1800, false,
				TEXT("Mejora coordinacion de mandos sin escalar guerra."));
			Add(TEXT("def_officer_rotation"), TEXT("Rotacion de oficiales"), EWLMinisterOffice::Defense, 2, 9, 900, false,
				TEXT("Reduce camarillas militares y riesgo de golpe."));

			Add(TEXT("int_police_reform"), TEXT("Reforma policial"), EWLMinisterOffice::Interior, 3, 10, 1800, true,
				TEXT("Mejora orden interno con menor coste politico futuro."));
			Add(TEXT("int_governors"), TEXT("Pacto con gobernadores"), EWLMinisterOffice::Interior, 3, 8, 1200, false,
				TEXT("Aumenta autoridad central y apoyo regional."));
			Add(TEXT("int_public_order"), TEXT("Operacion de orden publico"), EWLMinisterOffice::Interior, 2, 7, 1500, false,
				TEXT("Contiene protestas sin llegar a golpe abierto."));
			Add(TEXT("int_decentralization_compact"), TEXT("Pacto de descentralizacion"), EWLMinisterOffice::Interior, 4, 12, 1700, true,
				TEXT("Negocia autonomia y baja secesion a cambio de control central menor."));
			Add(TEXT("int_civil_service"), TEXT("Servicio civil profesional"), EWLMinisterOffice::Interior, 5, 14, 2600, true,
				TEXT("Aumenta burocracia y reduce patronazgo a largo plazo."));
			Add(TEXT("int_mayor_network"), TEXT("Red de alcaldes"), EWLMinisterOffice::Interior, 3, 8, 1300, false,
				TEXT("Construye maquinaria local para gobernabilidad."));
			Add(TEXT("int_disaster_response"), TEXT("Respuesta a emergencias"), EWLMinisterOffice::Interior, 2, 5, 1900, false,
				TEXT("Mejora legitimidad ante crisis territoriales."));
			Add(TEXT("int_prison_reform"), TEXT("Reforma penitenciaria"), EWLMinisterOffice::Interior, 4, 10, 2100, true,
				TEXT("Reduce violencia criminal y choques de derechos humanos."));
			Add(TEXT("int_local_security"), TEXT("Seguridad local"), EWLMinisterOffice::Interior, 3, 8, 1700, false,
				TEXT("Invierte en policia municipal y control territorial."));
			Add(TEXT("int_social_dialogue"), TEXT("Mesa social"), EWLMinisterOffice::Interior, 2, 7, 900, false,
				TEXT("Abre negociacion con sindicatos y regiones."));

			Add(TEXT("for_bloc"), TEXT("Construir bloque diplomatico"), EWLMinisterOffice::Foreign, 4, 10, 900, false,
				TEXT("Busca acuerdos con paises afines y sube opinion exterior."));
			Add(TEXT("for_trade_drive"), TEXT("Gira de acuerdos comerciales"), EWLMinisterOffice::Foreign, 3, 8, 1200, false,
				TEXT("Prioriza tratados comerciales con socios receptivos."));
			Add(TEXT("for_sanctions"), TEXT("Paquete de sanciones"), EWLMinisterOffice::Foreign, 2, 9, 600, true,
				TEXT("Embarga al rival mas hostil y endurece relaciones."));
			Add(TEXT("for_mediation"), TEXT("Mediacion regional"), EWLMinisterOffice::Foreign, 3, 8, 800, false,
				TEXT("Baja tension continental y gana legitimidad exterior."));
			Add(TEXT("for_multilateral_orgs"), TEXT("Ofensiva en organismos"), EWLMinisterOffice::Foreign, 4, 10, 1100, false,
				TEXT("Activa foros regionales para respaldo diplomatico."));
			Add(TEXT("for_border_commission"), TEXT("Comision fronteriza"), EWLMinisterOffice::Foreign, 3, 8, 700, false,
				TEXT("Reduce crisis fronteriza y prepara paz."));
			Add(TEXT("for_aid_request"), TEXT("Solicitud de ayuda exterior"), EWLMinisterOffice::Foreign, 2, 7, 400, false,
				TEXT("Busca asistencia fiscal con costo de soberania."));
			Add(TEXT("for_energy_diplomacy"), TEXT("Diplomacia energetica"), EWLMinisterOffice::Foreign, 4, 9, 900, false,
				TEXT("Negocia energia, sanciones y dependencia de socios."));
			Add(TEXT("for_defense_pact"), TEXT("Pacto de defensa"), EWLMinisterOffice::Foreign, 4, 13, 1200, true,
				TEXT("Formaliza alianzas militares y alarma rivales."));
			Add(TEXT("for_human_rights_track"), TEXT("Ruta de derechos humanos"), EWLMinisterOffice::Foreign, 3, 8, 700, false,
				TEXT("Repara imagen internacional tras represion."));

			Add(TEXT("spy_networks"), TEXT("Expansion de redes"), EWLMinisterOffice::Intelligence, 3, 8, 1000, false,
				TEXT("Fortalece inteligencia exterior contra rivales."));
			Add(TEXT("spy_counterintel"), TEXT("Barrido de contraespionaje"), EWLMinisterOffice::Intelligence, 2, 7, 900, false,
				TEXT("Reduce exposicion propia y redes rivales."));
			Add(TEXT("spy_covert_ops"), TEXT("Operaciones encubiertas"), EWLMinisterOffice::Intelligence, 3, 12, 1500, false,
				TEXT("Prepara sabotaje y propaganda contra objetivos hostiles."));
			Add(TEXT("spy_infiltrate_parties"), TEXT("Infiltrar partidos"), EWLMinisterOffice::Intelligence, 4, 11, 1200, false,
				TEXT("Penetra oposicion y coaliciones aliadas."));
			Add(TEXT("spy_media_monitoring"), TEXT("Monitoreo mediatico"), EWLMinisterOffice::Intelligence, 3, 8, 900, false,
				TEXT("Detecta campanias de prensa y fake news."));
			Add(TEXT("spy_purge_cells"), TEXT("Desarticular celulas"), EWLMinisterOffice::Intelligence, 3, 10, 1300, false,
				TEXT("Reduce riesgo de golpe blando y protesta organizada."));
			Add(TEXT("spy_black_budget"), TEXT("Presupuesto negro"), EWLMinisterOffice::Intelligence, 3, 13, 2400, true,
				TEXT("Financia operaciones sensibles con alto riesgo de escandalo."));
			Add(TEXT("spy_border_assets"), TEXT("Activos fronterizos"), EWLMinisterOffice::Intelligence, 3, 9, 1000, false,
				TEXT("Anticipa crisis y movimientos hostiles en frontera."));
			Add(TEXT("spy_anti_corruption_files"), TEXT("Expedientes anticorrupcion"), EWLMinisterOffice::Intelligence, 4, 12, 1100, false,
				TEXT("Reune pruebas contra redes corruptas y ministros rivales."));
			Add(TEXT("spy_false_flag_watch"), TEXT("Alerta de bandera falsa"), EWLMinisterOffice::Intelligence, 3, 10, 1000, false,
				TEXT("Reduce provocaciones y atribucion incierta."));

			return Items;
		}();
		return Definitions;
	}

	FWLPolicyReformDefinition MakePolicyReformDefinition(
		const FString& ReformId,
		const FString& Name,
		EWLPolicyReformArea Area,
		const TArray<FString>& Prerequisites,
		int32 RequiredCoalition,
		int32 RequiredCapacity,
		int32 PoliticalCost,
		int64 TreasuryCost,
		int32 ProtestRisk,
		int32 OppositionDelta,
		int32 PublicOrderDelta,
		int32 LongTermMonths,
		int64 MonthlyTreasuryDelta,
		int32 CapacityDelta,
		int32 CorruptionDelta,
		int32 LegitimacyDelta,
		const TArray<EWLPublicGroup>& AffectedGroups,
		int32 GroupSupportDelta,
		const FString& Description)
	{
		FWLPolicyReformDefinition Definition;
		Definition.ReformId = ReformId;
		Definition.Name = Name;
		Definition.Area = Area;
		Definition.PrerequisiteReformIds = Prerequisites;
		Definition.RequiredCoalitionSupport = RequiredCoalition;
		Definition.RequiredStateCapacity = RequiredCapacity;
		Definition.PoliticalCapitalCost = PoliticalCost;
		Definition.TreasuryCost = TreasuryCost;
		Definition.ProtestRisk = ProtestRisk;
		Definition.OppositionDelta = OppositionDelta;
		Definition.PublicOrderDelta = PublicOrderDelta;
		Definition.LongTermMonths = LongTermMonths;
		Definition.MonthlyTreasuryDelta = MonthlyTreasuryDelta;
		Definition.CapacityDelta = CapacityDelta;
		Definition.CorruptionDelta = CorruptionDelta;
		Definition.LegitimacyDelta = LegitimacyDelta;
		Definition.AffectedGroups = AffectedGroups;
		Definition.PublicGroupSupportDelta = GroupSupportDelta;
		Definition.Description = Description;
		return Definition;
	}

	bool ParsePolicyReformArea(const FString& Raw, EWLPolicyReformArea& OutArea)
	{
		const FString S = Raw.TrimStartAndEnd().ToLower();
		if (S == TEXT("tax") || S == TEXT("tributaria")) { OutArea = EWLPolicyReformArea::Tax; return true; }
		if (S == TEXT("labor") || S == TEXT("laboral")) { OutArea = EWLPolicyReformArea::Labor; return true; }
		if (S == TEXT("security") || S == TEXT("seguridad")) { OutArea = EWLPolicyReformArea::Security; return true; }
		if (S == TEXT("education") || S == TEXT("educacion")) { OutArea = EWLPolicyReformArea::Education; return true; }
		if (S == TEXT("health") || S == TEXT("salud")) { OutArea = EWLPolicyReformArea::Health; return true; }
		if (S == TEXT("decentralization") || S == TEXT("descentralizacion")) { OutArea = EWLPolicyReformArea::Decentralization; return true; }
		if (S == TEXT("military") || S == TEXT("militar")) { OutArea = EWLPolicyReformArea::Military; return true; }
		if (S == TEXT("justice") || S == TEXT("justicia")) { OutArea = EWLPolicyReformArea::Justice; return true; }
		if (S == TEXT("media") || S == TEXT("medios")) { OutArea = EWLPolicyReformArea::Media; return true; }
		if (S == TEXT("energy") || S == TEXT("energia")) { OutArea = EWLPolicyReformArea::Energy; return true; }
		if (S == TEXT("trade") || S == TEXT("comercio")) { OutArea = EWLPolicyReformArea::Trade; return true; }
		if (S == TEXT("constitution") || S == TEXT("constitucion")) { OutArea = EWLPolicyReformArea::Constitution; return true; }
		return false;
	}

	bool ParsePublicGroup(const FString& Raw, EWLPublicGroup& OutGroup)
	{
		const FString S = Raw.TrimStartAndEnd().ToLower();
		if (S == TEXT("business") || S == TEXT("empresarios")) { OutGroup = EWLPublicGroup::Business; return true; }
		if (S == TEXT("military") || S == TEXT("militares")) { OutGroup = EWLPublicGroup::Military; return true; }
		if (S == TEXT("workers") || S == TEXT("trabajadores")) { OutGroup = EWLPublicGroup::Workers; return true; }
		if (S == TEXT("regions") || S == TEXT("regiones")) { OutGroup = EWLPublicGroup::Regions; return true; }
		if (S == TEXT("middleclass") || S == TEXT("middle_class") || S == TEXT("clase_media")) { OutGroup = EWLPublicGroup::MiddleClass; return true; }
		if (S == TEXT("unions") || S == TEXT("sindicatos")) { OutGroup = EWLPublicGroup::Unions; return true; }
		return false;
	}

	TArray<FString> ReadStringArrayField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Obj->TryGetArrayField(FieldName, Array) || !Array)
		{
			return Out;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			FString Item;
			if (Value.IsValid() && Value->TryGetString(Item))
			{
				Item = Item.TrimStartAndEnd().ToLower();
				if (!Item.IsEmpty())
				{
					Out.Add(Item);
				}
			}
		}
		return Out;
	}

	bool LoadPolicyReformDefinitionsFromJson(TArray<FWLPolicyReformDefinition>& OutDefinitions)
	{
		const FString FilePath = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Political") / TEXT("PolicyReforms.json");
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *FilePath))
		{
			return false;
		}

		TArray<TSharedPtr<FJsonValue>> Array;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Array))
		{
			UE_LOG(LogWorldLeader, Warning, TEXT("WLPoliticalSubsystem: PolicyReforms.json invalido; usando catalogo C++."));
			return false;
		}

		TSet<FString> ReformIds;
		bool bInvalidCatalog = false;
		for (const TSharedPtr<FJsonValue>& Value : Array)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& Obj = *ObjPtr;
			FWLPolicyReformDefinition Definition;
			FString Area;
			Obj->TryGetStringField(TEXT("reform_id"), Definition.ReformId);
			Obj->TryGetStringField(TEXT("name"), Definition.Name);
			Obj->TryGetStringField(TEXT("area"), Area);
			Definition.PrerequisiteReformIds = ReadStringArrayField(Obj, TEXT("prerequisites"));
			Obj->TryGetNumberField(TEXT("required_coalition_support"), Definition.RequiredCoalitionSupport);
			Obj->TryGetNumberField(TEXT("required_state_capacity"), Definition.RequiredStateCapacity);
			Obj->TryGetNumberField(TEXT("political_capital_cost"), Definition.PoliticalCapitalCost);
			double TreasuryCost = 0.0;
			if (Obj->TryGetNumberField(TEXT("treasury_cost"), TreasuryCost))
			{
				Definition.TreasuryCost = static_cast<int64>(TreasuryCost);
			}
			Obj->TryGetNumberField(TEXT("protest_risk"), Definition.ProtestRisk);
			Obj->TryGetNumberField(TEXT("opposition_delta"), Definition.OppositionDelta);
			Obj->TryGetNumberField(TEXT("public_order_delta"), Definition.PublicOrderDelta);
			Obj->TryGetNumberField(TEXT("long_term_months"), Definition.LongTermMonths);
			double MonthlyTreasuryDelta = 0.0;
			if (Obj->TryGetNumberField(TEXT("monthly_treasury_delta"), MonthlyTreasuryDelta))
			{
				Definition.MonthlyTreasuryDelta = static_cast<int64>(MonthlyTreasuryDelta);
			}
			Obj->TryGetNumberField(TEXT("capacity_delta"), Definition.CapacityDelta);
			Obj->TryGetNumberField(TEXT("corruption_delta"), Definition.CorruptionDelta);
			Obj->TryGetNumberField(TEXT("legitimacy_delta"), Definition.LegitimacyDelta);
			for (const FString& GroupName : ReadStringArrayField(Obj, TEXT("affected_groups")))
			{
				EWLPublicGroup Group = EWLPublicGroup::MiddleClass;
				if (ParsePublicGroup(GroupName, Group))
				{
					Definition.AffectedGroups.Add(Group);
				}
			}
			Obj->TryGetNumberField(TEXT("public_group_support_delta"), Definition.PublicGroupSupportDelta);
			Obj->TryGetStringField(TEXT("description"), Definition.Description);
			Definition.ReformId = Definition.ReformId.TrimStartAndEnd().ToLower();
			if (!Definition.ReformId.IsEmpty()
				&& !Definition.Name.IsEmpty()
				&& ParsePolicyReformArea(Area, Definition.Area)
				&& Definition.LongTermMonths >= 0)
			{
				if (ReformIds.Contains(Definition.ReformId))
				{
					UE_LOG(LogWorldLeader, Warning,
						TEXT("WLPoliticalSubsystem: PolicyReforms.json duplica reform_id '%s'."),
						*Definition.ReformId);
					bInvalidCatalog = true;
					continue;
				}
				ReformIds.Add(Definition.ReformId);
				OutDefinitions.Add(MoveTemp(Definition));
			}
		}

		for (const FWLPolicyReformDefinition& Definition : OutDefinitions)
		{
			for (const FString& Prerequisite : Definition.PrerequisiteReformIds)
			{
				if (Prerequisite == Definition.ReformId || !ReformIds.Contains(Prerequisite))
				{
					UE_LOG(LogWorldLeader, Warning,
						TEXT("WLPoliticalSubsystem: PolicyReforms.json prerequisito invalido '%s' en '%s'."),
						*Prerequisite,
						*Definition.ReformId);
					bInvalidCatalog = true;
				}
			}
		}

		if (bInvalidCatalog || OutDefinitions.Num() < 24)
		{
			UE_LOG(LogWorldLeader, Warning,
				TEXT("WLPoliticalSubsystem: PolicyReforms.json invalido o incompleto (%d/24); usando catalogo C++."),
				OutDefinitions.Num());
			OutDefinitions.Reset();
			return false;
		}
		return true;
	}

	const TArray<FWLPolicyReformDefinition>& PolicyReformDefinitions()
	{
		static const TArray<FWLPolicyReformDefinition> Definitions = []()
		{
			TArray<FWLPolicyReformDefinition> Loaded;
			if (LoadPolicyReformDefinitionsFromJson(Loaded))
			{
				return Loaded;
			}
			TArray<FWLPolicyReformDefinition> Items;
			Items.Reserve(24);
			auto Add = [&Items](
				const TCHAR* Id,
				const TCHAR* Name,
				EWLPolicyReformArea Area,
				TArray<FString> Prerequisites,
				int32 Coalition,
				int32 Capacity,
				int32 PoliticalCost,
				int64 TreasuryCost,
				int32 ProtestRisk,
				int32 OppositionDelta,
				int32 PublicOrderDelta,
				int32 LongMonths,
				int64 MonthlyTreasury,
				int32 CapacityDelta,
				int32 CorruptionDelta,
				int32 LegitimacyDelta,
				TArray<EWLPublicGroup> Groups,
				int32 GroupDelta,
				const TCHAR* Description)
			{
				Items.Add(MakePolicyReformDefinition(
					Id, Name, Area, Prerequisites, Coalition, Capacity, PoliticalCost, TreasuryCost,
					ProtestRisk, OppositionDelta, PublicOrderDelta, LongMonths, MonthlyTreasury,
					CapacityDelta, CorruptionDelta, LegitimacyDelta, Groups, GroupDelta, Description));
			};

			Add(TEXT("tax_broad_base"), TEXT("Base tributaria amplia"), EWLPolicyReformArea::Tax, {}, 52, 45, 18, 800, 35, 8, -1, 24, 850, 2, -1, 2,
				{ EWLPublicGroup::Business, EWLPublicGroup::MiddleClass, EWLPublicGroup::Workers }, -1,
				TEXT("Amplia contribuyentes y formaliza recaudo; sube ingresos con resistencia social."));
			Add(TEXT("tax_progressive_code"), TEXT("Codigo progresivo"), EWLPolicyReformArea::Tax, { TEXT("tax_broad_base") }, 56, 50, 20, 1000, 42, 10, -1, 30, 650, 2, -2, 3,
				{ EWLPublicGroup::Business, EWLPublicGroup::MiddleClass, EWLPublicGroup::Unions }, 1,
				TEXT("Reordena impuestos por capacidad de pago; mejora legitimidad si se implementa bien."));
			Add(TEXT("labor_collective_bargaining"), TEXT("Negociacion colectiva"), EWLPolicyReformArea::Labor, {}, 50, 42, 16, 900, 32, 4, 1, 18, -180, 1, 0, 3,
				{ EWLPublicGroup::Workers, EWLPublicGroup::Unions, EWLPublicGroup::Business }, 2,
				TEXT("Reconoce sindicatos y reduce huelgas si hay cumplimiento."));
			Add(TEXT("labor_flexibility_package"), TEXT("Flexibilidad laboral"), EWLPolicyReformArea::Labor, {}, 50, 45, 16, 600, 38, 8, -1, 18, 420, 0, 1, -1,
				{ EWLPublicGroup::Business, EWLPublicGroup::Workers, EWLPublicGroup::Unions }, -1,
				TEXT("Facilita contratacion privada, pero tensiona sindicatos."));
			Add(TEXT("security_citizen_plan"), TEXT("Plan de seguridad ciudadana"), EWLPolicyReformArea::Security, {}, 50, 40, 14, 1800, 24, -4, 2, 18, -220, 2, 0, 2,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Military, EWLPublicGroup::Regions }, 2,
				TEXT("Invierte en seguridad cotidiana y coordinacion policial."));
			Add(TEXT("security_state_exception_law"), TEXT("Ley de estado de excepcion"), EWLPolicyReformArea::Security, {}, 58, 45, 22, 600, 55, 12, -3, 12, 0, -1, 2, -5,
				{ EWLPublicGroup::Military, EWLPublicGroup::Workers, EWLPublicGroup::Unions }, -2,
				TEXT("Da poderes extraordinarios; baja amenaza inmediata y puede erosionar legitimidad."));
			Add(TEXT("edu_public_schools"), TEXT("Escuelas publicas"), EWLPolicyReformArea::Education, {}, 48, 42, 14, 2400, 18, -2, 1, 36, -260, 2, -1, 4,
				{ EWLPublicGroup::Workers, EWLPublicGroup::MiddleClass, EWLPublicGroup::Regions }, 2,
				TEXT("Expande educacion y capital humano a largo plazo."));
			Add(TEXT("edu_university_autonomy"), TEXT("Autonomia universitaria"), EWLPolicyReformArea::Education, { TEXT("edu_public_schools") }, 52, 46, 16, 1600, 28, -1, 1, 24, -120, 1, 0, 3,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Unions }, 2,
				TEXT("Reduce protesta estudiantil si no se acompana de censura."));
			Add(TEXT("health_primary_care"), TEXT("Atencion primaria"), EWLPolicyReformArea::Health, {}, 48, 42, 14, 2600, 16, -3, 1, 30, -300, 2, -1, 4,
				{ EWLPublicGroup::Workers, EWLPublicGroup::MiddleClass, EWLPublicGroup::Regions }, 2,
				TEXT("Mejora legitimidad social con gasto recurrente."));
			Add(TEXT("health_insurance_reform"), TEXT("Seguro nacional"), EWLPolicyReformArea::Health, { TEXT("health_primary_care") }, 58, 52, 22, 4200, 32, -4, 1, 36, -360, 3, -1, 5,
				{ EWLPublicGroup::Workers, EWLPublicGroup::Unions, EWLPublicGroup::MiddleClass }, 2,
				TEXT("Crea cobertura nacional y exige alta capacidad estatal."));
			Add(TEXT("decentralization_fiscal_transfer"), TEXT("Transferencias regionales"), EWLPolicyReformArea::Decentralization, {}, 52, 42, 16, 1800, 26, -2, 1, 24, -220, 1, 1, 2,
				{ EWLPublicGroup::Regions, EWLPublicGroup::MiddleClass }, 3,
				TEXT("Compra obediencia territorial a costa de presupuesto."));
			Add(TEXT("decentralization_elected_governors"), TEXT("Gobernadores electos"), EWLPolicyReformArea::Decentralization, { TEXT("decentralization_fiscal_transfer") }, 60, 50, 24, 900, 38, 4, 0, 36, -80, -1, 0, 4,
				{ EWLPublicGroup::Regions, EWLPublicGroup::Business }, 2,
				TEXT("Legitima regiones, pero reduce control directo del centro."));
			Add(TEXT("military_professionalization"), TEXT("Profesionalizacion militar"), EWLPolicyReformArea::Military, {}, 50, 45, 16, 2200, 20, -3, 1, 24, -200, 1, -1, 1,
				{ EWLPublicGroup::Military, EWLPublicGroup::MiddleClass }, 2,
				TEXT("Sube disciplina y baja camarillas golpistas."));
			Add(TEXT("military_procurement_law"), TEXT("Ley de compras militares"), EWLPolicyReformArea::Military, { TEXT("military_professionalization") }, 56, 48, 18, 1600, 30, 2, 0, 24, -100, 1, -3, 2,
				{ EWLPublicGroup::Military, EWLPublicGroup::Business }, 1,
				TEXT("Audita compras y reduce corrupcion castrense."));
			Add(TEXT("justice_anti_corruption_court"), TEXT("Corte anticorrupcion"), EWLPolicyReformArea::Justice, {}, 56, 50, 20, 1800, 34, 3, 0, 30, -120, 3, -4, 4,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Business }, 2,
				TEXT("Institucionaliza persecucion de corrupcion y golpea redes viejas."));
			Add(TEXT("justice_judicial_independence"), TEXT("Independencia judicial"), EWLPolicyReformArea::Justice, { TEXT("justice_anti_corruption_court") }, 60, 55, 24, 1200, 36, 4, 0, 36, -80, 2, -2, 5,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Business, EWLPublicGroup::Unions }, 1,
				TEXT("Mejora legitimidad y limita arbitrariedad del ejecutivo."));
			Add(TEXT("media_public_broadcasting"), TEXT("Medios publicos autonomos"), EWLPolicyReformArea::Media, {}, 50, 42, 14, 1000, 24, -2, 1, 18, -90, 1, -1, 3,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Workers }, 2,
				TEXT("Construye comunicacion estatal creible sin censura dura."));
			Add(TEXT("media_information_security"), TEXT("Seguridad informativa"), EWLPolicyReformArea::Media, {}, 54, 45, 18, 900, 36, 2, 0, 18, -60, 1, 1, -1,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Military }, 1,
				TEXT("Combate fake news, con riesgo de abuso censor."));
			Add(TEXT("energy_sovereignty"), TEXT("Soberania energetica"), EWLPolicyReformArea::Energy, {}, 52, 46, 18, 3200, 26, 1, 0, 36, 500, 1, 1, 1,
				{ EWLPublicGroup::Business, EWLPublicGroup::Workers, EWLPublicGroup::Regions }, 1,
				TEXT("Invierte en energia y reduce dependencia exterior a largo plazo."));
			Add(TEXT("energy_green_transition"), TEXT("Transicion energetica"), EWLPolicyReformArea::Energy, { TEXT("energy_sovereignty") }, 56, 52, 20, 3800, 34, 3, 0, 36, 260, 2, -1, 3,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Workers, EWLPublicGroup::Business }, 1,
				TEXT("Diversifica energia y abre choques con sectores tradicionales."));
			Add(TEXT("trade_customs_modernization"), TEXT("Modernizacion aduanera"), EWLPolicyReformArea::Trade, {}, 48, 44, 14, 1500, 18, -1, 0, 24, 300, 2, -2, 2,
				{ EWLPublicGroup::Business, EWLPublicGroup::MiddleClass }, 2,
				TEXT("Reduce contrabando y mejora comercio exterior."));
			Add(TEXT("trade_export_promotion"), TEXT("Promocion exportadora"), EWLPolicyReformArea::Trade, { TEXT("trade_customs_modernization") }, 52, 48, 16, 2200, 24, 0, 0, 30, 420, 1, 0, 2,
				{ EWLPublicGroup::Business, EWLPublicGroup::Workers }, 2,
				TEXT("Expande ventas externas y dependencia de rutas abiertas."));
			Add(TEXT("constitution_term_rules"), TEXT("Reglas de reeleccion"), EWLPolicyReformArea::Constitution, {}, 64, 55, 28, 1200, 58, 12, -1, 48, 0, 1, 0, -6,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Military, EWLPublicGroup::Unions }, -1,
				TEXT("Cambia reglas de mandato; puede abrir crisis constitucional."));
			Add(TEXT("constitution_power_balance"), TEXT("Balance de poderes"), EWLPolicyReformArea::Constitution, { TEXT("justice_judicial_independence") }, 68, 60, 30, 1500, 44, 6, 0, 48, 0, 3, -2, 8,
				{ EWLPublicGroup::MiddleClass, EWLPublicGroup::Business, EWLPublicGroup::Regions }, 2,
				TEXT("Reordena ejecutivo, Congreso y justicia para estabilidad institucional."));

			return Items;
		}();
		return Definitions;
	}

	TArray<EWLGovernmentPriority> DefaultGovernmentAgenda()
	{
		return {
			EWLGovernmentPriority::Security,
			EWLGovernmentPriority::Growth,
			EWLGovernmentPriority::Diplomacy
		};
	}

	void AddDefaultEventDefinitions(TArray<FWLPoliticalEventDefinition>& OutDefinitions)
	{
		FWLPoliticalEventDefinition CoupTension;
		CoupTension.EventId = TEXT("internal_coup_tension");
		CoupTension.Title = TEXT("Rumores de golpe");
		CoupTension.Body = TEXT("Altos mandos desleales y oposicion organizada presionan al gobierno.");
		CoupTension.Trigger = TEXT("coup_risk");
		CoupTension.MinCoupRisk = 60;
		CoupTension.MinOppositionStrength = 25;
		CoupTension.MaxPublicOrder = 100;

		FWLPoliticalEventOption Negotiate;
		Negotiate.OptionId = TEXT("negotiate");
		Negotiate.Label = TEXT("Negociar con los mandos");
		Negotiate.PoliticalCapitalDelta = -8;
		Negotiate.OppositionDelta = -6;
		Negotiate.PublicOrderDelta = 1;
		CoupTension.Options.Add(Negotiate);

		FWLPoliticalEventOption Crackdown;
		Crackdown.OptionId = TEXT("crackdown");
		Crackdown.Label = TEXT("Ordenar una redada preventiva");
		Crackdown.OppositionDelta = -12;
		Crackdown.PublicOrderDelta = -3;
		CoupTension.Options.Add(Crackdown);
		OutDefinitions.Add(MoveTemp(CoupTension));

		FWLPoliticalEventDefinition Protest;
		Protest.EventId = TEXT("street_protests");
		Protest.Title = TEXT("Protestas nacionales");
		Protest.Body = TEXT("La oposicion capitaliza el deterioro del orden publico.");
		Protest.Trigger = TEXT("opposition");
		Protest.MinCoupRisk = 0;
		Protest.MinOppositionStrength = 35;
		Protest.MaxPublicOrder = 55;

		FWLPoliticalEventOption Concede;
		Concede.OptionId = TEXT("concede");
		Concede.Label = TEXT("Conceder reformas limitadas");
		Concede.PoliticalCapitalDelta = -6;
		Concede.OppositionDelta = -10;
		Concede.PublicOrderDelta = 2;
		Protest.Options.Add(Concede);

		FWLPoliticalEventOption Ignore;
		Ignore.OptionId = TEXT("ignore");
		Ignore.Label = TEXT("Ignorar las marchas");
		Ignore.OppositionDelta = 7;
		Ignore.PublicOrderDelta = -2;
		Protest.Options.Add(Ignore);
		OutDefinitions.Add(MoveTemp(Protest));
	}
}
