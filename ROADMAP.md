# WORLD LEADER — Game Design Document v2.1

> **Rome Total War 2 en el mundo moderno. Tú eres el líder supremo.**
> Plataforma: **PC nativo — Unreal Engine 5.x + C++ + Dedicated Servers**

---

## CONCEPTO CENTRAL

**Rome Total War 2** te da el mundo mediterráneo dividido en provincias, facciones con identidad real, ejércitos que se mueven en el mapa estratégico, diplomacia viva, economía basada en recursos por región y batallas tácticas en tiempo real cuando los ejércitos se encuentran.

**World Leader** toma esa fórmula y la adapta al mundo moderno completo.

Ya no es el mundo antiguo.
Ya no son legiones, caballos, catapultas, arqueros o elefantes de guerra.

Ahora el conflicto ocurre con:

* tanques;
* infantería mecanizada;
* soldados modernos;
* artillería autopropulsada;
* drones;
* helicópteros;
* aviones;
* portaaviones;
* submarinos;
* misiles;
* defensa aérea;
* guerra electrónica;
* operaciones especiales;
* sanciones económicas;
* espionaje;
* diplomacia internacional;
* armas nucleares;
* alianzas militares;
* organismos globales.

La esencia sigue siendo la misma:

```txt
Campaña estratégica global
+ Movimiento de ejércitos en mapa
+ Economía por provincias
+ Diplomacia viva
+ Batallas tácticas en tiempo real
+ Control territorial
+ Facciones con identidad propia
```

---

## COMPARACIÓN CON ROME TOTAL WAR 2

| Rome Total War 2                                   | World Leader                                                                   |
| -------------------------------------------------- | ------------------------------------------------------------------------------ |
| Roma, Egipto, Cartago, Partia                      | USA, China, Rusia, Venezuela, Brasil, India, Irán                              |
| Legiones, caballería, catapultas, elefantes        | Tanques, infantería, artillería, drones, aviones, flotas                       |
| Trigo, mármol, madera, hierro                      | Petróleo, gas, litio, uranio, alimentos, tecnología                            |
| Mediterráneo, Europa, África del Norte, Asia Menor | Norteamérica, Sudamérica, Europa, África, Medio Oriente, Asia, Oceanía, Ártico |
| Senado romano e intrigas internas                  | ONU, OTAN, FMI, BRICS, OPEP, golpes de Estado                                  |
| Batallas tácticas antiguas en tiempo real          | Batallas tácticas modernas en tiempo real                                      |
| Provincias con edificios                           | Provincias modernas con infraestructura, industria y defensa                   |
| Culturas antiguas                                  | Bloques geopolíticos, alianzas, ideologías y sistemas políticos                |
| Asedios con torres y catapultas                    | Guerra urbana, misiles, SAM, aviación, drones y artillería                     |
| Generales romanos                                  | Comandantes modernos, ministros, agencias de inteligencia                      |

---

## DIFERENCIA CLAVE

**Rome Total War 2** se concentra en el mundo antiguo: Europa, el Mediterráneo, África del Norte y Asia Menor.

**World Leader** cubre el mundo moderno completo:

* Norteamérica;
* Centroamérica;
* Caribe;
* Sudamérica;
* Europa;
* Rusia;
* Medio Oriente;
* Asia Central;
* Asia del Sur;
* Asia Oriental;
* Sudeste Asiático;
* África;
* Oceanía;
* Ártico;
* Antártida como zona diplomática especial;
* mares, canales, estrechos y rutas comerciales.

El objetivo no es copiar el mundo antiguo, sino adaptar el ADN de Total War a un conflicto geopolítico moderno.

---

# PLATAFORMA

## PC — Game First

**World Leader es un juego PC nativo.**

No es una aplicación web.
No es un dashboard geopolítico.
No es un simulador Tauri.
No es un mapa Leaflet con economía.

Es un juego de estrategia moderno con campaña global y batallas tácticas en tiempo real.

---

## Plataforma primaria

```txt
PC Windows
Steam
Unreal Engine 5.x
C++
Dedicated Servers para PvP
```

## Plataforma secundaria futura

```txt
Linux client opcional
macOS opcional si el alcance comercial lo justifica
Epic Games Store opcional
GOG opcional
```

## No contemplado inicialmente

```txt
Mobile
Browser
Tauri como cliente principal
React como UI principal del juego
Leaflet como motor de mapa
Supabase como motor central de partida
```

---

## Uso permitido de Tauri

Tauri no será parte del juego principal.

Puede usarse solo para herramientas internas:

```txt
Editor de provincias
Editor de facciones
Editor económico
Editor de balance
Launcher
Herramienta de modding
Panel administrativo
Validador de datos geopolíticos
```

---

# MODELO DE JUEGO

World Leader tiene dos capas principales.

---

## 1. Campaña estratégica global

La campaña representa el mundo moderno completo.

El jugador controla una nación y toma decisiones sobre:

* economía;
* construcción provincial;
* fuerzas armadas;
* diplomacia;
* comercio;
* sanciones;
* espionaje;
* investigación tecnológica;
* política interna;
* alianzas;
* guerra;
* tratados;
* organismos internacionales;
* crisis globales.

La campaña puede avanzar por ticks estratégicos, por ejemplo:

```txt
1 tick = 1 mes dentro del juego
```

Este tick no afecta el combate táctico.

El tick sirve para:

* economía;
* producción;
* presupuesto;
* construcción;
* diplomacia;
* eventos;
* investigación;
* IA estratégica;
* movimientos de campaña;
* cálculo de estabilidad;
* deuda;
* inflación;
* comercio;
* sanciones.

---

## 2. Batallas tácticas en tiempo real

Cuando dos fuerzas enemigas se encuentran, se activa una batalla táctica.

Aquí el juego se acerca más a la experiencia de **Rome Total War 2**, pero con guerra moderna.

El jugador controla unidades en tiempo real:

```txt
Tanques
Infantería
Artillería
Drones
Helicópteros
Aviones
Defensa aérea
Fuerzas especiales
Flotas
Misiles tácticos
```

La batalla incluye:

* formaciones;
* terreno;
* moral;
* cobertura;
* línea de visión;
* niebla de guerra;
* flanqueo;
* supresión;
* artillería;
* ataques aéreos;
* defensa aérea;
* drones de reconocimiento;
* guerra electrónica;
* captura de objetivos;
* retirada;
* refuerzos;
* logística táctica.

---

# EL MAPA MUNDIAL — EL CORAZÓN DEL JUEGO

## Cobertura completa

El mapa estratégico es el campo de batalla global.

Como en Rome Total War 2, el mapa está dividido en regiones y provincias.

Pero en lugar de limitarse al Mediterráneo, Europa, Asia Menor y África del Norte, World Leader cubre:

```txt
Norteamérica
Centroamérica
Caribe
Sudamérica
Europa
Rusia
Medio Oriente
Asia Central
Asia del Sur
Asia Oriental
Sudeste Asiático
África
Oceanía
Ártico
Antártida
Zonas marítimas estratégicas
```

---

## NORTEAMÉRICA (~85 provincias)

| Región                      | Provincias                                              | Recursos clave                   |
| --------------------------- | ------------------------------------------------------- | -------------------------------- |
| **Costa Oeste USA**         | California, Oregón, Washington, Nevada                  | Tecnología, agricultura          |
| **Interior Norte USA**      | Montana, Wyoming, Dakota, Colorado                      | Gas, carbón, agricultura         |
| **Cinturón Industrial USA** | Michigan, Ohio, Illinois, Pennsylvania                  | Acero, manufactura               |
| **Costa Este USA**          | Nueva York, Virginia, Carolina, Georgia                 | Finanzas, industria              |
| **Sur Profundo USA**        | Texas, Luisiana, Oklahoma, Mississippi                  | Petróleo, gas                    |
| **Alaska**                  | Alaska interior, costa ártica                           | Petróleo ártico, gas             |
| **Hawái / Pacífico**        | Hawái, Guam                                             | Base naval estratégica           |
| **Canadá Oeste**            | Columbia Británica, Alberta, Saskatchewan               | Petróleo, trigo                  |
| **Canadá Este**             | Ontario, Quebec, Marítimas                              | Manufactura, recursos forestales |
| **México Norte**            | Baja California, Sonora, Chihuahua                      | Manufactura maquiladora          |
| **México Centro**           | CDMX, Jalisco, Guanajuato                               | Industria, agricultura           |
| **México Sur**              | Oaxaca, Chiapas, Veracruz                               | Petróleo, café                   |
| **América Central**         | Guatemala, Honduras, El Salvador, Nicaragua             | Agricultura, canal               |
| **Costa Rica / Panamá**     | Costa Rica, Panamá                                      | Canal estratégico                |
| **Caribe**                  | Cuba, Jamaica, Haití, República Dominicana, Puerto Rico | Turismo, azúcar                  |

