// Copyright World Leader project. See ROADMAP.md.
//
// Modo BATALLA TACTICA 3D interactiva del PlayerController: inicia la batalla en
// el backend determinista (UWLTacticalBattleSubsystem via UWLMilitarySubsystem),
// spawnea la vista (AWLTacticalBattleView), avanza la simulacion cada frame,
// traduce clics a seleccion/ordenes y, al terminar, aplica el resultado a campana.

#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Battle/WLTacticalBattleSubsystem.h"
#include "Military/WLMilitarySubsystem.h"
#include "Presentation/WLTacticalBattleView.h"
#include "Presentation/WLCampaign3DView.h"
#include "UI/WLTacticalHudLayout.h"
#include "WorldLeader.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	UWLTacticalBattleSubsystem* GetTacticalSubsystem(const UObject* Ctx)
	{
		const UGameInstance* GI = UGameplayStatics::GetGameInstance(Ctx);
		return GI ? GI->GetSubsystem<UWLTacticalBattleSubsystem>() : nullptr;
	}
	UWLMilitarySubsystem* GetMilitarySubsystem(const UObject* Ctx)
	{
		const UGameInstance* GI = UGameplayStatics::GetGameInstance(Ctx);
		return GI ? GI->GetSubsystem<UWLMilitarySubsystem>() : nullptr;
	}

	// Ritmo de la batalla: segundos de simulacion por segundo real. >1 = mas agil de ver.
	constexpr float BattleSpeed = 1.6f;
}

void AWLCampaignPlayerController::EnterTacticalBattle(const FString& AttackerId, const FString& DefenderId)
{
	if (bTacticalBattleActive)
	{
		return;
	}
	UWLMilitarySubsystem* Military = GetMilitarySubsystem(this);
	UWLTacticalBattleSubsystem* Tactical = GetTacticalSubsystem(this);
	const UWLCampaignGameInstance* CampaignGI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!Military || !Tactical || !CampaignGI)
	{
		SetLastActionMessage(TEXT("No se pudo iniciar la batalla tactica (subsistemas no disponibles)."), false);
		return;
	}

	FWLTacticalBattleState Battle;
	FString Message;
	if (!Military->StartTacticalBattle(AttackerId, DefenderId, Battle, Message))
	{
		SetLastActionMessage(Message, false);
		return;
	}

	TacticalPlayerIso = CampaignGI->GetSelectedNationIso().TrimStartAndEnd().ToUpper();
	// El rival lo lleva la IA tactica; el bando del jugador queda bajo su mando directo.
	FString AiMessage;
	const bool bPlayerIsAttacker = Battle.AttackerIso.Equals(TacticalPlayerIso, ESearchCase::IgnoreCase);
	const bool bPlayerIsDefender = Battle.DefenderIso.Equals(TacticalPlayerIso, ESearchCase::IgnoreCase);
	if (bPlayerIsAttacker || bPlayerIsDefender)
	{
		Tactical->SetTacticalAIControl(Battle.BattleId,
			bPlayerIsAttacker ? Battle.DefenderIso : Battle.AttackerIso, true, AiMessage);
	}
	else
	{
		// El jugador no participa: ambos por IA (modo espectador).
		Tactical->SetTacticalAIControl(Battle.BattleId, Battle.AttackerIso, true, AiMessage);
		Tactical->SetTacticalAIControl(Battle.BattleId, Battle.DefenderIso, true, AiMessage);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TacticalBattleView = World->SpawnActor<AWLTacticalBattleView>(AWLTacticalBattleView::StaticClass());
	if (!TacticalBattleView)
	{
		SetLastActionMessage(TEXT("No se pudo crear la vista de batalla."), false);
		return;
	}
	TacticalBattleView->Initialize(Battle, TacticalPlayerIso);

	TacticalBattleId = Battle.BattleId;
	TacticalBattleCache = Battle;
	TacticalSelectedUnitId.Reset();
	TacticalStepAccumulator = 0.f;
	bTacticalFinished = false;
	bTacticalBattleActive = true;

	// Guarda la camara actual y encuadra el campo de batalla.
	PreTacticalViewTarget = GetViewTarget();
	if (ACameraActor* Cam = TacticalBattleView->GetBattleCamera())
	{
		SetViewTargetWithBlend(Cam, 0.5f);
	}
	EnterCampaignInputMode();   // GameAndUI con cursor: clics de seleccion/orden + HUD
	SetLastActionMessage(FString::Printf(
		TEXT("Batalla tactica: comandas tu bando. Clic en tu unidad y luego en destino/enemigo. %s"), *Message), true);
}

