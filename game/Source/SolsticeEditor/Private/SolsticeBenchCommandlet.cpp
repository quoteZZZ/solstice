// Solstice - editor module: the headless M0 regression.
//
// Determinism is the one thing the whole architecture rests on and the one thing that is
// invisible in a screenshot, so it gets its own commandlet. Same seed, same tick count, twice -
// if the two digests differ, something in Sim/ has picked up a dependency on wall time, on
// iteration order or on a shared RNG stream, and every save file from here on is suspect.

#include "SolsticeEditor.h"

#include "Core/SolsticeDataLoader.h"
#include "Sim/SolsticeKernel.h"
#include "Solstice.h"
#include "Sim/SolsticeWorldGen.h"

namespace
{
	/**
	 * FNV-1a 64 over a quantised structural summary of a realm.
	 *
	 * Floats are quantised to 1e-5 before hashing: a divergence we care about is a different
	 * world, not a different rounding of the same world, and hashing raw float bits would make
	 * the digest depend on the optimiser.
	 */
	struct FRealmDigest
	{
		uint64 Hash = 1469598103934665603ULL;
		int32 Mixed = 0;

		void Mix(uint64 Value)
		{
			Hash ^= Value;
			Hash *= 1099511628211ULL;
			++Mixed;
		}

		void MixFloat(float Value)
		{
			const int64 Quantised = static_cast<int64>(FMath::RoundToDouble(static_cast<double>(Value) * 100000.0));
			Mix(static_cast<uint64>(Quantised));
		}
	};

	/**
	 * Terrain census. Tallies the things a picture cannot tell you apart: how much of the realm is
	 * under water, how the heights are spread, and how long the shoreline is. Without it "there is
	 * no water in the frame" is indistinguishable from "water is there and the palette hides it",
	 * and the two have opposite fixes.
	 */
	struct FTerrainCensus
	{
		static constexpr int32 Bands = 8;		// 8 m per band, 0..64
		int32 BandCounts[Bands] = {};
		int32 WaterTiles = 0;
		int32 DiscTiles = 0;			// tiles inside the world disc (the census denominator)
		int32 OutsideDisc = 0;
		int32 ShoreTiles = 0;					// water tile with at least one land neighbour
		/** Chaos / Water / Grass / Woodland / RockSlope. A dead world is usually a biome census problem, not an ecology one. */
		int32 BiomeCounts[5] = {};
		float MinTempC = 999.0f;
		float MaxTempC = -999.0f;

		float MinHeight = 0.0f;
		float MaxHeight = 0.0f;
	};

	struct FBenchRun
	{
		FRealmDigest Digest;
		int64 WorldMinutes = 0;
		int32 PopulationTotal = 0;
		int32 PopulationByKind[static_cast<int32>(Solstice::ESpeciesKind::Count)] = {};
		int32 Individuals = 0;
		int32 ChronicleEntries = 0;
		int32 DeathMarks = 0;
		int32 DeathByCause[7] = {};
		int32 SeededPopulation = 0;
		float PaintedFraction = 0.0f;
		int32 Echo = 0;
		double Seconds = 0.0;
		FTerrainCensus Census;
		Solstice::WorldGen::FRoll Roll;
		float TerrainPeak = 0.0f;
	};

