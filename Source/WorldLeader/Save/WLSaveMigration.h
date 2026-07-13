// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UWLLocalSaveGame;

namespace WLSaveVersion
{
	inline constexpr int32 OldestMigratable = 16;
	inline constexpr int32 Current = 19;
}

/** Sequential, idempotent migrations for internal pre-production campaign saves. */
class WORLDLEADER_API FWLSaveMigration
{
public:
	static bool MigrateToCurrent(UWLLocalSaveGame& Save, FString& OutMessage);
};
