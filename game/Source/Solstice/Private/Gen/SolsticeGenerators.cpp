// Solstice - Gen layer: procedural geometry generators.

#include "Gen/SolsticeGenerators.h"

namespace Solstice
{
	namespace
	{
		/** A single flat card in local standee space, corners running around the rim. */
		struct FCard
		{
			FVector P0;
			FVector P1;
			FVector P2;
			FVector P3;
			FLinearColor Colour;
		};

		FCard MakeCard(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Colour)
		{
			FCard Card;
			Card.P0 = A;
			Card.P1 = B;
			Card.P2 = C;
			Card.P3 = D;
			Card.Colour = Colour;
			return Card;
		}

		/** A vertical rectangle in the local XZ plane (facing +Y). */
		FCard CardXZ(float X0, float X1, float Z0, float Z1, const FLinearColor& Colour)
		{
			return MakeCard(FVector(X0, 0.0f, Z0), FVector(X1, 0.0f, Z0), FVector(X1, 0.0f, Z1), FVector(X0, 0.0f, Z1), Colour);
		}

		/** A vertical rectangle in the local YZ plane (facing +X), for crossed standees. */
		FCard CardYZ(float Y0, float Y1, float Z0, float Z1, const FLinearColor& Colour)
		{
			return MakeCard(FVector(0.0f, Y0, Z0), FVector(0.0f, Y1, Z0), FVector(0.0f, Y1, Z1), FVector(0.0f, Y0, Z1), Colour);
		}

		/** A horizontal rectangle, used for wings and water-line fins. */
		FCard CardXY(float X0, float X1, float Y0, float Y1, float Z, const FLinearColor& Colour)
		{
			return MakeCard(FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X1, Y1, Z), FVector(X0, Y1, Z), Colour);
		}

