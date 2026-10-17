// Solstice - Core layer: the game instance.

#include "Core/SolsticeGameInstance.h"

#include "Core/SolsticeLocalization.h"
#include "Render/SolsticeCelMaterial.h"
#include "Solstice.h"

USolsticeGameInstance::USolsticeGameInstance()
{
}

void USolsticeGameInstance::Init()
{
	Super::Init();

	// ---- content ------------------------------------------------------
	Solstice::FLocalization::Get().Reset();
	const bool bContentOk = Solstice::FDataLoader::LoadAll(Loaded);

	for (const FString& Warning : Loaded.Warnings)
	{
		UE_LOG(LogSolstice, Warning, TEXT("content: %s"), *Warning);
	}

	// ---- reality anchor -----------------------------------------------
	Reality.Initialize(TEXT("shanghai"));
	if (Loaded.DefaultWorld.Latitude != 0.0f)
	{
		Reality.SetLatitude(Loaded.DefaultWorld.Latitude);
	}

	// ---- world --------------------------------------------------------
	Solstice::FWorldConfig Config = Loaded.DefaultWorld;

	// Development override: `-SolsticeMinute=<0..1439>` pins the world clock at startup, so the
	// sky can be checked at any hour without waiting for it to arrive (tools/run/shot.ps1 -Minute).
	// Without it the realm opens at the real clock, which is what a player gets.
	int32 PinnedMinute = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("SolsticeMinute="), PinnedMinute))
	{
		Config.StartingMinuteOfDay = FMath::Clamp(PinnedMinute, 0, 24 * 60 - 1);
		UE_LOG(LogSolstice, Log, TEXT("world clock pinned to minute %d of the day"), Config.StartingMinuteOfDay);
	}

	// `-SolsticeSeed=<n>` does the same job for the roll. `seed: 0` in world.json means "roll a
	// fresh world", which is right for play and useless for review: two shots of the same framing
	// were two different worlds, so no measurement could be compared against the one before it
	// (framing metrics moved 0.44 -> 0.37 with no code change - 2026-10-18). tools/run/shot.ps1
	// pins this, so a screenshot is reproducible.
	int32 PinnedSeed = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("SolsticeSeed="), PinnedSeed) && PinnedSeed > 0)
	{
		Config.Seed = static_cast<uint32>(PinnedSeed);
		UE_LOG(LogSolstice, Log, TEXT("world seed pinned to %u"), Config.Seed);
	}

	Sim.Initialize(Config, Loaded.Catalog);

	// The reference pace from docs/setting/SPECIES.md SS2.1: a healthy small realm runs a full
	// birth-predation-death-decomposition loop in 10-15 world days, which is about half an hour
	// of real time at three real minutes per world day. One-to-one is therefore the default.
	Sim.SetTimeMode(Solstice::ETimeMode::Real);

	bReady = bContentOk || Loaded.Catalog.Cards.Num() > 0;

	UE_LOG(LogSolstice, Log, TEXT("realm %s ready: %d cards from %d file(s), %d species, %d preset(s), %s"),
		*Config.RealmId, Loaded.Catalog.Cards.Num(), Loaded.CardFiles.Num(), Loaded.Catalog.Species.Num(),
		Loaded.Catalog.Presets.Num(), bReady ? TEXT("ok") : TEXT("DEGRADED"));
	UE_LOG(LogSolstice, Log, TEXT("reality anchor: %s, offline=%d, weather_from_net=%d"),
		*Reality.GetCityName(), Reality.IsOffline() ? 1 : 0, Reality.IsWeatherFromNetwork() ? 1 : 0);

	// The master material is generated on first launch. This runs before the SolsticeEditor
	// module (PostEngineInit) has necessarily installed its builder, so this is only a first
	// attempt - FCelMaterials::Ensure re-attempts once the builder arrives.
	UE_LOG(LogSolstice, Log, TEXT("paper materials at Init: %s"),
		Solstice::FCelMaterials::Ensure() ? TEXT("authored") : TEXT("pending"));
}

void USolsticeGameInstance::Shutdown()
{
	Super::Shutdown();
}

void USolsticeGameInstance::TickSimulation(float DeltaSeconds)
{
	if (!bReady)
	{
		return;
	}
	Reality.Tick(DeltaSeconds, /*bAllowNetwork=*/true);
	Sim.FeedReality(Reality.GetSample());
	Sim.Advance(DeltaSeconds);
}

bool USolsticeGameInstance::ReshapeToPreset(int32 PresetIndex)
{
	if (!Loaded.Catalog.Presets.IsValidIndex(PresetIndex))
	{
		return false;
	}
	const Solstice::FWorldConfig Config = Loaded.Catalog.Presets[PresetIndex];
	Sim.Initialize(Config, Loaded.Catalog);
	return true;
}

bool USolsticeGameInstance::SowLifeSeed()
{
	if (!bReady)
	{
		return false;
	}
	Solstice::FWorldConfig Config = Sim.GetConfig();
	if (Config.bWithLifeSeed)
	{
		return false;
	}
	Config.bWithLifeSeed = true;
	// Same seed, same terrain - only life is added, so sowing never undoes the player's work.
	Sim.Initialize(Config, Loaded.Catalog);
	return true;
}

void USolsticeGameInstance::ToggleLanguage()
{
	Solstice::FLocalization::Get().ToggleLanguage();
}

bool USolsticeGameInstance::IsChinese() const
{
	return Solstice::FLocalization::Get().IsChinese();
}

const TCHAR* USolsticeGameInstance::LanguageCode() const
{
	return IsChinese() ? TEXT("zh-Hans") : TEXT("en");
}
