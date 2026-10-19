// Solstice - Sim layer: plain data types shared by the whole game.
//
// HARD RULE (docs/TECH.md SS7.1): nothing in Sim/ may derive from UObject, and
// Sim/ may not depend on Tick / rendering / input. Everything here is a plain struct or a
// plain enum class, so the kernel can be driven headless and unit-checked.
//
// Design truth: docs/CHARTER.md (owner) + docs/setting/* + docs/gameplay/*.

#pragma once

#include "CoreMinimal.h"

namespace Solstice
{
	// ------------------------------------------------------------------
	// Enumerations
	// ------------------------------------------------------------------

	/** Seven primal elements (docs/WORLD.md SS1.2). Fixed set, never extended. */
	enum class EElement : uint8
	{
		None = 0,
		Metal,	// metal
		Wood,
		Water,
		Fire,
		Earth,
		Light,
		Dark,
		Count
	};

	/** Card class: what the card *is* in the world (docs/CARDS.md SS4). */
	enum class ECardClass : uint8
	{
		Matter = 0,	// object
		Rite,		// active effect
		Law,		// written rule
		Count
	};

	/** Persistence field (docs/CARDS.md SS6). */
	enum class EDuration : uint8
	{
		Instant = 0,
		Sustain,
		Permanent,
		Trigger,
		Count
	};

	enum class EDirection : uint8
	{
		Create = 0,
		Remove,
		Transform,
		Count
	};

	enum class EScope : uint8
	{
		Global = 0,
		Area,
		Single,
		Brush,
		Count
	};

	/** Dual time mode (docs/WORLD.md SS2). */
	enum class ETimeMode : uint8
	{
		Virtual = 0,
		Real,
		Count
	};

	/** M0 minimal weather state machine (docs/WORLD.md SS3.2). */
	enum class EWeather : uint8
	{
		Clear = 0,
		Cloudy,
		LightRain,
		Storm,
		Fog,
		Count
	};

	/** M0 biomes (docs/WORLD.md SS2.2) plus the chaos initial state. */
	enum class EBiome : uint8
	{
		Chaos = 0,
		Water,
		Grass,
		Woodland,
		RockSlope,
		Count
	};

	enum class EWaterBody : uint8
	{
		None = 0,
		Lake,
		River,
		Sea,
		Wetland,
		Count
	};

	/** Material dictionary (docs/WORLD.md SS2.1 table A/B). */
	enum class EMaterial : uint8
	{
		/* 首项是「裸地」（未上色的地表）。D-01 之前它叫 Paper，
		   那是纸雕时期的命名；Sim 侧它从来不是「纸」，是材质档位。 */
		Bare = 0,
		Soil,
		Clay,
		Mud,
		Stone,
		Ore,
		DeadWood,
		LivePlant,
		Water,
		Magma,
		Ice,
		Snow,
		Sand,
		Ash,
		Tissue,
		Count
	};

	/** M0 species set (docs/LATER.md SS2). */
	enum class ESpeciesKind : uint8
	{
		MossGrass = 0,
		Shrub,
		Grazer,
		Raptor,
		StreamFish,
		Count
	};

	/** Death causes, always readable on screen (docs/LATER.md SS5). */
	enum class EDeathCause : uint8
	{
		None = 0,
		Starved,
		Predated,
		Frozen,
		Drowned,
		Aged,
		Desiccated,
		Count
	};

	enum class EEventType : uint8
	{
		Genesis = 0,
		Terrain,
		Weather,
		Ecology,
		Echo,
		TimeMode,
		Reality,
		Trigger,
		InterAction,
		Count
	};

	/**
	 * Concrete simulated effects used by M0 cards. These are the card-facing names for the
	 * seven physical abilities listed in docs/WORLD.md SS6 (terrain op, state
	 * change, attribute write, eco op, record op...).
	 */
	enum class EEffect : uint8
	{
		None = 0,
		RaiseTerrain,
		LowerTerrain,
		FlattenTerrain,
		RidgeLine,
		PaintSurface,
		Ignite,
		HeatArea,
		ChillArea,
		WaterLevel,
		OreVein,
		SeedLife,
		RecordPillar,
		ErodeSurface,
		WeatherPush,
		Count
	};

