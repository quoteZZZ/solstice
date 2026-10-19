// Solstice - Sim layer: the world kernel.
//
// This is the M0 heart (docs/PLAN.md SS5 stage P2): dual-mode world clock, reality
// anchor, terrain, five-axis biome classification, weather state machine, five-species
// ecology, SimLOD, the card command interface (release / recycle / hand / pool), the echo
// economy and the chronicle.
//
// Hard constraints honoured here:
//   - no UObject, no Tick, no render / input dependency (docs/TECH.md SS2.2.1)
//   - fixed logic tick of one world minute, split RNG streams, explicit index order (SS2.2.3)
//   - every tile/individual iteration runs in ascending index order so results repeat

#pragma once

#include "CoreMinimal.h"
#include "Sim/SolsticeTypes.h"
#include "Sim/SolsticeWorldGen.h"
#include "Sim/SolsticeRng.h"

namespace Solstice
{
	/** A snapshot handed in by the reality anchor (system clock + weather API). */
	struct FRealitySample
	{
		bool bValid = false;
		int32 Year = 2026;
		int32 Month = 10;
		int32 Day = 16;
		float MinutesOfDay = 720.0f;
		float DayOfYearFraction = 0.0f;
		float TemperatureC = 15.0f;
		float Humidity01 = 0.5f;
		float CloudCover01 = 0.3f;
		float PrecipitationMm = 0.0f;
		float WindSpeedMs = 2.0f;
		float WindDirectionDeg = 270.0f;
		bool bWeatherFromNetwork = false;
	};

	class SOLSTICE_API FSimKernel
	{
	public:
		FSimKernel();
		~FSimKernel();

		// ------------------------------------------------------------------
		// Lifecycle
		// ------------------------------------------------------------------

		void Initialize(const FWorldConfig& InConfig, const FCatalog& InCatalog);

		/** Advance the world by real elapsed seconds; internally runs fixed 1-world-minute ticks. */
		void Advance(double RealDeltaSeconds);

		/** Feed the reality anchor. Real mode uses it for the clock and for weather. */
		void FeedReality(const FRealitySample& Sample);

		// ------------------------------------------------------------------
		// Time control (docs/WORLD.md SS2.2)
		// ------------------------------------------------------------------

		void SetTimeMode(ETimeMode NewMode);
		ETimeMode GetTimeMode() const { return TimeMode; }
		void SetPaused(bool bInPaused);
		bool IsPaused() const { return bPaused; }
		void SetSpeedIndex(int32 Index);
		int32 GetSpeedIndex() const { return SpeedIndex; }
		void SetRealMinutesPerWorldDay(float Minutes);
		float GetRealMinutesPerWorldDay() const { return RealMinutesPerWorldDay; }

		/** Optional SimLOD focus: the tile the camera looks at most closely. */
		void SetFocusTile(int32 Tile) { FocusTile = Tile; }
		int32 GetFocusTile() const { return FocusTile; }
		int32 GetSimLodLevel() const;

	// ------------------------------------------------------------------
	// World queries
	// ------------------------------------------------------------------

		const FWorldConfig& GetConfig() const { return Config; }
		const FCatalog& GetCatalog() const { return Catalog; }
		const TArray<FTile>& GetTiles() const { return Tiles; }
		const FSkyState& GetSky() const { return Sky; }
		/** What the roll decided about this world (archetype, climate, provinces). */
		const WorldGen::FRoll& GetWorldRoll() const { return WorldRoll; }
		/**
		 * Peak land height of this world, in metres.
		 *
		 * The render layer's height ramp divides by this instead of a fixed ceiling: the peak is
		 * 0.30 R x relief, so it moves with the realm size and with the roll, and a hardcoded
		 * divisor was what put every tile in one colour band twice already.
		 */
		float GetTerrainPeakMeters() const { return TerrainPeakMeters; }
		int32 GetSize() const { return Config.Size; }
		int32 GetChunksPerAxis() const { return ChunksPerAxis; }
		int32 GetChunkCount() const { return ChunksPerAxis * ChunksPerAxis; }
		int32 ToIndex(int32 X, int32 Y) const { return Y * Config.Size + X; }
		bool InBounds(int32 X, int32 Y) const;
		void IndexToXY(int32 Index, int32& OutX, int32& OutY) const;
		const FTile* TileAt(int32 X, int32 Y) const;
		float GetHeightAt(int32 X, int32 Y) const;
		bool IsWaterTile(int32 X, int32 Y) const;

		// ------------------------------------------------------------------
		// Clock readouts
		// ------------------------------------------------------------------