---

## SUDAMÉRICA (~110 provincias)

| Región                                  | Provincias                                   | Recursos clave                       |
| --------------------------------------- | -------------------------------------------- | ------------------------------------ |
| **Venezuela**                           | Zulia, Bolívar, Caracas, Apure, Orinoco      | Petróleo masivo, hierro, oro         |
| **Colombia Norte**                      | Atlántico, Bolívar, Córdoba                  | Carbón, turismo                      |
| **Colombia Sur**                        | Cundinamarca, Antioquia, Nariño              | Café, flores, petróleo               |
| **Ecuador**                             | Costa ecuatoriana, Sierra, Amazonía          | Petróleo, cacao, banano              |
| **Perú Costa**                          | Lima, Ica, La Libertad                       | Pesca, manufactura, finanzas         |
| **Perú Sierra**                         | Cusco, Puno, Arequipa, Ayacucho              | Cobre, plata, oro                    |
| **Perú Amazónico**                      | Loreto, Ucayali, Madre de Dios               | Petróleo, gas, madera                |
| **Bolivia**                             | Altiplano, Santa Cruz, Beni                  | Litio estratégico, gas, estaño, soya |
| **Chile Norte**                         | Antofagasta, Atacama, Tarapacá               | Cobre masivo, litio                  |
| **Chile Centro**                        | Valparaíso, Metropolitana, O'Higgins         | Manufactura, vino                    |
| **Chile Sur**                           | Biobío, Los Lagos, Magallanes                | Forestal, pesca, Patagonia           |
| **Argentina Pampa**                     | Buenos Aires, Santa Fe, Córdoba              | Soya, trigo, ganadería               |
| **Argentina Norte**                     | Tucumán, Salta, Jujuy, Chaco                 | Litio, caña, soya                    |
| **Argentina Patagonia**                 | Neuquén, Río Negro, Santa Cruz               | Petróleo, pesca, viento              |
| **Brasil Amazónico**                    | Amazonas, Pará, Roraima, Amapá               | Madera, minerales, biodiversidad     |
| **Brasil Noreste**                      | Bahía, Pernambuco, Ceará, Maranhão           | Petróleo offshore, manufactura       |
| **Brasil Sudeste**                      | São Paulo, Río, Minas Gerais, Espírito Santo | Industria masiva, finanzas           |
| **Brasil Sur**                          | Paraná, Santa Catarina, Rio Grande do Sul    | Soya, manufactura, ganadería         |
| **Brasil Centro-Oeste**                 | Mato Grosso, Goiás, Distrito Federal         | Soya masiva, ganadería               |
| **Paraguay**                            | Asunción, Interior                           | Soya, energía hidroeléctrica         |
| **Uruguay**                             | Montevideo, Interior                         | Ganadería, soya, finanzas            |
| **Guyana / Surinam / Guayana Francesa** | Costa norte sudamericana                     | Petróleo offshore, bauxita           |

---

## EUROPA (~130 provincias)

| Región                 | Provincias                                             | Recursos clave                               |
| ---------------------- | ------------------------------------------------------ | -------------------------------------------- |
| **UK e Irlanda**       | Inglaterra, Escocia, Gales, Irlanda                    | Finanzas, manufactura, gas del Mar del Norte |
| **Francia Norte**      | París, Normandía, Bretaña                              | Industria, finanzas, nuclear                 |
| **Francia Sur**        | Aquitania, Provenza, Midi                              | Agricultura, turismo                         |
| **Iberia Norte**       | Cataluña, País Vasco, Madrid                           | Manufactura, finanzas                        |
| **Iberia Sur**         | Andalucía, Valencia, Portugal                          | Agricultura, turismo, litio                  |
| **Benelux**            | Países Bajos, Bélgica, Luxemburgo                      | Puertos, finanzas, petroquímica              |
| **Alemania Norte**     | Berlín, Hamburgo, Schleswig                            | Manufactura avanzada, logística              |
| **Alemania Sur**       | Bavaria, Baden-Württemberg                             | Autos, alta tecnología                       |
| **Escandinavia Norte** | Noruega, Suecia Norte                                  | Petróleo, minerales                          |
| **Escandinavia Sur**   | Suecia Sur, Dinamarca, Finlandia                       | Tecnología, manufactura                      |
| **Italia Norte**       | Lombardía, Piamonte, Véneto                            | Manufactura, moda, diseño                    |
| **Italia Sur**         | Roma, Nápoles, Sicilia, Cerdeña                        | Agricultura, turismo                         |
| **Polonia / Bálticos** | Polonia, Estonia, Letonia, Lituania                    | Manufactura, carbón                          |
| **Europa Central**     | República Checa, Eslovaquia, Hungría                   | Manufactura, agricultura                     |
| **Balcanes Norte**     | Croacia, Eslovenia, Austria                            | Turismo, manufactura                         |
| **Balcanes Sur**       | Serbia, Bosnia, Macedonia, Bulgaria                    | Minerales, agricultura                       |
| **Grecia / Chipre**    | Grecia continental, islas, Chipre                      | Gas offshore, turismo, marítimo              |
| **Rumanía / Moldova**  | Rumanía, Moldova                                       | Gas, petróleo, agricultura                   |
| **Ucrania**            | Ucrania Oriental, Ucrania Occidental, Crimea disputada | Trigo, carbón, hierro, manufactura           |
| **Bielorrusia**        | Bielorrusia                                            | Potasa, manufactura, corredor logístico      |

---

## RUSIA (~80 provincias)

| Región                 | Provincias                           | Recursos clave              |
| ---------------------- | ------------------------------------ | --------------------------- |
| **Rusia Europea**      | Moscú, San Petersburgo, Volga, Ural  | Industria, finanzas         |
| **Siberia Occidental** | Yamalo-Nenets, Khanty-Mansiysk, Omsk | Petróleo masivo, gas        |
| **Siberia Oriental**   | Novosibirsk, Krasnoyarsk, Irkutsk    | Carbón, minerales, acero    |
| **Extremo Oriente**    | Vladivostok, Sakhalin, Kamchatka     | Gas, pesca, acceso Pacífico |
| **Ártico Ruso**        | Mar de Kara, Laptev, Este Siberiano  | Petróleo ártico, gas        |
| **Cáucaso Norte**      | Chechenia, Daguestán, Stavropol      | Petróleo, gas               |
| **Kaliningrado**       | Kaliningrado                         | Enclave estratégico         |

---

## ORIENTE MEDIO (~70 provincias)

| Región                   | Provincias                              | Recursos clave                 |
| ------------------------ | --------------------------------------- | ------------------------------ |
| **Arabia Saudita**       | Hiyaz, Nayran, Asir, Golfo, Rub al-Jali | Petróleo masivo, gas           |
| **Irak**                 | Bagdad, Basora, Kurdistán, Nínive       | Petróleo, gas                  |
| **Irán Norte**           | Teherán, Azerbaiyán Iraní, Gilan        | Petróleo, gas, industria       |
| **Irán Sur**             | Khuzestán, Isfahán, Shiraz              | Petróleo masivo, petroquímica  |
| **Turquía Oeste**        | Estambul, Ankara, Esmirna               | Manufactura, turismo, textiles |
| **Turquía Este**         | Anatolia Central, Este kurdo            | Agricultura, minerales         |
| **Israel / Territorios** | Israel, Cisjordania, Gaza               | Tecnología, gas offshore       |
| **Siria / Líbano**       | Damasco, Alepo, Beirut                  | Gas, corredor logístico        |
| **Jordania / Palestina** | Ammán, Negev                            | Potasa, fosfato                |
| **Golfo Pérsico**        | UAE, Kuwait, Qatar, Bahréin, Omán       | Gas, petróleo, finanzas        |
| **Yemen**                | Norte, Sur, Hadramaut                   | Gas, posición estratégica      |

---

## ASIA CENTRAL (~50 provincias)

| Región                      | Provincias                     | Recursos clave                      |
| --------------------------- | ------------------------------ | ----------------------------------- |
| **Kazajistán**              | Astaná, Almaty, Caspio, Estepa | Petróleo, uranio, trigo             |
| **Uzbekistán / Tayikistán** | Samarcanda, Taskent, Dushambé  | Gas, algodón, minerales             |
| **Turkmenistán**            | Ashgabat, costa Caspio         | Gas masivo                          |
| **Kirguistán / Mongolia**   | Bishkek, Ulán Bator            | Minerales, ganadería, tierras raras |
| **Afganistán**              | Kabul, Kandahar, norte afgano  | Minerales estratégicos              |

---

## ASIA DEL SUR (~60 provincias)

