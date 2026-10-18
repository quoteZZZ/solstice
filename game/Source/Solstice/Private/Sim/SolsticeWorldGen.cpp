// Solstice - Sim layer: the world roll and the height field.
//
// Ported from `studio/previews/world-model.html`. The structure, the tables and almost every
// constant are the prototype's; the comments are condensed to what a reader needs to *not*
// undo them. Where a number looks arbitrary, the prototype's own note explains it, and those
// notes are the reason this file is a port rather than an invention:
//
//   L0  continent field  - polar harmonics + a directional peninsula + two layers of coast
//                          noise + optional strait + optional offshore isles. Gives `df`.
//   L1  landform provinces - 9-13 provinces, each with its own profile function, blended by
//                          exponential weights over the three nearest. This is the "one
//                          landform with one or two inlays" read, not "one terrain".
//   L2  skeleton          - interior rise (further from the coast = higher), a continent-wide
//                          tilt (radial-only terrain is rotationally symmetric and reads as a
//                          disc), and an orogenic belt (without a ridge there is no drainage
//                          network: every gully runs straight to the sea).
//
// Heights come out of here *normalised* (0 = sea level). The absolute metres are assigned by
// area quantile in ResolveHeights, so "how much of the land is mountain" is an input instead
// of whatever the noise happened to produce.

#include "Sim/SolsticeWorldGen.h"

namespace Solstice
{
	namespace WorldGen
	{
		namespace
		{
			// ==================================================================
			// Noise. The prototype's own note: a value-noise lattice hash must not
			// use sin (74% of build time), so lattice corners use an integer hash.
			// ==================================================================
			FORCEINLINE float SmoothStep01(float Edge0, float Edge1, float X)
			{
				const float T = FMath::Clamp((X - Edge0) / FMath::Max(1.0e-9f, Edge1 - Edge0), 0.0f, 1.0f);
				return T * T * (3.0f - 2.0f * T);
			}

			FORCEINLINE float LatticeHash(int32 IX, int32 IY)
			{
				uint32 H = static_cast<uint32>(IX) * 374761393u + static_cast<uint32>(IY) * 668265263u;
				H ^= H >> 13;
				H *= 1274126177u;
				H ^= H >> 16;
				return static_cast<float>(H) / 4294967296.0f;
			}

			float ValueNoise(float X, float Y)
			{
				const int32 IX = FMath::FloorToInt(X);
				const int32 IY = FMath::FloorToInt(Y);
				const float FX = X - static_cast<float>(IX);
				const float FY = Y - static_cast<float>(IY);
				const float UX = FX * FX * (3.0f - 2.0f * FX);
				const float UY = FY * FY * (3.0f - 2.0f * FY);
				const float A = LatticeHash(IX, IY);
				const float B = LatticeHash(IX + 1, IY);
				const float C = LatticeHash(IX, IY + 1);
				const float D = LatticeHash(IX + 1, IY + 1);
				const float AB = A + (B - A) * UX;
				const float CD = C + (D - C) * UX;
				return AB + (CD - AB) * UY;
			}

			float Fbm(float X, float Y, int32 Octaves)
			{
				float Sum = 0.0f;
				float Amp = 0.5f;
				float Freq = 1.0f;
				float Norm = 0.0f;
				for (int32 Index = 0; Index < Octaves; ++Index)
				{
					Sum += ValueNoise(X * Freq, Y * Freq) * Amp;
					Norm += Amp;
					Amp *= 0.5f;
					Freq *= 2.0f;
				}
				return Sum / FMath::Max(1.0e-6f, Norm);
			}

			/**
			 * The roll's RNG: splitmix32, then avalanche. mulberry32 seeded straight from the
			 * user's seed grows near-identical worlds for adjacent seeds, which is exactly what
			 * "SOL-XXXX-XXXX" input would do.
			 */
			struct FSplitMix
			{
				uint32 State = 0;

				explicit FSplitMix(uint32 Seed) : State(Seed) {}

				float Next()
				{
					State += 0x9e3779b9u;
					uint32 T = State;
					T = (T ^ (T >> 15)) * (T | 1u);
					T ^= T + (T ^ (T >> 7)) * (T | 61u);
					return static_cast<float>(T ^ (T >> 14)) / 4294967296.0f;
				}

				float Range(float Low, float High) { return Low + Next() * (High - Low); }
				int32 RangeInt(int32 Low, int32 High) { return Low + FMath::FloorToInt(Next() * static_cast<float>(High - Low + 1)); }
			};

			/** `hash2` from the prototype: fine-coordinate hash, still used for wind/view angles. */
			float Hash2(float X, float Y)
			{
				const float H = FMath::Sin(X * 127.1f + Y * 311.7f) * 43758.5453f;
				return H - FMath::FloorToFloat(H);
			}

			// ==================================================================
			// Archetypes. The three constraints in the prototype's note are real:
			// aspect 0.82-1.12 (0.55 is a noodle, 1.5 is a cube), sea >= 0.20
			// (the outer water band is ~8% of the area), and few provinces with low
			// harmonics ("a piece of land", not "a flower of fragments").
			// ==================================================================
			struct FArchetypeSpec
			{
				float LandRLow, LandRHigh;
				float AspectLow, AspectHigh;
				float SeaLow, SeaHigh;
				float ReliefLow, ReliefHigh;
				int32 Harmonics;
				int32 MinProvinces, MaxProvinces;
			};

			const FArchetypeSpec Archetypes[] = {
				/* 0 continent coast */ { 0.82f, 1.00f, 0.74f, 1.08f, 0.36f, 0.58f, 0.72f, 1.00f, 3, 9, 13 },
				/* 1 inland plateau  */ { 0.94f, 1.06f, 0.84f, 1.10f, 0.26f, 0.46f, 0.85f, 1.12f, 2, 9, 12 },
				/* 2 mountain belt   */ { 0.90f, 1.04f, 0.72f, 1.02f, 0.28f, 0.48f, 1.00f, 1.28f, 3, 9, 13 },
				/* 3 archipelago     */ { 0.40f, 0.62f, 0.92f, 1.12f, 0.62f, 0.84f, 0.60f, 0.90f, 4, 7, 11 },
				/* 4 volcanic isles  */ { 0.46f, 0.66f, 0.92f, 1.12f, 0.58f, 0.78f, 0.85f, 1.15f, 3, 6, 10 },
				/* 5 arid mesa       */ { 0.96f, 1.06f, 0.86f, 1.12f, 0.26f, 0.46f, 0.68f, 0.98f, 2, 9, 13 },
				/* 6 continent       */ { 1.02f, 1.20f, 0.94f, 1.12f, 0.16f, 0.26f, 0.86f, 1.08f, 2, 12, 16 },
			};
			constexpr int32 ArchetypeCount = UE_ARRAY_COUNT(Archetypes);