		int64 GetWorldMinute() const { return WorldMinute; }
		int32 GetWorldHour() const { return static_cast<int32>((WorldMinute / 60) % 24); }
		int32 GetWorldMinuteOfHour() const { return static_cast<int32>(WorldMinute % 60); }
		int32 GetWorldDay() const;
		int32 GetWorldYear() const;
		float GetDayFraction() const;
		FString GetClockString() const;
		FString GetRealDateString() const;
		FString GetSeasonString() const;

		// ------------------------------------------------------------------
		// Realm readouts
		// ------------------------------------------------------------------

		int32 GetPopulation(ESpeciesKind Kind) const;
		int32 GetPopulationTotal() const;
		float GetVegetationMean() const;
		float GetFertilityMean() const;
		float GetBiomassNorm() const;
		float GetDiversityIndex() const;
		float GetStabilityIndex() const;
		float GetEcoPotential() const;
		bool IsChaos() const { return bChaosState; }
		float GetPaintedFraction() const;
		float GetDisturbance() const { return Disturbance; }
		const FEchoLedger& GetLedger() const { return Ledger; }
		int32 GetEcho() const { return Ledger.Balance; }
		int32 GetEventEchoThisMinute() const { return EchoThisMinute; }
		int32 GetSedimentPerHourEstimate() const;
		int32 GetIndividualCount() const { return Individuals.Num(); }

		const TArray<FIndividual>& GetIndividuals() const { return Individuals; }
		const TArray<FDeathMark>& GetDeathMarks() const { return DeathMarks; }
		const TArray<FChronicleEntry>& GetChronicle() const { return Chronicle; }

		// ------------------------------------------------------------------
		// Hand, pool and cards (docs/CARDS.md SS1)
		// ------------------------------------------------------------------

		int32 GetHandCount() const;
		int32 GetHandCardIndex(int32 Slot) const;
		int32 GetCopies(int32 CardIndex) const;
		bool IsCardUnlocked(int32 CardIndex) const;
		int32 GetUnlockedCardCount() const;
		int32 GetRecycleRefundFor(int32 CardIndex) const;
		bool CanRecycle(int32 CardIndex) const;
		void RerollHand();
		bool ReleaseCard(int32 HandSlot, int32 TargetTile, bool bSettleAsEntity, FReleaseResult& Out);
		bool RecycleCard(int32 HandSlot, int32& OutRefund, FString& OutReasonZh, FString& OutReasonEn);

		/**
		 * The only undo in the game: a card that landed as a permanent entity can be pulled back
		 * while the paper is still wet, i.e. within three real seconds
		 * (docs/CARDS.md SS1.1). Echo, pool and chronicle never rewind (SS5.4).
		 */
		bool CanUndoPendingSettle() const { return bUndoAvailable; }
		double GetUndoSecondsLeft() const { return UndoSecondsLeft; }
		bool UndoPendingSettle(FString& OutReasonZh, FString& OutReasonEn);

		// ------------------------------------------------------------------
		// Economy (docs/CARDS.md)
		// ------------------------------------------------------------------

		int32 GetPackPrice(EPackKind Kind) const;
		bool AreSpecialPacksAvailable() const;
		bool OpenPack(EPackKind Kind, uint8 ElementFilter, FPackResult& Out);
		int32 CalmDisturbance(int32 Points, int32& OutCost);
		bool CanAfford(int32 Cost) const { return Ledger.Balance >= Cost; }
		int32 GetCardValueForTier(int32 Tier) const;

		// ------------------------------------------------------------------
		// Authoring hooks (creator panel)
		// ------------------------------------------------------------------

		void SetWorldConstant(const FString& Key, float Value);
		float GetWorldConstant(const FString& Key, float Fallback) const;

		// ------------------------------------------------------------------
		// Dirty tracking for the render layer
		// ------------------------------------------------------------------

		bool IsRegionDirty(int32 ChunkX, int32 ChunkY) const;
		void ClearRegionDirty(int32 ChunkX, int32 ChunkY);
		bool IsRegionEverBuilt(int32 ChunkX, int32 ChunkY) const;
		void MarkRegionBuilt(int32 ChunkX, int32 ChunkY);
		void MarkAllDirty();
		bool ConsumeStructureChanged();

	private:
		// ---- simulation steps ----
		void GenerateTerrain();
		void ClassifyBiomes();
		void MarkDirtyAt(int32 TileIndex);
		void StepWorldMinute();
		// Both are pure functions of the clock/config, and both the solar pass and the seasonal
		// temperature pass need them - so they live in one place each instead of being rederived.
		int32 SolarDayOfYear() const;
		float SolarLatitudeDeg() const;
		void UpdateSolar();
		void StepEnvironment();
		void StepWeather(float DeltaWorldMinutes);
		void StepTiles(float DeltaWorldMinutes);
		void StepEcology();
		void StepDisturbance(float DeltaWorldMinutes);
		void StepSediment(double RealDeltaSeconds);
		void StepDeathMarks(double RealDeltaSeconds);