| Región                   | Provincias                                 | Recursos clave                    |
| ------------------------ | ------------------------------------------ | --------------------------------- |
| **India Norte**          | Delhi, Punjab, Rajastán, UP, Bihar         | Carbón, agricultura, manufactura  |
| **India Centro**         | Madhya Pradesh, Chhattisgarh               | Carbón, hierro, manufactura       |
| **India Sur**            | Maharashtra, Karnataka, Tamil Nadu, Kerala | Tecnología, manufactura, textiles |
| **India Noreste**        | Bengala, Assam, Odisha                     | Gas, petróleo, minerales          |
| **Pakistán**             | Punjab, Sindh, Baluchistán, KPK            | Gas, textiles, minerales          |
| **Bangladesh / Myanmar** | Dhaka, Yangón                              | Textiles, gas, jade               |
| **Sri Lanka / Maldivas** | Colombo, islas                             | Turismo, posición estratégica     |
| **Nepal / Bután**        | Katmandú, Timbu                            | Hidroeléctrica, turismo           |

---

## ASIA ORIENTAL (~90 provincias)

| Región              | Provincias                             | Recursos clave                         |
| ------------------- | -------------------------------------- | -------------------------------------- |
| **China Norte**     | Beijing, Hebei, Shanxi, Liaoning       | Carbón, acero, manufactura pesada      |
| **China Noreste**   | Jilin, Heilongjiang, Mongolia Interior | Soya, carbón, industria                |
| **China Este**      | Shanghai, Jiangsu, Zhejiang            | Manufactura avanzada, finanzas         |
| **China Sur**       | Guangdong, Fujian, Hainan              | Manufactura exportadora, electrónica   |
| **China Centro**    | Sichuan, Hubei, Hunan                  | Manufactura, agricultura               |
| **China Oeste**     | Sinkiang, Tíbet, Qinghai               | Uranio, minerales, litio               |
| **China Suroeste**  | Yunnan, Guangxi, Guizhou               | Minerales, tierras raras               |
| **Corea del Sur**   | Seúl, Busan, Incheon, Daejón           | Electrónica, autos, construcción naval |
| **Corea del Norte** | Pyongyang, norte montañoso             | Minerales, programa militar            |
| **Japón Norte**     | Hokkaido, Tohoku                       | Agricultura, pesca                     |
| **Japón Central**   | Tokio, Osaka, Nagoya                   | Tecnología avanzada, finanzas, autos   |
| **Japón Sur**       | Kyushu, Shikoku, Okinawa               | Manufactura, base estratégica          |
| **Taiwán**          | Taipei, centro, sur                    | Semiconductores                        |

---

## SUDESTE ASIÁTICO (~70 provincias)

| Región                       | Provincias                       | Recursos clave                        |
| ---------------------------- | -------------------------------- | ------------------------------------- |
| **Vietnam**                  | Hanói, Centro, HCMC, costa       | Petróleo offshore, manufactura, arroz |
| **Tailandia**                | Bangkok, Norte, Sur, Isan        | Arroz, manufactura, turismo           |
| **Indonesia Java**           | Yakarta, Surabaya, Bandung       | Manufactura, agricultura              |
| **Indonesia Exterior**       | Sumatra, Borneo, Sulawesi, Papúa | Petróleo, carbón, minerales, gas      |
| **Malasia**                  | Peninsular, Borneo malayo        | Petróleo, manufactura, palma          |
| **Filipinas**                | Luzón, Visayas, Mindanao         | Minería, manufactura, servicios       |
| **Singapur / Brunéi**        | Ciudad-estado, Brunéi            | Finanzas, petróleo, logística         |
| **Myanmar / Laos / Camboya** | Rangún, Vientiane, Phnom Penh    | Gas, minerales, agricultura           |

---

## ÁFRICA (~120 provincias)

| Región                         | Provincias                                       | Recursos clave                    |
| ------------------------------ | ------------------------------------------------ | --------------------------------- |
| **Marruecos / Argelia**        | Rabat, Casablanca, Argel, Orán                   | Fosfatos, gas                     |
| **Túnez / Libia**              | Túnez, Trípoli, Bengasi                          | Gas, petróleo                     |
| **Egipto**                     | El Cairo, Alejandría, Suez, Sinaí                | Canal estratégico, gas, turismo   |
| **Sudán / Sudán del Sur**      | Jartum, Juba                                     | Petróleo, agricultura             |
| **Etiopía / Eritrea**          | Addis Abeba, Asmara                              | Café, agricultura, hidroeléctrica |
| **Somalia / Yibuti**           | Mogadiscio, Yibuti                               | Posición estratégica              |
| **África Occidental**          | Nigeria, Ghana, Costa de Marfil                  | Petróleo, cacao, oro              |
| **Sahel**                      | Mali, Níger, Burkina Faso, Chad                  | Uranio, oro, volatilidad          |
| **África Central**             | RDC, Congo, Camerún, Gabón                       | Coltán, petróleo, cobre           |
| **África del Este**            | Kenia, Tanzania, Uganda, Rwanda                  | Flores, café, turismo, recursos   |
| **Angola / Mozambique**        | Luanda, Maputo                                   | Petróleo, gas natural             |
| **Zambia / Zimbabwe / Malawi** | Lusaka, Harare, Lilongüe                         | Cobre, tabaco, minerales          |
| **Sudáfrica**                  | Johannesburgo, Ciudad del Cabo, Durban, Pretoria | Oro, platino, carbón, manufactura |
| **Namibia / Botswana**         | Windhoek, Gaborone                               | Diamantes, uranio, carne          |
| **Madagascar**                 | Antananarivo                                     | Níquel, cobalto, vainilla         |

---

## OCEANÍA (~30 provincias)

| Región                   | Provincias                            | Recursos clave                  |
| ------------------------ | ------------------------------------- | ------------------------------- |
| **Australia Occidental** | Perth, Pilbara, Kalgoorlie            | Hierro masivo, litio, oro       |
| **Australia Central**    | Darwin, Alice Springs, Norte          | Gas, uranio, ganadería          |
| **Australia Oriental**   | Sídney, Melbourne, Brisbane, Adelaida | Manufactura, carbón, finanzas   |
| **Nueva Zelanda**        | Auckland, Wellington, Isla Sur        | Agricultura, ganadería, turismo |
| **Pacífico**             | PNG, Fiji, Solomón, Vanuatu           | Gas, cobre, pesca               |

---

## ZONAS ESTRATÉGICAS ESPECIALES

| Zona                        | Tipo                      | Importancia                                |
| --------------------------- | ------------------------- | ------------------------------------------ |
| **Ártico**                  | Región disputada          | Petróleo, gas, rutas comerciales del norte |
| **Antártida**               | Zona neutral por tratado  | Violación genera crisis diplomática global |
| **Mar de China Meridional** | Zona disputada            | Rutas comerciales, petróleo offshore       |
| **Canal de Suez**           | Infraestructura crítica   | Paso estratégico Europa-Asia               |
| **Canal de Panamá**         | Infraestructura crítica   | Paso Atlántico-Pacífico                    |
| **Estrecho de Ormuz**       | Punto de estrangulamiento | Petróleo del Golfo Pérsico                 |
| **Estrecho de Malaca**      | Punto de estrangulamiento | Comercio Asia-Europa                       |
| **Bósforo / Dardanelos**    | Punto de estrangulamiento | Acceso Mar Negro-Mediterráneo              |
| **Gibraltar**               | Punto de estrangulamiento | Acceso Atlántico-Mediterráneo              |

Mecánica especial:

```txt
Controlar estos puntos permite:
- cobrar peajes;
- bloquear rutas comerciales;
- sancionar enemigos;
- proyectar poder naval;
- forzar crisis diplomáticas;
- afectar precios globales.
```

---

# FACCIONES JUGABLES

Como en Rome Total War 2, no todas las facciones empiezan con el mismo poder.

Cada nación inicia con estadísticas aproximadas según el escenario elegido:

```txt
1962 — Guerra Fría
1990 — Fin de la URSS
2001 — Post 9/11
2010 — Multipolaridad emergente
2024 — Mundo actual
```

---

## Superpotencias de inicio

| Facción   | Fortaleza                                  | Debilidad                            | Estilo de juego                        |
| --------- | ------------------------------------------ | ------------------------------------ | -------------------------------------- |
| **USA**   | Ejército global, tecnología, finanzas      | Deuda, polarización, sobreextensión  | Naval/aéreo, proyección global         |
| **China** | Manufactura, población, ejército terrestre | Dependencia energética, Taiwán       | Expansión económica y regional         |
| **Rusia** | Nuclear, gas, petróleo, espacio            | PIB limitado, corrupción, demografía | Disuasión, guerra terrestre, espionaje |

---

## Potencias regionales

