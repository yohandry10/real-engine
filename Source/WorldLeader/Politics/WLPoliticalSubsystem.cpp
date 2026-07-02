// Copyright World Leader project. See ROADMAP.md.

#include "Politics/WLPoliticalSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "WorldLeader.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr int32 CoupAttemptRiskThreshold = 85;
	constexpr int64 RewardGeneralCost = 3000;
	constexpr int64 RepressOppositionCost = 2000;
	constexpr int32 MaxAgendaPriorities = 3;
	constexpr int32 ElectionCycleMonths = 48;

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

void UWLPoliticalSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UWLDataRegistry::StaticClass());
	Collection.InitializeDependency(UWLStrategicTickSubsystem::StaticClass());
	Collection.InitializeDependency(UWLCharacterSubsystem::StaticClass());
	ResetPoliticalState();
}

UWLDataRegistry* UWLPoliticalSubsystem::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLStrategicTickSubsystem* UWLPoliticalSubsystem::GetTick() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}

UWLCharacterSubsystem* UWLPoliticalSubsystem::GetCharacters() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLCharacterSubsystem>() : nullptr;
}

FString UWLPoliticalSubsystem::NormalizeIso(const FString& In)
{
	return In.TrimStartAndEnd().ToUpper();
}

FString UWLPoliticalSubsystem::NormalizeCharacterId(const FString& In)
{
	return In.TrimStartAndEnd().ToUpper();
}

int32 UWLPoliticalSubsystem::ClampPercent(int32 Value)
{
	return FMath::Clamp(Value, 0, 100);
}

FString UWLPoliticalSubsystem::RelationKey(const FString& NationA, const FString& NationB)
{
	const FString A = NormalizeIso(NationA);
	const FString B = NormalizeIso(NationB);
	return A < B ? A + TEXT("|") + B : B + TEXT("|") + A;
}

FString UWLPoliticalSubsystem::NetworkKey(const FString& OwnerIso, const FString& TargetIso)
{
	return NormalizeIso(OwnerIso) + TEXT(">") + NormalizeIso(TargetIso);
}

FString UWLPoliticalSubsystem::PublicGroupKey(const FString& NationIso, EWLPublicGroup Group)
{
	return NormalizeIso(NationIso) + TEXT("|") + FString::FromInt(static_cast<int32>(Group));
}

FString UWLPoliticalSubsystem::PoliticalMemoryKey(const FString& NationIso, const FString& MemoryKey)
{
	return NormalizeIso(NationIso) + TEXT("|") + MemoryKey.TrimStartAndEnd().ToLower();
}

FString UWLPoliticalSubsystem::TreatyToString(EWLTreatyType Treaty)
{
	switch (Treaty)
	{
	case EWLTreatyType::TradeAgreement: return TEXT("acuerdo comercial");
	case EWLTreatyType::NonAggression:  return TEXT("pacto de no agresion");
	case EWLTreatyType::Alliance:       return TEXT("alianza");
	case EWLTreatyType::Embargo:        return TEXT("embargo");
	default:                            return TEXT("tratado");
	}
}

FString UWLPoliticalSubsystem::OperationToString(EWLSpyOperationType Operation)
{
	switch (Operation)
	{
	case EWLSpyOperationType::SabotageEconomy:     return TEXT("sabotaje economico");
	case EWLSpyOperationType::SabotageArmy:        return TEXT("sabotaje militar");
	case EWLSpyOperationType::FundCoup:            return TEXT("financiar golpe");
	case EWLSpyOperationType::Propaganda:          return TEXT("propaganda");
	case EWLSpyOperationType::CounterIntelligence: return TEXT("contraespionaje");
	default:                                       return TEXT("operacion");
	}
}

FString UWLPoliticalSubsystem::GovernmentPriorityToString(EWLGovernmentPriority Priority)
{
	switch (Priority)
	{
	case EWLGovernmentPriority::Security:          return TEXT("seguridad");
	case EWLGovernmentPriority::Growth:            return TEXT("crecimiento");
	case EWLGovernmentPriority::Austerity:         return TEXT("austeridad");
	case EWLGovernmentPriority::Industrialization: return TEXT("industrializacion");
	case EWLGovernmentPriority::Diplomacy:         return TEXT("diplomacia");
	case EWLGovernmentPriority::Control:           return TEXT("control interno");
	default:                                       return TEXT("prioridad");
	}
}

FString UWLPoliticalSubsystem::PublicGroupToString(EWLPublicGroup Group)
{
	switch (Group)
	{
	case EWLPublicGroup::Business:   return TEXT("empresarios");
	case EWLPublicGroup::Military:   return TEXT("militares");
	case EWLPublicGroup::Workers:    return TEXT("trabajadores");
	case EWLPublicGroup::Regions:    return TEXT("regiones");
	case EWLPublicGroup::MiddleClass:return TEXT("clase media");
	case EWLPublicGroup::Unions:     return TEXT("sindicatos");
	default:                         return TEXT("grupo social");
	}
}

FString UWLPoliticalSubsystem::GovernmentAIObjectiveToString(EWLGovernmentAIObjective Objective)
{
	switch (Objective)
	{
	case EWLGovernmentAIObjective::Stabilize:     return TEXT("estabilizar");
	case EWLGovernmentAIObjective::Expand:        return TEXT("expandirse");
	case EWLGovernmentAIObjective::Borrow:        return TEXT("financiarse");
	case EWLGovernmentAIObjective::Militarize:    return TEXT("militarizar");
	case EWLGovernmentAIObjective::Align:         return TEXT("alinear bloque");
	case EWLGovernmentAIObjective::Industrialize: return TEXT("industrializar");
	default:                                      return TEXT("planificar");
	}
}

FString UWLPoliticalSubsystem::ReformMemoryKey(const FString& ReformId)
{
	return FString::Printf(TEXT("reform_%s"), *ReformId.TrimStartAndEnd().ToLower());
}

FString UWLPoliticalSubsystem::PartyKey(const FString& NationIso, const FString& PartyId)
{
	return NormalizeIso(NationIso) + TEXT("|") + PartyId.TrimStartAndEnd().ToLower();
}

FString UWLPoliticalSubsystem::CrisisChainKey(const FString& NationIso, EWLCrisisChainType Type)
{
	return FString::Printf(TEXT("%s|%d"), *NormalizeIso(NationIso), static_cast<int32>(Type));
}

bool UWLPoliticalSubsystem::ValidateNation(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetRegistry();
	FWLNationData Nation;
	return Registry && Registry->GetNation(NormalizeIso(NationIso), Nation);
}

FString UWLPoliticalSubsystem::GetPlayerNationIso() const
{
	if (const UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(GetGameInstance()))
	{
		return NormalizeIso(GI->GetSelectedNationIso());
	}
	return FString();
}

void UWLPoliticalSubsystem::ResetPoliticalState()
{
	InternalPowerByNation.Reset();
	RelationsByPair.Reset();
	IntelligenceByPair.Reset();
	EventQueue.Reset();
	EventDefinitions.Reset();
	NewsLog.Reset();
	GovernmentAgendaByNation.Reset();
	ActiveMinistryPrograms.Reset();
	CabinetDynamicsByNation.Reset();
	InstitutionalPowerByNation.Reset();
	PublicGroupSupportByKey.Reset();
	StateCapacityByNation.Reset();
	PoliticalMemoryByKey.Reset();
	GovernmentAIPlanByNation.Reset();
	ActivePolicyReforms.Reset();
	EnactedPolicyReforms.Reset();
	PoliticalParties.Reset();
	ElectionStateByNation.Reset();
	CharacterPoliticalProfilesById.Reset();
	PatronageStateByNation.Reset();
	MediaStateByNation.Reset();
	RegionGovernors.Reset();
	CrisisChains.Reset();
	GovernmentCalibrationByNation.Reset();
	CampaignOutcome = FWLCampaignOutcomeState();
	NextEventInstanceNumber = 1;
	LoadEventDefinitions();
	SeedInternalPower();
	SeedRelations();
	SeedGovernmentState();
}

void UWLPoliticalSubsystem::AddNews(const FString& Item)
{
	FString Stamp = Item;
	if (const UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Stamp = FString::Printf(TEXT("[%02d/%d] %s"), Tick->GetCurrentMonth(), Tick->GetCurrentYear(), *Item);
	}
	NewsLog.Insert(Stamp, 0);
	if (NewsLog.Num() > 40)
	{
		NewsLog.SetNum(40);
	}
}

void UWLPoliticalSubsystem::LoadEventDefinitions()
{
	const FString FilePath = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Political") / TEXT("PoliticalEvents.json");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		AddDefaultEventDefinitions(EventDefinitions);
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLPoliticalSubsystem: PoliticalEvents.json invalido, usando defaults."));
		AddDefaultEventDefinitions(EventDefinitions);
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;
		FWLPoliticalEventDefinition Def;
		Obj->TryGetStringField(TEXT("event_id"), Def.EventId);
		Obj->TryGetStringField(TEXT("title"), Def.Title);
		Obj->TryGetStringField(TEXT("body"), Def.Body);
		Obj->TryGetStringField(TEXT("trigger"), Def.Trigger);
		Obj->TryGetStringField(TEXT("target_iso"), Def.TargetIso);
		Obj->TryGetNumberField(TEXT("min_coup_risk"), Def.MinCoupRisk);
		Obj->TryGetNumberField(TEXT("min_opposition_strength"), Def.MinOppositionStrength);
		if (!Obj->TryGetNumberField(TEXT("max_public_order"), Def.MaxPublicOrder))
		{
			Def.MaxPublicOrder = 100;
		}

		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if (Obj->TryGetArrayField(TEXT("options"), Options) && Options)
		{
			for (const TSharedPtr<FJsonValue>& OptionValue : *Options)
			{
				const TSharedPtr<FJsonObject>* OptionObjPtr = nullptr;
				if (!OptionValue.IsValid() || !OptionValue->TryGetObject(OptionObjPtr) || !OptionObjPtr)
				{
					continue;
				}
				const TSharedPtr<FJsonObject>& OptionObj = *OptionObjPtr;
				FWLPoliticalEventOption Option;
				OptionObj->TryGetStringField(TEXT("option_id"), Option.OptionId);
				OptionObj->TryGetStringField(TEXT("label"), Option.Label);
				OptionObj->TryGetNumberField(TEXT("political_capital_delta"), Option.PoliticalCapitalDelta);
				double TreasuryDelta = 0.0;
				if (OptionObj->TryGetNumberField(TEXT("treasury_delta"), TreasuryDelta))
				{
					Option.TreasuryDelta = static_cast<int64>(TreasuryDelta);
				}
				OptionObj->TryGetNumberField(TEXT("opposition_delta"), Option.OppositionDelta);
				OptionObj->TryGetNumberField(TEXT("public_order_delta"), Option.PublicOrderDelta);
				OptionObj->TryGetNumberField(TEXT("relation_delta"), Option.RelationDelta);
				OptionObj->TryGetStringField(TEXT("market_shock_good_id"), Option.MarketShockGoodId);
				OptionObj->TryGetStringField(TEXT("market_shock_title"), Option.MarketShockTitle);
				OptionObj->TryGetNumberField(TEXT("market_shock_price_multiplier"), Option.MarketShockPriceMultiplier);
				OptionObj->TryGetNumberField(TEXT("market_shock_duration_months"), Option.MarketShockDurationMonths);
				if (!Option.OptionId.IsEmpty())
				{
					Def.Options.Add(MoveTemp(Option));
				}
			}
		}
		if (!Def.EventId.IsEmpty() && !Def.Options.IsEmpty())
		{
			EventDefinitions.Add(MoveTemp(Def));
		}
	}

	if (EventDefinitions.IsEmpty())
	{
		AddDefaultEventDefinitions(EventDefinitions);
	}
}

void UWLPoliticalSubsystem::SeedInternalPower()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		FWLInternalPowerState State;
		State.NationIso = Nation.Iso;
		State.AveragePublicOrder = GetAveragePublicOrder(Nation.Iso);
		InternalPowerByNation.Add(Nation.Iso, MoveTemp(State));
	}
}

void UWLPoliticalSubsystem::SeedRelations()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}
	const TArray<FWLNationData> Nations = Registry->GetAllNations();
	for (int32 i = 0; i < Nations.Num(); ++i)
	{
		for (int32 j = i + 1; j < Nations.Num(); ++j)
		{
			EnsureRelation(Nations[i].Iso, Nations[j].Iso);
		}
	}
}

void UWLPoliticalSubsystem::SeedGovernmentState()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		EnsureGovernmentAgenda(Nation.Iso);
		EnsureCabinetDynamics(Nation.Iso);
		EnsureInstitutionalPower(Nation.Iso);
		EnsureStateCapacity(Nation.Iso);
		EnsureGovernmentAIPlan(Nation.Iso);
		EnsureElectionState(Nation.Iso);
		EnsurePatronageState(Nation.Iso);
		EnsureMediaState(Nation.Iso);
		EnsureGovernmentCalibration(Nation.Iso);
		SeedPoliticalPartiesForNation(Nation.Iso);
		SeedRegionsForNation(Nation.Iso);
		SeedCharacterProfilesForNation(Nation.Iso);
		for (EWLPublicGroup Group : AllPublicGroups())
		{
			EnsurePublicGroup(Nation.Iso, Group);
		}
	}
}

FWLInternalPowerState& UWLPoliticalSubsystem::EnsureInternalPower(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLInternalPowerState& State = InternalPowerByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.AveragePublicOrder = GetAveragePublicOrder(Iso);
	}
	return State;
}

FWLDiplomaticRelationState& UWLPoliticalSubsystem::EnsureRelation(const FString& NationA, const FString& NationB)
{
	const FString Key = RelationKey(NationA, NationB);
	FWLDiplomaticRelationState& Relation = RelationsByPair.FindOrAdd(Key);
	if (Relation.NationA.IsEmpty() || Relation.NationB.IsEmpty())
	{
		const FString A = NormalizeIso(NationA);
		const FString B = NormalizeIso(NationB);
		Relation.NationA = A < B ? A : B;
		Relation.NationB = A < B ? B : A;
		Relation.Opinion = 0;
		Relation.Status = EWLDiplomaticStatus::Peace;
	}
	return Relation;
}

FWLIntelligenceNetworkState& UWLPoliticalSubsystem::EnsureNetwork(const FString& OwnerIso, const FString& TargetIso)
{
	const FString Key = NetworkKey(OwnerIso, TargetIso);
	FWLIntelligenceNetworkState& Network = IntelligenceByPair.FindOrAdd(Key);
	if (Network.OwnerIso.IsEmpty() || Network.TargetIso.IsEmpty())
	{
		Network.OwnerIso = NormalizeIso(OwnerIso);
		Network.TargetIso = NormalizeIso(TargetIso);
	}
	return Network;
}

FWLGovernmentAgendaState& UWLPoliticalSubsystem::EnsureGovernmentAgenda(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLGovernmentAgendaState& Agenda = GovernmentAgendaByNation.FindOrAdd(Iso);
	if (Agenda.NationIso.IsEmpty())
	{
		Agenda.NationIso = Iso;
		Agenda.Priorities = DefaultGovernmentAgenda();
		Agenda.LastAgendaReport = TEXT("Agenda inicial: seguridad, crecimiento y diplomacia.");
	}
	return Agenda;
}

FWLCabinetDynamicsState& UWLPoliticalSubsystem::EnsureCabinetDynamics(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLCabinetDynamicsState& State = CabinetDynamicsByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
	}
	return State;
}

FWLInstitutionalPowerState& UWLPoliticalSubsystem::EnsureInstitutionalPower(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLInstitutionalPowerState& State = InstitutionalPowerByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
	}
	return State;
}

FWLPublicGroupSupportState& UWLPoliticalSubsystem::EnsurePublicGroup(const FString& NationIso, EWLPublicGroup Group)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPublicGroupSupportState& State = PublicGroupSupportByKey.FindOrAdd(PublicGroupKey(Iso, Group));
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.Group = Group;
		State.Support = 50;
	}
	return State;
}

FWLStateCapacityState& UWLPoliticalSubsystem::EnsureStateCapacity(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLStateCapacityState& State = StateCapacityByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.Bureaucracy = 50;
		State.Corruption = 30;
		State.AdministrativeEfficiency = 50;
		State.CentralAuthority = 50;
		State.PolicyFailureRisk = 25;
	}
	return State;
}

FWLPoliticalAIPlanState& UWLPoliticalSubsystem::EnsureGovernmentAIPlan(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPoliticalAIPlanState& Plan = GovernmentAIPlanByNation.FindOrAdd(Iso);
	if (Plan.NationIso.IsEmpty())
	{
		Plan.NationIso = Iso;
		Plan.Objective = EWLGovernmentAIObjective::Stabilize;
		Plan.LastPlanReason = TEXT("Plan inicial: estabilizar gobierno.");
	}
	return Plan;
}

FWLElectionState& UWLPoliticalSubsystem::EnsureElectionState(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLElectionState& State = ElectionStateByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.MonthsToElection = ElectionCycleMonths;
		State.Phase = EWLElectionPhase::Governing;
		State.IncumbentApproval = 55;
		State.PollingGovernment = 52;
		State.PollingOpposition = 42;
		State.Legitimacy = 65;
		State.AbstentionRisk = 20;
		State.LastElectionReport = TEXT("Mandato en curso.");
	}
	return State;
}

FWLPatronageState& UWLPoliticalSubsystem::EnsurePatronageState(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPatronageState& State = PatronageStateByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.PatronagePower = 20;
		State.LastDeal = TEXT("Red clientelar limitada.");
	}
	return State;
}

FWLMediaPublicOpinionState& UWLPoliticalSubsystem::EnsureMediaState(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLMediaPublicOpinionState& State = MediaStateByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.PressFreedom = 60;
		State.MediaControl = 20;
		State.PresidentialApproval = 55;
		State.LastNarrative = TEXT("Opinion publica estable.");
	}
	return State;
}

FWLGovernmentCalibrationState& UWLPoliticalSubsystem::EnsureGovernmentCalibration(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLGovernmentCalibrationState& State = GovernmentCalibrationByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.LastReport = TEXT("Sin meses observados.");
	}
	return State;
}

FWLCharacterPoliticalProfile& UWLPoliticalSubsystem::EnsureCharacterPoliticalProfile(const FWLCharacter& Character)
{
	const FString CharacterId = NormalizeCharacterId(Character.Id);
	FWLCharacterPoliticalProfile& Profile = CharacterPoliticalProfilesById.FindOrAdd(CharacterId);
	if (Profile.CharacterId.IsEmpty())
	{
		Profile.NationIso = NormalizeIso(Character.CountryIso);
		Profile.CharacterId = CharacterId;
		Profile.FactionId = Character.PreferredOffice == EWLMinisterOffice::Defense || Character.Role == EWLCharacterRole::General
			? TEXT("security")
			: Character.PreferredOffice == EWLMinisterOffice::Economy
				? TEXT("technocrats")
				: Character.PreferredOffice == EWLMinisterOffice::Interior
					? TEXT("regional_machines")
					: Character.Role == EWLCharacterRole::Opposition
						? TEXT("opposition")
						: TEXT("presidential_circle");
		Profile.Biography = FString::Printf(TEXT("%s surge de la faccion %s y opera en la politica de %s."),
			*Character.Name, *Profile.FactionId, *Profile.NationIso);
		Profile.PresidentialAmbition = FMath::Clamp(Character.Ambition + Character.Popularity / 4, 0, 100);
		Profile.PersonalCorruption = Character.Traits.Contains(TEXT("corrupto")) || Character.Traits.Contains(TEXT("clientelista")) ? 55 : 20;
		Profile.ScandalHeat = Profile.PersonalCorruption / 4;
		Profile.SuccessionScore = FMath::Clamp(Character.Popularity / 2 + Character.Renown / 3 + Character.Ambition / 3, 0, 100);
		Profile.AgendaTags = Character.Traits;
		if (Profile.AgendaTags.IsEmpty())
		{
			Profile.AgendaTags.Add(Profile.FactionId);
		}
		Profile.LastProfileEvent = TEXT("Perfil politico inicial.");
	}
	return Profile;
}

void UWLPoliticalSubsystem::SeedPoliticalPartiesForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		return;
	}
	const FString Prefix = Iso.ToLower();
	TSet<FString> ExistingPartyKeys;
	for (const FWLPartyState& Party : PoliticalParties)
	{
		if (NormalizeIso(Party.NationIso) == Iso)
		{
			ExistingPartyKeys.Add(PartyKey(Party.NationIso, Party.PartyId));
		}
	}

	auto SelectLeader = [this, &Iso](EWLPartyRole Role, EWLPoliticalIdeology Ideology)
	{
		if (const UWLCharacterSubsystem* Characters = GetCharacters())
		{
			const TArray<FWLCharacter> Roster = Characters->GetCharactersByNation(Iso);
			auto FindByPredicate = [&Roster](auto Predicate)
			{
				for (const FWLCharacter& Candidate : Roster)
				{
					if (Candidate.bActive && Predicate(Candidate))
					{
						return UWLPoliticalSubsystem::NormalizeCharacterId(Candidate.Id);
					}
				}
				return FString();
			};

			if (Role == EWLPartyRole::Ruling)
			{
				const FString LeaderId = FindByPredicate([](const FWLCharacter& Candidate)
				{
					return Candidate.Role == EWLCharacterRole::ForeignLeader;
				});
				if (!LeaderId.IsEmpty())
				{
					return LeaderId;
				}
			}
			if (Role == EWLPartyRole::HardOpposition || Role == EWLPartyRole::SoftOpposition)
			{
				const FString OppositionId = FindByPredicate([](const FWLCharacter& Candidate)
				{
					return Candidate.Role == EWLCharacterRole::Opposition;
				});
				if (!OppositionId.IsEmpty())
				{
					return OppositionId;
				}
			}

			const FString MinisterId = FindByPredicate([Ideology](const FWLCharacter& Candidate)
			{
				if (Candidate.Role != EWLCharacterRole::Minister)
				{
					return false;
				}
				if (Ideology == EWLPoliticalIdeology::Technocratic)
				{
					return Candidate.PreferredOffice == EWLMinisterOffice::Economy
						|| Candidate.PreferredOffice == EWLMinisterOffice::Foreign;
				}
				if (Ideology == EWLPoliticalIdeology::Regionalist)
				{
					return Candidate.PreferredOffice == EWLMinisterOffice::Interior;
				}
				return true;
			});
			if (!MinisterId.IsEmpty())
			{
				return MinisterId;
			}
		}
		return FString();
	};

	auto AddParty = [this, &Iso, &ExistingPartyKeys, &SelectLeader](const FString& Suffix, const FString& Name, EWLPoliticalIdeology Ideology, EWLPartyRole Role,
		int32 Seats, int32 Discipline, int32 Loyalty, bool bCoalition)
	{
		FWLPartyState Party;
		Party.NationIso = Iso;
		Party.PartyId = Iso.ToLower() + TEXT("_") + Suffix;
		if (ExistingPartyKeys.Contains(PartyKey(Party.NationIso, Party.PartyId)))
		{
			return;
		}
		Party.Name = Name;
		Party.Ideology = Ideology;
		Party.Role = Role;
		Party.Seats = Seats;
		Party.Discipline = Discipline;
		Party.LoyaltyToGovernment = Loyalty;
		Party.Corruption = Role == EWLPartyRole::Ruling ? 28 : 18;
		Party.bInCoalition = bCoalition;
		Party.LeaderCharacterId = SelectLeader(Role, Ideology);
		Party.LastIncident = TEXT("Bancada constituida.");
		PoliticalParties.Add(Party);
	};

	AddParty(TEXT("gov"), TEXT("Partido de Gobierno"), EWLPoliticalIdeology::Liberal, EWLPartyRole::Ruling, 36, 72, 78, true);
	AddParty(TEXT("ally_reg"), TEXT("Bloque Regional"), EWLPoliticalIdeology::Regionalist, EWLPartyRole::Ally, 16, 58, 60, true);
	AddParty(TEXT("ally_market"), TEXT("Alianza de Centro"), EWLPoliticalIdeology::Technocratic, EWLPartyRole::Ally, 12, 62, 55, true);
	AddParty(TEXT("soft_opp"), TEXT("Oposicion Institucional"), EWLPoliticalIdeology::SocialDemocrat, EWLPartyRole::SoftOpposition, 20, 64, 28, false);
	AddParty(TEXT("hard_opp"), TEXT("Frente Duro"), EWLPoliticalIdeology::Nationalist, EWLPartyRole::HardOpposition, 16, 70, 8, false);

	for (FWLPartyState& Party : PoliticalParties)
	{
		if (Party.NationIso == Iso && Party.LeaderCharacterId.IsEmpty())
		{
			Party.LeaderCharacterId = SelectLeader(Party.Role, Party.Ideology);
		}
	}
}