void AWLCampaignPlayerController::RefreshTacticalBattleCache()
{
	if (UWLTacticalBattleSubsystem* Tactical = GetTacticalSubsystem(this))
	{
		Tactical->GetTacticalBattleState(TacticalBattleId, TacticalBattleCache);
	}
	if (TacticalBattleView)
	{
		TacticalBattleView->RefreshFromState(TacticalBattleCache);
	}
}

void AWLCampaignPlayerController::TickTacticalBattle(float DeltaSeconds)
{
	if (!bTacticalBattleActive)
	{
		return;
	}
	if (bTacticalFinished)
	{
		return;   // batalla resuelta: se queda a la vista hasta que el jugador pulse TERMINAR
	}

	UWLTacticalBattleSubsystem* Tactical = GetTacticalSubsystem(this);
	if (!Tactical)
	{
		return;
	}

	TArray<FString> Events;
	Tactical->AdvanceTacticalBattle(TacticalBattleId,
		static_cast<double>(FMath::Min(DeltaSeconds, 0.1f) * BattleSpeed), TacticalBattleCache, Events);
	if (TacticalBattleView)
	{
		TacticalBattleView->RefreshFromState(TacticalBattleCache);
	}
	if (!TacticalBattleCache.bActive && TacticalBattleCache.Result != EWLTacticalBattleResult::Ongoing)
	{
		bTacticalFinished = true;
		SetLastActionMessage(TEXT("Batalla resuelta. Pulsa TERMINAR para aplicar el resultado a la campana."), true);
	}
}

bool AWLCampaignPlayerController::HandleTacticalBattleClick()
{
	// 1) Botones del HUD tactico (hit-test en espacio Canvas, mismas coords que dibuja el HUD).
	float CanvasW = 0.f, CanvasH = 0.f, OffsetX = 0.f, OffsetY = 0.f;
	float MouseX = 0.f, MouseY = 0.f;
	if (GetCampaignCanvasMetrics(CanvasW, CanvasH, OffsetX, OffsetY) && GetMousePosition(MouseX, MouseY))
	{
		const FVector2D CanvasMouse(MouseX - OffsetX, MouseY - OffsetY);
		if (WLTacticalHudLayout::AutoResolveButton(CanvasW, CanvasH).IsInside(CanvasMouse))
		{
			AutoFinishTacticalBattle();
			return true;
		}
		if (WLTacticalHudLayout::ExitButton(CanvasW, CanvasH).IsInside(CanvasMouse))
		{
			ExitTacticalBattle(true);
			return true;
		}
	}

	if (bTacticalFinished || !TacticalBattleView)
	{
		return true;
	}

	// 2) Clic sobre el campo: deproyecta al plano del suelo (Z = GroundZ).
	FVector Origin, Direction;
	if (!DeprojectMousePositionToWorld(Origin, Direction) || FMath::IsNearlyZero(Direction.Z))
	{
		return true;
	}
	const double GroundZ = TacticalBattleView->GetGroundZ();
	const double T = (GroundZ - Origin.Z) / Direction.Z;
	if (T <= 0.0)
	{
		return true;
	}
	const FVector GroundPoint = Origin + Direction * T;

	const FString HitUnitId = TacticalBattleView->FindUnitNearWorldPoint(GroundPoint);

	// Es del jugador / del enemigo?
	auto UnitOwnerIso = [this](const FString& UnitId) -> FString
	{
		for (const FWLTacticalUnitState& U : TacticalBattleCache.Units)
		{
			if (U.TacticalUnitId == UnitId)
			{
				return U.OwnerIso;
			}
		}
		return FString();
	};

	UWLTacticalBattleSubsystem* Tactical = GetTacticalSubsystem(this);
	if (!Tactical)
	{
		return true;
	}

	if (!HitUnitId.IsEmpty() && UnitOwnerIso(HitUnitId).Equals(TacticalPlayerIso, ESearchCase::IgnoreCase))
	{
		// Selecciona una unidad propia.
		TacticalSelectedUnitId = HitUnitId;
		TacticalBattleView->SetSelectedUnit(HitUnitId);
		return true;
	}

	if (TacticalSelectedUnitId.IsEmpty())
	{
		return true;   // sin unidad seleccionada, un clic al vacio no hace nada
	}

	FString Message;
	if (!HitUnitId.IsEmpty())
	{
		// Clic sobre un enemigo -> orden de ataque.
		Tactical->IssueAttackOrder(TacticalBattleId, TacticalSelectedUnitId, HitUnitId, Message);
	}
	else
	{
		// Clic en suelo libre -> orden de movimiento a esa coordenada tactica.
		const FVector2D Target = TacticalBattleView->WorldToTactical(GroundPoint);
		Tactical->IssueMoveOrder(TacticalBattleId, TacticalSelectedUnitId, Target, Message);
	}
	return true;
}

