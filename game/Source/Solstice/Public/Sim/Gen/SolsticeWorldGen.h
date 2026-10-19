// Solstice - Sim layer: the world roll and the height field.
//
// This is a port of `studio/previews/world-model.html` (sections 2 "世界骰 与 高度场"),
// not a re-derivation: the prototype is where the coastline, the province blend, the
// interior rise, the orogenic belt and the area-quantile height remap were worked out,
// and its comments carry the measurements behind every number. Keeping the port close
// means a change made there can be carried over here file-by-file instead of re-discovered.
//
// Layer rule: Sim/, so no UObject, no Tick, no rendering, no colours (they live in Gen/).

#pragma once

#include "CoreMinimal.h"

namespace Solstice
{
	namespace WorldGen
	{
		/** Province types, in the prototype's order (`PT`). */
		enum class EProvince : uint8
		{
			Plain = 0,
			Hill,
			Mountain,
			Plateau,
			Basin,
			Karst,
			Danxia,
			Yardang,
			Loess,
			Volcano,
			Glacier,
			Dune,
			Delta,
			Mesa,
			Reef,
			Count
		};

		/** Climate of the world, in the prototype's order (`CLIMS`). */
		enum class EClimate : uint8
		{
			TemperateForest = 0,
			Steppe,
			Desert,
			Rainforest,
			Tundra,
			ColdDesert,
			Count
		};

		/** One landform province: where it sits, what it is, and how big. */
		struct FProvince
		{
			float X = 0.0f;
			float Z = 0.0f;
			EProvince Type = EProvince::Plain;
			float Amplitude = 1.0f;		// "how tall" relative to the type's profile
			float Radius = 0.10f;		// envelope, in world-normalised units
			float ScaleX = 1.0f;
			float ScaleZ = 1.0f;
			float Dir = 0.0f;
			float NoiseX = 0.0f;		// per-province noise offset
			float NoiseZ = 0.0f;
		};

		/** A bay cut into the coast. One-sided on purpose - see the prototype's note. */
		struct FStrait
		{
			float X = 0.0f;
			float Z = 0.0f;
			float DirX = 1.0f;
			float DirZ = 0.0f;
			float Width = 0.07f;
			float Strength = 0.8f;
			float Phase = 0.0f;
		};

		/**
		 * Everything the roll decided about one world. Deterministic in `Seed` alone.
		 *
		 * The names are the prototype's (`landR`, `sea`, `coastWarp`, `harm`, ...) so the two
		 * files can be read side by side; they are normalised units, see `PeakMetres`.
		 */
		struct FRoll
		{
			uint32 Seed = 0;
			int32 Archetype = 0;
			EClimate Climate = EClimate::TemperateForest;

			float AzimuthRad = 0.0f;
			float Aspect = 1.0f;
			float Relief = 0.85f;
			float Sea = 0.40f;			// target sea fraction, by area
			float CoastWarp = 0.17f;
			float LandR = 1.0f;			// solved by CalibrateLand

			/** Polar harmonics of the coastline: (amplitude, frequency, phase). */
			TArray<FVector> Harmonics;
			TArray<FStrait> Straits;
			/** Lakes: (x, z, radius). Dug into flat / basin provinces only. */
			TArray<FVector> LakeSpots;
			TArray<FProvince> Provinces;

			float ProvinceWarp = 0.06f;	// how much the province borders wander
			float Blend = 0.045f;		// higher = harder borders between provinces
			float SeaDepth = 0.10f;
			float IsleAmount = 0.0f;
			float TiltAmount = 0.055f;	// one side of the continent higher than the other
			float TiltDir = 0.0f;
			float InteriorRise = 0.025f;	// "further from the coast = higher"
			float OrogenyHeight = 0.05f;	// the belt that splits the drainages
			float OrogenyDirX = 1.0f;
			float OrogenyDirZ = 0.0f;
			float OrogenyHalfWidth = 0.22f;
			float PeninsulaAngle = 0.0f;
			float PeninsulaLength = 0.0f;
			float WindDir = 0.0f;
			float Mountain = 0.22f;		// share of the land that is mountain (area target)
			float MountainGate = 1.0f;		// scales the tall province profiles
			float FocalTypeMix = 1.0f;
		};

		/** Roll a world from a seed. Pure: same seed, same world, no clock. */
		FRoll Roll(uint32 Seed);

		/**
		 * Solve `LandR` so the measured sea area hits `Roll.Sea`.
		 *
		 * Called by `Roll`, and again by the kernel when the config overrides the sea budget
		 * (`land_ratio`): the criterion is the real height, not the mask, because provinces,
		 * interior rise, tilt and lake bowls all put land under water.
		 */
		void CalibrateLand(FRoll& Roll);

		/**
		 * Signed height of the raw field, in *normalised* units (0 = sea level).
		 *
		 * Negative is underwater. The absolute metres come later, from the area-quantile
		 * remap in the kernel - the shape must not be decided by a scale factor.
		 */
		float RawHeight(float NX, float NZ, const FRoll& Roll, bool bNoRim = false);

		/** Coastline distance field: 0 inland, 1 at the coast, >1 out at sea. */
		float LandField(float NX, float NZ, const FRoll& Roll);

		/**
		 * Turn the normalised field into absolute metre heights, one per tile, row-major
		 * (`Y * Size + X` - the same order as the kernel's tile array).
		 *
		 * This is where the *area* budget is spent: the height distribution is remapped by
		 * quantile, then smoothed, given fine ridges on the high ground, and clamped for spikes.
		 * `SeaLevelMetres` shifts the whole field so the water line lands on it, which is why
		 * the sea comes out at negative heights.
		 */
		void ResolveHeights(const FRoll& Roll, int32 Size, float SeaLevelMetres, TArray<float>& OutMetres);

		/** Land mask from the distance field; `bNoRim` skips the outer-ring water band. */
		float MaskOf(float DF, float RR, bool bNoRim);

		/** Peak height of this world in metres, for the height ramp in Gen/. */
		float PeakMetres(const FRoll& Roll, int32 SizeTiles);
	}
}
