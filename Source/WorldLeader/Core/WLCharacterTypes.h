// Copyright World Leader project. See ROADMAP.md.
//
// Modelo de PERSONAJE: el motor central del juego (ver Docs/WORLD_LEADER_VISION.md).
// Generales, ministros, oposicion, lideres extranjeros y espias comparten ESTE mismo struct;
// asi un solo sistema alimenta las 4 arenas (Guerra, Alto Mando, Diplomacia, Intriga).
// El estado mutable (lealtad, renombre, ejercito asignado) vive en runtime; los pools de
// nombres/rasgos se cargan desde Content/Data/Characters/ (fase F1.3).

#pragma once

#include "CoreMinimal.h"
#include "WLCharacterTypes.generated.h"

/** Que ES el personaje (define en que arena participa). */
UENUM(BlueprintType)
enum class EWLCharacterRole : uint8
{
	General       UMETA(DisplayName = "General"),        // lidera ejercitos (Guerra / Alto Mando)
	Minister      UMETA(DisplayName = "Ministro"),       // gabinete (gobierno interno)
	Opposition    UMETA(DisplayName = "Oposicion"),      // rival politico interno (golpes)
	ForeignLeader UMETA(DisplayName = "Lider extranjero"),// jefe de estado rival (Diplomacia)
	Spy           UMETA(DisplayName = "Espia")           // agente (Intriga)
};

/** Rango militar de un general. El ascenso (F1.7) sube por esta escala. */
UENUM(BlueprintType)
enum class EWLMilitaryRank : uint8
{
	Colonel         UMETA(DisplayName = "Coronel"),
	BrigadeGeneral  UMETA(DisplayName = "General de brigada"),
	DivisionGeneral UMETA(DisplayName = "General de division"),
	CorpsGeneral    UMETA(DisplayName = "General de cuerpo"),
	FieldMarshal    UMETA(DisplayName = "Mariscal de campo")
};

/**
 * Un PERSONAJE. Nucleo unico del juego: sus stats (Skill/Loyalty/Ambition/Popularity) y rasgos
 * gobiernan combate, riesgo de golpe, diplomacia e intriga segun su Role. Todos los valores 0..100
 * salvo Renown (acumulativo) y Rank (escala enum). Se construye/gestiona en UWLCharacterSubsystem (F1.2).
 */
USTRUCT(BlueprintType)
struct FWLCharacter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Character") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Character") FString Name;
	/** ISO del pais al que sirve (CO, VE...). */
	UPROPERTY(BlueprintReadOnly, Category = "Character") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "Character") EWLCharacterRole Role = EWLCharacterRole::General;
	UPROPERTY(BlueprintReadOnly, Category = "Character") EWLMilitaryRank Rank = EWLMilitaryRank::Colonel;

	/** Competencia: pesa en el resultado de batalla y en la eficacia de sus acciones. 0..100. */
	UPROPERTY(BlueprintReadOnly, Category = "Character") int32 Skill = 50;
	/** Lealtad al lider actual. Baja + Ambicion alta = riesgo de golpe (F2). 0..100. */
	UPROPERTY(BlueprintReadOnly, Category = "Character") int32 Loyalty = 60;
	/** Ambicion: cuanto ansia mas poder. Motor de golpes e intrigas. 0..100. */
	UPROPERTY(BlueprintReadOnly, Category = "Character") int32 Ambition = 40;
	/** Popularidad ante el pueblo/tropa: protege (o amenaza) al lider. 0..100. */
	UPROPERTY(BlueprintReadOnly, Category = "Character") int32 Popularity = 40;
	/** Renombre acumulado (experiencia): sube con turnos/combate y desbloquea ascensos/rasgos (F1.8). */
	UPROPERTY(BlueprintReadOnly, Category = "Character") int32 Renown = 0;

	/** IDs de rasgos (cargados del pool en F1.3): "audaz", "corrupto", "estratega"... */
	UPROPERTY(BlueprintReadOnly, Category = "Character") TArray<FString> Traits;

	/** Id del ejercito que lidera (vacio = sin mando). Enlaza personaje <-> ficha del mapa (F1.4). */
	UPROPERTY(BlueprintReadOnly, Category = "Character") FString AssignedArmyId;

	/** Vivo/activo. Al dar de baja o morir en un golpe pasa a false (no se borra: historial). */
	UPROPERTY(BlueprintReadOnly, Category = "Character") bool bActive = true;

	bool IsValid() const { return !Id.IsEmpty(); }
};
