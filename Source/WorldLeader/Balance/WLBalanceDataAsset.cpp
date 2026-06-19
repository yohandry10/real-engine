// Copyright World Leader project. See ROADMAP.md.

#include "Balance/WLBalanceDataAsset.h"

FWLBalanceRules UWLBalanceDataAsset::GetSanitizedRules() const
{
	return Rules.Sanitized();
}

FWLBalanceRules UWLBalanceDataAsset::GetDefaultRules()
{
	return FWLBalanceRules::Default();
}