			/** Province pool per archetype. First entry is the host (largest share). */
			const EProvince Pool0[] = { EProvince::Plain, EProvince::Hill, EProvince::Mountain, EProvince::Plain, EProvince::Karst, EProvince::Basin };
			const EProvince Pool1[] = { EProvince::Plateau, EProvince::Plain, EProvince::Hill, EProvince::Basin, EProvince::Loess, EProvince::Mountain };
			const EProvince Pool2[] = { EProvince::Mountain, EProvince::Hill, EProvince::Mountain, EProvince::Plain, EProvince::Basin };
			const EProvince Pool3[] = { EProvince::Hill, EProvince::Reef, EProvince::Plain, EProvince::Plain, EProvince::Karst, EProvince::Dune };
			const EProvince Pool4[] = { EProvince::Volcano, EProvince::Hill, EProvince::Plain, EProvince::Basin, EProvince::Reef };
			const EProvince Pool5[] = { EProvince::Mesa, EProvince::Plain, EProvince::Yardang, EProvince::Dune, EProvince::Hill, EProvince::Basin };
			const EProvince Pool6[] = { EProvince::Plain, EProvince::Plain, EProvince::Hill, EProvince::Mountain, EProvince::Plateau, EProvince::Plain };

			struct FProvincePool
			{
				const EProvince* Types;
				int32 Count;
			};

			const FProvincePool Pools[ArchetypeCount] = {
				{ Pool0, UE_ARRAY_COUNT(Pool0) }, { Pool1, UE_ARRAY_COUNT(Pool1) },
				{ Pool2, UE_ARRAY_COUNT(Pool2) }, { Pool3, UE_ARRAY_COUNT(Pool3) },
				{ Pool4, UE_ARRAY_COUNT(Pool4) }, { Pool5, UE_ARRAY_COUNT(Pool5) },
				{ Pool6, UE_ARRAY_COUNT(Pool6) },
			};

			/**
			 * Climate x landform legality. A glacier horn in a rainforest or a dune field on an
			 * equatorial island is the kind of thing that reads as "randomly assembled" immediately.
			 * Illegal types are *replaced*, not deleted, so the province count stays stable.
			 */
			const uint8 ClimateOk[static_cast<int32>(EClimate::Count)][static_cast<int32>(EProvince::Count)] = {
				/* temperate */ { 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0 },
				/* steppe    */ { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 },
				/* desert    */ { 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0 },
				/* rainforest*/ { 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1 },
				/* tundra    */ { 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0 },
				/* cold desert*/{ 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0 },
			};

			EProvince LegalType(EClimate Climate, EProvince Type)
			{
				if (ClimateOk[static_cast<int32>(Climate)][static_cast<int32>(Type)])
				{
					return Type;
				}
				if (Type == EProvince::Glacier || Type == EProvince::Danxia)
				{
					return EProvince::Mountain;
				}
				if (Type == EProvince::Reef || Type == EProvince::Karst)
				{
					return EProvince::Hill;
				}
				if (Type == EProvince::Dune || Type == EProvince::Yardang || Type == EProvince::Loess)
				{
					return EProvince::Hill;
				}
				return EProvince::Plain;
			}

			// ==================================================================
			// Coastline
			// ==================================================================
			/**
			 * Outer water band: the world's rim is *always* water.
			 *
			 * Hard constraint, not a probability. With land allowed to reach the rim, the outer
			 * ring of tiles is land and the top view reads "a dirt ring around a lake" while a
			 * side view shows a wall of earth. RIM_A sits well inside the cut (0.995) so the cut
			 * comes down through water: that is what keeps the world's edge one thin dark line
			 * instead of the cross-section of a hillside.
			 *
			 * (Was 0.940: only 22 m of band to drop 90 m made a vertical cliff around every
			 * continent. At 0.975 the coast slope is decided by the distance field, ~40 deg.)
			 */
			constexpr float RimA = 0.975f;
			constexpr float RimB = 0.9965f;

			float RimMask(float RR)
			{
				return 1.0f - SmoothStep01(RimA, RimB, RR);
			}
		}

		// ======================================================================
		// Roll
		// ======================================================================
		FRoll Roll(uint32 Seed)
		{
			FSplitMix Rng(Seed);
			FRoll Roll;
			Roll.Seed = Seed;

			Roll.Archetype = FMath::Clamp(FMath::FloorToInt(Rng.Next() * ArchetypeCount), 0, ArchetypeCount - 1);
			const FArchetypeSpec& Spec = Archetypes[Roll.Archetype];

			Roll.Climate = static_cast<EClimate>(
				FMath::Clamp(FMath::FloorToInt(Rng.Next() * static_cast<float>(EClimate::Count)), 0, static_cast<int32>(EClimate::Count) - 1));

			const float AzimuthDeg = Rng.Next() * 360.0f;
			Roll.AzimuthRad = FMath::DegreesToRadians(AzimuthDeg);
			Roll.Aspect = Rng.Range(Spec.AspectLow, Spec.AspectHigh);
			Roll.Relief = Rng.Range(Spec.ReliefLow, Spec.ReliefHigh);
			Roll.Sea = Rng.Range(Spec.SeaLow, Spec.SeaHigh);
			// 0.130-0.215: the coastline swings +/-15-20% of the radius. Enough to read as bays
			// and peninsulas; 0.28 or more throws out a spit a few metres wide.
			Roll.CoastWarp = 0.130f + Rng.Next() * 0.085f;
			Roll.LandR = 1.0f;
			Roll.ProvinceWarp = 0.045f + Rng.Next() * 0.045f;
			Roll.Blend = 0.030f + Rng.Next() * 0.028f;
			Roll.SeaDepth = 0.070f + Rng.Next() * 0.055f;
			Roll.TiltAmount = 0.040f + Rng.Next() * 0.036f;
			Roll.TiltDir = Rng.Next() * 6.2832f;
			Roll.InteriorRise = 0.016f + Rng.Next() * 0.020f;
			Roll.WindDir = Rng.Next() * 6.2832f;
			Roll.PeninsulaAngle = Rng.Next() * 6.2832f;
			Roll.PeninsulaLength = 0.14f + Rng.Next() * 0.20f;
			Roll.OrogenyHeight = 0.030f + Rng.Next() * 0.042f;
			Roll.OrogenyDirX = FMath::Cos(Rng.Next() * 6.2832f);
			Roll.OrogenyHalfWidth = 0.16f + Rng.Next() * 0.14f;
			Roll.OrogenyDirZ = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Roll.OrogenyDirX * Roll.OrogenyDirX))
				* (Rng.Next() > 0.5f ? 1.0f : -1.0f);