| Facción            | Región           | Estilo                                     |
| ------------------ | ---------------- | ------------------------------------------ |
| **Alemania**       | Europa           | Economía industrial, diplomacia            |
| **Francia**        | Europa           | Nuclear, África, diplomacia                |
| **UK**             | Europa           | Naval, finanzas, inteligencia              |
| **Japón**          | Asia             | Tecnología, naval, dependencia energética  |
| **India**          | Asia Sur         | Ejército terrestre, crecimiento, nuclear   |
| **Turquía**        | Eurasia          | Ejército, geopolítica, control de pasos    |
| **Irán**           | Medio Oriente    | Petróleo, proxies, aspiración nuclear      |
| **Arabia Saudita** | Medio Oriente    | Petróleo, inversión, OPEP                  |
| **Israel**         | Medio Oriente    | Tecnología, inteligencia, defensa avanzada |
| **Brasil**         | Sudamérica       | Recursos, manufactura, soft power          |
| **Nigeria**        | África           | Petróleo, población, inestabilidad         |
| **Sudáfrica**      | África           | Minerales, manufactura, diplomacia         |
| **Indonesia**      | Sudeste Asiático | Recursos, población, manufactura emergente |
| **México**         | Norteamérica     | Manufactura, petróleo, cercanía a USA      |

---

## América Latina — facciones completas

| Facción       | Fortalezas únicas                       | Reto inicial                                    |
| ------------- | --------------------------------------- | ----------------------------------------------- |
| **Brasil**    | Amazonia, economía grande, industria    | Deuda, desigualdad, política inestable          |
| **Argentina** | Litio, soya, Patagonia, industria       | Inflación, deuda, crisis fiscal                 |
| **Venezuela** | Petróleo masivo, ubicación Caribe       | Sanciones, infraestructura rota, crisis interna |
| **Colombia**  | Dos océanos, carbón, café               | Guerrilla, narcotráfico, presión interna        |
| **Chile**     | Cobre, litio, estabilidad               | Dependencia minera, geografía estrecha          |
| **Perú**      | Cobre, oro, plata, Pacífico             | Inestabilidad política, desigualdad             |
| **Bolivia**   | Litio, gas, estaño                      | Sin salida al mar, logística vulnerable         |
| **Ecuador**   | Petróleo amazónico, posición ecuatorial | Dolarización, deuda, seguridad                  |
| **Paraguay**  | Itaipú, soya                            | Sin litoral, corrupción, contrabando            |
| **Uruguay**   | Estabilidad, ganadería, finanzas        | Tamaño pequeño, dependencia regional            |
| **Cuba**      | Níquel, medicina, ubicación Caribe      | Embargo, economía planificada                   |
| **México**    | Manufactura, petróleo, frontera USA     | Carteles, dependencia comercial                 |

---

# SISTEMA DE PROVINCIAS

Cada provincia tiene:

```txt
PROVINCIA
├── ID único
├── Nombre
├── Capital provincial
├── País propietario
├── País controlador
├── Región
├── Terreno
│   ├── plano
│   ├── montaña
│   ├── desierto
│   ├── selva
│   ├── costera
│   ├── urbana
│   ├── ártica
│   └── marítima
├── Recursos base
│   ├── petróleo
│   ├── gas
│   ├── carbón
│   ├── hierro
│   ├── cobre
│   ├── litio
│   ├── uranio
│   ├── alimentos
│   ├── tecnología
│   └── industria
├── Población
├── Orden público
├── Estabilidad
├── Infraestructura
├── Valor estratégico
├── Ranuras de construcción
├── Guarnición mínima
├── Nivel defensivo
├── Conexiones terrestres
├── Conexiones marítimas
└── Estado de ocupación
```

---

## Edificios por slot

```txt
SLOT ECONÓMICO
→ Pozo de petróleo
→ Mina
→ Granja industrial
→ Parque industrial
→ Refinería
→ Complejo petroquímico

SLOT INDUSTRIAL
→ Fábrica básica
→ Complejo industrial
→ Megafábrica
→ Planta automotriz
→ Planta electrónica
→ Complejo militar-industrial

SLOT MILITAR
→ Base del ejército
→ Cuartel mecanizado
→ Cuartel blindado
→ Complejo de armas
→ Centro de entrenamiento avanzado

SLOT NAVAL
→ Puerto civil
→ Puerto comercial
→ Puerto militar
→ Astillero
→ Base naval
→ Base submarina

SLOT AÉREO
→ Aeródromo
→ Base aérea
→ Base de drones
→ Centro de defensa aérea
→ Base estratégica

SLOT TECNOLÓGICO
→ Universidad
→ Centro R&D
→ Instituto nuclear
→ Centro aeroespacial
→ Laboratorio de IA
→ Centro de ciberdefensa

SLOT FINANCIERO
→ Banco
→ Centro comercial
→ Zona franca
→ Bolsa regional
→ Centro financiero global

SLOT INFRAESTRUCTURA
→ Carretera
→ Autopista
→ Red ferroviaria
→ Tren de carga
→ Hub logístico
→ Red energética

SLOT DEFENSIVO
→ Cuartel
→ Bunker
→ Fortaleza moderna
→ Sistema SAM
→ Radar avanzado
→ Defensa costera
```

Nivel de edificios:

```txt
Nivel 1 → básico
Nivel 2 → desarrollado
Nivel 3 → avanzado
Nivel 4 → estratégico
Nivel 5 → único / maravilla moderna
```

---

# EJÉRCITOS EN EL MAPA

Como en Rome Total War 2, los ejércitos son stacks visibles en el mapa estratégico.

Ejemplo:

```txt
[Ícono de tanque] División Blindada "Simón Bolívar" — Venezuela
  ├── 2x Brigada de tanques T-72
  ├── 1x Infantería mecanizada
  ├── 1x Artillería autopropulsada
  ├── 1x Defensa aérea SAM
  ├── 1x Drones de reconocimiento
  └── General: Rodríguez — Especialidad: Asalto blindado
```

---

## Reglas de movimiento estratégico

```txt
- Los ejércitos se mueven entre provincias conectadas.
- Montañas, selvas, desiertos y zonas árticas reducen velocidad.
- Sin acceso diplomático no puedes cruzar territorio neutral.
- Sin suministro pierdes moral, combustible y efectividad.
- Las unidades aéreas operan por radio de alcance.
- Las flotas operan en zonas marítimas.
- Los submarinos pueden operar ocultos.
- Los portaaviones proyectan poder aéreo.
```

---

## Tipos de fuerzas

| Tipo                      | Movimiento                   | Especialidad                    |
| ------------------------- | ---------------------------- | ------------------------------- |
| **División terrestre**    | Provincias adyacentes        | Captura de territorio           |
| **División blindada**     | Rápida en terreno abierto    | Ruptura y flanqueo              |
| **Infantería mecanizada** | Media                        | Ocupación y combate urbano      |
| **Artillería**            | Apoyo indirecto              | Bombardeo y desgaste            |
| **Defensa aérea**         | Terrestre                    | Niega ataques aéreos            |
| **Fuerza naval**          | Zonas marítimas              | Bloqueo y desembarco            |
| **Escuadrón aéreo**       | Radio de acción              | Bombardeo, superioridad aérea   |
| **Drones**                | Largo alcance / sigilo       | Reconocimiento y ataque puntual |
| **Fuerzas especiales**    | Infiltración                 | Sabotaje, captura, decapitación |
| **Fuerza nuclear**        | Silos/submarinos/bombarderos | Disuasión estratégica           |

---

# BATALLAS TÁCTICAS EN TIEMPO REAL

Cuando dos ejércitos enemigos se encuentran en una provincia, se genera una batalla.

El jugador puede elegir:

```txt
A. Auto-resolve
B. Batalla táctica en tiempo real
```

---

## Modo A — Auto-resolve

Sistema rápido para batallas menores.

```txt
Resultado =
fuerza atacante
+ fuerza defensora
+ terreno
+ suministro
+ tecnología
+ moral
+ general
+ inteligencia previa
+ superioridad aérea
+ apoyo naval
+ defensa aérea
+ clima
```

Resultado posible:

```txt
Victoria decisiva
Victoria costosa
Empate táctico
Retirada ordenada
Derrota
Aniquilación
```

---

## Modo B — Batalla táctica moderna en tiempo real

Este es el modo principal para PvP y batallas importantes.

No es 2D por turnos.
No es mapa simplificado.
Es batalla táctica en tiempo real inspirada en Rome Total War 2, pero con guerra moderna.

---

## Características del combate táctico

