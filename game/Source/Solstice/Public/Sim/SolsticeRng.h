// Solstice - Sim layer: deterministic random number bank.
//
// docs/TECH.md SS2.2 rule 3 requires reproducible runs: same seed must give the
// same world. One shared generator would make draw order part of the contract, so every
// subsystem owns its own stream instead. All arithmetic is integer-only, so results do not
// drift between platforms.

#pragma once

#include "CoreMinimal.h"
#include "Sim/SolsticeTypes.h"

namespace Solstice
{
	/** splitmix64-seeded xorshift64* generator. */
	struct FRng
	{
		uint64 State = 0x853C49E6748FEA9BULL;

		void Seed(uint64 InSeed)
		{
			State = InSeed ^ 0x9E3779B97F4A7C15ULL;
			if (State == 0)
			{
				State = 0x2545F4914F6CDD1DULL;
			}
			// Warm up so nearby seeds diverge immediately.
			NextU32();
			NextU32();
		}

		uint32 NextU32()
		{
			State ^= State >> 12;
			State ^= State << 25;
			State ^= State >> 27;
			return static_cast<uint32>((State * 0x2545F4914F6CDD1DULL) >> 32);
		}

		/** Uniform in [0, 1). */
		float NextFloat()
		{
			return static_cast<float>(NextU32() >> 8) * (1.0f / 16777216.0f);
		}

		/** Uniform int in [MinInclusive, MaxExclusive). */
		int32 Range(int32 MinInclusive, int32 MaxExclusive)
		{
			const int32 Span = MaxExclusive - MinInclusive;
			if (Span <= 0)
			{
				return MinInclusive;
			}
			return MinInclusive + static_cast<int32>((static_cast<uint64>(NextU32()) * static_cast<uint64>(Span)) >> 32);
		}

		float RangeFloat(float Min, float Max)
		{
			return Min + (Max - Min) * NextFloat();
		}

		bool Chance(float Probability)
		{
			if (Probability <= 0.0f)
			{
				return false;
			}
			if (Probability >= 1.0f)
			{
				return true;
			}
			return NextFloat() < Probability;
		}

		/** Rough bell curve in [-1, 1] from three uniforms; avoids libm so it stays portable. */
		float NextCentred()
		{
			const float Sum = NextFloat() + NextFloat() + NextFloat();
			return (Sum * (2.0f / 3.0f)) - 1.0f;
		}
	};

	/** One stream per subsystem, all derived from the world seed. */
	class FRngBank
	{
	public:
		void SeedFrom(uint32 WorldSeed)
		{
			for (int32 Index = 0; Index < static_cast<int32>(ERngStream::Count); ++Index)
			{
				Streams[Index].Seed(static_cast<uint64>(WorldSeed) * 0x100000001B3ULL
					+ static_cast<uint64>(Index + 1) * 0xD6E8FEB86659FD93ULL);
			}
		}

		FRng& Stream(ERngStream Which) { return Streams[static_cast<int32>(Which)]; }
		const FRng& Stream(ERngStream Which) const { return Streams[static_cast<int32>(Which)]; }

	private:
		FRng Streams[static_cast<int32>(ERngStream::Count)];
	};
}