void UWLPoliticalSubsystem::SeedRegionsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry || !ValidateNation(Iso))
	{
		return;
	}
	TSet<FString> ExistingRegionIds;
	for (const FWLRegionGovernorState& Region : RegionGovernors)
	{
		if (Region.NationIso == Iso)
		{
			ExistingRegionIds.Add(Region.RegionId);
		}
	}
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (NormalizeIso(Province.CountryIso) != Iso)
		{
			continue;
		}
		const FString RegionName = Province.Region.TrimStartAndEnd().IsEmpty() ? Province.Name : Province.Region;
		const FString RegionId = Province.Id;
		if (ExistingRegionIds.Contains(RegionId))
		{
			continue;
		}
		FWLRegionGovernorState Region;
		Region.NationIso = Iso;
		Region.RegionId = RegionId;
		Region.RegionName = RegionName;
		Region.GovernorName = FString::Printf(TEXT("Gob. %s"), *RegionName);
		const uint32 Hash = GetTypeHash(Iso + RegionId);
		Region.Alignment = Hash % 5 == 0 ? EWLPartyRole::SoftOpposition : EWLPartyRole::Ally;
		Region.Obedience = 50 + static_cast<int32>(Hash % 18);
		Region.Autonomy = 25 + static_cast<int32>(Hash % 20);
		Region.CenterControl = 55;
		Region.ProtestRisk = 15 + static_cast<int32>(Hash % 12);
		Region.LastReport = TEXT("Gobernacion operativa.");
		RegionGovernors.Add(Region);
	}

	if (!RegionGovernors.ContainsByPredicate([&Iso](const FWLRegionGovernorState& Region)
	{
		return Region.NationIso == Iso;
	}))
	{
		FWLRegionGovernorState Region;
		Region.NationIso = Iso;
		Region.RegionId = Iso + TEXT("-CAPITAL");
		Region.RegionName = TEXT("Capital nacional");
		Region.GovernorName = TEXT("Gob. Capital");
		Region.Alignment = EWLPartyRole::Ally;
		Region.LastReport = TEXT("Gobernacion sintetica para pais sin provincias detalladas.");
		RegionGovernors.Add(Region);
	}
}

void UWLPoliticalSubsystem::SeedCharacterProfilesForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		const TArray<FWLCharacter> Roster = Characters->GetCharactersByNation(Iso);
		FString HeadOfStateId;
		for (const FWLCharacter& Character : Roster)
		{
			if (Character.bActive && Character.Role == EWLCharacterRole::ForeignLeader)
			{
				HeadOfStateId = NormalizeCharacterId(Character.Id);
				break;
			}
		}
		for (const FWLCharacter& Character : Roster)
		{
			EnsureCharacterPoliticalProfile(Character);
		}
		for (int32 Index = 0; Index < Roster.Num(); ++Index)
		{
			FWLCharacterPoliticalProfile& Profile = EnsureCharacterPoliticalProfile(Roster[Index]);
			if (Profile.PatronCharacterId.IsEmpty()
				&& !HeadOfStateId.IsEmpty()
				&& Profile.CharacterId != HeadOfStateId
				&& Roster[Index].Role != EWLCharacterRole::Opposition)
			{
				Profile.PatronCharacterId = HeadOfStateId;
				Profile.LastProfileEvent = TEXT("Alineado con el circulo presidencial.");
			}
			if (Roster.Num() > 1 && Profile.RivalCharacterIds.IsEmpty())
			{
				const FWLCharacter& Rival = Roster[(Index + 1) % Roster.Num()];
				if (Rival.Id != Roster[Index].Id)
				{
					Profile.RivalCharacterIds.Add(NormalizeCharacterId(Rival.Id));
				}
			}
			if (Roster.Num() > 2 && Profile.AllyCharacterIds.IsEmpty())
			{
				const FWLCharacter& Ally = Roster[(Index + 2) % Roster.Num()];
				if (Ally.Id != Roster[Index].Id)
				{
					Profile.AllyCharacterIds.Add(NormalizeCharacterId(Ally.Id));
				}
			}
		}
	}
}

int32 UWLPoliticalSubsystem::GetAveragePublicOrder(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick)
	{
		return 70;
	}

	const FString Iso = NormalizeIso(NationIso);
	int32 Sum = 0;
	int32 Count = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (Tick->GetProvinceControllerIso(Province.Id) != Iso)
		{
			continue;
		}
		FWLProvinceRuntimeState State;
		if (Tick->GetProvinceState(Province.Id, State))
		{
			Sum += ClampPercent(State.PublicOrder);
			++Count;
		}
	}
	return Count > 0 ? Sum / Count : 70;
}

TArray<FString> UWLPoliticalSubsystem::GetLeaderAgendaTraits(const FString& NationIso) const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters)
	{
		return TArray<FString>();
	}
	for (const FWLCharacter& Character : Characters->GetCharactersByRole(NationIso, EWLCharacterRole::ForeignLeader))
	{
		if (Character.CountryIso == NormalizeIso(NationIso) && Character.bActive)
		{
			return Character.Traits;
		}
	}
	return TArray<FString>();
}

int32 UWLPoliticalSubsystem::GetLeaderAgendaPressure(const FString& NationIso) const
{
	int32 Pressure = 0;
	for (const FString& Trait : GetLeaderAgendaTraits(NationIso))
	{
		const FString T = Trait.ToLower();
		if (T == TEXT("polarizante"))
		{
			Pressure += 8;
		}
		else if (T == TEXT("autoritarismo"))
		{
			Pressure += 6;
		}
		else if (T == TEXT("reformista"))
		{
			Pressure += 3;
		}
		else if (T == TEXT("resistente"))
		{
			Pressure -= 4;
		}
	}
	return FMath::Clamp(Pressure, -10, 15);
}

FWLGovernmentAgendaState UWLPoliticalSubsystem::GetGovernmentAgenda(const FString& NationIso) const
{
	if (const FWLGovernmentAgendaState* Found = GovernmentAgendaByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLGovernmentAgendaState State;
	State.NationIso = NormalizeIso(NationIso);
	State.Priorities = DefaultGovernmentAgenda();
	return State;
}

bool UWLPoliticalSubsystem::SetGovernmentAgenda(
	const FString& NationIso,
	const TArray<EWLGovernmentPriority>& Priorities,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para agenda de gobierno.");
		return false;
	}
	if (Priorities.IsEmpty() || Priorities.Num() > MaxAgendaPriorities)
	{
		OutMessage = FString::Printf(TEXT("La agenda debe tener entre 1 y %d prioridades."), MaxAgendaPriorities);
		return false;
	}

	TArray<EWLGovernmentPriority> UniquePriorities;
	for (EWLGovernmentPriority Priority : Priorities)
	{
		if (!UniquePriorities.Contains(Priority))
		{
			UniquePriorities.Add(Priority);
		}
	}
	if (UniquePriorities.IsEmpty())
	{
		OutMessage = TEXT("Agenda vacia.");
		return false;
	}

	FWLGovernmentAgendaState& Agenda = EnsureGovernmentAgenda(Iso);
	Agenda.Priorities = MoveTemp(UniquePriorities);
	Agenda.MonthsActive = 0;
	TArray<FString> Labels;
	for (EWLGovernmentPriority Priority : Agenda.Priorities)
	{
		Labels.Add(GovernmentPriorityToString(Priority));
	}
	Agenda.LastAgendaReport = FString::Printf(TEXT("%s define agenda: %s."),
		*Iso, *FString::Join(Labels, TEXT(", ")));
	OutMessage = Agenda.LastAgendaReport;
	AddPoliticalMemory(Iso, TEXT("agenda_changed"), 1, 6, OutMessage);
	return true;
}

TArray<FWLMinistryProgramDefinition> UWLPoliticalSubsystem::GetAvailableMinistryPrograms(const FString& NationIso) const
{
	TArray<FWLMinistryProgramDefinition> Out;
	if (!ValidateNation(NationIso))
	{
		return Out;
	}
	Out = GovernmentProgramDefinitions();
	Out.Sort([](const FWLMinistryProgramDefinition& A, const FWLMinistryProgramDefinition& B)
	{
		if (A.Office != B.Office)
		{
			return static_cast<int32>(A.Office) < static_cast<int32>(B.Office);
		}
		return A.ProgramId < B.ProgramId;
	});
	return Out;
}

TArray<FWLMinistryProgramState> UWLPoliticalSubsystem::GetActiveMinistryPrograms(const FString& NationIso) const
{
	TArray<FWLMinistryProgramState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLMinistryProgramState& Program : ActiveMinistryPrograms)
	{
		if (Program.NationIso == Iso && Program.RemainingMonths > 0)
		{
			Out.Add(Program);
		}
	}
	Out.Sort([](const FWLMinistryProgramState& A, const FWLMinistryProgramState& B)
	{
		return A.ProgramId < B.ProgramId;
	});
	return Out;
}

bool UWLPoliticalSubsystem::GetMinistryProgramDefinition(
	const FString& ProgramId,
	FWLMinistryProgramDefinition& OutDefinition) const
{
	const FString Id = ProgramId.TrimStartAndEnd().ToLower();
	for (const FWLMinistryProgramDefinition& Definition : GovernmentProgramDefinitions())
	{
		if (Definition.ProgramId == Id)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	return false;
}

bool UWLPoliticalSubsystem::StartMinistryProgram(const FString& NationIso, const FString& ProgramId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para programa ministerial.");
		return false;
	}

	FWLMinistryProgramDefinition Definition;
	if (!GetMinistryProgramDefinition(ProgramId, Definition))
	{
		OutMessage = FString::Printf(TEXT("Programa desconocido: %s"), *ProgramId);
		return false;
	}
	for (const FWLMinistryProgramState& Active : ActiveMinistryPrograms)
	{
		if (Active.NationIso == Iso && Active.Office == Definition.Office && Active.RemainingMonths > 0)
		{
			OutMessage = FString::Printf(TEXT("%s ya ejecuta un programa de %s."),
				*Iso, *UWLCharacterSubsystem::MinisterOfficeToString(Definition.Office));
			return false;
		}
	}

	if (Definition.TreasuryCost > 0)
	{
		UWLStrategicTickSubsystem* Tick = GetTick();
		if (!Tick || Tick->GetTreasury(Iso) < Definition.TreasuryCost)
		{
			OutMessage = FString::Printf(TEXT("Tesoro insuficiente para %s."), *Definition.Name);
			return false;
		}
	}

	if (Definition.bRequiresLegislation)
	{
		FString VoteMessage;
		if (!PassGovernmentReform(Iso, Definition.ProgramId, Definition.PoliticalCapitalCost, VoteMessage))
		{
			OutMessage = VoteMessage;
			return false;
		}
	}
	else if (Definition.PoliticalCapitalCost > 0)
	{
		UWLCharacterSubsystem* Characters = GetCharacters();
		if (!Characters || Characters->GetPoliticalCapital(Iso) < Definition.PoliticalCapitalCost)
		{
			OutMessage = TEXT("Capital politico insuficiente para iniciar programa.");
			return false;
		}
		Characters->AdjustPoliticalCapital(Iso, -Definition.PoliticalCapitalCost);
	}

	if (Definition.TreasuryCost > 0
		&& !TrySpendTreasury(Iso, Definition.TreasuryCost, Definition.Name, OutMessage))
	{
		return false;
	}

	FWLMinistryProgramState Program;
	Program.NationIso = Iso;
	Program.ProgramId = Definition.ProgramId;
	Program.Name = Definition.Name;
	Program.Office = Definition.Office;
	Program.RemainingMonths = FMath::Max(1, Definition.DurationMonths);
	Program.Progress = 0;
	Program.LastReport = FString::Printf(TEXT("%s inicia %s (%s, %d meses)."),
		*Iso, *Definition.Name, *UWLCharacterSubsystem::MinisterOfficeToString(Definition.Office), Program.RemainingMonths);
	ActiveMinistryPrograms.Add(Program);
	OutMessage = Program.LastReport;
	AddPoliticalMemory(Iso, TEXT("program_started"), 1, Program.RemainingMonths + 3, Program.LastReport);
	return true;
}

FWLCabinetDynamicsState UWLPoliticalSubsystem::GetCabinetDynamics(const FString& NationIso) const
{
	if (const FWLCabinetDynamicsState* Found = CabinetDynamicsByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLCabinetDynamicsState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

FWLInstitutionalPowerState UWLPoliticalSubsystem::GetInstitutionalPower(const FString& NationIso) const
{
	if (const FWLInstitutionalPowerState* Found = InstitutionalPowerByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLInstitutionalPowerState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

TArray<FWLPublicGroupSupportState> UWLPoliticalSubsystem::GetPublicGroups(const FString& NationIso) const
{
	TArray<FWLPublicGroupSupportState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLPublicGroupSupportState>& Pair : PublicGroupSupportByKey)
	{
		if (Pair.Value.NationIso == Iso)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLPublicGroupSupportState& A, const FWLPublicGroupSupportState& B)
	{
		return static_cast<int32>(A.Group) < static_cast<int32>(B.Group);
	});
	return Out;
}

FWLStateCapacityState UWLPoliticalSubsystem::GetStateCapacity(const FString& NationIso) const
{
	if (const FWLStateCapacityState* Found = StateCapacityByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLStateCapacityState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

TArray<FWLPoliticalMemoryRecord> UWLPoliticalSubsystem::GetPoliticalMemory(const FString& NationIso) const
{
	TArray<FWLPoliticalMemoryRecord> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLPoliticalMemoryRecord>& Pair : PoliticalMemoryByKey)
	{
		if (Pair.Value.NationIso == Iso && Pair.Value.MonthsRemaining > 0 && Pair.Value.Value != 0)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLPoliticalMemoryRecord& A, const FWLPoliticalMemoryRecord& B)
	{
		return A.MemoryKey < B.MemoryKey;
	});
	return Out;
}

FWLPoliticalAIPlanState UWLPoliticalSubsystem::GetGovernmentAIPlan(const FString& NationIso) const
{
	if (const FWLPoliticalAIPlanState* Found = GovernmentAIPlanByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLPoliticalAIPlanState Plan;
	Plan.NationIso = NormalizeIso(NationIso);
	return Plan;
}

bool UWLPoliticalSubsystem::PassGovernmentReform(
	const FString& NationIso,
	const FString& ReformId,
	int32 PoliticalCapitalCost,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para reforma.");
		return false;
	}
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	UWLCharacterSubsystem* Characters = GetCharacters();
	const int32 EffectiveCost = FMath::Max(0, PoliticalCapitalCost + Institutions.ReformCost / 2);
	if (!Characters || Characters->GetPoliticalCapital(Iso) < EffectiveCost)
	{
		OutMessage = FString::Printf(TEXT("Capital politico insuficiente para reforma %s (%d requerido)."),
			*ReformId, EffectiveCost);
		return false;
	}
	if (Institutions.GridlockRisk >= 80 && Institutions.RulingCoalitionSupport < 45)
	{
		Characters->AdjustPoliticalCapital(Iso, -FMath::Max(1, EffectiveCost / 2));
		Institutions.LegislativeOpposition = ClampPercent(Institutions.LegislativeOpposition + 3);
		Institutions.LastVoteReport = FString::Printf(TEXT("Reforma %s bloqueada por Congreso/oposicion."), *ReformId);
		OutMessage = Institutions.LastVoteReport;
		AddPoliticalMemory(Iso, TEXT("reform_blocked"), 1, 8, OutMessage);
		return false;
	}

	Characters->AdjustPoliticalCapital(Iso, -EffectiveCost);
	Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport - FMath::Max(1, EffectiveCost / 8));
	Institutions.LegislativeOpposition = ClampPercent(Institutions.LegislativeOpposition + FMath::Max(1, EffectiveCost / 10));
	Institutions.LastVoteReport = FString::Printf(TEXT("Reforma %s aprobada. Coste politico %d."), *ReformId, EffectiveCost);
	OutMessage = Institutions.LastVoteReport;
	AddPoliticalMemory(Iso, TEXT("reform_passed"), 1, 10, OutMessage);
	return true;
}

TArray<FWLPolicyReformDefinition> UWLPoliticalSubsystem::GetAvailablePolicyReforms(const FString& NationIso) const
{
	TArray<FWLPolicyReformDefinition> Out;
	if (!ValidateNation(NationIso))
	{
		return Out;
	}
	Out = PolicyReformDefinitions();
	Out.Sort([](const FWLPolicyReformDefinition& A, const FWLPolicyReformDefinition& B)
	{
		return A.Area == B.Area
			? A.ReformId < B.ReformId
			: static_cast<int32>(A.Area) < static_cast<int32>(B.Area);
	});
	return Out;
}

TArray<FWLActiveReformState> UWLPoliticalSubsystem::GetActivePolicyReforms(const FString& NationIso) const
{
	TArray<FWLActiveReformState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLActiveReformState& Reform : ActivePolicyReforms)
	{
		if (Reform.NationIso == Iso && Reform.MonthsRemaining > 0)
		{
			Out.Add(Reform);
		}
	}
	Out.Sort([](const FWLActiveReformState& A, const FWLActiveReformState& B)
	{
		return A.ReformId < B.ReformId;
	});
	return Out;
}

TArray<FWLEnactedPolicyReformState> UWLPoliticalSubsystem::GetEnactedPolicyReforms(const FString& NationIso) const
{
	TArray<FWLEnactedPolicyReformState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLEnactedPolicyReformState& Reform : EnactedPolicyReforms)
	{
		if (Reform.NationIso == Iso)
		{
			Out.Add(Reform);
		}
	}
	Out.Sort([](const FWLEnactedPolicyReformState& A, const FWLEnactedPolicyReformState& B)
	{
		return A.ReformId < B.ReformId;
	});
	return Out;
}

bool UWLPoliticalSubsystem::GetPolicyReformDefinition(const FString& ReformId, FWLPolicyReformDefinition& OutDefinition) const
{
	const FString Id = ReformId.TrimStartAndEnd().ToLower();
	for (const FWLPolicyReformDefinition& Definition : PolicyReformDefinitions())
	{
		if (Definition.ReformId == Id)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	return false;
}

bool UWLPoliticalSubsystem::HasReformMemory(const FString& NationIso, const FString& ReformId) const
{
	const FString Iso = NormalizeIso(NationIso);
	const FString Id = ReformId.TrimStartAndEnd().ToLower();
	if (HasEnactedPolicyReform(Iso, Id))
	{
		return true;
	}
	if (GetPoliticalMemoryValue(Iso, ReformMemoryKey(Id)) > 0)
	{
		return true;
	}
	return ActivePolicyReforms.ContainsByPredicate([&Iso, &Id](const FWLActiveReformState& Reform)
	{
		return Reform.NationIso == Iso && Reform.ReformId == Id;
	});
}

bool UWLPoliticalSubsystem::HasEnactedPolicyReform(const FString& NationIso, const FString& ReformId) const
{
	const FString Iso = NormalizeIso(NationIso);
	const FString Id = ReformId.TrimStartAndEnd().ToLower();
	return EnactedPolicyReforms.ContainsByPredicate([&Iso, &Id](const FWLEnactedPolicyReformState& Reform)
	{
		return Reform.NationIso == Iso && Reform.ReformId == Id;
	});
}

bool UWLPoliticalSubsystem::TrySpendTreasury(
	const FString& NationIso,
	int64 Amount,
	const FString& Reason,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (Amount <= 0)
	{
		return true;
	}

	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick)
	{
		OutMessage = FString::Printf(TEXT("No hay sistema economico para pagar %s."), *Reason);
		return false;
	}
	const int64 Treasury = Tick->GetTreasury(Iso);
	if (Treasury < Amount)
	{
		OutMessage = FString::Printf(TEXT("Tesoro insuficiente para %s (%lld/%lld)."),
			*Reason,
			static_cast<long long>(Treasury),
			static_cast<long long>(Amount));
		return false;
	}

	FString TreasuryMessage;
	Tick->AdjustTreasury(Iso, -Amount, TreasuryMessage);
	return true;
}

void UWLPoliticalSubsystem::RecordEnactedPolicyReform(
	const FString& NationIso,
	const FWLPolicyReformDefinition& Definition,
	const FString& Report)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso) || Definition.ReformId.IsEmpty())
	{
		return;
	}
	const FString PromiseFulfilledKey = TEXT("campaign_promise_fulfilled_") + Definition.ReformId;
	const FWLElectionState Election = GetElectionState(Iso);
	const bool bFulfilledPromise = Election.CampaignPromiseReformId == Definition.ReformId
		|| Election.bCampaignPromiseFulfilled
		|| GetPoliticalMemoryValue(Iso, PromiseFulfilledKey) > 0;
	for (FWLEnactedPolicyReformState& Existing : EnactedPolicyReforms)
	{
		if (Existing.NationIso == Iso && Existing.ReformId == Definition.ReformId)
		{
			Existing.Name = Definition.Name;
			Existing.Area = Definition.Area;
			Existing.bFulfilledCampaignPromise = Existing.bFulfilledCampaignPromise || bFulfilledPromise;
			Existing.LastReport = Report;
			return;
		}
	}

	FWLEnactedPolicyReformState Enacted;
	Enacted.NationIso = Iso;
	Enacted.ReformId = Definition.ReformId;
	Enacted.Name = Definition.Name;
	Enacted.Area = Definition.Area;
	Enacted.MonthsSinceEnacted = 0;
	Enacted.bFulfilledCampaignPromise = bFulfilledPromise;
	Enacted.LastReport = Report;
	EnactedPolicyReforms.Add(MoveTemp(Enacted));
}

void UWLPoliticalSubsystem::MarkCampaignPromiseFulfilled(
	const FString& NationIso,
	const FWLPolicyReformDefinition& Definition)
{
	FWLElectionState& Election = EnsureElectionState(NationIso);
	if (Election.CampaignPromiseReformId != Definition.ReformId || Election.bCampaignPromiseFulfilled)
	{
		return;
	}

	Election.bCampaignPromiseFulfilled = true;
	Election.Legitimacy = ClampPercent(Election.Legitimacy + 4);
	Election.PollingGovernment = ClampPercent(Election.PollingGovernment + 3);
	Election.LastElectionReport = FString::Printf(TEXT("%s cumple la promesa electoral: %s."),
		*NormalizeIso(NationIso),
		*Definition.Name);
	AddPoliticalMemory(NationIso, TEXT("campaign_promise_fulfilled"), 1, 18, Election.LastElectionReport);
	AddPoliticalMemory(NationIso, TEXT("campaign_promise_fulfilled_") + Definition.ReformId, 1, 84, Election.LastElectionReport);
}

bool UWLPoliticalSubsystem::EnactPolicyReform(const FString& NationIso, const FString& ReformId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para reforma P2.");
		return false;
	}

	FWLPolicyReformDefinition Definition;
	if (!GetPolicyReformDefinition(ReformId, Definition))
	{
		OutMessage = FString::Printf(TEXT("Reforma desconocida: %s"), *ReformId);
		return false;
	}
	if (HasReformMemory(Iso, Definition.ReformId))
	{
		OutMessage = FString::Printf(TEXT("%s ya aprobo o esta implementando %s."), *Iso, *Definition.Name);
		return false;
	}
	for (const FString& Prerequisite : Definition.PrerequisiteReformIds)
	{
		if (!HasReformMemory(Iso, Prerequisite))
		{
			OutMessage = FString::Printf(TEXT("%s requiere aprobar antes %s."), *Definition.Name, *Prerequisite);
			return false;
		}
	}

	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	if (Institutions.RulingCoalitionSupport < Definition.RequiredCoalitionSupport)
	{
		OutMessage = FString::Printf(TEXT("Coalicion insuficiente para %s (%d/%d)."),
			*Definition.Name, Institutions.RulingCoalitionSupport, Definition.RequiredCoalitionSupport);
		return false;
	}
	if (Capacity.AdministrativeEfficiency < Definition.RequiredStateCapacity)
	{
		OutMessage = FString::Printf(TEXT("Capacidad estatal insuficiente para %s (%d/%d)."),
			*Definition.Name, Capacity.AdministrativeEfficiency, Definition.RequiredStateCapacity);
		return false;
	}

	if (Definition.TreasuryCost > 0)
	{
		UWLStrategicTickSubsystem* Tick = GetTick();
		if (!Tick || Tick->GetTreasury(Iso) < Definition.TreasuryCost)
		{
			OutMessage = FString::Printf(TEXT("Tesoro insuficiente para %s."), *Definition.Name);
			return false;
		}
	}

	FString VoteMessage;
	if (!PassGovernmentReform(Iso, Definition.ReformId, Definition.PoliticalCapitalCost, VoteMessage))
	{
		OutMessage = VoteMessage;
		return false;
	}
	if (Definition.TreasuryCost > 0
		&& !TrySpendTreasury(Iso, Definition.TreasuryCost, Definition.Name, OutMessage))
	{
		return false;
	}

	FWLInternalPowerState& Internal = EnsureInternalPower(Iso);
	FWLElectionState& Election = EnsureElectionState(Iso);
	Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength + Definition.OppositionDelta);
	Election.Legitimacy = ClampPercent(Election.Legitimacy + Definition.LegitimacyDelta);
	if (Definition.ReformId == TEXT("constitution_term_rules"))
	{
		Election.bTermLimited = false;
		Election.ConsecutiveTermsWon = FMath::Min(Election.ConsecutiveTermsWon, 2);
		AddPoliticalMemory(Iso, TEXT("constitutional_rules_changed"), 1, 36,
			FString::Printf(TEXT("%s cambia reglas de mandato."), *Iso));
	}
	Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + Definition.CapacityDelta);
	Capacity.Corruption = ClampPercent(Capacity.Corruption + Definition.CorruptionDelta);
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		if (Definition.PublicOrderDelta != 0)
		{
			Tick->AdjustNationPublicOrder(Iso, Definition.PublicOrderDelta);
		}
	}
	for (EWLPublicGroup Group : Definition.AffectedGroups)
	{
		AdjustPublicGroupSupport(Iso, Group, Definition.PublicGroupSupportDelta, Definition.Name);
	}

	FWLActiveReformState Active;
	Active.NationIso = Iso;
	Active.ReformId = Definition.ReformId;
	Active.Name = Definition.Name;
	Active.Area = Definition.Area;
	Active.MonthsRemaining = FMath::Max(1, Definition.LongTermMonths);
	Active.ImplementationProgress = 0;
	Active.Backlash = Definition.ProtestRisk;
	Active.LastReport = FString::Printf(TEXT("%s aprobo %s. %s"), *Iso, *Definition.Name, *VoteMessage);
	ActivePolicyReforms.Add(Active);
	MarkCampaignPromiseFulfilled(Iso, Definition);
	AddPoliticalMemory(Iso, ReformMemoryKey(Definition.ReformId), 1, FMath::Max(60, Definition.LongTermMonths + 12), Active.LastReport);

	const int32 Roll = static_cast<int32>((GetTypeHash(Iso + Definition.ReformId) + Definition.ProtestRisk * 7) % 100);
	if (Roll < Definition.ProtestRisk + Institutions.GridlockRisk / 3)
	{
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, Definition.ProtestRisk,
			FString::Printf(TEXT("Backlash contra %s."), *Definition.Name));
	}

	OutMessage = Active.LastReport;
	AddNews(OutMessage);
	return true;
}

