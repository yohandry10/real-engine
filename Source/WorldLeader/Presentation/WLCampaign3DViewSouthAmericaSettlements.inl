void AWLCampaign3DView::BuildSouthAmericaFrontierSettlementLayer()
{
	const FLinearColor CityTone(0.74f, 0.64f, 0.42f);
	const FLinearColor FrontierTone(0.90f, 0.72f, 0.34f);
	const FLinearColor PortTone(0.74f, 0.64f, 0.44f);

	// Andes norte y Amazonia occidental.
	AddSettlementCluster(TEXT("EC-NUEVA-LOJA"), TEXT("Nueva Loja"), TEXT("EC"), TEXT("EC-SUC"), TEXT("Sucumbios"), -76.89f, 0.09f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("EC-HUAQUILLAS"), TEXT("Huaquillas"), TEXT("EC"), TEXT("EC-ELOR"), TEXT("El Oro"), -80.23f, -3.48f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("PE-TUMBES"), TEXT("Tumbes"), TEXT("PE"), TEXT("PE-TUM"), TEXT("Tumbes"), -80.45f, -3.57f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("PE-ICA"), TEXT("Ica"), TEXT("PE"), TEXT("PE-ICA"), TEXT("Ica"), -75.73f, -14.07f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("PE-HUANCAYO"), TEXT("Huancayo"), TEXT("PE"), TEXT("PE-JUN"), TEXT("Junin"), -75.21f, -12.07f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("PE-JULIACA"), TEXT("Juliaca"), TEXT("PE"), TEXT("PE-PUN"), TEXT("Puno"), -70.13f, -15.49f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("PE-PUCALLPA"), TEXT("Pucallpa"), TEXT("PE"), TEXT("PE-UCA"), TEXT("Ucayali"), -74.55f, -8.38f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("PE-PUERTO-MALDONADO"), TEXT("Puerto Maldonado"), TEXT("PE"), TEXT("PE-MDD"), TEXT("Madre de Dios"), -69.18f, -12.59f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("PE-TACNA"), TEXT("Tacna"), TEXT("PE"), TEXT("PE-TAC"), TEXT("Tacna"), -70.25f, -18.01f, EWLCampaignSettlementType::Frontier, FrontierTone);

	// Bolivia y Brasil occidental/interior: nodos de frontera que evitan saltos largos.
	AddSettlementCluster(TEXT("BO-VILLAZON"), TEXT("Villazon"), TEXT("BO"), TEXT("BO-POT"), TEXT("Potosi"), -65.59f, -22.09f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BO-DESAGUADERO"), TEXT("Desaguadero"), TEXT("BO"), TEXT("BO-LP"), TEXT("La Paz"), -69.04f, -16.56f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BO-TRINIDAD"), TEXT("Trinidad"), TEXT("BO"), TEXT("BO-BEN"), TEXT("Beni"), -64.90f, -14.83f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("BO-GUAYARAMERIN"), TEXT("Guayaramerin"), TEXT("BO"), TEXT("BO-BEN"), TEXT("Beni"), -65.36f, -10.82f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BO-COBIJA"), TEXT("Cobija"), TEXT("BO"), TEXT("BO-PAN"), TEXT("Pando"), -68.77f, -11.03f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BO-YACUIBA"), TEXT("Yacuiba"), TEXT("BO"), TEXT("BO-TJA"), TEXT("Tarija"), -63.65f, -22.03f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BO-PUERTO-SUAREZ"), TEXT("Puerto Suarez"), TEXT("BO"), TEXT("BO-SCZ"), TEXT("Santa Cruz"), -57.80f, -18.98f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-BRASILEIA"), TEXT("Brasileia"), TEXT("BR"), TEXT("BR-AC"), TEXT("Acre"), -68.75f, -11.01f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-PACARAIMA"), TEXT("Pacaraima"), TEXT("BR"), TEXT("BR-RR"), TEXT("Roraima"), -61.15f, 4.48f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-OIAPOQUE"), TEXT("Oiapoque"), TEXT("BR"), TEXT("BR-AP"), TEXT("Amapa"), -51.83f, 3.84f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-CORUMBA"), TEXT("Corumba"), TEXT("BR"), TEXT("BR-MS"), TEXT("Mato Grosso do Sul"), -57.65f, -19.01f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-SANTANA-LIVRAMENTO"), TEXT("Santana do Livramento"), TEXT("BR"), TEXT("BR-RS"), TEXT("Rio Grande do Sul"), -55.53f, -30.89f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-TABATINGA"), TEXT("Tabatinga"), TEXT("BR"), TEXT("BR-AM"), TEXT("Amazonas"), -69.94f, -4.25f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-RIO-BRANCO"), TEXT("Rio Branco"), TEXT("BR"), TEXT("BR-AC"), TEXT("Acre"), -67.81f, -9.97f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-PORTO-VELHO"), TEXT("Porto Velho"), TEXT("BR"), TEXT("BR-RO"), TEXT("Rondonia"), -63.90f, -8.76f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("BR-GOIANIA"), TEXT("Goiania"), TEXT("BR"), TEXT("BR-GO"), TEXT("Goias"), -49.25f, -16.68f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("BR-CUIABA"), TEXT("Cuiaba"), TEXT("BR"), TEXT("BR-MT"), TEXT("Mato Grosso"), -56.10f, -15.60f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("BR-CAMPO-GRANDE"), TEXT("Campo Grande"), TEXT("BR"), TEXT("BR-MS"), TEXT("Mato Grosso do Sul"), -54.62f, -20.45f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("BR-FOZ-DO-IGUACU"), TEXT("Foz do Iguacu"), TEXT("BR"), TEXT("BR-PR"), TEXT("Parana"), -54.59f, -25.54f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-FLORIANOPOLIS"), TEXT("Florianopolis"), TEXT("BR"), TEXT("BR-SC"), TEXT("Santa Catarina"), -48.55f, -27.59f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("BR-URUGUAIANA"), TEXT("Uruguaiana"), TEXT("BR"), TEXT("BR-RS"), TEXT("Rio Grande do Sul"), -57.09f, -29.75f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("BR-MACAPA"), TEXT("Macapa"), TEXT("BR"), TEXT("BR-AP"), TEXT("Amapa"), -51.06f, 0.03f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("BR-SANTAREM"), TEXT("Santarem"), TEXT("BR"), TEXT("BR-PA"), TEXT("Para"), -54.70f, -2.44f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("BR-SAO-LUIS"), TEXT("Sao Luis"), TEXT("BR"), TEXT("BR-MA"), TEXT("Maranhao"), -44.30f, -2.53f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("BR-TERESINA"), TEXT("Teresina"), TEXT("BR"), TEXT("BR-PI"), TEXT("Piaui"), -42.80f, -5.09f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("BR-NATAL"), TEXT("Natal"), TEXT("BR"), TEXT("BR-RN"), TEXT("Rio Grande do Norte"), -35.21f, -5.79f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("BR-VITORIA"), TEXT("Vitoria"), TEXT("BR"), TEXT("BR-ES"), TEXT("Espirito Santo"), -40.34f, -20.32f, EWLCampaignSettlementType::Port, PortTone);

	// Cono Sur y pasos andinos.
	AddSettlementCluster(TEXT("AR-LA-QUIACA"), TEXT("La Quiaca"), TEXT("AR"), TEXT("AR-JY"), TEXT("Jujuy"), -65.59f, -22.10f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("AR-SAN-LUIS"), TEXT("San Luis"), TEXT("AR"), TEXT("AR-SL"), TEXT("San Luis"), -66.34f, -33.30f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("AR-RIO-CUARTO"), TEXT("Rio Cuarto"), TEXT("AR"), TEXT("AR-CB"), TEXT("Cordoba"), -64.35f, -33.13f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("AR-PUERTO-IGUAZU"), TEXT("Puerto Iguazu"), TEXT("AR"), TEXT("AR-MI"), TEXT("Misiones"), -54.58f, -25.60f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("AR-BAHIA-BLANCA"), TEXT("Bahia Blanca"), TEXT("AR"), TEXT("AR-BA"), TEXT("Buenos Aires"), -62.27f, -38.72f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("AR-BARILOCHE"), TEXT("Bariloche"), TEXT("AR"), TEXT("AR-RN"), TEXT("Rio Negro"), -71.31f, -41.13f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("AR-SALTA"), TEXT("Salta"), TEXT("AR"), TEXT("AR-SA"), TEXT("Salta"), -65.41f, -24.79f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("AR-POSADAS"), TEXT("Posadas"), TEXT("AR"), TEXT("AR-MI"), TEXT("Misiones"), -55.90f, -27.37f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("AR-NEUQUEN"), TEXT("Neuquen"), TEXT("AR"), TEXT("AR-NQ"), TEXT("Neuquen"), -68.06f, -38.95f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("AR-RIO-GALLEGOS"), TEXT("Rio Gallegos"), TEXT("AR"), TEXT("AR-SC"), TEXT("Santa Cruz"), -69.22f, -51.62f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("CL-ARICA"), TEXT("Arica"), TEXT("CL"), TEXT("CL-AP"), TEXT("Arica y Parinacota"), -70.31f, -18.48f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("CL-IQUIQUE"), TEXT("Iquique"), TEXT("CL"), TEXT("CL-TA"), TEXT("Tarapaca"), -70.13f, -20.22f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("CL-COPIAPO"), TEXT("Copiapo"), TEXT("CL"), TEXT("CL-AT"), TEXT("Atacama"), -70.33f, -27.37f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("CL-LA-SERENA"), TEXT("La Serena"), TEXT("CL"), TEXT("CL-CO"), TEXT("Coquimbo"), -71.25f, -29.90f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("CL-TEMUCO"), TEXT("Temuco"), TEXT("CL"), TEXT("CL-AR"), TEXT("Araucania"), -72.59f, -38.74f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("CL-VALDIVIA"), TEXT("Valdivia"), TEXT("CL"), TEXT("CL-LR"), TEXT("Los Rios"), -73.25f, -39.81f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("CL-COYHAIQUE"), TEXT("Coyhaique"), TEXT("CL"), TEXT("CL-AI"), TEXT("Aysen"), -72.07f, -45.57f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("CL-PUERTO-NATALES"), TEXT("Puerto Natales"), TEXT("CL"), TEXT("CL-MA"), TEXT("Magallanes"), -72.51f, -51.73f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("UY-COLONIA"), TEXT("Colonia"), TEXT("UY"), TEXT("UY-CO"), TEXT("Colonia"), -57.84f, -34.47f, EWLCampaignSettlementType::Port, PortTone);
	AddSettlementCluster(TEXT("UY-ARTIGAS"), TEXT("Artigas"), TEXT("UY"), TEXT("UY-AR"), TEXT("Artigas"), -56.47f, -30.40f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("UY-MELO"), TEXT("Melo"), TEXT("UY"), TEXT("UY-CL"), TEXT("Cerro Largo"), -54.18f, -32.37f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("UY-CHUY"), TEXT("Chuy"), TEXT("UY"), TEXT("UY-RO"), TEXT("Rocha"), -53.46f, -33.69f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("UY-RIVERA"), TEXT("Rivera"), TEXT("UY"), TEXT("UY-RV"), TEXT("Rivera"), -55.55f, -30.90f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("UY-FRAY-BENTOS"), TEXT("Fray Bentos"), TEXT("UY"), TEXT("UY-RN"), TEXT("Rio Negro"), -58.30f, -33.12f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("PY-CONCEPCION"), TEXT("Concepcion"), TEXT("PY"), TEXT("PY-CN"), TEXT("Concepcion"), -57.43f, -23.41f, EWLCampaignSettlementType::LargeCity, CityTone);
	AddSettlementCluster(TEXT("PY-SALTO-DEL-GUAIRA"), TEXT("Salto del Guaira"), TEXT("PY"), TEXT("PY-CN"), TEXT("Canindeyu"), -54.31f, -24.06f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("PY-PEDRO-JUAN-CABALLERO"), TEXT("Pedro Juan Caballero"), TEXT("PY"), TEXT("PY-AM"), TEXT("Amambay"), -55.75f, -22.55f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("PY-FILADELFIA"), TEXT("Filadelfia"), TEXT("PY"), TEXT("PY-BO"), TEXT("Boqueron"), -60.03f, -22.35f, EWLCampaignSettlementType::Frontier, FrontierTone);

	// Guayanas: agrega la costa entre Surinam y Brasil.
	AddSettlementCluster(TEXT("SR-ALBINA"), TEXT("Albina"), TEXT("SR"), TEXT("SR-MA"), TEXT("Marowijne"), -54.06f, 5.50f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("GF-SAINT-LAURENT"), TEXT("Saint-Laurent"), TEXT("GF"), TEXT("GF-SL"), TEXT("Saint-Laurent-du-Maroni"), -54.03f, 5.50f, EWLCampaignSettlementType::Frontier, FrontierTone);
	AddSettlementCluster(TEXT("GF-CAYENNE"), TEXT("Cayenne"), TEXT("GF"), TEXT("GF-CY"), TEXT("Cayenne"), -52.33f, 4.94f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("GF-SAINT-GEORGES"), TEXT("Saint-Georges"), TEXT("GF"), TEXT("GF-SG"), TEXT("Saint-Georges"), -51.80f, 3.89f, EWLCampaignSettlementType::Frontier, FrontierTone);
}