		/**
		 * Emit a standee and its printed outline. The outline is the same card, scaled around its
		 * own centre and pushed back along its normal, so a dark rim reads on every silhouette -
		 * exactly how a printed paper standee carries an ink outline.
		 */
		void EmitCards(const TArray<FCard>& Cards, const FTransform& Transform,
			const FLinearColor& Ink, FMeshBuilder& OutBody, FMeshBuilder& OutOutline)
		{
			constexpr float OutlineScale = 1.14f;
			constexpr float OutlinePush = 2.0f;	// 2 cm behind the card

			for (const FCard& Card : Cards)
			{
				const FVector Centre = (Card.P0 + Card.P1 + Card.P2 + Card.P3) * 0.25f;
				FVector Normal = FVector::CrossProduct(Card.P1 - Card.P0, Card.P3 - Card.P0).GetSafeNormal();
				if (Normal.IsNearlyZero())
				{
					Normal = FVector::UpVector;
				}

				const FVector Corners[4] = { Card.P0, Card.P1, Card.P2, Card.P3 };
				FVector Body[4];
				FVector Rim[4];
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Body[Index] = Transform.TransformPosition(Corners[Index]);
					Rim[Index] = Transform.TransformPosition(Centre + (Corners[Index] - Centre) * OutlineScale - Normal * OutlinePush);
				}
				OutBody.QuadP(Body[0], Body[1], Body[2], Body[3], Card.Colour, Card.Colour, Card.Colour, Card.Colour);
				OutOutline.QuadP(Rim[0], Rim[1], Rim[2], Rim[3], Ink, Ink, Ink, Ink);
			}
		}

		/** Surface used for meshing: water tiles present as a flat sheet at the water level. */
		float SurfaceHeight(const FTile& Tile, float WaterLevel)
		{
			if (Tile.IsWater() && WaterLevel > Tile.Height)
			{
				return WaterLevel;
			}
			return Tile.Height;
		}

		/**
			Surface a tile contributes when a mesh vertex lands on its corner.

			Returns Fallback when the tile is missing or sits outside the realm disc, so the caller
			can clamp the sheet at the rim instead of tapering it away. This is what makes the ground
			one continuous surface instead of one plateau per tile.
		*/
		float TileSurfaceOr(const FSimKernel& Kernel, int32 X, int32 Y, float RadiusSq, float WaterLevel, float Fallback)
		{
			const FTile* Tile = Kernel.TileAt(X, Y);
			if (!Tile)
			{
				return Fallback;
			}
			const FVector C = Gen::TileToLocal(Kernel, X, Y, 0.0f);
			if (C.X * C.X + C.Y * C.Y > RadiusSq)
			{
				return Fallback;
			}
			return SurfaceHeight(*Tile, WaterLevel);
		}

		/**
			The realm edge is a torn paper cut, not a machined circle (WORLD SS1.1).

			The cull radius carries low-frequency harmonics of the azimuth, so the boundary breaks in
			irregular arcs instead of stepping around a perfect circle - the axis-aligned stair the
			review saw at R-19. The mesh cull and the rim cliff both call this, so the sheet and its
			edge always agree.
		*/
		float TornRadius(float BaseRadius, float X, float Y)
		{
			const float Angle = FMath::Atan2(Y, X);
			const float Wobble =
				0.022f * FMath::Sin(Angle * 3.0f + 0.7f) +
				0.015f * FMath::Sin(Angle * 7.0f + 2.1f) +
				0.009f * FMath::Sin(Angle * 13.0f + 4.2f);
			return BaseRadius * (1.0f + Wobble);
		}

		/** Cheapest possible paper grain, stable for a given tile so rebuilds never shimmer. */
		float CelGrain(int32 X, int32 Y)
		{
			uint32 Hash = static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u;
			Hash ^= Hash >> 13;
			Hash *= 0x5bd1e995u;
			Hash ^= Hash >> 15;
			return static_cast<float>(Hash & 0xFFu) / 255.0f;
		}

		/**
			Cel fibre, not per-tile speckle.

			The tile-locked version above was correct at one metre per tile but wrong at any real
			camera distance: adjacent tiles got independent values, and one tile is about one pixel
			from the default framing, so the whole sheet read as a moire grid - worst on water, which
			is flat and otherwise one colour. This samples the same hash on a coarse lattice and
			blends it with a smoothstep, so the variation survives pullback as fibre instead of grid.
		*/
		float CelGrainSmooth(int32 TileX, int32 TileY)
		{
			constexpr float Cell = 7.0f;			// metres per fibre patch
			const float FX = static_cast<float>(TileX) / Cell;
			const float FY = static_cast<float>(TileY) / Cell;
			const int32 X0 = FMath::FloorToInt(FX);
			const int32 Y0 = FMath::FloorToInt(FY);
			const float TX = FX - static_cast<float>(X0);
			const float TY = FY - static_cast<float>(Y0);
			const float SX = TX * TX * (3.0f - 2.0f * TX);
			const float SY = TY * TY * (3.0f - 2.0f * TY);

			const float A = CelGrain(X0, Y0);
			const float B = CelGrain(X0 + 1, Y0);
			const float C = CelGrain(X0, Y0 + 1);
			const float D = CelGrain(X0 + 1, Y0 + 1);

			return FMath::Lerp(FMath::Lerp(A, B, SX), FMath::Lerp(C, D, SX), SY);
		}
	}

	FCelPalette Gen::DefaultPalette()
	{
		return FCelPalette();
	}

	const FStandeeRecipe& Gen::StandeeRecipe(ESpeciesKind Kind)
	{
		// Indexed by ESpeciesKind; MossGrass is ground cover and has no standee.
		// Sizes are the paper-card geometry in UU, local to the tile centre, and follow the
		// real-world sizes in docs/setting/SPECIES.md SS2.
		static const TArray<FStandeeRecipe> Recipes = []()
		{
			TArray<FStandeeRecipe> All;
			All.SetNum(static_cast<int32>(ESpeciesKind::Count));

			// Shrub: 0.6-1.2 m tall, one tile of crown.
			{
				FStandeeRecipe& Recipe = All[static_cast<int32>(ESpeciesKind::Shrub)];
				Recipe.BaseColour = CelSrgb(0x5C, 0x85, 0x4D);			// leaf
				Recipe.AccentColour = CelSrgb(0x73, 0x5C, 0x3D);		// stem
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -9.0f, 9.0f, 0.0f, 42.0f, 0.0f, 1.00f, true });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -52.0f, 52.0f, 34.0f, 92.0f, 0.0f, 1.00f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -30.0f, 30.0f, 76.0f, 108.0f, 0.0f, 1.08f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::YZ, -40.0f, 40.0f, 30.0f, 86.0f, 0.0f, 0.92f, false });
			}

			// Grazer: 0.5 m tall, 0.8 m long.
			{
				FStandeeRecipe& Recipe = All[static_cast<int32>(ESpeciesKind::Grazer)];
				Recipe.BaseColour = CelSrgb(0xB8, 0x94, 0x61);			// hide
				Recipe.AccentColour = Recipe.BaseColour * 0.85f;			// belly
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -40.0f, 34.0f, 22.0f, 50.0f, 0.0f, 1.00f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, 34.0f, 50.0f, 30.0f, 50.0f, 0.0f, 0.85f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -30.0f, -22.0f, 0.0f, 24.0f, 0.0f, 0.85f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -12.0f, -4.0f, 0.0f, 24.0f, 0.0f, 0.85f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, 10.0f, 18.0f, 0.0f, 24.0f, 0.0f, 0.85f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, 24.0f, 32.0f, 0.0f, 24.0f, 0.0f, 0.85f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::YZ, -20.0f, 20.0f, 24.0f, 46.0f, 0.0f, 0.95f, false });
			}

			// Raptor: 0.4 m long, 0.8 m wingspan.
			{
				FStandeeRecipe& Recipe = All[static_cast<int32>(ESpeciesKind::Raptor)];
				Recipe.BaseColour = CelSrgb(0x57, 0x57, 0x66);			// feathers
				Recipe.AccentColour = Recipe.BaseColour * 0.85f;
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -20.0f, 22.0f, 18.0f, 34.0f, 0.0f, 1.00f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, 22.0f, 34.0f, 22.0f, 32.0f, 0.0f, 0.85f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XY, -10.0f, 6.0f, -40.0f, -6.0f, 30.0f, 1.10f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XY, -10.0f, 6.0f, 6.0f, 40.0f, 30.0f, 1.10f, false });
			}

			// Stream fish: 0.2 m long, lives only on water tiles.
			{
				FStandeeRecipe& Recipe = All[static_cast<int32>(ESpeciesKind::StreamFish)];
				Recipe.BaseColour = CelSrgb(0xC2, 0xD6, 0xE0);			// scales
				Recipe.AccentColour = Recipe.BaseColour * 0.9f;
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -10.0f, 10.0f, -4.0f, 4.0f, 0.0f, 1.00f, false });
				Recipe.Cards.Add(FStandeeCard{ EStandeePlane::XZ, -18.0f, -10.0f, -7.0f, 7.0f, 0.0f, 0.90f, false });
			}

			return All;
		}();

		const int32 Index = static_cast<int32>(Kind);
		static const FStandeeRecipe Empty;
		if (Index < 0 || Index >= Recipes.Num())
		{
			return Empty;
		}
		return Recipes[Index];
	}

	FVector Gen::TileToLocal(const FSimKernel& Kernel, int32 TileX, int32 TileY, float HeightMeters)
	{
		const float Half = static_cast<float>(Kernel.GetSize()) * 0.5f;
		return FVector(
			(static_cast<float>(TileX) - Half) * MeterUU,
			(static_cast<float>(TileY) - Half) * MeterUU,
			HeightMeters * MeterUU);
	}

	/**
	 * A low irregular cushion: an n-gon skirt rising to a slightly domed cap, corners
	 * jittered by the paper grain. See the header comment - canopies, boulders and cloud
	 * puffs are all this one solid shape.
	 */
	void Gen::Cushion(FMeshBuilder& Out, const FVector& Centre, int32 Sides, float RadiusUU, float HeightUU,
		const FLinearColor& SideColour, const FLinearColor& TopColour, int32 Seed, FVector UpAxis)
	{
		UpAxis = UpAxis.GetSafeNormal();
		if (UpAxis.IsNearlyZero())
		{
			UpAxis = FVector::UpVector;
		}
		FVector AxisX = FVector::CrossProduct(FVector::UpVector, UpAxis).GetSafeNormal();
		if (AxisX.IsNearlyZero())
		{
			AxisX = FVector(1.0f, 0.0f, 0.0f);
		}
		const FVector AxisY = FVector::CrossProduct(UpAxis, AxisX);

		const auto Wobble = [Seed](int32 Index)
		{
			return 0.80f + 0.40f * CelGrain(Seed + Index * 7, Seed * 3 + Index * 13);
		};

		TArray<int32> BaseRing, TopRing;
		BaseRing.SetNumUninitialized(Sides);
		TopRing.SetNumUninitialized(Sides);
		for (int32 Index = 0; Index < Sides; ++Index)
		{
			const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(Sides);
			const float CosA = FMath::Cos(Angle);
			const float SinA = FMath::Sin(Angle);
			const float W = Wobble(Index);
			const FVector Offset = AxisX * (CosA * RadiusUU * W) + AxisY * (SinA * RadiusUU * W);
			const FVector Outward = (AxisX * CosA + AxisY * SinA + UpAxis * 0.25f).GetSafeNormal();
			BaseRing[Index] = Out.Add(Centre + Offset, Outward,
				FVector2D(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA), SideColour);
			TopRing[Index] = Out.Add(Centre + Offset * 0.72f + UpAxis * (HeightUU * 0.62f),
				(Outward + UpAxis).GetSafeNormal(),
				FVector2D(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA),
				FMath::Lerp(SideColour, TopColour, 0.55f));
		}
		const int32 Cap = Out.Add(Centre + UpAxis * HeightUU, UpAxis, FVector2D(0.5f, 0.5f), TopColour);
		for (int32 Index = 0; Index < Sides; ++Index)
		{
			const int32 J = (Index + 1) % Sides;
			Out.Tri(BaseRing[Index], BaseRing[J], TopRing[J]);		// outward skirt wall
			Out.Tri(BaseRing[Index], TopRing[J], TopRing[Index]);
			Out.Tri(Cap, TopRing[Index], TopRing[J]);				// domed cap
		}
	}

	FLinearColor Gen::TileColour(const FTile& Tile, const FCelPalette& Palette)
	{
		if (Tile.IsWater())
		{
			// Deeper water is a darker sheet. Depth is read off the tile, which the mesh already knows.
			const float Depth = FMath::Clamp(
				(Tuning::WaterSheetShadingDepthMeters - Tile.Height) / Tuning::WaterSheetShadingDepthMeters, 0.0f, 1.0f);
			return FMath::Lerp(Palette.Water, Palette.WaterDeep, Depth);
		}

		FLinearColor Base = MaterialColor(Tile.GetMat());

		// The generator leaves almost every tile as raw Cel, so a material-only colour paints the
		// whole realm in one paper tone - which is exactly the grey sheet the reviews kept reporting
		// (the panel said "上色比例 72%" while the screen had no colour at all). Cel tiles now get
		// a height ramp instead, so relief is readable before any vegetation exists: silt at the
		// waterline, grass over the low ground, drier meadow above, pale rock on the tops.
		if (Tile.GetMat() == EMaterial::Bare)
		{
			// Height above the plinth, normalised by the height the generator can actually reach.
			// R-42: this divisor used to be a hardcoded 24 m from when the world topped out at 24,
			// and then Tuning::MaxHeightMeters (64) while the generator still capped at 24 - nearly
			// every tile clamped into the grass band, which is why the realm rendered as one green
			// sheet. Tuning::TerrainPeakMeters is the peak the profile is now built to.
			const float H = FMath::Clamp(Tile.Height / Tuning::TerrainPeakMeters, 0.0f, 1.0f);
			const FLinearColor Silt = CelSrgb(0xD9, 0xC2, 0x92);
			const FLinearColor Grass = CelSrgb(0x6F, 0x9B, 0x4A);
			const FLinearColor Meadow = CelSrgb(0x8A, 0xA9, 0x55);
			const FLinearColor Rock = CelSrgb(0x9A, 0x94, 0x88);
			Base = (H < 0.14f)
				? FMath::Lerp(Silt, Grass, H / 0.14f)
				: (H < 0.42f)
					? FMath::Lerp(Grass, Meadow, (H - 0.14f) / 0.28f)
					: FMath::Lerp(Meadow, Rock, FMath::Clamp((H - 0.42f) / 0.26f, 0.0f, 1.0f));
		}

		if (Tile.Vegetation > 0.01f)
		{
			Base = FMath::Lerp(Base, Palette.Moss,
				FMath::Clamp(Tile.Vegetation, 0.0f, 1.0f) * Palette.VegetationTintStrength);
		}
		if (Tile.Ash > 0.01f)
		{
			Base = FMath::Lerp(Base, Palette.Ash, FMath::Clamp(Tile.Ash, 0.0f, 1.0f));
		}
		// The chaos state is uncoloured paper: how much colour shows is exactly how much is painted.
		// The chaos state is uncoloured paper, but a realm generated at 0.72 painted was still 28%
		// raw paper everywhere - a uniform wash over the whole sheet. The ramp above is the colour,
		// so only a genuinely low paint value (chaos) should grey it out.
		Base = FMath::Lerp(Palette.RawBare, Base, FMath::Clamp(Tile.Painted * 1.4f, 0.0f, 1.0f));

		if (Tile.Burning > 0.0f)
		{
			Base = FMath::Lerp(Base, Palette.Ember,
				FMath::Clamp(Tile.Burning, 0.0f, 1.0f) * Palette.BurnTintStrength);
		}
		return Base;
	}

	void Gen::BuildChunk(const FSimKernel& Kernel, int32 ChunkX, int32 ChunkY, const FCelPalette& Palette, FMeshBuilder& Out)
	{
		const int32 Size = Kernel.GetSize();
		const int32 ChunkSize = Kernel.GetConfig().ChunkSize;
		const float WaterLevel = Kernel.GetSky().WaterLevel;

		const float RealmRadius = static_cast<float>(Size) * 0.5f * MeterUU;
		const float RealmRadiusSq = RealmRadius * RealmRadius;

		// Chunk-level LOD. Detail is worth paying for where the camera actually looks - the middle of
		// the disc - and worth nothing at the rim, which is seen at a glancing angle from 30k units
		// away. Splitting by chunk distance keeps the centre at 2x2 and drops the outer ring to 1x1,
		// which takes a 512 world from about 2.1M triangles to roughly 1.3M without a visible change.
		const float ChunkCentreX = (static_cast<float>(ChunkX) + 0.5f) * static_cast<float>(ChunkSize) * MeterUU - RealmRadius;
		const float ChunkCentreY = (static_cast<float>(ChunkY) + 0.5f) * static_cast<float>(ChunkSize) * MeterUU - RealmRadius;
		const float ChunkNormalised = FMath::Sqrt(ChunkCentreX * ChunkCentreX + ChunkCentreY * ChunkCentreY) / RealmRadius;
		const int32 ChunkSub = (Size <= 128) ? 3 : (ChunkNormalised < 0.55f ? 2 : 1);

		const int32 X0 = ChunkX * ChunkSize;
		const int32 Y0 = ChunkY * ChunkSize;
		const int32 X1 = FMath::Min(X0 + ChunkSize, Size);
		const int32 Y1 = FMath::Min(Y0 + ChunkSize, Size);

		constexpr float PlateThickness = 3.0f;	// the visible thickness of one paper plate

		// ---- pass 1: plates and cut edges -------------------------------------
		float MaxSurface = 0.0f;
		for (int32 Y = Y0; Y < Y1; ++Y)
		{
			for (int32 X = X0; X < X1; ++X)
			{
				const FTile* Tile = Kernel.TileAt(X, Y);
				if (!Tile)
				{
					continue;
				}
				const float Surface = SurfaceHeight(*Tile, WaterLevel);
				MaxSurface = FMath::Max(MaxSurface, Surface);

				const FVector Centre = TileToLocal(Kernel, X, Y, Surface);

				// The container is an upper hemisphere on a round paper plinth (docs/setting/WORLD.md
				// SS1.1), so the square tile grid is only the sim's addressing scheme. The boundary
				// is a torn cut rather than a circle: R-19 replaced the perfect-radius cull with
				// TornRadius, which breaks the edge in irregular arcs instead of a stair around a
				// machined circle.
				const float Torn = TornRadius(RealmRadius, Centre.X, Centre.Y);
				if (Centre.X * Centre.X + Centre.Y * Centre.Y > Torn * Torn)
				{
					continue;
				}

				const float HalfTile = MeterUU * 0.5f;
				// Water is one sheet: any per-tile variation on it turns into a moire grid, because
				// the sheet is flat and otherwise a single colour. Land gets slow fibre, softly.
				const float Grain = Tile->IsWater() ? 1.0f : (0.97f + CelGrainSmooth(X, Y) * 0.06f);
				const FLinearColor Colour = TileColour(*Tile, Palette) * Grain;

				// ---- the sheet ---------------------------------------------------------
				// One flat quad per tile made every tile edge a step; at one metre per tile that is
				// a minecraft read, not a paper model. Vertices sit on tile *corners* and carry the
				// average of the tiles meeting there, so the ground is one continuous surface and
				// only real relief shows. QuadP derives the normal from the quad itself, which is
				// what the material's hard bands read.
				const float H00 = 0.25f * (TileSurfaceOr(Kernel, X - 1, Y - 1, RealmRadiusSq, WaterLevel, Surface)
					+ TileSurfaceOr(Kernel, X, Y - 1, RealmRadiusSq, WaterLevel, Surface)
					+ TileSurfaceOr(Kernel, X - 1, Y, RealmRadiusSq, WaterLevel, Surface)
					+ Surface);
				const float H10 = 0.25f * (TileSurfaceOr(Kernel, X, Y - 1, RealmRadiusSq, WaterLevel, Surface)
					+ TileSurfaceOr(Kernel, X + 1, Y - 1, RealmRadiusSq, WaterLevel, Surface)
					+ Surface
					+ TileSurfaceOr(Kernel, X + 1, Y, RealmRadiusSq, WaterLevel, Surface));
				const float H11 = 0.25f * (Surface
					+ TileSurfaceOr(Kernel, X + 1, Y, RealmRadiusSq, WaterLevel, Surface)
					+ TileSurfaceOr(Kernel, X, Y + 1, RealmRadiusSq, WaterLevel, Surface)
					+ TileSurfaceOr(Kernel, X + 1, Y + 1, RealmRadiusSq, WaterLevel, Surface));
				const float H01 = 0.25f * (TileSurfaceOr(Kernel, X - 1, Y, RealmRadiusSq, WaterLevel, Surface)
					+ Surface
					+ TileSurfaceOr(Kernel, X - 1, Y + 1, RealmRadiusSq, WaterLevel, Surface)
					+ TileSurfaceOr(Kernel, X, Y + 1, RealmRadiusSq, WaterLevel, Surface));

				// The sim addresses the world in one-metre tiles, but that grid must not show up in
				// the picture: one quad per tile leaves a 1 m facet everywhere and the realm reads as
				// Minecraft. Subdividing each tile 3x3 with bilinear heights puts a 0.33 m facet on
				// screen instead, which the eye reads as terrain rather than blocks. The tile grid
				// stays the simulation's business.
				// Adaptive subdivision. The point of the 3x3 split is to make the facet smaller than
				// a tile, but the cost is the square of the world size: a 512x512 realm at 3x3 is
				// about 4.7 million triangles for what is a static mesh, which is most of the
				// stutter the review recorded. Dropping a level per world-size step keeps the total
				// near 500k while still leaving 0.5 m facets at 512 - twice as fine as the old
				// one-quad-per-tile sheet was.
				// Subdivision comes from the chunk LOD (see ChunkSub above): 3x3 on small worlds, 2x2
				// in the middle of a large one, 1x1 out at the rim.
				const int32 Sub = ChunkSub;
				const float InvSub = 1.0f / static_cast<float>(Sub);
				auto HeightAt = [&H00, &H10, &H11, &H01](float U, float V) -> float
				{
					const float West = FMath::Lerp(H00, H10, U);
					const float East = FMath::Lerp(H01, H11, U);
					return FMath::Lerp(West, East, V) * MeterUU;
				};

				for (int32 SubY = 0; SubY < Sub; ++SubY)
				{
					for (int32 SubX = 0; SubX < Sub; ++SubX)
					{
						const float U0 = static_cast<float>(SubX) * InvSub;
						const float U1 = static_cast<float>(SubX + 1) * InvSub;
						const float V0 = static_cast<float>(SubY) * InvSub;
						const float V1 = static_cast<float>(SubY + 1) * InvSub;

						const float XA = Centre.X - HalfTile;
						const float YA = Centre.Y - HalfTile;
						const float Span = HalfTile * 2.0f;

						const FVector S00(XA + Span * U0, YA + Span * V0, HeightAt(U0, V0));
						const FVector S10(XA + Span * U1, YA + Span * V0, HeightAt(U1, V0));
						const FVector S11(XA + Span * U1, YA + Span * V1, HeightAt(U1, V1));
						const FVector S01(XA + Span * U0, YA + Span * V1, HeightAt(U0, V1));

						// Smooth normals from the bilinear patch rather than one normal per quad. The
						// corner heights H are shared between neighbouring tiles, so this normal is
						// continuous across tile borders as well: the sheet shades as one surface
						// instead of 0.33 m facets. This is the half that actually removes the
						// "blocky" read - subdividing alone only makes the facets smaller.
						auto PatchNormal = [&H00, &H10, &H11, &H01, Span](float U, float V) -> FVector
						{
							const float DHdU = FMath::Lerp(H10 - H00, H11 - H01, V) * MeterUU / Span;
							const float DHdV = FMath::Lerp(H01 - H00, H11 - H10, U) * MeterUU / Span;
							return FVector(-DHdU, -DHdV, 1.0f).GetSafeNormal();
						};

						const int32 I00 = Out.Add(S00, PatchNormal(U0, V0), FVector2D(U0, V0), Colour);
						const int32 I10 = Out.Add(S10, PatchNormal(U1, V0), FVector2D(U1, V0), Colour);
						const int32 I11 = Out.Add(S11, PatchNormal(U1, V1), FVector2D(U1, V1), Colour);
						const int32 I01 = Out.Add(S01, PatchNormal(U0, V1), FVector2D(U0, V1), Colour);

						Out.Tri(I00, I10, I11);
						Out.Tri(I00, I11, I01);
					}
				}

				// ---- water: the surf rim and the ripple strokes --------------------------
				// art.md SS2.3: flat water colour + white ripple lines + a bright rim along the
				// shore. The rim is the one that matters at the entry framing - without it the
				// sheet is one flat blue area and reads as a hole in the terrain rather than as
				// water. Both are drawn on the water surface, lifted a hair to beat z-fighting.
				if (Tile->IsWater())
				{
					const float WaterZ = Surface * MeterUU;
					constexpr float RimWidth = 35.0f;	// 35 cm of pale water along the shore
					constexpr float RimLift = 2.0f;		// uu above the sheet
					static const int32 RNX[4] = { 1, -1, 0, 0 };
					static const int32 RNY[4] = { 0, 0, 1, -1 };

					bool bOpenWater = true;
					for (int32 Side = 0; Side < 4; ++Side)
					{
						const FTile* Neighbour = Kernel.TileAt(X + RNX[Side], Y + RNY[Side]);
						if (Neighbour && Neighbour->IsWater())
						{
							continue;		// still water that way: no rim
						}

						bOpenWater = false;
						if (!Neighbour)
						{
							continue;		// the realm rim has its own cut edge
						}
						const FVector EdgeCentre = Centre + FVector(
							RNX[Side] * HalfTile, RNY[Side] * HalfTile, 0.0f) + FVector(0.0f, 0.0f, RimLift);
						const FVector Along = (RNX[Side] != 0) ? FVector(0.0f, HalfTile, 0.0f) : FVector(HalfTile, 0.0f, 0.0f);
						const FVector Inward(-RNX[Side] * RimWidth, -RNY[Side] * RimWidth, 0.0f);
						Out.QuadP(EdgeCentre - Along, EdgeCentre + Along, EdgeCentre + Along + Inward,
							EdgeCentre - Along + Inward, Palette.Surf, Palette.Surf,
							Palette.Surf * 0.94f, Palette.Surf * 0.94f);
					}

					// Ripples: one short stroke on roughly one open-water tile in seven. Static for
					// now - drifting them needs the sheet rebuilt every frame, and at M0 the sheet
					// is 100k triangles; the strokes are here to give close-ups texture.
					if (bOpenWater && CelGrain(X * 37 + 11, Y * 41 + 7) > 0.86f)
					{
						const float Angle = CelGrain(X * 13 + 2, Y * 29 + 5) * PI;
						const float Length = (0.7f + 0.9f * CelGrain(X + 23, Y + 31)) * MeterUU * 0.5f;
						const float Thickness = 9.0f;
						const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
						const FVector Side(-Dir.Y, Dir.X, 0.0f);
						const FVector Head(Centre.X, Centre.Y, WaterZ + 1.0f);
						const FLinearColor Ink = Palette.Ripple;
						Out.QuadP(Head - Dir * Length, Head + Dir * Length,
							Head + Dir * Length + Side * Thickness, Head - Dir * Length + Side * Thickness,
							Ink, Ink, Ink, Ink);
					}
				}

				// ---- the grass layer ------------------------------------------------------
				// Tiny Glade's close-up is carried almost entirely by this: hundreds of small
				// standing blades over the sheet. Without it the terrain is a coloured plane and no
				// amount of light or grading makes it read as ground. Three crossed blades per land
				// tile above the waterline, deterministic per tile so rebuilds never shimmer.
				if (!Tile->IsWater() && Surface > 1.0f)
				{
					const FLinearColor Blade = Colour * 0.78f;
					const FLinearColor TipColour = Colour * 1.12f;
					for (int32 Tuft = 0; Tuft < 3; ++Tuft)
					{
						const float JitterX = (CelGrain(X * 3 + Tuft, Y) - 0.5f) * MeterUU * 0.7f;
						const float JitterY = (CelGrain(X, Y * 3 + Tuft) - 0.5f) * MeterUU * 0.7f;
						const float Height = 34.0f + 62.0f * CelGrain(X + Tuft, Y + Tuft);
						const float Angle = CelGrain(X * 7 + Tuft, Y * 5 + Tuft) * PI;
						const FVector Right(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
						const FVector Root(Centre.X + JitterX, Centre.Y + JitterY, Surface * MeterUU);
						const FVector Tip = Root + FVector(0.0f, 0.0f, Height);

						const int32 A = Out.Add(Root - Right * 6.0f, FVector(-Right.Y, Right.X, 0.35f), FVector2D(0.0f, 1.0f), Blade);
						const int32 B = Out.Add(Root + Right * 6.0f, FVector(-Right.Y, Right.X, 0.35f), FVector2D(1.0f, 1.0f), Blade);
						const int32 C2 = Out.Add(Tip, FVector(-Right.Y, Right.X, 0.35f), FVector2D(0.5f, 0.0f), TipColour);
						Out.Tri(A, B, C2);
					}
				}

				// ---- flowers -------------------------------------------------------------
				// Tiny Glade's meadow carries a sprinkle of white and ochre flecks that turn the
				// grass band into texture at mid distance. Two crossed horizontal diamonds per
				// flower, deterministic per tile, roughly one meadow tile in six.
				if (!Tile->IsWater() && Surface > 6.0f && Surface < 32.0f
					&& CelGrain(X * 17 + 3, Y * 11 + 9) > 0.84f)
				{
					const bool White = CelGrain(X * 5 + 1, Y * 9 + 4) > 0.5f;
					const FLinearColor Petal = White
						? CelSrgb(0xF5, 0xF0, 0xDB)
						: CelSrgb(0xE0, 0xA8, 0x33);
					const float JitterX = (CelGrain(X * 23, Y * 7) - 0.5f) * MeterUU * 0.8f;
					const float JitterY = (CelGrain(X * 7, Y * 23) - 0.5f) * MeterUU * 0.8f;
					const float HeadZ = Surface * MeterUU + 22.0f + 18.0f * CelGrain(X + 2, Y + 5);
					const float Half = 7.0f + 5.0f * CelGrain(X * 3, Y * 13);
					const FVector Head(Centre.X + JitterX, Centre.Y + JitterY, HeadZ);
					const FVector Axes[2] = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f) };
					for (int32 Pass = 0; Pass < 2; ++Pass)
					{
						const FVector& Axis = Axes[Pass];
						Out.QuadP(
							Head - Axis * Half,
							Head + Axis * Half,
							Head + Axis * Half + FVector(0.0f, 0.0f, Half * 0.6f),
							Head - Axis * Half + FVector(0.0f, 0.0f, Half * 0.6f),
							Petal, Petal, Petal * 0.92f, Petal * 0.92f);
					}
				}

				// ---- trees ---------------------------------------------------------------
				// A tile that passes a hash threshold grows one solid little tree: a tapered
				// hexagonal trunk under a canopy of three overlapping cushions, each smaller and
				// lighter than the last. The previous crossed-paper-strip trunk plus one flat
				// eight-fan read as a cut-out at every angle; stacked irregular blobs hold a
				// round, top-lit silhouette from the entry framing down to the close-up.
				if (!Tile->IsWater() && Surface > 8.0f && Surface < 38.0f
					&& CelGrain(X * 13 + 5, Y * 7 + 3) > 0.87f)
				{
					const float TrunkHeight = 120.0f + 120.0f * CelGrain(X + 11, Y + 17);
					const float CrownRadius = 90.0f + 70.0f * CelGrain(Y + 3, X + 29);
					const FVector Root(Centre.X, Centre.Y, Surface * MeterUU);

					// Tapered hexagonal trunk, one corner jitter per side so it is not a prism.
					{
						const float BaseR = 10.0f + 4.0f * CelGrain(X + 3, Y + 8);
						const float TopR = BaseR * 0.62f;
						const FLinearColor Bark = Palette.CutEdge * 0.52f;
						const FLinearColor BarkTop = Palette.CutEdge * 0.68f;
						constexpr int32 TrunkSides = 6;
						const FVector TrunkTop = Root + FVector(0.0f, 0.0f, TrunkHeight);
						int32 BaseIdx[TrunkSides];
						int32 TopIdx[TrunkSides];
						for (int32 Side = 0; Side < TrunkSides; ++Side)
						{
							const float Angle = 2.0f * PI * static_cast<float>(Side) / TrunkSides
								+ CelGrain(X + Side, Y - Side) * 0.4f;
							const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
							BaseIdx[Side] = Out.Add(Root + Dir * BaseR, Dir, FVector2D(0.0f, 1.0f), Bark);
							TopIdx[Side] = Out.Add(TrunkTop + Dir * TopR, Dir, FVector2D(0.0f, 0.0f), BarkTop);
						}
						for (int32 Side = 0; Side < TrunkSides; ++Side)
						{
							const int32 Next = (Side + 1) % TrunkSides;
							Out.Tri(BaseIdx[Side], BaseIdx[Next], TopIdx[Next]);
							Out.Tri(BaseIdx[Side], TopIdx[Next], TopIdx[Side]);
						}
					}

					// Canopy: three cushions climbing and shrinking, each rotated off the last.
					const FLinearColor LeafDark = Palette.Moss * (0.78f + 0.12f * CelGrain(X + 41, Y + 43));
					const FLinearColor LeafMid = Palette.Moss * (0.98f + 0.14f * CelGrain(X + 17, Y + 5));
					const FLinearColor LeafLit = Palette.Moss * (1.24f + 0.10f * CelGrain(X + 61, Y + 3));
					const float LayerH = CrownRadius * 0.52f;
					const float BaseZ = TrunkHeight * 0.82f;
					Cushion(Out, Root + FVector(0.0f, 0.0f, BaseZ), 7, CrownRadius, LayerH,
						LeafDark, LeafMid, X * 31 + Y * 7);
					Cushion(Out, Root + FVector(0.0f, 0.0f, BaseZ + LayerH * 0.62f), 7, CrownRadius * 0.82f, LayerH * 0.85f,
						LeafMid, LeafLit, (X + 9) * 31 + (Y + 4) * 7);
					Cushion(Out, Root + FVector(0.0f, 0.0f, BaseZ + LayerH * 1.18f), 6, CrownRadius * 0.58f, LayerH * 0.70f,
						LeafMid, LeafLit, (X + 17) * 31 + (Y + 11) * 7);
				}

				// ---- boulders -------------------------------------------------------------
				// Cold grey paper stones anchor the meadow the way Tiny Glade's ruins do: low
				// irregular cushions, one tile in twelve above the silt band, clustered by the
				// shared hash so they read as outcrops rather than sprinkles.
				if (!Tile->IsWater() && Surface > 10.0f
					&& CelGrain(X * 29 + 7, Y * 19 + 5) > 0.915f)
				{
					const float StoneR = 26.0f + 34.0f * CelGrain(X + 51, Y + 37);
					const float StoneH = StoneR * (0.55f + 0.45f * CelGrain(X + 63, Y + 21));
					const FLinearColor Stone(0.56f, 0.55f, 0.52f, 1.0f);
					const FLinearColor StoneTop(0.74f, 0.73f, 0.68f, 1.0f);
					Cushion(Out, Centre + FVector(0.0f, 0.0f, Surface * MeterUU), 6, StoneR, StoneH,
						Stone, StoneTop, X * 53 + Y * 17);
				}

				// ---- the rim: the only place a cut edge still shows ---------------------
				// Interior walls are gone with the plateaus. The realm keeps its silhouette by
				// closing the sheet against the plinth wherever there is no paper next door.
				static const int32 NX[4] = { 1, -1, 0, 0 };
				static const int32 NY[4] = { 0, 0, 1, -1 };
				static const FVector EdgeNormal[4] = {
					FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f),
					FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f)
				};

				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 NXIndex = X + NX[Side];
					const int32 NYIndex = Y + NY[Side];
					const FTile* Neighbour = Kernel.TileAt(NXIndex, NYIndex);
					if (Neighbour)
					{
						const FVector NC = Gen::TileToLocal(Kernel, NXIndex, NYIndex, 0.0f);
						const float NeighbourTorn = TornRadius(RealmRadius, NC.X, NC.Y);
						if (NC.X * NC.X + NC.Y * NC.Y <= NeighbourTorn * NeighbourTorn)
						{
							continue;		// paper next door: the sheet simply continues
						}
					}

					const float Ex = Centre.X + EdgeNormal[Side].X * HalfTile;
					const float Ey = Centre.Y + EdgeNormal[Side].Y * HalfTile;
					const FVector A = (Side == 0 || Side == 1)
						? FVector(Ex, Centre.Y - HalfTile, 0.0f)
						: FVector(Centre.X - HalfTile, Ey, 0.0f);
					const FVector B = (Side == 0 || Side == 1)
						? FVector(Ex, Centre.Y + HalfTile, 0.0f)
						: FVector(Centre.X + HalfTile, Ey, 0.0f);

					// Edge heights at both ends, so the cliff can follow the sheet's slope.
					const float TopA = (Side == 0) ? H10 : (Side == 1) ? H00 : (Side == 2) ? H01 : H00;
					const float TopB = (Side == 0) ? H11 : (Side == 1) ? H01 : (Side == 2) ? H11 : H10;

					const bool bWaterFace = Tile->IsWater();
					const FLinearColor Top = bWaterFace ? Colour : Colour * 0.78f;
					const FLinearColor Bottom = bWaterFace ? Colour * 0.72f : Palette.CutEdge * (0.90f + Grain * 0.10f);

					// The cliff is subdivided with the sheet. One wall per tile put a one-metre step
					// along an edge whose surface above it no longer has any steps, which left the rim
					// as the last place the tile grid still showed. Each segment takes the mean of its
					// two end heights so it stays a flat quad (Wall wants a constant top) at a third
					// of the step.
					constexpr float CliffDepth = 192.0f;
					for (int32 Seg = 0; Seg < Sub; ++Seg)
					{
						const float T0 = static_cast<float>(Seg) * InvSub;
						const float T1 = static_cast<float>(Seg + 1) * InvSub;
						const FVector P0 = FMath::Lerp(A, B, T0);
						const FVector P1 = FMath::Lerp(A, B, T1);
						const float TopZ = 0.5f * (FMath::Lerp(TopA, TopB, T0) + FMath::Lerp(TopA, TopB, T1)) * MeterUU + PlateThickness;
						// Collapsed to zero height on purpose. One wall per tile, each at its own
						// height, is the sawtooth edge in every screenshot: three segments per tile
						// over 177 boundary tiles is a pixel-stepped comb, not a cut edge. The cut is
						// drawn once, as a 96-segment swept band in BuildBase, so it is a smooth
						// circle at any zoom. (The variables stay used, so no unreachable-code warning.)
						Out.Wall(P0, P1, TopZ, TopZ, EdgeNormal[Side], Bottom, Top);
					}
				}
			}
		}

		// ---- pass 2: inked contour ribbons every whole metre -------------------
		// This is the topographic-model read: each metre of rise is drawn as a line, the way a
		// stacked paper relief map is drawn. Step widens when the terrain is tall, so the ribbon
		// count stays bounded.
		const int32 TopLevel = FMath::Clamp(FMath::CeilToInt(MaxSurface), 1, 64);
		// Ribbons are off. They were written for a stepped surface, where a line along each whole
		// metre read as a stacked contour map; on the continuous sheet the same geometry stands up
		// as a curtain of fins down every slope. Re-enable by restoring the old step
		// ((TopLevel > 24) ? 2 : 1) once they are drawn flat on the surface instead of vertically.
		const int32 LevelStep = 100000;
		constexpr float RibbonHalf = 5.0f;

		for (int32 Level = LevelStep; Level <= TopLevel; Level += LevelStep)
		{
			const float LevelZ = static_cast<float>(Level) * MeterUU;
			for (int32 Y = Y0; Y < Y1; ++Y)
			{
				for (int32 X = X0; X < X1; ++X)
				{
					const FTile* Tile = Kernel.TileAt(X, Y);
					if (!Tile || Tile->IsWater())
					{
						continue;
					}
					const float Surface = Tile->Height;
					if (Surface < static_cast<float>(Level) - 0.001f)
					{
						continue;
					}

					const FVector Centre = TileToLocal(Kernel, X, Y, 0.0f);
					const float HalfTile = MeterUU * 0.5f;

					static const int32 NX[4] = { 1, -1, 0, 0 };
					static const int32 NY[4] = { 0, 0, 1, -1 };
					static const FVector EdgeNormal[4] = {
						FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f),
						FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f)
					};

					for (int32 Side = 0; Side < 4; ++Side)
					{
						const FTile* Neighbour = Kernel.TileAt(X + NX[Side], Y + NY[Side]);
						const float NeighbourSurface = Neighbour ? SurfaceHeight(*Neighbour, WaterLevel) : 0.0f;
						if (NeighbourSurface >= static_cast<float>(Level) - 0.001f)
						{
							continue;
						}

						// Push the ribbon a couple of centimetres out so it never z-fights the wall.
						const float Push = 3.0f;
						const float Ex = Centre.X + EdgeNormal[Side].X * (HalfTile + Push);
						const float Ey = Centre.Y + EdgeNormal[Side].Y * (HalfTile + Push);
						const FVector A = (Side == 0 || Side == 1)
							? FVector(Ex, Centre.Y - HalfTile, 0.0f)
							: FVector(Centre.X - HalfTile, Ey, 0.0f);
						const FVector B = (Side == 0 || Side == 1)
							? FVector(Ex, Centre.Y + HalfTile, 0.0f)
							: FVector(Centre.X + HalfTile, Ey, 0.0f);

						Out.Wall(A, B, LevelZ - RibbonHalf, LevelZ + RibbonHalf, EdgeNormal[Side], Palette.Ribbon, Palette.Ribbon);
					}
				}
			}
		}
	}

	void Gen::BuildBase(const FSimKernel& Kernel, const FCelPalette& Palette, FMeshBuilder& Out)
	{
		const int32 Size = Kernel.GetSize();
		// The container is an upper hemisphere on a round paper plinth (docs/setting/WORLD.md SS1.1):
		// a square board would poke out of the dome at its four corners, so the plinth is a disc.
		const float Radius = static_cast<float>(Size) * 0.5f * MeterUU;
		const float Bottom = -192.0f;	// 1.6 m of kraft board below the lowest paper plate
		const float Top = 0.0f;

		const FLinearColor Kraft = Palette.CutEdge * 0.86f;
		const FVector Up(0.0f, 0.0f, 1.0f);
		const FVector Down(0.0f, 0.0f, -1.0f);

		constexpr int32 Segments = 96;
		constexpr float TwoPi = 6.28318530718f;

		// A disc fan: one centre vertex plus a rim ring, all at a constant Z.
		auto Disc = [&Out](float InRadius, float Z, const FVector& Normal, const FLinearColor& Colour)
		{
			const int32 Centre = Out.Add(FVector(0.0f, 0.0f, Z), Normal, FVector2D(0.5f, 0.5f), Colour);
			TArray<int32> Ring;
			Ring.Reserve(Segments);
			for (int32 Index = 0; Index < Segments; ++Index)
			{
				const float Angle = TwoPi * static_cast<float>(Index) / static_cast<float>(Segments);
				const float CosA = FMath::Cos(Angle);
				const float SinA = FMath::Sin(Angle);
				Ring.Add(Out.Add(
					FVector(CosA * InRadius, SinA * InRadius, Z),
					Normal,
					FVector2D(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA),
					Colour));
			}
			for (int32 Index = 0; Index < Segments; ++Index)
			{
				const int32 A = Ring[Index];
				const int32 B = Ring[(Index + 1) % Segments];
				// Winding follows the supplied normal, so the same fan serves either face.
				if (Normal.Z >= 0.0f) { Out.Tri(Centre, A, B); }
				else { Out.Tri(Centre, B, A); }
			}
		};

		Disc(Radius, Bottom, Down, Kraft * 0.8f);
		Disc(Radius, Top, Up, Kraft);

		// The round cut edge between the two faces.
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float A0 = TwoPi * static_cast<float>(Index) / static_cast<float>(Segments);
			const float A1 = TwoPi * static_cast<float>(Index + 1) / static_cast<float>(Segments);
			const FVector P0(FMath::Cos(A0) * Radius, FMath::Sin(A0) * Radius, 0.0f);
			const FVector P1(FMath::Cos(A1) * Radius, FMath::Sin(A1) * Radius, 0.0f);
			const FVector Normal = FVector::CrossProduct(P1 - P0, Up).GetSafeNormal();
			Out.Wall(P0, P1, Bottom, Top, Normal, Kraft * 0.7f, Kraft);
		}

		// WORLD SS1.1 第 53 轮：禁止托板 / 底座。圆盘之外什么都没有，界外是空场。
	}

	void Gen::BuildLife(const FSimKernel& Kernel, const FCelPalette& Palette, FMeshBuilder& Out, FMeshBuilder& OutOutline)
	{
		const float WaterLevel = Kernel.GetSky().WaterLevel;
		const TArray<FIndividual>& Individuals = Kernel.GetIndividuals();

		for (int32 Index = 0; Index < Individuals.Num(); ++Index)
		{
			const FIndividual& Individual = Individuals[Index];
			const ESpeciesKind Kind = Individual.GetKind();
			if (Kind == ESpeciesKind::MossGrass)
			{
				// Moss-grass is a coverage layer, not an individual (docs/setting/SPECIES.md SS2).
				continue;
			}

			const int32 Size = Kernel.GetSize();
			const int32 TileX = Individual.Tile % Size;
			const int32 TileY = Individual.Tile / Size;
			const FTile* Tile = Kernel.TileAt(TileX, TileY);
			if (!Tile)
			{
				continue;
			}
			const float Surface = (Kind == ESpeciesKind::StreamFish)
				? FMath::Max(Tile->Height, WaterLevel) * MeterUU
				: SurfaceHeight(*Tile, WaterLevel) * MeterUU;

			const FVector Origin = TileToLocal(Kernel, TileX, TileY, 0.0f) + FVector(0.0f, 0.0f, Surface);
			const FRotator Facing(0.0f, static_cast<float>(Individual.Facing) * 45.0f, 0.0f);
			const FTransform Transform(Facing, Origin, FVector(1.0f));

			// The silhouette is data, not code - see Gen::StandeeRecipe.
			const FStandeeRecipe& Recipe = StandeeRecipe(Kind);
			TArray<FCard> Cards;
			Cards.Reserve(Recipe.Cards.Num());
			for (const FStandeeCard& Spec : Recipe.Cards)
			{
				const FLinearColor Tint = (Spec.bUseAccent ? Recipe.AccentColour : Recipe.BaseColour) * Spec.Shade;
				switch (Spec.Plane)
				{
				case EStandeePlane::XZ:
					Cards.Add(CardXZ(Spec.A0, Spec.A1, Spec.B0, Spec.B1, Tint));
					break;
				case EStandeePlane::YZ:
					Cards.Add(CardYZ(Spec.A0, Spec.A1, Spec.B0, Spec.B1, Tint));
					break;
				case EStandeePlane::XY:
					Cards.Add(CardXY(Spec.A0, Spec.A1, Spec.B0, Spec.B1, Spec.HeightOffset, Tint));
					break;
				default:
					break;
				}
			}

			if (Cards.Num() > 0)
			{
				EmitCards(Cards, Transform, Palette.InkWash, Out, OutOutline);
			}
		}
	}

	namespace
	{
		// ---- sky card ---------------------------------------------------------
		// Card space (docs/art/art.md SS2.2): X right, Y up, Z = distance from the camera. The
		// gradient panel sits at Z = 1 and every layer above it has to sit at a *smaller* Z,
		// because smaller Z is nearer the camera - at 1.0002 the clouds are behind the panel and
		// the sky renders as a bare gradient. Each layer still gets its own hairline of depth, so
		// no two surfaces share a plane.
		constexpr float StarZ = 0.9994f;
		constexpr float SunZ = 0.9990f;
		constexpr float MoonZ = 0.9986f;
		constexpr float CirrusZ = 0.9980f;
		constexpr float CumulusZ = 0.9972f;

		/** How high the bright horizon band sits, in card-height units. */
		constexpr float HorizonBand = 0.07f;

		/**
		 * The three-band gradient. Above the horizon band it walks Horizon -> Middle -> Zenith;
		 * below it drops back a step so the frame has a bottom edge instead of fading to mush.
		 * The stop at 0.58 is art.md's "middle band covers 55-70% of the sky height".
		 */
		FLinearColor SkyRamp(const Gen::FSkyPaint& Paint, float V)
		{
			const float H = FMath::Clamp(Paint.HorizonV, -0.75f, 0.92f);

			if (V >= H - HorizonBand * 0.5f)
			{
				const float Span = FMath::Max(0.10f, 1.0f - H);
				const float T = FMath::Clamp((V - H) / Span, 0.0f, 1.0f);
				return (T < 0.58f)
					? FMath::Lerp(Paint.Horizon, Paint.Middle, T / 0.58f)
					: FMath::Lerp(Paint.Middle, Paint.Zenith, (T - 0.58f) / 0.42f);
			}

			const float Span = FMath::Max(0.10f, H + 1.0f);
			const float T = FMath::Clamp((H - V) / Span, 0.0f, 1.0f);
			return FMath::Lerp(Paint.Horizon, Paint.Under, FMath::Min(1.0f, T / 0.55f));
		}

		/**
		 * A flat top and a round bottom: the cumulus silhouette in one shape. The top stays
		 * nearly level (a 15% droop at the shoulders is all it takes to stop reading as a slab)
		 * and the underside is a half ellipse, so a bank of three of these reads as one cloud.
		 *
		 * Colours run from the rim's top edge to its bottom edge, which is the cel recipe:
		 * a white top, a cool base, no gradient in between.
		 */
		void SkyPuff(FMeshBuilder& Out, float Z, const FVector2D& Centre, float HalfWidth,
			float Top, float Bottom, const FLinearColor& TopColour, const FLinearColor& BaseColour)
		{
			if (HalfWidth <= 0.0f || (Top <= 0.0f && Bottom <= 0.0f))
			{
				return;
			}

			constexpr int32 Steps = 9;
			TArray<FVector2D> Rim;
			Rim.Reserve(Steps * 2);

			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const float T = static_cast<float>(Step) / static_cast<float>(Steps);
				const float X = FMath::Lerp(-1.0f, 1.0f, T);
				const float Droop = 1.0f - 0.15f * FMath::Pow(FMath::Abs(X), 6.0f);
				Rim.Add(FVector2D(X * HalfWidth, Top * Droop));
			}
			for (int32 Step = Steps - 1; Step >= 1; --Step)
			{
				const float T = static_cast<float>(Step) / static_cast<float>(Steps);
				const float X = FMath::Lerp(-1.0f, 1.0f, T);
				const float Underside = FMath::Sqrt(FMath::Max(0.0f, 1.0f - X * X));
				Rim.Add(FVector2D(X * HalfWidth, -Bottom * Underside));
			}

			const int32 CentreIndex = Out.Add(
				FVector(Centre.X, Centre.Y, Z), FVector(0.0f, 0.0f, -1.0f), FVector2D(0.5f, 0.5f),
				FMath::Lerp(BaseColour, TopColour, 0.5f));

			TArray<int32> Indices;
			Indices.Reserve(Rim.Num());
			for (const FVector2D& Point : Rim)
			{
				const float Height01 = FMath::Clamp((Point.Y + Bottom) / FMath::Max(1.0f, Top + Bottom), 0.0f, 1.0f);
				Indices.Add(Out.Add(FVector(Centre.X + Point.X, Centre.Y + Point.Y, Z), FVector(0.0f, 0.0f, -1.0f),
					FVector2D(0.5f, Height01), FMath::Lerp(BaseColour, TopColour, Height01)));
			}
			for (int32 Index = 0; Index < Indices.Num(); ++Index)
			{
				const int32 Next = (Index + 1) % Indices.Num();
				Out.Tri(CentreIndex, Indices[Index], Indices[Next]);
			}
		}

		/** A flat ring, used for the two sun halos. */
		void SkyRing(FMeshBuilder& Out, float Z, const FVector2D& Centre, float Inner, float Outer,
			const FLinearColor& Colour, int32 Sides)
		{
			if (Outer <= Inner)
			{
				return;
			}
			TArray<int32> InnerRing;
			TArray<int32> OuterRing;
			InnerRing.Reserve(Sides);
			OuterRing.Reserve(Sides);
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const float Angle = 2.0f * PI * static_cast<float>(Side) / static_cast<float>(Sides);
				const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
				InnerRing.Add(Out.Add(FVector(Centre.X + Dir.X * Inner, Centre.Y + Dir.Y * Inner, Z),
					FVector(0.0f, 0.0f, -1.0f), FVector2D(0.5f, 0.5f), Colour));
				OuterRing.Add(Out.Add(FVector(Centre.X + Dir.X * Outer, Centre.Y + Dir.Y * Outer, Z),
					FVector(0.0f, 0.0f, -1.0f), FVector2D(0.5f, 0.5f), Colour));
			}
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const int32 Next = (Side + 1) % Sides;
				Out.Quad(InnerRing[Side], InnerRing[Next], OuterRing[Next], OuterRing[Side]);
			}
		}

		/** A four-vertex diamond: at 2-4 px a star is a point, and a disc is a waste of geometry. */
		void SkyDot(FMeshBuilder& Out, float Z, const FVector2D& Centre, float Size, const FLinearColor& Colour)
		{
			const int32 A = Out.Add(FVector(Centre.X, Centre.Y + Size, Z),
				FVector(0.0f, 0.0f, -1.0f), FVector2D(0.5f, 1.0f), Colour);
			const int32 B = Out.Add(FVector(Centre.X + Size, Centre.Y, Z),
				FVector(0.0f, 0.0f, -1.0f), FVector2D(1.0f, 0.5f), Colour);
			const int32 C = Out.Add(FVector(Centre.X, Centre.Y - Size, Z),
				FVector(0.0f, 0.0f, -1.0f), FVector2D(0.5f, 0.0f), Colour);
			const int32 D = Out.Add(FVector(Centre.X - Size, Centre.Y, Z),
				FVector(0.0f, 0.0f, -1.0f), FVector2D(0.0f, 0.5f), Colour);
			Out.Quad(A, B, C, D);
		}
	}

	void Gen::BuildSkyCard(float HalfWidth, float HalfHeight, const FSkyPaint& Paint, FMeshBuilder& Out)
	{
		if (HalfWidth <= 0.0f || HalfHeight <= 0.0f)
		{
			return;
		}

		const float Cloud = FMath::Clamp(Paint.CloudCover, 0.0f, 1.0f);
		const float Wet = FMath::Clamp(Paint.Rain, 0.0f, 1.0f);
		const float H = FMath::Clamp(Paint.HorizonV, -0.75f, 0.92f);

		// ---- 1. the gradient -------------------------------------------------
		// 40 x 24 cells: the ramp is piecewise linear, so the only thing resolution buys here
		// is a smooth edge where the two legs of the gradient meet. The panel is 8% wider than
		// the frame so a rounding error in the viewport aspect can never leave a hairline of
		// clear colour along the border.
		{
			constexpr int32 Cols = 40;
			constexpr int32 Rows = 24;
			constexpr float Panel = 1.08f;
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				const float V0 = -1.0f + 2.0f * static_cast<float>(Row) / static_cast<float>(Rows);
				const float V1 = -1.0f + 2.0f * static_cast<float>(Row + 1) / static_cast<float>(Rows);
				const FLinearColor C0 = SkyRamp(Paint, V0);
				const FLinearColor C1 = SkyRamp(Paint, V1);
				for (int32 Col = 0; Col < Cols; ++Col)
				{
					const float U0 = -1.0f + 2.0f * static_cast<float>(Col) / static_cast<float>(Cols);
					const float U1 = -1.0f + 2.0f * static_cast<float>(Col + 1) / static_cast<float>(Cols);
					Out.QuadP(
						FVector(U0 * Panel * HalfWidth, V0 * Panel * HalfHeight, 1.0f),
						FVector(U1 * Panel * HalfWidth, V0 * Panel * HalfHeight, 1.0f),
						FVector(U1 * Panel * HalfWidth, V1 * Panel * HalfHeight, 1.0f),
						FVector(U0 * Panel * HalfWidth, V1 * Panel * HalfHeight, 1.0f),
						C0, C0, C1, C1);
				}
			}
		}

		// ---- 2. stars --------------------------------------------------------
		// Above the horizon only, and only as much as the night is clear. They sit behind the
		// clouds on purpose: a star through a cumulus reads as a rendering bug.
		if (Paint.StarAlpha > 0.05f)
		{
			constexpr int32 StarCount = 90;
			for (int32 Index = 0; Index < StarCount; ++Index)
			{
				const float Seed = static_cast<float>(Index) * 2.399963f;	// golden angle
				const float U = FMath::Frac(Seed * 3.11f) * 2.0f - 1.0f;
				const float V = FMath::Lerp(H + 0.06f, 0.98f, FMath::Frac(Seed * 0.717f));
				const float Size = FMath::Lerp(0.0022f, 0.0048f, FMath::Frac(Seed * 1.93f));
				SkyDot(Out, StarZ,
					FVector2D(U * HalfWidth, V * HalfHeight), Size * HalfHeight,
					Paint.Star * Paint.StarAlpha);
			}
		}

		// ---- 3. the sun: a disc and two rings of halo ------------------------
		if (Paint.bSunVisible && Paint.SunRadius > 0.0f)
		{
			// "Transparent" on an opaque card means mixing with the sky that is actually behind
			// the ring - so the sample has to be clamped onto the card. It used to be
			// SunPos.Y / HalfHeight with no clamp; once the disc was placed past the top edge the
			// sample read a V above the ramp's range, came back as the deep zenith blue, and two
			// rings of that read as a dark smudge around a white dot (2026-10-18).
			//
			// Two concentric rings with decreasing mix, per art.md SS2.2 - but *small and faint*.
			// There is no alpha over an opaque card: "opacity" is the mix ratio against the sky
			// sampled behind the ring, so a 3.2R ring at 0.34 stopped being a halo and became a
			// 250 px cream annulus across the corner of the frame (2026-10-18).
			const float SunV = FMath::Clamp(Paint.SunPos.Y / HalfHeight, -0.95f, 0.95f);
			const FLinearColor Around = SkyRamp(Paint, SunV);
			const FVector2D Centre = Paint.SunPos;
			SkyRing(Out, SunZ, Centre, Paint.SunRadius * 1.40f, Paint.SunRadius * 2.00f,
				FMath::Lerp(Around, Paint.SunHalo, 0.09f), 28);
			SkyRing(Out, SunZ, Centre, Paint.SunRadius * 1.06f, Paint.SunRadius * 1.40f,
				FMath::Lerp(Around, Paint.SunHalo, 0.26f), 28);
			Gen::BuildDisc(FVector(Centre.X, Centre.Y, SunZ),
				FVector(Paint.SunRadius, 0.0f, 0.0f), FVector(0.0f, Paint.SunRadius, 0.0f),
				1.0f, Paint.SunCore, Out);
		}

		// ---- 4. the moon -----------------------------------------------------
		if (Paint.bMoonVisible && Paint.MoonRadius > 0.0f)
		{
			Gen::BuildDisc(FVector(Paint.MoonPos.X, Paint.MoonPos.Y, MoonZ),
				FVector(Paint.MoonRadius, 0.0f, 0.0f), FVector(0.0f, Paint.MoonRadius, 0.0f),
				1.0f, Paint.MoonCore, Out);
		}

		// ---- 5. cirrus: a high thin veil of long streaks ---------------------
		// "半透明" without an alpha channel is a colour mixed most of the way into the sky
		// behind it - which is also why the streaks have to be recomputed when the sky moves.
		//
		// The band is pinned to [horizon, frame top] rather than to fractions of the whole card:
		// at the standard 35 degree framing the realm covers the bottom four fifths, so a deck
		// placed at "half height" is behind the terrain and never seen. Everything the sky has to
		// say has to fit in the strip above the silhouette.
		// V runs on the *card*, and the card is SkyCardOverscan (1.12) times the viewport, so the
		// visible top edge is at 1/1.12 = 0.893 - not at 1.0. Clamping clouds to the card edge
		// drew them half outside the frame in every dusk shot (2026-10-18).
		constexpr float CardVisibleTop = 0.88f;
		if (Cloud > 0.05f)
		{
			const float Top = FMath::Min(CardVisibleTop, FMath::Max(H + 0.16f, 0.4f));
			const int32 Streaks = FMath::RoundToInt(FMath::Lerp(4.0f, 10.0f, Cloud));
			const FLinearColor Tone = FMath::Lerp(Paint.Cirrus, FLinearColor(0.72f, 0.76f, 0.80f), Wet * 0.6f);
			for (int32 Index = 0; Index < Streaks; ++Index)
			{
				const float Seed = static_cast<float>(Index) * 2.399963f;
				const float V = FMath::Lerp(H + 0.10f, Top, FMath::Frac(Seed * 0.417f));
				// The deck loops horizontally; fading it out towards the sides means the wrap
				// happens while the streak is invisible instead of popping at the frame edge.
				const float U = FMath::Frac(FMath::Frac(Seed * 0.219f) + Paint.Drift) * 1.90f - 0.95f;
				const float Fade = 1.0f - FMath::SmoothStep(0.58f, 0.94f, FMath::Abs(U));
				if (Fade <= 0.01f)
				{
					continue;
				}
				const float Length = HalfWidth * FMath::Lerp(0.24f, 0.55f, FMath::Frac(Seed * 1.31f));
				const FLinearColor Mixed = FMath::Lerp(SkyRamp(Paint, V), Tone, 0.70f * Fade);
				SkyPuff(Out, CirrusZ, FVector2D(U * HalfWidth, V * HalfHeight),
					Length, HalfHeight * 0.013f, HalfHeight * 0.010f, Mixed, Mixed);
			}
		}

		// ---- 6. cumulus: a lower bank of flat-topped cushions ----------------
		// One bank per 12% of coverage, so a clear sky still carries a couple of small ones: a
		// completely empty gradient is the single most "not a sky" thing a frame can do.
		if (Cloud > 0.08f)
		{
			const int32 Banks = FMath::RoundToInt(FMath::Lerp(2.0f, 8.0f, (Cloud - 0.08f) / 0.92f));
			const FLinearColor Lit = FMath::Lerp(Paint.CumulusLit, FLinearColor(0.70f, 0.73f, 0.78f), Wet * 0.8f);
			const FLinearColor Base = FMath::Lerp(Paint.CumulusBase, FLinearColor(0.48f, 0.52f, 0.58f), Wet * 0.8f);
			const float Top = FMath::Min(CardVisibleTop, FMath::Max(H + 0.40f, 0.55f));
			for (int32 Index = 0; Index < Banks; ++Index)
			{
				const float Seed = static_cast<float>(Index) * 2.399963f;
				const float Width = HalfWidth * FMath::Lerp(0.07f, 0.16f, FMath::Frac(Seed * 1.07f));
				const float Height = HalfHeight * FMath::Lerp(0.040f, 0.072f, FMath::Frac(Seed * 1.51f));
				// Kept clear of the horizon band on purpose: a white cloud against the near-white
				// horizon is invisible, which is how the first pass "had no clouds" while drawing
				// ten of them. The top of the range leaves room for the bank's own height - a
				// centre at 0.99 puts half a cloud outside the card, and drift moves it there.
				const float Height01 = Height / HalfHeight;
				const float VMax = FMath::Max(H + 0.20f, Top - Height01 * 1.15f);
				const float V = FMath::Lerp(H + 0.16f, VMax, FMath::Frac(Seed * 0.613f));
				const float U = FMath::Frac(FMath::Frac(Seed * 0.377f) + Paint.Drift * 0.8f) * 1.90f - 0.95f;
				const float Fade = 1.0f - FMath::SmoothStep(0.56f, 0.93f, FMath::Abs(U));
				if (Fade <= 0.01f)
				{
					continue;
				}
				const FLinearColor LitNow = FMath::Lerp(SkyRamp(Paint, V), Lit, Fade);
				const FLinearColor BaseNow = FMath::Lerp(SkyRamp(Paint, V), Base, Fade);
				const FVector2D Centre(U * HalfWidth, V * HalfHeight);

				// Three cushions, not one: a single puff at a 3:1 width-to-height ratio with a
				// dark underside reads as a flying saucer (it did, in every dusk frame). Staggering
				// the side puffs upward and thinning the shaded base to a sliver gives a lumpy
				// silhouette with the flat top the cel recipe asks for.
				SkyPuff(Out, CumulusZ, Centre, Width, Height, Height * 0.34f, LitNow, BaseNow);
				SkyPuff(Out, CumulusZ, Centre + FVector2D(Width * 0.70f, Height * 0.34f),
					Width * 0.60f, Height * 0.80f, Height * 0.26f, LitNow, BaseNow);
				SkyPuff(Out, CumulusZ, Centre - FVector2D(Width * 0.72f, Height * 0.10f),
					Width * 0.52f, Height * 0.66f, Height * 0.22f, LitNow, BaseNow);
			}
		}
	}

	void Gen::BuildDisc(const FVector& Centre, const FVector& Right, const FVector& Up, float RadiusUU, const FLinearColor& Colour, FMeshBuilder& Out)
	{
		constexpr int32 Sides = 20;
		const FVector Normal = FVector::CrossProduct(Right, Up).GetSafeNormal();
		const int32 CentreIndex = Out.Add(Centre, Normal, FVector2D(0.5f, 0.5f), Colour);
		int32 Previous = INDEX_NONE;
		int32 First = INDEX_NONE;
		for (int32 Side = 0; Side <= Sides; ++Side)
		{
			const float Angle = 2.0f * PI * static_cast<float>(Side) / static_cast<float>(Sides);
			const FVector Rim = Centre + Right * (FMath::Cos(Angle) * RadiusUU) + Up * (FMath::Sin(Angle) * RadiusUU);
			const int32 Index = Out.Add(Rim, Normal, FVector2D(FMath::Cos(Angle) * 0.5f + 0.5f, FMath::Sin(Angle) * 0.5f + 0.5f), Colour);
			if (First == INDEX_NONE)
			{
				First = Index;
			}
			if (Previous != INDEX_NONE)
			{
				Out.Tri(CentreIndex, Previous, Index);
			}
			Previous = Index;
		}
		(void)First;
	}
}