void AWLCampaignPlayerController::AutoFinishTacticalBattle()
{
	UWLTacticalBattleSubsystem* Tactical = GetTacticalSubsystem(this);
	if (!Tactical || !bTacticalBattleActive || bTacticalFinished)
	{
		return;
	}
	// IA en ambos bandos y avanza hasta resolver (tope de seguridad).
	FString Message;
	Tactical->SetTacticalAIControl(TacticalBattleId, TacticalBattleCache.AttackerIso, true, Message);
	Tactical->SetTacticalAIControl(TacticalBattleId, TacticalBattleCache.DefenderIso, true, Message);
	for (int32 Step = 0; Step < 600; ++Step)
	{
		TArray<FString> Events;
		Tactical->AdvanceTacticalBattle(TacticalBattleId, 1.0, TacticalBattleCache, Events);
		if (!TacticalBattleCache.bActive && TacticalBattleCache.Result != EWLTacticalBattleResult::Ongoing)
		{
			break;
		}
	}
	if (TacticalBattleView)
	{
		TacticalBattleView->RefreshFromState(TacticalBattleCache);
	}
	bTacticalFinished = true;
	SetLastActionMessage(TEXT("Batalla auto-resuelta. Pulsa TERMINAR para aplicar el resultado."), true);
}

void AWLCampaignPlayerController::ExitTacticalBattle(bool bApplyResult)
{
	if (!bTacticalBattleActive)
	{
		return;
	}

	if (bApplyResult)
	{
		if (UWLMilitarySubsystem* Military = GetMilitarySubsystem(this))
		{
			// Si el jugador sale antes de tiempo, la IA cierra la batalla para poder aplicar el resultado.
			if (!bTacticalFinished)
			{
				AutoFinishTacticalBattle();
			}
			FString Message;
			Military->ApplyTacticalBattleResult(TacticalBattleId, Message);
			SetLastActionMessage(Message, true);
		}
	}

	if (TacticalBattleView)
	{
		TacticalBattleView->Destroy();
		TacticalBattleView = nullptr;
	}
	if (PreTacticalViewTarget)
	{
		SetViewTargetWithBlend(PreTacticalViewTarget, 0.5f);
		PreTacticalViewTarget = nullptr;
	}

	bTacticalBattleActive = false;
	bTacticalFinished = false;
	TacticalBattleId.Reset();
	TacticalSelectedUnitId.Reset();
	TacticalBattleCache = FWLTacticalBattleState();

	EnterCampaignInputMode();

	// Refresca los tokens del mapa: un ejercito pudo desaparecer/ocupar provincia.
	if (Campaign3DView)
	{
		Campaign3DView->SyncRecruitedArmyTokens();
	}
}