	/** Pack kinds (docs/CARDS.md SS4.1). */
	enum class EPackKind : uint8
	{
		RandomElement = 0,
		SpecificElement,
		Special,
		Count
	};

	/** Split RNG streams, so one system's draws never shift another's sequence. */
	enum class ERngStream : uint8
	{
		Terrain = 0,
		Eco,
		Weather,
		Event,
		Loot,
		Count
	};

	// ------------------------------------------------------------------
	// Sim constant tables (transcribed from docs; tune here without touching structure)
	// ------------------------------------------------------------------

	namespace Tuning
	{
		/** docs/CARDS.md SS3.1 - value by tier. Index 10 = over-tier, 11 = forbidden. */
		extern const int32 CardValue[12];

		/** docs/CARDS.md SS3.2 - recycle refund by tier (0 and forbidden cannot be recycled). */
		extern const int32 RecycleRefund[12];

		/** docs/CARDS.md SS5.1 - pack drop weights for tiers 0..9 (percent, sums to 100). */
		extern const float TierWeight[10];

		/** docs/CARDS.md SS2.1 - event echo by offset bracket (min/max inclusive). */
		extern const float OffsetBracket[5][2];
		extern const int32 EchoBracketMin[5];
		extern const int32 EchoBracketMax[5];

		extern const int32 PackPriceRandom;		// 70
		extern const int32 PackPriceSpecific;	// 105
		extern const int32 PackPriceSpecial;	// 150
		extern const int32 StylePackPrice;		// 400
		extern const float SpecialPackOverTierChance;	// 0.08
		extern const int32 FirsthandWeather;	// 20
		extern const int32 FirsthandEco;		// 25
		extern const int32 FirsthandExtinction;// 40
		extern const int32 MaxCopiesPerKind;	// 3
		extern const int32 HandSize;			// 10
		extern const int32 ChronicleCapacity;	// 500
		extern const float DisturbanceDecayPerWorldHour;	// -2
		extern const int32 EchoPerMinuteCap;	// 20
		extern const float EchoOverCapMultiplier;	// 0.25
		extern const float SedimentCoefficient;		// 0.5
		extern const float DisturbancePenaltyThreshold;	// 60
		extern const int32 WorldMinutesPerWorldDay;	// 1440
		extern const float DefaultRealMinutesPerWorldDay;	// 3
		extern const float TileSizeMeters;		// 1.0
		extern const float MaxHeightMeters;		// 64.0
		extern const float HeightQuantum;		// 0.25
		extern const float BiomeDesertFertility;	// 0.15
		extern const float LapseRatePer100m;	// -0.6
		extern const int32 IndividualBudgetSmall;	// 1200 (docs WORLD SS2.1)

		// ---- terrain shaping (docs/WORLD.md SS2.2) ----
		// The shape itself is no longer a set of tuning scalars: it is the port of
		// studio/previews/world-model.html in Sim/SolsticeWorldGen.{h,cpp}, where the coastline,
		// the province profiles and the area-quantile remap live with their own numbers.
		/** Fertility / moisture fields, each seeded from its own noise layer. */
		extern const float TerrainFertilityBase;		// 0.35
		extern const float TerrainFertilityNoise;		// 0.6
		extern const float TerrainFertilityMin;			// 0.05
		extern const float TerrainMoistureBase;			// 0.30
		extern const float TerrainMoistureNoise;		// 0.5

		// ---- water bodies (docs/WORLD.md SS3.1) ----
		/** Depth below the water level still counted as water; sea / lake / wetland split. */
		extern const float WaterDepthEpsilonMeters;		// 0.01
		extern const float SeaDepthMeters;				// 1.0
		extern const float LakeDepthMeters;				// 0.5

		// ---- biome classification (docs/WORLD.md SS2.2) ----
		/** One biome's admissible window; every bound is inclusive. */
		struct FBiomeRange
		{
			float TempMinC;
			float TempMaxC;
			float MoistureMin;
			float MoistureMax;
			float HeightMaxM;
			float FertilityMin;
		};
		extern const FBiomeRange BiomeWoodland;
		extern const FBiomeRange BiomeGrass;
		/** Rock slope is a "too dry / too high / too poor" test, not a window. */
		extern const float BiomeRockSlopeMoistureMax;	// 0.40
		extern const float BiomeRockSlopeHeightMinM;	// 25.0
		extern const float BiomeRockSlopeFertilityMax;	// 0.20
		/** Fallback when nothing above matched: above this height it reads as rock, else grass. */
		extern const float BiomeHighlandHeightM;		// 25.0

