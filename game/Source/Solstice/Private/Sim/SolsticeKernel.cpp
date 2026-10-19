// Solstice - Sim layer: world kernel implementation.
//
// Tick model: Advance() converts real seconds into whole world minutes and runs
// StepWorldMinute() once per world minute (docs/TECH.md SS2.2.3 "fixed logic
// tick = 1 world minute"). Expensive subsystems run at a lower cadence (SimLOD) but always
// on the same discrete boundary, so a given seed and a given wall of real seconds always
// produce the same world.

#include "Sim/SolsticeKernel.h"

namespace Solstice
{
	namespace
	{
		/** Position-hashed integer noise: draw order can never change the result. */
		FORCEINLINE uint32 HashCoord(int32 X, int32 Y, uint32 Seed)
		{
			uint32 H = static_cast<uint32>(X) * 0x8DA6B343u
				+ static_cast<uint32>(Y) * 0xD8163841u
				+ Seed * 0xCB1AB31Fu;
			H ^= H >> 13;
			H *= 0x7FEB352Du;
			H ^= H >> 15;
			H *= 0x846CA68Bu;
			H ^= H >> 16;
			return H;
		}

		FORCEINLINE float Hash01(int32 X, int32 Y, uint32 Seed)
		{
			return static_cast<float>(HashCoord(X, Y, Seed) & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
		}

		FORCEINLINE float ValueNoise(float X, float Y, uint32 Seed)
		{
			const int32 X0 = FMath::FloorToInt(X);
			const int32 Y0 = FMath::FloorToInt(Y);
			const float FX = X - static_cast<float>(X0);
			const float FY = Y - static_cast<float>(Y0);
			const float SX = FX * FX * (3.0f - 2.0f * FX);
			const float SY = FY * FY * (3.0f - 2.0f * FY);
			const float A = Hash01(X0, Y0, Seed);
			const float B = Hash01(X0 + 1, Y0, Seed);
			const float C = Hash01(X0, Y0 + 1, Seed);
			const float D = Hash01(X0 + 1, Y0 + 1, Seed);
			return FMath::Lerp(FMath::Lerp(A, B, SX), FMath::Lerp(C, D, SX), SY);
		}

		FORCEINLINE float Fbm(float X, float Y, uint32 Seed, int32 Octaves)
		{
			float Sum = 0.0f;
			float Amp = 1.0f;
			float Norm = 0.0f;
			float FX = X;
			float FY = Y;
			for (int32 Octave = 0; Octave < Octaves; ++Octave)
			{
				Sum += Amp * ValueNoise(FX, FY, Seed + static_cast<uint32>(Octave) * 7919u);
				Norm += Amp;
				Amp *= 0.5f;
				FX *= 2.0f;
				FY *= 2.0f;
			}
			return Norm > 0.0f ? Sum / Norm : 0.0f;
		}

		// ------------------------------------------------------------------
		// Terrain noise parameters (docs/WORLD.md SS2.2)
		// One group per generator branch in GenerateTerrain(). These are implementation
		// constants of the noise itself - they live here, named, instead of inline in the
		// switch, so a shape tune is a single-file read.
		// ------------------------------------------------------------------
		namespace TerrainNoise
		{
			/** Seed offsets that keep each derived field independent of the base field. */
			constexpr uint32 RidgeDetailSeed = 31u;
			constexpr uint32 FertilitySeed = 77u;
			constexpr uint32 MoistureSeed = 131u;

			// ridges: 山脊
			constexpr float RidgeFreq = 3.0f;
			constexpr int32 RidgeOctaves = 5;
			constexpr float RidgeDetailFreq = 9.0f;
			constexpr int32 RidgeDetailOctaves = 3;
			constexpr float RidgeMainWeight = 0.85f;
			constexpr float RidgeDetailWeight = 0.15f;

			// islands: 岛屿
			constexpr float IslandFreq = 2.6f;
			constexpr int32 IslandOctaves = 4;
			constexpr float IslandFalloffExponent = 2.2f;
			constexpr float IslandScale = 1.35f;

			// canyon: 峡谷
			constexpr float CanyonFreq = 2.2f;
			constexpr int32 CanyonOctaves = 4;
			constexpr float CanyonCutScale = 2.0f;
			constexpr float CanyonScale = 0.9f;
			constexpr float CanyonBias = 0.35f;
			constexpr float CanyonCutWeight = 0.55f;

			// plateau: 台地
			constexpr float PlateauFreq = 1.8f;
			constexpr int32 PlateauOctaves = 3;
			constexpr float PlateauThreshold = 0.48f;
			constexpr float PlateauHighBase = 0.78f;
			constexpr float PlateauHighGain = 0.4f;
			constexpr float PlateauLowBase = 0.18f;
			constexpr float PlateauLowGain = 0.3f;

			// basin: 盆地
			constexpr float BasinFreq = 2.4f;
			constexpr int32 BasinOctaves = 4;
			constexpr float BasinScale = 0.75f;
			constexpr float BasinRadialWeight = 0.55f;
			constexpr float BasinBias = 0.12f;

			// archipelago: 群岛
			constexpr float ArchipelagoFreq = 4.2f;
			constexpr int32 ArchipelagoOctaves = 5;
			constexpr float ArchipelagoRadialWeight = 0.75f;
			constexpr float ArchipelagoScale = 1.5f;
			constexpr float ArchipelagoBias = 0.45f;
			constexpr float ArchipelagoFalloffScale = 1.6f;

			// shared derived fields
			constexpr float FertilityFreq = 5.0f;
			constexpr int32 FertilityOctaves = 3;
			constexpr float MoistureFreq = 4.0f;
			constexpr int32 MoistureOctaves = 3;
		}

		/** Material dictionary, docs/WORLD.md SS2.1 table A + table B albedo. */
		struct FMaterialProps
		{
			float Density;
			float Hardness;
			float Combustibility;
			float IgniteTempC;
			float HeatCapacity;
			float Conductivity;
			float Permeability;
			float Albedo;
		};

		const FMaterialProps GMaterialProps[static_cast<int32>(EMaterial::Count)] =
		{
			/* Cel     */ { 700.0f, 0.20f, 0.90f, 230.0f, 4.0f, 0.10f, 0.10f, 0.70f },
			/* Soil      */ { 1600.0f, 0.30f, 0.00f, 9999.0f, 3.0f, 0.20f, 0.50f, 0.30f },
			/* Clay      */ { 1800.0f, 0.40f, 0.00f, 9999.0f, 3.0f, 0.20f, 0.20f, 0.35f },
			/* Mud       */ { 1700.0f, 0.10f, 0.00f, 9999.0f, 4.0f, 0.20f, 0.90f, 0.12f },
			/* Stone     */ { 2500.0f, 0.90f, 0.00f, 9999.0f, 2.0f, 0.40f, 0.05f, 0.30f },
			/* Ore       */ { 4000.0f, 0.80f, 0.00f, 9999.0f, 1.0f, 0.95f, 0.00f, 0.60f },
			/* DeadWood  */ { 500.0f, 0.50f, 0.95f, 260.0f, 3.0f, 0.10f, 0.30f, 0.50f },
			/* LivePlant */ { 800.0f, 0.30f, 0.60f, 300.0f, 5.0f, 0.10f, 0.80f, 0.40f },
			/* Water     */ { 1000.0f, 0.00f, 0.00f, 9999.0f, 10.0f, 0.60f, 1.00f, 0.10f },
			/* Magma     */ { 2700.0f, 0.00f, 0.00f, 9999.0f, 4.0f, 0.80f, 0.00f, 0.05f },
			/* Ice       */ { 917.0f, 0.40f, 0.00f, 9999.0f, 9.0f, 0.50f, 0.90f, 0.70f },
			/* Snow      */ { 300.0f, 0.10f, 0.00f, 9999.0f, 9.0f, 0.30f, 0.70f, 0.85f },
			/* Sand      */ { 1600.0f, 0.00f, 0.00f, 9999.0f, 2.0f, 0.30f, 0.60f, 0.45f },
			/* Ash       */ { 400.0f, 0.05f, 0.00f, 9999.0f, 2.0f, 0.20f, 0.80f, 0.15f },
			/* Tissue    */ { 1000.0f, 0.20f, 0.30f, 400.0f, 8.0f, 0.30f, 0.70f, 0.30f }
		};

		FORCEINLINE const FMaterialProps& PropsOf(EMaterial Mat)
		{
			const int32 Index = FMath::Clamp(static_cast<int32>(Mat), 0, static_cast<int32>(EMaterial::Count) - 1);
			return GMaterialProps[Index];
		}

		/** Month/day of the two solstices and two equinoxes; used to gate the special pack. */
		FORCEINLINE bool IsNearSolsticeOrEquinox(int32 Month, int32 Day)
		{
			static const int32 Keys[4][2] = { { 3, 20 }, { 6, 21 }, { 9, 23 }, { 12, 21 } };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				if (Keys[Index][0] == Month && FMath::Abs(Day - Keys[Index][1]) <= 3)
				{
					return true;
				}
			}
			return false;
		}

		FORCEINLINE int32 DaysInMonth(int32 Year, int32 Month)
		{
			static const int32 Table[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
			if (Month < 1 || Month > 12)
			{
				return 31;
			}
			if (Month == 2 && ((Year % 4 == 0 && Year % 100 != 0) || Year % 400 == 0))
			{
				return 29;
			}
			return Table[Month - 1];
		}
	}

	// ======================================================================
	// Construction
	// ======================================================================

	FSimKernel::FSimKernel() = default;
	FSimKernel::~FSimKernel() = default;

	void FSimKernel::Initialize(const FWorldConfig& InConfig, const FCatalog& InCatalog)
	{
		Config = InConfig;
		Catalog = InCatalog;
		Config.Size = FMath::Max(16, Config.Size);
		Config.ChunkSize = FMath::Clamp(Config.ChunkSize, 8, Config.Size);
		ChunksPerAxis = FMath::Max(1, (Config.Size + Config.ChunkSize - 1) / Config.ChunkSize);

		// Seed 0 means "no fixed seed": draw one per realm, the way a new world in a sandbox game
		// does. world.json ships 0, so every fresh realm is a different place; pin a number there
		// (or in Config.Seed) when a specific world has to be reproduced.
		if (Config.Seed == 0u)
		{
			Config.Seed = static_cast<uint32>(FDateTime::Now().GetTicks() & 0xFFFFFFFFull);
			if (Config.Seed == 0u)
			{
				Config.Seed = 1u;
			}
		}
		Rng.SeedFrom(Config.Seed);

		const int32 TileCount = Config.Size * Config.Size;
		Tiles.SetNum(TileCount);

		const int32 ChunkCount = ChunksPerAxis * ChunksPerAxis;
		RegionDirty.Init(1, ChunkCount);
		RegionBuilt.Init(0, ChunkCount);

		// --- time origin -------------------------------------------------
		if (Config.StartingDayOfYear >= 0)
		{
			StartDayOfYear = Config.StartingDayOfYear;
		}
		else
		{
			// Day of year from the real date: the world starts on the real calendar.
			const int32 Month = FMath::Clamp(Reality.Month, 1, 12);
			int32 DayOfYear = FMath::Clamp(Reality.Day, 1, 31) - 1;
			for (int32 M = 1; M < Month; ++M)
			{
				DayOfYear += DaysInMonth(Reality.Year, M);
			}
			StartDayOfYear = DayOfYear;
		}

		// Start at the real clock's time of day, not at midnight. WorldMinute drives the sun, the
		// sky and every solar term, and the world runs 1:1 with the system clock - so starting it
		// at zero meant every new realm opened at 00:00, sun below the horizon, the whole sheet
		// multiplied down into night tints. That was the single largest reason a fresh world read
		// as grey. The date was already taken from the real calendar above; take the hour too.
		//
		// A pinned minute skips all of it. The headless bench pins one because a digest that
		// depends on when the process started is not a digest.
		if (Config.StartingMinuteOfDay >= 0)
		{
			WorldMinute = static_cast<int64>(Config.StartingMinuteOfDay);
		}
		else
		{
			const FDateTime RealNow = FDateTime::Now();
			int32 MinuteOfDay = RealNow.GetHour() * 60 + RealNow.GetMinute();

			// ...but a fresh realm should not open in the dark, and "dark" starts much earlier
			// than the clock suggests. The window used to be 06:30-20:30, which allowed 20:00 -
			// a time this sky model puts 30 degrees *below* the horizon, i.e. fully night. A
			// player who opened the game at nine in the evening got a black sea, a silhouette
			// for an island, and concluded the renderer was broken (2026-10-18, exactly that).
			//
			// 17:30 is the last minute with the sun still low and warm; 08:30 is the first minute
			// that reads as morning rather than as dawn. Outside that band the world opens at
			// 08:30 and runs forward; the date is still the real one.
			//
			// The band is 09:30-16:00 for now, not 08:30-17:30. Measured at 08:30 on a clear
			// day: the sky's horizon band and the sea that mirrors it both washed out to white -
			// the low-sun palette has not been calibrated since the ground became lit, and until
			// it is, opening a fresh world there would be a worse first impression than a plain
			// high sun. Widen this once the morning and evening grades are re-measured.
			constexpr int32 DawnMinute = 9 * 60 + 30;
			constexpr int32 DuskMinute = 16 * 60;
			if (MinuteOfDay < DawnMinute || MinuteOfDay > DuskMinute)
			{
				MinuteOfDay = DawnMinute;
			}
			WorldMinute = static_cast<int64>(MinuteOfDay);
		}
		DayReleaseCount.Init(0, Catalog.Cards.Num());

		// --- sky initial values -----------------------------------------
		Sky.WaterLevel = Config.BaseWaterLevel;
		Sky.AirTempC = Reality.bValid ? Reality.TemperatureC : Tuning::BaseAirTempC;
		Sky.Humidity = Reality.bValid
			? FMath::Clamp(Reality.Humidity01, 0.05f, 0.95f) : Tuning::DefaultHumidity01;
		Sky.CloudCover = Reality.bValid
			? FMath::Clamp(Reality.CloudCover01, 0.0f, 1.0f) : Tuning::DefaultCloudCover01;
		// Paint the sky from the clock we just picked instead of leaving the sun at the struct
		// default until the first world minute ticks (see UpdateSolar).
		UpdateSolar();

		// --- world -------------------------------------------------------
		GenerateTerrain();
		ClassifyBiomes();
		InitPoolAndUnlocks();
		ResetHand();

		// 混沌初态: no life, uncoloured paper (docs/WORLD.md SS3.3 step 5).
		bChaosState = true;
		if (Config.bWithLifeSeed)
		{
			SeedLifeFromBiomes();
			bChaosState = (Individuals.Num() == 0);
		}

		// --- genesis entry ----------------------------------------------
		if (!bGenesisWritten)
		{
			FChronicleEntry Entry;
			Entry.WorldYear = 1;
			Entry.WorldDay = 1;
			Entry.MinuteOfDay = 0;
			Entry.RealYear = Reality.Year;
			Entry.RealMonth = Reality.Month;
			Entry.RealDay = Reality.Day;
			Entry.Type = static_cast<uint8>(EEventType::Genesis);
			Entry.SubjectZh = Config.RealmNameZh;
			Entry.SubjectEn = Config.RealmNameEn;
			Entry.SummaryZh = FString::Printf(TEXT("创世：种子 %u，%d × %d，未上色的纸"), Config.Seed, Config.Size, Config.Size);
			Entry.SummaryEn = FString::Printf(TEXT("Genesis: seed %u, %dx%d, unpainted paper"), Config.Seed, Config.Size, Config.Size);
			PushChronicle(Entry);
			bGenesisWritten = true;
		}

		bStructureDirty = true;
		MarkAllDirty();
	}