		// ---- hand helpers ----
		void ResetHand();
		void RefillHand();
		int32 DrawFromPool(int32 Slot);
		int32 ComputeDrawWeight(int32 CardIndex) const;

		// ---- effects ----
		void ApplyEffect(const FCardDef& Card, int32 TargetTile, bool bSettleAsEntity, float& OutOffset, TArray<int32>& OutAffected);
		int32 CollectRadius(int32 CenterTile, float RadiusMeters, TArray<int32>& OutTiles) const;
		float ComputeOffset(int32 AffectedTileCount, float MaxHeightDelta, float BiomassDeltaRatio) const;
		int32 AwardEventEcho(float Offset, const FCardDef& Card);
		int32 AwardFirsthand(const FString& Key, int32 Amount, EEventType Type, const FString& Zh, const FString& En);
		void RecomputeWaterBodies();
		void SeedLifeFromBiomes();
		void Note(const FString& Zh, const FString& En, EEventType Type,
			EElement Element = EElement::None, int32 Tier = 255, const FString& SubjectZh = FString(), const FString& SubjectEn = FString());
		void PushChronicle(const FChronicleEntry& Entry);
		void SpawnIndividual(ESpeciesKind Kind, int32 Tile);
		void KillIndividual(int32 IndividualIndex, EDeathCause Cause);
		int32 FindWaterTileNear(int32 Tile, int32 MaxRadius) const;
		int32 FindForageTile(int32 Tile, int32 MaxRadius, float MinVegetation) const;
		void InitPoolAndUnlocks();

		// ---- state ----
		FWorldConfig Config;
		FCatalog Catalog;
		TArray<FTile> Tiles;
		FSkyState Sky;
		/** The roll behind this world; see Sim/SolsticeWorldGen.h. */
		WorldGen::FRoll WorldRoll;
		float TerrainPeakMeters = 19.2f;
		FRealitySample Reality;
		TArray<FIndividual> Individuals;
		TArray<FDeathMark> DeathMarks;
		TArray<FChronicleEntry> Chronicle;
		FEchoLedger Ledger;
		TArray<int32> Hand;
		TArray<int32> Pool;
		TArray<int32> DayReleaseCount;
		TArray<uint8> Unlocked;
		TSet<FString> FirsthandKeys;
		FRngBank Rng;

		ETimeMode TimeMode = ETimeMode::Virtual;
		bool bPaused = false;
		int32 SpeedIndex = 2;
		float RealMinutesPerWorldDay = Tuning::DefaultRealMinutesPerWorldDay;
		double MinuteAccumulator = 0.0;
		double SedimentAccumulator = 0.0;
		double DeathMarkAccumulator = 0.0;
		double SedimentRealMinutes = 0.0;
		int64 WorldMinute = 0;
		int64 EchoMinuteBucket = -1;
		int64 RealMinuteIndex = 0;
		int32 EchoThisMinute = 0;
		double RealSecondAccumulator = 0.0;
		int32 LastWorldDaySeen = -1;
		int32 StartDayOfYear = 0;
		float Disturbance = 0.0f;
		float EcoAccumulator = 0.0f;
		float WeatherAccumulator = 0.0f;
		int32 NextIndividualId = 1;
		bool bStructureDirty = true;
		bool bChaosState = true;
		int32 ChunksPerAxis = 2;
		TArray<uint8> RegionDirty;
		TArray<uint8> RegionBuilt;
		bool bGenesisWritten = false;
		bool bFirstRainSeen = false;
		bool bFirstSnowSeen = false;
		bool bFirstFogSeen = false;
		bool bFirstBirthSeen = false;
		bool bFirstPredationSeen = false;
		bool bFirstDeathSeen = false;
		bool bFirstExtinctionSeen = false;
		bool bFirstImmigrationSeen = false;
		int32 PreviousTotalPopulation = 0;
		int32 FocusTile = -1;
		TMap<FString, float> WorldConstants;
		int32 DaysWithoutRain = 0;
		int32 DaysWithoutRainMax = 0;
		float TidalPhase = 0.0f;
		float ChaosSeedPaint = 0.0f;
		int32 EcoStepCounter = 0;
		int32 ExtinctionCooldownDays = 0;
		uint32 SeedRainCounter = 0;

		// --- the 3 second wet-paper undo window ---
		bool bUndoAvailable = false;
		double UndoSecondsLeft = 0.0;
		TArray<FTile> UndoSnapshot;
		TArray<int32> UndoTiles;
		FString UndoCardNameZh;
		FString UndoCardNameEn;
	};
}