		// ---- cadence (docs/TECH.md SS1.3) ----
		/** World minutes between weather steps (SimLOD: cheap always, expensive rarely). */
		extern const float WeatherStepWorldMinutes;		// 10.0
		/** Ecology step period in world minutes, by SimLOD level 0 / 1 / 2. */
		extern const float EcoPeriodLod0WorldMinutes;	// 4.0
		extern const float EcoPeriodLod1WorldMinutes;	// 8.0
		extern const float EcoPeriodLod2WorldMinutes;	// 12.0
		/** Biome drift is a threshold crossing, so it is only rechecked every Nth eco step. */
		extern const int32 BiomeRecheckEveryEcoSteps;	// 15
		/** One coarse visual refresh per this many world minutes. */
		extern const int32 MarkAllDirtyEveryWorldMinutes;	// 360

		// ---- solar geometry (docs/WORLD.md SS3.2, docs/WORLD.md SS3) ----
		extern const float EarthAxialTiltDeg;			// 23.44
		extern const float DeclinationDayOffset;		// 284.0
		extern const float SolarHourAngleDegPerHour;	// 15.0
		extern const float SolarAzimuthOffsetDeg;		// 180.0
		extern const float LatitudeClampDeg;			// 66.0
		/** Light intensity ramps over altitude: (altitude + floor) / span. */
		extern const float LightAltitudeFloorDeg;		// -6.0
		extern const float LightAltitudeSpanDeg;		// 70.0
		extern const float CloudOcclusionFactor;		// 0.7
		/** Colour temperature sweeps horizon -> zenith as the sun rises. */
		extern const float ColorTempHorizonK;			// 2100.0
		extern const float ColorTempZenithK;			// 6800.0
		extern const float ColorTempExponent;			// 0.6
		/** Air temperature = base + seasonal + diurnal - cloud cooling. */
		extern const float BaseAirTempC;				// 14.0
		extern const float SeasonalAmplitudeC;			// 8.0
		extern const float SeasonalLatitudeGain;		// 0.08
		extern const float SeasonalDayOffset;			// 15.0
		extern const float DiurnalAmplitudeC;			// 5.0
		extern const float DiurnalPeakHour;				// 9.0
		extern const float CloudCoolingC;				// 1.5

		// ---- slow drift rates (docs/WORLD.md SS5) ----
		extern const float HumidityRainGain;			// 0.02
		extern const float HumidityEvaporateBaseC;		// 10.0
		extern const float HumidityEvaporateRate;		// 0.0004
		extern const float HumidityWaterGain;			// 0.002
		extern const float OxygenTargetVegetation;		// 0.35
		extern const float OxygenDriftRate;				// 0.00005
		extern const float OxygenMin;					// 0.85
		extern const float OxygenMax;					// 1.20
		extern const float WaterLevelLerpRate;			// 0.08
		extern const float PrecipitationToRainDivisor;	// 4.0
		extern const float TileTempLerpRate;			// 0.25
		extern const float WaterTempOffsetC;			// -0.8
		extern const float BurnRatePerMinute;			// 0.05
		extern const float BurnVegetationLossPerMinute;	// 0.03
		extern const float BurnAshGainPerMinute;		// 0.01
		extern const float BurnMoistureLossPerMinute;	// 0.01
		extern const float AshToFertility;				// 0.15

		// ---- presentation-facing sim readouts ----
		/** Depth span that normalises the water sheet shading (matches the base water level). */
		extern const float WaterSheetShadingDepthMeters;	// 6.0
		/** Fallback sky readings when the real-weather bridge is unavailable (docs/WORLD.md SS4.2). */
		extern const float DefaultHumidity01;			// 0.45
		extern const float DefaultCloudCover01;			// 0.15
		/** Real-weather bridge thresholds (docs/WORLD.md SS4.2). */
		extern const float RainToStormThreshold01;		// 0.5
		extern const float CloudToOvercastThreshold01;	// 0.6