	// ======================================================================
	// Terrain
	// ======================================================================

	void FSimKernel::GenerateTerrain()
	{
		const int32 Size = Config.Size;
		const float InvSize = 1.0f / static_cast<float>(Size);

		// The world is rolled, then the field is turned into metres. Both halves live in
		// Sim/SolsticeWorldGen.* and are a port of studio/previews/world-model.html - the coastline,
		// the province blend, the interior rise, the tilt and the orogenic belt all come from
		// there, with the prototype's own numbers.
		//
		// `Config.Algo` no longer selects a noise function: the archetype is part of the roll
		// (seed in, same world out). world.json still carries the old `algo` strings and they are
		// inert - the presets need re-authoring around `archetype` before they mean anything again.
		WorldRoll = WorldGen::Roll(Config.Seed);

		// The land budget is a design input, not a side effect of the roll: the archetype brings
		// its own sea range, but `land_ratio` says how much of this realm should stand above
		// water, and the generator solves its coastline radius to hit it. Without this the sea was
		// whatever the noise produced - 77% on the default seed, which is not a diorama.
		WorldRoll.Sea = FMath::Clamp(1.0f - Config.LandRatio, 0.10f, 0.88f);
		WorldGen::CalibrateLand(WorldRoll);

		TerrainPeakMeters = FMath::Max(4.0f, WorldGen::PeakMetres(WorldRoll, Size));

		TArray<float> Heights;
		WorldGen::ResolveHeights(WorldRoll, Size, Config.BaseWaterLevel, Heights);

		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 Index = ToIndex(X, Y);
				const float U = static_cast<float>(X) * InvSize;
				const float V = static_cast<float>(Y) * InvSize;

				FTile& Tile = Tiles[Index];
				Tile = FTile();

				// Heights are metres above the plinth, and the sea floor is *negative*: the water
				// line is Config.BaseWaterLevel, so `Height < BaseWaterLevel` is water. The old
				// clamp to >= 0 flattened every sea bed to one depth and took the shelf with it.
				const float Height = FMath::RoundToFloat(Heights[Index] / Tuning::HeightQuantum) * Tuning::HeightQuantum;
				Tile.Height = FMath::Clamp(Height, -Tuning::MaxHeightMeters, Tuning::MaxHeightMeters);

				Tile.Fertility = FMath::Clamp(
					Tuning::TerrainFertilityBase
						+ Fbm(U * TerrainNoise::FertilityFreq, V * TerrainNoise::FertilityFreq,
							Config.Seed + TerrainNoise::FertilitySeed, TerrainNoise::FertilityOctaves) * Tuning::TerrainFertilityNoise,
					Tuning::TerrainFertilityMin, 1.0f);
				Tile.Moisture = FMath::Clamp(
					Tuning::TerrainMoistureBase
						+ Fbm(U * TerrainNoise::MoistureFreq, V * TerrainNoise::MoistureFreq,
							Config.Seed + TerrainNoise::MoistureSeed, TerrainNoise::MoistureOctaves) * Tuning::TerrainMoistureNoise,
					0.0f, 1.0f);
				Tile.Mat = static_cast<uint8>(EMaterial::Bare);
				// A realm that does not open in chaos opens already painted. The self-paint rate is
				// 0.012 per world minute, so waiting for it meant staring at an unbroken grey sheet
				// for the first forty minutes; colour the land now and let the seasons change it.
				Tile.Painted = Config.bChaosStart ? 0.0f : 0.72f;
			}
		}