TArray<FWLPartyState> UWLPoliticalSubsystem::GetPoliticalParties(const FString& NationIso) const
{
	TArray<FWLPartyState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLPartyState& Party : PoliticalParties)
	{
		if (Party.NationIso == Iso)
		{
			Out.Add(Party);
		}
	}
	Out.Sort([](const FWLPartyState& A, const FWLPartyState& B)
	{
		return A.Seats == B.Seats ? A.PartyId < B.PartyId : A.Seats > B.Seats;
	});
	return Out;
}

FWLPartyState* UWLPoliticalSubsystem::FindMutableParty(const FString& NationIso, const FString& PartyId)
{
	const FString Key = PartyKey(NationIso, PartyId);
	return PoliticalParties.FindByPredicate([&Key, this](const FWLPartyState& Party)
	{
		return PartyKey(Party.NationIso, Party.PartyId) == Key;
	});
}

bool UWLPoliticalSubsystem::NegotiatePartySupport(const FString& NationIso, const FString& PartyId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPartyState* Party = FindMutableParty(Iso, PartyId);
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Party || !Characters || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Partido invalido para negociacion.");
		return false;
	}
	constexpr int32 PoliticalCost = 6;
	if (Characters->GetPoliticalCapital(Iso) < PoliticalCost)
	{
		OutMessage = TEXT("Capital politico insuficiente para negociar coalicion.");
		return false;
	}
	Characters->AdjustPoliticalCapital(Iso, -PoliticalCost);
	Party->LoyaltyToGovernment = ClampPercent(Party->LoyaltyToGovernment + 18);
	Party->Discipline = ClampPercent(Party->Discipline + 6);
	Party->bInCoalition = Party->Role != EWLPartyRole::HardOpposition || Party->LoyaltyToGovernment >= 35;
	Party->Role = Party->bInCoalition && Party->Role != EWLPartyRole::Ruling ? EWLPartyRole::Ally : Party->Role;
	Party->Corruption = ClampPercent(Party->Corruption + 4);
	Party->LastIncident = FString::Printf(TEXT("%s negocia apoyo con %s."), *Iso, *Party->Name);
	EnsureInstitutionalPower(Iso).RulingCoalitionSupport = ClampPercent(EnsureInstitutionalPower(Iso).RulingCoalitionSupport + 5);
	EnsurePatronageState(Iso).ClientelistPressure = ClampPercent(EnsurePatronageState(Iso).ClientelistPressure + 3);
	OutMessage = Party->LastIncident;
	return true;
}

bool UWLPoliticalSubsystem::HoldPartyInternalElection(const FString& NationIso, const FString& PartyId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPartyState* Party = FindMutableParty(Iso, PartyId);
	if (!Party || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Partido invalido para eleccion interna.");
		return false;
	}
	const int32 Renewal = static_cast<int32>((GetTypeHash(Iso + Party->PartyId + TEXT("internal")) % 21));
	Party->MonthsSinceInternalElection = 0;
	Party->Discipline = ClampPercent(55 + Renewal);
	if (Party->Role == EWLPartyRole::Ruling || Party->Role == EWLPartyRole::Ally)
	{
		Party->LoyaltyToGovernment = ClampPercent(Party->LoyaltyToGovernment + Renewal / 3 - 4);
	}
	else
	{
		Party->LoyaltyToGovernment = ClampPercent(Party->LoyaltyToGovernment - 2);
	}
	Party->LastIncident = FString::Printf(TEXT("%s celebra eleccion interna; disciplina %d."),
		*Party->Name, Party->Discipline);
	OutMessage = Party->LastIncident;
	return true;
}

FWLElectionState UWLPoliticalSubsystem::GetElectionState(const FString& NationIso) const
{
	if (const FWLElectionState* Found = ElectionStateByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLElectionState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

bool UWLPoliticalSubsystem::MakeCampaignPromise(const FString& NationIso, const FString& ReformId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPolicyReformDefinition Definition;
	if (!ValidateNation(Iso) || !GetPolicyReformDefinition(ReformId, Definition))
	{
		OutMessage = TEXT("Promesa de campania invalida.");
		return false;
	}
	FWLElectionState& Election = EnsureElectionState(Iso);
	Election.CampaignPromiseReformId = Definition.ReformId;
	Election.bCampaignPromiseFulfilled = HasReformMemory(Iso, Definition.ReformId);
	Election.CampaignIntensity = ClampPercent(Election.CampaignIntensity + 12);
	Election.PollingGovernment = ClampPercent(Election.PollingGovernment + 4);
	Election.LastElectionReport = FString::Printf(TEXT("%s promete %s en campania."), *Iso, *Definition.Name);
	AddPoliticalMemory(Iso, TEXT("campaign_promise"), 1, FMath::Max(6, Election.MonthsToElection + 3), Election.LastElectionReport);
	OutMessage = Election.LastElectionReport;
	return true;
}

TArray<FWLCharacterPoliticalProfile> UWLPoliticalSubsystem::GetCharacterPoliticalProfiles(const FString& NationIso) const
{
	TArray<FWLCharacterPoliticalProfile> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLCharacterPoliticalProfile>& Pair : CharacterPoliticalProfilesById)
	{
		if (Pair.Value.NationIso == Iso)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLCharacterPoliticalProfile& A, const FWLCharacterPoliticalProfile& B)
	{
		return A.SuccessionScore == B.SuccessionScore ? A.CharacterId < B.CharacterId : A.SuccessionScore > B.SuccessionScore;
	});
	return Out;
}

bool UWLPoliticalSubsystem::GetCharacterPoliticalProfile(const FString& CharacterId, FWLCharacterPoliticalProfile& OutProfile) const
{
	if (const FWLCharacterPoliticalProfile* Found = CharacterPoliticalProfilesById.Find(NormalizeCharacterId(CharacterId)))
	{
		OutProfile = *Found;
		return true;
	}
	return false;
}

FWLPatronageState UWLPoliticalSubsystem::GetPatronageState(const FString& NationIso) const
{
	if (const FWLPatronageState* Found = PatronageStateByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLPatronageState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

bool UWLPoliticalSubsystem::UsePatronage(const FString& NationIso, EWLPatronageActionType Action, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para patronazgo.");
		return false;
	}
	FWLPatronageState& Patronage = EnsurePatronageState(Iso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	int64 TreasuryCost = 0;
	switch (Action)
	{
	case EWLPatronageActionType::AwardContract:
		TreasuryCost = 2200;
		break;
	case EWLPatronageActionType::GrantFavor:
		TreasuryCost = 900;
		break;
	case EWLPatronageActionType::FundGovernor:
		TreasuryCost = 1800;
		break;
	default:
		break;
	}
	if (TreasuryCost > 0
		&& !TrySpendTreasury(Iso, TreasuryCost, TEXT("accion de patronazgo"), OutMessage))
	{
		return false;
	}

	switch (Action)
	{
	case EWLPatronageActionType::AppointLoyalist:
		Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower + 8);
		Patronage.ClientelistPressure = ClampPercent(Patronage.ClientelistPressure + 5);
		Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport + 4);
		Capacity.Corruption = ClampPercent(Capacity.Corruption + 2);
		Patronage.LastDeal = TEXT("Leales nombrados en cargos sensibles.");
		break;
	case EWLPatronageActionType::AwardContract:
		Patronage.ContractCorruption = ClampPercent(Patronage.ContractCorruption + 12);
		Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower + 7);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 3, TEXT("contratos publicos"));
		Patronage.LastDeal = TEXT("Contratos publicos repartidos a aliados.");
		break;
	case EWLPatronageActionType::GrantFavor:
		Patronage.ClientelistPressure = ClampPercent(Patronage.ClientelistPressure + 8);
		Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport + 6);
		Patronage.LastDeal = TEXT("Favores politicos aseguran votos temporales.");
		break;
	case EWLPatronageActionType::FundGovernor:
		Patronage.RegionalMachines = ClampPercent(Patronage.RegionalMachines + 10);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Regions, 3, TEXT("financiacion regional"));
		Patronage.LastDeal = TEXT("Gobernadores aliados reciben presupuesto regional.");
		break;
	case EWLPatronageActionType::GrantConcession:
	default:
		Patronage.ConcessionBacklash = ClampPercent(Patronage.ConcessionBacklash + 10);
		Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower + 5);
		Capacity.Corruption = ClampPercent(Capacity.Corruption + 3);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 4, TEXT("concesiones"));
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -2, TEXT("concesiones"));
		Patronage.LastDeal = TEXT("Concesiones economicas compran apoyo y elevan backlash.");
		break;
	}
	OutMessage = Patronage.LastDeal;
	return true;
}

FWLMediaPublicOpinionState UWLPoliticalSubsystem::GetMediaPublicOpinion(const FString& NationIso) const
{
	if (const FWLMediaPublicOpinionState* Found = MediaStateByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLMediaPublicOpinionState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

bool UWLPoliticalSubsystem::RunMediaAction(const FString& NationIso, EWLMediaActionType Action, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para accion de medios.");
		return false;
	}
	FWLMediaPublicOpinionState& Media = EnsureMediaState(Iso);
	FWLElectionState& Election = EnsureElectionState(Iso);
	switch (Action)
	{
	case EWLMediaActionType::PressBriefing:
		Media.PressFreedom = ClampPercent(Media.PressFreedom + 2);
		Media.PresidentialApproval = ClampPercent(Media.PresidentialApproval + 3);
		Media.MediaCrisisRisk = ClampPercent(Media.MediaCrisisRisk - 4);
		Media.LastNarrative = TEXT("Rueda de prensa reduce incertidumbre publica.");
		break;
	case EWLMediaActionType::StateBroadcast:
		Media.MediaControl = ClampPercent(Media.MediaControl + 5);
		Media.PropagandaReach = ClampPercent(Media.PropagandaReach + 8);
		Media.PresidentialApproval = ClampPercent(Media.PresidentialApproval + 4);
		Media.LastNarrative = TEXT("Cadena nacional instala narrativa oficial.");
		break;
	case EWLMediaActionType::Propaganda:
		Media.PropagandaReach = ClampPercent(Media.PropagandaReach + 12);
		Media.FakeNewsPressure = ClampPercent(Media.FakeNewsPressure + 3);
		Media.PresidentialApproval = ClampPercent(Media.PresidentialApproval + 5);
		Media.LastNarrative = TEXT("Propaganda interna moviliza base oficialista.");
		break;
	case EWLMediaActionType::Censorship:
		Media.PressFreedom = ClampPercent(Media.PressFreedom - 10);
		Media.MediaControl = ClampPercent(Media.MediaControl + 12);
		Media.CensorshipBacklash = ClampPercent(Media.CensorshipBacklash + 14);
		Election.Legitimacy = ClampPercent(Election.Legitimacy - 4);
		Media.LastNarrative = TEXT("Censura contiene escandalo y erosiona legitimidad.");
		break;
	case EWLMediaActionType::CounterFakeNews:
	default:
		Media.FakeNewsPressure = ClampPercent(Media.FakeNewsPressure - 12);
		Media.MediaCrisisRisk = ClampPercent(Media.MediaCrisisRisk - 8);
		Media.PressFreedom = ClampPercent(Media.PressFreedom + 1);
		Media.LastNarrative = TEXT("Contra fake news estabiliza la opinion publica.");
		break;
	}
	OutMessage = Media.LastNarrative;
	return true;
}

TArray<FWLRegionGovernorState> UWLPoliticalSubsystem::GetRegionGovernors(const FString& NationIso) const
{
	TArray<FWLRegionGovernorState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLRegionGovernorState& Region : RegionGovernors)
	{
		if (Region.NationIso == Iso)
		{
			Out.Add(Region);
		}
	}
	Out.Sort([](const FWLRegionGovernorState& A, const FWLRegionGovernorState& B)
	{
		return A.RegionId < B.RegionId;
	});
	return Out;
}

FWLRegionGovernorState* UWLPoliticalSubsystem::FindMutableRegion(const FString& NationIso, const FString& RegionId)
{
	const FString Iso = NormalizeIso(NationIso);
	const FString Id = RegionId.TrimStartAndEnd();
	return RegionGovernors.FindByPredicate([&Iso, &Id](const FWLRegionGovernorState& Region)
	{
		return Region.NationIso == Iso && Region.RegionId == Id;
	});
}

bool UWLPoliticalSubsystem::RunRegionPolicy(
	const FString& NationIso,
	const FString& RegionId,
	EWLRegionPolicyActionType Action,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLRegionGovernorState* Region = FindMutableRegion(Iso, RegionId);
	if (!Region || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Region invalida para politica territorial.");
		return false;
	}
	switch (Action)
	{
	case EWLRegionPolicyActionType::AppointGovernor:
		Region->GovernorName = FString::Printf(TEXT("Gob. Leal %s"), *Region->RegionName);
		Region->Alignment = EWLPartyRole::Ally;
		Region->Obedience = ClampPercent(Region->Obedience + 15);
		Region->CenterControl = ClampPercent(Region->CenterControl + 8);
		Region->Autonomy = ClampPercent(Region->Autonomy - 5);
		EnsurePatronageState(Iso).RegionalMachines = ClampPercent(EnsurePatronageState(Iso).RegionalMachines + 5);
		Region->LastReport = TEXT("Gobernador leal nombrado.");
		break;
	case EWLRegionPolicyActionType::RegionalInvestment:
		if (!TrySpendTreasury(Iso, 1800, TEXT("inversion regional"), OutMessage))
		{
			return false;
		}
		Region->InvestmentLevel = ClampPercent(Region->InvestmentLevel + 12);
		Region->ProtestRisk = ClampPercent(Region->ProtestRisk - 10);
		Region->Obedience = ClampPercent(Region->Obedience + 5);
		Region->LastReport = TEXT("Inversion regional baja protesta.");
		break;
	case EWLRegionPolicyActionType::AutonomyDeal:
		Region->Autonomy = ClampPercent(Region->Autonomy + 10);
		Region->ProtestRisk = ClampPercent(Region->ProtestRisk - 12);
		Region->CenterControl = ClampPercent(Region->CenterControl - 6);
		Region->LastReport = TEXT("Pacto autonomico calma region y reduce control central.");
		break;
	case EWLRegionPolicyActionType::SecurityOperation:
	default:
		Region->CenterControl = ClampPercent(Region->CenterControl + 12);
		Region->ProtestRisk = ClampPercent(Region->ProtestRisk - 5);
		Region->Obedience = ClampPercent(Region->Obedience + 4);
		Region->Autonomy = ClampPercent(Region->Autonomy - 4);
		if (UWLStrategicTickSubsystem* Tick = GetTick())
		{
			Tick->AdjustNationPublicOrder(Iso, -1);
		}
		Region->LastReport = TEXT("Operacion de seguridad aumenta control y tensiona derechos.");
		break;
	}
	OutMessage = Region->LastReport;
	return true;
}

TArray<FWLCrisisChainState> UWLPoliticalSubsystem::GetActiveCrisisChains(const FString& NationIso) const
{
	TArray<FWLCrisisChainState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLCrisisChainState& Crisis : CrisisChains)
	{
		if (!Crisis.bResolved && (Iso.IsEmpty() || Crisis.NationIso == Iso))
		{
			Out.Add(Crisis);
		}
	}
	Out.Sort([](const FWLCrisisChainState& A, const FWLCrisisChainState& B)
	{
		return A.Intensity == B.Intensity ? A.CrisisId < B.CrisisId : A.Intensity > B.Intensity;
	});
	return Out;
}

FWLGovernmentCalibrationState UWLPoliticalSubsystem::GetGovernmentCalibration(const FString& NationIso) const
{
	if (const FWLGovernmentCalibrationState* Found = GovernmentCalibrationByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLGovernmentCalibrationState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

void UWLPoliticalSubsystem::UpdateInternalPowerForNation(const FString& NationIso)
{
	FWLInternalPowerState& State = EnsureInternalPower(NationIso);
	State.AveragePublicOrder = GetAveragePublicOrder(NationIso);

	const int32 DisorderPressure = FMath::Max(0, 65 - State.AveragePublicOrder) / 2;
	State.OppositionStrength = ClampPercent(State.OppositionStrength + DisorderPressure - 2);
	State.OppositionPopularity = ClampPercent(State.OppositionPopularity + DisorderPressure - 1);

	int32 GeneralPressure = 0;
	int32 GeneralCount = 0;
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		for (const FWLCharacter& General : Characters->GetGenerals(State.NationIso))
		{
			GeneralPressure += FMath::Max(0, General.Ambition - General.Loyalty / 2);
			++GeneralCount;
		}
	}
	const int32 AverageGeneralPressure = GeneralCount > 0 ? GeneralPressure / GeneralCount : 0;
	const int32 PublicOrderRisk = FMath::Max(0, 60 - State.AveragePublicOrder);
	State.CoupRisk = ClampPercent(
		AverageGeneralPressure / 2
		+ PublicOrderRisk
		+ State.OppositionStrength / 3
		+ State.ExternalCoupFunding / 2
		+ GetLeaderAgendaPressure(State.NationIso));
}

FWLInternalPowerState UWLPoliticalSubsystem::GetInternalPower(const FString& NationIso) const
{
	if (const FWLInternalPowerState* Found = InternalPowerByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLInternalPowerState State;
	State.NationIso = NormalizeIso(NationIso);
	State.AveragePublicOrder = GetAveragePublicOrder(NationIso);
	return State;
}

void UWLPoliticalSubsystem::ProcessPoliticalMonth()
{
	const UWLDataRegistry* Registry = GetRegistry();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Registry)
	{
		return;
	}

	const FString PlayerIso = GetPlayerNationIso();
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		const bool bIsAI = !PlayerIso.IsEmpty() && Nation.Iso != PlayerIso;
		ProcessGovernmentMonthForNation(Nation.Iso, bIsAI);
		if (Characters)
		{
			Characters->AddMonthlyRenownToGenerals(Nation.Iso, 1);
		}
		UpdateInternalPowerForNation(Nation.Iso);
		QueueTriggeredEventsForNation(Nation.Iso);
		if (bIsAI)
		{
			AutoResolveEventsForAI(Nation.Iso);   // la IA no deja eventos pudriendose en cola
			RunStrategicAIForNation(Nation.Iso);  // la "computadora" juega: fisco, diplomacia, intriga, reclutamiento
		}
		FWLInternalPowerState& State = EnsureInternalPower(Nation.Iso);
		if (!CampaignOutcome.bGameOver && State.CoupRisk >= CoupAttemptRiskThreshold)
		{
			FString Report;
			AttemptCoup(Nation.Iso, Report);
		}
	}

	// Las redes de espionaje se enfrian con el tiempo: sin operaciones nuevas, la exposicion baja.
	for (TPair<FString, FWLIntelligenceNetworkState>& Pair : IntelligenceByPair)
	{
		Pair.Value.Exposure = ClampPercent(Pair.Value.Exposure - 4);
	}

	// Fase 3 auditoria: un buen ministro de Exterior mejora la opinion con cada pais mes a mes
	// (uno inepto la erosiona). Ambos lados de la relacion aplican su deriva.
	if (Characters)
	{
		const UWLStrategicTickSubsystem* Tick = GetTick();
		const FWLBalanceRules Rules = Tick ? Tick->GetBalanceRules() : FWLBalanceRules::Default();
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			const int32 OpinionDrift = FMath::RoundToInt(
				Characters->GetMinisterEffectFactor(Nation.Iso, EWLMinisterOffice::Foreign)
				* Rules.ForeignMinisterOpinionPerMonth);
			if (OpinionDrift == 0)
			{
				continue;
			}
			for (TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
			{
				FWLDiplomaticRelationState& Relation = Pair.Value;
				if (Relation.NationA != Nation.Iso && Relation.NationB != Nation.Iso)
				{
					continue;
				}
				Relation.Opinion = FMath::Clamp(Relation.Opinion + OpinionDrift, -100, 100);
				if (Relation.Status != EWLDiplomaticStatus::War)
				{
					Relation.Status = Relation.Opinion < -35 ? EWLDiplomaticStatus::Tension : EWLDiplomaticStatus::Peace;
				}
			}
		}
	}

	CheckCampaignOutcome();
}

void UWLPoliticalSubsystem::ProcessGovernmentMonthForNation(const FString& NationIso, bool bRunAI)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		return;
	}
	if (bRunAI)
	{
		RunGovernmentAIForNation(Iso);
	}
	ApplyGovernmentAgendaMonthly(Iso);
	ApplyMinistryProgramsMonthly(Iso);
	ApplyPolicyReformsMonthly(Iso);
	UpdateCharacterProfilesForNation(Iso);
	UpdatePatronageForNation(Iso);
	UpdateMediaForNation(Iso);
	UpdatePoliticalPartiesForNation(Iso);
	UpdateCabinetDynamicsForNation(Iso);
	UpdateInstitutionalPowerForNation(Iso);
	UpdatePublicGroupsForNation(Iso);
	UpdateStateCapacityForNation(Iso);
	UpdateRegionsForNation(Iso);
	UpdateElectionForNation(Iso);
	AdvancePoliticalMemoryForNation(Iso);
	QueueGovernmentCrisisEventsForNation(Iso);
	UpdateCrisisChainsForNation(Iso);
	UpdateGovernmentCalibrationForNation(Iso);
}

void UWLPoliticalSubsystem::ApplyGovernmentAgendaMonthly(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLGovernmentAgendaState& Agenda = EnsureGovernmentAgenda(Iso);
	++Agenda.MonthsActive;

	UWLStrategicTickSubsystem* Tick = GetTick();
	FWLInternalPowerState& Internal = EnsureInternalPower(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);

	for (EWLGovernmentPriority Priority : Agenda.Priorities)
	{
		switch (Priority)
		{
		case EWLGovernmentPriority::Security:
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 2, TEXT("agenda seguridad"));
			Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 1);
			if (Tick) { Tick->AdjustNationPublicOrder(Iso, 1); }
			break;
		case EWLGovernmentPriority::Growth:
			if (Tick)
			{
				FString TreasuryMessage;
				if (TrySpendTreasury(Iso, 600, TEXT("agenda crecimiento"), TreasuryMessage))
				{
					AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 1, TEXT("agenda crecimiento"));
					AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 1, TEXT("agenda crecimiento"));
					Tick->AdjustNationPublicOrder(Iso, 1);
				}
			}
			break;
		case EWLGovernmentPriority::Austerity:
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 1, TEXT("agenda austeridad"));
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -2, TEXT("agenda austeridad"));
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -2, TEXT("agenda austeridad"));
			Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength + 1);
			if (Tick)
			{
				FString TreasuryMessage;
				Tick->AdjustTreasury(Iso, 900, TreasuryMessage);
				Tick->SetTaxRate(Iso, Tick->GetTaxRate(Iso) + 1);
			}
			break;
		case EWLGovernmentPriority::Industrialization:
			if (Tick)
			{
				FString TreasuryMessage;
				if (TrySpendTreasury(Iso, 800, TEXT("agenda industrial"), TreasuryMessage))
				{
					AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 2, TEXT("agenda industrial"));
					AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 1, TEXT("agenda industrial"));
					Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + 1);
				}
			}
			break;
		case EWLGovernmentPriority::Diplomacy:
			for (TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
			{
				FWLDiplomaticRelationState& Relation = Pair.Value;
				if (Relation.NationA == Iso || Relation.NationB == Iso)
				{
					Relation.Opinion = FMath::Clamp(Relation.Opinion + 1, -100, 100);
				}
			}
			Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport + 1);
			break;
		case EWLGovernmentPriority::Control:
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 1, TEXT("agenda control"));
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -1, TEXT("agenda control"));
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -1, TEXT("agenda control"));
			Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 2);
			Capacity.CentralAuthority = ClampPercent(Capacity.CentralAuthority + 2);
			if (Tick) { Tick->AdjustNationPublicOrder(Iso, -1); }
			break;
		default:
			break;
		}
	}
	Agenda.LastAgendaReport = FString::Printf(TEXT("%s ejecuta agenda de gobierno (%d prioridades)."),
		*Iso, Agenda.Priorities.Num());
}