			// Harmonics: 2-4 terms. Amplitude capped at 0.16 (0.24 threw a needle-thin cape off
			// the coast) and frequency 2-5 (higher is "embroidery", not a continent).
			for (int32 Index = 0; Index < Spec.Harmonics; ++Index)
			{
				const float Amp = (0.078f + Rng.Next() * 0.132f) * (1.0f - static_cast<float>(Index) * 0.11f);
				const float Freq = 2.0f + static_cast<float>(Index) + static_cast<float>(FMath::FloorToInt(Rng.Next() * 2.0f));
				Roll.Harmonics.Add(FVector(Amp, Freq, Rng.Next() * 6.2832f));
			}

			// A strait cuts a *bay* into the coast - one-sided, closing inland. A channel that
			// crosses the whole world slices it into two planks with straight edges.
			if (Roll.Archetype != 3 && Roll.Archetype != 4 && Rng.Next() > 0.50f)
			{
				const float Angle = Rng.Next() * 6.2832f;
				const float Offset = (Rng.Next() - 0.5f) * 0.55f;
				FStrait Strait;
				Strait.X = FMath::Cos(Angle + 1.5708f) * Offset;
				Strait.Z = FMath::Sin(Angle + 1.5708f) * Offset;
				Strait.DirX = FMath::Cos(Angle);
				Strait.DirZ = FMath::Sin(Angle);
				Strait.Width = 0.055f + Rng.Next() * 0.040f;
				Strait.Strength = 0.62f + Rng.Next() * 0.36f;
				Strait.Phase = Rng.Next() * 6.2832f;
				Roll.Straits.Add(Strait);
			}

			// Offshore isles: without them every coast is "one lump floating on water".
			if (Roll.Archetype == 6)
			{
				Roll.IsleAmount = 0.0f;
			}
			else if (Roll.Archetype == 3 || Roll.Archetype == 4)
			{
				Roll.IsleAmount = 0.35f + Rng.Next() * 0.45f;
			}
			else if (Rng.Next() < 0.46f)
			{
				Roll.IsleAmount = 0.16f + Rng.Next() * 0.42f;
			}

			Roll.Mountain = 0.10f + Rng.Next() * 0.10f;
			Roll.Mountain = FMath::Clamp(
				Roll.Mountain * (0.55f + (Spec.ReliefLow + Spec.ReliefHigh) * 0.5f), 0.12f, 0.62f);

			// ---- provinces ----------------------------------------------------
			const FProvincePool& Pool = Pools[Roll.Archetype];
			const int32 ProvinceCount = FMath::Max(3, Spec.MinProvinces + FMath::FloorToInt(Rng.Next()
				* static_cast<float>(Spec.MaxProvinces - Spec.MinProvinces + 1)));

			for (int32 Index = 0; Index < ProvinceCount; ++Index)
			{
				const bool bHost = (Index == 0);
				float X = 0.0f;
				float Z = 0.0f;
				if (bHost)
				{
					// The host sits near the centre and covers half the continent: without a host
					// every province is the same size and the world reads as a tray of samples.
					const float Angle = Rng.Next() * 6.2832f;
					const float Radius = Rng.Next() * 0.26f;
					X = FMath::Cos(Angle) * Radius;
					Z = FMath::Sin(Angle) * Radius;
					const float RR = FMath::Sqrt(X * X + Z * Z);
					if (RR > 0.24f)
					{
						X *= 0.24f / RR;
						Z *= 0.24f / RR;
					}
				}
				else
				{
					// Poisson-ish rejection: nearest-neighbour spacing 0.185, so provinces read as
					// inlays rather than a mosaic of same-sized cells.
					bool bOk = false;
					int32 Tries = 0;
					while (!bOk && Tries < 96)
					{
						++Tries;
						const float Angle = Rng.Next() * 6.2832f;
						const float Radius = FMath::Sqrt(Rng.Next()) * 0.90f;
						X = FMath::Cos(Angle) * Radius;
						Z = FMath::Sin(Angle) * Radius;
						bOk = true;
						for (const FProvince& Other : Roll.Provinces)
						{
							const float DX = X - Other.X;
							const float DZ = Z - Other.Z;
							if (FMath::Sqrt(DX * DX + DZ * DZ) < 0.185f)
							{
								bOk = false;
								break;
							}
						}
					}
				}

				const EProvince Picked = bHost
					? (Rng.Next() < 0.40f ? Pool.Types[1] : Pool.Types[0])
					: Pool.Types[FMath::Clamp(FMath::FloorToInt(Rng.Next() * static_cast<float>(Pool.Count)), 0, Pool.Count - 1)];
				const EProvince Legal = LegalType(Roll.Climate, Picked);

				// Coastal landforms belong on the coast; pushed to 0.78-0.90 of the radius so the
				// outer water band does not drown them.
				if (Legal == EProvince::Reef || Legal == EProvince::Delta)
				{
					const float RR = FMath::Max(0.001f, FMath::Sqrt(X * X + Z * Z));
					const float Want = (Legal == EProvince::Delta ? 0.66f : 0.78f) + Rng.Next() * 0.11f;
					X *= Want / RR;
					Z *= Want / RR;
				}
				// Orogenic provinces are inlays, not the centrepiece: keep them off the host.
				const bool bUplifted = Legal == EProvince::Mountain || Legal == EProvince::Plateau
					|| Legal == EProvince::Glacier || Legal == EProvince::Volcano
					|| Legal == EProvince::Mesa || Legal == EProvince::Danxia;
				if (!bHost && bUplifted)
				{
					const float RR = FMath::Max(0.001f, FMath::Sqrt(X * X + Z * Z));
					if (RR < 0.34f)
					{
						X *= 0.34f / RR;
						Z *= 0.34f / RR;
					}
				}

				FProvince Province;
				Province.X = X;
				Province.Z = Z;
				Province.Type = Legal;
				Province.Amplitude = bHost ? (0.92f + Rng.Next() * 0.34f) : (0.44f + Rng.Next() * 0.52f);
				Province.Radius = bHost ? (0.20f + Rng.Next() * 0.16f) : (0.08f + Rng.Next() * 0.11f);
				// Envelope 0.88-1.12 keeps the host at 40-55% of the land; at 1.05 it ate 64% and
				// the world was "one mesa plus three hills".
				Province.ScaleX = bHost ? (0.88f + Rng.Next() * 0.24f) : (0.52f + Rng.Next() * 0.34f);
				Province.ScaleZ = bHost ? (0.88f + Rng.Next() * 0.24f) : (0.52f + Rng.Next() * 0.34f);
				Province.Dir = Rng.Next() * 6.2832f;
				Province.NoiseX = Rng.Next() * 60.0f;
				Province.NoiseZ = Rng.Next() * 60.0f;
				Roll.Provinces.Add(Province);
			}

