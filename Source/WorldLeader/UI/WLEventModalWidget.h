// Copyright World Leader project. See ROADMAP.md.
//
// Popup modal de EVENTO POLITICO (F5): cuando un mes deja eventos sin resolver,
// el juego los pone en primer plano en vez de esperar a que el jugador abra
// GOBIERNO > POLITICA por azar. Muestra UN evento a la vez con sus opciones,
// coste, riesgo e impacto esperado, resuelve via UWLPoliticalSubsystem::ResolveEvent
// y avanza al siguiente de la cola. Misma estetica que la ventana de Gobierno
// (WLGovernmentWidgetShared.h); toda la logica vive en el subsystem.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "WLEventModalWidget.generated.h"

class UBorder;
class UScrollBox;
class UVerticalBox;
class UWLCampaignGameInstance;
class UWLPoliticalSubsystem;
class UWLEventModalWidget;

/** Boton con payload (evento+opcion) que reenvia al dispatcher del modal. */
UCLASS()
class WORLDLEADER_API UWLEventOptionButton : public UButton
{
	GENERATED_BODY()

public:
	void BindOption(UWLEventModalWidget* InOwner, const FString& InInstanceId, const FString& InOptionId);

private:
	UFUNCTION() void HandleClicked();

	UPROPERTY() TObjectPtr<UWLEventModalWidget> Owner = nullptr;
	FString InstanceId;
	FString OptionId;
};

UCLASS()
class WORLDLEADER_API UWLEventModalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Resuelve la opcion elegida (segundo clic si es sensible) y refresca al siguiente evento. */
	void ChooseOption(const FString& InstanceId, const FString& OptionId);

	/** True si el jugador tiene al menos un evento sin resolver (lo usa el PlayerController). */
	bool HasPendingEvents() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildShell();
	void Rebuild();

	UFUNCTION() void OnPostpone();

	FString PlayerIso() const;
	UWLCampaignGameInstance* GetCampaignGI() const;
	UWLPoliticalSubsystem* GetPolitical() const;

	UPROPERTY() UVerticalBox* ContentBox = nullptr;

	/** Opcion sensible (impacto grande) esperando el segundo clic de confirmacion. */
	FString PendingConfirmKey;

	FString LastResolutionMessage;
};