void UWLPoliticalSubsystem::ApplyMinistryProgramsMonthly(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	for (int32 Index = ActiveMinistryPrograms.Num() - 1; Index >= 0; --Index)
	{
		FWLMinistryProgramState& Program = ActiveMinistryPrograms[Index];
		if (Program.NationIso != Iso || Program.RemainingMonths <= 0)
		{
			continue;
		}

		FWLMinistryProgramDefinition Definition;
		if (!GetMinistryProgramDefinition(Program.ProgramId, Definition))
		{
			ActiveMinistryPrograms.RemoveAt(Index);
			continue;
		}

		const int32 MinisterModifier = GetMinisterProgramModifier(Iso, Program.Office);
		const FWLStateCapacityState Capacity = GetStateCapacity(Iso);
		const int32 Risk = FMath::Clamp(Capacity.PolicyFailureRisk - MinisterModifier, 0, 90);
		const int32 Roll = static_cast<int32>((GetTypeHash(Iso + Program.ProgramId) + Program.RemainingMonths * 17) % 100);
		if (Roll < Risk / 3)
		{
			Program.bBlocked = true;
			Program.LastReport = FString::Printf(TEXT("%s se atasca por baja capacidad estatal (riesgo %d)."),
				*Program.Name, Risk);
			AddPoliticalMemory(Iso, TEXT("policy_failure"), 1, 8, Program.LastReport);
		}
		else
		{
			Program.bBlocked = false;
			ApplyProgramEffect(Iso, Definition, Program);
			Program.Progress = FMath::Clamp(
				Program.Progress + 25 + FMath::Max(0, MinisterModifier / 2) - FMath::Max(0, Risk / 10),
				0,
				100);
		}

		--Program.RemainingMonths;
		if (Program.Progress >= 100 || Program.RemainingMonths <= 0)
		{
			Program.LastReport = FString::Printf(TEXT("%s completa %s."), *Iso, *Program.Name);
			AddPoliticalMemory(Iso, TEXT("program_completed"), 1, 10, Program.LastReport);
			ActiveMinistryPrograms.RemoveAt(Index);
		}
	}
}

void UWLPoliticalSubsystem::ApplyPolicyReformsMonthly(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	for (int32 Index = ActivePolicyReforms.Num() - 1; Index >= 0; --Index)
	{
		FWLActiveReformState& Reform = ActivePolicyReforms[Index];
		if (Reform.NationIso != Iso || Reform.MonthsRemaining <= 0)
		{
			continue;
		}

		FWLPolicyReformDefinition Definition;
		if (!GetPolicyReformDefinition(Reform.ReformId, Definition))
		{
			ActivePolicyReforms.RemoveAt(Index);
			continue;
		}

		FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
		FWLElectionState& Election = EnsureElectionState(Iso);
		if (UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (Definition.MonthlyTreasuryDelta != 0)
			{
				FString TreasuryMessage;
				Tick->AdjustTreasury(Iso, Definition.MonthlyTreasuryDelta, TreasuryMessage);
			}
		}
		const int32 MonthlyCapacity = FMath::Clamp(Definition.CapacityDelta, -3, 3);
		const int32 MonthlyCorruption = FMath::Clamp(Definition.CorruptionDelta, -3, 3);
		Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + MonthlyCapacity);
		Capacity.Corruption = ClampPercent(Capacity.Corruption + MonthlyCorruption);
		Election.Legitimacy = ClampPercent(Election.Legitimacy + FMath::Clamp(Definition.LegitimacyDelta, -2, 2));
		Reform.ImplementationProgress = ClampPercent(Reform.ImplementationProgress + FMath::Max(4, Capacity.AdministrativeEfficiency / 12));
		Reform.Backlash = ClampPercent(Reform.Backlash - 2 + EnsureInstitutionalPower(Iso).GridlockRisk / 30);
		--Reform.MonthsRemaining;
		Reform.LastReport = FString::Printf(TEXT("%s implementa %s: progreso %d, backlash %d."),
			*Iso, *Reform.Name, Reform.ImplementationProgress, Reform.Backlash);
		if (Reform.Backlash >= 70)
		{
			TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, Reform.Backlash, Reform.LastReport);
		}
		if (Reform.MonthsRemaining <= 0)
		{
			const FString Report = FString::Printf(TEXT("%s queda consolidada."), *Reform.Name);
			RecordEnactedPolicyReform(Iso, Definition, Report);
			AddPoliticalMemory(Iso, ReformMemoryKey(Reform.ReformId), 1, 84, Report);
			ActivePolicyReforms.RemoveAt(Index);
		}
	}

	for (FWLEnactedPolicyReformState& Reform : EnactedPolicyReforms)
	{
		if (Reform.NationIso == Iso)
		{
			++Reform.MonthsSinceEnacted;
		}
	}
}

void UWLPoliticalSubsystem::UpdatePoliticalPartiesForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	SeedPoliticalPartiesForNation(Iso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	int32 CoalitionSeats = 0;
	int32 CoalitionLoyalty = 0;
	int32 CoalitionParties = 0;
	int32 OppositionSeats = 0;
	for (FWLPartyState& Party : PoliticalParties)
	{
		if (Party.NationIso != Iso)
		{
			continue;
		}
		++Party.MonthsSinceInternalElection;
		if (Party.bInCoalition || Party.Role == EWLPartyRole::Ruling || Party.Role == EWLPartyRole::Ally)
		{
			CoalitionSeats += Party.Seats;
			CoalitionLoyalty += Party.LoyaltyToGovernment;
			++CoalitionParties;
		}
		else
		{
			OppositionSeats += Party.Seats;
		}

		const int32 BetrayalRoll = static_cast<int32>((GetTypeHash(Iso + Party.PartyId) + Party.MonthsSinceInternalElection * 13) % 100);
		if (Party.bInCoalition && Party.Role != EWLPartyRole::Ruling
			&& Party.LoyaltyToGovernment < 28
			&& BetrayalRoll < 18)
		{
			Party.bInCoalition = false;
			Party.Role = EWLPartyRole::SoftOpposition;
			Party.LastIncident = FString::Printf(TEXT("%s rompe con el gobierno."), *Party.Name);
			AddPoliticalMemory(Iso, TEXT("party_betrayal"), 1, 10, Party.LastIncident);
			TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::Impeachment, 35, Party.LastIncident);
		}
		else if (Party.MonthsSinceInternalElection >= 24)
		{
			Party.Discipline = ClampPercent(Party.Discipline - 1);
			Party.LoyaltyToGovernment = ClampPercent(Party.LoyaltyToGovernment - (Party.Role == EWLPartyRole::Ruling ? 0 : 1));
		}
	}
	const int32 AvgCoalitionLoyalty = CoalitionParties > 0 ? CoalitionLoyalty / CoalitionParties : 35;
	Institutions.RulingCoalitionSupport = ClampPercent((CoalitionSeats + AvgCoalitionLoyalty) / 2);
	Institutions.LegislativeOpposition = ClampPercent(OppositionSeats + FMath::Max(0, 60 - AvgCoalitionLoyalty) / 2);
}

void UWLPoliticalSubsystem::UpdateElectionForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLElectionState& Election = EnsureElectionState(Iso);
	FWLMediaPublicOpinionState& Media = EnsureMediaState(Iso);
	const FWLInstitutionalPowerState Institutions = GetInstitutionalPower(Iso);
	const FWLPatronageState Patronage = GetPatronageState(Iso);
	int32 AverageGroupSupport = 50;
	const TArray<FWLPublicGroupSupportState> Groups = GetPublicGroups(Iso);
	if (!Groups.IsEmpty())
	{
		int32 Sum = 0;
		for (const FWLPublicGroupSupportState& Group : Groups)
		{
			Sum += Group.Support - Group.Pressure / 3;
		}
		AverageGroupSupport = FMath::Clamp(Sum / Groups.Num(), 0, 100);
	}

	Election.IncumbentApproval = ClampPercent((AverageGroupSupport + Media.PresidentialApproval + Institutions.RulingCoalitionSupport) / 3);
	Election.FraudRisk = ClampPercent(EnsureStateCapacity(Iso).Corruption / 2 + Media.MediaControl / 4 + Patronage.ClientelistPressure / 3);
	Election.AbstentionRisk = ClampPercent(35 - Election.Legitimacy / 3 + Media.FakeNewsPressure / 3 + Media.CensorshipBacklash / 4);
	Election.PollingGovernment = ClampPercent(Election.IncumbentApproval + Election.CampaignIntensity / 4 + Patronage.PatronagePower / 8);
	Election.PollingOpposition = ClampPercent(100 - Election.PollingGovernment + GetInternalPower(Iso).OppositionPopularity / 5);

	if (Election.MonthsToElection > 0)
	{
		--Election.MonthsToElection;
	}
	if (Election.MonthsToElection <= 12 && Election.MonthsToElection > 6)
	{
		Election.Phase = EWLElectionPhase::PreCampaign;
	}
	else if (Election.MonthsToElection <= 6 && Election.MonthsToElection > 0)
	{
		Election.Phase = EWLElectionPhase::Campaign;
		Election.CampaignIntensity = ClampPercent(Election.CampaignIntensity + 2);
	}
	else if (Election.MonthsToElection <= 0)
	{
		Election.Phase = EWLElectionPhase::Election;
		int32 GovernmentScore = Election.PollingGovernment + Election.Legitimacy / 5 - Election.FraudRisk / 3;
		int32 OppositionScore = Election.PollingOpposition + Election.AbstentionRisk / 4;
		FString ElectionNote;
		if (!Election.CampaignPromiseReformId.IsEmpty())
		{
			if (Election.bCampaignPromiseFulfilled || HasReformMemory(Iso, Election.CampaignPromiseReformId))
			{
				GovernmentScore += 8;
				Election.Legitimacy = ClampPercent(Election.Legitimacy + 3);
				ElectionNote += TEXT(" Promesa cumplida.");
			}
			else
			{
				GovernmentScore -= 12;
				OppositionScore += 5;
				Election.Legitimacy = ClampPercent(Election.Legitimacy - 6);
				ElectionNote += TEXT(" Promesa incumplida.");
				AddPoliticalMemory(Iso, TEXT("campaign_promise_broken"), 1, 24,
					FString::Printf(TEXT("%s no cumplio la promesa %s antes de la eleccion."),
						*Iso,
						*Election.CampaignPromiseReformId));
			}
		}

		const bool bConstitutionalTermRules = HasReformMemory(Iso, TEXT("constitution_term_rules"));
		if (Election.bTermLimited && !bConstitutionalTermRules)
		{
			GovernmentScore -= 18;
			OppositionScore += 8;
			ElectionNote += TEXT(" Limite de mandato fuerza candidato sucesor.");
			AddPoliticalMemory(Iso, TEXT("term_limit_pressure"), 1, 18,
				FString::Printf(TEXT("%s llega a eleccion con limite de mandato."), *Iso));
		}
		Election.bLastElectionWon = GovernmentScore >= OppositionScore;
		if (Election.bLastElectionWon)
		{
			Election.ConsecutiveTermsWon = FMath::Max(1, Election.ConsecutiveTermsWon + 1);
			Election.LastElectionReport = FString::Printf(TEXT("%s gana la eleccion (%d vs %d).%s"),
				*Iso, GovernmentScore, OppositionScore, *ElectionNote);
			Election.Legitimacy = ClampPercent(Election.Legitimacy + 8 - Election.FraudRisk / 8);
			Election.Phase = EWLElectionPhase::Transition;
		}
		else
		{
			Election.ConsecutiveTermsWon = 0;
			Election.LastElectionReport = FString::Printf(TEXT("%s pierde la eleccion (%d vs %d).%s"),
				*Iso, GovernmentScore, OppositionScore, *ElectionNote);
			Election.Legitimacy = ClampPercent(Election.Legitimacy - 10);
			Election.Phase = Election.FraudRisk >= 55 ? EWLElectionPhase::Crisis : EWLElectionPhase::Transition;
			TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::SoftCoup, 45, Election.LastElectionReport);
			const FString PlayerIso = GetPlayerNationIso();
			if (!PlayerIso.IsEmpty() && PlayerIso == Iso)
			{
				CampaignOutcome.bGameOver = true;
				CampaignOutcome.OutcomeType = TEXT("ElectionDefeat");
				CampaignOutcome.LosingNationIso = Iso;
				CampaignOutcome.Reason = Election.LastElectionReport;
			}
		}
		const int32 MaxConsecutiveTerms = bConstitutionalTermRules ? 3 : 2;
		Election.bTermLimited = Election.ConsecutiveTermsWon >= MaxConsecutiveTerms;
		Election.MonthsToElection = ElectionCycleMonths;
		Election.CampaignIntensity = 0;
		Election.CampaignPromiseReformId.Reset();
		Election.bCampaignPromiseFulfilled = false;
		AddNews(Election.LastElectionReport);
	}
	else
	{
		Election.Phase = EWLElectionPhase::Governing;
		Election.LastElectionReport = FString::Printf(TEXT("%s gobierna; eleccion en %d meses."), *Iso, Election.MonthsToElection);
	}
}

void UWLPoliticalSubsystem::UpdateCharacterProfilesForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	SeedCharacterProfilesForNation(Iso);
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		for (const FWLCharacter& Character : Characters->GetCharactersByNation(Iso))
		{
			FWLCharacterPoliticalProfile& Profile = EnsureCharacterPoliticalProfile(Character);
			Profile.PresidentialAmbition = ClampPercent(Character.Ambition + Character.Popularity / 5 + Character.Renown / 5);
			Profile.PersonalCorruption = ClampPercent(Profile.PersonalCorruption + (Character.Traits.Contains(TEXT("corrupto")) ? 2 : -1));
			Profile.ScandalHeat = ClampPercent(Profile.ScandalHeat + Profile.PersonalCorruption / 25 + EnsurePatronageState(Iso).ContractCorruption / 30 - 2);
			Profile.SuccessionScore = ClampPercent(Character.Popularity / 2 + Character.Renown / 3 + Profile.PresidentialAmbition / 4 - Profile.ScandalHeat / 5);
			if (Character.Role == EWLCharacterRole::Minister && Character.AssignedOffice != EWLMinisterOffice::None)
			{
				Profile.SuccessionScore = ClampPercent(Profile.SuccessionScore + Character.Skill / 8 + Character.Loyalty / 12);
				Profile.LastProfileEvent = FString::Printf(TEXT("%s gana peso politico desde %s."),
					*Character.Name,
					*UWLCharacterSubsystem::MinisterOfficeToString(Character.AssignedOffice));
			}
			else if (Character.Role == EWLCharacterRole::Opposition)
			{
				Profile.SuccessionScore = ClampPercent(Profile.SuccessionScore + GetInternalPower(Iso).OppositionPopularity / 8);
			}
			if (Profile.ScandalHeat >= 70)
			{
				Profile.LastProfileEvent = FString::Printf(TEXT("%s acumula expediente de escandalo."), *Character.Name);
				AddPoliticalMemory(Iso, TEXT("personal_scandal"), 1, 8, Profile.LastProfileEvent);
				TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::CorruptionScandal, Profile.ScandalHeat, Profile.LastProfileEvent);
			}
		}
	}
}

void UWLPoliticalSubsystem::UpdatePatronageForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPatronageState& Patronage = EnsurePatronageState(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower - 1 + Patronage.RegionalMachines / 35);
	Patronage.ClientelistPressure = ClampPercent(Patronage.ClientelistPressure - 1 + Patronage.PatronagePower / 40);
	Patronage.ConcessionBacklash = ClampPercent(Patronage.ConcessionBacklash - 1);
	Capacity.Corruption = ClampPercent(Capacity.Corruption + Patronage.ContractCorruption / 40 + Patronage.ClientelistPressure / 60);
	if (Patronage.ContractCorruption + Patronage.ConcessionBacklash >= 85)
	{
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::CorruptionScandal,
			Patronage.ContractCorruption + Patronage.ConcessionBacklash,
			TEXT("Contratos y concesiones alimentan escandalo de patronazgo."));
	}
}

void UWLPoliticalSubsystem::UpdateMediaForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLMediaPublicOpinionState& Media = EnsureMediaState(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);
	Media.PresidentialApproval = ClampPercent(
		Media.PresidentialApproval
		+ (GetAveragePublicOrder(Iso) - 55) / 15
		+ Media.PropagandaReach / 40
		- Internal.OppositionStrength / 35
		- Media.CensorshipBacklash / 35);
	Media.FakeNewsPressure = ClampPercent(Media.FakeNewsPressure - 1 + Internal.OppositionPopularity / 45);
	Media.MediaCrisisRisk = ClampPercent(Internal.OppositionPopularity / 2 + Media.FakeNewsPressure / 2 + Media.CensorshipBacklash / 2 - Media.PressFreedom / 5);
	Media.PropagandaReach = ClampPercent(Media.PropagandaReach - 2);
	Media.CensorshipBacklash = ClampPercent(Media.CensorshipBacklash - 1 + FMath::Max(0, Media.MediaControl - 65) / 20);
	if (Media.MediaCrisisRisk >= 70)
	{
		Media.LastNarrative = FString::Printf(TEXT("%s enfrenta crisis mediatica."), *Iso);
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, Media.MediaCrisisRisk, Media.LastNarrative);
	}
	else
	{
		Media.LastNarrative = FString::Printf(TEXT("Aprobacion %d, crisis mediatica %d."),
			Media.PresidentialApproval, Media.MediaCrisisRisk);
	}
}

void UWLPoliticalSubsystem::UpdateRegionsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	SeedRegionsForNation(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	for (FWLRegionGovernorState& Region : RegionGovernors)
	{
		if (Region.NationIso != Iso)
		{
			continue;
		}
		const int32 Authority = Capacity.CentralAuthority;
		Region.Obedience = ClampPercent(Region.Obedience + (Authority - 50) / 20 - Region.Autonomy / 35 + Region.InvestmentLevel / 30);
		Region.ProtestRisk = ClampPercent(Region.ProtestRisk + FMath::Max(0, 55 - Region.Obedience) / 8 - Region.InvestmentLevel / 30);
		Region.SecessionRisk = ClampPercent(Region.Autonomy / 2 + FMath::Max(0, 45 - Region.Obedience) + Region.ProtestRisk / 3 - Region.CenterControl / 3);
		Region.RebellionRisk = ClampPercent(Region.ProtestRisk / 2 + Region.SecessionRisk / 2 - Authority / 5);
		Region.InvestmentLevel = ClampPercent(Region.InvestmentLevel - 1);
		Region.LastReport = FString::Printf(TEXT("%s: obediencia %d, protesta %d, secesion %d."),
			*Region.RegionName, Region.Obedience, Region.ProtestRisk, Region.SecessionRisk);
		if (Region.RebellionRisk >= 70)
		{
			TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, Region.RebellionRisk, Region.LastReport);
		}
	}
}

FWLCrisisChainState& UWLPoliticalSubsystem::EnsureCrisisChain(
	const FString& NationIso,
	EWLCrisisChainType Type,
	const FString& Reason)
{
	const FString Iso = NormalizeIso(NationIso);
	const FString Key = CrisisChainKey(Iso, Type);
	for (FWLCrisisChainState& Crisis : CrisisChains)
	{
		if (!Crisis.bResolved && Crisis.CrisisId == Key)
		{
			return Crisis;
		}
	}

	FWLCrisisChainState Crisis;
	Crisis.NationIso = Iso;
	Crisis.CrisisId = Key;
	Crisis.Type = Type;
	Crisis.Stage = 1;
	Crisis.Intensity = 20;
	Crisis.LastReport = Reason;
	CrisisChains.Add(Crisis);
	return CrisisChains.Last();
}

void UWLPoliticalSubsystem::TriggerGovernmentP2Crisis(
	const FString& NationIso,
	EWLCrisisChainType Type,
	int32 Intensity,
	const FString& Reason)
{
	FWLCrisisChainState& Crisis = EnsureCrisisChain(NationIso, Type, Reason);
	Crisis.Intensity = ClampPercent(Crisis.Intensity + FMath::Max(5, Intensity / 4));
	Crisis.LastReport = Reason;
	AddPoliticalMemory(NationIso, TEXT("p2_crisis"), 1, 8, Reason);
}

void UWLPoliticalSubsystem::UpdateCrisisChainsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	for (FWLCrisisChainState& Crisis : CrisisChains)
	{
		if (Crisis.NationIso != Iso || Crisis.bResolved)
		{
			continue;
		}
		++Crisis.MonthsActive;
		Crisis.Intensity = ClampPercent(Crisis.Intensity + GetInternalPower(Iso).OppositionStrength / 30 + EnsureMediaState(Iso).MediaCrisisRisk / 40 - EnsureStateCapacity(Iso).AdministrativeEfficiency / 35);
		if (Crisis.Intensity >= 80)
		{
			Crisis.Stage = FMath::Min(4, Crisis.Stage + 1);
			if (UWLStrategicTickSubsystem* Tick = GetTick())
			{
				Tick->AdjustNationPublicOrder(Iso, -2);
			}
			EnsureInternalPower(Iso).OppositionStrength = ClampPercent(EnsureInternalPower(Iso).OppositionStrength + 3);
		}
		else if (Crisis.Intensity <= 20 && Crisis.MonthsActive > 2)
		{
			Crisis.bResolved = true;
			Crisis.LastReport = FString::Printf(TEXT("Crisis %s resuelta por desgaste."), *Crisis.CrisisId);
			continue;
		}
		Crisis.LastReport = FString::Printf(TEXT("Crisis %s etapa %d intensidad %d."),
			*Crisis.CrisisId, Crisis.Stage, Crisis.Intensity);
		if (Crisis.Stage >= 4)
		{
			if (Crisis.Type == EWLCrisisChainType::Impeachment || Crisis.Type == EWLCrisisChainType::SoftCoup)
			{
				EnsureInstitutionalPower(Iso).GridlockRisk = ClampPercent(EnsureInstitutionalPower(Iso).GridlockRisk + 8);
				EnsureInternalPower(Iso).CoupRisk = ClampPercent(EnsureInternalPower(Iso).CoupRisk + 8);
			}
			else if (Crisis.Type == EWLCrisisChainType::DebtCrisis)
			{
				EnsureMediaState(Iso).PresidentialApproval = ClampPercent(EnsureMediaState(Iso).PresidentialApproval - 5);
			}
		}
	}
}

void UWLPoliticalSubsystem::UpdateGovernmentCalibrationForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLGovernmentCalibrationState& Calibration = EnsureGovernmentCalibration(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);
	const FWLInstitutionalPowerState Institutions = GetInstitutionalPower(Iso);
	const FWLMediaPublicOpinionState Media = GetMediaPublicOpinion(Iso);
	const FWLPatronageState Patronage = GetPatronageState(Iso);
	const FWLStateCapacityState Capacity = GetStateCapacity(Iso);
	++Calibration.MonthsObserved;
	if (const UWLStrategicTickSubsystem* Tick = GetTick())
	{
		const int64 ClampedTreasury = FMath::Clamp(Tick->GetTreasury(Iso), static_cast<int64>(0), static_cast<int64>(50000));
		Calibration.DebtVsGrowthPressure = ClampPercent(
			(Tick->GetMonthlyBalance(Iso) < 0 ? 35 : 10)
			+ static_cast<int32>((50000 - ClampedTreasury) / 2000));
		Calibration.SubsidyInflationPressure = ClampPercent(GetPoliticalMemoryValue(Iso, TEXT("program_started")) * 4 + FMath::Max(0, 45 - Tick->GetTaxRate(Iso)));
	}
	Calibration.RepressionLegitimacyPressure = ClampPercent(GetPoliticalMemoryValue(Iso, TEXT("recent_repression")) * 18 + FMath::Max(0, 50 - Media.PresidentialApproval));
	Calibration.ReformGridlockPressure = ClampPercent(Institutions.GridlockRisk + GetPoliticalMemoryValue(Iso, TEXT("reform_blocked")) * 10);
	bool bLowMilitarySupport = false;
	for (const FWLPublicGroupSupportState& Group : GetPublicGroups(Iso))
	{
		if (Group.Group == EWLPublicGroup::Military && Group.Support < 45)
		{
			bLowMilitarySupport = true;
			break;
		}
	}
	Calibration.CivilMilitaryTension = ClampPercent((bLowMilitarySupport ? 35 : 10) + Internal.CoupRisk / 2);
	Calibration.SovereigntyAlliancePressure = ClampPercent(Patronage.ConcessionBacklash + FMath::Max(0, 45 - Capacity.CentralAuthority));
	Calibration.LastReport = FString::Printf(TEXT("Calibracion %s: deuda %d, represion %d, gridlock %d, civil-militar %d."),
		*Iso,
		Calibration.DebtVsGrowthPressure,
		Calibration.RepressionLegitimacyPressure,
		Calibration.ReformGridlockPressure,
		Calibration.CivilMilitaryTension);
}

