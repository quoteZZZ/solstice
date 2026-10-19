// Solstice - Core layer: bilingual string table (docs/WORLD.md SS7).
//
// Rules from the truth source: Chinese and English are stored as a pair, missing English
// falls back to Chinese, single entries stay short. UI chrome keys live in
// Config/Solstice/Localization/{zh-Hans,en}.json; card names and descriptions travel with the
// card data itself.

#pragma once

#include "CoreMinimal.h"

namespace Solstice
{
	class FLocalization
	{
	public:
		static FLocalization& Get()
		{
			static FLocalization Instance;
			return Instance;
		}

		void Reset()
		{
			Table.Empty();
			bChinese = true;
		}

		void Add(const FString& Key, const FString& Zh, const FString& En)
		{
			Table.Add(Key, TPair<FString, FString>(Zh, En));
		}

		/** Convenience for loading a flat {"key": "text"} json object for one language. */
		void AddFlat(const FString& Language, const TMap<FString, FString>& Entries)
		{
			const bool bZh = Language.StartsWith(TEXT("zh"));
			for (const TPair<FString, FString>& Pair : Entries)
			{
				TPair<FString, FString>* Existing = Table.Find(Pair.Key);
				if (Existing)
				{
					(bZh ? Existing->Key : Existing->Value) = Pair.Value;
				}
				else
				{
					Table.Add(Pair.Key, bZh ? TPair<FString, FString>(Pair.Value, FString()) : TPair<FString, FString>(FString(), Pair.Value));
				}
			}
		}

		bool IsChinese() const { return bChinese; }
		void SetChinese(bool bInChinese) { bChinese = bInChinese; }
		void ToggleLanguage() { bChinese = !bChinese; }

		/** Bilingual lookup. Falls back to the other language, then to the key itself. */
		FString Get(const FString& Key) const
		{
			const TPair<FString, FString>* Found = Table.Find(Key);
			if (!Found)
			{
				return Key;
			}
			const FString& Chosen = bChinese ? Found->Key : Found->Value;
			if (!Chosen.IsEmpty())
			{
				return Chosen;
			}
			return bChinese ? Found->Value : Found->Key;
		}

		/** Pick the active side of an inline pair, which is how most runtime text arrives. */
		FString Pick(const FString& Zh, const FString& En) const
		{
			if (bChinese)
			{
				return Zh.IsEmpty() ? En : Zh;
			}
			return En.IsEmpty() ? Zh : En;
		}

		int32 Num() const { return Table.Num(); }

	private:
		TMap<FString, TPair<FString, FString>> Table;
		bool bChinese = true;
	};

	/** Shorthand used all over the UI layer. */
	FORCEINLINE FString Loc(const FString& Key)
	{
		return FLocalization::Get().Get(Key);
	}

	FORCEINLINE FString Loc2(const FString& Zh, const FString& En)
	{
		return FLocalization::Get().Pick(Zh, En);
	}

	FORCEINLINE bool LocIsChinese()
	{
		return FLocalization::Get().IsChinese();
	}
}
