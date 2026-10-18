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
				// A standee stands *in* the grass: its lower edge is occluded by whatever it grows out of.
				OutBody.QuadP(Body[0], Body[1], Body[2], Body[3], Card.Colour, Card.Colour, Card.Colour, Card.Colour, 0.80f);
				OutOutline.QuadP(Rim[0], Rim[1], Rim[2], Rim[3], Ink, Ink, Ink, Ink, 0.80f);
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

		/** Terrain surface height of one tile, with a fallback for tiles outside the grid. */
		float TileSurface(const FSimKernel& Kernel, int32 X, int32 Y, float WaterLevel, float Fallback)
		{
			const FTile* Tile = Kernel.TileAt(X, Y);
			return Tile ? SurfaceHeight(*Tile, WaterLevel) : Fallback;
		}

		/**
		 * Height of the *rendered* sheet at an arbitrary local point.
		 *
		 * This repeats BuildChunk's bilinear patch on purpose: the cut edge has to start exactly
		 * where the sheet ends, and the only way to guarantee that is to evaluate the same
		 * function. Sampling the tile height instead leaves a hairline between ground and wall
		 * wherever a hillside is cut by the boundary.
		 */
		float SheetHeightAt(const FSimKernel& Kernel, float LocalX, float LocalY, float WaterLevel)
		{
			const float Half = static_cast<float>(Kernel.GetSize()) * 0.5f;
			const float GX = LocalX / MeterUU + Half;
			const float GY = LocalY / MeterUU + Half;
			const int32 X = FMath::FloorToInt(GX);
			const int32 Y = FMath::FloorToInt(GY);
			const float U = FMath::Clamp(GX - static_cast<float>(X), 0.0f, 1.0f);
			const float V = FMath::Clamp(GY - static_cast<float>(Y), 0.0f, 1.0f);

			const FTile* Tile = Kernel.TileAt(X, Y);
			const float Fallback = Tile ? SurfaceHeight(*Tile, WaterLevel) : WaterLevel;
			const auto S = [&Kernel, WaterLevel, Fallback](int32 NX, int32 NY)
			{
				return TileSurface(Kernel, NX, NY, WaterLevel, Fallback);
			};

			const float H00 = 0.25f * (S(X - 1, Y - 1) + S(X, Y - 1) + S(X - 1, Y) + S(X, Y));
			const float H10 = 0.25f * (S(X, Y - 1) + S(X + 1, Y - 1) + S(X, Y) + S(X + 1, Y));
			const float H11 = 0.25f * (S(X, Y) + S(X + 1, Y) + S(X, Y + 1) + S(X + 1, Y + 1));
			const float H01 = 0.25f * (S(X - 1, Y) + S(X, Y) + S(X - 1, Y + 1) + S(X, Y + 1));

			const float West = FMath::Lerp(H00, H10, U);
			const float East = FMath::Lerp(H01, H11, U);
			return FMath::Lerp(West, East, V) * MeterUU;
		}

		/**
		 * Copy the triangles of one object, scaled about its own pivot, as ink.
		 *
		 * `From`/`To` are vertex indices into `Src`, `TriFrom` a triangle index. Both are recorded
		 * by the caller just before and after it emits the object, so this is exact - no normal
		 * guessing, no re-tessellation.
		 */
		void EmitHull(const FMeshBuilder& Src, int32 From, int32 To, int32 TriFrom,
			const FVector& Pivot, float Scale, const FLinearColor& Ink, FMeshBuilder& Hull)
		{
			if (To <= From)
			{
				return;
			}
			const int32 Base = Hull.Num();
			for (int32 Index = From; Index < To; ++Index)
			{
				const FVector Position = Pivot + (Src.Positions[Index] - Pivot) * Scale;
				Hull.Add(Position, Src.Normals[Index], Src.UVs[Index], Ink, 1.0f);
			}
			for (int32 Index = TriFrom; Index + 2 < Src.Triangles.Num(); Index += 3)
			{
				const int32 A = Src.Triangles[Index];
				const int32 B = Src.Triangles[Index + 1];
				const int32 C = Src.Triangles[Index + 2];
				if (A < From || B < From || C < From || A >= To || B >= To || C >= To)
				{
					break;
				}
				Hull.Tri(Base + (A - From), Base + (B - From), Base + (C - From));
			}
		}

		/** One vertex of the terrain sheet while it is being clipped. */
		struct FSheetCorner
		{
			FVector P;
			FVector N;
			FVector2D UV;
		};

		/**
		 * Sutherland-Hodgman against the circle |P.xy| = Radius.
		 *
		 * The distance is evaluated per vertex and the crossing point interpolated linearly along
		 * the edge. Edges here are at most one tile long against a radius of sixty-four metres, so
		 * the chord error is a centimetre - invisible, and far cheaper than an exact circle solve.
		 */
		void ClipPolyToDisc(const FSheetCorner* Poly, int32 Count, float Radius, TArray<FSheetCorner>& Out)
		{
			Out.Reset();
			if (Count < 3)
			{
				return;
			}
			Out.Reserve(Count + 2);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FSheetCorner& A = Poly[Index];
				const FSheetCorner& B = Poly[(Index + 1) % Count];
				const float DA = FVector2D(A.P.X, A.P.Y).Size() - Radius;
				const float DB = FVector2D(B.P.X, B.P.Y).Size() - Radius;

				if (DA <= 0.0f)
				{
					Out.Add(A);
				}
				if ((DA < 0.0f) != (DB < 0.0f))
				{
					const float T = DA / (DA - DB);
					FSheetCorner Mid;
					Mid.P = FMath::Lerp(A.P, B.P, T);
					Mid.N = FMath::Lerp(A.N, B.N, T).GetSafeNormal();
					Mid.UV = FMath::Lerp(A.UV, B.UV, T);
					Out.Add(Mid);
				}
			}
		}

		/**
		 * The realm edge is a torn paper cut, not a machined circle (WORLD SS1.1).
		 *
		 * The cull radius carries low-frequency harmonics of the azimuth, so the boundary breaks in
		 * irregular arcs instead of stepping around a perfect circle - the axis-aligned stair the
		 * review saw at R-19. Defined once in Gen::TornRadius (declared in the header) so the sheet
		 * clip, the cut edge and the standee cull can never disagree.
		 */
		float TornRadiusLocal(float BaseRadius, float X, float Y)
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

	float Gen::TornRadius(float BaseRadius, float X, float Y)
	{
		return TornRadiusLocal(BaseRadius, X, Y);
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
			// Occlusion by ring: the skirt is in the object's own shade, the cap is open sky. This is
			// the whole reason a canopy now reads as a volume instead of a flat green ball - M_Cel is
			// unlit, so without it every ring is lit identically.
			BaseRing[Index] = Out.Add(Centre + Offset, Outward,
				FVector2D(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA), SideColour, 0.58f);
			TopRing[Index] = Out.Add(Centre + Offset * 0.72f + UpAxis * (HeightUU * 0.62f),
				(Outward + UpAxis).GetSafeNormal(),
				FVector2D(0.5f + 0.5f * CosA, 0.5f + 0.5f * SinA),
				FMath::Lerp(SideColour, TopColour, 0.55f), 0.84f);
		}
		const int32 Cap = Out.Add(Centre + UpAxis * HeightUU, UpAxis, FVector2D(0.5f, 0.5f), TopColour, 1.0f);
		for (int32 Index = 0; Index < Sides; ++Index)
		{
			const int32 J = (Index + 1) % Sides;
			Out.Tri(BaseRing[Index], BaseRing[J], TopRing[J]);		// outward skirt wall
			Out.Tri(BaseRing[Index], TopRing[J], TopRing[Index]);
			Out.Tri(Cap, TopRing[Index], TopRing[J]);				// domed cap
		}
	}

	FLinearColor Gen::TileColour(const FTile& Tile, float HeightAboveSeaMetres, float Slope01, const FCelPalette& Palette)
	{
		if (Tile.IsWater())
		{
			// Water is painted in BuildChunk, where the water line is known (the sheet needs
			// `BaseWaterLevel - Tile.Height` in metres, not a height fraction). This is the flat
			// fallback for anything that asks without that context.
			return Palette.WaterMid;
		}

		FLinearColor Base = MaterialColor(Tile.GetMat());

		// The generator leaves almost every tile as raw Cel, so a material-only colour paints the
		// whole realm in one paper tone - which is exactly the grey sheet the reviews kept reporting
		// (the panel said "上色比例 72%" while the screen had no colour at all). Cel tiles now get
		// a height ramp instead, so relief is readable before any vegetation exists: silt at the
		// waterline, grass over the low ground, drier meadow above, pale rock on the tops.
		// Which materials are painted by the height ramp below, instead of by their own swatch.
		//
		// Only `Bare` used to qualify, and everything else fell through to MaterialColor() - so a
		// realm whose generator happened to mark its ground as Clay came out as #8A6A4B over the
		// entire island. That is exactly the red-brown lump in the 2026-10-18 screenshot: the
		// land was not badly lit, it was never painted at all. Ground that can hold plants has to
		// be painted as ground (sand, grass, rock with height), not as a material sample; only
		// materials that *are* the look - stone, ore, magma, ice, snow, ash - keep their swatch.
		const EMaterial Mat = Tile.GetMat();
		const bool bPaintByHeight =
			Mat == EMaterial::Bare || Mat == EMaterial::Soil || Mat == EMaterial::Clay ||
			Mat == EMaterial::Mud || Mat == EMaterial::Sand || Mat == EMaterial::DeadWood ||
			Mat == EMaterial::Tissue;

		if (bPaintByHeight)
		{
			// Four bands, in metres above the water line. They are absolute because the remap
			// spends the land budget on the low ground: ~40% of the land sits under 2 m and only a
			// few percent reaches the peak, so any fraction-of-peak threshold bands the whole
			// island at once (tried twice: once all meadow, once all beach).
			const float H = HeightAboveSeaMetres;
			// Almost white, not ochre. A tropical shoreline is bleached coral sand; at #D9C292
			// it read as wet mud and pulled the whole island warm-brown.
			const FLinearColor Silt = CelSrgb(0xF2, 0xE6, 0xC8);
			// The three greens are art.md SS2.3's own numbers (#7FBF5F / #5F9E4A / #3F7A3C), in
			// that order: sunlit meadow, grass, and the shaded face of a slope.
			const FLinearColor Meadow = CelSrgb(0x7F, 0xBF, 0x5F);
			const FLinearColor Grass = CelSrgb(0x5F, 0x9E, 0x4A);
			const FLinearColor GrassDark = CelSrgb(0x3F, 0x7A, 0x3C);
			const FLinearColor Rock = CelSrgb(0x9A, 0x94, 0x88);
			const FLinearColor RockDark = CelSrgb(0x71, 0x6C, 0x64);

			// Four *hard* stops, not a lerp (art.md SS2.3: "色带 2-3 档同族色，硬边切换"). The lerp
			// this replaces produced a perfectly smooth gradient, which is the other half of why
			// the realm read as a featureless green wash: every silhouette had to be found from
			// lighting alone, and the material's bands are uniform on a gentle slope.
			//
			// Slope overrides height *within* a zone rather than globally: a hillside is a darker
			// shade of its own green, not stone. Painting every slope grey turned the realm into a
			// rock pile at the first attempt (measured: saturation 0.351 → 0.218, 2026-10-18).
			// Slope01 is 1 - N.z, so 1 m of rise over 2 m of ground is 0.11.
			// 20 m, not 11. The peak is now 29.5 m (the profile was raised on 2026-10-18), so an
			// 11 m threshold turned the top third of the island into bare rock - and rock, being
			// dark, read as a burnt crater sitting on a meadow. A third of the height is a lot of
			// island to give away; 20 m keeps it to the summit.
			const bool bHigh = H >= 20.0f;
			const bool bSteep = Slope01 > 0.42f;
			Base = bHigh
				? (bSteep ? RockDark : Rock)
				: (H < 0.6f)
					? Silt
					: (bSteep)
						? GrassDark
						: (H < 4.5f)
							? Grass
							: Meadow;
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

	void Gen::BuildChunk(const FSimKernel& Kernel, int32 ChunkX, int32 ChunkY, const FCelPalette& Palette, FMeshBuilder& Out, FMeshBuilder& OutHull)
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
				//
				// Culling by tile *centre* still left a one-metre staircase (the review's "comb"),
				// because a boundary that passes through the middle of a row of tiles can only be
				// drawn as whole tiles. So the test is now per corner: tiles wholly inside are
				// emitted as before, tiles the boundary crosses are clipped to the same curve, and
				// land the tile carries (grass, trees, stones) is dropped there - a blade rooted in
				// a tile that no longer exists is one of the specks that hang past the edge.
				const float HalfTile = MeterUU * 0.5f;
				const float Torn = TornRadius(RealmRadius, Centre.X, Centre.Y);
				float CornerMin = TNumericLimits<float>::Max();
				float CornerMax = 0.0f;
				for (int32 CornerY = 0; CornerY < 2; ++CornerY)
				{
					for (int32 CornerX = 0; CornerX < 2; ++CornerX)
					{
						const float DX = Centre.X + (CornerX ? HalfTile : -HalfTile);
						const float DY = Centre.Y + (CornerY ? HalfTile : -HalfTile);
						const float Dist = FMath::Sqrt(DX * DX + DY * DY);
						CornerMin = FMath::Min(CornerMin, Dist);
						CornerMax = FMath::Max(CornerMax, Dist);
					}
				}
				if (CornerMin > Torn)
				{
					continue;
				}
				const bool bClipped = CornerMax > Torn;

				// Water is one sheet: any per-tile variation on it turns into a moire grid, because
				// the sheet is flat and otherwise a single colour. Land gets slow fibre, softly.
				const float Grain = Tile->IsWater() ? 1.0f : (0.97f + CelGrainSmooth(X, Y) * 0.06f);

				// Water: depth in metres, three stops (art.md SS2.3 / the prototype's palette).
				// Depth is read off the tile, not the vertex - the sheet is flat at the water line,
				// so a per-vertex height would report zero depth everywhere and paint one colour.
				const auto WaterColour = [&Palette, WaterLevel, Tile]() -> FLinearColor
				{
					const float Depth = FMath::Max(0.0f, WaterLevel - Tile->Height);
					const float T01 = FMath::Clamp(Depth / Tuning::WaterSheetShadingDepthMeters, 0.0f, 1.0f);
					return (T01 < 0.5f)
						? FMath::Lerp(Palette.Water, Palette.WaterMid, T01 * 2.0f)
						: FMath::Lerp(Palette.WaterMid, Palette.WaterDeep, (T01 - 0.5f) * 2.0f);
				};

				// Height bands are measured from the *water line*, not from the plinth: the world's
				// land starts at `BaseWaterLevel`, so dividing the raw height by the peak put every
				// tile above 0.3 and painted the whole island one band of meadow.
				const FLinearColor Colour = (Tile->IsWater() ? WaterColour() : TileColour(*Tile, Tile->Height - WaterLevel, 0.0f, Palette)) * Grain;

				// Per-vertex ground colour: the bands are contours and cliff faces, so they have to
				// be sampled where the vertex actually is. Water keeps the single tile colour.
				const auto GroundColour = [&](const FVector& P, const FVector& N) -> FLinearColor
				{
					return Tile->IsWater()
						? Colour
						: TileColour(*Tile, P.Z / MeterUU - WaterLevel, FMath::Clamp(1.0f - N.Z, 0.0f, 1.0f), Palette) * Grain;
				};

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

				// Curvature occlusion: a vertex that sits below the average of its own tile's corners
				// is in a hollow, and gets darker. Cheap (no ray casts), stable between rebuilds, and
				// it is what gives valleys and the foot of a ridge a readable crease.
				const float LocalAvg = 0.25f * (H00 + H10 + H11 + H01) * MeterUU;
				const auto GroundOcclusion = [LocalAvg, Tile](const FVector& P) -> float
				{
					if (Tile->IsWater())
					{
						return 1.0f;
					}
					const float Below = FMath::Max(0.0f, LocalAvg - P.Z);
					return FMath::Clamp(1.0f - Below / 70.0f, 0.45f, 1.0f);
				};

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

						// Water tiles emit **no sheet**.
						//
						// The world had two water surfaces: these tiles (lit M_Cel, colour from a
						// depth ramp) and the open ocean (emissive M_Sea). Two colour models for
						// the same material can never agree, and the seam is the navy ring that sat
						// around every island - 5 cm of tile sheet floating above the ocean.
						// There is now exactly one water surface, Gen::BuildOcean, which starts at
						// the centre of the world. The shore rim and the ripples stay: they are
						// lifted above the ocean plane and are the only water detail the near view
						// actually reads.
						if (Tile->IsWater())
						{
						}
						else if (!bClipped)
						{
							const int32 I00 = Out.Add(S00, PatchNormal(U0, V0), FVector2D(U0, V0), GroundColour(S00, PatchNormal(U0, V0)), GroundOcclusion(S00));
							const int32 I10 = Out.Add(S10, PatchNormal(U1, V0), FVector2D(U1, V0), GroundColour(S10, PatchNormal(U1, V0)), GroundOcclusion(S10));
							const int32 I11 = Out.Add(S11, PatchNormal(U1, V1), FVector2D(U1, V1), GroundColour(S11, PatchNormal(U1, V1)), GroundOcclusion(S11));
							const int32 I01 = Out.Add(S01, PatchNormal(U0, V1), FVector2D(U0, V1), GroundColour(S01, PatchNormal(U0, V1)), GroundOcclusion(S01));

							Out.Tri(I00, I10, I11);
							Out.Tri(I00, I11, I01);
						}
						else
						{
							// Boundary tile: clip the sub-quad to the torn circle and emit it as a
							// fan. Only the ~200 tiles the edge crosses take this path.
							const FSheetCorner Poly[4] = {
								{ S00, PatchNormal(U0, V0), FVector2D(U0, V0) },
								{ S10, PatchNormal(U1, V0), FVector2D(U1, V0) },
								{ S11, PatchNormal(U1, V1), FVector2D(U1, V1) },
								{ S01, PatchNormal(U0, V1), FVector2D(U0, V1) }
							};
							TArray<FSheetCorner> Clipped;
							ClipPolyToDisc(Poly, 4, Torn, Clipped);
							if (Clipped.Num() >= 3)
							{
								const int32 First = Out.Add(Clipped[0].P, Clipped[0].N, Clipped[0].UV, GroundColour(Clipped[0].P, Clipped[0].N), GroundOcclusion(Clipped[0].P));
								int32 Prev = Out.Add(Clipped[1].P, Clipped[1].N, Clipped[1].UV, GroundColour(Clipped[1].P, Clipped[1].N), GroundOcclusion(Clipped[1].P));
								for (int32 Index = 2; Index < Clipped.Num(); ++Index)
								{
									const int32 Next = Out.Add(Clipped[Index].P, Clipped[Index].N, Clipped[Index].UV,
										GroundColour(Clipped[Index].P, Clipped[Index].N), GroundOcclusion(Clipped[Index].P));
									Out.Tri(First, Prev, Next);
									Prev = Next;
								}
							}
						}
					}
				}

				// ---- water: the surf rim and the ripple strokes --------------------------
				// art.md SS2.3: flat water colour + white ripple lines + a bright rim along the
				// shore. The rim is the one that matters at the entry framing - without it the
				// sheet is one flat blue area and reads as a hole in the terrain rather than as
				// water. Both are drawn on the water surface, lifted a hair to beat z-fighting.
				if (Tile->IsWater() && !bClipped)
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
				if (!bClipped && !Tile->IsWater() && Surface > 1.0f)
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
				if (!bClipped && !Tile->IsWater() && Surface > 6.0f && Surface < 32.0f
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
				if (!bClipped && !Tile->IsWater() && Surface > 8.0f && Surface < 38.0f
					&& CelGrain(X * 13 + 5, Y * 7 + 3) > 0.87f)
				{
					const int32 HullFrom = Out.Num();
					const int32 HullTriFrom = Out.Triangles.Num();
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
							BaseIdx[Side] = Out.Add(Root + Dir * BaseR, Dir, FVector2D(0.0f, 1.0f), Bark, 0.55f);
							TopIdx[Side] = Out.Add(TrunkTop + Dir * TopR, Dir, FVector2D(0.0f, 0.0f), BarkTop, 0.85f);
						}
						for (int32 Side = 0; Side < TrunkSides; ++Side)
						{
							const int32 Next = (Side + 1) % TrunkSides;
							Out.Tri(BaseIdx[Side], BaseIdx[Next], TopIdx[Next]);
							Out.Tri(BaseIdx[Side], TopIdx[Next], TopIdx[Side]);
						}
					}

					// Canopy: three cushions climbing and shrinking, each rotated off the last.
					// Canopies are a *darker, cooler* green than the grass, not the same green at another
					// brightness: the prototype's tree palette note is explicit about this ("树冠要比草地
					// 更深... 靠深绿的团和亮绿的草拉开层次"), and with one palette for both, an island of
					// trees and grass reads as a single green carpet.
					const FLinearColor LeafDark = CelSrgb(0x2A, 0x55, 0x30) * (0.94f + 0.10f * CelGrain(X + 41, Y + 43));
					const FLinearColor LeafMid = CelSrgb(0x3C, 0x74, 0x38) * (0.96f + 0.10f * CelGrain(X + 17, Y + 5));
					const FLinearColor LeafLit = CelSrgb(0x55, 0x95, 0x45) * (0.98f + 0.08f * CelGrain(X + 61, Y + 3));
					const float LayerH = CrownRadius * 0.52f;
					const float BaseZ = TrunkHeight * 0.82f;
					Cushion(Out, Root + FVector(0.0f, 0.0f, BaseZ), 7, CrownRadius, LayerH,
						LeafDark, LeafMid, X * 31 + Y * 7);
					Cushion(Out, Root + FVector(0.0f, 0.0f, BaseZ + LayerH * 0.62f), 7, CrownRadius * 0.82f, LayerH * 0.85f,
						LeafMid, LeafLit, (X + 9) * 31 + (Y + 4) * 7);
					Cushion(Out, Root + FVector(0.0f, 0.0f, BaseZ + LayerH * 1.18f), 6, CrownRadius * 0.58f, LayerH * 0.70f,
						LeafMid, LeafLit, (X + 17) * 31 + (Y + 11) * 7);

					// Ink hull around the whole tree, pivoted at its root so the crown grows outward
					// rather than sliding away from the trunk.
					EmitHull(Out, HullFrom, Out.Num(), HullTriFrom,
						Root + FVector(0.0f, 0.0f, TrunkHeight * 0.55f), 1.09f, Palette.InkWash, OutHull);
				}

				// ---- boulders -------------------------------------------------------------
				// Cold grey paper stones anchor the meadow the way Tiny Glade's ruins do: low
				// irregular cushions, one tile in twelve above the silt band, clustered by the
				// shared hash so they read as outcrops rather than sprinkles.
				if (!bClipped && !Tile->IsWater() && Surface > 10.0f
					&& CelGrain(X * 29 + 7, Y * 19 + 5) > 0.915f)
				{
					const int32 HullFrom = Out.Num();
					const int32 HullTriFrom = Out.Triangles.Num();
					const float StoneR = 26.0f + 34.0f * CelGrain(X + 51, Y + 37);
					const float StoneH = StoneR * (0.55f + 0.45f * CelGrain(X + 63, Y + 21));
					const FLinearColor Stone(0.56f, 0.55f, 0.52f, 1.0f);
					const FLinearColor StoneTop(0.74f, 0.73f, 0.68f, 1.0f);
					// Centre already carries the tile surface height in Z - adding it again put every
					// boulder at twice its ground height, i.e. a ring of pale cubes hanging in the sky
					// above the hills (the "specks" that survived every edge fix, 2026-10-18).
					Cushion(Out, FVector(Centre.X, Centre.Y, Surface * MeterUU), 6, StoneR, StoneH,
						Stone, StoneTop, X * 53 + Y * 17);

					EmitHull(Out, HullFrom, Out.Num(), HullTriFrom,
						FVector(Centre.X, Centre.Y, Surface * MeterUU), 1.10f, Palette.InkWash, OutHull);
				}

				// The rim wall used to live here: one zero-height wall per tile edge, i.e. the very
				// comb it was written to avoid, kept alive by a comment claiming the cut was drawn
				// elsewhere. It is drawn elsewhere now (Gen::BuildBase sweeps the same TornRadius the
				// sheet is clipped to), so the dead geometry is gone rather than disabled.
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
		const float Radius = static_cast<float>(Size) * 0.5f * MeterUU;

		// WORLD SS1.1: the world is a *thin slice of earth*, and the cut edge is a single dark
		// line - "0.03 R thick, one colour, no highlight, no shading, no rolled lip", colour
		// #080B18, i.e. darker than the void behind it. Everything the old plinth did contradicted
		// that: it was a 1.9 m kraft board in *cut-edge beige*, its top disc was wider than the
		// terrain (a sand-coloured ring peeking out around the world at every framing), and its
		// radius was a machined circle while the terrain's edge was torn. The whole thing read as
		// "a cake on a plate", which is exactly the "no plinth / no base" line WORLD draws.
		//
		// So: the wall follows the same TornRadius the sheet is clipped to, its thickness is 3% of
		// the radius (not a hardcoded 192 uu that would vanish on a 512 realm), and every face of
		// it - wall, top, bottom - is the same near-black. Seen from the entry framing it is a
		// thin dark line under the land, which is what "the world ends here" should look like.
		const FLinearColor CutInk = CelSrgb(0x08, 0x0B, 0x18);
		const float Thickness = Radius * 0.03f;
		const float Bottom = -Thickness;
		const FVector Up(0.0f, 0.0f, 1.0f);
		const FVector Down(0.0f, 0.0f, -1.0f);
		const float WaterLevel = Kernel.GetSky().WaterLevel;

		// The wall has to start where the *sheet* ends, which is not Z = 0: the ground is a surface
		// with hills on it, so its boundary is at whatever height the profile has there. A wall that
		// started at Z = 0 left the underside of the sheet open, and the flat cap under it showed
		// through as a black plane wherever the camera looked under the rim (2026-10-18).
		//
		// The lip lifts the wall a few centimetres above the sampled height so the seam is never a
		// gap. It reads as the hard outline art.md asks for at the world's edge.
		constexpr float LipUU = 30.0f;

		constexpr int32 Segments = 192;		// one segment every ~2 m at a 128 m realm
		constexpr float TwoPi = 6.28318530718f;

		TArray<FVector2D> Rim;
		TArray<float> RimTop;
		Rim.Reserve(Segments);
		RimTop.Reserve(Segments);
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float Angle = TwoPi * static_cast<float>(Index) / static_cast<float>(Segments);
			const float CosA = FMath::Cos(Angle);
			const float SinA = FMath::Sin(Angle);
			const float RimRadius = TornRadius(Radius, CosA, SinA);
			const FVector2D Point(CosA * RimRadius, SinA * RimRadius);
			Rim.Add(Point);
			// The cut edge used to follow the ground up the hillside. With an open sea around the
			// world that wall is a black ring on the water: keep the whole edge under the water
			// line, so the land simply rises out of the sea.
			const float UnderWater = WaterLevel * MeterUU - 90.0f;
			RimTop.Add(FMath::Clamp(SheetHeightAt(Kernel, Point.X, Point.Y, WaterLevel) + LipUU, Bottom, UnderWater));
		}

		// Underside only. The top cap is gone with the exposed-plane bug: the sheet covers the top,
		// and a cap at Z = 0 could only ever be seen through a hole.
		{
			const int32 Centre = Out.Add(FVector(0.0f, 0.0f, Bottom), Down, FVector2D(0.5f, 0.5f), CutInk);
			TArray<int32> Ring;
			Ring.Reserve(Rim.Num());
			for (const FVector2D& Point : Rim)
			{
				Ring.Add(Out.Add(FVector(Point.X, Point.Y, Bottom), Down, FVector2D(0.5f, 0.5f), CutInk));
			}
			for (int32 Index = 0; Index < Ring.Num(); ++Index)
			{
				Out.Tri(Centre, Ring[(Index + 1) % Ring.Num()], Ring[Index]);
			}
		}

		for (int32 Index = 0; Index < Rim.Num(); ++Index)
		{
			const int32 Next = (Index + 1) % Rim.Num();
			const FVector P0(Rim[Index].X, Rim[Index].Y, 0.0f);
			const FVector P1(Rim[Next].X, Rim[Next].Y, 0.0f);
			const FVector Normal = FVector::CrossProduct(P1 - P0, Up).GetSafeNormal();
			// Single colour top to bottom: the spec calls the cut edge a flat dark band.
			Out.Wall(P0, P1, Bottom, FMath::Max(RimTop[Index], RimTop[Next]), Normal, CutInk, CutInk);
		}
	}

	void Gen::BuildOcean(float InnerRadiusUU, float OuterRadiusUU, float SeaLevelUU,
		const FCelPalette& Palette, const FLinearColor& HorizonHaze, FMeshBuilder& Out)
	{
		constexpr int32 Segments = 160;
		constexpr int32 Rings = 18;
		const float Inner = FMath::Max(1.0f, InnerRadiusUU);
		const float Outer = FMath::Max(Inner * 1.2f, OuterRadiusUU);

		// Ring spacing grows with radius: the near water needs enough rows to carry the depth
		// gradient, the far water is a colour wash and one row per kilometre is plenty.
		TArray<float> Radii;
		Radii.Reserve(Rings + 1);
		for (int32 Ring = 0; Ring <= Rings; ++Ring)
		{
			const float T = static_cast<float>(Ring) / static_cast<float>(Rings);
			Radii.Add(Inner + (Outer - Inner) * FMath::Pow(T, 2.2f));
		}

		const FVector Up(0.0f, 0.0f, 1.0f);
		TArray<int32> Previous;
		for (int32 Ring = 0; Ring <= Rings; ++Ring)
		{
			const float Radius = Radii[Ring];
			const float T = static_cast<float>(Ring) / static_cast<float>(Rings);

			// Three legs, not one.
			//
			// The sea used to start at the deep tone and fade straight into haze. That is why the
			// lower half of the frame was a flat slab: there was nothing between "the tile water
			// around the island" and "deep ocean", so the eye had no depth cue at all. Real water
			// reads as lagoon -> shelf -> deep -> haze, and each leg is a different colour.
			//
			//   T 0.00-0.14  lagoon       (matches the tile water the island sits in)
			//   T 0.14-0.32  shelf        (where the bottom stops being visible)
			//   T 0.32-1.00  deep -> haze (fades into the horizon band, no seam)
			FLinearColor Colour;
			if (T < 0.18f)
			{
				Colour = FMath::Lerp(Palette.Water, Palette.WaterMid, T / 0.18f);
			}
			else if (T < 0.34f)
			{
				Colour = FMath::Lerp(Palette.WaterMid, Palette.WaterDeep, (T - 0.18f) / 0.16f);
			}
			else
			{
				const float HazeT = FMath::Clamp((T - 0.34f) / 0.66f, 0.0f, 1.0f);
				Colour = FMath::Lerp(Palette.WaterDeep, HorizonHaze, FMath::Pow(HazeT, 1.5f));
			}

			TArray<int32> Current;
			Current.Reserve(Segments);
			for (int32 Index = 0; Index < Segments; ++Index)
			{
				const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(Segments);
				const float CosA = FMath::Cos(Angle);
				const float SinA = FMath::Sin(Angle);
				Current.Add(Out.Add(FVector(CosA * Radius, SinA * Radius, SeaLevelUU), Up,
					FVector2D(static_cast<float>(Index) / Segments, T), Colour, 1.0f));
			}

			if (Ring > 0)
			{
				for (int32 Index = 0; Index < Segments; ++Index)
				{
					const int32 Next = (Index + 1) % Segments;
					// Wound so the face looks UP, i.e. this is the front face for a surface seen
					// from above.
					//
					// The previous winding was the opposite. It stayed hidden because M_Sea was
					// two-sided - but a two-sided material flips the normal for what it considers
					// a back face, so every visible pixel had a normal pointing *down*. Fresnel
					// then saw dot(N, V) <= 0, clamped to zero, and reported total reflection at
					// every angle: the sea was permanently 75% sky, which is why it measured
					// (0.50, 0.79, 0.90) no matter what the water palette said. A geometry bug
					// that a material setting hides is still a geometry bug.
					Out.Tri(Previous[Index], Current[Next], Current[Index]);
					Out.Tri(Previous[Index], Previous[Next], Current[Next]);
				}
			}
			Previous = MoveTemp(Current);
		}
	}

	// ======================================================================
	// Trees
	// ======================================================================
	//
	// A tree used to be four rectangles: `StandeeRecipe(Shrub)` describes a paper silhouette,
	// which is right for the animals (they only ever need to read from the side) and wrong for
	// the thing that covers every slope on the island. At the entry framing the realm is a
	// hillside of identical green mushrooms - the single most "placeholder" thing in the frame.
	//
	// These are real geometry at a deliberately low budget: a six-sided trunk and one to three
	// rings of an eight-sided sphere. About 130 triangles a tree, 120 trees is under 16k, which
	// is nothing next to the 200k the terrain already spends.
	namespace
	{
		/** UV sphere with the poles on Z. Normals are the radial direction, so it shades as a ball. */
		void AddSphere(FMeshBuilder& Out, const FVector& Centre, float Radius, int32 Segments,
			int32 Rings, const FLinearColor& Colour, float Occlusion)
		{
			const int32 Base = Out.Positions.Num();
			for (int32 Ring = 0; Ring <= Rings; ++Ring)
			{
				const float V = static_cast<float>(Ring) / Rings;
				const float Phi = PI * V;
				for (int32 Seg = 0; Seg <= Segments; ++Seg)
				{
					const float U = static_cast<float>(Seg) / Segments;
					const float Theta = 2.0f * PI * U;
					const FVector Normal(FMath::Sin(Phi) * FMath::Cos(Theta),
						FMath::Sin(Phi) * FMath::Sin(Theta), FMath::Cos(Phi));
					Out.Add(Centre + Normal * Radius, Normal, FVector2D(U, V), Colour, Occlusion);
				}
			}
			for (int32 Ring = 0; Ring < Rings; ++Ring)
			{
				for (int32 Seg = 0; Seg < Segments; ++Seg)
				{
					const int32 A = Base + Ring * (Segments + 1) + Seg;
					const int32 B = A + 1;
					const int32 C = A + Segments + 1;
					const int32 D = C + 1;
					Out.Tri(A, C, D);
					Out.Tri(A, D, B);
				}
			}
		}

		/** Tapered prism from Base to Top. */
		void AddTrunk(FMeshBuilder& Out, const FVector& Base, const FVector& Top, float Radius,
			int32 Sides, const FLinearColor& Colour)
		{
			const FVector Axis = (Top - Base).GetSafeNormal();
			const FVector Right = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal();
			const FVector RightSafe = Right.IsNearlyZero() ? FVector::ForwardVector : Right;
			const FVector Forward = FVector::CrossProduct(Axis, RightSafe);
			const float TopRadius = Radius * 0.72f;

			TArray<FVector> Bottom;
			TArray<FVector> Upper;
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const float Angle = 2.0f * PI * Side / Sides;
				const FVector Offset = RightSafe * FMath::Cos(Angle) + Forward * FMath::Sin(Angle);
				Bottom.Add(Base + Offset * Radius);
				Upper.Add(Top + Offset * TopRadius);
			}
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const int32 Next = (Side + 1) % Sides;
				const FVector Normal = (RightSafe * FMath::Cos(2.0f * PI * (Side + 0.5f) / Sides)
					+ Forward * FMath::Sin(2.0f * PI * (Side + 0.5f) / Sides)).GetSafeNormal();
				const int32 A = Out.Add(Bottom[Side], Normal, FVector2D(0.0f, 0.0f), Colour, 0.85f);
				const int32 B = Out.Add(Bottom[Next], Normal, FVector2D(1.0f, 0.0f), Colour, 0.85f);
				const int32 C = Out.Add(Upper[Next], Normal, FVector2D(1.0f, 1.0f), Colour, 1.0f);
				const int32 D = Out.Add(Upper[Side], Normal, FVector2D(0.0f, 1.0f), Colour, 1.0f);
				Out.Tri(A, B, C);
				Out.Tri(A, C, D);
			}
		}

		/**
		 * One tree. `Seed` picks the silhouette family so a hillside is not one clone repeated:
		 * round broadleaf, tall conifer, or a low shrub.
		 */
		void AddTree(FMeshBuilder& Out, const FVector& Origin, float Scale, float Yaw, uint32 Seed,
			const FCelPalette& Palette)
		{
			FRandomStream Rng(static_cast<int32>(Seed));
			const float Lean = (Rng.FRandRange(-1.0f, 1.0f)) * 0.10f;
			// Proportions matter more than polygon count here. A 1.8 m trunk under a 0.9 m crown
			// is a lollipop; the crown has to start low and be wide enough to hide the top of the
			// trunk, or the tree reads as a stick with a ball on it no matter how it is shaded.
			const float Height = (2.2f + Rng.FRand() * 1.8f) * Scale;
			const float TrunkRadius = (0.18f + Rng.FRand() * 0.06f) * Scale;
			const FVector Up(Lean, Lean * 0.6f, 1.0f);

			// Dark, and darker than it looks like it should be: at a 100-unit sun anything above
			// about 0.10 linear albedo clips to white, and a white trunk under a green crown is
			// exactly the mushroom silhouette this is trying to get away from.
			// Written as a linear triple, not as sRGB hex times a factor.
			//
			// Every "make it darker" attempt that went through CelSrgb() stayed white: at this
			// sun a linear albedo of 0.03 already lands at ~0.7 after the /pi and the exposure,
			// so the trunk has to start an order of magnitude lower than a palette colour rather
			// than a multiplier away from one.
			const FLinearColor Bark(0.030f, 0.019f, 0.011f, 1.0f);
			const FLinearColor Leaf = FMath::Lerp(Palette.Moss, Solstice::CelSrgb(0x6F, 0xA8, 0x4E),
				Rng.FRand() * 0.55f);
			const FLinearColor LeafDark = Leaf * 0.72f;

			AddTrunk(Out, Origin, Origin + Up.GetSafeNormal() * Height, TrunkRadius, 6, Bark);

			const int32 Family = Rng.RandRange(0, 2);
			if (Family == 0)
			{
				// Broadleaf: one big crown and a smaller one offset from it.
				const float Radius = (1.10f + Rng.FRand() * 0.45f) * Scale;
				AddSphere(Out, Origin + FVector(0.0f, 0.0f, Height * 0.78f), Radius, 8, 5, Leaf, 1.0f);
				AddSphere(Out, Origin + FVector(Radius * 0.60f, Radius * 0.38f, Height * 0.62f),
					Radius * 0.66f, 7, 4, Leaf * 0.90f, 0.9f);
			}
			else if (Family == 1)
			{
				// Conifer: three rings, each smaller and higher, so the silhouette is a cone.
				const float Radius = (1.00f + Rng.FRand() * 0.30f) * Scale;
				AddSphere(Out, Origin + FVector(0.0f, 0.0f, Height * 0.50f), Radius, 8, 4, LeafDark, 0.85f);
				AddSphere(Out, Origin + FVector(0.0f, 0.0f, Height * 0.74f), Radius * 0.76f, 8, 4, Leaf, 0.95f);
				AddSphere(Out, Origin + FVector(0.0f, 0.0f, Height * 0.96f), Radius * 0.48f, 7, 3, Leaf, 1.0f);
			}
			else
			{
				// Low shrub: no visible trunk, one squashed crown.
				const float Radius = (0.75f + Rng.FRand() * 0.35f) * Scale;
				AddSphere(Out, Origin + FVector(0.0f, 0.0f, Radius * 0.50f),
					Radius, 8, 4, Leaf * 0.86f, 0.95f);
			}

			(void)Yaw;
		}
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

			// The sim addresses the whole square grid; the picture is a torn disc. Every standee
			// outside that disc used to be drawn anyway and floated over the void - the ring of
			// specks past the edge visible in every wide shot (2026-10-18). Same radius as the
			// sheet clip, so an individual can never stand where there is no ground.
			{
				const float RealmRadius = static_cast<float>(Size) * 0.5f * MeterUU;
				const FVector Flat = TileToLocal(Kernel, TileX, TileY, 0.0f);
				const float Torn = TornRadius(RealmRadius, Flat.X, Flat.Y);
				if (Flat.X * Flat.X + Flat.Y * Flat.Y > Torn * Torn)
				{
					continue;
				}
			}

			const float Surface = (Kind == ESpeciesKind::StreamFish)
				? FMath::Max(Tile->Height, WaterLevel) * MeterUU
				: SurfaceHeight(*Tile, WaterLevel) * MeterUU;

			const FVector Origin = TileToLocal(Kernel, TileX, TileY, 0.0f) + FVector(0.0f, 0.0f, Surface);
			const FRotator Facing(0.0f, static_cast<float>(Individual.Facing) * 45.0f, 0.0f);
			const FTransform Transform(Facing, Origin, FVector(1.0f));

			// Shrubs are the island's actual vegetation, so they get real geometry instead of a
			// paper silhouette (see AddTree above). Everything else - grazers, raptors, fish -
			// stays a standee: they are characters that only have to read from the front.
			if (Kind == ESpeciesKind::Shrub)
			{
				const uint32 Seed = static_cast<uint32>(Individual.Tile) * 2654435761u
					^ static_cast<uint32>(Individual.Facing) * 40503u;
				// MeterUU, not 1.0: Origin is in unreal units (it already carries Surface * MeterUU),
			// so a scale of 1 would build a three-centimetre tree.
			AddTree(Out, Origin, MeterUU, Facing.Yaw, Seed, Palette);
				continue;
			}

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

		/**
		 * Thickness of the bright horizon band, in card-height units.
		 *
		 * 0.07 was a tenth of the frame of near-white. Once the sea runs to the horizon the band
		 * reads as a fog bank lying on the water, and the sky above it turns into a pale wash: a
		 * cel horizon is a *line*, not a band.
		 */
		// 0.010, not 0.022: measured on a 720p frame the band was 60 px of near-white - a stripe
		// that cut the picture in three. A cel horizon is a line.
		constexpr float HorizonBand = 0.010f;

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
				const float Span = FMath::Max(0.10f, Paint.VisibleTop - H);
				const float T = FMath::Clamp((V - H) / Span, 0.0f, 1.0f);
				// 0.30, not 0.58: at 0.58 the near-white horizon colour owned 40 px of sky above
				// the line and the frame read as a white stripe with blue above it.
				// Smoothstep, not two linear legs.
				//
				// Piecewise-linear interpolation leaves a *slope discontinuity* at the join, and on
				// a large smooth gradient the eye reads that crease as a horizontal band - which is
				// exactly what kept appearing just above the horizon. One smooth curve has no join.
				const float S = T * T * (3.0f - 2.0f * T);
				return (S < 0.5f)
					? FMath::Lerp(Paint.Horizon, Paint.Middle, S / 0.5f)
					: FMath::Lerp(Paint.Middle, Paint.Zenith, (S - 0.5f) / 0.5f);
			}

			// Below the horizon the sky darkens quickly: this is the far sea's backdrop, and a long
			// ramp here was the pale wash that hid the water's own haze.
			const float Span = FMath::Max(0.10f, H + 1.0f);
			const float T = FMath::Clamp((H - V) / Span, 0.0f, 1.0f);
			return FMath::Lerp(Paint.Horizon, Paint.Under, FMath::Min(1.0f, T / 0.10f));
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

	void Gen::BuildSkyDome(int32 Segments, int32 Rings, FMeshBuilder& Out)
	{
		Segments = FMath::Max(8, Segments);
		Rings = FMath::Max(4, Rings);

		// A unit sphere. The caller scales it: the realm sets the component scale to SkyDistance,
		// which keeps the geometry a metre-per-unit affair instead of a 400 km mesh.
		//
		// Normals point *inward* (we are inside the sphere looking out), and the winding is
		// reversed to match, so the front faces are the ones the camera sees. M_Sky is two-sided
		// as a belt-and-braces measure, but the direction the shader uses comes from
		// normalize(WorldPosition - CameraPosition), so a flipped normal could not break it anyway.
		TArray<int32> Previous;
		for (int32 Ring = 0; Ring <= Rings; ++Ring)
		{
			const float V = static_cast<float>(Ring) / static_cast<float>(Rings);
			const float Phi = (V - 0.5f) * PI;			// -90 deg .. +90 deg
			const float SinPhi = FMath::Sin(Phi);
			const float CosPhi = FMath::Cos(Phi);

			TArray<int32> Current;
			Current.Reserve(Segments);
			for (int32 Index = 0; Index < Segments; ++Index)
			{
				const float U = static_cast<float>(Index) / static_cast<float>(Segments);
				const float Theta = U * 2.0f * PI;
				const FVector Outward(CosPhi * FMath::Cos(Theta), CosPhi * FMath::Sin(Theta), SinPhi);
				Current.Add(Out.Add(Outward, -Outward, FVector2D(U, V), FLinearColor::White));
			}

			if (Ring > 0)
			{
				for (int32 Index = 0; Index < Segments; ++Index)
				{
					const int32 Next = (Index + 1) % Segments;
					// Reversed relative to an outward sphere so the visible side is the front face.
					Out.Tri(Previous[Index], Current[Index], Previous[Next]);
					Out.Tri(Previous[Next], Current[Index], Current[Next]);
				}
			}
			Previous = MoveTemp(Current);
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
			// 64 x 96, not 40 x 24.
			//
			// The horizon transition is 0.010 of the card, and 24 rows meant one row spanned
			// V = 0.083 - eight times the whole transition. The horizon was therefore quantised
			// into a near-white row followed by a grey one: two hard bands where the reference
			// has a line. Rows have to be dense enough that the band is thinner than a row.
			constexpr int32 Cols = 64;
			constexpr int32 Rows = 96;
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
				// Thicker and fainter. At 0.013 card-height against a 0.24-0.55 length the streak was
				// a 20:1 sliver mixed 70% toward white - three hard horizontal scratches across the
				// sky, which is exactly what it looked like. A cirrus veil is wide, soft and barely
				// there: 2.5x thicker, mixed only 32%.
				const float Length = HalfWidth * FMath::Lerp(0.30f, 0.62f, FMath::Frac(Seed * 1.31f));
				const FLinearColor Mixed = FMath::Lerp(SkyRamp(Paint, V), Tone, 0.32f * Fade);
				SkyPuff(Out, CirrusZ, FVector2D(U * HalfWidth, V * HalfHeight),
					Length, HalfHeight * 0.032f, HalfHeight * 0.026f, Mixed, Mixed);
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
				// Wider than tall, but not by the 4:1 the first pass used: at that ratio a bank is
				// a pale plate lying on the sky (every noon frame, 2026-10-18). 2.4:1 with a deeper
				// shadow sliver reads as a cloud you could almost touch.
				const float Width = HalfWidth * FMath::Lerp(0.065f, 0.125f, FMath::Frac(Seed * 1.07f));
				const float Height = HalfHeight * FMath::Lerp(0.055f, 0.095f, FMath::Frac(Seed * 1.51f));
				// Kept clear of the horizon band on purpose: a white cloud against the near-white
				// horizon is invisible, which is how the first pass "had no clouds" while drawing
				// ten of them. The top of the range leaves room for the bank's own height - a
				// centre at 0.99 puts half a cloud outside the card, and drift moves it there.
				const float Height01 = Height / HalfHeight;
				const float VMax = FMath::Max(H + 0.20f, Top - Height01 * 1.15f);
								// Further up the card than the horizon band: a white cloud against a near-white
				// horizon has no contrast at all, and every noon frame's cloud bank sat exactly
				// there (2026-10-18). Higher up the sky is bluer, so the same white reads.
				const float V = FMath::Lerp(H + 0.28f, VMax, FMath::Frac(Seed * 0.613f));
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
				SkyPuff(Out, CumulusZ, Centre, Width, Height, Height * 0.46f, LitNow, BaseNow);
				SkyPuff(Out, CumulusZ, Centre + FVector2D(Width * 0.70f, Height * 0.34f),
					Width * 0.60f, Height * 0.80f, Height * 0.36f, LitNow, BaseNow);
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