void UWLPoliticalSubsystem::UpdateCabinetDynamicsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLCabinetDynamicsState& Dynamics = EnsureCabinetDynamics(Iso);
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters)
	{
		return;
	}

	int32 AmbitionPressure = 0;
	int32 LowLoyaltyPressure = 0;
	int32 CorruptTraits = 0;
	int32 Ministers = 0;
	FWLCharacter LowestLoyaltyMinister;
	bool bHasLowest = false;
	for (const FWLCabinetSeat& Seat : Characters->GetCabinet(Iso))
	{
		if (Seat.CharacterId.IsEmpty())
		{
			continue;
		}
		const FWLCharacter& Minister = Seat.Minister;
		AmbitionPressure += FMath::Max(0, Minister.Ambition - 45);
		LowLoyaltyPressure += FMath::Max(0, 55 - Minister.Loyalty);
		if (Minister.Traits.Contains(TEXT("corrupto")) || Minister.Traits.Contains(TEXT("clientelista")))
		{
			++CorruptTraits;
		}
		if (!bHasLowest || Minister.Loyalty < LowestLoyaltyMinister.Loyalty)
		{
			LowestLoyaltyMinister = Minister;
			bHasLowest = true;
		}
		++Ministers;
	}

	const int32 AvgAmbition = Ministers > 0 ? AmbitionPressure / Ministers : 0;
	const int32 AvgLowLoyalty = Ministers > 0 ? LowLoyaltyPressure / Ministers : 0;
	Dynamics.RivalryPressure = ClampPercent(AvgAmbition + AvgLowLoyalty / 2);
	Dynamics.Factionalism = ClampPercent(Dynamics.RivalryPressure + CorruptTraits * 8);
	Dynamics.ScandalRisk = ClampPercent(Dynamics.Factionalism / 2 + CorruptTraits * 12 + GetStateCapacity(Iso).Corruption / 3);
	Dynamics.SabotageRisk = ClampPercent(Dynamics.RivalryPressure + AvgLowLoyalty);
	Dynamics.ResignationRisk = ClampPercent(AvgLowLoyalty + Dynamics.ScandalRisk / 2);

	if (Dynamics.ScandalRisk >= 75)
	{
		Dynamics.LastIncident = FString::Printf(TEXT("%s enfrenta rumores de escandalo de gabinete."), *Iso);
		AddPoliticalMemory(Iso, TEXT("cabinet_scandal"), 1, 8, Dynamics.LastIncident);
		EnsureInternalPower(Iso).OppositionStrength = ClampPercent(EnsureInternalPower(Iso).OppositionStrength + 2);
	}
	else if (Dynamics.SabotageRisk >= 70)
	{
		Dynamics.LastIncident = FString::Printf(TEXT("%s detecta sabotaje burocratico entre ministros."), *Iso);
		AddPoliticalMemory(Iso, TEXT("cabinet_sabotage"), 1, 6, Dynamics.LastIncident);
		for (FWLMinistryProgramState& Program : ActiveMinistryPrograms)
		{
			if (Program.NationIso == Iso)
			{
				Program.bBlocked = true;
				Program.LastReport = Dynamics.LastIncident;
				break;
			}
		}
	}
	else if (Dynamics.ResignationRisk >= 95 && bHasLowest)
	{
		if (UWLCharacterSubsystem* MutableCharacters = GetCharacters())
		{
			FString RetireMessage;
			if (MutableCharacters->RetireCharacter(LowestLoyaltyMinister.Id, RetireMessage))
			{
				Dynamics.LastIncident = FString::Printf(TEXT("%s renuncia al gabinete de %s."),
					*LowestLoyaltyMinister.Name, *Iso);
				AddPoliticalMemory(Iso, TEXT("minister_resigned"), 1, 10, Dynamics.LastIncident);
			}
		}
	}
	else
	{
		Dynamics.LastIncident = TEXT("Gabinete operativo.");
	}
}

void UWLPoliticalSubsystem::UpdateInstitutionalPowerForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);

	int32 AverageGroupSupport = 50;
	const TArray<FWLPublicGroupSupportState> Groups = GetPublicGroups(Iso);
	if (!Groups.IsEmpty())
	{
		int32 Sum = 0;
		for (const FWLPublicGroupSupportState& Group : Groups)
		{
			Sum += Group.Support;
		}
		AverageGroupSupport = Sum / Groups.Num();
	}

	int32 CoalitionSeats = 0;
	int32 OppositionSeats = 0;
	for (const FWLPartyState& Party : GetPoliticalParties(Iso))
	{
		if (Party.bInCoalition || Party.Role == EWLPartyRole::Ruling || Party.Role == EWLPartyRole::Ally)
		{
			CoalitionSeats += Party.Seats;
		}
		else
		{
			OppositionSeats += Party.Seats;
		}
	}
	const FWLPatronageState Patronage = GetPatronageState(Iso);

	Institutions.RulingCoalitionSupport = ClampPercent(
		Institutions.RulingCoalitionSupport
		+ (AverageGroupSupport - 50) / 12
		+ (CoalitionSeats - 50) / 10
		+ Patronage.PatronagePower / 35
		- Internal.OppositionStrength / 40);
	Institutions.LegislativeOpposition = ClampPercent(
		100 - Institutions.RulingCoalitionSupport + Internal.OppositionStrength / 5 + OppositionSeats / 12);
	Institutions.GridlockRisk = ClampPercent(
		Institutions.LegislativeOpposition - Institutions.RulingCoalitionSupport / 2
		+ GetPoliticalMemoryValue(Iso, TEXT("reform_blocked")) * 10
		- Patronage.ClientelistPressure / 8);
	Institutions.ReformCost = FMath::Clamp(8 + Institutions.GridlockRisk / 5, 6, 30);
}

void UWLPoliticalSubsystem::UpdateStateCapacityForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);

	int32 CabinetSkill = 55;
	int32 CabinetCorruptionPressure = 0;
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
		if (Stats.FilledOffices > 0)
		{
			CabinetSkill = Stats.AverageSkill;
			CabinetCorruptionPressure = Stats.Corruption / 6;
		}
	}

	Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + (CabinetSkill - 50) / 20);
	const FWLPatronageState Patronage = GetPatronageState(Iso);
	int32 AverageRegionalObedience = 55;
	const TArray<FWLRegionGovernorState> Regions = GetRegionGovernors(Iso);
	if (!Regions.IsEmpty())
	{
		int32 Sum = 0;
		for (const FWLRegionGovernorState& Region : Regions)
		{
			Sum += Region.Obedience;
		}
		AverageRegionalObedience = Sum / Regions.Num();
	}
	Capacity.Corruption = ClampPercent(
		Capacity.Corruption
		+ CabinetCorruptionPressure / 5
		+ Patronage.ContractCorruption / 50
		+ Patronage.ClientelistPressure / 70
		- Capacity.Bureaucracy / 30);
	Capacity.AdministrativeEfficiency = ClampPercent(
		40 + Capacity.Bureaucracy / 2 + CabinetSkill / 3 + Internal.AveragePublicOrder / 5
		+ AverageRegionalObedience / 8
		- Capacity.Corruption / 2);
	Capacity.CentralAuthority = ClampPercent(
		Capacity.CentralAuthority
		+ (Internal.AveragePublicOrder - 55) / 20
		+ (AverageRegionalObedience - 55) / 18
		- Internal.OppositionStrength / 35);
	Capacity.PolicyFailureRisk = ClampPercent(
		70 - Capacity.AdministrativeEfficiency / 2 + Capacity.Corruption / 2
		+ EnsureInstitutionalPower(Iso).GridlockRisk / 4
		+ GetPoliticalMemoryValue(Iso, TEXT("policy_failure")) * 5);
	Capacity.LastReport = FString::Printf(TEXT("Capacidad estatal %s: burocracia %d, corrupcion %d, eficiencia %d, fallo %d."),
		*Iso, Capacity.Bureaucracy, Capacity.Corruption, Capacity.AdministrativeEfficiency, Capacity.PolicyFailureRisk);
}

void UWLPoliticalSubsystem::UpdatePublicGroupsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);
	for (EWLPublicGroup Group : AllPublicGroups())
	{
		FWLPublicGroupSupportState& State = EnsurePublicGroup(Iso, Group);
		const int32 Disorder = FMath::Max(0, 60 - Internal.AveragePublicOrder);
		State.Pressure = ClampPercent(50 - State.Support + Disorder / 2);
		if (Internal.AveragePublicOrder >= 65 && State.Support < 55)
		{
			State.Support = ClampPercent(State.Support + 1);
			State.LastShiftReason = TEXT("normalizacion por orden publico");
		}
		else if (Internal.AveragePublicOrder <= 45)
		{
			State.Support = ClampPercent(State.Support - 1);
			State.LastShiftReason = TEXT("desgaste por crisis de orden publico");
		}
	}
}

void UWLPoliticalSubsystem::AdvancePoliticalMemoryForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	TArray<FString> RemoveKeys;
	for (TPair<FString, FWLPoliticalMemoryRecord>& Pair : PoliticalMemoryByKey)
	{
		FWLPoliticalMemoryRecord& Memory = Pair.Value;
		if (Memory.NationIso != Iso)
		{
			continue;
		}
		Memory.MonthsRemaining = FMath::Max(0, Memory.MonthsRemaining - 1);
		if (Memory.MonthsRemaining <= 0 || Memory.Value == 0)
		{
			RemoveKeys.Add(Pair.Key);
		}
	}
	for (const FString& Key : RemoveKeys)
	{
		PoliticalMemoryByKey.Remove(Key);
	}
}

void UWLPoliticalSubsystem::QueueGovernmentCrisisEventsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	auto QueueCrisis = [this, &Iso](const FString& EventId, const FString& Title, const FString& Body, const TArray<FWLPoliticalEventOption>& Options)
	{
		if (HasQueuedUnresolvedEvent(Iso, EventId))
		{
			return;
		}
		FWLPoliticalEventInstance Event;
		Event.InstanceId = FString::Printf(TEXT("EV-%04d"), NextEventInstanceNumber++);
		Event.EventId = EventId;
		Event.NationIso = Iso;
		Event.Title = Title;
		Event.Body = Body;
		Event.Options = Options;
		EventQueue.Add(MoveTemp(Event));
		if (Iso == GetPlayerNationIso())
		{
			AddNews(FString::Printf(TEXT("Crisis en desarrollo: %s."), *Title));
		}
	};

	if (GetPoliticalMemoryValue(Iso, TEXT("crisis_unrest")) >= 2)
	{
		FWLPoliticalEventOption Negotiate;
		Negotiate.OptionId = TEXT("negotiate");
		Negotiate.Label = TEXT("Negociar con lideres sociales");
		Negotiate.PoliticalCapitalDelta = -8;
		Negotiate.TreasuryDelta = -1500;
		Negotiate.OppositionDelta = -8;
		Negotiate.PublicOrderDelta = 2;

		FWLPoliticalEventOption Repress;
		Repress.OptionId = TEXT("repress");
		Repress.Label = TEXT("Romper la huelga");
		Repress.OppositionDelta = -12;
		Repress.PublicOrderDelta = -4;
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, 45, TEXT("Protesta escala hacia huelga nacional."));
		QueueCrisis(TEXT("crisis_strike"), TEXT("Huelga nacional"), TEXT("Las protestas escalan a paro coordinado."), { Negotiate, Repress });
	}

	if (GetPoliticalMemoryValue(Iso, TEXT("cabinet_scandal")) >= 2)
	{
		FWLPoliticalEventOption Investigate;
		Investigate.OptionId = TEXT("investigate");
		Investigate.Label = TEXT("Abrir investigacion");
		Investigate.PoliticalCapitalDelta = -6;
		Investigate.OppositionDelta = -5;
		Investigate.PublicOrderDelta = 1;

		FWLPoliticalEventOption Cover;
		Cover.OptionId = TEXT("cover_up");
		Cover.Label = TEXT("Tapar el escandalo");
		Cover.OppositionDelta = 8;
		Cover.PublicOrderDelta = -2;
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::CorruptionScandal, 50, TEXT("Escandalo de gabinete se vuelve crisis nacional."));
		QueueCrisis(TEXT("cabinet_scandal_chain"), TEXT("Escandalo de gabinete"), TEXT("La memoria politica de malos manejos abre una crisis."), { Investigate, Cover });
	}
}

void UWLPoliticalSubsystem::AddPoliticalMemory(
	const FString& NationIso,
	const FString& MemoryKey,
	int32 Delta,
	int32 Months,
	const FString& Reason)
{
	const FString Iso = NormalizeIso(NationIso);
	if (Iso.IsEmpty() || MemoryKey.TrimStartAndEnd().IsEmpty())
	{
		return;
	}
	const FString Key = PoliticalMemoryKey(Iso, MemoryKey);
	FWLPoliticalMemoryRecord& Memory = PoliticalMemoryByKey.FindOrAdd(Key);
	if (Memory.NationIso.IsEmpty())
	{
		Memory.NationIso = Iso;
		Memory.MemoryKey = MemoryKey.TrimStartAndEnd().ToLower();
	}
	Memory.Value = FMath::Clamp(Memory.Value + Delta, -100, 100);
	Memory.MonthsRemaining = FMath::Max(Memory.MonthsRemaining, Months);
	Memory.LastReason = Reason;
}

int32 UWLPoliticalSubsystem::GetPoliticalMemoryValue(const FString& NationIso, const FString& MemoryKey) const
{
	if (const FWLPoliticalMemoryRecord* Found = PoliticalMemoryByKey.Find(PoliticalMemoryKey(NationIso, MemoryKey)))
	{
		return Found->MonthsRemaining > 0 ? Found->Value : 0;
	}
	return 0;
}

void UWLPoliticalSubsystem::AutoResolveEventsForAI(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	for (FWLPoliticalEventInstance& Event : EventQueue)
	{
		if (Event.bResolved || Event.NationIso != Iso || Event.Options.IsEmpty())
		{
			continue;
		}
		// Criterio IA: la opcion que mas reduce su oposicion (empate -> menor coste de orden publico).
		const FWLPoliticalEventOption* Best = &Event.Options[0];
		for (const FWLPoliticalEventOption& Option : Event.Options)
		{
			if (Option.OppositionDelta < Best->OppositionDelta
				|| (Option.OppositionDelta == Best->OppositionDelta && Option.PublicOrderDelta > Best->PublicOrderDelta))
			{
				Best = &Option;
			}
		}
		FString Message;
		ResolveEvent(Event.InstanceId, Best->OptionId, Message);
	}
}

void UWLPoliticalSubsystem::RunStrategicAIForNation(const FString& NationIso)
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Tick || !Registry)
	{
		return;
	}

	const FString Iso = NormalizeIso(NationIso);
	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const FWLNationBudget Budget = Tick->GetNationBudget(Iso);
	const int64 Treasury = Tick->GetTreasury(Iso);
	const int64 OwnStrength = Tick->GetNationMilitaryStrength(Iso);
	const int32 Month = Tick->GetCurrentMonth();
	const FWLPoliticalAIPlanState DiplomaticPlan = GetGovernmentAIPlan(Iso);
	const bool bBlocDoctrine = DiplomaticPlan.Objective == EWLGovernmentAIObjective::Align;
	const bool bExpansionDoctrine = DiplomaticPlan.Objective == EWLGovernmentAIObjective::Expand
		|| DiplomaticPlan.Objective == EWLGovernmentAIObjective::Militarize;
	const bool bWarCooldown = GetPoliticalMemoryValue(Iso, TEXT("war_declared_recently")) > 0;

	// 1) Fisco: en deficit o deuda sube impuestos; con bonanza los relaja hacia el default.
	const int32 Tax = Tick->GetTaxRate(Iso);
	if ((Treasury < 0 || Budget.Net() < 0) && Tax < Rules.TaxRateMaxPercent)
	{
		Tick->SetTaxRate(Iso, Tax + Rules.GetStrategicAITaxStep());
		UE_LOG(LogWorldLeader, Log, TEXT("IA %s: sube impuestos a %d%%."), *Iso, Tick->GetTaxRate(Iso));
	}
	else if (Treasury > Budget.TotalIncome() * 6 && Tax > Rules.TaxRateDefaultPercent)
	{
		Tick->SetTaxRate(Iso, Tax - Rules.GetStrategicAITaxStep());
	}

	// 2) Arancel: deficit comercial -> protege; superavit claro -> abre.
	const int32 Tariff = Tick->GetTariffRate(Iso);
	FString TariffMessage;
	if (Budget.ImportCost > Budget.ExportIncome && Tariff < Rules.GetStrategicAITariffCeiling())
	{
		SetNationTariffRate(Iso, Tariff + Rules.GetStrategicAITariffStep(), TariffMessage);
	}
	else if (Budget.ExportIncome > Budget.ImportCost * 2 && Tariff > 0)
	{
		SetNationTariffRate(Iso, Tariff - Rules.GetStrategicAITariffStep(), TariffMessage);
	}

	// 3) Diplomacia por pais: tratados si la opinion acompana, paz si pierde la guerra, guerra solo
	//    con opinion hundida y clara superioridad militar.
	FString WorstIso;
	int32 WorstOpinion = 0;
	for (const FWLNationData& Other : Registry->GetAllNations())
	{
		if (Other.Iso == Iso)
		{
			continue;
		}
		FWLDiplomaticRelationState Relation;
		GetRelation(Iso, Other.Iso, Relation);
		const int64 OtherStrength = Tick->GetNationMilitaryStrength(Other.Iso);
		FString Message;

		if (Relation.Status == EWLDiplomaticStatus::War)
		{
			if (static_cast<double>(OwnStrength) < static_cast<double>(OtherStrength) * Rules.GetStrategicAIPeaceStrengthRatio())
			{
				MakePeace(Iso, Other.Iso, Message);
				UE_LOG(LogWorldLeader, Log, TEXT("IA %s: pide la paz con %s."), *Iso, *Other.Iso);
			}
			continue;
		}

		const int32 TreatyOffset = Rules.GetStrategicAITreatyOpinionOffset() + (bBlocDoctrine ? -10 : 0) + (bExpansionDoctrine ? 5 : 0);
		if (Relation.Opinion >= 20 + TreatyOffset && !Relation.Treaties.Contains(EWLTreatyType::TradeAgreement))
		{
			if (SignTreaty(Iso, Other.Iso, EWLTreatyType::TradeAgreement, Message))
			{
				AddPoliticalMemory(Iso, TEXT("diplomatic_bloc_building"), 1, 18, Message);
			}
		}
		if (Relation.Opinion >= 25 + TreatyOffset && !Relation.Treaties.Contains(EWLTreatyType::NonAggression))
		{
			if (SignTreaty(Iso, Other.Iso, EWLTreatyType::NonAggression, Message))
			{
				AddPoliticalMemory(Iso, TEXT("diplomatic_bloc_building"), 1, 18, Message);
			}
		}
		if (Relation.Opinion >= 60 + TreatyOffset && !Relation.Treaties.Contains(EWLTreatyType::Alliance))
		{
			if (SignTreaty(Iso, Other.Iso, EWLTreatyType::Alliance, Message))
			{
				AddPoliticalMemory(Iso, TEXT("diplomatic_bloc_building"), 1, 24, Message);
			}
		}
		const int32 WarOpinionThreshold = Rules.GetStrategicAIWarOpinionThreshold() + (bExpansionDoctrine ? 8 : -8);
		const double WarStrengthRatio = Rules.GetStrategicAIWarStrengthRatio() + (bExpansionDoctrine ? -0.15 : 0.25);
		if (!bWarCooldown
			&& Relation.Opinion <= WarOpinionThreshold
			&& static_cast<double>(OwnStrength) > static_cast<double>(OtherStrength) * WarStrengthRatio)
		{
			if (DeclareWar(Iso, Other.Iso, Message))
			{
				AddPoliticalMemory(Iso, TEXT("war_declared_recently"), 1, 18, Message);
				AddPoliticalMemory(Iso, TEXT("diplomatic_war_doctrine"), 1, 24, Message);
				UE_LOG(LogWorldLeader, Warning, TEXT("IA %s: declara la guerra a %s."), *Iso, *Other.Iso);
			}
		}

		if (WorstIso.IsEmpty() || Relation.Opinion < WorstOpinion)
		{
			WorstIso = Other.Iso;
			WorstOpinion = Relation.Opinion;
		}
	}

	// 4) Intriga contra su peor relacion: primero red, luego alterna financiar golpe / propaganda.
	if (!WorstIso.IsEmpty()
		&& WorstOpinion < Rules.GetStrategicAIIntrigueOpinionThreshold()
		&& GetPoliticalMemoryValue(Iso, TEXT("covert_pressure_recently")) < 3
		&& Characters)
	{
		FString SpyId;
		for (const FWLCharacter& Spy : Characters->GetCharactersByRole(Iso, EWLCharacterRole::Spy))
		{
			if (Spy.bActive)
			{
				SpyId = Spy.Id;
				break;
			}
		}
		if (!SpyId.IsEmpty())
		{
			FString Message;
			const FWLIntelligenceNetworkState Network = GetIntelligenceNetwork(Iso, WorstIso);
			if (Network.NetworkStrength < Rules.GetStrategicAISpyNetworkTarget())
			{
				BuildSpyNetwork(Iso, WorstIso, SpyId, Message);
			}
			else if (Network.Exposure < Rules.GetStrategicAISpyExposureLimit())
			{
				RunSpyOperation(Iso, WorstIso, SpyId,
					Month % 2 == 0 ? EWLSpyOperationType::FundCoup : EWLSpyOperationType::Propaganda, Message);
				AddPoliticalMemory(Iso, TEXT("covert_pressure_recently"), 1, 12, Message);
				UE_LOG(LogWorldLeader, Log, TEXT("IA %s vs %s: %s"), *Iso, *WorstIso, *Message);
			}
		}
	}

	// 5) Reclutamiento: con caja y por detras militarmente de su peor relacion, encola tropa en su HQ.
	if (!WorstIso.IsEmpty() && Treasury > Rules.GetStrategicAIRecruitTreasuryThreshold()
		&& static_cast<double>(OwnStrength)
			< static_cast<double>(Tick->GetNationMilitaryStrength(WorstIso)) * Rules.GetStrategicAIRecruitStrengthRatio())
	{
		FString Message;
		if (Tick->QueueRecruit(Iso + TEXT("-AI-HQ"), Iso, TEXT("infantry"), Message))
		{
			AddNews(FString::Printf(TEXT("%s expande su ejercito: nueva leva de infanteria."), *Iso));
			UE_LOG(LogWorldLeader, Log, TEXT("IA %s: recluta infanteria (%s)."), *Iso, *Message);
		}
	}
}