```txt
- Cámara táctica 3D
- Control de unidades por escuadrones, pelotones, compañías o batallones
- Formaciones modernas
- Línea de visión
- Fog of war
- Cobertura
- Supresión
- Moral
- Munición
- Combustible
- Daño por penetración
- Daño por explosión
- Artillería indirecta
- Ataques aéreos
- Drones de reconocimiento
- Drones kamikaze
- Helicópteros
- Tanques
- Infantería urbana
- Defensa aérea
- Radar
- Guerra electrónica
- Refuerzos
- Retirada
- Captura de puntos estratégicos
```

---

## Equivalencias modernas con Rome Total War 2

| Rome Total War 2       | World Leader                                 |
| ---------------------- | -------------------------------------------- |
| Caballería flanqueando | Tanques flanqueando                          |
| Arqueros en colina     | Drones y francotiradores con visión elevada  |
| Catapultas             | Artillería autopropulsada y misiles tácticos |
| Infantería pesada      | Infantería mecanizada                        |
| Elefantes de guerra    | Tanques pesados / IFV                        |
| Murallas               | Defensa urbana, bunkers, SAM                 |
| General romano         | Comandante moderno                           |
| Moral de tropas        | Moral, supresión y comunicación              |
| Formación de falange   | Formación blindada / defensa escalonada      |
| Asedio de ciudad       | Guerra urbana moderna                        |
| Flota antigua          | Portaaviones, fragatas, submarinos           |

---

# ECONOMÍA

## Ingreso provincial

```txt
Ingreso provincial =
  recursos_extraídos * precio_mercado
+ impuesto_población * tasa_impositiva
+ producción_industrial
+ comercio_ruta_activa
+ ingresos_logísticos
+ peajes estratégicos
```

## Gasto provincial

```txt
Gasto provincial =
  mantenimiento_edificios
+ gasto_orden_público
+ subsidios
+ reparación_infraestructura
+ seguridad_interna
+ costo_ocupación
```

## Balance nacional

```txt
Balance nacional =
  Σ ingresos provinciales
+ exportaciones
+ aranceles
+ servicios financieros
+ ingresos energéticos
- gasto militar
- mantenimiento de bases
- deuda_servicio
- importaciones críticas
- subsidios
- gasto social
- corrupción
```

Si el balance es negativo durante varios ticks:

```txt
Déficit
→ deuda
→ inflación
→ pérdida de confianza
→ protestas
→ inestabilidad
→ crisis política
→ posible golpe de Estado
```

---

# SISTEMAS POLÍTICOS Y DIPLOMÁTICOS

World Leader conserva y expande los sistemas del GDD v1.

## Sistemas principales

```txt
- Gobierno
- Ministerios
- Leyes
- Elecciones
- Partidos políticos
- Estabilidad interna
- Corrupción
- Protestas
- Rebeliones
- Golpes de Estado
- Diplomacia bilateral
- ONU
- OTAN
- BRICS
- OPEP
- FMI
- Banco Mundial
- Sanciones
- Tratados comerciales
- Bloques militares
- Bases extranjeras
- Espionaje
- Contrainteligencia
- Ciberoperaciones
- Falsas banderas
- Crisis globales
- Pandemias
- Terremotos
- Crisis financieras
- Colapsos energéticos
- Carrera espacial
- Carrera nuclear
```

---

# TECNOLOGÍA

## Árbol tecnológico

Ramas:

```txt
Economía
Industria
Energía
Militar terrestre
Militar naval
Militar aérea
Misiles
Defensa aérea
Ciberseguridad
Inteligencia artificial
Espacio
Nuclear
Drones
Guerra electrónica
Logística
Medicina
Agricultura
```

---

# ARMAS NUCLEARES

Las armas nucleares no son armas normales.

Funcionan como sistema de disuasión estratégica.

```txt
- Desarrollo nuclear
- Enriquecimiento
- Silos
- Submarinos nucleares
- Bombarderos estratégicos
- Doctrina nuclear
- Segundo golpe
- MAD
- Sanciones internacionales
- Crisis diplomática
- Escalada global
```

Uso nuclear:

```txt
Si una nación usa armas nucleares:
- destrucción masiva;
- colapso diplomático;
- sanciones globales;
- posible respuesta nuclear;
- contaminación;
- pérdida de estabilidad mundial;
- cambio permanente del escenario.
```

---

# CONDICIONES DE VICTORIA

```txt
Dominación global
Victoria regional
Victoria económica
Victoria tecnológica
Victoria diplomática
Victoria energética
Victoria militar
Victoria ideológica
Victoria por supervivencia
Victoria nuclear disuasiva
```

---

# ARQUITECTURA TÉCNICA — STACK IDEAL ESCALABLE

## Stack principal

```txt
Game Engine:         Unreal Engine 5.x
Lenguaje principal:  C++
Scripting visual:    Blueprints controlados
UI del juego:         UMG + CommonUI
Gameplay moderno:    Gameplay Ability System
Entidades masivas:   Mass Entity
Mundo/escenarios:    World Partition
IA táctica:           Behavior Trees + EQS + C++ custom AI
Navegación:           Unreal Navigation System + pathfinding custom
Networking:          Unreal Networking + Replication Graph
PvP:                 Dedicated Servers autoritativos
Online:              Steamworks / Epic Online Services
Backend:             Rust + Axum
Base de datos:       PostgreSQL
Cache/sesiones:      Redis
Mensajería:          NATS opcional
Storage:             S3-compatible storage
Persistencia local:  SQLite + Unreal SaveGame
Distribución:        Steam
CI/CD:               GitHub Actions + Unreal Build Automation
Infraestructura:     Docker + Kubernetes
Cloud:               AWS / Azure / GCP
Observabilidad:      OpenTelemetry + Prometheus + Grafana + Loki
```

---

## Arquitectura general

```txt
┌──────────────────────────────────────────────────────────────┐
│                         PC CLIENT                            │
│                     Unreal Engine 5.x                        │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │                 CAMPAÑA GLOBAL                         │  │
│  │  - Mapa mundial                                        │  │
│  │  - Provincias                                          │  │
│  │  - Naciones                                            │  │
│  │  - Economía                                            │  │
│  │  - Diplomacia                                          │  │
│  │  - Política                                            │  │
│  │  - Tecnología                                          │  │
│  │  - Espionaje                                           │  │
│  └────────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │          BATALLAS TÁCTICAS EN TIEMPO REAL              │  │
│  │  - Tanques                                             │  │
│  │  - Infantería                                          │  │
│  │  - Artillería                                          │  │
│  │  - Aviación                                            │  │
│  │  - Drones                                              │  │
│  │  - Flotas                                              │  │
│  │  - Moral                                               │  │
│  │  - Terreno                                             │  │
│  │  - PvP                                                 │  │
│  └────────────────────────────────────────────────────────┘  │
└───────────────────────────┬──────────────────────────────────┘
                            │
                            │ Multiplayer
                            ▼
┌──────────────────────────────────────────────────────────────┐
│                 DEDICATED BATTLE SERVERS                     │
│                Unreal Dedicated Server Linux                 │
│                                                              │
│  - Estado autoritativo de batalla                            │
│  - Posición real de unidades                                 │
│  - Daño                                                      │
│  - Moral                                                     │
│  - Proyectiles relevantes                                    │
│  - Captura de objetivos                                      │
│  - Victoria / derrota                                        │
│  - Anti-cheat básico                                         │
└───────────────────────────┬──────────────────────────────────┘
                            │
                            │ Resultado validado
                            ▼
┌──────────────────────────────────────────────────────────────┐
│                      BACKEND ONLINE                          │
│                         Rust + Axum                          │
│                                                              │
│  - Usuarios                                                  │
│  - Matchmaking                                               │
│  - Campañas online                                           │
│  - Ranking                                                   │
│  - Guardado cloud                                            │
│  - Historial diplomático                                     │
│  - Validación de resultados                                  │
│  - Telemetría                                                │
└───────────────┬───────────────────────────────┬──────────────┘
                │                               │
                ▼                               ▼
┌──────────────────────────┐       ┌──────────────────────────┐
│       PostgreSQL         │       │          Redis           │
│                          │       │                          │
│ - Usuarios               │       │ - Sesiones               │
│ - Campañas               │       │ - Matchmaking temporal   │
│ - Naciones               │       │ - Lobbies                │
│ - Provincias             │       │ - Presencia online       │
│ - Ranking                │       │ - Locks                  │
│ - Historial              │       │ - Estado efímero         │
└──────────────────────────┘       └──────────────────────────┘
```

---

## Reglas de arquitectura

### Regla 1 — El cliente no manda la verdad en PvP

```txt
El cliente muestra, predice y anima.
El servidor dedicado valida.
```

El servidor decide:

```txt
- posición real;
- daño;
- moral;
- captura;
- victoria;
- derrota;
- resultados oficiales;
- recompensas;
- ranking.
```

---

### Regla 2 — El estado de batalla PvP vive en el Dedicated Server