// --- Lectura para el HUD tactico ---

int32 AWLCampaignPlayerController::GetTacticalPlayerUnitCount() const
{
	int32 Count = 0;
	for (const FWLTacticalUnitState& U : TacticalBattleCache.Units)
	{
		if (!U.bDestroyed && U.Health > 0.0 && U.OwnerIso.Equals(TacticalPlayerIso, ESearchCase::IgnoreCase))
		{
			++Count;
		}
	}
	return Count;
}

int32 AWLCampaignPlayerController::GetTacticalEnemyUnitCount() const
{
	int32 Count = 0;
	for (const FWLTacticalUnitState& U : TacticalBattleCache.Units)
	{
		if (!U.bDestroyed && U.Health > 0.0 && !U.OwnerIso.Equals(TacticalPlayerIso, ESearchCase::IgnoreCase))
		{
			++Count;
		}
	}
	return Count;
}

FString AWLCampaignPlayerController::GetTacticalBattleHeadline() const
{
	return FString::Printf(TEXT("BATALLA TACTICA — %s en %s"),
		*TacticalBattleId, *TacticalBattleCache.ProvinceId);
}

FString AWLCampaignPlayerController::GetTacticalBattleStatusLine() const
{
	FString ObjectiveText = TEXT("neutral");
	if (TacticalBattleCache.Objectives.Num() > 0)
	{
		const FString Ctrl = TacticalBattleCache.Objectives[0].ControllerIso;
		ObjectiveText = Ctrl.IsEmpty()
			? TEXT("en disputa")
			: (Ctrl.Equals(TacticalPlayerIso, ESearchCase::IgnoreCase) ? TEXT("TUYO") : FString::Printf(TEXT("de %s"), *Ctrl));
	}
	return FString::Printf(TEXT("Tus unidades: %d   ·   Enemigo: %d   ·   Objetivo: %s   ·   Tiempo: %.0fs"),
		GetTacticalPlayerUnitCount(), GetTacticalEnemyUnitCount(), *ObjectiveText, TacticalBattleCache.ElapsedSeconds);
}

FString AWLCampaignPlayerController::GetTacticalBattleResultText() const
{
	if (!bTacticalFinished)
	{
		return FString();
	}
	switch (TacticalBattleCache.Result)
	{
	case EWLTacticalBattleResult::AttackerVictory:
		return TacticalBattleCache.AttackerIso.Equals(TacticalPlayerIso, ESearchCase::IgnoreCase)
			? TEXT("VICTORIA") : TEXT("DERROTA");
	case EWLTacticalBattleResult::DefenderVictory:
		return TacticalBattleCache.DefenderIso.Equals(TacticalPlayerIso, ESearchCase::IgnoreCase)
			? TEXT("VICTORIA") : TEXT("DERROTA");
	default:
		return TEXT("EMPATE");
	}
}

FString AWLCampaignPlayerController::GetTacticalSelectedUnitInfo() const
{
	if (TacticalSelectedUnitId.IsEmpty())
	{
		return TEXT("Sin unidad seleccionada — clic en una unidad azul para darle ordenes.");
	}
	for (const FWLTacticalUnitState& U : TacticalBattleCache.Units)
	{
		if (U.TacticalUnitId == TacticalSelectedUnitId)
		{
			return FString::Printf(TEXT("Seleccion: %s   ·   Salud %.0f   ·   Moral %.0f   ·   %s"),
				*U.DisplayName, U.Health, U.Morale,
				U.Order == EWLTacticalUnitOrder::Attacking ? TEXT("atacando")
				: (U.Order == EWLTacticalUnitOrder::Moving ? TEXT("moviendose")
				: (U.Order == EWLTacticalUnitOrder::Routing ? TEXT("en desbandada") : TEXT("en espera"))));
		}
	}
	return FString();
}