void UWLPoliticalSubsystem::RunGovernmentAIForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Tick || !Characters || !ValidateNation(Iso))
	{
		return;
	}

	FWLPoliticalAIPlanState& Plan = EnsureGovernmentAIPlan(Iso);
	const FWLNationBudget Budget = Tick->GetNationBudget(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);
	const int64 Treasury = Tick->GetTreasury(Iso);
	const FString TargetIso = SelectGovernmentAITarget(Iso);
	const int64 OwnStrength = Tick->GetNationMilitaryStrength(Iso);
	const int64 TargetStrength = TargetIso.IsEmpty() ? 0 : Tick->GetNationMilitaryStrength(TargetIso);

	if (Internal.AveragePublicOrder < 50 || Internal.CoupRisk > 60 || Internal.OppositionStrength > 55)
	{
		Plan.Objective = EWLGovernmentAIObjective::Stabilize;
		Plan.LastPlanReason = TEXT("orden publico/oposicion exigen estabilizar");
	}
	else if (Treasury < 0 || Budget.Net() < 0)
	{
		Plan.Objective = EWLGovernmentAIObjective::Borrow;
		Plan.LastPlanReason = TEXT("deficit obliga a financiarse");
	}
	else if (!TargetIso.IsEmpty() && OwnStrength < TargetStrength)
	{
		Plan.Objective = EWLGovernmentAIObjective::Militarize;
		Plan.LastPlanReason = TEXT("rival supera fuerza militar");
	}
	else if (!TargetIso.IsEmpty() && OwnStrength > TargetStrength * 2 && TargetStrength > 0)
	{
		Plan.Objective = EWLGovernmentAIObjective::Expand;
		Plan.LastPlanReason = TEXT("ventaja militar abre expansion");
	}
	else if (GetRelationsForNation(Iso).Num() > 0 && Budget.ExportIncome >= Budget.ImportCost)
	{
		Plan.Objective = EWLGovernmentAIObjective::Align;
		Plan.LastPlanReason = TEXT("economia permite diplomacia de bloque");
	}
	else
	{
		Plan.Objective = EWLGovernmentAIObjective::Industrialize;
		Plan.LastPlanReason = TEXT("base economica pide industrializacion");
	}
	Plan.TargetIso = TargetIso;
	++Plan.MonthsOnPlan;

	TArray<EWLGovernmentPriority> Agenda;
	FString ProgramId;
	switch (Plan.Objective)
	{
	case EWLGovernmentAIObjective::Stabilize:
		Agenda = { EWLGovernmentPriority::Control, EWLGovernmentPriority::Security, EWLGovernmentPriority::Growth };
		ProgramId = Internal.OppositionStrength > 55 ? TEXT("int_public_order") : TEXT("int_governors");
		if (Internal.OppositionStrength > 70)
		{
			FString RepressMessage;
			RepressOpposition(Iso, RepressMessage);
		}
		break;
	case EWLGovernmentAIObjective::Borrow:
		Agenda = { EWLGovernmentPriority::Austerity, EWLGovernmentPriority::Growth, EWLGovernmentPriority::Diplomacy };
		ProgramId = TEXT("econ_tax_reform");
		if (Treasury < 0)
		{
			FString FinanceMessage;
			Tick->IssueBond(Iso, 8000, 24, FinanceMessage);
		}
		break;
	case EWLGovernmentAIObjective::Militarize:
	case EWLGovernmentAIObjective::Expand:
		Agenda = { EWLGovernmentPriority::Security, EWLGovernmentPriority::Industrialization, EWLGovernmentPriority::Control };
		ProgramId = Plan.Objective == EWLGovernmentAIObjective::Expand ? TEXT("def_mobilization") : TEXT("def_procurement");
		break;
	case EWLGovernmentAIObjective::Align:
		Agenda = { EWLGovernmentPriority::Diplomacy, EWLGovernmentPriority::Growth, EWLGovernmentPriority::Security };
		ProgramId = TEXT("for_bloc");
		break;
	case EWLGovernmentAIObjective::Industrialize:
	default:
		Agenda = { EWLGovernmentPriority::Industrialization, EWLGovernmentPriority::Growth, EWLGovernmentPriority::Diplomacy };
		ProgramId = TEXT("econ_public_investment");
		break;
	}

	FString AgendaMessage;
	SetGovernmentAgenda(Iso, Agenda, AgendaMessage);
	if (GetActiveMinistryPrograms(Iso).Num() < 2)
	{
		FString ProgramMessage;
		if (StartMinistryProgram(Iso, ProgramId, ProgramMessage))
		{
			Plan.CurrentProgramId = ProgramId;
		}
	}

	if (EnsureInstitutionalPower(Iso).RulingCoalitionSupport < 50)
	{
		for (const FWLPartyState& Party : GetPoliticalParties(Iso))
		{
			if ((Party.Role == EWLPartyRole::Ally || Party.Role == EWLPartyRole::SoftOpposition) && !Party.bInCoalition)
			{
				FString PartyMessage;
				NegotiatePartySupport(Iso, Party.PartyId, PartyMessage);
				break;
			}
		}
	}

	if (GetActivePolicyReforms(Iso).IsEmpty() && Characters->GetPoliticalCapital(Iso) >= 35)
	{
		const FString ReformId =
			Plan.Objective == EWLGovernmentAIObjective::Stabilize ? TEXT("security_citizen_plan") :
			Plan.Objective == EWLGovernmentAIObjective::Borrow ? TEXT("tax_broad_base") :
			Plan.Objective == EWLGovernmentAIObjective::Militarize || Plan.Objective == EWLGovernmentAIObjective::Expand ? TEXT("military_professionalization") :
			Plan.Objective == EWLGovernmentAIObjective::Align ? TEXT("trade_customs_modernization") :
			TEXT("energy_sovereignty");
		FString ReformMessage;
		EnactPolicyReform(Iso, ReformId, ReformMessage);
	}

	FWLElectionState Election = GetElectionState(Iso);
	if (Election.MonthsToElection <= 8 && Election.CampaignPromiseReformId.IsEmpty())
	{
		FString PromiseMessage;
		MakeCampaignPromise(Iso,
			Plan.Objective == EWLGovernmentAIObjective::Stabilize ? TEXT("security_citizen_plan") : TEXT("edu_public_schools"),
			PromiseMessage);
	}

	if (GetMediaPublicOpinion(Iso).MediaCrisisRisk > 55)
	{
		FString MediaMessage;
		RunMediaAction(Iso, EWLMediaActionType::CounterFakeNews, MediaMessage);
	}
	else if (GetMediaPublicOpinion(Iso).PresidentialApproval < 45)
	{
		FString MediaMessage;
		RunMediaAction(Iso, EWLMediaActionType::StateBroadcast, MediaMessage);
	}

	for (const FWLRegionGovernorState& Region : GetRegionGovernors(Iso))
	{
		if (Region.RebellionRisk > 55 || Region.ProtestRisk > 60)
		{
			FString RegionMessage;
			RunRegionPolicy(Iso, Region.RegionId, EWLRegionPolicyActionType::RegionalInvestment, RegionMessage);
			break;
		}
	}

	if (EnsureInstitutionalPower(Iso).GridlockRisk > 65 && GetPatronageState(Iso).ClientelistPressure < 70)
	{
		FString PatronageMessage;
		UsePatronage(Iso, EWLPatronageActionType::GrantFavor, PatronageMessage);
	}

	FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
	if (Stats.FilledOffices > 0 && Stats.AverageSkill < 56 && Characters->GetPoliticalCapital(Iso) >= 20)
	{
		FWLCharacter Hired;
		FString HireMessage;
		const EWLMinisterOffice Office =
			Plan.Objective == EWLGovernmentAIObjective::Militarize || Plan.Objective == EWLGovernmentAIObjective::Expand
				? EWLMinisterOffice::Defense
				: Plan.Objective == EWLGovernmentAIObjective::Align
					? EWLMinisterOffice::Foreign
					: Plan.Objective == EWLGovernmentAIObjective::Stabilize
						? EWLMinisterOffice::Interior
						: EWLMinisterOffice::Economy;
		Characters->HireMinister(Iso, Office, Hired, HireMessage);
	}
}

int32 UWLPoliticalSubsystem::GetMinisterProgramModifier(const FString& NationIso, EWLMinisterOffice Office) const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters)
	{
		return 0;
	}
	FWLCharacter Minister;
	if (!Characters->GetCabinetMinister(NationIso, Office, Minister))
	{
		return -12;
	}

	int32 Modifier = FMath::RoundToInt((static_cast<double>(Minister.Skill) - 50.0) / 3.0);
	for (const FString& Trait : Minister.Traits)
	{
		const FString T = Trait.ToLower();
		if (T == TEXT("tecnocrata") || T == TEXT("fiscalista") || T == TEXT("disciplinado") || T == TEXT("diplomatica"))
		{
			Modifier += 4;
		}
		else if (T == TEXT("halcon") && Office == EWLMinisterOffice::Defense)
		{
			Modifier += 3;
		}
		else if (T == TEXT("sigiloso") && Office == EWLMinisterOffice::Intelligence)
		{
			Modifier += 3;
		}
		else if (T == TEXT("corrupto"))
		{
			Modifier -= 10;
		}
		else if (T == TEXT("clientelista"))
		{
			Modifier -= 6;
		}
	}
	return FMath::Clamp(Modifier, -20, 25);
}

void UWLPoliticalSubsystem::AdjustPublicGroupSupport(
	const FString& NationIso,
	EWLPublicGroup Group,
	int32 SupportDelta,
	const FString& Reason)
{
	FWLPublicGroupSupportState& State = EnsurePublicGroup(NationIso, Group);
	State.Support = ClampPercent(State.Support + SupportDelta);
	State.LastShiftReason = Reason;
}

bool UWLPoliticalSubsystem::ApplyProgramEffect(
	const FString& NationIso,
	const FWLMinistryProgramDefinition& Definition,
	FWLMinistryProgramState& Program)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	FWLInternalPowerState& Internal = EnsureInternalPower(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);

	const FString Id = Definition.ProgramId;
	if (Id == TEXT("econ_tax_reform"))
	{
		if (Tick) { Tick->SetTaxRate(Iso, Tick->GetTaxRate(Iso) + 2); }
		Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + 1);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 1, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -1, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -1, Definition.Name);
	}
	else if (Id == TEXT("econ_public_investment"))
	{
		if (Tick) { Tick->AdjustNationPublicOrder(Iso, 1); }
		Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + 1);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 2, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::MiddleClass, 1, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 1, Definition.Name);
	}
	else if (Id == TEXT("econ_subsidies"))
	{
		if (Tick) { Tick->AdjustNationPublicOrder(Iso, 2); }
		Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 2);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 2, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, 2, Definition.Name);
	}
	else if (Id == TEXT("def_doctrine"))
	{
		EnsureCabinetDynamics(Iso).RivalryPressure = ClampPercent(EnsureCabinetDynamics(Iso).RivalryPressure - 2);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 2, Definition.Name);
	}
	else if (Id == TEXT("def_procurement"))
	{
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 2, Definition.Name);
		if (Tick)
		{
			FString RecruitMessage;
			Tick->QueueRecruit(Iso + TEXT("-AI-HQ"), Iso, TEXT("infantry"), RecruitMessage);
		}
	}
	else if (Id == TEXT("def_mobilization"))
	{
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 3, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -2, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -1, Definition.Name);
		if (Tick)
		{
			Tick->AdjustNationPublicOrder(Iso, -1);
			FString RecruitMessage;
			Tick->QueueRecruit(Iso + TEXT("-AI-HQ"), Iso, TEXT("infantry"), RecruitMessage);
		}
	}
	else if (Id == TEXT("int_police_reform"))
	{
		Capacity.CentralAuthority = ClampPercent(Capacity.CentralAuthority + 2);
		Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 2);
		if (Tick) { Tick->AdjustNationPublicOrder(Iso, 1); }
	}
	else if (Id == TEXT("int_governors"))
	{
		Capacity.CentralAuthority = ClampPercent(Capacity.CentralAuthority + 2);
		Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport + 1);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Regions, 3, Definition.Name);
	}
	else if (Id == TEXT("int_public_order"))
	{
		Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 3);
		if (Tick) { Tick->AdjustNationPublicOrder(Iso, 2); }
	}
	else if (Id == TEXT("for_bloc") || Id == TEXT("for_trade_drive"))
	{
		FString BestIso;
		int32 BestOpinion = -101;
		for (const FWLDiplomaticRelationState& Relation : GetRelationsForNation(Iso))
		{
			const FString OtherIso = Relation.NationA == Iso ? Relation.NationB : Relation.NationA;
			if (Relation.Opinion > BestOpinion)
			{
				BestOpinion = Relation.Opinion;
				BestIso = OtherIso;
			}
		}
		if (!BestIso.IsEmpty())
		{
			FString Message;
			AdjustRelationOpinion(Iso, BestIso, 2, Message);
			SignTreaty(Iso, BestIso,
				Id == TEXT("for_trade_drive") ? EWLTreatyType::TradeAgreement : EWLTreatyType::NonAggression,
				Message);
		}
	}
	else if (Id == TEXT("for_sanctions"))
	{
		const FString Target = SelectGovernmentAITarget(Iso);
		if (!Target.IsEmpty())
		{
			FString Message;
			SignTreaty(Iso, Target, EWLTreatyType::Embargo, Message);
		}
	}
	else if (Id == TEXT("spy_networks") || Id == TEXT("spy_covert_ops"))
	{
		const FString Target = SelectGovernmentAITarget(Iso);
		if (const UWLCharacterSubsystem* Characters = GetCharacters())
		{
			FString SpyId;
			for (const FWLCharacter& Spy : Characters->GetCharactersByRole(Iso, EWLCharacterRole::Spy))
			{
				if (Spy.bActive)
				{
					SpyId = Spy.Id;
					break;
				}
			}
			if (!SpyId.IsEmpty() && !Target.IsEmpty())
			{
				FString Message;
				if (Id == TEXT("spy_networks"))
				{
					BuildSpyNetwork(Iso, Target, SpyId, Message);
				}
				else
				{
					if (GetIntelligenceNetwork(Iso, Target).NetworkStrength < 20)
					{
						BuildSpyNetwork(Iso, Target, SpyId, Message);
					}
					else
					{
						RunSpyOperation(Iso, Target, SpyId, EWLSpyOperationType::Propaganda, Message);
					}
				}
			}
		}
	}
	else if (Id == TEXT("spy_counterintel"))
	{
		for (TPair<FString, FWLIntelligenceNetworkState>& Pair : IntelligenceByPair)
		{
			FWLIntelligenceNetworkState& Network = Pair.Value;
			if (Network.TargetIso == Iso)
			{
				Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 8);
				Network.Exposure = ClampPercent(Network.Exposure + 5);
			}
			else if (Network.OwnerIso == Iso)
			{
				Network.Exposure = ClampPercent(Network.Exposure - 8);
			}
		}
	}
	else
	{
		switch (Definition.Office)
		{
		case EWLMinisterOffice::Economy:
			Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + (Id.Contains(TEXT("corruption")) ? 2 : 1));
			if (Id.Contains(TEXT("privatization")) || Id.Contains(TEXT("concession")))
			{
				if (Tick)
				{
					FString TreasuryMessage;
					Tick->AdjustTreasury(Iso, 900, TreasuryMessage);
				}
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 2, Definition.Name);
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -2, Definition.Name);
			}
			else
			{
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 1, Definition.Name);
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::MiddleClass, 1, Definition.Name);
			}
			break;
		case EWLMinisterOffice::Defense:
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 2, Definition.Name);
			EnsureCabinetDynamics(Iso).RivalryPressure = ClampPercent(EnsureCabinetDynamics(Iso).RivalryPressure - 1);
			if (Id.Contains(TEXT("service")) || Id.Contains(TEXT("counterinsurgency")))
			{
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -1, Definition.Name);
				if (Tick) { Tick->AdjustNationPublicOrder(Iso, -1); }
			}
			break;
		case EWLMinisterOffice::Interior:
			Capacity.CentralAuthority = ClampPercent(Capacity.CentralAuthority + 1);
			Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 1);
			if (Id.Contains(TEXT("decentralization")) || Id.Contains(TEXT("dialogue")))
			{
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Regions, 2, Definition.Name);
			}
			if (Tick) { Tick->AdjustNationPublicOrder(Iso, 1); }
			break;
		case EWLMinisterOffice::Foreign:
			for (TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
			{
				FWLDiplomaticRelationState& Relation = Pair.Value;
				if (Relation.NationA == Iso || Relation.NationB == Iso)
				{
					Relation.Opinion = FMath::Clamp(Relation.Opinion + 1, -100, 100);
				}
			}
			EnsureElectionState(Iso).Legitimacy = ClampPercent(EnsureElectionState(Iso).Legitimacy + 1);
			break;
		case EWLMinisterOffice::Intelligence:
			EnsureMediaState(Iso).FakeNewsPressure = ClampPercent(EnsureMediaState(Iso).FakeNewsPressure - 2);
			EnsureCabinetDynamics(Iso).SabotageRisk = ClampPercent(EnsureCabinetDynamics(Iso).SabotageRisk - 1);
			if (Id.Contains(TEXT("black_budget")))
			{
				EnsurePatronageState(Iso).ContractCorruption = ClampPercent(EnsurePatronageState(Iso).ContractCorruption + 2);
			}
			break;
		default:
			return false;
		}
	}

	Program.LastReport = FString::Printf(TEXT("%s avanza: %s (%d%%)."), *Definition.Name, *Iso, Program.Progress);
	return true;
}

FString UWLPoliticalSubsystem::SelectGovernmentAITarget(const FString& NationIso) const
{
	const FString Iso = NormalizeIso(NationIso);
	FString WorstIso;
	int32 WorstOpinion = 101;
	for (const FWLDiplomaticRelationState& Relation : GetRelationsForNation(Iso))
	{
		const FString OtherIso = Relation.NationA == Iso ? Relation.NationB : Relation.NationA;
		if (Relation.Opinion < WorstOpinion)
		{
			WorstOpinion = Relation.Opinion;
			WorstIso = OtherIso;
		}
	}
	return WorstIso;
}

bool UWLPoliticalSubsystem::AttemptCoup(const FString& NationIso, FString& OutReport)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutReport = FString::Printf(TEXT("Nacion no disponible: %s"), *NationIso);
		return false;
	}

	UpdateInternalPowerForNation(Iso);
	FWLInternalPowerState& State = EnsureInternalPower(Iso);

	int32 GeneralLoyalty = 60;
	int32 GeneralSkill = 50;
	int32 Count = 0;
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		int32 LoyaltySum = 0;
		int32 SkillSum = 0;
		for (const FWLCharacter& General : Characters->GetGenerals(Iso))
		{
			LoyaltySum += General.Loyalty;
			SkillSum += General.Skill;
			++Count;
		}
		if (Count > 0)
		{
			GeneralLoyalty = LoyaltySum / Count;
			GeneralSkill = SkillSum / Count;
		}
	}

	const int32 CoupPressure = State.CoupRisk + State.OppositionStrength / 2 + State.ExternalCoupFunding / 3;
	const int32 LoyalDefense = GeneralLoyalty / 2 + GeneralSkill / 4 + State.AveragePublicOrder / 3;
	State.LastCoupRoll = CoupPressure - LoyalDefense;
	State.bLastCoupSucceeded = State.LastCoupRoll > 0;

	if (State.bLastCoupSucceeded)
	{
		const FString PlayerIso = GetPlayerNationIso();
		if (!PlayerIso.IsEmpty() && Iso != PlayerIso)
		{
			// Golpe en una nacion IA: cambio de regimen, NO fin de la partida del jugador.
			// La junta purga a la oposicion, corta la financiacion externa y desestabiliza el pais.
			State.OppositionStrength = ClampPercent(State.OppositionStrength - 30);
			State.ExternalCoupFunding = 0;
			if (UWLStrategicTickSubsystem* Tick = GetTick())
			{
				Tick->AdjustNationPublicOrder(Iso, -12);
				FString TreasuryMessage;
				Tick->AdjustTreasury(Iso, -Tick->GetTreasury(Iso) / 5, TreasuryMessage);   // fuga de capitales
			}
			State.LastCoupReport = FString::Printf(
				TEXT("Golpe exitoso en %s: una junta toma el poder (presion %d vs defensa %d)."),
				*Iso, CoupPressure, LoyalDefense);
			UpdateInternalPowerForNation(Iso);
			AddNews(State.LastCoupReport);
			OutReport = State.LastCoupReport;
			return true;
		}

		CampaignOutcome.bGameOver = true;
		CampaignOutcome.OutcomeType = TEXT("Coup");
		CampaignOutcome.LosingNationIso = Iso;
		CampaignOutcome.Reason = FString::Printf(TEXT("Golpe exitoso en %s (presion %d vs defensa %d)."),
			*Iso, CoupPressure, LoyalDefense);
		State.LastCoupReport = CampaignOutcome.Reason;
		OutReport = State.LastCoupReport;
		return true;
	}

	State.ExternalCoupFunding = ClampPercent(State.ExternalCoupFunding - 15);
	State.OppositionStrength = ClampPercent(State.OppositionStrength - 8);
	State.LastCoupReport = FString::Printf(TEXT("Golpe fallido en %s (presion %d vs defensa %d)."),
		*Iso, CoupPressure, LoyalDefense);
	AddNews(State.LastCoupReport);
	OutReport = State.LastCoupReport;
	return false;
}

bool UWLPoliticalSubsystem::RewardGeneral(
	const FString& NationIso,
	const FString& CharacterId,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Tick || !Characters || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Subsistemas de politica no disponibles.");
		return false;
	}
	FWLCharacter Character;
	if (!Characters->GetCharacter(CharacterId, Character)
		|| Character.CountryIso != Iso
		|| Character.Role != EWLCharacterRole::General)
	{
		OutMessage = FString::Printf(TEXT("General invalido para %s: %s"), *Iso, *CharacterId);
		return false;
	}
	if (Tick->GetTreasury(Iso) < RewardGeneralCost)
	{
		OutMessage = TEXT("Tesoro insuficiente para recompensar al general.");
		return false;
	}
	FString TreasuryMessage;
	Tick->AdjustTreasury(Iso, -RewardGeneralCost, TreasuryMessage);
	FString LoyaltyMessage;
	Characters->AdjustCharacterLoyalty(Character.Id, 10, LoyaltyMessage);
	FString RenownMessage;
	Characters->AddRenownToGeneral(Character.Id, 5, RenownMessage);
	OutMessage = FString::Printf(TEXT("%s %s %s"), *TreasuryMessage, *LoyaltyMessage, *RenownMessage);
	return true;
}

bool UWLPoliticalSubsystem::PurgeCharacter(
	const FString& NationIso,
	const FString& CharacterId,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Subsistema de personajes no disponible.");
		return false;
	}
	FWLCharacter Character;
	if (!Characters->GetCharacter(CharacterId, Character) || Character.CountryIso != Iso)
	{
		OutMessage = FString::Printf(TEXT("Personaje invalido para %s: %s"), *Iso, *CharacterId);
		return false;
	}
	if (!Characters->RetireCharacter(Character.Id, OutMessage))
	{
		return false;
	}
	FWLInternalPowerState& State = EnsureInternalPower(Iso);
	State.OppositionStrength = ClampPercent(State.OppositionStrength + 12);
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->AdjustNationPublicOrder(Iso, -2);
	}
	OutMessage += TEXT(" La purga aumenta tension politica.");
	return true;
}

bool UWLPoliticalSubsystem::RepressOpposition(const FString& NationIso, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Subsistema economico/provincial no disponible.");
		return false;
	}
	if (Tick->GetTreasury(Iso) < RepressOppositionCost)
	{
		OutMessage = TEXT("Tesoro insuficiente para reprimir oposicion.");
		return false;
	}
	FWLInternalPowerState& State = EnsureInternalPower(Iso);
	const int32 PreviousOpposition = State.OppositionStrength;
	State.OppositionStrength = ClampPercent(State.OppositionStrength - 18);
	State.OppositionPopularity = ClampPercent(State.OppositionPopularity - 10);
	FString TreasuryMessage;
	Tick->AdjustTreasury(Iso, -RepressOppositionCost, TreasuryMessage);
	Tick->AdjustNationPublicOrder(Iso, -4);
	OutMessage = FString::Printf(TEXT("Oposicion %d -> %d. %s"),
		PreviousOpposition, State.OppositionStrength, *TreasuryMessage);
	return true;
}

TArray<FWLDiplomaticRelationState> UWLPoliticalSubsystem::GetRelationsForNation(const FString& NationIso) const
{
	TArray<FWLDiplomaticRelationState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
	{
		if (Pair.Value.NationA == Iso || Pair.Value.NationB == Iso)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLDiplomaticRelationState& A, const FWLDiplomaticRelationState& B)
	{
		return (A.NationA + A.NationB) < (B.NationA + B.NationB);
	});
	return Out;
}

bool UWLPoliticalSubsystem::GetRelation(
	const FString& NationA,
	const FString& NationB,
	FWLDiplomaticRelationState& OutRelation) const
{
	if (const FWLDiplomaticRelationState* Found = RelationsByPair.Find(RelationKey(NationA, NationB)))
	{
		OutRelation = *Found;
		return true;
	}
	return false;
}

bool UWLPoliticalSubsystem::SetRelationOpinion(
	const FString& NationA,
	const FString& NationB,
	int32 Opinion,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Relacion diplomatica invalida.");
		return false;
	}
	FWLDiplomaticRelationState& Relation = EnsureRelation(NationA, NationB);
	Relation.Opinion = FMath::Clamp(Opinion, -100, 100);
	if (Relation.Status != EWLDiplomaticStatus::War)
	{
		Relation.Status = Relation.Opinion < -35 ? EWLDiplomaticStatus::Tension : EWLDiplomaticStatus::Peace;
	}
	OutMessage = FString::Printf(TEXT("Opinion %s-%s = %d."), *Relation.NationA, *Relation.NationB, Relation.Opinion);
	return true;
}

bool UWLPoliticalSubsystem::AdjustRelationOpinion(
	const FString& NationA,
	const FString& NationB,
	int32 Delta,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Relacion diplomatica invalida.");
		return false;
	}

	FWLDiplomaticRelationState Relation;
	const int32 BaseOpinion = GetRelation(NationA, NationB, Relation) ? Relation.Opinion : 0;
	return SetRelationOpinion(NationA, NationB, BaseOpinion + Delta, OutMessage);
}

bool UWLPoliticalSubsystem::DeclareWar(
	const FString& AggressorIso,
	const FString& TargetIso,
	FString& OutMessage)
{
	if (!ValidateNation(AggressorIso) || !ValidateNation(TargetIso) || NormalizeIso(AggressorIso) == NormalizeIso(TargetIso))
	{
		OutMessage = TEXT("Declaracion de guerra invalida.");
		return false;
	}
	FWLDiplomaticRelationState& Relation = EnsureRelation(AggressorIso, TargetIso);
	Relation.Status = EWLDiplomaticStatus::War;
	Relation.Opinion = -100;
	Relation.CasusBelli = FString::Printf(TEXT("%s declaro la guerra a %s."), *NormalizeIso(AggressorIso), *NormalizeIso(TargetIso));
	Relation.Treaties.Remove(EWLTreatyType::TradeAgreement);
	Relation.Treaties.Remove(EWLTreatyType::NonAggression);
	Relation.Treaties.Remove(EWLTreatyType::Alliance);
	AddNews(Relation.CasusBelli);
	OutMessage = Relation.CasusBelli;
	return true;
}

bool UWLPoliticalSubsystem::MakePeace(
	const FString& NationA,
	const FString& NationB,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Paz invalida.");
		return false;
	}
	FWLDiplomaticRelationState& Relation = EnsureRelation(NationA, NationB);
	if (Relation.Status != EWLDiplomaticStatus::War)
	{
		OutMessage = FString::Printf(TEXT("%s y %s no estan en guerra."), *Relation.NationA, *Relation.NationB);
		return false;
	}
	// La posguerra deja tension, no amistad: opinion arranca en -30 y las rutas reabren a medias.
	Relation.Status = EWLDiplomaticStatus::Tension;
	Relation.Opinion = FMath::Max(Relation.Opinion, -30);
	Relation.CasusBelli.Reset();
	OutMessage = FString::Printf(TEXT("Paz firmada entre %s y %s. La tension persiste."),
		*Relation.NationA, *Relation.NationB);
	AddNews(OutMessage);
	return true;
}