			// Volcanic isles: the cone has to be dead centre. Scattered at random, the "volcano
			// world" rolls out as a field of mesas with no cone in sight.
			if (Roll.Archetype == 4 && Roll.Provinces.Num() > 0)
			{
				FProvince& Main = Roll.Provinces[0];
				Main.X = 0.0f;
				Main.Z = 0.0f;
				Main.Type = EProvince::Volcano;
				// 1.42/0.44/1.30 made the cone 87% of the land; 1.22/0.36/1.02 leaves a coast.
				Main.Amplitude = 1.22f;
				Main.Radius = 0.36f;
				Main.ScaleX = 1.02f;
				Main.ScaleZ = 1.02f;
				Main.Dir = Rng.Next() * 6.2832f;
				int32 Secondaries = 0;
				for (int32 Index = 1; Index < Roll.Provinces.Num() && Secondaries < 2; ++Index)
				{
					if (Rng.Next() > 0.55f)
					{
						FProvince& Sub = Roll.Provinces[Index];
						Sub.Type = EProvince::Volcano;
						Sub.Amplitude = 0.62f + Rng.Next() * 0.22f;
						Sub.Radius = 0.14f + Rng.Next() * 0.09f;
						++Secondaries;
					}
				}
			}

			// Every world gets at least one uplift: with the pools as they are, the chance of
			// rolling zero mountains is close to a third, and "mountain belt with no mountain"
			// is not a world worth shipping.
			int32 Uplifted = 0;
			for (const FProvince& Province : Roll.Provinces)
			{
				const bool bUp = Province.Type == EProvince::Mountain || Province.Type == EProvince::Plateau
					|| Province.Type == EProvince::Glacier || Province.Type == EProvince::Mesa
					|| Province.Type == EProvince::Volcano || Province.Type == EProvince::Danxia;
				Uplifted += bUp ? 1 : 0;
			}
			if (Uplifted == 0 && Roll.Provinces.Num() > 0)
			{
				int32 Best = 0;
				float BestRadius = -1.0f;
				for (int32 Index = 0; Index < Roll.Provinces.Num(); ++Index)
				{
					const FProvince& Province = Roll.Provinces[Index];
					if (Province.Type != EProvince::Plain && Province.Type != EProvince::Hill)
					{
						continue;
					}
					const float RR = FMath::Sqrt(Province.X * Province.X + Province.Z * Province.Z);
					if (RR > BestRadius)
					{
						BestRadius = RR;
						Best = Index;
					}
				}
				Roll.Provinces[Best].Type = EProvince::Mountain;
				Roll.Provinces[Best].Amplitude = 0.85f + Rng.Next() * 0.45f;
			}

			// Lakes only in flat ground: a lake on a mountainside is a hole in the sky.
			const int32 LakeCount = 1 + FMath::FloorToInt(Rng.Next() * 3.0f);
			for (int32 Index = 0; Index < LakeCount && Roll.Provinces.Num() > 0; ++Index)
			{
				const FProvince& Pick = Roll.Provinces[FMath::Clamp(
					FMath::FloorToInt(Rng.Next() * static_cast<float>(Roll.Provinces.Num())), 0, Roll.Provinces.Num() - 1)];
				const bool bFlat = Pick.Type == EProvince::Basin || Pick.Type == EProvince::Plain
					|| Pick.Type == EProvince::Delta || Pick.Type == EProvince::Reef;
				if (!bFlat)
				{
					continue;
				}
				const float LX = Pick.X + (Rng.Next() - 0.5f) * 0.20f;
				const float LZ = Pick.Z + (Rng.Next() - 0.5f) * 0.20f;
				bool bOk = true;
				for (const FVector& Existing : Roll.LakeSpots)
				{
					const float DX = LX - Existing.X;
					const float DZ = LZ - Existing.Y;
					if (FMath::Sqrt(DX * DX + DZ * DZ) < 0.26f)
					{
						bOk = false;
						break;
					}
				}
				if (bOk)
				{
					Roll.LakeSpots.Add(FVector(LX, LZ, 0.055f + Rng.Next() * 0.062f));
				}
			}

			// Mountain gate: the relief slider has to move *absolute* height, not just squash
			// the final remap, or "12% mountain" still produces cliffs everywhere.
			Roll.MountainGate = FMath::Clamp(
				0.30f + Roll.Mountain * 2.95f + (Roll.Relief - 0.85f) * 0.42f, 0.42f, 1.45f);

			CalibrateLand(Roll);