	/** One full headless run. Nothing outside Sim/ is touched. */
	FBenchRun RunOnce(const Solstice::FWorldConfig& Config, const Solstice::FCatalog& Catalog, int64 WorldMinutes)
	{
		FBenchRun Run;

		Solstice::FSimKernel Kernel;
		Kernel.Initialize(Config, Catalog);

		// Seeding diagnostics: how many creatures the world opens with, and what it is made of.
		// "population 2 after two days" was indistinguishable from "seeding never placed anything"
		// until this line existed.
		{
			int32 BiomeCounts[5] = {};

			for (int32 Y = 0; Y < Config.Size; ++Y)
			{
				for (int32 X = 0; X < Config.Size; ++X)
				{
					if (const Solstice::FTile* Tile = Kernel.TileAt(X, Y))
					{
						const int32 B = static_cast<int32>(Tile->GetBiome());
						if (B >= 0 && B < 5) { ++BiomeCounts[B]; }
					}
				}
			}
			Run.SeededPopulation = Kernel.GetIndividuals().Num();
			UE_LOG(LogSolstice, Display,
				TEXT("bench: seeded pop %d (unrest %d / water %d / grass %d / woodland %d / rock %d)"),
				Kernel.GetIndividuals().Num(), BiomeCounts[0], BiomeCounts[1], BiomeCounts[2],
				BiomeCounts[3], BiomeCounts[4]);
		}

		// Virtual time, speed index 2 = the 4x preset. Derive the real-seconds-per-world-minute
		// from the kernel itself rather than hard-coding it, so this keeps working if the pace
		// tuning (docs/LATER.md SS2.1) ever moves.
		Kernel.SetTimeMode(Solstice::ETimeMode::Virtual);
		Kernel.SetSpeedIndex(2);
		Kernel.SetPaused(false);

		const double MinutesPerWorldDay = FMath::Max(0.5, static_cast<double>(Kernel.GetRealMinutesPerWorldDay()));
		const double Multipliers[4] = { 0.0, 1.0, 4.0, 20.0 };
		const double WorldMinutesPerRealSecond = (1440.0 / MinutesPerWorldDay / 60.0) * Multipliers[Kernel.GetSpeedIndex()];
		const double SecondsPerWorldMinute = (WorldMinutesPerRealSecond > 0.0) ? (1.0 / WorldMinutesPerRealSecond) : 0.25;

		const double StartSeconds = FPlatformTime::Seconds();
		for (int64 Minute = 0; Minute < WorldMinutes; ++Minute)
		{
			Kernel.Advance(SecondsPerWorldMinute);
		}
		Run.Seconds = FPlatformTime::Seconds() - StartSeconds;

		// ---- digest, in ascending index order (never a TMap, never a pointer) ----
		Run.Digest.Mix(Config.Seed);
		Run.Digest.Mix(static_cast<uint64>(Config.Size));
		Run.Digest.Mix(static_cast<uint64>(Config.Algo));

		for (const Solstice::FTile& Tile : Kernel.GetTiles())
		{
			Run.Digest.MixFloat(Tile.Height);
			Run.Digest.MixFloat(Tile.Vegetation);
			Run.Digest.MixFloat(Tile.Moisture);
			Run.Digest.MixFloat(Tile.Fertility);
			Run.Digest.MixFloat(Tile.Painted);
			Run.Digest.MixFloat(Tile.Ash);
			Run.Digest.Mix(Tile.Mat);
			Run.Digest.Mix(Tile.Biome);
			Run.Digest.Mix(Tile.WaterBody);
		}

		for (int32 Kind = 0; Kind < static_cast<int32>(Solstice::ESpeciesKind::Count); ++Kind)
		{
			Run.PopulationByKind[Kind] = Kernel.GetPopulation(static_cast<Solstice::ESpeciesKind>(Kind));
			Run.Digest.Mix(static_cast<uint64>(Run.PopulationByKind[Kind]));
		}

		Run.WorldMinutes = Kernel.GetWorldMinute();
		Run.PopulationTotal = Kernel.GetPopulationTotal();
		Run.Individuals = Kernel.GetIndividualCount();
		Run.ChronicleEntries = Kernel.GetChronicle().Num();
		Run.DeathMarks = Kernel.GetDeathMarks().Num();
		for (int32 Cause = 0; Cause < 7; ++Cause)
		{
			Run.DeathByCause[Cause] = Kernel.GetDeathsByCause(static_cast<Solstice::EDeathCause>(Cause));
		}
		Run.PaintedFraction = Kernel.GetPaintedFraction();
		Run.Roll = Kernel.GetWorldRoll();
		Run.TerrainPeak = Kernel.GetTerrainPeakMeters();
		Run.Echo = Kernel.GetEcho();

		// ---- terrain census ------------------------------------------------
		{
			const int32 Size = Config.Size;
			const float Half = static_cast<float>(Size) * 0.5f;
			Run.Census.MinHeight = TNumericLimits<float>::Max();
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					const Solstice::FTile* Tile = Kernel.TileAt(X, Y);
					if (!Tile)
					{
						continue;
					}
					// The disc, not the square: the four corners are r > 1 and are always sea.
					// Counting them diluted the sea fraction by ~20 points and made "target 45%"
					// read as "62%", which is the prototype's own note about area accounting.
					const float DX = static_cast<float>(X) - Half;
					const float DY = static_cast<float>(Y) - Half;
					if (FMath::Sqrt(DX * DX + DY * DY) > Half * 0.985f)
					{
						++Run.Census.OutsideDisc;
						continue;
					}
					++Run.Census.DiscTiles;
					Run.Census.MinTempC = FMath::Min(Run.Census.MinTempC, Tile->LocalTempC);
					Run.Census.MaxTempC = FMath::Max(Run.Census.MaxTempC, Tile->LocalTempC);
					Run.Census.MinHeight = FMath::Min(Run.Census.MinHeight, Tile->Height);
					Run.Census.MaxHeight = FMath::Max(Run.Census.MaxHeight, Tile->Height);
					const int32 Band = FMath::Clamp(
						FMath::FloorToInt(Tile->Height / 64.0f * static_cast<float>(FTerrainCensus::Bands)),
						0, FTerrainCensus::Bands - 1);
					++Run.Census.BandCounts[Band];
					if (Tile->IsWater())
					{
						++Run.Census.WaterTiles;
					}
					Run.Census.MinTempC = FMath::Min(Run.Census.MinTempC, Tile->LocalTempC);
					Run.Census.MaxTempC = FMath::Max(Run.Census.MaxTempC, Tile->LocalTempC);
					{
						const int32 BiomeIndex = static_cast<int32>(Tile->GetBiome());
						if (BiomeIndex >= 0 && BiomeIndex < 5)
						{
							++Run.Census.BiomeCounts[BiomeIndex];
						}
					}
				}
			}
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					const Solstice::FTile* Tile = Kernel.TileAt(X, Y);
					if (!Tile || !Tile->IsWater())
					{
						continue;
					}
					const Solstice::FTile* N = Kernel.TileAt(X + 1, Y);
					const Solstice::FTile* S = Kernel.TileAt(X - 1, Y);
					const Solstice::FTile* E = Kernel.TileAt(X, Y + 1);
					const Solstice::FTile* W = Kernel.TileAt(X, Y - 1);
					const bool bLandNextDoor =
						(N && !N->IsWater()) || (S && !S->IsWater()) ||
						(E && !E->IsWater()) || (W && !W->IsWater());
					if (bLandNextDoor)
					{
						++Run.Census.ShoreTiles;
					}
				}
			}
		}

		Run.Digest.Mix(static_cast<uint64>(Run.WorldMinutes));
		Run.Digest.Mix(static_cast<uint64>(Run.PopulationTotal));
		Run.Digest.Mix(static_cast<uint64>(Run.Individuals));
		Run.Digest.Mix(static_cast<uint64>(Run.ChronicleEntries));
		Run.Digest.Mix(static_cast<uint64>(Run.Echo));
		Run.Digest.MixFloat(Run.PaintedFraction);
		Run.Digest.MixFloat(Kernel.GetVegetationMean());
		Run.Digest.MixFloat(Kernel.GetFertilityMean());
		Run.Digest.MixFloat(Kernel.GetDiversityIndex());

		return Run;
	}
}