bool UWLPoliticalSubsystem::SignTreaty(
	const FString& NationA,
	const FString& NationB,
	EWLTreatyType Treaty,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Tratado invalido.");
		return false;
	}
	FWLDiplomaticRelationState& Relation = EnsureRelation(NationA, NationB);
	if (Relation.Status == EWLDiplomaticStatus::War && Treaty != EWLTreatyType::Embargo)
	{
		OutMessage = TEXT("No se puede firmar tratado durante una guerra.");
		return false;
	}
	const int32 RequiredOpinion =
		Treaty == EWLTreatyType::Alliance ? 55 :
		Treaty == EWLTreatyType::NonAggression ? 20 :
		Treaty == EWLTreatyType::TradeAgreement ? 0 : -100;
	if (Relation.Opinion < RequiredOpinion)
	{
		OutMessage = FString::Printf(TEXT("Opinion insuficiente para %s (%d/%d)."),
			*TreatyToString(Treaty), Relation.Opinion, RequiredOpinion);
		return false;
	}
	if (!Relation.Treaties.Contains(Treaty))
	{
		Relation.Treaties.Add(Treaty);
	}
	if (Treaty == EWLTreatyType::TradeAgreement)
	{
		Relation.Treaties.Remove(EWLTreatyType::Embargo);
	}
	if (Treaty == EWLTreatyType::Embargo)
	{
		Relation.Treaties.Remove(EWLTreatyType::TradeAgreement);
		Relation.Opinion = FMath::Clamp(Relation.Opinion - 20, -100, 100);
		Relation.Status = EWLDiplomaticStatus::Tension;
	}
	OutMessage = FString::Printf(TEXT("%s firmado entre %s y %s."),
		*TreatyToString(Treaty), *Relation.NationA, *Relation.NationB);
	AddNews(OutMessage);
	return true;
}

bool UWLPoliticalSubsystem::BreakTreaty(
	const FString& NationA,
	const FString& NationB,
	EWLTreatyType Treaty,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Tratado invalido.");
		return false;
	}

	FWLDiplomaticRelationState& Relation = EnsureRelation(NationA, NationB);
	const int32 Removed = Relation.Treaties.Remove(Treaty);
	if (Removed <= 0)
	{
		OutMessage = FString::Printf(TEXT("No existe %s entre %s y %s."),
			*TreatyToString(Treaty), *Relation.NationA, *Relation.NationB);
		return false;
	}
	Relation.Opinion = FMath::Clamp(Relation.Opinion - 10, -100, 100);
	OutMessage = FString::Printf(TEXT("%s roto entre %s y %s."),
		*TreatyToString(Treaty), *Relation.NationA, *Relation.NationB);
	AddNews(OutMessage);
	return true;
}

bool UWLPoliticalSubsystem::SetNationTariffRate(
	const FString& NationIso,
	int32 RatePercent,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para aranceles.");
		return false;
	}
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick)
	{
		OutMessage = TEXT("Economia no disponible.");
		return false;
	}

	const int32 PreviousRate = Tick->GetTariffRate(Iso);
	const int32 EffectiveRate = Tick->SetTariffRate(Iso, RatePercent);
	const int32 Delta = EffectiveRate - PreviousRate;
	int32 RelationsTouched = 0;
	if (Delta != 0)
	{
		const FWLBalanceRules Rules = Tick->GetBalanceRules();
		const int32 OpinionDelta = Delta > 0
			? -FMath::CeilToInt(static_cast<double>(Delta) * Rules.TariffRelationPenaltyPerPoint)
			: FMath::CeilToInt(static_cast<double>(-Delta) * Rules.TariffRelationPenaltyPerPoint * 0.5);
		if (OpinionDelta != 0)
		{
			for (TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
			{
				FWLDiplomaticRelationState& Relation = Pair.Value;
				if (Relation.NationA != Iso && Relation.NationB != Iso)
				{
					continue;
				}
				Relation.Opinion = FMath::Clamp(Relation.Opinion + OpinionDelta, -100, 100);
				if (Relation.Status != EWLDiplomaticStatus::War)
				{
					Relation.Status = Relation.Opinion < -35 ? EWLDiplomaticStatus::Tension : EWLDiplomaticStatus::Peace;
				}
				++RelationsTouched;
			}
		}
	}

	OutMessage = FString::Printf(TEXT("Arancel %s: %d%% -> %d%% (%d relaciones afectadas)."),
		*Iso, PreviousRate, EffectiveRate, RelationsTouched);
	return true;
}

bool UWLPoliticalSubsystem::ValidateSpy(
	const FString& OwnerIso,
	const FString& SpyCharacterId,
	int32& OutSkill,
	FString& OutMessage) const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	FWLCharacter Spy;
	if (!Characters || !Characters->GetCharacter(NormalizeCharacterId(SpyCharacterId), Spy))
	{
		OutMessage = FString::Printf(TEXT("Espia desconocido: %s"), *SpyCharacterId);
		return false;
	}
	if (!Spy.bActive || Spy.CountryIso != NormalizeIso(OwnerIso) || Spy.Role != EWLCharacterRole::Spy)
	{
		OutMessage = FString::Printf(TEXT("%s no es un espia activo de %s."), *Spy.Name, *OwnerIso);
		return false;
	}
	OutSkill = Spy.Skill;
	return true;
}

FWLIntelligenceNetworkState UWLPoliticalSubsystem::GetIntelligenceNetwork(
	const FString& OwnerIso,
	const FString& TargetIso) const
{
	if (const FWLIntelligenceNetworkState* Found = IntelligenceByPair.Find(NetworkKey(OwnerIso, TargetIso)))
	{
		return *Found;
	}
	FWLIntelligenceNetworkState State;
	State.OwnerIso = NormalizeIso(OwnerIso);
	State.TargetIso = NormalizeIso(TargetIso);
	return State;
}

bool UWLPoliticalSubsystem::BuildSpyNetwork(
	const FString& OwnerIso,
	const FString& TargetIso,
	const FString& SpyCharacterId,
	FString& OutMessage)
{
	const FString Owner = NormalizeIso(OwnerIso);
	const FString Target = NormalizeIso(TargetIso);
	if (!ValidateNation(Owner) || !ValidateNation(Target) || Owner == Target)
	{
		OutMessage = TEXT("Red de inteligencia invalida.");
		return false;
	}
	int32 SpySkill = 0;
	if (!ValidateSpy(Owner, SpyCharacterId, SpySkill, OutMessage))
	{
		return false;
	}
	SpySkill += GetIntelligenceMinisterSkillBonus(Owner);   // Fase 3: el ministro de Inteligencia potencia a los espias
	FWLIntelligenceNetworkState& Network = EnsureNetwork(Owner, Target);
	const int32 Previous = Network.NetworkStrength;
	Network.NetworkStrength = ClampPercent(Network.NetworkStrength + 10 + SpySkill / 8);
	Network.Exposure = ClampPercent(Network.Exposure + 2);
	Network.LastOperationReport = FString::Printf(TEXT("Red %s>%s %d -> %d."),
		*Owner, *Target, Previous, Network.NetworkStrength);
	OutMessage = Network.LastOperationReport;
	return true;
}

bool UWLPoliticalSubsystem::RunSpyOperation(
	const FString& OwnerIso,
	const FString& TargetIso,
	const FString& SpyCharacterId,
	EWLSpyOperationType Operation,
	FString& OutMessage)
{
	const FString Owner = NormalizeIso(OwnerIso);
	const FString Target = NormalizeIso(TargetIso);
	if (!ValidateNation(Owner) || !ValidateNation(Target) || Owner == Target)
	{
		OutMessage = TEXT("Operacion de intriga invalida.");
		return false;
	}
	int32 SpySkill = 0;
	if (!ValidateSpy(Owner, SpyCharacterId, SpySkill, OutMessage))
	{
		return false;
	}
	SpySkill += GetIntelligenceMinisterSkillBonus(Owner);   // Fase 3: el ministro de Inteligencia potencia a los espias
	FWLIntelligenceNetworkState& Network = EnsureNetwork(Owner, Target);
	if (Network.NetworkStrength < 15 && Operation != EWLSpyOperationType::CounterIntelligence)
	{
		OutMessage = TEXT("Red de inteligencia insuficiente.");
		return false;
	}

	FWLInternalPowerState& TargetPower = EnsureInternalPower(Target);
	const int32 SuccessScore = SpySkill + Network.NetworkStrength - Network.Exposure / 2;
	// Fase 3 auditoria: el exito ESCALA los efectos (antes "exito/limitado" era solo texto).
	const double Scale = FMath::Clamp(static_cast<double>(SuccessScore) / 90.0, 0.35, 1.25);
	const auto Scaled = [Scale](int32 Base)
	{
		return FMath::Max(1, FMath::RoundToInt(static_cast<double>(Base) * Scale));
	};
	UWLStrategicTickSubsystem* Tick = GetTick();
	FString Effect;
	switch (Operation)
	{
	case EWLSpyOperationType::SabotageEconomy:
		if (Tick) { Tick->AdjustNationPublicOrder(Target, -Scaled(3)); }
		TargetPower.OppositionStrength = ClampPercent(TargetPower.OppositionStrength + Scaled(6));
		Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 8);
		Effect = FString::Printf(TEXT("orden publico rival -%d, oposicion +%d"), Scaled(3), Scaled(6));
		break;
	case EWLSpyOperationType::SabotageArmy:
		TargetPower.CoupRisk = ClampPercent(TargetPower.CoupRisk + Scaled(6));
		Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 6);
		Effect = FString::Printf(TEXT("riesgo militar rival +%d"), Scaled(6));
		break;
	case EWLSpyOperationType::FundCoup:
		TargetPower.ExternalCoupFunding = ClampPercent(TargetPower.ExternalCoupFunding + Scaled(15));
		Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 10);
		Effect = FString::Printf(TEXT("financiacion externa de golpe +%d"), Scaled(15));
		break;
	case EWLSpyOperationType::Propaganda:
		if (Tick) { Tick->AdjustNationPublicOrder(Target, -Scaled(2)); }
		TargetPower.OppositionStrength = ClampPercent(TargetPower.OppositionStrength + Scaled(8));
		TargetPower.OppositionPopularity = ClampPercent(TargetPower.OppositionPopularity + Scaled(5));
		Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 5);
		Effect = FString::Printf(TEXT("oposicion rival +%d, popularidad opositora +%d"), Scaled(8), Scaled(5));
		break;
	case EWLSpyOperationType::CounterIntelligence:
	{
		FWLIntelligenceNetworkState& Reverse = EnsureNetwork(Target, Owner);
		Reverse.NetworkStrength = ClampPercent(Reverse.NetworkStrength - Scaled(12 + SpySkill / 10));
		Reverse.Exposure = ClampPercent(Reverse.Exposure + Scaled(8));
		Effect = TEXT("red enemiga reducida");
		break;
	}
	default:
		break;
	}

	Network.Exposure = ClampPercent(Network.Exposure + 8);
	const bool bDetected = Network.Exposure > SuccessScore / 2;
	if (bDetected)
	{
		FString RelationMessage;
		AdjustRelationOpinion(Owner, Target, -18, RelationMessage);
		Effect += TEXT("; operacion detectada");
		AddNews(FString::Printf(TEXT("Operacion de espionaje de %s detectada en %s: la relacion se deteriora."), *Owner, *Target));
	}
	Network.LastOperationReport = FString::Printf(TEXT("%s: %s (%s)."),
		*OperationToString(Operation), SuccessScore >= 45 ? TEXT("exito") : TEXT("resultado limitado"), *Effect);
	OutMessage = Network.LastOperationReport;
	return true;
}

int32 UWLPoliticalSubsystem::GetIntelligenceMinisterSkillBonus(const FString& OwnerIso) const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Characters || !Tick)
	{
		return 0;
	}
	return FMath::RoundToInt(
		Characters->GetMinisterEffectFactor(NormalizeIso(OwnerIso), EWLMinisterOffice::Intelligence)
		* Tick->GetBalanceRules().IntelligenceMinisterSpyBonus);
}

bool UWLPoliticalSubsystem::HasQueuedUnresolvedEvent(const FString& NationIso, const FString& EventId) const
{
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLPoliticalEventInstance& Event : EventQueue)
	{
		if (!Event.bResolved && Event.NationIso == Iso && Event.EventId == EventId)
		{
			return true;
		}
	}
	return false;
}

void UWLPoliticalSubsystem::QueueTriggeredEventsForNation(const FString& NationIso)
{
	const FWLInternalPowerState State = GetInternalPower(NationIso);
	for (const FWLPoliticalEventDefinition& Def : EventDefinitions)
	{
		if (HasQueuedUnresolvedEvent(State.NationIso, Def.EventId))
		{
			continue;
		}
		if (State.CoupRisk < Def.MinCoupRisk
			|| State.OppositionStrength < Def.MinOppositionStrength
			|| State.AveragePublicOrder > Def.MaxPublicOrder)
		{
			continue;
		}
		FWLPoliticalEventInstance Event;
		Event.InstanceId = FString::Printf(TEXT("EV-%04d"), NextEventInstanceNumber++);
		Event.EventId = Def.EventId;
		Event.NationIso = State.NationIso;
		Event.TargetIso = NormalizeIso(Def.TargetIso);
		Event.Title = Def.Title;
		Event.Body = Def.Body;
		Event.Options = Def.Options;

		// F5.5: la agenda del lider modifica las opciones. Un lider autoritario reprime mas fuerte (y mas
		// sucio); un reformista negocia mas barato; uno polarizante agrava el coste de ceder o ignorar.
		const TArray<FString> AgendaTraits = GetLeaderAgendaTraits(State.NationIso);
		for (FWLPoliticalEventOption& Option : Event.Options)
		{
			for (const FString& Trait : AgendaTraits)
			{
				const FString T = Trait.ToLower();
				if (T == TEXT("autoritarismo") && Option.OppositionDelta < 0)
				{
					Option.OppositionDelta = FMath::RoundToInt(Option.OppositionDelta * 1.5f);
					Option.PublicOrderDelta -= 1;
				}
				else if (T == TEXT("reformista") && Option.PoliticalCapitalDelta < 0)
				{
					Option.PoliticalCapitalDelta = Option.PoliticalCapitalDelta / 2;
				}
				else if (T == TEXT("polarizante") && Option.OppositionDelta > 0)
				{
					Option.OppositionDelta = FMath::RoundToInt(Option.OppositionDelta * 1.5f);
				}
			}
		}

		if (State.NationIso == GetPlayerNationIso())
		{
			AddNews(FString::Printf(TEXT("Nuevo evento: %s — decision pendiente en POLITICA."), *Event.Title));
		}
		EventQueue.Add(MoveTemp(Event));
	}
}

TArray<FWLPoliticalEventInstance> UWLPoliticalSubsystem::GetQueuedEvents(const FString& NationIso) const
{
	TArray<FWLPoliticalEventInstance> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLPoliticalEventInstance& Event : EventQueue)
	{
		if (!Event.bResolved && (Iso.IsEmpty() || Event.NationIso == Iso))
		{
			Out.Add(Event);
		}
	}
	Out.Sort([](const FWLPoliticalEventInstance& A, const FWLPoliticalEventInstance& B)
	{
		return A.InstanceId < B.InstanceId;
	});
	return Out;
}

bool UWLPoliticalSubsystem::ResolveEvent(
	const FString& InstanceId,
	const FString& OptionId,
	FString& OutMessage)
{
	FWLPoliticalEventInstance* Event = EventQueue.FindByPredicate(
		[&InstanceId](const FWLPoliticalEventInstance& E)
		{
			return E.InstanceId == InstanceId && !E.bResolved;
		});
	if (!Event)
	{
		OutMessage = FString::Printf(TEXT("Evento no disponible: %s"), *InstanceId);
		return false;
	}
	const FWLPoliticalEventOption* Option = Event->Options.FindByPredicate(
		[&OptionId](const FWLPoliticalEventOption& O)
		{
			return O.OptionId == OptionId;
		});
	if (!Option)
	{
		OutMessage = FString::Printf(TEXT("Opcion no disponible: %s"), *OptionId);
		return false;
	}

	const FString EventNationIso = NormalizeIso(Event->NationIso);
	if (!ValidateNation(EventNationIso))
	{
		OutMessage = FString::Printf(TEXT("Evento %s tiene nacion invalida: %s"), *Event->InstanceId, *Event->NationIso);
		return false;
	}

	const FString RelationTargetIso = NormalizeIso(Event->TargetIso);
	if (Option->RelationDelta != 0
		&& (!ValidateNation(RelationTargetIso) || RelationTargetIso == EventNationIso))
	{
		OutMessage = FString::Printf(TEXT("Evento %s requiere objetivo diplomatico valido."), *Event->InstanceId);
		return false;
	}

	Event->NationIso = EventNationIso;
	Event->TargetIso = RelationTargetIso;

	FWLInternalPowerState& State = EnsureInternalPower(EventNationIso);
	State.OppositionStrength = ClampPercent(State.OppositionStrength + Option->OppositionDelta);
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		if (Option->TreasuryDelta != 0)
		{
			FString TreasuryMessage;
			Tick->AdjustTreasury(EventNationIso, Option->TreasuryDelta, TreasuryMessage);
		}
		if (Option->PublicOrderDelta != 0)
		{
			Tick->AdjustNationPublicOrder(EventNationIso, Option->PublicOrderDelta);
		}
		if (!Option->MarketShockGoodId.TrimStartAndEnd().IsEmpty()
			&& Option->MarketShockPriceMultiplier > 0.0
			&& Option->MarketShockDurationMonths > 0)
		{
			FString ShockMessage;
			Tick->ApplyMarketShock(
				Option->MarketShockGoodId,
				Option->MarketShockPriceMultiplier,
				Option->MarketShockDurationMonths,
				Option->MarketShockTitle,
				Event->EventId,
				ShockMessage);
		}
	}
	if (Option->PoliticalCapitalDelta != 0)
	{
		if (UWLCharacterSubsystem* Characters = GetCharacters())
		{
			Characters->AdjustPoliticalCapital(EventNationIso, Option->PoliticalCapitalDelta);
		}
	}
	if (Option->RelationDelta != 0)
	{
		FString RelationMessage;
		AdjustRelationOpinion(EventNationIso, RelationTargetIso, Option->RelationDelta, RelationMessage);
	}
	if (Option->OppositionDelta > 0 || Option->PublicOrderDelta < 0)
	{
		AddPoliticalMemory(EventNationIso, TEXT("crisis_unrest"), 1, 8,
			FString::Printf(TEXT("Evento %s eleva tension social."), *Event->EventId));
	}
	if (Option->OppositionDelta < 0 && Option->PublicOrderDelta < 0)
	{
		AddPoliticalMemory(EventNationIso, TEXT("recent_repression"), 1, 10,
			FString::Printf(TEXT("Evento %s resuelto con coercion."), *Event->EventId));
		AdjustPublicGroupSupport(EventNationIso, EWLPublicGroup::Workers, -2, TEXT("represion reciente"));
		AdjustPublicGroupSupport(EventNationIso, EWLPublicGroup::Unions, -2, TEXT("represion reciente"));
	}
	if (Option->PoliticalCapitalDelta < 0 && Option->OppositionDelta < 0)
	{
		AddPoliticalMemory(EventNationIso, TEXT("negotiated_concession"), 1, 8,
			FString::Printf(TEXT("Evento %s deja concesiones politicas."), *Event->EventId));
		AdjustPublicGroupSupport(EventNationIso, EWLPublicGroup::MiddleClass, 1, TEXT("concesion politica"));
	}
	Event->bResolved = true;
	UpdateInternalPowerForNation(EventNationIso);
	OutMessage = FString::Printf(TEXT("Evento %s resuelto con %s."), *Event->EventId, *Option->OptionId);
	return true;
}

void UWLPoliticalSubsystem::CheckCampaignOutcome()
{
	if (CampaignOutcome.bGameOver)
	{
		return;
	}
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick)
	{
		return;
	}

	TMap<FString, int32> ControlledByNation;
	int32 TotalProvinces = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		const FString Controller = Tick->GetProvinceControllerIso(Province.Id);
		if (!Controller.IsEmpty())
		{
			++TotalProvinces;
			ControlledByNation.FindOrAdd(Controller) += 1;
		}
	}
	for (const TPair<FString, int32>& Pair : ControlledByNation)
	{
		if (TotalProvinces > 0 && Pair.Value == TotalProvinces)
		{
			CampaignOutcome.bGameOver = true;
			CampaignOutcome.OutcomeType = TEXT("Domination");
			CampaignOutcome.WinningNationIso = Pair.Key;
			CampaignOutcome.Reason = FString::Printf(TEXT("%s controla todas las provincias."), *Pair.Key);
			return;
		}
	}

	// F5.3: victorias no militares. Meses de campana derivados de la fecha (sin estado nuevo que guardar).
	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const int32 MonthsElapsed =
		(Tick->GetCurrentYear() - Rules.StartYear) * Rules.MonthsPerYear
		+ (Tick->GetCurrentMonth() - Rules.StartMonth);

	// Hegemonia: concentrar la cuota configurada del PIB total (cualquier nacion puede lograrla).
	if (MonthsElapsed >= Rules.HegemonyMinMonths)
	{
		int64 TotalGDP = 0;
		TMap<FString, int64> GDPByNation;
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			const int64 GDP = Tick->GetNationGDP(Nation.Iso);
			GDPByNation.Add(Nation.Iso, GDP);
			TotalGDP += GDP;
		}
		if (TotalGDP > 0)
		{
			for (const TPair<FString, int64>& Pair : GDPByNation)
			{
				const double Share = static_cast<double>(Pair.Value) / static_cast<double>(TotalGDP);
				if (Share >= Rules.HegemonyGDPShare)
				{
					CampaignOutcome.bGameOver = true;
					CampaignOutcome.OutcomeType = TEXT("Hegemony");
					CampaignOutcome.WinningNationIso = Pair.Key;
					CampaignOutcome.Reason = FString::Printf(
						TEXT("%s concentra el %.0f%% del PIB continental."), *Pair.Key, Share * 100.0);
					return;
				}
			}
		}
	}

	// Regimen/Legado: el jugador sobrevive en el poder los meses configurados.
	const FString PlayerIso = GetPlayerNationIso();
	if (!PlayerIso.IsEmpty() && MonthsElapsed >= Rules.RegimeVictoryMonths)
	{
		CampaignOutcome.bGameOver = true;
		CampaignOutcome.OutcomeType = TEXT("Regime");
		CampaignOutcome.WinningNationIso = PlayerIso;
		CampaignOutcome.Reason = FString::Printf(
			TEXT("Te mantuviste en el poder %d meses: tu legado esta asegurado."), MonthsElapsed);
	}
}

void UWLPoliticalSubsystem::WriteSaveSnapshot(
	TArray<FWLInternalPowerState>& OutInternalPower,
	TArray<FWLDiplomaticRelationState>& OutRelations,
	TArray<FWLIntelligenceNetworkState>& OutNetworks,
	TArray<FWLPoliticalEventInstance>& OutEvents,
	FWLCampaignOutcomeState& OutOutcome) const
{
	InternalPowerByNation.GenerateValueArray(OutInternalPower);
	RelationsByPair.GenerateValueArray(OutRelations);
	IntelligenceByPair.GenerateValueArray(OutNetworks);
	OutEvents = EventQueue;
	OutOutcome = CampaignOutcome;
	OutInternalPower.Sort([](const FWLInternalPowerState& A, const FWLInternalPowerState& B) { return A.NationIso < B.NationIso; });
	OutRelations.Sort([](const FWLDiplomaticRelationState& A, const FWLDiplomaticRelationState& B)
	{
		return (A.NationA + A.NationB) < (B.NationA + B.NationB);
	});
	OutNetworks.Sort([](const FWLIntelligenceNetworkState& A, const FWLIntelligenceNetworkState& B)
	{
		return (A.OwnerIso + A.TargetIso) < (B.OwnerIso + B.TargetIso);
	});
}

void UWLPoliticalSubsystem::WriteGovernmentSaveSnapshot(
	TArray<FWLGovernmentAgendaState>& OutAgendas,
	TArray<FWLMinistryProgramState>& OutPrograms,
	TArray<FWLCabinetDynamicsState>& OutCabinetDynamics,
	TArray<FWLInstitutionalPowerState>& OutInstitutions,
	TArray<FWLPublicGroupSupportState>& OutPublicGroups,
	TArray<FWLStateCapacityState>& OutStateCapacity,
	TArray<FWLPoliticalMemoryRecord>& OutMemory,
	TArray<FWLPoliticalAIPlanState>& OutAIPlans) const
{
	GovernmentAgendaByNation.GenerateValueArray(OutAgendas);
	OutPrograms = ActiveMinistryPrograms;
	CabinetDynamicsByNation.GenerateValueArray(OutCabinetDynamics);
	InstitutionalPowerByNation.GenerateValueArray(OutInstitutions);
	PublicGroupSupportByKey.GenerateValueArray(OutPublicGroups);
	StateCapacityByNation.GenerateValueArray(OutStateCapacity);
	PoliticalMemoryByKey.GenerateValueArray(OutMemory);
	GovernmentAIPlanByNation.GenerateValueArray(OutAIPlans);

	OutAgendas.Sort([](const FWLGovernmentAgendaState& A, const FWLGovernmentAgendaState& B) { return A.NationIso < B.NationIso; });
	OutPrograms.Sort([](const FWLMinistryProgramState& A, const FWLMinistryProgramState& B)
	{
		return A.NationIso == B.NationIso ? A.ProgramId < B.ProgramId : A.NationIso < B.NationIso;
	});
	OutCabinetDynamics.Sort([](const FWLCabinetDynamicsState& A, const FWLCabinetDynamicsState& B) { return A.NationIso < B.NationIso; });
	OutInstitutions.Sort([](const FWLInstitutionalPowerState& A, const FWLInstitutionalPowerState& B) { return A.NationIso < B.NationIso; });
	OutPublicGroups.Sort([](const FWLPublicGroupSupportState& A, const FWLPublicGroupSupportState& B)
	{
		return A.NationIso == B.NationIso
			? static_cast<int32>(A.Group) < static_cast<int32>(B.Group)
			: A.NationIso < B.NationIso;
	});
	OutStateCapacity.Sort([](const FWLStateCapacityState& A, const FWLStateCapacityState& B) { return A.NationIso < B.NationIso; });
	OutMemory.Sort([](const FWLPoliticalMemoryRecord& A, const FWLPoliticalMemoryRecord& B)
	{
		return A.NationIso == B.NationIso ? A.MemoryKey < B.MemoryKey : A.NationIso < B.NationIso;
	});
	OutAIPlans.Sort([](const FWLPoliticalAIPlanState& A, const FWLPoliticalAIPlanState& B) { return A.NationIso < B.NationIso; });
}