			return Roll;
		}

		// ======================================================================
		// Land calibration
		// ======================================================================
		void CalibrateLand(FRoll& Roll)
		{
			float Low = 0.30f;
			float High = 2.30f;
			constexpr int32 Samples = 26;
			for (int32 Iteration = 0; Iteration < 12; ++Iteration)
			{
				Roll.LandR = 0.5f * (Low + High);
				int32 SeaSamples = 0;
				int32 Total = 0;
				for (int32 J = 0; J < Samples; ++J)
				{
					for (int32 I = 0; I < Samples; ++I)
					{
						const float NX = (static_cast<float>(I) / (Samples - 1)) * 2.0f - 1.0f;
						const float NZ = (static_cast<float>(J) / (Samples - 1)) * 2.0f - 1.0f;
						if (NX * NX + NZ * NZ > 0.96f)
						{
							continue;
						}
						++Total;
						// bNoRim: the outer water band is ~25% of the area. Counted as sea, the
						// bisection pushes landR up until the land hits the cut and the whole
						// continent ends in a vertical wall.
						if (RawHeight(NX, NZ, Roll, true) <= 0.0f)
						{
							++SeaSamples;
						}
					}
				}
				if (static_cast<float>(SeaSamples) / FMath::Max(1.0f, static_cast<float>(Total)) > Roll.Sea)
				{
					Low = Roll.LandR;
				}
				else
				{
					High = Roll.LandR;
				}
			}
		}

		// ======================================================================
		// Field functions
		// ======================================================================
		float LandField(float NX, float NZ, const FRoll& Roll)
		{
			const float CosA = FMath::Cos(Roll.AzimuthRad);
			const float SinA = FMath::Sin(Roll.AzimuthRad);
			float PX = (NX * CosA - NZ * SinA) / FMath::Max(0.2f, Roll.Aspect);
			float PZ = (NX * SinA + NZ * CosA);

			// Warp before converting to polar, or the coastline disagrees with the continent.
			const float Warp = Roll.CoastWarp * 0.85f;
			PX += (Fbm(NX * 5.1f + 31.1f, NZ * 5.1f + 7.3f, 3) - 0.5f) * Warp;
			PZ += (Fbm(NX * 5.1f - 17.3f, NZ * 5.1f - 9.1f, 3) - 0.5f) * Warp;

			const float R = FMath::Sqrt(PX * PX + PZ * PZ);
			const float Theta = FMath::Atan2(PZ, PX);

			float CR = 1.0f;
			for (const FVector& Harmonic : Roll.Harmonics)
			{
				CR += Harmonic.X * FMath::Cos(Theta * Harmonic.Y + Harmonic.Z);
			}
			CR *= Roll.LandR;

			// A directional peninsula. Harmonics only wave the edge; they cannot push a cape out,
			// which is why every world used to be a ball with a +/-15% wobble.
			if (Roll.PeninsulaLength > 0.0f)
			{
				// cos^4 (not cos^6): a 6th power is a +/-30 degree window and reads as a spike
				// pulled out of the edge; 4th power is a stretch of land.
				const float Window = FMath::Max(0.0f, FMath::Cos(Theta - Roll.PeninsulaAngle));
				CR *= 1.0f + Roll.PeninsulaLength * FMath::Pow(Window, 4.0f);
			}

			// Coast break-up: two layers, weighted by radius so the noise cannot dig a sea out of
			// the middle of the continent.
			const float CoastWeight = 0.22f + 0.78f * SmoothStep01(0.12f, 0.72f, R);
			const float CoastNoise = ((Fbm(NX * 2.6f + 13.1f, NZ * 2.6f - 8.3f, 4) - 0.5f) * Roll.CoastWarp
				+ (Fbm(NX * 7.4f - 2.7f, NZ * 7.4f + 5.9f, 3) - 0.5f) * Roll.CoastWarp * 0.46f) * CoastWeight;

			float D = (R + CoastNoise) / FMath::Max(0.05f, CR);

			for (const FStrait& Strait : Roll.Straits)
			{
				const float DX = PX - Strait.X;
				const float DZ = PZ - Strait.Z;
				float Perp = -DX * Strait.DirZ + DZ * Strait.DirX;
				const float Along = DX * Strait.DirZ + DZ * Strait.DirX;

				Perp += FMath::Sin(Along * 2.6f + Strait.Phase) * 0.082f
					+ FMath::Sin(Along * 1.15f + Strait.Phase * 1.7f) * 0.115f
					+ (Fbm(NX * 2.4f + 11.1f, NZ * 2.4f + 3.3f, 3) - 0.5f) * 0.070f;
				const float Width = Strait.Width * (0.72f + 0.62f * (FMath::Sin(Along * 1.9f + Strait.Phase * 2.3f) * 0.5f + 0.5f))
					* (0.55f + 0.85f * SmoothStep01(0.10f, 0.95f, Along));
				const float Taper = SmoothStep01(0.06f, 0.34f, Along);
				if (Taper > 0.001f)
				{
					D += FMath::Exp(-FMath::Pow(Perp / FMath::Max(1.0e-3f, Width), 2.0f)) * Strait.Strength * Taper;
				}
			}

			if (Roll.IsleAmount > 0.01f)
			{
				const float Band = SmoothStep01(0.96f, 1.12f, D) * (1.0f - SmoothStep01(1.62f, 1.98f, D));
				const float Isles = Fbm(NX * 2.8f + 41.2f, NZ * 2.8f + 17.7f, 4);
				D -= Band * Roll.IsleAmount * SmoothStep01(0.60f, 0.73f, Isles) * 0.78f;
			}

			return D;
		}

		float MaskOf(float DF, float RR, bool bNoRim)
		{
			// Transition 0.700-1.140 rather than 0.700-1.000: on a 1 km world the narrow version
			// drops a 150 m hill into the sea over 70 m of ground - a 65 degree wall all the way
			// round, i.e. "a slab of cake". 0.28 of the field is ~125 m and reads as a coastal
			// slope.
			const float Mask = 1.0f - SmoothStep01(0.700f, 1.140f, DF);
			if (bNoRim)
			{
				return Mask;
			}
			return Mask * RimMask(RR);
		}

		namespace
		{
			/** Shared noise fields, evaluated once per sample (all provinces use the same ones). */
			struct FNoiseFields
			{
				float Base = 0.0f;
				float Fine = 0.0f;
				float Low = 0.0f;
				float Ridge = 0.0f;
				float RidgeFine = 0.0f;
			};

			/** One province's profile. Ported branch for branch; see the prototype for reasons. */
			float ProvinceHeight(const FProvince& Province, float NX, float NZ, const FRoll& Roll, const FNoiseFields& NF)
			{
				const float Amplitude = Province.Amplitude;
				const float LocalX = NX - Province.X;
				const float LocalZ = NZ - Province.Z;
				const float LocalR = FMath::Sqrt(LocalX * LocalX + LocalZ * LocalZ);
				const float Gate = Roll.MountainGate;
				const float WX = Province.NoiseX;
				const float WZ = Province.NoiseZ;

				switch (Province.Type)
				{
				case EProvince::Plain:
					return 0.012f + 0.030f * NF.Base + 0.009f * NF.Fine;

				case EProvince::Hill:
					return 0.044f + 0.048f * NF.Base + 0.032f * NF.Ridge + 0.007f * NF.Fine;

				case EProvince::Mountain:
				{
					// A ridge-only field is a fish bone: equal-height fins everywhere. Real ranges
					// are "a few massifs with valleys between", so a low-frequency field picks the
					// massifs and the ridges only rise inside them.
					const float Massif = Fbm(NX * 1.90f + WX, NZ * 1.90f + WZ, 3);
					const float Mask = SmoothStep01(0.30f, 0.72f, Massif);
					const float Ridge2 = FMath::Pow(1.0f - FMath::Abs(
						Fbm(NX * 7.2f - NZ * 1.7f + WX, NZ * 7.2f + NX * 1.7f + WZ, 3) * 2.0f - 1.0f), 1.55f);
					return 0.048f + 0.048f * NF.Base
						+ Mask * (0.070f + 0.330f * Amplitude * Gate * (0.10f + 0.54f * NF.Ridge + 0.36f * Ridge2));
				}

				case EProvince::Plateau:
				{
					// Quantised low-frequency steps with a rounded shoulder - a 0.48 step height
					// made a slab of tofu, and a 0.14 step width made it a cliff.
					const float Q = Fbm(NX * 1.55f + WX, NZ * 1.55f + WZ, 3) + 0.10f * (NF.Ridge - 0.42f);
					const float Steps = 2.0f;
					const float Index = FMath::FloorToFloat(Q * Steps);
					const float Frac = Q * Steps - Index;
					const float Surf = (Index + FMath::Pow(FMath::Max(0.0f, Frac - 0.70f) / 0.30f, 1.7f)) / Steps;
					return 0.074f + 0.185f * Amplitude * Gate * Surf + 0.028f * NF.Base;
				}

				case EProvince::Basin:
				{
					const float Bowl = 1.0f - SmoothStep01(0.0f, 0.62f, LocalR);
					return 0.046f + 0.034f * NF.Base + 0.037f * (1.0f - Bowl) - 0.046f * Bowl;
				}

				case EProvince::Karst:
				{
					// Isolated cones on a solution plain. A ridge field here is a row of teeth;
					// thresholded mid-frequency fbm gives blobs, and the 0.78 power rounds the top
					// while keeping the foot steep.
					const float NK = Fbm(NX * 4.6f + WX, NZ * 4.6f + WZ, 4);
					const float Tower = FMath::Max(0.0f, NK - 0.485f) / 0.515f;
					const float HeightK = 0.55f + 0.85f * Fbm(NX * 1.55f - WZ, NZ * 1.55f + WX, 3);
					return 0.009f + 0.015f * NF.Base + 0.152f * Amplitude * Gate * FMath::Pow(Tower, 0.78f) * HeightK
						- 0.006f * (1.0f - Tower);
				}

				case EProvince::Danxia:
				{
					const float Blx = Fbm(NX * 0.74f + WX, NZ * 0.74f + WZ, 3);
					const float Mask = SmoothStep01(0.38f, 0.70f, Blx);
					const float Q = Fbm(NX * 4.2f + WX, NZ * 4.2f + WZ, 3) + 0.13f * (NF.Ridge - 0.44f);
					const float Steps = 2.0f;
					const float Index = FMath::FloorToFloat(Q * Steps);
					const float Frac = Q * Steps - Index;
					const float Surf = (FMath::Clamp(Index + FMath::Pow(FMath::Max(0.0f, Frac - 0.68f) / 0.32f, 2.0f), 0.0f, 8.0f)) / Steps;
					return 0.024f + 0.084f * Amplitude * Gate * Mask * (0.30f + 0.70f * Surf) + 0.019f * NF.Base;
				}

				case EProvince::Yardang:
				{
					// Parallel ridges along the *global* wind direction - one wind per world is
					// what makes a desert read as one desert.
					const float Wind = Roll.WindDir;
					const float CA = FMath::Cos(Wind);
					const float SA = FMath::Sin(Wind);
					const float Along = -LocalX * SA + LocalZ * CA;
					float Crest = FMath::Sin(Along * 44.0f + 1.4f + (NF.Fine - 0.5f) * 5.2f) * 0.5f + 0.5f;
					Crest = FMath::Pow(Crest, 2.7f);
					return 0.046f + 0.071f * Amplitude * Crest + 0.012f * NF.Base;
				}

				case EProvince::Loess:
				{
					// A flat table cut by a gully network: the gullies are negative ridges.
					float Gully = 1.0f - FMath::Abs(Fbm(NX * 5.4f + WX, NZ * 5.4f + WZ, 4) * 2.0f - 1.0f);
					Gully = FMath::Pow(Gully, 2.6f);
					return 0.133f * Amplitude * (1.0f - 0.74f * Gully) + 0.016f * NF.Base + 0.019f;
				}

				case EProvince::Volcano:
				{
					// Cone + crater lip + rim + radial gullies.
					const float Base = FMath::Max(0.12f, Province.Radius * 2.10f);
					const float Cone = FMath::Max(0.0f, 1.0f - LocalR / Base);
					float H = 0.043f + FMath::Pow(Cone, 2.15f) * (0.480f * Amplitude * Gate);
					const float Crater = Province.Radius * 0.34f;
					const float Lip = FMath::Exp(-FMath::Pow((LocalR - Crater * 1.16f) / (Crater * 0.44f), 2.0f));
					H += 0.043f * Amplitude * Gate * Lip * Cone;
					H -= 0.095f * Amplitude * Gate * FMath::Exp(-FMath::Pow(LocalR / Crater, 2.0f));
					const float Angle = FMath::Atan2(LocalZ, LocalX);
					H += (0.025f * FMath::Sin(Angle * 11.0f + Province.Dir)
						+ 0.016f * FMath::Sin(Angle * 23.0f + Province.Dir * 1.7f)) * Cone * Amplitude * Gate;
					return H;
				}

				case EProvince::Dune:
				{
					// Asymmetric profile: gentle windward, steep lee.
					const float Wind = Roll.WindDir;
					const float UX = -FMath::Sin(Wind);
					const float UZ = FMath::Cos(Wind);
					float Phase = (LocalX * UX + LocalZ * UZ) * 9.0f + (NF.Base - 0.5f) * 9.0f;
					Phase -= FMath::FloorToFloat(Phase / 6.2832f) * 6.2832f;
					const float S = Phase / 6.2832f;
					const float Surf = S < 0.74f ? FMath::Pow(S / 0.74f, 1.45f) : (1.0f - (S - 0.74f) / 0.26f);
					return 0.030f + 0.060f * Amplitude * Surf + 0.006f * NF.Fine;
				}

				case EProvince::Delta:
				{
					const float Channel = FMath::Abs(Fbm(NX * 4.4f + WX, NZ * 4.4f + WZ, 3) - 0.5f);
					return 0.015f + 0.012f * Amplitude - 0.019f * FMath::Exp(-FMath::Pow(Channel / 0.045f, 2.0f)) + 0.005f * NF.Fine;
				}

				case EProvince::Mesa:
				{
					// Isolated blocks: a mask decides *where* the mesa is, so the province is not
					// one giant stepped cake.
					const float Bl = Fbm(NX * 0.62f + WX, NZ * 0.62f + WZ, 3);
					const float Mask = SmoothStep01(0.40f, 0.72f, Bl);
					const float Q = Fbm(NX * 5.2f - WZ, NZ * 5.2f + WX, 3) + 0.09f * (NF.Ridge - 0.45f);
					const float Steps = 2.0f;
					const float Index = FMath::FloorToFloat(Q * Steps);
					const float Frac = Q * Steps - Index;
					const float Surf = (FMath::Clamp(Index + FMath::Pow(FMath::Max(0.0f, Frac - 0.72f) / 0.28f, 1.9f), 0.0f, 8.0f)) / Steps;
					return 0.026f + 0.102f * Amplitude * Gate * Mask * (0.28f + 0.72f * Surf) + 0.021f * NF.Base;
				}

				case EProvince::Reef:
				default:
					return 0.008f + 0.028f * Amplitude * NF.Ridge + 0.006f * NF.Base;
				}
			}
		}

		float RawHeight(float NX, float NZ, const FRoll& Roll, bool bNoRim)
		{
			FNoiseFields NF;
			NF.Base = Fbm(NX * 2.0f + 3.1f, NZ * 2.0f - 6.4f, 5);
			NF.Fine = Fbm(NX * 7.4f + 17.3f, NZ * 7.4f - 2.1f, 4);
			NF.Low = Fbm(NX * 0.95f + 8.7f, NZ * 0.95f + 4.2f, 3);
			NF.Ridge = FMath::Pow(1.0f - FMath::Abs(Fbm(NX * 3.6f + 21.2f, NZ * 3.6f + 7.7f, 4) * 2.0f - 1.0f), 1.42f);
			NF.RidgeFine = FMath::Pow(1.0f - FMath::Abs(Fbm(NX * 7.6f - 4.1f, NZ * 7.6f + 12.5f, 3) * 2.0f - 1.0f), 1.85f);

			const float DF = LandField(NX, NZ, Roll);
			const float Radius = FMath::Sqrt(NX * NX + NZ * NZ);
			const float Land = MaskOf(DF, Radius, bNoRim);

			// Province blend: exponential weights over the three nearest, with warped
			// coordinates so the borders wander instead of being Voronoi cells.
			float WarpX = NX + (Fbm(NX * 1.9f + 7.7f, NZ * 1.9f - 3.1f, 3) - 0.5f) * Roll.ProvinceWarp;
			float WarpZ = NZ + (Fbm(NX * 1.9f - 4.3f, NZ * 1.9f + 9.2f, 3) - 0.5f) * Roll.ProvinceWarp;
			float Best0 = 1.0e9f, Best1 = 1.0e9f, Best2 = 1.0e9f;
			int32 Id0 = 0, Id1 = 0, Id2 = 0;
			for (int32 Index = 0; Index < Roll.Provinces.Num(); ++Index)
			{
				const FProvince& Province = Roll.Provinces[Index];
				const float DX = (WarpX - Province.X) / FMath::Max(0.05f, Province.ScaleX);
				const float DZ = (WarpZ - Province.Z) / FMath::Max(0.05f, Province.ScaleZ);
				const float DD = FMath::Sqrt(DX * DX + DZ * DZ);
				if (DD < Best0) { Best2 = Best1; Id2 = Id1; Best1 = Best0; Id1 = Id0; Best0 = DD; Id0 = Index; }
				else if (DD < Best1) { Best2 = Best1; Id2 = Id1; Best1 = DD; Id1 = Index; }
				else if (DD < Best2) { Best2 = DD; Id2 = Index; }
			}

			const float K = 1.0f / (Roll.Blend + 0.004f);
			const float E0 = 1.0f;
			const float E1 = FMath::Exp(-K * (Best1 - Best0));
			const float E2 = FMath::Exp(-K * (Best2 - Best0));
			const float Sum = E0 + E1 + E2;

			float Height = 0.0f;
			const int32 Ids[3] = { Id0, Id1, Id2 };
			const float Weights[3] = { E0 / Sum, E1 / Sum, E2 / Sum };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (Weights[Index] <= 0.0015f || !Roll.Provinces.IsValidIndex(Ids[Index]))
				{
					continue;
				}
				Height += Weights[Index] * ProvinceHeight(Roll.Provinces[Ids[Index]], NX, NZ, Roll, NF);
			}

			// ---- continent skeleton -------------------------------------------
			if (DF < 1.35f)
			{
				const float Interior = FMath::Max(0.0f, 1.0f - DF);
				const float Interior2 = Interior * Interior;
				Height += Roll.InteriorRise * Interior2 * (0.32f + 0.68f * NF.Low);
				Height += Roll.TiltAmount * (NX * FMath::Cos(Roll.TiltDir) + NZ * FMath::Sin(Roll.TiltDir))
					* (0.30f + 0.70f * Interior2);

				// Orogenic belt. A smooth Gaussian wall makes drainage run along its foot; the
				// ridge noise gives the crest its own relief, so the two sides grow separate
				// networks and tributaries actually join.
				const float Perp = -NX * Roll.OrogenyDirZ + NZ * Roll.OrogenyDirX;
				const float Belt = FMath::Exp(-FMath::Pow(Perp / FMath::Max(0.02f, Roll.OrogenyHalfWidth), 2.0f));
				Height += Roll.OrogenyHeight * Belt * Interior2 * (0.40f + 0.75f * NF.Ridge);
			}

			// Lakes: shallow bowls on flat ground, with a noisy shore so they are not ellipses.
			for (const FVector& Spot : Roll.LakeSpots)
			{
				const float DX = NX - Spot.X;
				const float DZ = NZ - Spot.Y;
				const float DD = FMath::Sqrt(DX * DX + DZ * DZ);
				const float Shore = 1.0f + (Fbm(NX * 5.1f + Spot.X * 9.1f, NZ * 5.1f + Spot.Y * 7.3f, 3) - 0.5f) * 1.9f
					+ (Fbm(NX * 11.3f - Spot.Y * 4.4f, NZ * 11.3f + Spot.X * 6.2f, 2) - 0.5f) * 0.75f;
				Height -= 0.072f * FMath::Exp(-FMath::Pow(DD / (Spot.Z * FMath::Max(0.28f, Shore)), 1.55f));
			}

			// Coast falloff: mountains are not allowed to walk into the sea. df = 0 is inland,
			// df = 1 is the coast; written the other way round it does nothing at the coast.
			const float CoastFall = 1.0f - 0.70f * SmoothStep01(0.45f, 1.00f, DF);

			// Sea bed: pow(1 - land, 1.85) puts a continental shelf in front of every coast
			// (0.8 m deep at lm = 0.9, 13 m at 0.5, 48 m at 0). Linear depth put deep water
			// ten metres off the beach and made every island look like a floating plate.
			return Height * CoastFall * Land - FMath::Pow(1.0f - Land, 1.85f) * Roll.SeaDepth;
		}

		// ======================================================================
		// Absolute heights
		// ======================================================================
		namespace
		{
			/**
			 * Area quantile remap, the prototype's answer to "the world is all mountain".
			 *
			 * The shape of the height distribution is not left to the noise: the table says how
			 * much of the *land area* sits at plain / low hill / high hill / mid / high, so
			 * `mountain` is an input. qTab are the source quantiles, hTab the target heights in
			 * fractions of the realm radius.
			 */
			void RemapByArea(TArray<float>& Heights, const FRoll& Roll, float RadiusMetres)
			{
				TArray<float> Land;
				Land.Reserve(Heights.Num());
				float SeaMin = 0.0f;
				for (const float Value : Heights)
				{
					if (Value > 0.0f)
					{
						Land.Add(Value);
					}
					else
					{
						SeaMin = FMath::Min(SeaMin, Value);
					}
				}
				if (Land.Num() == 0)
				{
					return;
				}
				Land.Sort();

				const float Mountain = FMath::Clamp(Roll.Mountain, 0.05f, 0.62f);
				float QTab[6] = { 0.0f, 0.42f, 0.42f + Mountain * 1.55f, 1.0f - Mountain * 0.42f, 1.0f - Mountain * 0.14f, 1.0f };
				for (int32 Index = 1; Index < 6; ++Index)
				{
					if (QTab[Index] <= QTab[Index - 1])
					{
						QTab[Index] = FMath::Min(0.9995f, QTab[Index - 1] + 0.008f);
					}
				}
				QTab[5] = 1.0f;

				// Target heights (x radius): plain / low hill / high hill / mid mountain / high.
				// The top used to be 0.612 R - a 135 m boulder in a 220 m world, i.e. a pillar.
				// 0.300 is where it landed after that.
				//
				// Raised to 0.440 (and the whole lower ladder lifted with it, +40%) after a
				// measurement of the *distribution* rather than the peak: the bench reported
				// "height bands 0-8: 90.2%, 8-16: 6.9%" - nine tenths of the land was inside the
				// first eight metres. A 128 m island whose highest point is 21 m has a mean slope
				// of 16 degrees, and no amount of material work turns that into an island with an
				// edge; it reads as a sandbar with a lump on it. Islands have cliffs.
				const float HTab[6] = { 0.000f, 0.070f, 0.155f, 0.250f, 0.350f, 0.450f };

				const int32 Count = Land.Num();
				float SourceQ[6];
				for (int32 Index = 0; Index < 6; ++Index)
				{
					const float F = QTab[Index] * static_cast<float>(Count - 1);
					const int32 I0 = FMath::Clamp(FMath::FloorToInt(F), 0, Count - 1);
					const int32 I1 = FMath::Min(Count - 1, I0 + 1);
					const float T = F - static_cast<float>(I0);
					SourceQ[Index] = Land[I0] * (1.0f - T) + Land[I1] * T;
				}

				const float Top = FMath::Max(1.0e-6f, SourceQ[5]);
				for (float& Value : Heights)
				{
					if (Value <= 0.0f)
					{
						// Underwater, spread by relative depth: 0.010-0.082 R, enough for the
						// shallow / shelf / deep three-step read.
						const float SR = SeaMin < -1.0e-6f ? FMath::Min(1.0f, Value / SeaMin) : 0.0f;
						Value = -RadiusMetres * (0.010f + 0.082f * SR);
						continue;
					}

					int32 Segment = 0;
					while (Segment < 4 && Value > SourceQ[Segment + 1])
					{
						++Segment;
					}
					const float Span = FMath::Max(1.0e-9f, SourceQ[Segment + 1] - SourceQ[Segment]);
					const float U = FMath::Clamp((Value - SourceQ[Segment]) / Span, 0.0f, 1.0f);

					// Pure piecewise-linear remap has a slope break at every anchor and leaves five
					// "shoulders" - the stepped cake again, one layer higher. 28% of a smooth power
					// curve spreads them out while the area distribution stays quantile-driven.
					const float Raw01 = FMath::Clamp(Value / Top, 0.0f, 1.0f);
					const float Smooth = FMath::Pow(Raw01, 1.52f);
					Value = ((HTab[Segment] + (HTab[Segment + 1] - HTab[Segment]) * U) * 0.72f
						+ Smooth * HTab[5] * 0.28f) * Roll.Relief * RadiusMetres;
				}
			}
		}

		void ResolveHeights(const FRoll& Roll, int32 Size, float SeaLevelMetres, TArray<float>& OutMetres)
		{
			const int32 Count = Size * Size;
			OutMetres.SetNumUninitialized(Count);

			const float RadiusMetres = static_cast<float>(Size) * 0.5f;
			const float InvSpan = (Size > 1) ? 1.0f / static_cast<float>(Size - 1) : 0.0f;

			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					const float NX = static_cast<float>(X) * InvSpan * 2.0f - 1.0f;
					const float NZ = static_cast<float>(Y) * InvSpan * 2.0f - 1.0f;
					OutMetres[Y * Size + X] = RawHeight(NX, NZ, Roll, false);
				}
			}

			RemapByArea(OutMetres, Roll, RadiusMetres);

			// ---- relaxation and detail ----------------------------------------
			// A 5x5 box blur removes single-tile spikes before the ridge pass amplifies them.
			{
				TArray<float> Blurred;
				Blurred.SetNumUninitialized(Count);
				constexpr int32 Rad = 2;
				for (int32 Y = 0; Y < Size; ++Y)
				{
					for (int32 X = 0; X < Size; ++X)
					{
						float Sum = 0.0f;
						int32 Num = 0;
						for (int32 DY = -Rad; DY <= Rad; ++DY)
						{
							for (int32 DX = -Rad; DX <= Rad; ++DX)
							{
								const int32 SX = X + DX;
								const int32 SY = Y + DY;
								if (SX < 0 || SY < 0 || SX >= Size || SY >= Size)
								{
									continue;
								}
								Sum += OutMetres[SY * Size + SX];
								++Num;
							}
						}
						Blurred[Y * Size + X] = Sum / static_cast<float>(FMath::Max(1, Num));
					}
				}
				OutMetres = MoveTemp(Blurred);
			}

			// Fine ridges only on high ground: low ground stays smooth, and without this the
			// whole relief reads as one bar of soap.
			const float Peak = FMath::Max(1.0f, RadiusMetres * 0.30f * Roll.Relief);
			for (int32 Y = 1; Y < Size - 1; ++Y)
			{
				for (int32 X = 1; X < Size - 1; ++X)
				{
					const float NX = static_cast<float>(X) * InvSpan * 2.0f - 1.0f;
					const float NZ = static_cast<float>(Y) * InvSpan * 2.0f - 1.0f;
					float F = (OutMetres[Y * Size + X] / (Peak * 0.96f) - 0.14f) / 0.58f;
					F = FMath::Clamp(F, 0.0f, 1.0f);
					const float Detail = 1.0f - FMath::Abs(Fbm(NX * 5.8f + 31.2f, NZ * 5.8f - 12.6f, 4) * 2.0f - 1.0f);
					OutMetres[Y * Size + X] += RadiusMetres * 0.070f * F * F * (Detail * Detail - 0.30f);
				}
			}

			// Spike clamp: the ridge pass pokes isolated needles, which read as white burrs from
			// a distance. This limits *convexity*, not slope.
			{
				const float Limit = RadiusMetres * 0.020f;
				TArray<float> Clamped = OutMetres;
				for (int32 Y = 1; Y < Size - 1; ++Y)
				{
					for (int32 X = 1; X < Size - 1; ++X)
					{
						const int32 Index = Y * Size + X;
						const float NeighbourAvg = 0.25f * (OutMetres[Index - 1] + OutMetres[Index + 1]
							+ OutMetres[Index - Size] + OutMetres[Index + Size]);
						const float Delta = OutMetres[Index] - NeighbourAvg;
						if (FMath::Abs(Delta) > Limit)
						{
							Clamped[Index] = NeighbourAvg + (Delta > 0.0f ? Limit : -Limit);
						}
					}
				}
				OutMetres = MoveTemp(Clamped);
			}

			// Shift so the water line sits at `SeaLevelMetres`: below it is sea, above it land.
			for (float& Value : OutMetres)
			{
				Value += SeaLevelMetres;
			}
		}

		float PeakMetres(const FRoll& Roll, int32 SizeTiles)
		{
			return 0.300f * Roll.Relief * static_cast<float>(SizeTiles) * 0.5f;
		}
	}
}
