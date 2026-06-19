// Copyright World Leader project. See ROADMAP.md.

#include "Balance/WLBalanceSettings.h"

#if WITH_EDITOR
FText UWLBalanceSettings::GetSectionText() const
{
	return NSLOCTEXT("WorldLeader", "WorldLeaderBalanceSettingsName", "World Leader Balance");
}

FText UWLBalanceSettings::GetSectionDescription() const
{
	return NSLOCTEXT(
		"WorldLeader",
		"WorldLeaderBalanceSettingsDescription",
		"DataAsset activo para economia, calendario y reglas numericas de campania.");
}
#endif