void UWLPoliticalSubsystem::WriteGovernmentP2SaveSnapshot(
	TArray<FWLActiveReformState>& OutReforms,
	TArray<FWLEnactedPolicyReformState>& OutEnactedReforms,
	TArray<FWLPartyState>& OutParties,
	TArray<FWLElectionState>& OutElections,
	TArray<FWLCharacterPoliticalProfile>& OutCharacterProfiles,
	TArray<FWLPatronageState>& OutPatronage,
	TArray<FWLMediaPublicOpinionState>& OutMedia,
	TArray<FWLRegionGovernorState>& OutRegions,
	TArray<FWLCrisisChainState>& OutCrisisChains,
	TArray<FWLGovernmentCalibrationState>& OutCalibration) const
{
	OutReforms = ActivePolicyReforms;
	OutEnactedReforms = EnactedPolicyReforms;
	OutParties = PoliticalParties;
	ElectionStateByNation.GenerateValueArray(OutElections);
	CharacterPoliticalProfilesById.GenerateValueArray(OutCharacterProfiles);
	PatronageStateByNation.GenerateValueArray(OutPatronage);
	MediaStateByNation.GenerateValueArray(OutMedia);
	OutRegions = RegionGovernors;
	OutCrisisChains = CrisisChains;
	GovernmentCalibrationByNation.GenerateValueArray(OutCalibration);

	OutReforms.Sort([](const FWLActiveReformState& A, const FWLActiveReformState& B)
	{
		return A.NationIso == B.NationIso ? A.ReformId < B.ReformId : A.NationIso < B.NationIso;
	});
	OutEnactedReforms.Sort([](const FWLEnactedPolicyReformState& A, const FWLEnactedPolicyReformState& B)
	{
		return A.NationIso == B.NationIso ? A.ReformId < B.ReformId : A.NationIso < B.NationIso;
	});
	OutParties.Sort([](const FWLPartyState& A, const FWLPartyState& B)
	{
		return A.NationIso == B.NationIso ? A.PartyId < B.PartyId : A.NationIso < B.NationIso;
	});
	OutElections.Sort([](const FWLElectionState& A, const FWLElectionState& B) { return A.NationIso < B.NationIso; });
	OutCharacterProfiles.Sort([](const FWLCharacterPoliticalProfile& A, const FWLCharacterPoliticalProfile& B)
	{
		return A.NationIso == B.NationIso ? A.CharacterId < B.CharacterId : A.NationIso < B.NationIso;
	});
	OutPatronage.Sort([](const FWLPatronageState& A, const FWLPatronageState& B) { return A.NationIso < B.NationIso; });
	OutMedia.Sort([](const FWLMediaPublicOpinionState& A, const FWLMediaPublicOpinionState& B) { return A.NationIso < B.NationIso; });
	OutRegions.Sort([](const FWLRegionGovernorState& A, const FWLRegionGovernorState& B)
	{
		return A.NationIso == B.NationIso ? A.RegionId < B.RegionId : A.NationIso < B.NationIso;
	});
	OutCrisisChains.Sort([](const FWLCrisisChainState& A, const FWLCrisisChainState& B)
	{
		return A.NationIso == B.NationIso ? A.CrisisId < B.CrisisId : A.NationIso < B.NationIso;
	});
	OutCalibration.Sort([](const FWLGovernmentCalibrationState& A, const FWLGovernmentCalibrationState& B)
	{
		return A.NationIso < B.NationIso;
	});
}

bool UWLPoliticalSubsystem::RestoreSaveSnapshot(
	const TArray<FWLInternalPowerState>& SavedInternalPower,
	const TArray<FWLDiplomaticRelationState>& SavedRelations,
	const TArray<FWLIntelligenceNetworkState>& SavedNetworks,
	const TArray<FWLPoliticalEventInstance>& SavedEvents,
	const FWLCampaignOutcomeState& SavedOutcome,
	FString& OutMessage)
{
	ResetPoliticalState();

	for (FWLInternalPowerState State : SavedInternalPower)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.AveragePublicOrder = ClampPercent(State.AveragePublicOrder);
			State.OppositionStrength = ClampPercent(State.OppositionStrength);
			State.OppositionPopularity = ClampPercent(State.OppositionPopularity);
			State.ExternalCoupFunding = ClampPercent(State.ExternalCoupFunding);
			State.CoupRisk = ClampPercent(State.CoupRisk);
			InternalPowerByNation.Add(Iso, MoveTemp(State));
		}
	}
	for (FWLDiplomaticRelationState Relation : SavedRelations)
	{
		if (ValidateNation(Relation.NationA) && ValidateNation(Relation.NationB))
		{
			Relation.NationA = NormalizeIso(Relation.NationA);
			Relation.NationB = NormalizeIso(Relation.NationB);
			Relation.Opinion = FMath::Clamp(Relation.Opinion, -100, 100);
			RelationsByPair.Add(RelationKey(Relation.NationA, Relation.NationB), MoveTemp(Relation));
		}
	}
	for (FWLIntelligenceNetworkState Network : SavedNetworks)
	{
		if (ValidateNation(Network.OwnerIso) && ValidateNation(Network.TargetIso))
		{
			Network.OwnerIso = NormalizeIso(Network.OwnerIso);
			Network.TargetIso = NormalizeIso(Network.TargetIso);
			Network.NetworkStrength = ClampPercent(Network.NetworkStrength);
			Network.Exposure = ClampPercent(Network.Exposure);
			IntelligenceByPair.Add(NetworkKey(Network.OwnerIso, Network.TargetIso), MoveTemp(Network));
		}
	}
	EventQueue.Reset();
	for (FWLPoliticalEventInstance Event : SavedEvents)
	{
		Event.NationIso = NormalizeIso(Event.NationIso);
		Event.TargetIso = NormalizeIso(Event.TargetIso);
		if (!ValidateNation(Event.NationIso))
		{
			continue;
		}
		if (!Event.TargetIso.IsEmpty()
			&& (!ValidateNation(Event.TargetIso) || Event.TargetIso == Event.NationIso))
		{
			continue;
		}
		if (Event.InstanceId.IsEmpty() || Event.EventId.IsEmpty() || Event.Options.IsEmpty())
		{
			continue;
		}
		EventQueue.Add(MoveTemp(Event));
	}
	for (const FWLPoliticalEventInstance& Event : EventQueue)
	{
		if (Event.InstanceId.StartsWith(TEXT("EV-")))
		{
			NextEventInstanceNumber = FMath::Max(NextEventInstanceNumber, FCString::Atoi(*Event.InstanceId.Mid(3)) + 1);
		}
	}
	CampaignOutcome = SavedOutcome;
	OutMessage = FString::Printf(TEXT("Politica restaurada: %d estados, %d relaciones, %d redes, %d eventos."),
		InternalPowerByNation.Num(), RelationsByPair.Num(), IntelligenceByPair.Num(), EventQueue.Num());
	return true;
}

bool UWLPoliticalSubsystem::RestoreGovernmentP2SaveSnapshot(
	const TArray<FWLActiveReformState>& SavedReforms,
	const TArray<FWLEnactedPolicyReformState>& SavedEnactedReforms,
	const TArray<FWLPartyState>& SavedParties,
	const TArray<FWLElectionState>& SavedElections,
	const TArray<FWLCharacterPoliticalProfile>& SavedCharacterProfiles,
	const TArray<FWLPatronageState>& SavedPatronage,
	const TArray<FWLMediaPublicOpinionState>& SavedMedia,
	const TArray<FWLRegionGovernorState>& SavedRegions,
	const TArray<FWLCrisisChainState>& SavedCrisisChains,
	const TArray<FWLGovernmentCalibrationState>& SavedCalibration,
	FString& OutMessage)
{
	ActivePolicyReforms.Reset();
	EnactedPolicyReforms.Reset();
	TSet<FString> RestoredActiveReformKeys;
	for (FWLActiveReformState Reform : SavedReforms)
	{
		Reform.NationIso = NormalizeIso(Reform.NationIso);
		FWLPolicyReformDefinition Definition;
		if (!ValidateNation(Reform.NationIso)
			|| !GetPolicyReformDefinition(Reform.ReformId, Definition)
			|| Reform.MonthsRemaining <= 0)
		{
			continue;
		}
		Reform.ReformId = Definition.ReformId;
		Reform.Name = Definition.Name;
		Reform.Area = Definition.Area;
		Reform.MonthsRemaining = FMath::Max(0, Reform.MonthsRemaining);
		Reform.ImplementationProgress = ClampPercent(Reform.ImplementationProgress);
		Reform.Backlash = ClampPercent(Reform.Backlash);
		const FString ReformKey = Reform.NationIso + TEXT("|") + Reform.ReformId;
		if (RestoredActiveReformKeys.Contains(ReformKey))
		{
			continue;
		}
		RestoredActiveReformKeys.Add(ReformKey);
		ActivePolicyReforms.Add(MoveTemp(Reform));
	}
	for (FWLEnactedPolicyReformState Reform : SavedEnactedReforms)
	{
		Reform.NationIso = NormalizeIso(Reform.NationIso);
		Reform.ReformId = Reform.ReformId.TrimStartAndEnd().ToLower();
		FWLPolicyReformDefinition Definition;
		if (!ValidateNation(Reform.NationIso)
			|| !GetPolicyReformDefinition(Reform.ReformId, Definition)
			|| HasEnactedPolicyReform(Reform.NationIso, Reform.ReformId))
		{
			continue;
		}
		Reform.ReformId = Definition.ReformId;
		Reform.Name = Definition.Name;
		Reform.Area = Definition.Area;
		Reform.MonthsSinceEnacted = FMath::Max(0, Reform.MonthsSinceEnacted);
		EnactedPolicyReforms.Add(MoveTemp(Reform));
	}
	ActivePolicyReforms.RemoveAll([this](const FWLActiveReformState& Reform)
	{
		return HasEnactedPolicyReform(Reform.NationIso, Reform.ReformId);
	});

	if (!SavedParties.IsEmpty())
	{
		PoliticalParties.Reset();
		TSet<FString> RestoredPartyKeys;
		for (FWLPartyState Party : SavedParties)
		{
			Party.NationIso = NormalizeIso(Party.NationIso);
			Party.PartyId = Party.PartyId.TrimStartAndEnd().ToLower();
			const FString Key = PartyKey(Party.NationIso, Party.PartyId);
			if (!ValidateNation(Party.NationIso) || Party.PartyId.IsEmpty() || RestoredPartyKeys.Contains(Key))
			{
				continue;
			}
			RestoredPartyKeys.Add(Key);
			Party.Seats = FMath::Clamp(Party.Seats, 0, 100);
			Party.Discipline = ClampPercent(Party.Discipline);
			Party.LoyaltyToGovernment = ClampPercent(Party.LoyaltyToGovernment);
			Party.Corruption = ClampPercent(Party.Corruption);
			Party.MonthsSinceInternalElection = FMath::Max(0, Party.MonthsSinceInternalElection);
			Party.LeaderCharacterId = NormalizeCharacterId(Party.LeaderCharacterId);
			PoliticalParties.Add(MoveTemp(Party));
		}
	}
	if (const UWLDataRegistry* Registry = GetRegistry())
	{
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			SeedPoliticalPartiesForNation(Nation.Iso);
		}
	}

	if (!SavedElections.IsEmpty())
	{
		ElectionStateByNation.Reset();
		for (FWLElectionState Election : SavedElections)
		{
			Election.NationIso = NormalizeIso(Election.NationIso);
			if (!ValidateNation(Election.NationIso))
			{
				continue;
			}
			Election.MonthsToElection = FMath::Clamp(Election.MonthsToElection, 0, ElectionCycleMonths);
			Election.IncumbentApproval = ClampPercent(Election.IncumbentApproval);
			Election.PollingGovernment = ClampPercent(Election.PollingGovernment);
			Election.PollingOpposition = ClampPercent(Election.PollingOpposition);
			Election.CampaignIntensity = ClampPercent(Election.CampaignIntensity);
			Election.Legitimacy = ClampPercent(Election.Legitimacy);
			Election.FraudRisk = ClampPercent(Election.FraudRisk);
			Election.AbstentionRisk = ClampPercent(Election.AbstentionRisk);
			Election.ConsecutiveTermsWon = FMath::Clamp(Election.ConsecutiveTermsWon, 0, 8);
			Election.CampaignPromiseReformId = Election.CampaignPromiseReformId.TrimStartAndEnd().ToLower();
			ElectionStateByNation.Add(Election.NationIso, MoveTemp(Election));
		}
	}

	if (!SavedCharacterProfiles.IsEmpty())
	{
		CharacterPoliticalProfilesById.Reset();
		for (FWLCharacterPoliticalProfile Profile : SavedCharacterProfiles)
		{
			Profile.NationIso = NormalizeIso(Profile.NationIso);
			Profile.CharacterId = NormalizeCharacterId(Profile.CharacterId);
			if (!ValidateNation(Profile.NationIso) || Profile.CharacterId.IsEmpty())
			{
				continue;
			}
			Profile.PresidentialAmbition = ClampPercent(Profile.PresidentialAmbition);
			Profile.PersonalCorruption = ClampPercent(Profile.PersonalCorruption);
			Profile.ScandalHeat = ClampPercent(Profile.ScandalHeat);
			Profile.SuccessionScore = ClampPercent(Profile.SuccessionScore);
			Profile.PatronCharacterId = NormalizeCharacterId(Profile.PatronCharacterId);
			for (FString& RivalId : Profile.RivalCharacterIds)
			{
				RivalId = NormalizeCharacterId(RivalId);
			}
			for (FString& AllyId : Profile.AllyCharacterIds)
			{
				AllyId = NormalizeCharacterId(AllyId);
			}
			CharacterPoliticalProfilesById.Add(Profile.CharacterId, MoveTemp(Profile));
		}
	}
	if (const UWLDataRegistry* Registry = GetRegistry())
	{
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			SeedCharacterProfilesForNation(Nation.Iso);
		}
	}

	if (!SavedPatronage.IsEmpty())
	{
		PatronageStateByNation.Reset();
		for (FWLPatronageState Patronage : SavedPatronage)
		{
			Patronage.NationIso = NormalizeIso(Patronage.NationIso);
			if (!ValidateNation(Patronage.NationIso))
			{
				continue;
			}
			Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower);
			Patronage.ClientelistPressure = ClampPercent(Patronage.ClientelistPressure);
			Patronage.ContractCorruption = ClampPercent(Patronage.ContractCorruption);
			Patronage.RegionalMachines = ClampPercent(Patronage.RegionalMachines);
			Patronage.ConcessionBacklash = ClampPercent(Patronage.ConcessionBacklash);
			PatronageStateByNation.Add(Patronage.NationIso, MoveTemp(Patronage));
		}
	}

	if (!SavedMedia.IsEmpty())
	{
		MediaStateByNation.Reset();
		for (FWLMediaPublicOpinionState Media : SavedMedia)
		{
			Media.NationIso = NormalizeIso(Media.NationIso);
			if (!ValidateNation(Media.NationIso))
			{
				continue;
			}
			Media.PressFreedom = ClampPercent(Media.PressFreedom);
			Media.MediaControl = ClampPercent(Media.MediaControl);
			Media.PresidentialApproval = ClampPercent(Media.PresidentialApproval);
			Media.PropagandaReach = ClampPercent(Media.PropagandaReach);
			Media.CensorshipBacklash = ClampPercent(Media.CensorshipBacklash);
			Media.FakeNewsPressure = ClampPercent(Media.FakeNewsPressure);
			Media.MediaCrisisRisk = ClampPercent(Media.MediaCrisisRisk);
			MediaStateByNation.Add(Media.NationIso, MoveTemp(Media));
		}
	}

	if (!SavedRegions.IsEmpty())
	{
		RegionGovernors.Reset();
		TSet<FString> RestoredRegionKeys;
		for (FWLRegionGovernorState Region : SavedRegions)
		{
			Region.NationIso = NormalizeIso(Region.NationIso);
			Region.RegionId = Region.RegionId.TrimStartAndEnd();
			const FString RegionKey = Region.NationIso + TEXT("|") + Region.RegionId;
			if (!ValidateNation(Region.NationIso) || Region.RegionId.IsEmpty() || RestoredRegionKeys.Contains(RegionKey))
			{
				continue;
			}
			RestoredRegionKeys.Add(RegionKey);
			Region.Obedience = ClampPercent(Region.Obedience);
			Region.Autonomy = ClampPercent(Region.Autonomy);
			Region.ProtestRisk = ClampPercent(Region.ProtestRisk);
			Region.CenterControl = ClampPercent(Region.CenterControl);
			Region.InvestmentLevel = ClampPercent(Region.InvestmentLevel);
			Region.SecessionRisk = ClampPercent(Region.SecessionRisk);
			Region.RebellionRisk = ClampPercent(Region.RebellionRisk);
			RegionGovernors.Add(MoveTemp(Region));
		}
	}
	if (const UWLDataRegistry* Registry = GetRegistry())
	{
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			SeedRegionsForNation(Nation.Iso);
		}
	}

	CrisisChains.Reset();
	for (FWLCrisisChainState Crisis : SavedCrisisChains)
	{
		Crisis.NationIso = NormalizeIso(Crisis.NationIso);
		if (!ValidateNation(Crisis.NationIso))
		{
			continue;
		}
		Crisis.CrisisId = Crisis.CrisisId.TrimStartAndEnd();
		if (Crisis.CrisisId.IsEmpty())
		{
			Crisis.CrisisId = CrisisChainKey(Crisis.NationIso, Crisis.Type);
		}
		Crisis.Stage = FMath::Clamp(Crisis.Stage, 1, 4);
		Crisis.Intensity = ClampPercent(Crisis.Intensity);
		Crisis.MonthsActive = FMath::Max(0, Crisis.MonthsActive);
		CrisisChains.Add(MoveTemp(Crisis));
	}

	if (!SavedCalibration.IsEmpty())
	{
		GovernmentCalibrationByNation.Reset();
		for (FWLGovernmentCalibrationState Calibration : SavedCalibration)
		{
			Calibration.NationIso = NormalizeIso(Calibration.NationIso);
			if (!ValidateNation(Calibration.NationIso))
			{
				continue;
			}
			Calibration.DebtVsGrowthPressure = ClampPercent(Calibration.DebtVsGrowthPressure);
			Calibration.RepressionLegitimacyPressure = ClampPercent(Calibration.RepressionLegitimacyPressure);
			Calibration.SubsidyInflationPressure = ClampPercent(Calibration.SubsidyInflationPressure);
			Calibration.ReformGridlockPressure = ClampPercent(Calibration.ReformGridlockPressure);
			Calibration.CivilMilitaryTension = ClampPercent(Calibration.CivilMilitaryTension);
			Calibration.SovereigntyAlliancePressure = ClampPercent(Calibration.SovereigntyAlliancePressure);
			Calibration.MonthsObserved = FMath::Max(0, Calibration.MonthsObserved);
			GovernmentCalibrationByNation.Add(Calibration.NationIso, MoveTemp(Calibration));
		}
	}

	OutMessage = FString::Printf(
		TEXT("Gobierno P2 restaurado: %d reformas activas, %d reformas consolidadas, %d partidos, %d elecciones, %d regiones, %d crisis."),
		ActivePolicyReforms.Num(),
		EnactedPolicyReforms.Num(),
		PoliticalParties.Num(),
		ElectionStateByNation.Num(),
		RegionGovernors.Num(),
		CrisisChains.Num());
	return true;
}

bool UWLPoliticalSubsystem::RestoreGovernmentSaveSnapshot(
	const TArray<FWLGovernmentAgendaState>& SavedAgendas,
	const TArray<FWLMinistryProgramState>& SavedPrograms,
	const TArray<FWLCabinetDynamicsState>& SavedCabinetDynamics,
	const TArray<FWLInstitutionalPowerState>& SavedInstitutions,
	const TArray<FWLPublicGroupSupportState>& SavedPublicGroups,
	const TArray<FWLStateCapacityState>& SavedStateCapacity,
	const TArray<FWLPoliticalMemoryRecord>& SavedMemory,
	const TArray<FWLPoliticalAIPlanState>& SavedAIPlans,
	FString& OutMessage)
{
	SeedGovernmentState();

	for (FWLGovernmentAgendaState Agenda : SavedAgendas)
	{
		const FString Iso = NormalizeIso(Agenda.NationIso);
		if (!ValidateNation(Iso) || Agenda.Priorities.IsEmpty())
		{
			continue;
		}
		if (Agenda.Priorities.Num() > MaxAgendaPriorities)
		{
			Agenda.Priorities.SetNum(MaxAgendaPriorities);
		}
		Agenda.NationIso = Iso;
		Agenda.MonthsActive = FMath::Max(0, Agenda.MonthsActive);
		GovernmentAgendaByNation.Add(Iso, MoveTemp(Agenda));
	}

	ActiveMinistryPrograms.Reset();
	for (FWLMinistryProgramState Program : SavedPrograms)
	{
		Program.NationIso = NormalizeIso(Program.NationIso);
		FWLMinistryProgramDefinition Definition;
		if (!ValidateNation(Program.NationIso)
			|| !GetMinistryProgramDefinition(Program.ProgramId, Definition)
			|| Program.RemainingMonths <= 0)
		{
			continue;
		}
		Program.Name = Definition.Name;
		Program.Office = Definition.Office;
		Program.Progress = ClampPercent(Program.Progress);
		ActiveMinistryPrograms.Add(MoveTemp(Program));
	}

	for (FWLCabinetDynamicsState State : SavedCabinetDynamics)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.RivalryPressure = ClampPercent(State.RivalryPressure);
			State.Factionalism = ClampPercent(State.Factionalism);
			State.ScandalRisk = ClampPercent(State.ScandalRisk);
			State.SabotageRisk = ClampPercent(State.SabotageRisk);
			State.ResignationRisk = ClampPercent(State.ResignationRisk);
			CabinetDynamicsByNation.Add(Iso, MoveTemp(State));
		}
	}
	for (FWLInstitutionalPowerState State : SavedInstitutions)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.RulingCoalitionSupport = ClampPercent(State.RulingCoalitionSupport);
			State.LegislativeOpposition = ClampPercent(State.LegislativeOpposition);
			State.ReformCost = FMath::Clamp(State.ReformCost, 0, 100);
			State.GridlockRisk = ClampPercent(State.GridlockRisk);
			InstitutionalPowerByNation.Add(Iso, MoveTemp(State));
		}
	}
	for (FWLPublicGroupSupportState State : SavedPublicGroups)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.Support = ClampPercent(State.Support);
			State.Pressure = ClampPercent(State.Pressure);
			PublicGroupSupportByKey.Add(PublicGroupKey(Iso, State.Group), MoveTemp(State));
		}
	}
	for (FWLStateCapacityState State : SavedStateCapacity)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.Bureaucracy = ClampPercent(State.Bureaucracy);
			State.Corruption = ClampPercent(State.Corruption);
			State.AdministrativeEfficiency = ClampPercent(State.AdministrativeEfficiency);
			State.CentralAuthority = ClampPercent(State.CentralAuthority);
			State.PolicyFailureRisk = ClampPercent(State.PolicyFailureRisk);
			StateCapacityByNation.Add(Iso, MoveTemp(State));
		}
	}
	for (FWLPoliticalMemoryRecord Memory : SavedMemory)
	{
		const FString Iso = NormalizeIso(Memory.NationIso);
		Memory.MemoryKey = Memory.MemoryKey.TrimStartAndEnd().ToLower();
		if (ValidateNation(Iso) && !Memory.MemoryKey.IsEmpty() && Memory.MonthsRemaining > 0 && Memory.Value != 0)
		{
			Memory.NationIso = Iso;
			Memory.Value = FMath::Clamp(Memory.Value, -100, 100);
			Memory.MonthsRemaining = FMath::Max(0, Memory.MonthsRemaining);
			PoliticalMemoryByKey.Add(PoliticalMemoryKey(Iso, Memory.MemoryKey), MoveTemp(Memory));
		}
	}
	for (FWLPoliticalAIPlanState Plan : SavedAIPlans)
	{
		const FString Iso = NormalizeIso(Plan.NationIso);
		if (ValidateNation(Iso))
		{
			Plan.NationIso = Iso;
			Plan.TargetIso = NormalizeIso(Plan.TargetIso);
			if (!Plan.TargetIso.IsEmpty() && !ValidateNation(Plan.TargetIso))
			{
				Plan.TargetIso.Reset();
			}
			Plan.MonthsOnPlan = FMath::Max(0, Plan.MonthsOnPlan);
			GovernmentAIPlanByNation.Add(Iso, MoveTemp(Plan));
		}
	}

	OutMessage = FString::Printf(
		TEXT("Gobierno restaurado: %d agendas, %d programas, %d grupos, %d memorias."),
		GovernmentAgendaByNation.Num(),
		ActiveMinistryPrograms.Num(),
		PublicGroupSupportByKey.Num(),
		PoliticalMemoryByKey.Num());
	return true;
}