		// ---- tile moisture / snow / ice (docs/WORLD.md SS5) ----
		extern const float TileMoistureRainGain;		// 0.004
		extern const float TileMoistureEvaporateBaseC;	// 5.0
		extern const float TileMoistureEvaporateRate;	// 0.00002
		extern const float TileWaterMoisture;			// 0.95
		extern const float TileWaterMoistureLerpRate;	// 0.05
		/** Ice forms at or below 0 C, and melts once it warms past this again. */
		extern const float IceMeltTempC;				// 0.5
		extern const float SnowMinRainIntensity;		// 0.2
		extern const float SnowMeltTempC;				// 1.0
		extern const float SnowMeltMoistureGain;		// 0.08
	}

	// ------------------------------------------------------------------
	// World data
	// ------------------------------------------------------------------

	/** One tile = 1 m x 1 m (docs/WORLD.md SS2.1). */
	struct SOLSTICE_API FTile
	{
		float Height = 0.0f;			// 0..64 m, quantised to 0.25 m
		float Vegetation = 0.0f;		// 0..1
		float Moisture = 0.0f;			// 0..1
		float Fertility = 0.0f;			// 0..1
		float LocalTempC = 12.0f;		// degC
		float Burning = 0.0f;			// 0 = unlit, 0..1 = 明燃 intensity
		float Painted = 0.0f;			// 0 = raw paper (chaos), 1 = fully coloured
		float Ash = 0.0f;				// 0..1 burnt layer, feeds fertility
		uint8 Mat = static_cast<uint8>(EMaterial::Bare);
		uint8 Biome = static_cast<uint8>(EBiome::Chaos);
		uint8 WaterBody = static_cast<uint8>(EWaterBody::None);

		EMaterial GetMat() const { return static_cast<EMaterial>(Mat); }
		EBiome GetBiome() const { return static_cast<EBiome>(Biome); }
		EWaterBody GetWater() const { return static_cast<EWaterBody>(WaterBody); }
		bool IsWater() const { return GetWater() != EWaterBody::None; }
	};

	struct SOLSTICE_API FWorldConfig
	{
		int32 Size = 128;					// 小界 128 x 128 (docs/WORLD.md SS2.1)
		int32 ChunkSize = 64;				// 分块 64 x 64
		int32 Algo = 0;						// 0 ridges / 1 islands / 2 canyon / 3 plateau / 4 basin / 5 archipelago
		uint32 Seed = 20261016u;
		float BaseWaterLevel = 18.0f;	// ~mid height of the 0-42 m terrain: low ground becomes a lake, not a puddle
		/**
		 * Share of the realm that should be land, by area (`world.json` `land_ratio`).
		 *
		 * The roll's archetype brings its own sea range, but the *budget* is a design input: the
		 * world generator solves its coastline radius so the measured sea area hits this. It was
		 * documented in the six-axis contract for months and read by nothing, so the sea fraction
		 * was whatever noise produced (measured 77% on the default seed).
		 */
		float LandRatio = 0.62f;
		float Latitude = 31.23f;			// real-world anchor (docs/WORLD.md SS4.2)
		float TidalAmplitude = 1.0f;
		bool bTides = false;
		bool bChaosStart = false;			// the realm opens already painted (WORLD SS3.3 step 5)
		bool bWithLifeSeed = true;			// creator step 8 "life seed"
		int32 StartingDayOfYear = -1;		// -1 = derive from the real clock
		/**
		 * Minute of day the realm opens on. -1 = read the real clock (the shipped behaviour: the
		 * world runs on the real calendar). Anything >= 0 pins it, which is what the headless
		 * bench needs - two runs of the same seed must not differ by the wall clock, or the
		 * determinism check is comparing two different worlds.
		 */
		int32 StartingMinuteOfDay = -1;
		FString RealmId = TEXT("realm.valley-20261016");
		FString RealmNameZh = TEXT("无名之界");
		FString RealmNameEn = TEXT("Nameless Realm");
	};

