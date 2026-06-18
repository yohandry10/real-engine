// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLCampaignHUD.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

UWLDataRegistry* AWLCampaignHUD::GetRegistry() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLStrategicTickSubsystem* AWLCampaignHUD::GetTick() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}

void AWLCampaignHUD::DrawHUD()
{
	Super::DrawHUD();

	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick || !Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	float X = 60.f;
	float Y = 60.f;
	const float LineHeight = 24.f;

	DrawText(TEXT("WORLD LEADER  -  Campana estrategica"), FLinearColor::White, X, Y, Font, 1.3f);
	Y += LineHeight * 1.6f;

	DrawText(FString::Printf(TEXT("Fecha: %02d/%d"), Tick->GetCurrentMonth(), Tick->GetCurrentYear()),
		FLinearColor(0.75f, 0.9f, 1.0f), X, Y, Font);
	Y += LineHeight * 1.5f;

	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		const FString Line = FString::Printf(TEXT("%s (%s)    Tesoro: %lld    Balance/mes: %+lld"),
			*Nation.Name, *Nation.Iso,
			Tick->GetTreasury(Nation.Iso), Tick->GetMonthlyBalance(Nation.Iso));
		DrawText(Line, FLinearColor(1.0f, 0.85f, 0.2f), X, Y, Font);
		Y += LineHeight;
	}

	Y += LineHeight;
	DrawText(FString::Printf(TEXT("Provincias cargadas: %d    Naciones: %d"),
		Registry->GetProvinceCount(), Registry->GetNationCount()),
		FLinearColor(0.6f, 0.6f, 0.6f), X, Y, Font);
	Y += LineHeight * 1.5f;

	DrawText(TEXT("[M] Avanzar mes      [P] Imprimir estado al log"),
		FLinearColor(0.55f, 0.8f, 0.55f), X, Y, Font);
}