```txt
Jugador A ─┐
           ├── Unreal Dedicated Server ─── Resultado oficial
Jugador B ─┘
```

---

### Regla 3 — El estado de campaña online vive en backend

```txt
Unreal Client
→ Backend Rust/Axum
→ PostgreSQL
→ Redis para sesiones
```

---

### Regla 4 — El single-player debe funcionar offline

```txt
Single Player:
Unreal Client
→ Simulación local
→ SQLite / SaveGame local
```

---

### Regla 5 — No hardcodear balance del juego

Todo dato de juego debe estar en:

```txt
Data Assets
Data Tables
JSON versionado
CSV versionado
SQLite local
PostgreSQL backend
```

Nunca hardcodear:

```txt
stats de unidades
recursos
costos
daño
moral
precios
bonos
edificios
tecnologías
relaciones diplomáticas
```

---

# MÓDULOS DE UNREAL

## Módulos principales

```txt
WorldLeaderCore
WorldLeaderCampaign
WorldLeaderBattle
WorldLeaderAI
WorldLeaderEconomy
WorldLeaderDiplomacy
WorldLeaderMilitary
WorldLeaderPolitics
WorldLeaderTechnology
WorldLeaderOnline
WorldLeaderUI
WorldLeaderData
WorldLeaderSave
WorldLeaderModding
```

---

## Estructura sugerida

```txt
Source/
└── WorldLeader/
    ├── Core/
    │   ├── WLGameTypes.h
    │   ├── WLConstants.h
    │   └── WLDataRegistry.h
    │
    ├── Campaign/
    │   ├── WLCampaignGameMode.cpp
    │   ├── WLWorldMapManager.cpp
    │   ├── WLProvinceSystem.cpp
    │   ├── WLCountrySystem.cpp
    │   └── WLStrategicTickSystem.cpp
    │
    ├── Battle/
    │   ├── WLBattleGameMode.cpp
    │   ├── WLUnit.cpp
    │   ├── WLSquad.cpp
    │   ├── WLTankUnit.cpp
    │   ├── WLInfantryUnit.cpp
    │   ├── WLArtillerySystem.cpp
    │   ├── WLAirSupportSystem.cpp
    │   ├── WLMoraleSystem.cpp
    │   └── WLDamageResolver.cpp
    │
    ├── AI/
    │   ├── WLStrategicAI.cpp
    │   ├── WLTacticalAI.cpp
    │   ├── WLDiplomacyAI.cpp
    │   └── WLEconomicAI.cpp
    │
    ├── Online/
    │   ├── WLMatchmakingClient.cpp
    │   ├── WLBackendClient.cpp
    │   ├── WLSteamIntegration.cpp
    │   └── WLReplayUploader.cpp
    │
    ├── UI/
    │   ├── WLCampaignHUD.cpp
    │   ├── WLBattleHUD.cpp
    │   ├── WLDiplomacyPanel.cpp
    │   ├── WLEconomyPanel.cpp
    │   └── WLProvincePanel.cpp
    │
    └── Save/
        ├── WLSaveGame.cpp
        ├── WLSaveSerializer.cpp
        └── WLSQLiteSaveProvider.cpp
```

---

# BACKEND ONLINE

## Stack backend

```txt
Lenguaje:        Rust
Framework API:   Axum
DB:              PostgreSQL
Cache:           Redis
Mensajería:      NATS opcional
Storage:         S3-compatible
Auth:            Steam / Epic / cuenta propia
Deploy:          Docker
Orquestación:    Kubernetes
Observabilidad:  OpenTelemetry + Prometheus + Grafana + Loki
```

---

## Servicios backend

```txt
auth-service
profile-service
campaign-service
matchmaking-service
battle-result-service
ranking-service
diplomacy-service
mod-service
telemetry-service
replay-service
```

---

## Base de datos PostgreSQL

Tablas principales:

```txt
users
profiles
games
campaigns
nations
provinces
province_ownership
armies
units
buildings
technologies
diplomacy_relations
alliances
wars
treaties
battle_results
rankings
replays
mods
audit_logs
```

---

## Redis

Uso:

```txt
sesiones activas
lobbies
matchmaking temporal
presence online
locks de campaña
colas ligeras
rate limiting
estado efímero de sala
```

---

# MULTIPLAYER Y PVP

## Tipos de multiplayer

```txt
1v1 batalla táctica
2v2 batalla táctica
campaña cooperativa
campaña competitiva asíncrona
campaña persistente por temporadas
```

---

## Modelo recomendado

```txt
PvP táctico:
Unreal Dedicated Server autoritativo

Campaña online:
Backend Rust + PostgreSQL

Matchmaking:
Steamworks + backend propio

Ranking:
Backend propio

Replays:
Cliente graba + servidor valida + storage S3
```

---

## Flujo PvP

```txt
1. Jugador busca partida
2. Steamworks / backend crea lobby
3. Backend asigna dedicated server
4. Clientes conectan al server
5. Server carga mapa táctico
6. Server recibe composición de ejército
7. Server valida unidades
8. Batalla inicia
9. Server simula estado autoritativo
10. Clientes renderizan y envían comandos
11. Server determina resultado
12. Backend guarda resultado
13. Ranking se actualiza
14. Replay se almacena
```

---

# DATOS GEOGRÁFICOS

## Fuentes de datos

```txt
Natural Earth
GADM
OpenStreetMap opcional
Datos manuales balanceados
```

---

## Pipeline recomendado

No cargar GeoJSON crudo directamente dentro del juego final.

Usar pipeline de conversión:

```txt
GeoJSON / Shapefile
→ herramienta interna de validación
→ simplificación de polígonos
→ generación de ProvinceGraph
→ conversión a Unreal DataAssets
→ generación de meshes/mapa estratégico
→ empaquetado en build
```

---

## Formato de provincia

```json
{
  "id": "VE-ZU",
  "name": "Zulia",
  "country_iso": "VE",
  "region": "Venezuela",
  "capital": "Maracaibo",
  "terrain": "coastal_plain",
  "base_oil": 800,
  "base_gas": 300,
  "base_food": 120,
  "base_minerals": 100,
  "base_industry": 180,
  "population": 5200000,
  "infrastructure": 55,
  "strategic_value": 9,
  "is_capital": false,
  "has_port": true,
  "has_airbase": true,
  "neighbors": ["VE-FA", "VE-LA", "VE-TR", "CO-GUA"]
}
```

---

# FASES DE DESARROLLO

## Phase 0 — Unreal Foundation

Duración estimada: 2-4 semanas

```txt
- Crear proyecto Unreal Engine 5.x C++
- Configurar módulos base
- Configurar Git LFS
- Configurar estructura de carpetas
- Crear mapa estratégico prototipo
- Crear menú principal
- Crear selección de nación básica
- Crear DataTables iniciales
- Crear SaveGame local
- Crear 5 provincias de prueba
- Crear 2 naciones de prueba
```

Entregable:

```txt
Proyecto Unreal funcional con menú, mapa estratégico básico y datos cargados.
```

---

## Phase 1 — Campaña estratégica MVP

Duración estimada: 6-10 semanas

```txt
- Mapa mundial simplificado
- 30 países jugables o visibles
- 100-300 provincias iniciales
- Sistema de provincias
- Sistema económico básico
- Construcción de edificios
- Recursos
- Población
- Orden público
- Tesoro nacional
- Tick estratégico mensual
- IA económica simple
- Guardado local
```

Entregable:

```txt
Puedes elegir una nación, administrar provincias, construir edificios y sostener economía.
```

---

## Phase 2 — Ejércitos y auto-resolve

Duración estimada: 6-10 semanas

```txt
- Crear ejércitos en mapa estratégico
- Movimiento entre provincias
- Rutas terrestres
- Rutas marítimas
- Acceso militar
- Declaración de guerra
- Suministro
- Combustible
- Auto-resolve
- Ocupación
- Guarniciones
- IA militar básica
```

Entregable:

```txt
Puedes mover ejércitos, declarar guerra, conquistar provincias y defender territorio.
```

---

## Phase 3 — Batalla táctica vertical slice

Duración estimada: 10-16 semanas

```txt
- Mapa táctico 3D de prueba
- Cámara tipo Total War
- Selección de unidades
- Movimiento en tiempo real
- Infantería
- Tanques
- Artillería
- Moral
- Daño básico
- Línea de visión
- Captura de punto
- Victoria / derrota
- IA táctica básica
```

Entregable:

```txt
Primera batalla táctica moderna jugable en tiempo real.
```

---

## Phase 4 — PvP con Dedicated Server

Duración estimada: 10-16 semanas

```txt
- Build de Unreal Dedicated Server Linux
- Replicación de unidades
- Comandos del jugador
- Servidor autoritativo
- Lobbies
- Matchmaking básico
- Backend Rust/Axum
- PostgreSQL
- Redis
- Resultado validado
- Replay básico
```