	/** Environmental readouts (docs/WORLD.md SS3.2). */
	struct SOLSTICE_API FSkyState
	{
		float SolarAltitudeDeg = 45.0f;
		float SunAzimuthDeg = 180.0f;
		float LightIntensity = 1.0f;		// 0..1 horizontal irradiance multiplier
		float ColorTempK = 5600.0f;
		float AirTempC = 14.0f;
		float Humidity = 0.45f;
		float CloudCover = 0.15f;
		float RainIntensity = 0.0f;			// 0..1
		float WindSpeed = 1.0f;
		float WindDirDeg = 270.0f;
		float OxygenLevel = 1.0f;
		float WaterLevel = 18.0f;
		uint8 Weather = static_cast<uint8>(EWeather::Clear);
		bool bSnowing = false;				// rain replaced by snow below 0 degC
		bool bRealityOffline = false;		// weather API degraded (docs/WORLD.md SS4.2)
		float DayFraction = 0.5f;			// 0 = midnight, 0.5 = noon

		EWeather GetWeather() const { return static_cast<EWeather>(Weather); }
	};

	/** A living individual (docs/LATER.md SS2). */
	struct FIndividual
	{
		int32 Tile = 0;
		uint32 Id = 0;
		float Energy = 100.0f;
		float AgeDays = 0.0f;
		float RestTimer = 0.0f;
		uint32 PreyCount = 0;			// raptor breeding counter
		uint8 Kind = 0;					// ESpeciesKind
		uint8 Facing = 0;				// 0..7, presentation only
		uint8 Status = 0;				// 0 idle / 1 foraging / 2 hunting / 3 resting / 4 breeding
		ESpeciesKind GetKind() const { return static_cast<ESpeciesKind>(Kind); }
	};

	/** Transient on-screen death marker so a cause is always readable (SPECIES SS5). */
	struct FDeathMark
	{
		int32 Tile = 0;
		float TimeToLive = 0.0f;
		uint8 Cause = 0;
	};

	struct FChronicleEntry
	{
		int32 WorldYear = 1;
		int32 WorldDay = 1;
		int32 MinuteOfDay = 0;
		int32 RealYear = 2026;
		int32 RealMonth = 10;
		int32 RealDay = 16;
		uint8 Type = static_cast<uint8>(EEventType::Genesis);
		uint8 Element = static_cast<uint8>(EElement::None);
		uint8 Tier = 255;
		FString SubjectZh;
		FString SubjectEn;
		FString SummaryZh;
		FString SummaryEn;

		EEventType GetType() const { return static_cast<EEventType>(Type); }
	};

	/** Echo accounting, one and only one account (docs/CARDS.md SS7.2). */
	struct FEchoLedger
	{
		int32 Balance = 0;
		int32 FromEvent = 0;
		int32 FromSediment = 0;
		int32 FromFirsthand = 0;
		int32 FromRecycle = 0;
		int32 FromAutoConvert = 0;
		int32 SpentPacks = 0;
		int32 SpentCalm = 0;
		int32 SpentStyle = 0;
		int32 EchoLastRealMinute = 0;
	};

	// ------------------------------------------------------------------
	// Card catalogue (runtime data, loaded from Config/Solstice/Data/cards.*.json)
	// ------------------------------------------------------------------

	struct FCardDef
	{
		FName Id;
		FString NameZh;
		FString NameEn;
		FString DescZh;
		FString DescEn;
		uint8 Element = static_cast<uint8>(EElement::None);
		uint8 ElementAux = static_cast<uint8>(EElement::None);	// second tag for multi-element cards
		uint8 Tier = 0;					// 0..9 / 10 = over-tier / 11 = forbidden
		uint8 CardClass = 0;			// ECardClass
		uint8 Duration = 0;				// EDuration
		uint8 Direction = 0;			// EDirection
		uint8 Scope = 0;				// EScope
		uint8 Effect = 0;				// EEffect
		int32 Value = 5;
		bool bProvisional = false;		// M0 grid slot written for playability, pending per-card authoring
		TMap<FString, float> Params;

		EElement GetElement() const { return static_cast<EElement>(Element); }
		ECardClass GetCardClass() const { return static_cast<ECardClass>(CardClass); }
		EDuration GetDuration() const { return static_cast<EDuration>(Duration); }
		EEffect GetEffect() const { return static_cast<EEffect>(Effect); }
		bool IsForbidden() const { return Tier == 11; }
		bool IsOverTier() const { return Tier == 10; }