USolsticeBenchCommandlet::USolsticeBenchCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 USolsticeBenchCommandlet::Main(const FString& Params)
{
	Solstice::FLoadedContent Content;
	if (!Solstice::FDataLoader::LoadAll(Content) || Content.Catalog.Cards.Num() == 0)
	{
		UE_LOG(LogSolstice, Error, TEXT("bench: content did not load; cannot run"));
		return 1;
	}

	int32 Days = 1;
	int32 SizeOverride = 0;
	int32 SeedOverride = 0;
	int32 Life = 1;
	FParse::Value(*Params, TEXT("Days="), Days);
	FParse::Value(*Params, TEXT("Size="), SizeOverride);
	FParse::Value(*Params, TEXT("Seed="), SeedOverride);
	FParse::Value(*Params, TEXT("Life="), Life);

	Solstice::FWorldConfig Config = Content.DefaultWorld;
	if (SizeOverride > 0)
	{
		Config.Size = SizeOverride;
		Config.ChunkSize = FMath::Max(8, SizeOverride / 2);
	}
	if (SeedOverride > 0)
	{
		Config.Seed = static_cast<uint32>(SeedOverride);
	}

	// world.json ships seed 0 = "roll a fresh world per launch", which is right for play and
	// poison for a regression: the two runs would roll two different worlds and the digest would
	// always differ. Pin the historical default seed when nobody asked for one.
	constexpr uint32 BenchSeed = 20261016u;
	if (Config.Seed == 0u)
	{
		Config.Seed = BenchSeed;
	}

	// Same reason, second half: the realm derives its opening day-of-year and minute-of-day from
	// the wall clock. A digest that changes with the time of day is not a digest, so both are
	// pinned here - the run has to be reproducible tomorrow.
	if (Config.StartingDayOfYear < 0)
	{
		Config.StartingDayOfYear = 80;		// 21 March: the equinox, i.e. the middle of the year
	}
	Config.StartingMinuteOfDay = 6 * 60 + 30;

	// Seed life by default. The default preset is deliberately a bare chaos start ("no life,
	// uncoloured paper"), but a regression that never spawns a creature would pass a digest
	// comparison while proving nothing about the ecology - and the ecology is the game. Pass
	// -Life=0 to bench the untouched chaos start instead.
	Config.bWithLifeSeed = (Life != 0);

	const int64 WorldMinutes = FMath::Max<int64>(1, static_cast<int64>(Days) * 1440);
	const int32 Tiles = Config.Size * Config.Size;

	UE_LOG(LogSolstice, Display, TEXT("bench: realm %d x %d (%d tiles), seed %u, %lld world minutes, life_seed=%d, 2 runs"),
		Config.Size, Config.Size, Tiles, Config.Seed, WorldMinutes, Config.bWithLifeSeed ? 1 : 0);

	const FBenchRun A = RunOnce(Config, Content.Catalog, WorldMinutes);
	const FBenchRun B = RunOnce(Config, Content.Catalog, WorldMinutes);

	const double PerSecond = (A.Seconds > 0.0) ? (static_cast<double>(A.WorldMinutes) / A.Seconds) : 0.0;

	UE_LOG(LogSolstice, Display,
		TEXT("bench: run A %lld world minutes in %.2fs (%.0f world-min/s), population %d, individuals %d, chronicle %d, deaths %d, painted %.1f%%, echo %d"),
		A.WorldMinutes, A.Seconds, PerSecond, A.PopulationTotal, A.Individuals,
		A.ChronicleEntries, A.DeathMarks, A.PaintedFraction * 100.0f, A.Echo);
	UE_LOG(LogSolstice, Display,
		TEXT("bench: run B %lld world minutes in %.2fs, population %d, individuals %d, chronicle %d, deaths %d, painted %.1f%%, echo %d"),
		B.WorldMinutes, B.Seconds, B.PopulationTotal, B.Individuals,
		B.ChronicleEntries, B.DeathMarks, B.PaintedFraction * 100.0f, B.Echo);
	UE_LOG(LogSolstice, Display,
		TEXT("bench: deaths starved %d / predated %d / frozen %d / drowned %d / aged %d / desiccated %d"),
		A.DeathByCause[1], A.DeathByCause[2], A.DeathByCause[3],
		A.DeathByCause[4], A.DeathByCause[5], A.DeathByCause[6]);
	UE_LOG(LogSolstice, Display,
		TEXT("bench: species A moss %d, shrub %d, grazer %d, raptor %d, fish %d"),
		A.PopulationByKind[0], A.PopulationByKind[1], A.PopulationByKind[2], A.PopulationByKind[3], A.PopulationByKind[4]);
	UE_LOG(LogSolstice, Display, TEXT("bench: digest A %016llX (%d mixed) / digest B %016llX (%d mixed)"),
		A.Digest.Hash, A.Digest.Mixed, B.Digest.Hash, B.Digest.Mixed);

	// Terrain census, run A. Percent is of all tiles; the bands are 8 m each from 0 to 64.
	{
		const float Total = static_cast<float>(FMath::Max(1, A.Census.DiscTiles));
		UE_LOG(LogSolstice, Display,
			TEXT("bench: terrain height %.1f..%.1f m, water %.1f%% (%d tiles), shore %d tiles"),
			A.Census.MinHeight, A.Census.MaxHeight,
			static_cast<float>(A.Census.WaterTiles) / Total * 100.0f, A.Census.WaterTiles,
			A.Census.ShoreTiles);
		// The roll itself, so a frame can be traced back to the world it came from: archetype and
		// climate decide the coastline and the province pool, and `sea target` is what the
		// calibration solved the coastline radius for.
		{
			const Solstice::WorldGen::FRoll& Roll = A.Roll;
			static const TCHAR* ArchetypeNames[] = {
				TEXT("continent-coast"), TEXT("inland-plateau"), TEXT("mountain-belt"), TEXT("archipelago"),
				TEXT("volcanic-isles"), TEXT("arid-mesa"), TEXT("continent")
			};
			static const TCHAR* ClimateNames[] = {
				TEXT("temperate"), TEXT("steppe"), TEXT("desert"), TEXT("rainforest"), TEXT("tundra"), TEXT("cold-desert")
			};
			UE_LOG(LogSolstice, Display,
				TEXT("bench: roll %s / %s, sea target %.0f%% (landR %.2f), relief %.2f, mountain %.0f%%, %d provinces, peak %.1f m"),
				ArchetypeNames[FMath::Clamp(Roll.Archetype, 0, 6)],
				ClimateNames[FMath::Clamp(static_cast<int32>(Roll.Climate), 0, 5)],
				Roll.Sea * 100.0f, Roll.LandR, Roll.Relief, Roll.Mountain * 100.0f,
				Roll.Provinces.Num(), A.TerrainPeak);
		}
		FString Bands;
		UE_LOG(LogSolstice, Display, TEXT("bench: local temp %.1f .. %.1f C"),
			A.Census.MinTempC, A.Census.MaxTempC);
		UE_LOG(LogSolstice, Display,
			TEXT("bench: biomes unrest %d / water %d / grass %d / woodland %d / rock %d"),
			A.Census.BiomeCounts[0], A.Census.BiomeCounts[1], A.Census.BiomeCounts[2],
			A.Census.BiomeCounts[3], A.Census.BiomeCounts[4]);
		for (int32 Band = 0; Band < FTerrainCensus::Bands; ++Band)
		{
			Bands += FString::Printf(TEXT("%s%d-%d:%.1f%%"), Band > 0 ? TEXT(" ") : TEXT(""),
				Band * 8, Band * 8 + 8, static_cast<float>(A.Census.BandCounts[Band]) / Total * 100.0f);
		}
		UE_LOG(LogSolstice, Display, TEXT("bench: height bands %s"), *Bands);
	}

	if (A.Digest.Hash != B.Digest.Hash)
	{
		UE_LOG(LogSolstice, Error, TEXT("bench: NON-DETERMINISTIC - the two runs diverged"));
		return 1;
	}

	// A digest match on a realm where nothing happened would be a vacuous pass, so require the
	// run to have actually produced a world.
	if (A.PopulationTotal <= 0 || A.ChronicleEntries <= 0)
	{
		UE_LOG(LogSolstice, Error,
			TEXT("bench: INERT - %lld minutes produced population %d / chronicle %d, so the digest proves nothing"),
			A.WorldMinutes, A.PopulationTotal, A.ChronicleEntries);
		return 1;
	}

	UE_LOG(LogSolstice, Display, TEXT("bench: ok - deterministic and alive"));
	return 0;
}
