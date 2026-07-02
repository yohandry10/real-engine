// Copyright World Leader project. See ROADMAP.md.
//
// Test funcional del balance editable de Phase 1. Garantiza que economia y
// construccion pueden evaluarse con reglas distintas a los defaults.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Balance/WLBalanceTypes.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Core/WLGameTypes.h"
#include "Economy/WLEconomyLibrary.h"
#include "Engine/GameInstance.h"
#include "Politics/WLPoliticalSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLBalanceRulesEconomyTest,
	"WorldLeader.Balance.CustomRulesAffectEconomy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLBalanceRulesEconomyTest::RunTest(const FString& Parameters)
{
	FWLBalanceRules Rules;
	Rules.OilPrice = 10;
	Rules.GasPrice = 20;
	Rules.FoodPrice = 30;
	Rules.MineralsPrice = 40;
	Rules.IndustryValue = 50;
	Rules.PopulationTaxPerCapita = 0.1;
	Rules.InfrastructureUpkeepFactor = 3;

	FWLProvinceData Province;
	Province.BaseOil = 2;
	Province.BaseGas = 3;
	Province.BaseFood = 4;
	Province.BaseMinerals = 5;
	Province.BaseIndustry = 6;
	Province.Population = 1000;
	Province.Infrastructure = 7;

	TestEqual(TEXT("Ingreso provincial con reglas custom"),
		UWLEconomyLibrary::CalculateProvinceIncomeWithRules(Province, Rules), static_cast<int64>(800));
	TestEqual(TEXT("Upkeep provincial con reglas custom"),
		UWLEconomyLibrary::CalculateProvinceUpkeepWithRules(Province, Rules), static_cast<int64>(21));
	TestEqual(TEXT("Balance provincial con reglas custom"),
		UWLEconomyLibrary::CalculateProvinceBalanceWithRules(Province, Rules), static_cast<int64>(779));

	FWLBuildingData Building;
	Building.BonusOil = 1;
	Building.BonusGas = 2;
	Building.BonusFood = 3;
	Building.BonusMinerals = 4;
	Building.BonusIndustry = 5;

	TestEqual(TEXT("Ingreso edificio con reglas custom"),
		UWLEconomyLibrary::CalculateBuildingIncomeWithRules(Building, Rules), static_cast<int64>(550));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTaxLeverLafferTest,
	"WorldLeader.Balance.TaxLeverLaffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTaxLeverLafferTest::RunTest(const FString& Parameters)
{
	const FWLBalanceRules Rules = FWLBalanceRules::Default();

	const double AtDefault = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(Rules.TaxRateDefaultPercent, Rules);
	TestEqual(TEXT("Tasa default recauda x1.0"), AtDefault, 1.0, 1e-9);

	const double AtMax = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(Rules.TaxRateMaxPercent, Rules);
	const double AtMin = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(Rules.TaxRateMinPercent, Rules);
	TestTrue(TEXT("Subir impuestos recauda mas"), AtMax > 1.0);
	TestTrue(TEXT("Bajar impuestos recauda menos"), AtMin < 1.0);

	// Rendimiento decreciente (Laffer): doblar la tasa NO dobla la recaudacion.
	const double RateRatio = static_cast<double>(Rules.TaxRateMaxPercent) / static_cast<double>(Rules.TaxRateDefaultPercent);
	TestTrue(TEXT("Curva Laffer: rendimiento decreciente"), AtMax < RateRatio);

	// Fuera de rango se clampa a los limites de la palanca.
	TestEqual(TEXT("Tasa sobre el maximo clampeada"),
		UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(100, Rules), AtMax, 1e-9);
	TestEqual(TEXT("Tasa bajo el minimo clampeada"),
		UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(0, Rules), AtMin, 1e-9);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLAIDifficultyTuningTest,
	"WorldLeader.Balance.AIDifficultyTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLAIDifficultyTuningTest::RunTest(const FString& Parameters)
{
	FWLBalanceRules Easy = FWLBalanceRules::Default();
	FWLBalanceRules Medium = FWLBalanceRules::Default();
	FWLBalanceRules Hard = FWLBalanceRules::Default();
	Easy.AIDifficulty = EWLAIDifficulty::Easy;
	Medium.AIDifficulty = EWLAIDifficulty::Medium;
	Hard.AIDifficulty = EWLAIDifficulty::Hard;

	TestEqual(TEXT("ID facil"), Easy.GetAIDifficultyId(), FString(TEXT("easy")));
	TestEqual(TEXT("ID medio"), Medium.GetAIDifficultyId(), FString(TEXT("medium")));
	TestEqual(TEXT("ID dificil"), Hard.GetAIDifficultyId(), FString(TEXT("hard")));
	TestTrue(TEXT("Dificil construye mas por mes"),
		Hard.GetEconomicAIMaxBuildsForDifficulty() > Medium.GetEconomicAIMaxBuildsForDifficulty());
	TestTrue(TEXT("Facil conserva mas reserva"),
		Easy.GetEconomicAIMinTreasuryReserveForDifficulty() > Medium.GetEconomicAIMinTreasuryReserveForDifficulty());
	TestTrue(TEXT("Dificil acepta retornos mas largos"),
		Hard.GetEconomicAIMaxPaybackMonthsForDifficulty() > Medium.GetEconomicAIMaxPaybackMonthsForDifficulty());
	TestTrue(TEXT("Dificil declara guerra con menos ventaja"),
		Hard.GetStrategicAIWarStrengthRatio() < Medium.GetStrategicAIWarStrengthRatio());
	TestTrue(TEXT("Facil evita mas la guerra"),
		Easy.GetStrategicAIWarOpinionThreshold() < Medium.GetStrategicAIWarOpinionThreshold());
	TestTrue(TEXT("Dificil espia con mas riesgo"),
		Hard.GetStrategicAISpyExposureLimit() > Medium.GetStrategicAISpyExposureLimit());

	FWLBalanceRules Invalid = FWLBalanceRules::Default();
	Invalid.AIDifficulty = static_cast<EWLAIDifficulty>(255);
	TestEqual(TEXT("Dificultad invalida sanea a medio"),
		static_cast<int32>(Invalid.Sanitized().AIDifficulty), static_cast<int32>(EWLAIDifficulty::Medium));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLNationBudgetBreakdownTest,
	"WorldLeader.Balance.NationBudgetBreakdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLNationBudgetBreakdownTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	const FWLNationBudget Budget = Tick->GetNationBudget(TEXT("VE"));
	TestTrue(TEXT("Ingreso por recursos/produccion"), Budget.ResourceIncome > 0);
	TestTrue(TEXT("Ingreso por impuestos"), Budget.TaxIncome > 0);
	TestTrue(TEXT("Gasto militar (fuerzas desplegadas)"), Budget.MilitaryUpkeep > 0);
	TestTrue(TEXT("Gasto en infraestructura"), Budget.InfrastructureUpkeep > 0);
	TestTrue(TEXT("Salarios publicos"), Budget.PublicWages > 0);
	TestTrue(TEXT("Gasto social"), Budget.SocialSpending > 0);
	TestEqual(TEXT("El neto del presupuesto ES el balance mensual"),
		Budget.Net(), Tick->GetMonthlyBalance(TEXT("VE")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLProvinceSectorsProductionTest,
	"WorldLeader.Economy.ProvinceSectorsProduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLProvinceSectorsProductionTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	auto FindUnits = [](const TArray<FWLGoodOutput>& Production, const FString& GoodId) -> int64
	{
		for (const FWLGoodOutput& Out : Production)
		{
			if (Out.GoodId == GoodId)
			{
				return Out.Units;
			}
		}
		return 0;
	};

	// CO-ARA (Arauca): rica en petroleo (base 720) y poco poblada -> el trabajo limita la produccion.
	const FWLSectorEmployment Arauca = Tick->GetProvinceEmployment(TEXT("CO-ARA"));
	TestTrue(TEXT("Arauca tiene fuerza laboral"), Arauca.Workforce > 0);
	TestTrue(TEXT("Arauca esta limitada por trabajo"), Arauca.LaborCoverage > 0.0 && Arauca.LaborCoverage < 1.0);
	TestTrue(TEXT("Empleo por sectores repartido"), Arauca.Extraction > 0 && Arauca.Services >= 0);

	const TArray<FWLGoodOutput> AraucaProduction = Tick->GetProvinceProduction(TEXT("CO-ARA"));
	const int64 AraucaOil = FindUnits(AraucaProduction, TEXT("oil"));
	TestTrue(TEXT("Arauca produce petroleo"), AraucaOil > 0);
	TestTrue(TEXT("La falta de trabajo subproduce petroleo"), AraucaOil < 720);

	// CO-ANT (Antioquia): muy poblada -> trabajo pleno; la industria saca manufacturados por IndustryShare.
	const FWLSectorEmployment Antioquia = Tick->GetProvinceEmployment(TEXT("CO-ANT"));
	TestEqual(TEXT("Antioquia con trabajo pleno"), Antioquia.LaborCoverage, 1.0);
	const TArray<FWLGoodOutput> AntioquiaProduction = Tick->GetProvinceProduction(TEXT("CO-ANT"));
	TestTrue(TEXT("Antioquia produce acero"), FindUnits(AntioquiaProduction, TEXT("steel")) > 0);
	TestTrue(TEXT("Antioquia produce bienes de consumo"), FindUnits(AntioquiaProduction, TEXT("consumer_goods")) > 0);

	// Agregado nacional: la produccion de CO incluye al menos el petroleo de Arauca.
	TestTrue(TEXT("La nacion agrega la produccion provincial"),
		FindUnits(Tick->GetNationProduction(TEXT("CO")), TEXT("oil")) >= AraucaOil);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLGDPAndGrowthTest,
	"WorldLeader.Balance.GDPAndGrowth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLGDPAndGrowthTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	const int64 GDP = Tick->GetNationGDP(TEXT("VE"));
	TestTrue(TEXT("PIB positivo"), GDP > 0);
	TestTrue(TEXT("El PIB supera el ingreso fiscal (actividad total > recaudacion)"),
		GDP > Tick->GetNationBudget(TEXT("VE")).TotalIncome());
	TestEqual(TEXT("Sin ticks aun no hay tasa de crecimiento"), Tick->GetNationGDPGrowth(TEXT("VE")), 0.0);

	// Dos ticks: el primero fija la base, el segundo mide crecimiento (la poblacion crece con orden estable).
	Tick->AdvanceMonth();
	Tick->AdvanceMonth();
	const double Growth = Tick->GetNationGDPGrowth(TEXT("VE"));
	TestTrue(TEXT("Crecimiento medido tras dos ticks"), Growth != 0.0);
	TestTrue(TEXT("Crecimiento en rango razonable (+-10%)"), FMath::Abs(Growth) < 0.10);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLEconomicGovernanceBackendTest,
	"WorldLeader.Economy.GovernanceFE6",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLEconomicGovernanceBackendTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	TestNotNull(TEXT("Character subsystem"), Characters);
	if (!Tick || !Characters)
	{
		GameInstance->Shutdown();
		return false;
	}

	const FWLEconomicGovernanceStats Initial = Tick->GetEconomicGovernanceStats(TEXT("CO"));
	TestEqual(TEXT("Ministro de economia conectado"), Initial.EconomyMinisterId, FString(TEXT("CO-MIN-ECO-ROJAS")));
	TestTrue(TEXT("Corrupcion sistemica en rango"), Initial.SystemicCorruption >= 0 && Initial.SystemicCorruption <= 100);
	TestTrue(TEXT("Multiplicador fiscal positivo"), Initial.TaxCollectionMultiplier > 0.0);
	TestTrue(TEXT("Multiplicador productividad positivo"), Initial.ProductivityMultiplier > 0.0);
	TestTrue(TEXT("Perdida por corrupcion entra al presupuesto"),
		Tick->GetNationBudget(TEXT("CO")).CorruptionLoss > 0);

	FString Message;
	TestTrue(TEXT("Nombrar ministra economica alternativa"),
		Characters->AppointMinister(TEXT("CO"), EWLMinisterOffice::Economy, TEXT("CO-MIN-ECO-ALT-SALCEDO"), Message));
	const FWLEconomicGovernanceStats AfterMinister = Tick->GetEconomicGovernanceStats(TEXT("CO"));
	TestEqual(TEXT("Nuevo ministro conectado"), AfterMinister.EconomyMinisterId, FString(TEXT("CO-MIN-ECO-ALT-SALCEDO")));
	TestTrue(TEXT("Mejor skill sube eficiencia fiscal"),
		AfterMinister.TaxCollectionMultiplier > Initial.TaxCollectionMultiplier);

	const int32 TechnologyBefore = AfterMinister.TechnologyLevel;
	const double ProductivityBefore = AfterMinister.ProductivityMultiplier;
	TestTrue(TEXT("Construir campus tecnologico"),
		Tick->BuildBuilding(TEXT("CO-DC"), TEXT("research_campus"), Message));
	const FWLEconomicGovernanceStats AfterTech = Tick->GetEconomicGovernanceStats(TEXT("CO"));
	TestTrue(TEXT("Tecnologia nacional sube"), AfterTech.TechnologyLevel > TechnologyBefore);
	TestTrue(TEXT("Tecnologia sube productividad"),
		AfterTech.ProductivityMultiplier > ProductivityBefore);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLNationalMarketAndMacroTest,
	"WorldLeader.Economy.NationalMarketAndMacro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLNationalMarketAndMacroTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	const TArray<FWLGoodMarketBalance> Market = Tick->GetNationGoodMarketBalance(TEXT("CO"));
	TestTrue(TEXT("Mercado nacional con bienes"), Market.Num() >= 10);

	auto FindBalance = [&Market](const FString& GoodId) -> const FWLGoodMarketBalance*
	{
		for (const FWLGoodMarketBalance& Balance : Market)
		{
			if (Balance.GoodId == GoodId)
			{
				return &Balance;
			}
		}
		return nullptr;
	};

	const FWLGoodMarketBalance* Oil = FindBalance(TEXT("oil"));
	const FWLGoodMarketBalance* Steel = FindBalance(TEXT("steel"));
	const FWLGoodMarketBalance* ConsumerGoods = FindBalance(TEXT("consumer_goods"));
	TestTrue(TEXT("Balance de petroleo"), Oil != nullptr);
	TestTrue(TEXT("Balance de acero"), Steel != nullptr);
	TestTrue(TEXT("Balance de bienes de consumo"), ConsumerGoods != nullptr);
	if (Oil)
	{
		TestTrue(TEXT("Petroleo se produce"), Oil->Production > 0);
		TestTrue(TEXT("Petroleo se consume como insumo de combustible"), Oil->UsedAsInput > 0);
		TestTrue(TEXT("Precio de petroleo positivo"), Oil->UnitPrice > 0.0);
	}
	if (Steel)
	{
		TestTrue(TEXT("Acero se produce"), Steel->Production > 0);
		TestTrue(TEXT("Acero se consume en cadenas superiores"), Steel->UsedAsInput > 0);
	}
	if (ConsumerGoods)
	{
		TestTrue(TEXT("Bienes de consumo tienen demanda"), ConsumerGoods->Demand > 0);
	}

	bool bHasTrade = false;
	for (const FWLGoodMarketBalance& Balance : Market)
	{
		TestTrue(FString::Printf(TEXT("Precio valido para %s"), *Balance.GoodId), Balance.UnitPrice > 0.0);
		TestTrue(FString::Printf(TEXT("Multiplicador valido para %s"), *Balance.GoodId), Balance.PriceMultiplier > 0.0);
		bHasTrade = bHasTrade || Balance.Imports > 0 || Balance.Exports > 0;
	}
	TestTrue(TEXT("Mercado regional genera importaciones o exportaciones"), bHasTrade);

	const FWLNationLaborStats Labor = Tick->GetNationLaborStats(TEXT("CO"));
	TestTrue(TEXT("Fuerza laboral nacional"), Labor.Workforce > 0);
	TestTrue(TEXT("Desempleo en rango"), Labor.UnemploymentRate >= 0.0 && Labor.UnemploymentRate <= 1.0);
	TestTrue(TEXT("Productividad en rango"), Labor.Productivity > 0.0 && Labor.Productivity <= 1.0);

	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const double Inflation = Tick->GetNationInflationRate(TEXT("CO"));
	TestTrue(TEXT("Inflacion en rango saneado"),
		Inflation >= Rules.MinMonthlyInflationRate && Inflation <= Rules.MaxMonthlyInflationRate);
	TestFalse(TEXT("Etiqueta de ciclo economico"), Tick->GetNationEconomicCycleLabel(TEXT("CO")).IsEmpty());

	const FWLNationBudget Budget = Tick->GetNationBudget(TEXT("CO"));
	TestTrue(TEXT("Exportaciones o importaciones entran al presupuesto"),
		Budget.ExportIncome > 0 || Budget.ImportCost > 0);
	TestEqual(TEXT("Balance mensual incluye comercio"), Budget.Net(), Tick->GetMonthlyBalance(TEXT("CO")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLMarketShocksFE34Test,
	"WorldLeader.Economy.MarketShocksFE34",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLMarketShocksFE34Test::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	auto FindBalance = [](const TArray<FWLGoodMarketBalance>& Market, const FString& GoodId) -> const FWLGoodMarketBalance*
	{
		for (const FWLGoodMarketBalance& Balance : Market)
		{
			if (Balance.GoodId == GoodId)
			{
				return &Balance;
			}
		}
		return nullptr;
	};

	const FWLGoodMarketBalance* OilBefore = FindBalance(Tick->GetNationGoodMarketBalance(TEXT("CO")), TEXT("oil"));
	TestTrue(TEXT("Petroleo en mercado inicial"), OilBefore != nullptr);
	if (!OilBefore)
	{
		GameInstance->Shutdown();
		return false;
	}
	const double OilPriceBefore = OilBefore->UnitPrice;
	const double InflationBefore = Tick->GetNationInflationRate(TEXT("CO"));

	FString Message;
	TestTrue(TEXT("Aplicar shock petrolero"),
		Tick->ApplyMarketShock(TEXT("oil"), 1.75, 2, TEXT("Crisis petrolera"), TEXT("test_event"), Message));
	TestEqual(TEXT("Un shock activo"), Tick->GetActiveMarketShocks().Num(), 1);
	TestEqual(TEXT("Multiplicador especifico"), Tick->GetMarketShockMultiplier(TEXT("oil")), 1.75, 1e-9);
	TestEqual(TEXT("Bien no afectado queda neutro"), Tick->GetMarketShockMultiplier(TEXT("consumer_goods")), 1.0, 1e-9);

	const FWLGoodMarketBalance* OilAfter = FindBalance(Tick->GetNationGoodMarketBalance(TEXT("CO")), TEXT("oil"));
	TestTrue(TEXT("Petroleo en mercado con shock"), OilAfter != nullptr);
	if (OilAfter)
	{
		TestTrue(TEXT("Shock sube precio unitario"), OilAfter->UnitPrice > OilPriceBefore);
		TestTrue(TEXT("Balance expone multiplicador de shock"), OilAfter->MarketShockMultiplier > 1.0);
	}
	TestTrue(TEXT("Shock presiona inflacion"),
		Tick->GetNationInflationRate(TEXT("CO")) >= InflationBefore);

	int32 Year = 0;
	int32 Month = 0;
	TArray<FWLNationTreasurySave> Treasuries;
	TArray<FWLProvinceBuildingsSave> Buildings;
	TArray<FWLProvinceRuntimeState> ProvinceStates;
	TArray<FWLMarketShockState> Shocks;
	Tick->WriteSaveSnapshot(Year, Month, Treasuries, Buildings, ProvinceStates, &Shocks);
	TestEqual(TEXT("Snapshot guarda shocks"), Shocks.Num(), 1);

	UGameInstance* RestoredGameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("Restored GameInstance"), RestoredGameInstance);
	if (!RestoredGameInstance)
	{
		GameInstance->Shutdown();
		return false;
	}
	RestoredGameInstance->Init();
	UWLStrategicTickSubsystem* RestoredTick = RestoredGameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Restored tick subsystem"), RestoredTick);
	if (!RestoredTick)
	{
		RestoredGameInstance->Shutdown();
		GameInstance->Shutdown();
		return false;
	}

	TestTrue(TEXT("Restaurar shock de mercado"),
		RestoredTick->RestoreSaveSnapshot(Year, Month, Treasuries, Buildings, ProvinceStates, Shocks, Message));
	TestEqual(TEXT("Shock restaurado"), RestoredTick->GetActiveMarketShocks().Num(), 1);
	TestEqual(TEXT("Multiplicador restaurado"), RestoredTick->GetMarketShockMultiplier(TEXT("oil")), 1.75, 1e-9);
	RestoredTick->AdvanceMonth();
	TestEqual(TEXT("Shock queda con un mes"), RestoredTick->GetActiveMarketShocks()[0].RemainingMonths, 1);
	RestoredTick->AdvanceMonth();
	TestEqual(TEXT("Shock expira"), RestoredTick->GetActiveMarketShocks().Num(), 0);

	RestoredGameInstance->Shutdown();
	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLAdvancedTradeFE4Test,
	"WorldLeader.Economy.AdvancedTradeFE4",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLAdvancedTradeFE4Test::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	TestNotNull(TEXT("Political subsystem"), Politics);
	if (!Tick || !Politics)
	{
		GameInstance->Shutdown();
		return false;
	}

	auto TotalTradeVolume = [Tick](const FString& NationIso) -> int64
	{
		int64 Volume = 0;
		for (const FWLGoodMarketBalance& Balance : Tick->GetNationGoodMarketBalance(NationIso))
		{
			Volume += Balance.Imports + Balance.Exports;
		}
		return Volume;
	};
	auto HasImports = [Tick](const FString& NationIso) -> bool
	{
		for (const FWLGoodMarketBalance& Balance : Tick->GetNationGoodMarketBalance(NationIso))
		{
			if (Balance.Imports > 0)
			{
				return true;
			}
		}
		return false;
	};

	const TArray<FWLTradeRouteState> RoutesBefore = Tick->GetTradeRoutesForNation(TEXT("CO"));
	TestTrue(TEXT("CO tiene rutas comerciales"), RoutesBefore.Num() > 0);
	const double RouteAccessBefore = RoutesBefore.Num() > 0 ? RoutesBefore[0].AccessMultiplier : 0.0;
	const int64 TradeBefore = TotalTradeVolume(TEXT("CO"));

	FString Message;
	TestTrue(TEXT("Opinion permite acuerdo comercial"),
		Politics->SetRelationOpinion(TEXT("CO"), TEXT("VE"), 30, Message));
	TestTrue(TEXT("Firmar acuerdo comercial"),
		Politics->SignTreaty(TEXT("CO"), TEXT("VE"), EWLTreatyType::TradeAgreement, Message));
	const FWLTradeRouteState AgreementRoute = Tick->GetTradeRouteBetween(TEXT("CO"), TEXT("VE"));
	TestTrue(TEXT("Ruta con acuerdo abierta"), AgreementRoute.bOpen);
	TestTrue(TEXT("Acuerdo comercial marcado"), AgreementRoute.bTradeAgreement);
	TestTrue(TEXT("Acuerdo sube acceso"), AgreementRoute.AccessMultiplier > RouteAccessBefore);
	TestTrue(TEXT("Acuerdo no reduce volumen comercial"), TotalTradeVolume(TEXT("CO")) >= TradeBefore);

	const FString TariffNation = HasImports(TEXT("CO")) ? FString(TEXT("CO")) : FString(TEXT("VE"));
	const FString TariffPartner = TariffNation == TEXT("CO") ? FString(TEXT("VE")) : FString(TEXT("CO"));
	TestTrue(TEXT("La nacion de prueba tiene importaciones"), HasImports(TariffNation));
	FWLDiplomaticRelationState RelationBeforeTariff;
	TestTrue(TEXT("Relacion antes de arancel"), Politics->GetRelation(TariffNation, TariffPartner, RelationBeforeTariff));
	TestTrue(TEXT("Subir arancel via politica"),
		Politics->SetNationTariffRate(TariffNation, 25, Message));
	TestEqual(TEXT("Arancel aplicado en economia"), Tick->GetTariffRate(TariffNation), 25);
	FWLDiplomaticRelationState RelationAfterTariff;
	TestTrue(TEXT("Relacion despues de arancel"), Politics->GetRelation(TariffNation, TariffPartner, RelationAfterTariff));
	TestTrue(TEXT("Arancel penaliza relacion"), RelationAfterTariff.Opinion < RelationBeforeTariff.Opinion);
	TestTrue(TEXT("Arancel genera ingreso si hay importaciones"),
		Tick->GetNationBudget(TariffNation).TariffIncome > 0);
	bool bSawTariffOnImport = false;
	for (const FWLGoodMarketBalance& Balance : Tick->GetNationGoodMarketBalance(TariffNation))
	{
		if (Balance.Imports > 0)
		{
			bSawTariffOnImport = bSawTariffOnImport || Balance.ImportTariffIncome > 0;
			TestEqual(TEXT("Balance expone tasa arancelaria"), Balance.TariffRatePercent, 25);
			TestTrue(TEXT("Arancel reduce volumen importable"), Balance.TariffImportVolumeMultiplier < 1.0);
		}
	}
	TestTrue(TEXT("Algun bien importado paga arancel"), bSawTariffOnImport);

	TestTrue(TEXT("Embargo se firma como sancion"),
		Politics->SignTreaty(TEXT("CO"), TEXT("VE"), EWLTreatyType::Embargo, Message));
	const FWLTradeRouteState EmbargoRoute = Tick->GetTradeRouteBetween(TEXT("CO"), TEXT("VE"));
	TestFalse(TEXT("Embargo cierra ruta"), EmbargoRoute.bOpen);
	TestTrue(TEXT("Embargo marcado"), EmbargoRoute.bEmbargoed);
	TestEqual(TEXT("Embargo corta acceso"), EmbargoRoute.AccessMultiplier, 0.0);

	TestTrue(TEXT("Declarar guerra corta rutas"),
		Politics->DeclareWar(TEXT("CO"), TEXT("VE"), Message));
	const FWLTradeRouteState WarRoute = Tick->GetTradeRouteBetween(TEXT("CO"), TEXT("VE"));
	TestTrue(TEXT("Guerra marcada en ruta"), WarRoute.bAtWar);
	TestFalse(TEXT("Guerra cierra ruta"), WarRoute.bOpen);

	int32 Year = 0;
	int32 Month = 0;
	TArray<FWLNationTreasurySave> Treasuries;
	TArray<FWLProvinceBuildingsSave> Buildings;
	TArray<FWLProvinceRuntimeState> ProvinceStates;
	TArray<FWLMarketShockState> Shocks;
	Tick->WriteSaveSnapshot(Year, Month, Treasuries, Buildings, ProvinceStates, &Shocks);
	const FWLNationTreasurySave* TariffSave = Treasuries.FindByPredicate([&TariffNation](const FWLNationTreasurySave& Entry)
	{
		return Entry.NationIso == TariffNation;
	});
	TestTrue(TEXT("Tesoro con arancel guardado"), TariffSave != nullptr);
	if (TariffSave)
	{
		TestEqual(TEXT("Arancel guardado"), TariffSave->TariffRatePercent, 25);
	}

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLDebtAndCreditLimitTest,
	"WorldLeader.Balance.DebtAndCreditLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLDebtAndCreditLimitTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const int64 NetWithoutDebt = Tick->GetMonthlyBalance(TEXT("VE"));
	TestEqual(TEXT("Sin deuda no hay intereses"), Tick->GetNationBudget(TEXT("VE")).DebtInterest, static_cast<int64>(0));

	// Forzar deuda: restaurar snapshot con el tesoro de VE en -100000.
	auto RestoreTreasury = [&](int64 Treasury)
	{
		TArray<FWLNationTreasurySave> Treasuries;
		FWLNationTreasurySave Saved;
		Saved.NationIso = TEXT("VE");
		Saved.Treasury = Treasury;
		Treasuries.Add(Saved);
		FString Message;
		return Tick->RestoreSaveSnapshot(Tick->GetCurrentYear(), Tick->GetCurrentMonth(),
			Treasuries, TArray<FWLProvinceBuildingsSave>(), TArray<FWLProvinceRuntimeState>(), Message);
	};
	TestTrue(TEXT("Restaurar tesoro en deuda"), RestoreTreasury(-100000));

	const int64 ExpectedInterest = static_cast<int64>(
		FMath::RoundToDouble(100000.0 * Rules.DebtMonthlyInterestRate));
	TestEqual(TEXT("Interes mensual sobre la deuda"),
		Tick->GetNationBudget(TEXT("VE")).DebtInterest, ExpectedInterest);
	TestEqual(TEXT("El interes drena el balance mensual"),
		Tick->GetMonthlyBalance(TEXT("VE")), NetWithoutDebt - ExpectedInterest);

	// Gastar en deficit genera deuda mientras quede credito.
	const int64 CreditLimit = Tick->GetCreditLimit(TEXT("VE"));
	TestTrue(TEXT("Limite de credito positivo"), CreditLimit > 0);
	FString Message;
	TestTrue(TEXT("Construir en deficit (dentro del credito)"),
		Tick->BuildBuilding(TEXT("VE-ZU"), TEXT("oil_well"), Message));
	TestTrue(TEXT("El gasto en deficit aumenta la deuda"), Tick->GetTreasury(TEXT("VE")) < -100000);

	// Con el credito agotado, el gasto se bloquea.
	TestTrue(TEXT("Restaurar tesoro al limite de credito"), RestoreTreasury(-Tick->GetCreditLimit(TEXT("VE"))));
	TestFalse(TEXT("Credito agotado bloquea construir"),
		Tick->BuildBuilding(TEXT("VE-ZU"), TEXT("oil_well"), Message));
	TestTrue(TEXT("Mensaje explica el credito"), Message.Contains(TEXT("Credito agotado")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLAdvancedFinanceFE5Test,
	"WorldLeader.Economy.AdvancedFinanceFE5",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLAdvancedFinanceFE5Test::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	TestNotNull(TEXT("Political subsystem"), Politics);
	if (!Tick || !Politics)
	{
		GameInstance->Shutdown();
		return false;
	}

	const int64 TreasuryBeforeBond = Tick->GetTreasury(TEXT("CO"));
	FWLFinancialProfile ProfileBefore = Tick->GetFinancialProfile(TEXT("CO"));
	TestTrue(TEXT("Perfil financiero inicial tiene credito"), ProfileBefore.CreditLimit > 0);
	TestFalse(TEXT("CO inicia sin default"), ProfileBefore.bInDefault);

	FString Message;
	TestTrue(TEXT("Emitir bono soberano"),
		Tick->IssueBond(TEXT("CO"), 10000, 24, Message));
	TestTrue(TEXT("Bono aumenta tesoro"),
		Tick->GetTreasury(TEXT("CO")) >= TreasuryBeforeBond + 10000);
	TestTrue(TEXT("Servicio de deuda entra al presupuesto"),
		Tick->GetNationBudget(TEXT("CO")).DebtService > 0);
	TestTrue(TEXT("Perfil refleja deuda emitida"),
		Tick->GetFinancialProfile(TEXT("CO")).OutstandingDebt >= 10000);

	TestTrue(TEXT("Opinion permite financiamiento aliado"),
		Politics->SetRelationOpinion(TEXT("VE"), TEXT("CO"), 60, Message));
	TestTrue(TEXT("Registrar prestamo bilateral"),
		Tick->RegisterBilateralLoan(TEXT("VE"), TEXT("CO"), 5000, 12, Message));
	const TArray<FWLFinancialInstrumentState> InstrumentsAfterLoan = Tick->GetFinancialInstrumentsForNation(TEXT("CO"));
	TestTrue(TEXT("CO tiene bono y prestamo"),
		InstrumentsAfterLoan.Num() >= 2);

	TestTrue(TEXT("Registrar ayuda exterior"),
		Tick->GrantForeignAid(TEXT("VE"), TEXT("CO"), 1000, 2, Message));
	TestTrue(TEXT("Ayuda entra como ingreso fiscal"),
		Tick->GetNationBudget(TEXT("CO")).ForeignAidIncome > 0);

	TestTrue(TEXT("Registrar FDI"),
		Tick->StartForeignInvestment(TEXT("VE"), TEXT("CO"), TEXT("CO-DC"), TEXT("financial_center"), 1200, 1, Message));
	TestTrue(TEXT("FDI visible como inflow"),
		Tick->GetNationBudget(TEXT("CO")).ForeignInvestmentInflow > 0);

	Tick->AdvanceMonth();
	TestTrue(TEXT("FDI construye edificio al completar"),
		Tick->GetProvinceBuildings(TEXT("CO-DC")).Contains(TEXT("financial_center")));
	bool bAidStillActive = false;
	for (const FWLForeignSupportState& Support : Tick->GetForeignSupportForNation(TEXT("CO")))
	{
		if (Support.Type == EWLForeignSupportType::ForeignAid && Support.IsActive())
		{
			bAidStillActive = true;
		}
	}
	TestTrue(TEXT("Ayuda exterior persiste tras un mes"), bAidStillActive);

	TestTrue(TEXT("Default manual marca deuda"),
		Tick->MarkDebtDefault(TEXT("CO"), Message));
	const FWLFinancialProfile DefaultProfile = Tick->GetFinancialProfile(TEXT("CO"));
	TestTrue(TEXT("Perfil detecta default"), DefaultProfile.bInDefault);
	TestEqual(TEXT("Rating default"), DefaultProfile.CreditRatingLabel, FString(TEXT("Default")));
	TestTrue(TEXT("FMI disponible tras default"),
		Tick->RequestIMFProgram(TEXT("CO"), 1000, 24, Message));
	bool bHasIMF = false;
	for (const FWLFinancialInstrumentState& Instrument : Tick->GetFinancialInstrumentsForNation(TEXT("CO")))
	{
		bHasIMF = bHasIMF || Instrument.Type == EWLFinancialInstrumentType::IMFProgram;
	}
	TestTrue(TEXT("Programa FMI queda registrado"), bHasIMF);

	int32 Year = 0;
	int32 Month = 0;
	TArray<FWLNationTreasurySave> Treasuries;
	TArray<FWLProvinceBuildingsSave> Buildings;
	TArray<FWLProvinceRuntimeState> ProvinceStates;
	TArray<FWLMarketShockState> Shocks;
	TArray<FWLFinancialInstrumentState> Financial;
	TArray<FWLForeignSupportState> Support;
	Tick->WriteSaveSnapshot(Year, Month, Treasuries, Buildings, ProvinceStates, &Shocks, &Financial, &Support);
	TestTrue(TEXT("Instrumentos financieros guardados"), Financial.Num() > 0);
	TestTrue(TEXT("Apoyo exterior guardado"), Support.Num() > 0);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLBalanceRulesSanitizeTest,
	"WorldLeader.Balance.SanitizeRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLBalanceRulesSanitizeTest::RunTest(const FString& Parameters)
{
	FWLBalanceRules Rules;
	Rules.OilPrice = -10;
	Rules.PopulationTaxPerCapita = -1.0;
	Rules.MonthlyPopulationGrowthRate = -0.25;
	Rules.InitialPublicOrder = 250;
	Rules.PublicOrderNeutral = 0;
	Rules.LowOrderIncomePenaltyAtZero = 2.0;
	Rules.HighOrderIncomeBonusAtMax = -1.0;
	Rules.PublicOrderDriftPerMonth = -5;
	Rules.PublicOrderDeficitPenalty = -3;
	Rules.PublicOrderBankruptcyPenalty = -4;
	Rules.OccupationPublicOrderPenalty = 250;
	Rules.InfrastructureUpkeepFactor = -2;
	Rules.PublicWagesPerCapita = -1.0;
	Rules.SocialSpendingPerCapita = -2.0;
	Rules.DebtMonthlyInterestRate = 5.0;
	Rules.DebtCreditLimitIncomeMonths = -3.0;
	Rules.GDPPerCapitaActivity = -0.5;
	Rules.LaborParticipationRate = 7.0;
	Rules.WorkersPerBasePoint = -5;
	Rules.SectorOutputPerBasePoint = -1.0;
	Rules.ServiceEmploymentPerCapita = 7.0;
	Rules.UnemploymentProductivityPenalty = 2.0;
	Rules.PriceShortageSensitivity = -0.5;
	Rules.PriceSurplusSensitivity = -0.5;
	Rules.MinMarketPriceMultiplier = -1.0;
	Rules.MaxMarketPriceMultiplier = 0.2;
	Rules.MinMarketShockPriceMultiplier = -0.5;
	Rules.MaxMarketShockPriceMultiplier = 0.1;
	Rules.MaxMarketShockDurationMonths = -3;
	Rules.RegionalTradeAccess = 4.0;
	Rules.GlobalTradeAccess = -1.0;
	Rules.ImportPriceMarkup = -0.1;
	Rules.ExportPriceDiscount = 2.0;
	Rules.TradeAgreementAccessBonus = -0.5;
	Rules.TariffRateDefaultPercent = 90;
	Rules.TariffRateMaxPercent = 40;
	Rules.TariffImportPenaltyPerPoint = 2.0;
	Rules.TariffRelationPenaltyPerPoint = -1.0;
	Rules.EmbargoTradeRouteAccessMultiplier = -0.5;
	Rules.WarTradeRouteAccessMultiplier = -0.25;
	Rules.TensionTradeRouteAccessMultiplier = -0.1;
	Rules.InflationPressureToMonthlyRate = -0.3;
	Rules.BondBaseMonthlyInterestRate = 2.0;
	Rules.BilateralLoanMonthlyInterestRate = -0.5;
	Rules.IMFMonthlyInterestRate = 3.0;
	Rules.FinancialInstrumentMaxMonths = -12;
	Rules.MaxDebtServiceIncomeRatio = 2.0;
	Rules.DefaultDebtServiceIncomeRatio = -0.5;
	Rules.DefaultPublicOrderPenalty = 250;
	Rules.ForeignAidMinOpinion = -150;
	Rules.ForeignInvestmentMinOpinion = 150;
	Rules.ForeignSupportMaxMonths = -4;
	Rules.ForeignAidMonthlyCapIncomeRatio = 2.0;
	Rules.ForeignInvestmentMonthlyCapIncomeRatio = -0.5;
	Rules.GeneralSkillCombatEffectAtMax = 2.0;
	Rules.TacticalMoveSpeedUnitsPerSecond = -10.0;
	Rules.TacticalAttackRangeUnits = -20.0;
	Rules.TacticalDamagePerAttackPerSecond = -0.5;
	Rules.TacticalDefenseMitigationPerPoint = -0.1;
	Rules.TacticalMoraleDamagePerHealth = -2.0;
	Rules.TacticalRoutMoraleThreshold = 250;
	Rules.TacticalObjectiveCaptureSeconds = -3.0;
	Rules.MaxMonthlyInflationRate = 2.0;
	Rules.MinMonthlyInflationRate = -2.0;
	Rules.CycleUnemploymentWeight = -0.1;
	Rules.CycleInflationWeight = -0.2;
	Rules.CycleTradeBalanceWeight = -0.3;
	Rules.EconomicAIMinTreasuryReserve = -5000;
	Rules.EconomicAIMaxBuildsPerNationPerMonth = -2;
	Rules.EconomicAIMaxPaybackMonths = -12;
	Rules.EconomicAIMinPublicOrderToBuild = 250;
	Rules.StartYear = -5;
	Rules.StartMonth = 99;
	Rules.MonthsPerYear = 6;

	const FWLBalanceRules Sanitized = Rules.Sanitized();
	TestEqual(TEXT("Precio negativo saneado"), Sanitized.OilPrice, 0);
	TestEqual(TEXT("Impuesto negativo saneado"), Sanitized.PopulationTaxPerCapita, 0.0);
	TestEqual(TEXT("Crecimiento negativo saneado"), Sanitized.MonthlyPopulationGrowthRate, 0.0);
	TestEqual(TEXT("Orden publico inicial clamp"), Sanitized.InitialPublicOrder, 100);
	TestEqual(TEXT("Orden publico neutral clamp"), Sanitized.PublicOrderNeutral, 1);
	TestEqual(TEXT("Penalizacion ingreso clamp"), Sanitized.LowOrderIncomePenaltyAtZero, 1.0);
	TestEqual(TEXT("Bonus ingreso clamp"), Sanitized.HighOrderIncomeBonusAtMax, 0.0);
	TestEqual(TEXT("Deriva orden publico saneada"), Sanitized.PublicOrderDriftPerMonth, 0);
	TestEqual(TEXT("Penalizacion deficit saneada"), Sanitized.PublicOrderDeficitPenalty, 0);
	TestEqual(TEXT("Penalizacion bancarrota saneada"), Sanitized.PublicOrderBankruptcyPenalty, 0);
	TestEqual(TEXT("Penalizacion ocupacion clamp"), Sanitized.OccupationPublicOrderPenalty, 100);
	TestEqual(TEXT("Upkeep negativo saneado"), Sanitized.InfrastructureUpkeepFactor, 0);
	TestEqual(TEXT("Salarios negativos saneados"), Sanitized.PublicWagesPerCapita, 0.0);
	TestEqual(TEXT("Gasto social negativo saneado"), Sanitized.SocialSpendingPerCapita, 0.0);
	TestEqual(TEXT("Interes de deuda clamp"), Sanitized.DebtMonthlyInterestRate, 1.0);
	TestEqual(TEXT("Limite de credito negativo saneado"), Sanitized.DebtCreditLimitIncomeMonths, 0.0);
	TestEqual(TEXT("Actividad PIB negativa saneada"), Sanitized.GDPPerCapitaActivity, 0.0);
	TestEqual(TEXT("Participacion laboral clamp"), Sanitized.LaborParticipationRate, 1.0);
	TestEqual(TEXT("Trabajadores por punto saneados"), Sanitized.WorkersPerBasePoint, 0);
	TestEqual(TEXT("Output por punto saneado"), Sanitized.SectorOutputPerBasePoint, 0.0);
	TestEqual(TEXT("Servicios por capita clamp"), Sanitized.ServiceEmploymentPerCapita, 1.0);
	TestEqual(TEXT("Penalizacion desempleo clamp"), Sanitized.UnemploymentProductivityPenalty, 1.0);
	TestEqual(TEXT("Sensibilidad escasez saneada"), Sanitized.PriceShortageSensitivity, 0.0);
	TestEqual(TEXT("Sensibilidad excedente saneada"), Sanitized.PriceSurplusSensitivity, 0.0);
	TestEqual(TEXT("Multiplicador minimo saneado"), Sanitized.MinMarketPriceMultiplier, 0.0);
	TestEqual(TEXT("Multiplicador maximo respeta minimo"), Sanitized.MaxMarketPriceMultiplier, 0.2);
	TestEqual(TEXT("Shock minimo saneado"), Sanitized.MinMarketShockPriceMultiplier, 0.0);
	TestEqual(TEXT("Shock maximo respeta minimo"), Sanitized.MaxMarketShockPriceMultiplier, 0.1);
	TestEqual(TEXT("Duracion maxima shock saneada"), Sanitized.MaxMarketShockDurationMonths, 1);
	TestEqual(TEXT("Acceso regional clamp"), Sanitized.RegionalTradeAccess, 1.0);
	TestEqual(TEXT("Acceso global clamp"), Sanitized.GlobalTradeAccess, 0.0);
	TestEqual(TEXT("Markup import saneado"), Sanitized.ImportPriceMarkup, 0.0);
	TestEqual(TEXT("Descuento export clamp"), Sanitized.ExportPriceDiscount, 1.0);
	TestEqual(TEXT("Bonus acuerdo comercial saneado"), Sanitized.TradeAgreementAccessBonus, 0.0);
	TestEqual(TEXT("Arancel maximo clamp"), Sanitized.TariffRateMaxPercent, 40);
	TestEqual(TEXT("Arancel default respeta maximo"), Sanitized.TariffRateDefaultPercent, 40);
	TestEqual(TEXT("Penalizacion import por arancel clamp"), Sanitized.TariffImportPenaltyPerPoint, 1.0);
	TestEqual(TEXT("Penalizacion diplomatica por arancel saneada"), Sanitized.TariffRelationPenaltyPerPoint, 0.0);
	TestEqual(TEXT("Embargo route saneado"), Sanitized.EmbargoTradeRouteAccessMultiplier, 0.0);
	TestEqual(TEXT("Guerra route saneado"), Sanitized.WarTradeRouteAccessMultiplier, 0.0);
	TestEqual(TEXT("Tension route saneado"), Sanitized.TensionTradeRouteAccessMultiplier, 0.0);
	TestEqual(TEXT("Inflacion escala saneada"), Sanitized.InflationPressureToMonthlyRate, 0.0);
	TestEqual(TEXT("Bono interes clamp"), Sanitized.BondBaseMonthlyInterestRate, 1.0);
	TestEqual(TEXT("Prestamo interes saneado"), Sanitized.BilateralLoanMonthlyInterestRate, 0.0);
	TestEqual(TEXT("FMI interes clamp"), Sanitized.IMFMonthlyInterestRate, 1.0);
	TestEqual(TEXT("Plazo financiero saneado"), Sanitized.FinancialInstrumentMaxMonths, 1);
	TestEqual(TEXT("Servicio deuda max clamp"), Sanitized.MaxDebtServiceIncomeRatio, 1.0);
	TestEqual(TEXT("Servicio default saneado"), Sanitized.DefaultDebtServiceIncomeRatio, 0.0);
	TestEqual(TEXT("Default orden clamp"), Sanitized.DefaultPublicOrderPenalty, 100);
	TestEqual(TEXT("Opinion ayuda clamp"), Sanitized.ForeignAidMinOpinion, -100);
	TestEqual(TEXT("Opinion FDI clamp"), Sanitized.ForeignInvestmentMinOpinion, 100);
	TestEqual(TEXT("Duracion apoyo saneada"), Sanitized.ForeignSupportMaxMonths, 1);
	TestEqual(TEXT("Cap ayuda clamp"), Sanitized.ForeignAidMonthlyCapIncomeRatio, 1.0);
	TestEqual(TEXT("Cap FDI saneado"), Sanitized.ForeignInvestmentMonthlyCapIncomeRatio, 0.0);
	TestEqual(TEXT("Efecto skill general clamp"), Sanitized.GeneralSkillCombatEffectAtMax, 1.0);
	TestEqual(TEXT("Velocidad tactica saneada"), Sanitized.TacticalMoveSpeedUnitsPerSecond, 0.0);
	TestEqual(TEXT("Alcance tactico saneado"), Sanitized.TacticalAttackRangeUnits, 0.0);
	TestEqual(TEXT("Dano tactico saneado"), Sanitized.TacticalDamagePerAttackPerSecond, 0.0);
	TestEqual(TEXT("Mitigacion tactica saneada"), Sanitized.TacticalDefenseMitigationPerPoint, 0.0);
	TestEqual(TEXT("Moral tactica saneada"), Sanitized.TacticalMoraleDamagePerHealth, 0.0);
	TestEqual(TEXT("Umbral retirada tactica clamp"), Sanitized.TacticalRoutMoraleThreshold, 100);
	TestEqual(TEXT("Captura tactica saneada"), Sanitized.TacticalObjectiveCaptureSeconds, 1.0);
	TestEqual(TEXT("Inflacion maxima clamp"), Sanitized.MaxMonthlyInflationRate, 1.0);
	TestEqual(TEXT("Inflacion minima clamp"), Sanitized.MinMonthlyInflationRate, -1.0);
	TestEqual(TEXT("Peso desempleo ciclo saneado"), Sanitized.CycleUnemploymentWeight, 0.0);
	TestEqual(TEXT("Peso inflacion ciclo saneado"), Sanitized.CycleInflationWeight, 0.0);
	TestEqual(TEXT("Peso comercio ciclo saneado"), Sanitized.CycleTradeBalanceWeight, 0.0);
	TestEqual(TEXT("Reserva IA saneada"), Sanitized.EconomicAIMinTreasuryReserve, static_cast<int64>(0));
	TestEqual(TEXT("Construcciones IA saneadas"), Sanitized.EconomicAIMaxBuildsPerNationPerMonth, 0);
	TestEqual(TEXT("Retorno IA saneado"), Sanitized.EconomicAIMaxPaybackMonths, 0);
	TestEqual(TEXT("Orden publico minimo IA clamp"), Sanitized.EconomicAIMinPublicOrderToBuild, 100);
	TestEqual(TEXT("Anio inicial saneado"), Sanitized.StartYear, 1);
	TestEqual(TEXT("Mes inicial clamp a calendario"), Sanitized.StartMonth, 6);
	TestEqual(TEXT("Meses por anio preservado"), Sanitized.MonthsPerYear, 6);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
