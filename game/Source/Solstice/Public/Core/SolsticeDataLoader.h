// Solstice - Core layer: runtime data loading.
//
// Contract: content is data driven (docs/TECH.md SS2.2 rule 4). Species, cards,
// world presets, style themes and localisation all arrive as JSON from
// Config/Solstice/{Data,Localization}/ and are parsed into plain Sim structs, so the kernel
// itself never has to know where data comes from.

#pragma once

#include "CoreMinimal.h"
#include "Sim/State/SolsticeTypes.h"

namespace Solstice
{
	struct SOLSTICE_API FLoadedContent
	{
		FCatalog Catalog;
		FWorldConfig DefaultWorld;
		TArray<FString> CardFiles;
		TArray<FString> Warnings;
		bool bLoaded = false;
	};

	class SOLSTICE_API FDataLoader
	{
	public:
		/**
		 * Loads every card file, the species file, world presets and localisation from the
		 * project Config directory. Never throws; problems accumulate in Out.Warnings so a
		 * half-authored data set still yields a playable realm.
		 */
		static bool LoadAll(FLoadedContent& Out);

		static bool LoadFileToString(const FString& RelativePath, FString& OutText);

		/** Config/Solstice/Data - the runtime contract location from docs/TECH.md SS6. */
		static FString GetDataDirectory();
		static FString GetLocalizationDirectory();
	};
}