Entregable:

```txt
Batallas PvP online 1v1 funcionales.
```

---

## Phase 5 — Diplomacia, política y mundo vivo

Duración estimada: 8-14 semanas

```txt
- Diplomacia bilateral
- Tratados
- Alianzas
- ONU
- OTAN
- BRICS
- OPEP
- FMI
- Sanciones
- Elecciones
- Leyes
- Estabilidad
- Corrupción
- Espionaje
- Eventos globales
- Crisis regionales
```

Entregable:

```txt
Campaña global con política, diplomacia y consecuencias.
```

---

## Phase 6 — Escala, contenido y Steam

Duración estimada: 12-24 semanas

```txt
- 750 provincias completas
- 60+ facciones jugables
- Escenarios históricos
- Balance económico
- Balance militar
- Tutorial
- Campaña Venezuela / Colombia
- Steam integration
- Logros
- Steam Cloud
- Workshop
- Mods
- Optimización
- QA
- Telemetría
- Anti-cheat básico
```

Entregable:

```txt
Beta pública comercializable.
```

---

# INSTRUCCIONES PARA CLAUDE CODE / CODEX / AGENTES DE DESARROLLO

```txt
LEER ESTE ARCHIVO COMPLETO ANTES DE CUALQUIER TAREA.

PROYECTO:
World Leader — Juego PC de estrategia geopolítica moderna.

INSPIRACIÓN:
Rome Total War 2 + Hearts of Iron IV + Democracy Series + guerra moderna.

PLATAFORMA:
PC nativo con Unreal Engine 5.x.

STACK PRINCIPAL:
Game Engine:         Unreal Engine 5.x
Lenguaje:            C++
Scripting visual:    Blueprints controlados
UI:                  UMG + CommonUI
Gameplay:            Gameplay Ability System
Entidades masivas:   Mass Entity
Networking:          Unreal Dedicated Servers
Backend:             Rust + Axum
DB:                  PostgreSQL
Cache:               Redis
Distribución:        Steam

NO USAR COMO CLIENTE PRINCIPAL:
Tauri
React
Leaflet
Web Browser
Supabase como motor central del juego

TAURI SOLO PUEDE USARSE PARA:
Launcher
Herramientas internas
Editor de provincias
Editor de balance
Herramienta de modding
Panel administrativo
```

---

## Convenciones Unreal

```txt
- C++ para sistemas core.
- Blueprints solo para UI, prototipos y eventos visuales.
- DataAssets/DataTables para datos balanceables.
- GameInstance para estado global del cliente.
- GameMode para reglas del servidor.
- GameState para estado replicado.
- PlayerController para input del jugador.
- PlayerState para datos replicados del jugador.
- Actor Components para capacidades reutilizables.
- Mass Entity para entidades numerosas.
- GAS para habilidades, atributos, efectos, moral, buffs y debuffs.
```

---

## Archivos críticos

```txt
Source/WorldLeader/Campaign/WLStrategicTickSystem.cpp
→ Motor de campaña estratégica.

Source/WorldLeader/Campaign/WLProvinceSystem.cpp
→ Sistema de provincias.

Source/WorldLeader/Campaign/WLCountrySystem.cpp
→ Sistema de países.

Source/WorldLeader/Economy/WLEconomySystem.cpp
→ Economía nacional y provincial.

Source/WorldLeader/Military/WLMilitarySystem.cpp
→ Ejércitos, unidades y composición militar.

Source/WorldLeader/Battle/WLBattleGameMode.cpp
→ Reglas de batalla táctica.

Source/WorldLeader/Battle/WLDamageResolver.cpp
→ Resolución de daño.

Source/WorldLeader/Battle/WLMoraleSystem.cpp
→ Moral, supresión y retirada.

Source/WorldLeader/AI/WLStrategicAI.cpp
→ IA de campaña.

Source/WorldLeader/AI/WLTacticalAI.cpp
→ IA de batalla.

Source/WorldLeader/Online/WLBackendClient.cpp
→ Comunicación con backend Rust.

Source/WorldLeader/Online/WLMatchmakingClient.cpp
→ Matchmaking.

Content/Data/Provinces/
→ Datos de provincias.

Content/Data/Nations/
→ Datos de naciones.

Content/Data/Units/
→ Datos de unidades.

Content/Data/Buildings/
→ Datos de edificios.

Content/Data/Technologies/
→ Árbol tecnológico.
```

---

## Al implementar una feature

```txt
1. Definir datos en DataAsset/DataTable/JSON.
2. Crear structs C++.
3. Crear sistema core en C++.
4. Exponer solo lo necesario a Blueprint.
5. Crear UI en UMG/CommonUI.
6. Crear test funcional.
7. Validar guardado.
8. Validar multiplayer si aplica.
9. Validar performance.
10. Documentar.
```

---

## Reglas obligatorias

```txt
NUNCA hardcodear valores de balance.

NUNCA poner lógica crítica de PvP solo en cliente.

NUNCA permitir que el cliente decida resultados oficiales.

NUNCA hacer queries directas a PostgreSQL desde el cliente Unreal.

NUNCA usar Redis como base de datos permanente.

NUNCA usar Blueprints para sistemas core pesados.

NUNCA cargar GeoJSON crudo masivo en runtime si ya puede preprocesarse.

NUNCA mezclar lógica de campaña con lógica de batalla sin interfaces claras.

NUNCA modificar archivos críticos sin revisar dependencias.

NUNCA romper compatibilidad de saves sin migración.
```

---

## Prioridad actual

```txt
PRIORIDAD ACTUAL:
Phase 0 — Unreal Foundation

OBJETIVO:
Crear una vertical slice mínima que pruebe:

1. Mapa estratégico básico.
2. Selección de nación.
3. Provincias.
4. Economía simple.
5. Ejército básico.
6. Transición a batalla táctica.
7. Batalla en tiempo real con infantería y tanques.
```

---

# RESUMEN FINAL DEL STACK

```txt
World Leader ya no usa Tauri como cliente principal.

World Leader usa:

Unreal Engine 5.x
+ C++
+ UMG/CommonUI
+ Gameplay Ability System
+ Mass Entity
+ Dedicated Servers
+ Rust/Axum backend
+ PostgreSQL
+ Redis
+ Steamworks
+ Docker/Kubernetes
+ AWS/Azure/GCP
```

La arquitectura correcta es:

```txt
Unreal para el juego.
Rust para el backend.
PostgreSQL para persistencia.
Redis para sesiones.
Dedicated Servers para PvP.
Steam para distribución, lobbies y comunidad.
Tauri solo para herramientas internas.
```

---

*World Leader — GDD v2.1 | PC Native | Unreal Engine | Mapa mundial completo | Rome Total War 2 DNA moderno*

---

## ANEXO — RED VIAL DE CAMPAÑA = TABLERO DE GUERRA E INTRIGA

> Por qué nos obsesiona que TODAS las ciudades y carreteras estén interconectadas
> y bien hechas: porque la red vial **no es decorado, es el grafo de movimiento**.

**Las tropas se mueven por las carreteras.** Como en Total War, un ejército recorre el
mapa estratégico ciudad por ciudad por los corredores reales — no se teletransporta.
De ahí se desprenden las reglas duras del mapa:

- **Ninguna carretera cortada ni desconectada.** Una ruta rota = un ejército que no puede
  llegar, reforzar ni invadir por ahí.
- **Ciudades = nodos** (objetivos: capital, puerto, frontera, industrial). **Carreteras =
  aristas** con costo/distancia por donde fluyen ejércitos y logística.
- **Cruces fronterizos reales = puntos de choque.** Una invasión Colombia→Venezuela entra
  por San Antonio del Táchira–Cúcuta (puente Simón Bolívar) o por Paraguachón–Maicao. El
  que controla el cruce controla la ruta de invasión.

### Mecánicas de política e intriga que corren sobre esta red
- **Golpes de Estado:** mover/posicionar la guarnición leal por el eje interno (p.ej.
  Troncal 1 San Cristóbal→Caracas) para tomar la capital; el rival corre tropas por las
  mismas vías para defender. La geografía dicta velocidad y ventana del golpe.
- **Incitar golpes en otros países:** financiar/insertar agentes en una capital vecina y
  desestabilizarla; el éxito depende de presencia, rutas de apoyo y control de cruces.
- **Guerrillas / insurgencia:** focos que cortan carreteras y emboscan en terreno difícil
  (selva, Andes), obligando a desviar tropas y encareciendo el control del territorio.
- **Invasiones convencionales:** concentrar fuerzas en ciudades fronterizas y empujar por
  los corredores reales hacia las capitales/puertos enemigos; los puentes y pasos son
  cuellos de botella decisivos.