		RecomputeWaterBodies();
	}

	void FSimKernel::RecomputeWaterBodies()
	{
		const int32 Size = Config.Size;
		const float WaterLevel = Sky.WaterLevel;

		// Rivers are M1; M0 exposes lake / sea / wetland around the single base water level.
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			FTile& Tile = Tiles[Index];
			const int32 X = Index % Size;
			const int32 Y = Index / Size;

			if (Tile.Height < WaterLevel - Tuning::WaterDepthEpsilonMeters)
			{
				const bool bAtEdge = (X == 0 || Y == 0 || X == Size - 1 || Y == Size - 1);
				const float Depth = WaterLevel - Tile.Height;
				if (bAtEdge && Depth > Tuning::SeaDepthMeters)
				{
					Tile.WaterBody = static_cast<uint8>(EWaterBody::Sea);
				}
				else if (Depth > Tuning::LakeDepthMeters)
				{
					Tile.WaterBody = static_cast<uint8>(EWaterBody::Lake);
				}
				else
				{
					Tile.WaterBody = static_cast<uint8>(EWaterBody::Wetland);
				}
				Tile.Mat = static_cast<uint8>(EMaterial::Water);
			}
			else if (Tile.WaterBody != static_cast<uint8>(EWaterBody::None))
			{
				Tile.WaterBody = static_cast<uint8>(EWaterBody::None);
			}
		}
	}

	void FSimKernel::ClassifyBiomes()
	{
		// Priority order from docs/WORLD.md SS2.2, restricted to the M0 set:
		// water -> rock slope -> woodland -> grass.
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			FTile& Tile = Tiles[Index];
			if (Tile.IsWater())
			{
				Tile.Biome = static_cast<uint8>(EBiome::Water);
				continue;
			}
			const float Temp = Tile.LocalTempC;
			const Tuning::FBiomeRange& Woodland = Tuning::BiomeWoodland;
			const Tuning::FBiomeRange& Grass = Tuning::BiomeGrass;
			if (Tile.Moisture < Tuning::BiomeRockSlopeMoistureMax
				&& Tile.Height > Tuning::BiomeRockSlopeHeightMinM
				&& Tile.Fertility < Tuning::BiomeRockSlopeFertilityMax)
			{
				Tile.Biome = static_cast<uint8>(EBiome::RockSlope);
			}
			else if (Temp >= Woodland.TempMinC && Temp <= Woodland.TempMaxC
				&& Tile.Moisture >= Woodland.MoistureMin && Tile.Moisture <= Woodland.MoistureMax
				&& Tile.Height < Woodland.HeightMaxM && Tile.Fertility > Woodland.FertilityMin)
			{
				Tile.Biome = static_cast<uint8>(EBiome::Woodland);
			}
			else if (Temp >= Grass.TempMinC && Temp <= Grass.TempMaxC
				&& Tile.Moisture >= Grass.MoistureMin && Tile.Moisture <= Grass.MoistureMax
				&& Tile.Height < Grass.HeightMaxM && Tile.Fertility > Grass.FertilityMin)
			{
				Tile.Biome = static_cast<uint8>(EBiome::Grass);
			}
			else if (Tile.Fertility < Tuning::BiomeDesertFertility)
			{
				Tile.Biome = static_cast<uint8>(EBiome::RockSlope);
			}
			else
			{
				Tile.Biome = (Tile.Height > Tuning::BiomeHighlandHeightM)
					? static_cast<uint8>(EBiome::RockSlope) : static_cast<uint8>(EBiome::Grass);
			}
		}
	}

	// ======================================================================
	// Time
	// ======================================================================

	void FSimKernel::SetTimeMode(ETimeMode NewMode)
	{
		if (TimeMode == NewMode)
		{
			return;
		}
		TimeMode = NewMode;
		MinuteAccumulator = 0.0;
		Note(NewMode == ETimeMode::Real ? TEXT("时间交给现实") : TEXT("时间回到手里"),
			NewMode == ETimeMode::Real ? TEXT("Time handed to reality") : TEXT("Time back in hand"),
			EEventType::TimeMode);
	}

	void FSimKernel::SetPaused(bool bInPaused)
	{
		bPaused = bInPaused;
		if (bPaused)
		{
			SpeedIndex = 0;
		}
		else if (SpeedIndex == 0)
		{
			SpeedIndex = 2;
		}
	}

	void FSimKernel::SetSpeedIndex(int32 Index)
	{
		SpeedIndex = FMath::Clamp(Index, 0, 3);
		bPaused = (SpeedIndex == 0);
	}

	void FSimKernel::SetRealMinutesPerWorldDay(float Minutes)
	{
		RealMinutesPerWorldDay = FMath::Clamp(Minutes, 1.0f, 60.0f);
	}

	void FSimKernel::FeedReality(const FRealitySample& Sample)
	{
		Reality = Sample;
	}

	void FSimKernel::Advance(double RealDeltaSeconds)
	{
		if (RealDeltaSeconds <= 0.0)
		{
			return;
		}

		// Sediment echo and the death markers run on real seconds, in both modes.
		RealSecondAccumulator += RealDeltaSeconds;
		const int64 MinuteNow = static_cast<int64>(RealSecondAccumulator / 60.0);
		if (MinuteNow != RealMinuteIndex)
		{
			RealMinuteIndex = MinuteNow;
			EchoThisMinute = 0;	// the rate ceiling is a real-minute window (ECONOMY SS2.1 rule 3)
		}

		StepSediment(RealDeltaSeconds);
		StepDeathMarks(RealDeltaSeconds);

		// The wet-paper undo window is measured in real seconds, in both modes.
		if (bUndoAvailable)
		{
			UndoSecondsLeft -= RealDeltaSeconds;
			if (UndoSecondsLeft <= 0.0)
			{
				bUndoAvailable = false;
				UndoSnapshot.Reset();
				UndoTiles.Reset();
			}
		}

		if (bPaused && TimeMode == ETimeMode::Virtual)
		{
			return;
		}

		double WorldMinutesPerRealSecond = 0.0;
		if (TimeMode == ETimeMode::Real)
		{
			WorldMinutesPerRealSecond = 1.0 / 60.0;	// 1:1 with the system clock
		}
		else
		{
			const double Multipliers[4] = { 0.0, 1.0, 4.0, 20.0 };
			const double Multiplier = Multipliers[FMath::Clamp(SpeedIndex, 0, 3)];
			const double MinutesPerWorldDay = FMath::Max(0.5, static_cast<double>(RealMinutesPerWorldDay));
			WorldMinutesPerRealSecond = (1440.0 / MinutesPerWorldDay / 60.0) * Multiplier;
		}

		MinuteAccumulator += RealDeltaSeconds * WorldMinutesPerRealSecond;

		// Cap catch-up work per call so a stalled frame cannot lock the game.
		int32 Steps = static_cast<int32>(MinuteAccumulator);
		const int32 MaxSteps = 600;
		if (Steps > MaxSteps)
		{
			Steps = MaxSteps;
			MinuteAccumulator = 0.0;
		}
		else
		{
			MinuteAccumulator -= static_cast<double>(Steps);
		}

		for (int32 Step = 0; Step < Steps; ++Step)
		{
			StepWorldMinute();
		}
	}

	// ======================================================================
	// The one world minute
	// ======================================================================

	void FSimKernel::StepWorldMinute()
	{
		++WorldMinute;

		const int32 WorldDayNow = GetWorldDay();
		if (WorldDayNow != LastWorldDaySeen)
		{
			LastWorldDaySeen = WorldDayNow;
			for (int32 Index = 0; Index < DayReleaseCount.Num(); ++Index)
			{
				DayReleaseCount[Index] = 0;
			}
		}

		StepEnvironment();

		// SimLOD: cheap always, expensive rarely (docs/TECH.md SS1.3).
		StepTiles(1.0f);

		WeatherAccumulator += 1.0f;
		if (WeatherAccumulator >= Tuning::WeatherStepWorldMinutes)
		{
			StepWeather(WeatherAccumulator);
			WeatherAccumulator = 0.0f;
		}

		const int32 LOD = GetSimLodLevel();
		const float EcoPeriod = (LOD >= 2) ? Tuning::EcoPeriodLod2WorldMinutes
			: ((LOD == 1) ? Tuning::EcoPeriodLod1WorldMinutes : Tuning::EcoPeriodLod0WorldMinutes);
		EcoAccumulator += 1.0f;
		if (EcoAccumulator >= EcoPeriod)
		{
			StepEcology();
			// Reclassify occasionally: biome drift is a threshold crossing, not a script.
			if ((++EcoStepCounter % Tuning::BiomeRecheckEveryEcoSteps) == 0)
			{
				ClassifyBiomes();
			}
			EcoAccumulator = 0.0f;
		}

		StepDisturbance(1.0f);

		// One coarse visual refresh every six world hours (see MarkRegionBuilt contract).
		if ((WorldMinute % Tuning::MarkAllDirtyEveryWorldMinutes) == 0)
		{
			MarkAllDirty();
		}
	}

	int32 FSimKernel::SolarDayOfYear() const
	{
		return (StartDayOfYear + (GetWorldDay() - 1)) % 365;
	}

	float FSimKernel::SolarLatitudeDeg() const
	{
		return FMath::Clamp(Config.Latitude, -Tuning::LatitudeClampDeg, Tuning::LatitudeClampDeg);
	}

	// The sun's position, the light level and the light's colour temperature are a pure function
	// of the world clock and the cloud cover - no drift, no history. That is why Init() can call
	// this before the first world minute ever elapses: until it did, Sky sat at FSkyState's
	// default 45-degree sun and every dusk/night test frame came out daylit (2026-10-18).
	void FSimKernel::UpdateSolar()
	{
		const float MinutesOfDay = static_cast<float>(WorldMinute % 1440);	// 1440 world minutes per day
		const float HourOfDay = MinutesOfDay / 60.0f;
		const float DayFraction = MinutesOfDay / 1440.0f;
		Sky.DayFraction = DayFraction;

		// ---- solar position (docs/WORLD.md SS3.2, docs/WORLD.md SS3) ----
		const int32 DayOfYear = SolarDayOfYear();
		const float Declination = Tuning::EarthAxialTiltDeg
			* FMath::Sin(2.0f * PI * (Tuning::DeclinationDayOffset + static_cast<float>(DayOfYear)) / 365.0f);
		const float Latitude = SolarLatitudeDeg();
		const float HourAngle = Tuning::SolarHourAngleDegPerHour * (HourOfDay - 12.0f);	// solar noon at 12:00
		const float SinAlt = FMath::Sin(FMath::DegreesToRadians(Latitude)) * FMath::Sin(FMath::DegreesToRadians(Declination))
			+ FMath::Cos(FMath::DegreesToRadians(Latitude)) * FMath::Cos(FMath::DegreesToRadians(Declination))
			* FMath::Cos(FMath::DegreesToRadians(HourAngle));
		const float Altitude = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f)));
		Sky.SolarAltitudeDeg = Altitude;
		Sky.SunAzimuthDeg = FMath::Fmod(
			HourOfDay * Tuning::SolarHourAngleDegPerHour + Tuning::SolarAzimuthOffsetDeg, 360.0f);

		const float Elevation01 = FMath::Clamp(
			(Altitude - Tuning::LightAltitudeFloorDeg) / Tuning::LightAltitudeSpanDeg, 0.0f, 1.0f);
		Sky.LightIntensity = Elevation01 * (1.0f - Sky.CloudCover * Tuning::CloudOcclusionFactor);

		// Warmer at the horizon, cooler at zenith.
		Sky.ColorTempK = FMath::Lerp(Tuning::ColorTempHorizonK, Tuning::ColorTempZenithK,
			FMath::Pow(Elevation01, Tuning::ColorTempExponent));
	}

	void FSimKernel::StepEnvironment()
	{
		UpdateSolar();

		const float HourOfDay = static_cast<float>(WorldMinute % 1440) / 60.0f;

		// ---- air temperature ----
		const float Seasonal = -FMath::Cos(
			2.0f * PI * (static_cast<float>(SolarDayOfYear()) - Tuning::SeasonalDayOffset) / 365.0f);	// -1 winter .. +1 summer
		const float Hemisphere = (SolarLatitudeDeg() >= 0.0f) ? 1.0f : -1.0f;
		const float SeasonalC = Seasonal * Hemisphere
			* (Tuning::SeasonalAmplitudeC + FMath::Abs(SolarLatitudeDeg()) * Tuning::SeasonalLatitudeGain);
		const float DiurnalC = FMath::Sin(2.0f * PI * (HourOfDay - Tuning::DiurnalPeakHour) / 24.0f)
			* Tuning::DiurnalAmplitudeC;
		Sky.AirTempC = Tuning::BaseAirTempC + SeasonalC + DiurnalC - Sky.CloudCover * Tuning::CloudCoolingC;

		if (TimeMode == ETimeMode::Real && Reality.bValid)
		{
			// The bridge: the real local reading is the base, shaped by the local hour.
			Sky.AirTempC = Reality.TemperatureC + DiurnalC * 0.5f;
			Sky.Humidity = FMath::Clamp(Reality.Humidity01, 0.02f, 0.98f);
			Sky.CloudCover = FMath::Clamp(Reality.CloudCover01, 0.0f, 1.0f);
			Sky.WindSpeed = FMath::Max(0.1f, Reality.WindSpeedMs);
			Sky.WindDirDeg = Reality.WindDirectionDeg;
			Sky.bRealityOffline = !Reality.bWeatherFromNetwork;
		}
		else if (TimeMode == ETimeMode::Real)
		{
			Sky.bRealityOffline = true;
		}
		else
		{
			Sky.bRealityOffline = false;
		}

		// Humidity creeps toward its weather target, plus water adjacency.
		float WaterTiles = 0.0f;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			if (Tiles[Index].IsWater())
			{
				WaterTiles += 1.0f;
			}
		}
		const float WaterFraction = WaterTiles / FMath::Max(1.0f, static_cast<float>(Tiles.Num()));

		// Tides: one world day period (docs/WORLD.md SS3.1).
		float TargetWaterLevel = Config.BaseWaterLevel;
		if (Config.bTides)
		{
			TidalPhase = FMath::Fmod(static_cast<float>(WorldMinute) / 1440.0f * 2.0f * PI, 2.0f * PI);
			TargetWaterLevel += Config.TidalAmplitude * FMath::Sin(TidalPhase);
		}
		if (!FMath::IsNearlyEqual(Sky.WaterLevel, TargetWaterLevel, 0.02f))
		{
			const float Previous = Sky.WaterLevel;
			Sky.WaterLevel = FMath::Lerp(Sky.WaterLevel, TargetWaterLevel, Tuning::WaterLevelLerpRate);
			if (!FMath::IsNearlyEqual(Previous, Sky.WaterLevel, Tuning::HeightQuantum))
			{
				RecomputeWaterBodies();
				MarkAllDirty();
			}
		}

		// Net humidity drift.
		const float RainIn = Sky.RainIntensity * Tuning::HumidityRainGain;
		const float Evaporate = FMath::Max(0.0f, Sky.AirTempC - Tuning::HumidityEvaporateBaseC)
			* Tuning::HumidityEvaporateRate;
		Sky.Humidity = FMath::Clamp(
			Sky.Humidity + RainIn - Evaporate + WaterFraction * Tuning::HumidityWaterGain, 0.02f, 0.99f);

		// Slow variable: oxygen tracks total vegetation.
		const float VegMean = GetVegetationMean();
		Sky.OxygenLevel = FMath::Clamp(
			Sky.OxygenLevel + (VegMean - Tuning::OxygenTargetVegetation) * Tuning::OxygenDriftRate,
			Tuning::OxygenMin, Tuning::OxygenMax);

		// Weather may follow the real reading in real mode.
		if (TimeMode == ETimeMode::Real && Reality.bValid && Reality.bWeatherFromNetwork)
		{
			Sky.RainIntensity = FMath::Clamp(Reality.PrecipitationMm / Tuning::PrecipitationToRainDivisor, 0.0f, 1.0f);
			if (Sky.RainIntensity > 0.0f)
			{
				// The real reading only says "water is falling"; the state machine picks the label.
				Sky.Weather = static_cast<uint8>(
					(Sky.RainIntensity > Tuning::RainToStormThreshold01) ? EWeather::Storm : EWeather::LightRain);
			}
			else if (Sky.CloudCover > Tuning::CloudToOvercastThreshold01)
			{
				Sky.Weather = static_cast<uint8>(EWeather::Cloudy);
			}
			else
			{
				Sky.Weather = static_cast<uint8>(EWeather::Clear);
			}
		}
		Sky.bSnowing = (Sky.RainIntensity > 0.0f && Sky.AirTempC < 0.0f);

		// Local tile temperature: base + lapse rate + water proximity.
		const float BaseTemp = Sky.AirTempC;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			FTile& Tile = Tiles[Index];
			const float Lapse = Tuning::LapseRatePer100m * (Tile.Height / 100.0f);
			float Target = BaseTemp + Lapse;
			if (Tile.IsWater())
			{
				Target += Tuning::WaterTempOffsetC;
			}
			Tile.LocalTempC = FMath::Lerp(Tile.LocalTempC, Target, Tuning::TileTempLerpRate);
		}
	}

	void FSimKernel::StepTiles(float DeltaWorldMinutes)
	{
		const float DeltaHours = DeltaWorldMinutes / 60.0f;
		const bool bWet = Sky.RainIntensity > 0.01f;

		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			FTile& Tile = Tiles[Index];

			// ---- moisture: rain in, evaporation out, water bodies balance ----
			float Moisture = Tile.Moisture;
			if (bWet)
			{
				Moisture += Sky.RainIntensity * Tuning::TileMoistureRainGain * DeltaWorldMinutes;
			}
			Moisture -= FMath::Max(0.0f, Tile.LocalTempC - Tuning::TileMoistureEvaporateBaseC)
				* Tuning::TileMoistureEvaporateRate * DeltaWorldMinutes;
			if (Tile.IsWater())
			{
				Moisture = FMath::Lerp(Moisture, Tuning::TileWaterMoisture, Tuning::TileWaterMoistureLerpRate);
			}
			Tile.Moisture = FMath::Clamp(Moisture, 0.0f, 1.0f);

			// ---- snow / ice phase change (docs/WORLD.md SS5) ----
			if (Tile.IsWater() && Tile.LocalTempC < 0.0f)
			{
				Tile.Mat = static_cast<uint8>(EMaterial::Ice);
			}
			else if (Tile.GetMat() == EMaterial::Ice && Tile.LocalTempC >= Tuning::IceMeltTempC)
			{
				Tile.Mat = Tile.IsWater() ? static_cast<uint8>(EMaterial::Water) : static_cast<uint8>(EMaterial::Mud);
			}
			else if (!Tile.IsWater() && Sky.bSnowing && Sky.RainIntensity > Tuning::SnowMinRainIntensity
				&& Tile.LocalTempC < 0.0f && Tile.GetMat() != EMaterial::Snow)
			{
				Tile.Mat = static_cast<uint8>(EMaterial::Snow);
			}
			else if (Tile.GetMat() == EMaterial::Snow && Tile.LocalTempC > Tuning::SnowMeltTempC && !Sky.bSnowing)
			{
				Tile.Mat = static_cast<uint8>(EMaterial::Mud);
				Tile.Moisture = FMath::Clamp(Tile.Moisture + Tuning::SnowMeltMoistureGain, 0.0f, 1.0f);
			}

			// ---- fire: spread needs heat, fuel and dryness (PHYSICS SS5) ----
			const FMaterialProps& Props = PropsOf(Tile.GetMat());
			if (Tile.Burning > 0.0f)
			{
				const float BurnRate = Tuning::BurnRatePerMinute * DeltaWorldMinutes * Sky.OxygenLevel;
				Tile.Burning = FMath::Max(0.0f, Tile.Burning - BurnRate);
				Tile.Vegetation = FMath::Max(0.0f,
					Tile.Vegetation - Tuning::BurnVegetationLossPerMinute * DeltaWorldMinutes);
				Tile.Ash = FMath::Clamp(Tile.Ash + Tuning::BurnAshGainPerMinute * DeltaWorldMinutes, 0.0f, 1.0f);
				Tile.Moisture = FMath::Max(0.0f,
					Tile.Moisture - Tuning::BurnMoistureLossPerMinute * DeltaWorldMinutes);
				if (Tile.Burning <= 0.0f)
				{
					Tile.Mat = static_cast<uint8>(EMaterial::Ash);
					Tile.Fertility = FMath::Clamp(Tile.Fertility + Tile.Ash * Tuning::AshToFertility, 0.0f, 1.0f);
					MarkDirtyAt(Index);
				}
				else
				{
					// Wind carries the flame downwind.
					const int32 Size = Config.Size;
					const int32 X = Index % Size;
					const int32 Y = Index / Size;
					const float WindRad = FMath::DegreesToRadians(Sky.WindDirDeg);
					const int32 DX = FMath::RoundToInt(FMath::Cos(WindRad));
					const int32 DY = FMath::RoundToInt(FMath::Sin(WindRad));
					const int32 NX = X + DX;
					const int32 NY = Y + DY;
					if (InBounds(NX, NY))
					{
						FTile& Neighbour = Tiles[ToIndex(NX, NY)];
						const FMaterialProps& NeighbourProps = PropsOf(Neighbour.GetMat());
						const float Dryness = 1.0f - Neighbour.Moisture;
						const float Chance = NeighbourProps.Combustibility * Dryness * Sky.WindSpeed * 0.02f * DeltaWorldMinutes;
						if (Neighbour.Burning <= 0.0f && Neighbour.LocalTempC > NeighbourProps.IgniteTempC - 300.0f
							&& Rng.Stream(ERngStream::Weather).Chance(Chance))
						{
							Neighbour.Burning = 1.0f;
							Neighbour.Mat = static_cast<uint8>(EMaterial::DeadWood);
							MarkDirtyAt(ToIndex(NX, NY));
						}
					}
				}
			}
			else if (Props.Combustibility > 0.0f && Tile.LocalTempC > Props.IgniteTempC
				&& Sky.OxygenLevel > 0.9f && Tile.Vegetation > 0.1f)
			{
				Tile.Burning = 1.0f;
			}

			// ---- chaos -> colour: the world paints itself as life takes hold ----
			if (Tile.Painted < 1.0f)
			{
				if (Tile.Vegetation > 0.03f || Tile.Ash > 0.05f)
				{
					Tile.Painted = FMath::Min(1.0f, Tile.Painted + 0.012f * DeltaWorldMinutes);
				}
				if (Tile.Painted >= 0.5f && Tile.GetMat() == EMaterial::Bare)
				{
					// First visible colouring of this tile: switch to its biome material.
					EMaterial Painted2 = EMaterial::Soil;
					switch (Tile.GetBiome())
					{
					case EBiome::Water:     Painted2 = EMaterial::Water; break;
					case EBiome::Grass:     Painted2 = EMaterial::Soil; break;
					case EBiome::Woodland:  Painted2 = EMaterial::Soil; break;
					case EBiome::RockSlope: Painted2 = EMaterial::Stone; break;
					default:                Painted2 = EMaterial::Soil; break;
					}
					Tile.Mat = static_cast<uint8>(Painted2);
					MarkDirtyAt(Index);
				}
			}
		}

		// Chaos ends once the world has taken on enough colour, or life exists.
		if (bChaosState)
		{
			const bool bLife = (Individuals.Num() > 0);
			if (bLife || GetPaintedFraction() > 0.25f)
			{
				bChaosState = false;
				Note(TEXT("这片纸开始上色了"), TEXT("The paper starts taking colour"), EEventType::Genesis);
			}
		}
	}

	void FSimKernel::StepWeather(float DeltaWorldMinutes)
	{
		if (TimeMode == ETimeMode::Real && Reality.bValid && Reality.bWeatherFromNetwork)
		{
			return;	// the real machine has already spoken
		}

		const int32 Hour = GetWorldHour();
		const bool bNight = (Hour < 6 || Hour >= 20);
		const float Humidity = Sky.Humidity;
		const float SeasonalBias = FMath::Clamp((10.0f - Sky.AirTempC) / 30.0f, 0.0f, 0.6f);

		// Transition weights, docs/WORLD.md SS3.2 (initial proposal values).
		float Weights[static_cast<int32>(EWeather::Count)] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		switch (Sky.GetWeather())
		{
		case EWeather::Clear:
			Weights[static_cast<int32>(EWeather::Cloudy)] = 0.30f + 0.5f * Humidity;
			Weights[static_cast<int32>(EWeather::LightRain)] = 0.05f * Humidity;
			if (bNight && Humidity > 0.7f)
			{
				Weights[static_cast<int32>(EWeather::Fog)] = 0.05f;
			}
			break;
		case EWeather::Cloudy:
			Weights[static_cast<int32>(EWeather::Clear)] = 0.35f;
			Weights[static_cast<int32>(EWeather::LightRain)] = 0.15f + 0.4f * Humidity;
			Weights[static_cast<int32>(EWeather::Storm)] = 0.02f * Humidity;
			if (bNight)
			{
				Weights[static_cast<int32>(EWeather::Fog)] = 0.05f;
			}
			break;
		case EWeather::LightRain:
			Weights[static_cast<int32>(EWeather::Clear)] = 0.20f;
			Weights[static_cast<int32>(EWeather::Cloudy)] = 0.30f;
			Weights[static_cast<int32>(EWeather::Storm)] = 0.10f + 0.4f * Humidity;
			Weights[static_cast<int32>(EWeather::Fog)] = 0.05f;
			break;
		case EWeather::Storm:
			Weights[static_cast<int32>(EWeather::Clear)] = 0.10f;
			Weights[static_cast<int32>(EWeather::Cloudy)] = 0.30f;
			Weights[static_cast<int32>(EWeather::LightRain)] = 0.50f;
			break;
		default:	// Fog
			Weights[static_cast<int32>(EWeather::Clear)] = (Hour >= 7 && Hour <= 18) ? 0.40f : 0.05f;
			Weights[static_cast<int32>(EWeather::Cloudy)] = 0.30f;
			Weights[static_cast<int32>(EWeather::LightRain)] = 0.10f;
			break;
		}

		// Cold air pushes toward neither rain nor fog but keeps the sky grey.
		Weights[static_cast<int32>(EWeather::Cloudy)] += SeasonalBias * 0.1f;

		float Total = 0.0f;
		for (int32 Index = 0; Index < static_cast<int32>(EWeather::Count); ++Index)
		{
			Total += Weights[Index];
		}

		if (Total > 1.0e-4f)
		{
			float Roll = Rng.Stream(ERngStream::Weather).RangeFloat(0.0f, Total);
			for (int32 Index = 0; Index < static_cast<int32>(EWeather::Count); ++Index)
			{
				Roll -= Weights[Index];
				if (Roll <= 0.0f)
				{
					const EWeather NewWeather = static_cast<EWeather>(Index);
					if (NewWeather != Sky.GetWeather())
					{
						Sky.Weather = static_cast<uint8>(NewWeather);
						const TCHAR* Glyph = WeatherGlyph(NewWeather);
						Note(FString::Printf(TEXT("天气转为%s"), Glyph),
							FString::Printf(TEXT("Weather turns to %s"), *FString(Glyph)),
							EEventType::Weather);
					}
					break;
				}
			}
		}

		// Rain intensity and cloud cover follow the chosen state.
		switch (Sky.GetWeather())
		{
		case EWeather::Clear:
			Sky.RainIntensity = 0.0f;
			Sky.CloudCover = FMath::Lerp(Sky.CloudCover, 0.10f, 0.35f);
			break;
		case EWeather::Cloudy:
			Sky.RainIntensity = 0.0f;
			Sky.CloudCover = FMath::Lerp(Sky.CloudCover, 0.55f, 0.35f);
			break;
		case EWeather::LightRain:
			Sky.RainIntensity = FMath::Lerp(Sky.RainIntensity, 0.35f, 0.4f);
			Sky.CloudCover = FMath::Lerp(Sky.CloudCover, 0.80f, 0.35f);
			break;
		case EWeather::Storm:
			Sky.RainIntensity = FMath::Lerp(Sky.RainIntensity, 0.90f, 0.4f);
			Sky.CloudCover = FMath::Lerp(Sky.CloudCover, 0.95f, 0.35f);
			break;
		default:
			Sky.RainIntensity = FMath::Lerp(Sky.RainIntensity, 0.05f, 0.3f);
			Sky.CloudCover = FMath::Lerp(Sky.CloudCover, 0.45f, 0.35f);
			break;
		}

		// First-sight rewards for weather phenomena (docs/CARDS.md SS2.3).
		if (Sky.RainIntensity > 0.2f)
		{
			DaysWithoutRain = 0;
			if (!bFirstRainSeen)
			{
				bFirstRainSeen = true;
				AwardFirsthand(TEXT("first.rain"), Sky.bSnowing ? Tuning::FirsthandWeather + 5 : Tuning::FirsthandWeather,
					EEventType::Weather,
					Sky.bSnowing ? TEXT("初雪落下来了") : TEXT("第一场雨"),
					Sky.bSnowing ? TEXT("First snow falls") : TEXT("The first rain"));
			}
			if (Sky.bSnowing && !bFirstSnowSeen)
			{
				bFirstSnowSeen = true;
				AwardFirsthand(TEXT("first.snow"), Tuning::FirsthandWeather, EEventType::Weather,
					TEXT("界里下起了雪"), TEXT("Snow inside the realm"));
			}
		}
		else
		{
			++DaysWithoutRain;
			DaysWithoutRainMax = FMath::Max(DaysWithoutRainMax, DaysWithoutRain);
		}
		if (Sky.GetWeather() == EWeather::Fog && !bFirstFogSeen)
		{
			bFirstFogSeen = true;
			AwardFirsthand(TEXT("first.fog"), Tuning::FirsthandWeather, EEventType::Weather,
				TEXT("起雾了"), TEXT("A fog rolls in"));
		}
	}

	void FSimKernel::StepDisturbance(float DeltaWorldMinutes)
	{
		Disturbance = FMath::Clamp(Disturbance + Tuning::DisturbanceDecayPerWorldHour * (DeltaWorldMinutes / 60.0f), 0.0f, 100.0f);
	}

	void FSimKernel::StepSediment(double RealDeltaSeconds)
	{
		// docs/CARDS.md SS2.2: sediment = potential x 0.5 per real minute.
		const double RealMinutes = RealDeltaSeconds / 60.0;
		const double Potential = static_cast<double>(GetEcoPotential());
		SedimentAccumulator += Potential * static_cast<double>(Tuning::SedimentCoefficient) * RealMinutes;

		if (SedimentAccumulator >= 1.0)
		{
			const int32 Whole = static_cast<int32>(SedimentAccumulator);
			SedimentAccumulator -= static_cast<double>(Whole);
			Ledger.Balance += Whole;
			Ledger.FromSediment += Whole;
		}
	}

	void FSimKernel::StepDeathMarks(double RealDeltaSeconds)
	{
		for (int32 Index = DeathMarks.Num() - 1; Index >= 0; --Index)
		{
			DeathMarks[Index].TimeToLive -= static_cast<float>(RealDeltaSeconds);
			if (DeathMarks[Index].TimeToLive <= 0.0f)
			{
				DeathMarks.RemoveAt(Index);
			}
		}
	}

	// ======================================================================
	// Ecology
	// ======================================================================

	void FSimKernel::StepEcology()
	{
		// 4 world minutes per eco step, in hours. This used to read `0.0667f * 4.0f` and several
		// rates multiplied by another 4 on top of it, so the whole ecology ran 16x fast: shrubs
		// burned their fertility and woody cover died out inside a day. One number, one unit.
		const float DeltaHours = 0.0667f;
		const float VegMean = GetVegetationMean();
		const int32 TotalIndividuals = Individuals.Num();
		const bool bWet = Sky.RainIntensity > 0.01f;

		// --- 1 producers: the moss-grass cover layer ---
		const int32 MossIndex = Catalog.FindSpecies(static_cast<uint8>(ESpeciesKind::MossGrass));
		if (MossIndex != INDEX_NONE)
		{
			// GrowthPerHour is a per-hour rate, but the species table was tuned against the old
			// 16x-fast step. Keep the food supply where the balance expects it with an explicit
			// gain rather than by hiding a second factor inside the step length.
			constexpr float MossGrowthGain = 4.0f;
			const FSpeciesDef& Moss = Catalog.Species[MossIndex];
			for (int32 Index = 0; Index < Tiles.Num(); ++Index)
			{
				FTile& Tile = Tiles[Index];
				if (Tile.IsWater())
				{
					continue;
				}
				const float Light = Sky.LightIntensity;
				const float Temp = Tile.LocalTempC;
				if (Light > 0.30f && Tile.Moisture > 0.30f && Tile.Fertility > 0.02f
					&& Temp > 0.0f && Temp < 38.0f)
				{
					const float TempFactor = (Temp >= 10.0f && Temp <= 28.0f) ? 1.0f : 0.35f;
					const float Growth = Moss.GrowthPerHour * MossGrowthGain * DeltaHours * TempFactor * Light * Tile.Fertility;
					const float Capacity = 0.35f + Tile.Fertility * 0.65f;
					if (Tile.Vegetation < Capacity)
					{
						const float Added = FMath::Min(Growth, Capacity - Tile.Vegetation);
						Tile.Vegetation += Added;
						Tile.Fertility = FMath::Max(0.0f, Tile.Fertility - Added * 0.05f);
					}
				}
				else if (Temp < 0.0f || Temp > 38.0f)
				{
					Tile.Vegetation = FMath::Max(0.0f, Tile.Vegetation - 0.004f * DeltaHours);
				}
			}
		}

		// --- 2 consumers: decide, eat, breed, die (ascending index = reproducible) ---
		int32 Births = 0;
		int32 Deaths = 0;
		const int32 Budget = Tuning::IndividualBudgetSmall;
		const bool bSpring = (GetWorldDay() % 90) < 30;

		// Backwards: KillIndividual() removes from this very array, so a forward loop skips
		// the next creature and holds a reference into storage that just moved.
		for (int32 Index = Individuals.Num() - 1; Index >= 0; --Index)
		{
			FIndividual& Creature = Individuals[Index];
			const int32 SpeciesIndex = Catalog.FindSpecies(Creature.Kind);
			if (SpeciesIndex == INDEX_NONE)
			{
				continue;
			}
			const FSpeciesDef& Def = Catalog.Species[SpeciesIndex];
			Creature.AgeDays += DeltaHours / 24.0f;

			switch (Creature.GetKind())
			{
			case ESpeciesKind::Grazer:
			{
				Creature.Energy -= Def.EnergyDrainPerHour * DeltaHours;
				const int32 Size = Config.Size;
				const int32 X = Creature.Tile % Size;
				const int32 Y = Creature.Tile / Size;

				// Forage when hungry; drink when far from water for too long.
				const int32 ForageTile = FindForageTile(Creature.Tile, 6, 0.08f);
				const int32 WaterTile = FindWaterTileNear(Creature.Tile, static_cast<int32>(Def.WaterReachCells));
				if (WaterTile == INDEX_NONE)
				{
					// No water in reach: dehydration clock starts.
					if (Creature.RestTimer > 0.0f)
					{
						Creature.RestTimer -= DeltaHours;
					}
					else
					{
						Creature.RestTimer = 0.0f;
					}
					Creature.Status = 1;
				}
				else
				{
					Creature.RestTimer = 3.0f;
				}

				if (Creature.Energy < 30.0f)
				{
					Creature.Status = 1;
					if (ForageTile != INDEX_NONE)
					{
						FTile& Target = Tiles[ForageTile];
						const float Eaten = FMath::Min(Def.EatVegetationPerHour * DeltaHours, Target.Vegetation);
						Target.Vegetation -= Eaten;
						Creature.Energy = FMath::Min(Def.EnergyMax, Creature.Energy + Eaten * 900.0f);
						Creature.Tile = ForageTile;
					}
				}
				else if (Creature.Energy > Def.EnergyMax * Def.BreedEnergyFraction
					&& Tiles[Creature.Tile].Vegetation > Def.BreedVegetation
					&& Births < 40 && (TotalIndividuals + Births) < Budget)
				{
					Creature.Status = 4;
					Creature.Energy -= Def.BreedCostEnergy;
					SpawnIndividual(ESpeciesKind::Grazer, Creature.Tile);
					++Births;
					if (!bFirstBirthSeen)
					{
						bFirstBirthSeen = true;
						AwardFirsthand(TEXT("first.birth"), Tuning::FirsthandEco, EEventType::Ecology,
							TEXT("第一次繁殖"), TEXT("The first birth"));
					}
				}
				else
				{
					Creature.Status = 0;
				}

				// Wander a little so the herd is not static.
				if (Creature.Status == 0 && Rng.Stream(ERngStream::Eco).Chance(0.08f))
				{
					const int32 NX = FMath::Clamp(X + Rng.Stream(ERngStream::Eco).Range(-1, 2), 0, Size - 1);
					const int32 NY = FMath::Clamp(Y + Rng.Stream(ERngStream::Eco).Range(-1, 2), 0, Size - 1);
					if (!Tiles[ToIndex(NX, NY)].IsWater())
					{
						Creature.Tile = ToIndex(NX, NY);
					}
				}

				if (Creature.Energy <= 0.0f)
				{
					KillIndividual(Index, EDeathCause::Starved);
					++Deaths;
				}
				else if (Creature.RestTimer <= 0.0f)
				{
					KillIndividual(Index, EDeathCause::Desiccated);
					++Deaths;
				}
				break;
			}
			case ESpeciesKind::Raptor:
			{
				Creature.Energy -= Def.EnergyDrainPerHour * DeltaHours;
				const float ActivityScale = bWet ? 0.5f : 1.0f;

				if (Creature.Energy < Def.EnergyMax * 0.5f && Rng.Stream(ERngStream::Eco).Chance(0.35f * ActivityScale))
				{
					// Hunt the nearest grazer inside the perception radius.
					int32 BestIndex = INDEX_NONE;
					int32 BestDistance = FMath::RoundToInt(Def.WaterReachCells * 0.0f) + 30 * 30;
					const int32 Size = Config.Size;
					const int32 HX = Creature.Tile % Size;
					const int32 HY = Creature.Tile / Size;
					for (int32 Other = 0; Other < Individuals.Num(); ++Other)
					{
						if (Individuals[Other].GetKind() != ESpeciesKind::Grazer)
						{
							continue;
						}
						const int32 OX = Individuals[Other].Tile % Size;
						const int32 OY = Individuals[Other].Tile / Size;
						const int32 Distance = (OX - HX) * (OX - HX) + (OY - HY) * (OY - HY);
						if (Distance < BestDistance)
						{
							BestDistance = Distance;
							BestIndex = Other;
						}
					}
					if (BestIndex != INDEX_NONE)
					{
						Creature.Status = 2;
						if (Rng.Stream(ERngStream::Eco).Chance(Def.PredationSuccess))
						{
							KillIndividual(BestIndex, EDeathCause::Predated);
							++Deaths;
							Creature.Energy = FMath::Min(Def.EnergyMax, Creature.Energy + 55.0f);
							++Creature.PreyCount;
							if (!bFirstPredationSeen)
							{
								bFirstPredationSeen = true;
								AwardFirsthand(TEXT("first.predation"), Tuning::FirsthandEco, EEventType::Ecology,
									TEXT("第一次捕食"), TEXT("The first hunt"));
							}
						}
					}
				}
				if (Creature.PreyCount >= 3 && Births < 40 && (TotalIndividuals + Births) < Budget)
				{
					Creature.PreyCount = 0;
					Creature.Energy -= Def.BreedCostEnergy * 0.5f;
					SpawnIndividual(ESpeciesKind::Raptor, Creature.Tile);
					++Births;
				}
				if (Creature.Energy <= 0.0f)
				{
					KillIndividual(Index, EDeathCause::Starved);
					++Deaths;
				}
				break;
			}
			case ESpeciesKind::StreamFish:
			{
				Creature.Energy -= Def.EnergyDrainPerHour * DeltaHours;
				FTile& Current = Tiles[Creature.Tile];
				if (!Current.IsWater())
				{
					// Stranding: one world day out of water is fatal (SPECIES SS2.1).
					Creature.RestTimer -= DeltaHours;
					if (Creature.RestTimer <= 0.0f)
					{
						KillIndividual(Index, EDeathCause::Drowned);
						++Deaths;
					}
				}
				else
				{
					Creature.RestTimer = 1.0f;
					if (Current.LocalTempC < -2.0f || Current.LocalTempC > 30.0f)
					{
						KillIndividual(Index, EDeathCause::Frozen);
						++Deaths;
						break;
					}
					Creature.Energy = FMath::Min(Def.EnergyMax, Creature.Energy + 1.6f * DeltaHours);
					if (bSpring && Current.LocalTempC > 15.0f && Rng.Stream(ERngStream::Eco).Chance(0.05f)
						&& Births < 40 && (TotalIndividuals + Births) < Budget)
					{
						SpawnIndividual(ESpeciesKind::StreamFish, Creature.Tile);
						SpawnIndividual(ESpeciesKind::StreamFish, Creature.Tile);
						Births += 2;
					}
					if (Rng.Stream(ERngStream::Eco).Chance(0.05f))
					{
						// Migrate with the water level.
						const int32 Size = Config.Size;
						const int32 X = Creature.Tile % Size;
						const int32 Y = Creature.Tile / Size;
						const int32 NX = FMath::Clamp(X + Rng.Stream(ERngStream::Eco).Range(-1, 2), 0, Size - 1);
						const int32 NY = FMath::Clamp(Y + Rng.Stream(ERngStream::Eco).Range(-1, 2), 0, Size - 1);
						if (Tiles[ToIndex(NX, NY)].IsWater())
						{
							Creature.Tile = ToIndex(NX, NY);
						}
					}
				}
				break;
			}
			case ESpeciesKind::Shrub:
			{
				Creature.Energy = Def.EnergyMax;
				FTile& Spot = Tiles[Creature.Tile];
				// Drought is dormancy, not death.
				//
				// The old rule killed a shrub the moment its tile dropped below 0.15 moisture.
				// The default world rolls "steppe", most of its land sits near that line, and the
				// result was a world with no woody cover at all - measured as 228 triangles of
				// props on a whole island, and no shade for anything that eats.
				if (Spot.LocalTempC < -2.0f)
				{
					KillIndividual(Index, EDeathCause::Frozen);
					++Deaths;
					break;
				}
				const bool bDormant = (Spot.Moisture < 0.15f);
				if (!bDormant)
				{
					Spot.Moisture = FMath::Clamp(Spot.Moisture + 0.05f * DeltaHours, 0.0f, 1.0f);
					Spot.Fertility = FMath::Max(0.0f, Spot.Fertility - Def.FertilityCostPerHour * DeltaHours);
					Spot.Vegetation = FMath::Min(1.0f, Spot.Vegetation + 0.04f * DeltaHours);
				}
				break;
			}
			default:
				break;
			}

			// Lifespan.
			if (Individuals.IsValidIndex(Index) && Creature.AgeDays > Def.LifespanDays)
			{
				KillIndividual(Index, EDeathCause::Aged);
				++Deaths;
			}
		}

		// --- 3 decomposition: abiotic rule returning fertility (SPECIES SS2 note) ---
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			FTile& Tile = Tiles[Index];
			if (Tile.Ash > 0.0f)
			{
				const float Returned = FMath::Min(Tile.Ash, 0.002f * DeltaHours * 4.0f);
				Tile.Ash -= Returned;
				Tile.Fertility = FMath::Clamp(Tile.Fertility + Returned * 0.8f, 0.0f, 1.0f);
			}
			if (Tile.Vegetation <= 0.001f && Tile.Fertility < Tuning::BiomeDesertFertility
				&& Tile.GetBiome() != EBiome::RockSlope && !Tile.IsWater())
			{
				Tile.Biome = static_cast<uint8>(EBiome::RockSlope);
			}
		}

		// --- 4 death marks and first-sight bookkeeping ---
		if (Deaths > 0 && !bFirstDeathSeen)
		{
			bFirstDeathSeen = true;
			AwardFirsthand(TEXT("first.death"), Tuning::FirsthandEco, EEventType::Ecology,
				TEXT("第一次死亡"), TEXT("The first death"));
		}

		// --- 5 seed rain: nothing is ever permanently dead (SPECIES SS2.1 / SS3) ---
		++SeedRainCounter;
		if (SeedRainCounter % 30 == 0 && VegMean > 0.15f)
		{
			for (int32 KindIndex = 0; KindIndex < static_cast<int32>(ESpeciesKind::Count); ++KindIndex)
			{
				const ESpeciesKind Kind = static_cast<ESpeciesKind>(KindIndex);
				if (Kind == ESpeciesKind::MossGrass || Kind == ESpeciesKind::Shrub)
				{
					continue;
				}
				if (GetPopulation(Kind) > 0)
				{
					continue;
				}
				if (Rng.Stream(ERngStream::Eco).Chance(0.10f))
				{
					const int32 Size = Config.Size;
					for (int32 Attempt = 0; Attempt < 40; ++Attempt)
					{
						const int32 X = Rng.Stream(ERngStream::Eco).Range(1, Size - 1);
						const int32 Y = Rng.Stream(ERngStream::Eco).Range(1, Size - 1);
						const int32 TileIndex = ToIndex(X, Y);
						const FTile& Tile = Tiles[TileIndex];
						const bool bOk = (Kind == ESpeciesKind::StreamFish) ? Tile.IsWater() : !Tile.IsWater();
						if (bOk)
						{
							SpawnIndividual(Kind, TileIndex);
							SpawnIndividual(Kind, TileIndex);
							if (!bFirstImmigrationSeen)
							{
								bFirstImmigrationSeen = true;
								AwardFirsthand(TEXT("first.immigration"), Tuning::FirsthandExtinction, EEventType::Ecology,
									TEXT("有外来的活物迁入"), TEXT("Something drifts in from outside"));
							}
							break;
						}
					}
				}
			}
		}

		// --- 6 population delta bookkeeping ---
		const int32 NowTotal = Individuals.Num();
		if (PreviousTotalPopulation > 0 && NowTotal < PreviousTotalPopulation / 2 && PreviousTotalPopulation > 20)
		{
			Note(FString::Printf(TEXT("种群从 %d 跌到 %d"), PreviousTotalPopulation, NowTotal),
				FString::Printf(TEXT("Population crashes from %d to %d"), PreviousTotalPopulation, NowTotal),
				EEventType::Ecology);
			PreviousTotalPopulation = NowTotal;
		}
		else
		{
			PreviousTotalPopulation = NowTotal;
		}
	}

	void FSimKernel::SeedLifeFromBiomes()
	{
		// Biome x species, with a **per-species quota**.
		//
		// The old loop had one shared "Placed < 120" cap and walked tiles in index order, which
		// starts with the sea. On any lake-heavy roll the whole budget went to fish; when the
		// water temperature sat a tenth of a degree under the fish's hard threshold they all
		// died on day one and the world finished the week with two grazers. A global cap makes
		// the opening population a property of iteration order, which is not a design.
		const int32 Budget = Tuning::IndividualBudgetSmall;
		int32 Quota[static_cast<int32>(ESpeciesKind::Count)] = {};
		Quota[static_cast<int32>(ESpeciesKind::Shrub)] = FMath::RoundToInt(Budget * 0.40f);
		Quota[static_cast<int32>(ESpeciesKind::Grazer)] = FMath::RoundToInt(Budget * 0.30f);
		Quota[static_cast<int32>(ESpeciesKind::StreamFish)] = FMath::RoundToInt(Budget * 0.15f);
		Quota[static_cast<int32>(ESpeciesKind::Raptor)] = FMath::RoundToInt(Budget * 0.10f);

		int32 Placed[static_cast<int32>(ESpeciesKind::Count)] = {};
		const auto TryPlace = [&](ESpeciesKind Kind, int32 Index)
		{
			const int32 Slot = static_cast<int32>(Kind);
			if (Placed[Slot] >= Quota[Slot])
			{
				return;
			}
			SpawnIndividual(Kind, Index);
			++Placed[Slot];
		};

		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			const FTile& Tile = Tiles[Index];
			if (Tile.IsWater())
			{
				if (Tile.WaterBody == static_cast<uint8>(EWaterBody::Lake) && Rng.Stream(ERngStream::Eco).Chance(0.04f))
				{
					TryPlace(ESpeciesKind::StreamFish, Index);
				}
				continue;
			}
			switch (Tile.GetBiome())
			{
			case EBiome::Grass:
				if (Rng.Stream(ERngStream::Eco).Chance(0.012f))
				{
					TryPlace(ESpeciesKind::Grazer, Index);
				}
				else if (Rng.Stream(ERngStream::Eco).Chance(0.004f))
				{
					TryPlace(ESpeciesKind::Raptor, Index);
				}
				break;
			case EBiome::Woodland:
				if (Rng.Stream(ERngStream::Eco).Chance(0.012f))
				{
					TryPlace(ESpeciesKind::Shrub, Index);
				}
				if (Rng.Stream(ERngStream::Eco).Chance(0.006f))
				{
					TryPlace(ESpeciesKind::Grazer, Index);
				}
				break;
			case EBiome::RockSlope:
				if (Rng.Stream(ERngStream::Eco).Chance(0.006f))
				{
					TryPlace(ESpeciesKind::Raptor, Index);
				}
				break;
			default:
				break;
			}
		}
		bChaosState = (Individuals.Num() == 0);
	}

		void FSimKernel::SpawnIndividual(ESpeciesKind Kind, int32 Tile)
	{
		if (!Tiles.IsValidIndex(Tile))
		{
			return;
		}
		FIndividual Creature;
		Creature.Kind = static_cast<uint8>(Kind);
		Creature.Tile = Tile;
		Creature.Id = static_cast<uint32>(NextIndividualId++);
		Creature.Energy = 100.0f;
		Creature.AgeDays = 0.0f;
		Creature.RestTimer = (Kind == ESpeciesKind::StreamFish) ? 1.0f : 3.0f;
		Creature.Facing = static_cast<uint8>(Rng.Stream(ERngStream::Eco).Range(0, 8));
		Individuals.Add(Creature);
		MarkDirtyAt(Tile);
	}

	void FSimKernel::KillIndividual(int32 IndividualIndex, EDeathCause Cause)
	{
		if (!Individuals.IsValidIndex(IndividualIndex))
		{
			return;
		}
		const int32 Tile = Individuals[IndividualIndex].Tile;
		FDeathMark Mark;
		Mark.Tile = Tile;
		Mark.Cause = static_cast<uint8>(Cause);
		Mark.TimeToLive = 6.0f;
		DeathMarks.Add(Mark);
		++DeathCauseCounts[static_cast<int32>(Cause)];

		// Decompose into fertility: the loop closes (SPECIES SS3).
		if (Tiles.IsValidIndex(Tile))
		{
			Tiles[Tile].Fertility = FMath::Clamp(Tiles[Tile].Fertility + 0.01f, 0.0f, 1.0f);
			Tiles[Tile].Vegetation = FMath::Max(0.0f, Tiles[Tile].Vegetation - 0.02f);
		}
		Individuals.RemoveAt(IndividualIndex);
	}

	int32 FSimKernel::FindWaterTileNear(int32 Tile, int32 MaxRadius) const
	{
		const int32 Size = Config.Size;
		const int32 CX = Tile % Size;
		const int32 CY = Tile / Size;
		for (int32 Radius = 0; Radius <= MaxRadius; ++Radius)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				for (int32 DX = -Radius; DX <= Radius; ++DX)
				{
					if (Radius > 0 && FMath::Abs(DX) != Radius && FMath::Abs(DY) != Radius)
					{
						continue;
					}
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (InBounds(X, Y) && Tiles[ToIndex(X, Y)].IsWater())
					{
						return ToIndex(X, Y);
					}
				}
			}
		}
		return INDEX_NONE;
	}

	int32 FSimKernel::FindForageTile(int32 Tile, int32 MaxRadius, float MinVegetation) const
	{
		const int32 Size = Config.Size;
		const int32 CX = Tile % Size;
		const int32 CY = Tile / Size;
		for (int32 Radius = 0; Radius <= MaxRadius; ++Radius)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				for (int32 DX = -Radius; DX <= Radius; ++DX)
				{
					if (Radius > 0 && FMath::Abs(DX) != Radius && FMath::Abs(DY) != Radius)
					{
						continue;
					}
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (!InBounds(X, Y))
					{
						continue;
					}
					const int32 Index = ToIndex(X, Y);
					if (!Tiles[Index].IsWater() && Tiles[Index].Vegetation >= MinVegetation)
					{
						return Index;
					}
				}
			}
		}
		return INDEX_NONE;
	}

	int32 FSimKernel::GetSimLodLevel() const
	{
		const int32 Count = Individuals.Num();
		if (Count > 800)
		{
			return 2;
		}
		if (Count > 400)
		{
			return 1;
		}
		return 0;
	}

	// ======================================================================
	// Cards: pool, hand, release, recycle
	// ======================================================================

	void FSimKernel::InitPoolAndUnlocks()
	{
		const int32 Count = Catalog.Cards.Num();
		Pool.Init(0, Count);
		Unlocked.Init(0, Count);
		DayReleaseCount.Init(0, Count);

		// docs/CARDS.md SS1.6: the starting pool is every 0-tier kind, one copy.
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Catalog.Cards[Index].Tier == 0)
			{
				Pool[Index] = 1;
				Unlocked[Index] = 1;
			}
		}
	}

	void FSimKernel::ResetHand()
	{
		const int32 Count = Catalog.Cards.Num();
		Hand.Init(INDEX_NONE, Tuning::HandSize);
		RefillHand();
	}

	int32 FSimKernel::ComputeDrawWeight(int32 CardIndex) const
	{
		if (!Pool.IsValidIndex(CardIndex) || !Catalog.Cards.IsValidIndex(CardIndex))
		{
			return 0;
		}
		// 0-tier is free and unlimited; weight it at the holding cap so it cannot drown the hand.
		const int32 Copies = (Catalog.Cards[CardIndex].Tier == 0) ? Tuning::MaxCopiesPerKind : Pool[CardIndex];
		return FMath::Clamp(Copies, 0, Tuning::MaxCopiesPerKind);
	}

	int32 FSimKernel::DrawFromPool(int32 Slot)
	{
		int32 Total = 0;
		for (int32 Index = 0; Index < Pool.Num(); ++Index)
		{
			if (Index == Slot)
			{
				continue;
			}
			bool bAlreadyInHand = false;
			for (int32 HandIndex = 0; HandIndex < Hand.Num(); ++HandIndex)
			{
				if (Hand[HandIndex] == Index)
				{
					bAlreadyInHand = true;
					break;
				}
			}
			if (!bAlreadyInHand)
			{
				Total += ComputeDrawWeight(Index);
			}
		}
		if (Total <= 0)
		{
			return INDEX_NONE;
		}

		int32 Roll = Rng.Stream(ERngStream::Loot).Range(0, Total);
		for (int32 Index = 0; Index < Pool.Num(); ++Index)
		{
			if (Index == Slot)
			{
				continue;
			}
			bool bAlreadyInHand = false;
			for (int32 HandIndex = 0; HandIndex < Hand.Num(); ++HandIndex)
			{
				if (Hand[HandIndex] == Index)
				{
					bAlreadyInHand = true;
					break;
				}
			}
			if (bAlreadyInHand)
			{
				continue;
			}
			Roll -= ComputeDrawWeight(Index);
			if (Roll < 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	void FSimKernel::RefillHand()
	{
		for (int32 Slot = 0; Slot < Hand.Num(); ++Slot)
		{
			if (Hand[Slot] != INDEX_NONE)
			{
				continue;
			}
			const int32 Drawn = DrawFromPool(Slot);
			if (Drawn != INDEX_NONE)
			{
				Hand[Slot] = Drawn;
			}
		}

		// Opening-hand guarantee (docs/CARDS.md SS1.6): at least one terrain
		// card and one weather card, so the first hand is never all record cards.
		static const EEffect TerrainEffects[] = { EEffect::RaiseTerrain, EEffect::LowerTerrain, EEffect::FlattenTerrain, EEffect::RidgeLine, EEffect::ErodeSurface, EEffect::PaintSurface };
		static const EEffect WeatherEffects[] = { EEffect::WeatherPush, EEffect::HeatArea, EEffect::ChillArea, EEffect::WaterLevel, EEffect::Ignite };

		auto HasCategory = [this](const EEffect* List, int32 ListCount) -> bool
		{
			for (int32 Slot = 0; Slot < Hand.Num(); ++Slot)
			{
				if (Hand[Slot] == INDEX_NONE)
				{
					continue;
				}
				const EEffect Effect = Catalog.Cards[Hand[Slot]].GetEffect();
				for (int32 Index = 0; Index < ListCount; ++Index)
				{
					if (Effect == List[Index])
					{
						return true;
					}
				}
			}
			return false;
		};

		auto ForceCategory = [this](const EEffect* List, int32 ListCount) -> void
		{
			for (int32 Slot = 0; Slot < Hand.Num(); ++Slot)
			{
				if (Hand[Slot] == INDEX_NONE)
				{
					continue;
				}
				if (Catalog.Cards[Hand[Slot]].Tier != 0)
				{
					continue;	// never throw away an extended card for the guarantee
				}
				for (int32 Index = 0; Index < Catalog.Cards.Num(); ++Index)
				{
					if (ComputeDrawWeight(Index) <= 0)
					{
						continue;
					}
					bool bInHand = false;
					for (int32 Other = 0; Other < Hand.Num(); ++Other)
					{
						if (Other != Slot && Hand[Other] == Index)
						{
							bInHand = true;
							break;
						}
					}
					if (bInHand)
					{
						continue;
					}
					for (int32 EffectIndex = 0; EffectIndex < ListCount; ++EffectIndex)
					{
						if (Catalog.Cards[Index].GetEffect() == List[EffectIndex])
						{
							Hand[Slot] = Index;
							return;
						}
					}
				}
			}
		};

		if (!HasCategory(TerrainEffects, UE_ARRAY_COUNT(TerrainEffects)))
		{
			ForceCategory(TerrainEffects, UE_ARRAY_COUNT(TerrainEffects));
		}
		if (!HasCategory(WeatherEffects, UE_ARRAY_COUNT(WeatherEffects)))
		{
			ForceCategory(WeatherEffects, UE_ARRAY_COUNT(WeatherEffects));
		}
	}

	void FSimKernel::RerollHand()
	{
		// Whole hand back to the pool, then redraw: free, no cooldown, no echo (INTERVENTION SS1.5).
		Hand.Init(INDEX_NONE, Tuning::HandSize);
		RefillHand();
	}

	int32 FSimKernel::GetHandCount() const
	{
		int32 Count = 0;
		for (int32 Slot = 0; Slot < Hand.Num(); ++Slot)
		{
			if (Hand[Slot] != INDEX_NONE)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 FSimKernel::GetHandCardIndex(int32 Slot) const
	{
		return Hand.IsValidIndex(Slot) ? Hand[Slot] : INDEX_NONE;
	}

	int32 FSimKernel::GetCopies(int32 CardIndex) const
	{
		if (!Pool.IsValidIndex(CardIndex))
		{
			return 0;
		}
		if (Catalog.Cards.IsValidIndex(CardIndex) && Catalog.Cards[CardIndex].Tier == 0)
		{
			return 999;	// free and unlimited
		}
		return Pool[CardIndex];
	}

	bool FSimKernel::IsCardUnlocked(int32 CardIndex) const
	{
		return Unlocked.IsValidIndex(CardIndex) && Unlocked[CardIndex] != 0;
	}

	int32 FSimKernel::GetUnlockedCardCount() const
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Unlocked.Num(); ++Index)
		{
			Count += (Unlocked[Index] != 0) ? 1 : 0;
		}
		return Count;
	}

	bool FSimKernel::CanRecycle(int32 CardIndex) const
	{
		if (!Catalog.Cards.IsValidIndex(CardIndex))
		{
			return false;
		}
		const FCardDef& Card = Catalog.Cards[CardIndex];
		// 0-tier and forbidden cards cannot be recycled (docs/CARDS.md SS3.2).
		if (Card.Tier == 0 || Card.IsForbidden())
		{
			return false;
		}
		return Pool.IsValidIndex(CardIndex) && Pool[CardIndex] > 0;
	}

	int32 FSimKernel::GetRecycleRefundFor(int32 CardIndex) const
	{
		if (!CanRecycle(CardIndex))
		{
			return 0;
		}
		return Tuning::RecycleRefund[FMath::Clamp(Catalog.Cards[CardIndex].Tier, 0, 11)];
	}

	bool FSimKernel::RecycleCard(int32 HandSlot, int32& OutRefund, FString& OutReasonZh, FString& OutReasonEn)
	{
		OutRefund = 0;
		if (!Hand.IsValidIndex(HandSlot) || Hand[HandSlot] == INDEX_NONE)
		{
			OutReasonZh = TEXT("这个格子是空的");
			OutReasonEn = TEXT("That slot is empty");
			return false;
		}
		const int32 CardIndex = Hand[HandSlot];
		const FCardDef& Card = Catalog.Cards[CardIndex];
		if (Card.Tier == 0)
		{
			OutReasonZh = TEXT("碎片卡不可回收，它只能被用掉");
			OutReasonEn = TEXT("Fragment cards cannot be recycled, only used");
			return false;
		}
		if (Card.IsForbidden())
		{
			OutReasonZh = TEXT("禁物不可回收");
			OutReasonEn = TEXT("Forbidden things cannot be recycled");
			return false;
		}

		OutRefund = Tuning::RecycleRefund[FMath::Clamp(Card.Tier, 0, 11)];
		Pool[CardIndex] = FMath::Max(0, Pool[CardIndex] - 1);
		Ledger.Balance += OutRefund;
		Ledger.FromRecycle += OutRefund;
		Hand[HandSlot] = INDEX_NONE;

		Note(FString::Printf(TEXT("回收「%s」"), *Card.NameZh),
			FString::Printf(TEXT("Recycled %s"), *Card.NameEn),
			EEventType::Echo, Card.GetElement(), Card.Tier);
		RefillHand();
		return true;
	}

	int32 FSimKernel::CollectRadius(int32 CenterTile, float RadiusMeters, TArray<int32>& OutTiles) const
	{
		OutTiles.Reset();
		if (!Tiles.IsValidIndex(CenterTile))
		{
			return 0;
		}
		const int32 Size = Config.Size;
		const int32 CX = CenterTile % Size;
		const int32 CY = CenterTile / Size;
		const int32 Radius = FMath::Max(0, FMath::CeilToInt(RadiusMeters));
		const float RadiusSq = RadiusMeters * RadiusMeters;

		for (int32 Y = CY - Radius; Y <= CY + Radius; ++Y)
		{
			for (int32 X = CX - Radius; X <= CX + Radius; ++X)
			{
				if (!InBounds(X, Y))
				{
					continue;
				}
				const float DX = static_cast<float>(X - CX);
				const float DY = static_cast<float>(Y - CY);
				if (DX * DX + DY * DY <= RadiusSq)
				{
					OutTiles.Add(ToIndex(X, Y));
				}
			}
		}
		return OutTiles.Num();
	}

	float FSimKernel::ComputeOffset(int32 AffectedTileCount, float MaxHeightDelta, float BiomassDeltaRatio) const
	{
		// docs/CARDS.md SS2.1.1
		const float RealmTiles = FMath::Max(1.0f, static_cast<float>(Tiles.Num()));
		const float AreaTerm = FMath::Sqrt(FMath::Clamp(static_cast<float>(AffectedTileCount) / RealmTiles, 0.0f, 1.0f));
		const float HeightTerm = FMath::Clamp(FMath::Abs(MaxHeightDelta) / Tuning::MaxHeightMeters, 0.0f, 1.0f);
		const float BioTerm = FMath::Clamp(FMath::Abs(BiomassDeltaRatio), 0.0f, 1.0f);
		return 0.5f * AreaTerm + 0.3f * HeightTerm + 0.2f * BioTerm;
	}

	int32 FSimKernel::AwardEventEcho(float Offset, const FCardDef& Card)
	{
		int32 Bracket = 0;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			if (Offset >= Tuning::OffsetBracket[Index][0] && Offset < Tuning::OffsetBracket[Index][1])
			{
				Bracket = Index;
				break;
			}
		}

		const int32 CardIndex = Catalog.FindCard(Card.Id);
		int32 Base = Rng.Stream(ERngStream::Event).Range(Tuning::EchoBracketMin[Bracket], Tuning::EchoBracketMax[Bracket] + 1);

		// Gate two: same-kind cooldown inside one world day (ECONOMY SS2.1).
		if (DayReleaseCount.IsValidIndex(CardIndex))
		{
			const int32 Repeat = DayReleaseCount[CardIndex];
			if (Repeat == 1)
			{
				Base = Base / 2;
			}
			else if (Repeat == 2)
			{
				Base = Base / 4;
			}
			else if (Repeat >= 3)
			{
				Base = 0;
			}
			++DayReleaseCount[CardIndex];
		}

		// Gate three: per-real-minute ceiling; the overflow pays a quarter (ECONOMY SS2.1 rule 3).
		int32 Awarded = Base;
		const int32 Headroom = FMath::Max(0, Tuning::EchoPerMinuteCap - EchoThisMinute);
		if (Awarded > Headroom)
		{
			const int32 Over = Awarded - Headroom;
			Awarded = Headroom + FMath::RoundToInt(static_cast<float>(Over) * Tuning::EchoOverCapMultiplier);
		}
		EchoThisMinute += Base;
		EchoMinuteBucket = RealMinuteIndex;

		if (Awarded > 0)
		{
			Ledger.Balance += Awarded;
			Ledger.FromEvent += Awarded;
		}
		return Awarded;
	}

	int32 FSimKernel::AwardFirsthand(const FString& Key, int32 Amount, EEventType Type, const FString& Zh, const FString& En)
	{
		if (FirsthandKeys.Contains(Key))
		{
			return 0;	// gate five: one-off sources fire once (ECONOMY SS6)
		}
		FirsthandKeys.Add(Key);
		Ledger.Balance += Amount;
		Ledger.FromFirsthand += Amount;
		Note(Zh, En, Type);
		return Amount;
	}

	void FSimKernel::ApplyEffect(const FCardDef& Card, int32 TargetTile, bool bSettleAsEntity, float& OutOffset, TArray<int32>& OutAffected)
	{
		OutOffset = 0.0f;
		OutAffected.Reset();
		if (!Tiles.IsValidIndex(TargetTile))
		{
			return;
		}

		TArray<int32> Affected;
		const float Radius = FMath::Max(0.0f, Card.Param(TEXT("radius"), 1.0f));
		const float Peak = Card.Param(TEXT("peak"), 1.0f);
		const float DeltaTemp = Card.Param(TEXT("delta_temp"), 0.0f);
		const float WaterDelta = Card.Param(TEXT("water_delta"), 0.0f);
		const float Strength = Card.Param(TEXT("strength"), 1.0f);
		const int32 PaintedMaterial = FMath::Clamp(FMath::RoundToInt(Card.Param(TEXT("mat"), 1.0f)), 0, static_cast<int32>(EMaterial::Count) - 1);
		const int32 SpeciesToSeed = FMath::Clamp(FMath::RoundToInt(Card.Param(TEXT("species"), 2.0f)), 0, static_cast<int32>(ESpeciesKind::Count) - 1);
		const int32 SpawnCount = FMath::Clamp(FMath::RoundToInt(Card.Param(TEXT("count"), 4.0f)), 1, 64);

		const int32 Size = Config.Size;
		const int32 TX = TargetTile % Size;
		const int32 TY = TargetTile / Size;
		float MaxHeightDelta = 0.0f;
		const float BiomassBefore = static_cast<float>(Individuals.Num());

		switch (Card.GetEffect())
		{
		case EEffect::RaiseTerrain:
		case EEffect::LowerTerrain:
		{
			CollectRadius(TargetTile, Radius, Affected);
			const float Sign = (Card.GetEffect() == EEffect::RaiseTerrain) ? 1.0f : -1.0f;
			for (int32 Index = 0; Index < Affected.Num(); ++Index)
			{
				FTile& Tile = Tiles[Affected[Index]];
				const float DX = static_cast<float>(Affected[Index] % Size - TX);
				const float DY = static_cast<float>(Affected[Index] / Size - TY);
				const float Normalised = FMath::Clamp(1.0f - FMath::Sqrt(DX * DX + DY * DY) / FMath::Max(0.5f, Radius), 0.0f, 1.0f);
				const float Delta = Sign * Peak * Normalised * Normalised;
				Tile.Height = FMath::Clamp(FMath::RoundToFloat((Tile.Height + Delta) / Tuning::HeightQuantum) * Tuning::HeightQuantum,
					0.0f, Tuning::MaxHeightMeters);
				MaxHeightDelta = FMath::Max(MaxHeightDelta, FMath::Abs(Delta));
				MarkDirtyAt(Affected[Index]);
			}
			break;
		}
		case EEffect::FlattenTerrain:
		case EEffect::ErodeSurface:
		{
			CollectRadius(TargetTile, Radius, Affected);
			float Sum = 0.0f;
			for (int32 Index = 0; Index < Affected.Num(); ++Index)
			{
				Sum += Tiles[Affected[Index]].Height;
			}
			const float Mean = Affected.Num() > 0 ? Sum / static_cast<float>(Affected.Num()) : Tiles[TargetTile].Height;
			const float Blend = (Card.GetEffect() == EEffect::FlattenTerrain) ? 1.0f : 0.6f;
			for (int32 Index = 0; Index < Affected.Num(); ++Index)
			{
				FTile& Tile = Tiles[Affected[Index]];
				const float Before = Tile.Height;
				Tile.Height = FMath::RoundToFloat(FMath::Lerp(Tile.Height, Mean, Blend) / Tuning::HeightQuantum) * Tuning::HeightQuantum;
				MaxHeightDelta = FMath::Max(MaxHeightDelta, FMath::Abs(Tile.Height - Before));
				MarkDirtyAt(Affected[Index]);
			}
			break;
		}
		case EEffect::RidgeLine:
		{
			// Brush scope: a ridge running away from the cursor along +X.
			const int32 Length = FMath::Max(2, FMath::RoundToInt(Radius));
			for (int32 Step = 0; Step < Length; ++Step)
			{
				const int32 X = TX + Step;
				if (X >= Size)
				{
					break;
				}
				const float Falloff = 1.0f - static_cast<float>(Step) / static_cast<float>(Length);
				for (int32 W = -1; W <= 1; ++W)
				{
					const int32 Y = TY + W;
					if (!InBounds(X, Y))
					{
						continue;
					}
					FTile& Tile = Tiles[ToIndex(X, Y)];
					const float Delta = Peak * Falloff * (W == 0 ? 1.0f : 0.55f);
					Tile.Height = FMath::Clamp(FMath::RoundToFloat((Tile.Height + Delta) / Tuning::HeightQuantum) * Tuning::HeightQuantum,
						0.0f, Tuning::MaxHeightMeters);
					MaxHeightDelta = FMath::Max(MaxHeightDelta, FMath::Abs(Delta));
					Affected.Add(ToIndex(X, Y));
					MarkDirtyAt(ToIndex(X, Y));
				}
			}
			break;
		}
		case EEffect::PaintSurface:
		{
			CollectRadius(TargetTile, FMath::Max(1.0f, Radius), Affected);
			for (int32 Index = 0; Index < Affected.Num(); ++Index)
			{
				FTile& Tile = Tiles[Affected[Index]];
				Tile.Mat = static_cast<uint8>(PaintedMaterial);
				Tile.Painted = 1.0f;
				MarkDirtyAt(Affected[Index]);
			}
			break;
		}
		case EEffect::Ignite:
		{
			CollectRadius(TargetTile, FMath::Max(0.5f, Radius), Affected);
			for (int32 Index = 0; Index < Affected.Num(); ++Index)
			{
				FTile& Tile = Tiles[Affected[Index]];
				const FMaterialProps& Props = PropsOf(Tile.GetMat());
				if (Props.Combustibility > 0.0f || Tile.Vegetation > 0.05f)
				{
					Tile.Burning = 1.0f;
					if (Tile.GetMat() == EMaterial::Bare || Tile.GetMat() == EMaterial::LivePlant)
					{
						Tile.Mat = static_cast<uint8>(EMaterial::DeadWood);
					}
					MarkDirtyAt(Affected[Index]);
				}
			}
			break;
		}
		case EEffect::HeatArea:
		case EEffect::ChillArea:
		{
			CollectRadius(TargetTile, FMath::Max(1.0f, Radius), Affected);
			const float Sign = (Card.GetEffect() == EEffect::HeatArea) ? 1.0f : -1.0f;
			for (int32 Index = 0; Index < Affected.Num(); ++Index)
			{
				FTile& Tile = Tiles[Affected[Index]];
				Tile.LocalTempC += Sign * DeltaTemp;
				if (Sign > 0.0f && Tile.LocalTempC > 700.0f)
				{
					Tile.Mat = static_cast<uint8>(EMaterial::Magma);
				}
				MarkDirtyAt(Affected[Index]);
			}
			MaxHeightDelta = FMath::Abs(DeltaTemp) / 10.0f;
			break;
		}
		case EEffect::WaterLevel:
		{
			Sky.WaterLevel = FMath::Clamp(Sky.WaterLevel + WaterDelta, 0.0f, Tuning::MaxHeightMeters * 0.5f);
			RecomputeWaterBodies();
			MarkAllDirty();
			Affected.SetNum(Tiles.Num());
			MaxHeightDelta = FMath::Abs(WaterDelta);
			break;
		}
		case EEffect::OreVein:
		{
			const int32 Length = FMath::Max(2, FMath::RoundToInt(Radius));
			for (int32 Step = 0; Step < Length; ++Step)
			{
				const int32 X = TX + Step;
				const int32 Y = TY + ((Step % 3) - 1);
				if (!InBounds(X, Y))
				{
					continue;
				}
				FTile& Tile = Tiles[ToIndex(X, Y)];
				Tile.Mat = static_cast<uint8>(EMaterial::Ore);
				Tile.Painted = 1.0f;
				Tile.Fertility = FMath::Max(0.0f, Tile.Fertility - 0.1f);
				Affected.Add(ToIndex(X, Y));
				MarkDirtyAt(ToIndex(X, Y));
			}
			break;
		}
		case EEffect::SeedLife:
		{
			CollectRadius(TargetTile, FMath::Max(2.0f, Radius), Affected);
			const bool bAquatic = (static_cast<ESpeciesKind>(SpeciesToSeed) == ESpeciesKind::StreamFish);
			int32 Placed = 0;
			for (int32 Index = 0; Index < Affected.Num() && Placed < SpawnCount; ++Index)
			{
				const FTile& Tile = Tiles[Affected[Index]];
				if (bAquatic != Tile.IsWater())
				{
					continue;
				}
				SpawnIndividual(static_cast<ESpeciesKind>(SpeciesToSeed), Affected[Index]);
				++Placed;
			}
			bChaosState = false;
			break;
		}
		case EEffect::RecordPillar:
		{
			FTile& Tile = Tiles[TargetTile];
			Tile.Mat = static_cast<uint8>(EMaterial::Stone);
			Tile.Painted = 1.0f;
			Affected.Add(TargetTile);
			MarkDirtyAt(TargetTile);
			Note(FString::Printf(TEXT("你立起了一根纪年柱：「%s」"), *Card.NameZh),
				FString::Printf(TEXT("You raise a chronicle pillar: %s"), *Card.NameEn),
				EEventType::Terrain, Card.GetElement(), Card.Tier);
			break;
		}
		case EEffect::WeatherPush:
		{
			// Nudge the state machine: humidity and cloud cover step toward the card's target.
			const float TargetHumidity = FMath::Clamp(Card.Param(TEXT("humidity"), 0.7f), 0.0f, 1.0f);
			Sky.Humidity = FMath::Clamp(FMath::Lerp(Sky.Humidity, TargetHumidity, Strength), 0.02f, 0.99f);
			Sky.CloudCover = FMath::Clamp(FMath::Lerp(Sky.CloudCover, Card.Param(TEXT("cloud"), 0.6f), Strength), 0.0f, 1.0f);
			const int32 ForceWeather = FMath::RoundToInt(Card.Param(TEXT("weather"), -1.0f));
			if (ForceWeather >= 0 && ForceWeather < static_cast<int32>(EWeather::Count))
			{
				Sky.Weather = static_cast<uint8>(ForceWeather);
			}
			Affected.SetNum(Tiles.Num());
			break;
		}
		default:
			break;
		}

		// Landed-as-entity cards leave a permanent mark on the chronicle (INTERVENTION SS5.1).
		if (bSettleAsEntity && Affected.Num() > 0)
		{
			Note(FString::Printf(TEXT("「%s」落地成实体"), *Card.NameZh),
				FString::Printf(TEXT("%s settles as a permanent thing"), *Card.NameEn),
				EEventType::Terrain, Card.GetElement(), Card.Tier);
		}

		const float BiomassAfter = static_cast<float>(Individuals.Num());
		const float BiomassDelta = (BiomassBefore > 0.0f) ? (BiomassAfter - BiomassBefore) / FMath::Max(1.0f, BiomassBefore) : 0.0f;
		OutOffset = ComputeOffset(Affected.Num(), MaxHeightDelta, BiomassDelta);
		OutAffected = Affected;
	}

	bool FSimKernel::ReleaseCard(int32 HandSlot, int32 TargetTile, bool bSettleAsEntity, FReleaseResult& Out)
	{
		Out = FReleaseResult();
		if (!Hand.IsValidIndex(HandSlot) || Hand[HandSlot] == INDEX_NONE)
		{
			Out.ReasonZh = TEXT("这个格子是空的");
			Out.ReasonEn = TEXT("That slot is empty");
			return false;
		}
		if (!Tiles.IsValidIndex(TargetTile))
		{
			Out.ReasonZh = TEXT("落点不在界里");
			Out.ReasonEn = TEXT("That landing spot is outside the realm");
			return false;
		}

		const int32 CardIndex = Hand[HandSlot];
		const FCardDef& Card = Catalog.Cards[CardIndex];

		float Offset = 0.0f;
		TArray<int32> Affected;
		ApplyEffect(Card, TargetTile, bSettleAsEntity, Offset, Affected);

		// Wet-paper window: a card that landed as a permanent entity can be pulled back within
		// three real seconds (docs/CARDS.md SS1.1). State-scoped effects get none.
		bUndoAvailable = false;
		if (bSettleAsEntity && Affected.Num() > 0 && Affected.Num() <= 4096)
		{
			UndoSnapshot.Reset();
			UndoTiles = Affected;
			UndoSnapshot.Reserve(Affected.Num());
			for (int32 Index = 0; Index < Affected.Num(); ++Index)
			{
				UndoSnapshot.Add(Tiles[Affected[Index]]);
			}
			UndoSecondsLeft = 3.0;
			bUndoAvailable = true;
			UndoCardNameZh = Card.NameZh;
			UndoCardNameEn = Card.NameEn;
			Out.bCanUndo = true;
		}
		Out.AffectedTiles = Affected;

		// Release consumes one physical copy; 0-tier is free and unlimited.
		if (Card.Tier != 0 && Pool.IsValidIndex(CardIndex) && Pool[CardIndex] > 0)
		{
			Pool[CardIndex] -= 1;
		}
		Hand[HandSlot] = INDEX_NONE;

		// Disturbance: every change pushes the realm off its natural baseline (PHYSICS SS8).
		Disturbance = FMath::Clamp(Disturbance + Offset * 45.0f, 0.0f, 100.0f);

		const int32 Echo = AwardEventEcho(Offset, Card);
		Out.bOk = true;
		Out.Offset = Offset;
		Out.EchoGained = Echo;
		Out.bCrossedThreshold = (Echo > 0);

		if (Echo > 0)
		{
			Note(FString::Printf(TEXT("释放「%s」"), *Card.NameZh),
				FString::Printf(TEXT("Released %s"), *Card.NameEn),
				EEventType::Terrain, Card.GetElement(), Card.Tier);
		}
		else
		{
			Note(FString::Printf(TEXT("释放「%s」（未跨阈值）"), *Card.NameZh),
				FString::Printf(TEXT("Released %s (no threshold crossed)"), *Card.NameEn),
				EEventType::Terrain, Card.GetElement(), Card.Tier);
		}

		RefillHand();
		return true;
	}

	int32 FSimKernel::GetCardValueForTier(int32 Tier) const
	{
		return Tuning::CardValue[FMath::Clamp(Tier, 0, 11)];
	}

	bool FSimKernel::UndoPendingSettle(FString& OutReasonZh, FString& OutReasonEn)
	{
		if (!bUndoAvailable || UndoTiles.Num() != UndoSnapshot.Num() || UndoTiles.Num() == 0)
		{
			OutReasonZh = TEXT("纸已经贴牢了");
			OutReasonEn = TEXT("The paper has already set");
			return false;
		}
		for (int32 Index = 0; Index < UndoTiles.Num(); ++Index)
		{
			if (Tiles.IsValidIndex(UndoTiles[Index]))
			{
				Tiles[UndoTiles[Index]] = UndoSnapshot[Index];
				MarkDirtyAt(UndoTiles[Index]);
			}
		}
		Note(FString::Printf(TEXT("你揭回了「%s」——纸还没贴牢"), *UndoCardNameZh),
			FString::Printf(TEXT("You peel back %s while the paper is still wet"), *UndoCardNameEn),
			EEventType::Terrain);
		bUndoAvailable = false;
		UndoSnapshot.Reset();
		UndoTiles.Reset();
		return true;
	}

	// ======================================================================
	// Packs and economy
	// ======================================================================

	int32 FSimKernel::GetPackPrice(EPackKind Kind) const
	{
		switch (Kind)
		{
		case EPackKind::SpecificElement: return Tuning::PackPriceSpecific;
		case EPackKind::Special:         return Tuning::PackPriceSpecial;
		default:                          return Tuning::PackPriceRandom;
		}
	}

	bool FSimKernel::AreSpecialPacksAvailable() const
	{
		// Special packs are bound to the real calendar (docs/CARDS.md SS5.2).
		return IsNearSolsticeOrEquinox(FMath::Clamp(Reality.Month, 1, 12), FMath::Clamp(Reality.Day, 1, 31));
	}

	int32 FSimKernel::CalmDisturbance(int32 Points, int32& OutCost)
	{
		const int32 Wanted = FMath::Clamp(Points, 0, 100);
		const int32 Available = FMath::FloorToInt(Disturbance);
		const int32 Applied = FMath::Min(Wanted, Available);
		OutCost = (Applied + 1) / 2;	// 1 echo per 2 points (docs/CARDS.md SS4)
		if (OutCost > Ledger.Balance)
		{
			OutCost = Ledger.Balance;
		}
		const int32 CanDo = OutCost * 2;
		Disturbance = FMath::Max(0.0f, Disturbance - static_cast<float>(CanDo));
		Ledger.Balance -= OutCost;
		Ledger.SpentCalm += OutCost;
		return CanDo;
	}

	bool FSimKernel::OpenPack(EPackKind Kind, uint8 ElementFilter, FPackResult& Out)
	{
		Out = FPackResult();
		const int32 Price = GetPackPrice(Kind);
		if (Ledger.Balance < Price)
		{
			return false;
		}
		if (Kind == EPackKind::Special && !AreSpecialPacksAvailable())
		{
			return false;
		}

		// Element candidates: M0 ships earth / fire / metal.
		TArray<uint8> ElementChoices;
		if (Kind == EPackKind::SpecificElement && ElementFilter != static_cast<uint8>(EElement::None))
		{
			ElementChoices.Add(ElementFilter);
		}
		else
		{
			for (int32 Index = 0; Index < Catalog.Cards.Num(); ++Index)
			{
				const uint8 Element = Catalog.Cards[Index].Element;
				if (Element != static_cast<uint8>(EElement::None) && !ElementChoices.Contains(Element))
				{
					ElementChoices.Add(Element);
				}
			}
		}
		if (ElementChoices.Num() == 0)
		{
			return false;
		}

		Ledger.Balance -= Price;
		Ledger.SpentPacks += Price;

		const int32 CardCount = Rng.Stream(ERngStream::Loot).Range(3, 6);	// 3..5 inclusive
		for (int32 Draw = 0; Draw < CardCount; ++Draw)
		{
			const uint8 Element = ElementChoices[Rng.Stream(ERngStream::Loot).Range(0, ElementChoices.Num())];

			// Tier from the drop weight table (docs/CARDS.md SS5.1).
			float TotalWeight = 0.0f;
			for (int32 Tier = 0; Tier < 10; ++Tier)
			{
				TotalWeight += Tuning::TierWeight[Tier];
			}
			float Roll = Rng.Stream(ERngStream::Loot).RangeFloat(0.0f, TotalWeight);
			int32 ChosenTier = 0;
			for (int32 Tier = 0; Tier < 10; ++Tier)
			{
				Roll -= Tuning::TierWeight[Tier];
				if (Roll <= 0.0f)
				{
					ChosenTier = Tier;
					break;
				}
			}

			// Pick a card of that element and tier; prefer one this realm can actually use.
			TArray<int32> Candidates;
			for (int32 Index = 0; Index < Catalog.Cards.Num(); ++Index)
			{
				const FCardDef& Candidate = Catalog.Cards[Index];
				if (Candidate.Element == Element && Candidate.Tier == ChosenTier && !Candidate.IsForbidden())
				{
					Candidates.Add(Index);
				}
			}
			if (Candidates.Num() == 0)
			{
				continue;
			}
			const int32 Picked = Candidates[Rng.Stream(ERngStream::Loot).Range(0, Candidates.Num())];

			// Overflow past the holding cap converts to echo at 20% (ECONOMY SS3.3).
			if (ChosenTier == 0)
			{
				Pool[Picked] = FMath::Min(Pool[Picked] + 1, Tuning::MaxCopiesPerKind);
			}
			else if (Pool[Picked] >= Tuning::MaxCopiesPerKind)
			{
				const int32 Converted = FMath::RoundToInt(Tuning::CardValue[ChosenTier] * 0.2f);
				Ledger.Balance += Converted;
				Ledger.FromAutoConvert += Converted;
				Out.ConvertedEcho += Converted;
			}
			else
			{
				Pool[Picked] += 1;
				Unlocked[Picked] = 1;
			}
			Out.Cards.Add(Picked);
		}

		// Over-tier: special packs only, at most one per pack (ECONOMY SS5.2).
		if (Kind == EPackKind::Special && Rng.Stream(ERngStream::Loot).Chance(Tuning::SpecialPackOverTierChance))
		{
			const uint8 Element = ElementChoices[Rng.Stream(ERngStream::Loot).Range(0, ElementChoices.Num())];
			for (int32 Index = 0; Index < Catalog.Cards.Num(); ++Index)
			{
				if (Catalog.Cards[Index].IsOverTier() && Catalog.Cards[Index].Element == Element)
				{
					Pool[Index] = FMath::Min(Pool[Index] + 1, Tuning::MaxCopiesPerKind);
					Unlocked[Index] = 1;
					Out.Cards.Add(Index);
					break;
				}
			}
		}

		Out.Cost = Price;
		Out.TitleZh = (Kind == EPackKind::Special) ? TEXT("特殊包") : ((Kind == EPackKind::SpecificElement) ? TEXT("指定元素包") : TEXT("元素包"));
		Out.TitleEn = (Kind == EPackKind::Special) ? TEXT("Special Pack") : ((Kind == EPackKind::SpecificElement) ? TEXT("Chosen Element Pack") : TEXT("Element Pack"));

		Note(FString::Printf(TEXT("开了一个%s，得 %d 张"), *Out.TitleZh, Out.Cards.Num()),
			FString::Printf(TEXT("Opened a %s, %d cards"), *Out.TitleEn, Out.Cards.Num()),
			EEventType::Echo);
		RefillHand();
		return true;
	}

	// ======================================================================
	// Chronicle
	// ======================================================================

	void FSimKernel::PushChronicle(const FChronicleEntry& Entry)
	{
		Chronicle.Add(Entry);
		while (Chronicle.Num() > Tuning::ChronicleCapacity)
		{
			Chronicle.RemoveAt(0);
		}
	}

	void FSimKernel::Note(const FString& Zh, const FString& En, EEventType Type, EElement Element, int32 Tier,
		const FString& SubjectZh, const FString& SubjectEn)
	{
		FChronicleEntry Entry;
		Entry.WorldYear = GetWorldYear();
		Entry.WorldDay = GetWorldDay();
		Entry.MinuteOfDay = static_cast<int32>(WorldMinute % 1440);
		Entry.RealYear = Reality.Year;
		Entry.RealMonth = Reality.Month;
		Entry.RealDay = Reality.Day;
		Entry.Type = static_cast<uint8>(Type);
		Entry.Element = static_cast<uint8>(Element);
		Entry.Tier = (Tier < 0 || Tier > 255) ? 255 : static_cast<uint8>(Tier);
		Entry.SubjectZh = SubjectZh;
		Entry.SubjectEn = SubjectEn;
		Entry.SummaryZh = Zh;
		Entry.SummaryEn = En;
		PushChronicle(Entry);
	}

	// ======================================================================
	// Readouts
	// ======================================================================

	bool FSimKernel::InBounds(int32 X, int32 Y) const
	{
		return X >= 0 && Y >= 0 && X < Config.Size && Y < Config.Size;
	}

	void FSimKernel::IndexToXY(int32 Index, int32& OutX, int32& OutY) const
	{
		OutX = Index % Config.Size;
		OutY = Index / Config.Size;
	}

	const FTile* FSimKernel::TileAt(int32 X, int32 Y) const
	{
		return InBounds(X, Y) ? &Tiles[ToIndex(X, Y)] : nullptr;
	}

	float FSimKernel::GetHeightAt(int32 X, int32 Y) const
	{
		const FTile* Tile = TileAt(X, Y);
		return Tile ? Tile->Height : 0.0f;
	}

	bool FSimKernel::IsWaterTile(int32 X, int32 Y) const
	{
		const FTile* Tile = TileAt(X, Y);
		return Tile ? Tile->IsWater() : false;
	}

	int32 FSimKernel::GetWorldDay() const
	{
		return static_cast<int32>((WorldMinute / 1440) % 360) + 1;
	}

	int32 FSimKernel::GetWorldYear() const
	{
		return static_cast<int32>(WorldMinute / (1440 * 360)) + 1;
	}

	float FSimKernel::GetDayFraction() const
	{
		return static_cast<float>(WorldMinute % 1440) / 1440.0f;
	}

	FString FSimKernel::GetClockString() const
	{
		return FString::Printf(TEXT("第 %d 年 · %d 日 · %02d:%02d"), GetWorldYear(), GetWorldDay(), GetWorldHour(), GetWorldMinuteOfHour());
	}

	FString FSimKernel::GetRealDateString() const
	{
		return FString::Printf(TEXT("%04d-%02d-%02d"), Reality.Year, Reality.Month, Reality.Day);
	}

	FString FSimKernel::GetSeasonString() const
	{
		const int32 Month = FMath::Clamp(Reality.Month, 1, 12);
		if (Month >= 3 && Month <= 5)
		{
			return TEXT("春");
		}
		if (Month >= 6 && Month <= 8)
		{
			return TEXT("夏");
		}
		if (Month >= 9 && Month <= 11)
		{
			return TEXT("秋");
		}
		return TEXT("冬");
	}

	int32 FSimKernel::GetDeathsByCause(EDeathCause Cause) const
	{
		const int32 Index = static_cast<int32>(Cause);
		return (Index >= 0 && Index < static_cast<int32>(EDeathCause::Count)) ? DeathCauseCounts[Index] : 0;
	}

	int32 FSimKernel::GetPopulation(ESpeciesKind Kind) const
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Individuals.Num(); ++Index)
		{
			if (Individuals[Index].GetKind() == Kind)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 FSimKernel::GetPopulationTotal() const
	{
		return Individuals.Num();
	}

	float FSimKernel::GetVegetationMean() const
	{
		if (Tiles.Num() == 0)
		{
			return 0.0f;
		}
		double Sum = 0.0;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			Sum += Tiles[Index].Vegetation;
		}
		return static_cast<float>(Sum / static_cast<double>(Tiles.Num()));
	}

	float FSimKernel::GetFertilityMean() const
	{
		if (Tiles.Num() == 0)
		{
			return 0.0f;
		}
		double Sum = 0.0;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			Sum += Tiles[Index].Fertility;
		}
		return static_cast<float>(Sum / static_cast<double>(Tiles.Num()));
	}

	float FSimKernel::GetPaintedFraction() const
	{
		if (Tiles.Num() == 0)
		{
			return 0.0f;
		}
		double Sum = 0.0;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			Sum += Tiles[Index].Painted;
		}
		return static_cast<float>(Sum / static_cast<double>(Tiles.Num()));
	}

	float FSimKernel::GetBiomassNorm() const
	{
		const float Budget = static_cast<float>(Tuning::IndividualBudgetSmall);
		const float FromCreatures = FMath::Clamp(static_cast<float>(Individuals.Num()) / Budget, 0.0f, 1.0f);
		const float FromPlants = FMath::Clamp(GetVegetationMean(), 0.0f, 1.0f);
		return FMath::Clamp(0.5f * FromCreatures + 0.5f * FromPlants, 0.0f, 1.0f);
	}

	float FSimKernel::GetDiversityIndex() const
	{
		const int32 Total = Individuals.Num();
		if (Total <= 0)
		{
			return 0.0f;
		}
		int32 Counts[static_cast<int32>(ESpeciesKind::Count)] = { 0 };
		for (int32 Index = 0; Index < Total; ++Index)
		{
			Counts[FMath::Clamp(static_cast<int32>(Individuals[Index].GetKind()), 0, static_cast<int32>(ESpeciesKind::Count) - 1)] += 1;
		}
		int32 Present = 0;
		float SumSquares = 0.0f;
		for (int32 Index = 0; Index < static_cast<int32>(ESpeciesKind::Count); ++Index)
		{
			if (Counts[Index] > 0)
			{
				++Present;
				const float Share = static_cast<float>(Counts[Index]) / static_cast<float>(Total);
				SumSquares += Share * Share;
			}
		}
		if (Present == 0 || SumSquares <= 1.0e-4f)
		{
			return 0.0f;
		}
		const float Simpson = 1.0f / SumSquares;
		return FMath::Clamp(Simpson / static_cast<float>(static_cast<int32>(ESpeciesKind::Count)), 0.0f, 1.0f);
	}

	float FSimKernel::GetStabilityIndex() const
	{
		return FMath::Clamp(1.0f - Disturbance / 150.0f, 0.0f, 1.0f);
	}

	float FSimKernel::GetEcoPotential() const
	{
		float Potential = GetBiomassNorm() * GetDiversityIndex() * GetStabilityIndex();
		if (Disturbance > Tuning::DisturbancePenaltyThreshold)
		{
			Potential *= 0.5f;	// gate four (docs/CARDS.md SS6)
		}
		return FMath::Clamp(Potential, 0.0f, 1.0f);
	}

	int32 FSimKernel::GetSedimentPerHourEstimate() const
	{
		return FMath::RoundToInt(GetEcoPotential() * Tuning::SedimentCoefficient * 60.0f);
	}

	// ======================================================================
	// World constants (creator panel)
	// ======================================================================

	void FSimKernel::SetWorldConstant(const FString& Key, float Value)
	{
		WorldConstants.Add(Key, Value);
		Note(FString::Printf(TEXT("法则参数「%s」改为 %.2f"), *Key, Value),
			FString::Printf(TEXT("World constant %s set to %.2f"), *Key, Value),
			EEventType::Trigger);
	}

	float FSimKernel::GetWorldConstant(const FString& Key, float Fallback) const
	{
		const float* Found = WorldConstants.Find(Key);
		return Found ? *Found : Fallback;
	}

	// ======================================================================
	// Dirty tracking
	// ======================================================================

	void FSimKernel::MarkDirtyAt(int32 TileIndex)
	{
		if (!Tiles.IsValidIndex(TileIndex))
		{
			return;
		}
		const int32 ChunkSize = Config.ChunkSize;
		const int32 CX = FMath::Clamp((TileIndex % Config.Size) / ChunkSize, 0, ChunksPerAxis - 1);
		const int32 CY = FMath::Clamp((TileIndex / Config.Size) / ChunkSize, 0, ChunksPerAxis - 1);
		const int32 ChunkIndex = CY * ChunksPerAxis + CX;
		if (RegionDirty.IsValidIndex(ChunkIndex))
		{
			RegionDirty[ChunkIndex] = 1;
		}
		// Seams: a change on a chunk border also dirties the neighbour (WORLD SS3.4).
		const int32 LocalX = (TileIndex % Config.Size) % ChunkSize;
		const int32 LocalY = (TileIndex / Config.Size) % ChunkSize;
		auto Touch = [this](int32 X, int32 Y)
		{
			if (X < 0 || Y < 0 || X >= ChunksPerAxis || Y >= ChunksPerAxis)
			{
				return;
			}
			const int32 Index = Y * ChunksPerAxis + X;
			if (RegionDirty.IsValidIndex(Index))
			{
				RegionDirty[Index] = 1;
			}
		};
		if (LocalX == 0)
		{
			Touch(CX - 1, CY);
		}
		if (LocalY == 0)
		{
			Touch(CX, CY - 1);
		}
		if (LocalX == ChunkSize - 1)
		{
			Touch(CX + 1, CY);
		}
		if (LocalY == ChunkSize - 1)
		{
			Touch(CX, CY + 1);
		}
	}

	bool FSimKernel::IsRegionDirty(int32 ChunkX, int32 ChunkY) const
	{
		if (ChunkX < 0 || ChunkY < 0 || ChunkX >= ChunksPerAxis || ChunkY >= ChunksPerAxis)
		{
			return false;
		}
		const int32 Index = ChunkY * ChunksPerAxis + ChunkX;
		return RegionDirty.IsValidIndex(Index) && RegionDirty[Index] != 0;
	}

	void FSimKernel::ClearRegionDirty(int32 ChunkX, int32 ChunkY)
	{
		if (ChunkX < 0 || ChunkY < 0 || ChunkX >= ChunksPerAxis || ChunkY >= ChunksPerAxis)
		{
			return;
		}
		const int32 Index = ChunkY * ChunksPerAxis + ChunkX;
		if (RegionDirty.IsValidIndex(Index))
		{
			RegionDirty[Index] = 0;
		}
	}

	bool FSimKernel::IsRegionEverBuilt(int32 ChunkX, int32 ChunkY) const
	{
		if (ChunkX < 0 || ChunkY < 0 || ChunkX >= ChunksPerAxis || ChunkY >= ChunksPerAxis)
		{
			return true;
		}
		const int32 Index = ChunkY * ChunksPerAxis + ChunkX;
		return RegionBuilt.IsValidIndex(Index) && RegionBuilt[Index] != 0;
	}

	void FSimKernel::MarkRegionBuilt(int32 ChunkX, int32 ChunkY)
	{
		if (ChunkX < 0 || ChunkY < 0 || ChunkX >= ChunksPerAxis || ChunkY >= ChunksPerAxis)
		{
			return;
		}
		const int32 Index = ChunkY * ChunksPerAxis + ChunkX;
		if (RegionBuilt.IsValidIndex(Index))
		{
			RegionBuilt[Index] = 1;
		}
	}

	void FSimKernel::MarkAllDirty()
	{
		for (int32 Index = 0; Index < RegionDirty.Num(); ++Index)
		{
			RegionDirty[Index] = 1;
		}
		bStructureDirty = true;
	}

	bool FSimKernel::ConsumeStructureChanged()
	{
		const bool bWasDirty = bStructureDirty;
		bStructureDirty = false;
		return bWasDirty;
	}
}