		float Param(const TCHAR* Key, float Fallback = 0.0f) const
		{
			const float* Found = Params.Find(FString(Key));
			return Found ? *Found : Fallback;
		}
	};

	struct FSpeciesDef
	{
		FName Id;
		FString NameZh;
		FString NameEn;
		uint8 Kind = 0;					// ESpeciesKind
		uint8 Role = 0;					// 0 cover producer / 1 standing producer / 2 consumer
		float HeightM = 0.5f;
		float LengthM = 0.8f;
		float LifespanDays = 90.0f;
		float MaturityDays = 5.0f;
		float EnergyMax = 100.0f;
		float EnergyDrainPerHour = 1.0f;
		float EatVegetationPerHour = 0.03f;
		float BreedEnergyFraction = 0.7f;
		float BreedVegetation = 0.5f;
		float BreedCostEnergy = 40.0f;
		float MoveCellsPerHour = 2.0f;
		float TempMinC = 10.0f;
		float TempMaxC = 28.0f;
		float WaterReachCells = 8.0f;
		float PredationSuccess = 0.3f;
		float StarvationDays = 4.0f;
		float GrowthPerHour = 0.02f;
		float FertilityCostPerHour = 0.01f;
		int32 CarryCapacityPerTile = 1;
		bool bAquatic = false;
		bool bFlying = false;
		bool bCover = false;			// moss-grass: a coverage layer, no individual tiles
	};

	struct SOLSTICE_API FCatalog
	{
		TArray<FCardDef> Cards;
		TArray<FSpeciesDef> Species;
		TArray<FWorldConfig> Presets;

		int32 FindCard(const FName& Id) const;
		int32 FindSpecies(uint8 Kind) const;
		const FCardDef* CardAt(int32 Index) const;
	};

	// ------------------------------------------------------------------
	// Transient result payloads
	// ------------------------------------------------------------------

	struct FReleaseResult
	{
		bool bOk = false;
		bool bCrossedThreshold = false;
		bool bCanUndo = false;
		float Offset = 0.0f;
		int32 EchoGained = 0;
		FString ReasonZh;
		FString ReasonEn;
		/** Tiles the effect touched, kept so the 3 second "wet paper" undo window can rewind them. */
		TArray<int32> AffectedTiles;
	};

	struct FPackResult
	{
		TArray<int32> Cards;			// indices into the catalogue
		int32 Cost = 0;
		int32 ConvertedEcho = 0;		// auto-converted overflow past the 3-copy cap
		FString TitleZh;
		FString TitleEn;
	};

	// ------------------------------------------------------------------
	// Small helpers shared by Sim and the UE layer
	// ------------------------------------------------------------------

	const TCHAR* ElementId(EElement Element);
	const TCHAR* ElementGlyph(EElement Element);		// zh label
	const TCHAR* ElementGlyphEn(EElement Element);
	const TCHAR* CardClassId(ECardClass Class);
	const TCHAR* CardClassGlyph(ECardClass Class);
	const TCHAR* DurationGlyph(EDuration Duration);
	const TCHAR* WeatherGlyph(EWeather Weather);
	const TCHAR* BiomeGlyph(EBiome Biome);
	const TCHAR* MaterialGlyph(EMaterial Material);
	const TCHAR* DeathCauseGlyph(EDeathCause Cause);
	const TCHAR* SpeciesId(ESpeciesKind Kind);
	const TCHAR* SpeciesGlyphZh(ESpeciesKind Kind);

	/** Presentation only - never read by the simulation. */
	FLinearColor ElementColor(EElement Element);
	FLinearColor CardClassColor(ECardClass Class);
	FLinearColor MaterialColor(EMaterial Material);

	/** Cel base tones used by the chaos (uncoloured) state, docs/WORLD.md SS3.3. */
	FLinearColor RawBareColor();
	FLinearColor KraftCelColor();

	/** Tier display: 0..9 numeric, "over" for the over-tier, "ban" for forbidden. */
	FString TierLabel(int32 Tier);
}