**Implicación técnica (pendiente):** convertir esta red en el **grafo de pathfinding** de
ejércitos (nodos=ciudades, aristas=corredores con costo), para que "elegí destino → la
tropa recorre las carreteras reales y cruza por la frontera real" sea la jugada base.

---

## ANEXO — CAMPAIGN3D: GAMEPLAY DE PRESIDENTE + UX PENDIENTE

> **Estado actual del prototipo Campaign3D (teatro Colombia–Venezuela):** ya hay mapa 3D,
> ciudades, carreteras, **fuertes de reclutamiento**, **ejércitos que se mueven siguiendo el
> trazado de la carretera** por turnos, **tick mensual [M]**, tesoro por país y
> **reclutamiento por país (cada país en sus fuertes, tesoro/ejército independientes)**.
> Lo que FALTA es la **capa de gobierno** ("¿qué puedo hacer como presidente?") y **pulir la
> UX**. La visión macro ya está descrita arriba en «SISTEMAS POLÍTICOS Y DIPLOMÁTICOS»,
> «ECONOMÍA», «Edificios por slot» y la Phase 5; esta sección la baja a **tareas concretas**.

### A. UX / pulido inmediato (lo que se ve feo o confuso HOY)
- [x] **Cards de reclutamiento legibles y amigables.** (1ª pasada) Cards grandes de 60px con icono + nombre + coste + días + franja de categoría; fuentes mayores. Queda afinar tamaños de fuente si hace falta.
- [x] **Avance de turno claro.** Botón grande «AVANZAR DIA» abajo-derecha (estilo "Finalizar turno") + tecla [M]; el tiempo ahora avanza en DÍAS (fecha DD/MM/AAAA, "Balance/día").
- [~] **Leyenda de controles / onboarding.** Leyenda inferior actualizada ("[M] Avanzar dia"); falta el panel/tooltips de onboarding completo.
- [ ] **Feedback claro de cada acción.** Mensajes de reclutar / mover / fondos insuficientes / "fuerte de otro país" bien visibles.
- [ ] **HUD con estándar visual AAA** (tipografías, colores, paneles) — ver `Docs/CAMPAIGN3D_VISUAL_STANDARD.md`.

### B. Gameplay de presidente (la capa que falta)
Qué puede HACER el jugador gobernando su país, sobre el tick mensual + economía ya existentes:
- [~] **Economía activa:** backend FE1-FE6 listo para UI en campaña local: impuestos, presupuesto, deuda, bonos/préstamos/FMI, rating/default, ayuda exterior, FDI, PIB, sectores, cadenas, demanda, precios, shocks de mercado, comercio regional/global, acuerdos comerciales, aranceles, embargos/sanciones, rutas cortables por guerra, inflación, ciclo económico y gobernanza económica por ministro/corrupción/tecnología. Falta backend online/PvP si se entra a Phase 4 y falta UIX completa.
- [~] **Ministerios / gabinete:** backend F1 listo para UI: roster, gabinete, capital político, nombrar/remover ministros, autogeneral al crear ejército, crear/asignar generales, ascender/retirar, renombre y save/load. Falta la UIX completa para que el jugador lo use desde el HUD.
- [~] **Edificios provinciales:** backend listo para UI: catálogo JSON con slots económico/industrial/militar/naval/aéreo/tecnológico/financiero/infra/defensa, niveles 1–5, coste de mejora, mantenimiento, efectos económicos/militares/orden público, save/load v9, IA económica que construye o mejora, y bonus defensivo en auto-resolve. Falta UIX completa de slots, niveles y botón de mejorar.
- [~] **Relaciones internacionales:** backend F3 listo para UI: matriz de opinión, paz/tensión/guerra, declarar guerra, tratados y bloqueo de combate si no hay guerra.
- [~] **Alianzas y bloques:** backend F3 listo para UI: tratados `TradeAgreement`, `NonAggression`, `Alliance`, `Embargo`; falta representación visual/acciones completas de bloque.
- [~] **Espionaje y sabotaje:** backend F4 listo para UI: redes de inteligencia, sabotaje económico/militar, financiar golpe, propaganda y contraespionaje.
- [~] **Orden público y sublevaciones:** backend F2/F5 listo para UI: orden público alimenta oposición, eventos y riesgo de golpe; falta pantalla/feedback de revuelta.
- [~] **Golpes de Estado (sufrirlos y darlos):** backend F2/F4 listo para UI: riesgo, intento de golpe, financiación externa y derrota por golpe exitoso.
- [~] **Generales rebeldes:** backend base listo: lealtad/ambición alimentan golpe; falta la rama visual/narrativa de deserción con ejército.
- [~] **Intrigas / eventos políticos:** backend F5 listo para UI: eventos JSON, cola, resolver opción, efectos persistentes.
- [~] **Ascensiones / progresión:** backend F1 listo para UI: ascenso, retiro, renombre mensual y por batalla; falta pantalla y ceremonias/feedback.

### C. Frontend / UIX pendiente para gobierno
- [ ] **Panel Gabinete real:** cards por ministerio con ministro actual, skill, lealtad, ambición, popularidad, rasgos y coste de acción.
- [ ] **Acciones de gabinete:** flujo de nombrar/remover ministro con selector de candidatos, confirmación, preview de capital político y feedback visible.
- [ ] **Panel Personajes:** lista filtrable por rol (ministros, generales, oposición, espías, líderes) y país.
- [ ] **Stats presidenciales:** mostrar capital político, estabilidad, corrupción y riesgo de golpe con tooltips y colores de riesgo.
- [ ] **Panel Gobernanza económica:** mostrar ministro de Economía, skill/rasgos, tecnología, eficiencia fiscal, pérdida por corrupción y productividad.
- [ ] **Panel finanzas avanzadas / ayuda / FDI:** mostrar rating, deuda, crédito disponible, servicio mensual, riesgo/default, FMI, ayudas activas, FDI y acciones de emitir deuda, pedir préstamo/FMI, conceder ayuda e invertir.
- [ ] **Panel shocks de mercado:** mostrar shocks activos, bien afectado, duración restante, multiplicador e impacto en inflación/comercio.
- [ ] **Panel comercio exterior:** rutas por país desde `GetTradeRoutesForNation`, acuerdos/embargos, arancel nacional, imports/exports, `TariffIncome` y preview de impacto diplomático.
- [ ] **Generales en UI militar:** mostrar general asignado en ejército/selección y permitir asignarlo desde una lista válida.
- [ ] **Panel Diplomacia:** lista de relaciones desde `UWLPoliticalSubsystem::GetRelationsForNation`, acciones `DeclareWar`/`SignTreaty`/`BreakTreaty`.
- [ ] **Panel Intriga:** redes desde `GetIntelligenceNetwork`, acciones `BuildSpyNetwork`/`RunSpyOperation`, riesgo de exposición y resultado.
- [ ] **Eventos políticos:** cola desde `GetQueuedEvents`, modal con opciones y `ResolveEvent`.
- [ ] **Registro político:** log de decisiones y eventos de gobierno para que las consecuencias sean legibles.
- [ ] **Edificios provinciales UIX:** mostrar slots reales, edificio actual, nivel 1–5, coste de mejora, mantenimiento, efectos y acciones construir/mejorar desde HUD.

### Orden sugerido de implementación
1. **A (UX)** — barato y desbloquea poder jugar/testear con gusto (cards + botón de turno + leyenda).
2. **Economía activa + edificios + orden público** — el bucle de gobierno base.
3. **Diplomacia + alianzas + ministerios** — el mundo vivo.
4. **Espionaje + golpes de Estado + generales rebeldes + intrigas** — la capa de traición Total-War.

**Código actual relevante:** `WLStrategicTickSubsystem*` (tick, tesoro, orden público, reclutamiento, economía, shocks de mercado FE3.4, comercio avanzado FE4.2-FE4.5, finanzas avanzadas FE5.1/FE5.3, gobernanza económica FE6, edificios provinciales con niveles/efectos), `WLFinancialTypes.h` (instrumentos de deuda, perfil financiero, ayuda/FDI), `WLCharacterSubsystem*` (personajes/gabinete/capital político/generales), `WLPoliticalSubsystem*` (poder interno, golpes, diplomacia, tratados/acuerdos/embargos, aranceles, intriga, eventos/outcome y disparo de shocks desde opciones F5), `WLCampaign3DView*` (mapa/ciudades/ejércitos/carreteras), `WLCampaignHUD*` + `WLCampaignHUDPanels.inl` (paneles/cards), `WLCampaignPlayerController*` (input/selección/reclutar). Datos en `Content/Data/` (Nations, Provinces, Units, Buildings, Goods, Characters, Political, Campaign3D/RecruitableUnits.json).
