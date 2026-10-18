// Solstice - Gen layer: the procedural geometry generators.
//
// Nothing here is modelled by hand. The paper diorama is produced from four rules
// (docs/art/ART-STYLE.md SS2):
//   1. the ground is a stack of cut paper plates, so every height step shows a cut edge;
//   2. whole-metre contours are inked as ribbons, so the terrain reads as a topographic model;
//   3. living things are flat standees with a printed outline card behind them;
//   4. the world is closed by a paper dome ringed with petals.
//
// Layer rule: Gen/ reads Sim, Sim never reads Gen.

#pragma once

#include "CoreMinimal.h"
#include "Gen/SolsticeMeshBuilder.h"
#include "Sim/SolsticeKernel.h"

namespace Solstice
{
	/**
	 * Palette helper. Colours in this project are *picked* (a colour wheel, a reference frame),
	 * not measured, so they are sRGB values - and an FLinearColor built from raw sRGB numbers is
	 * not the colour that comes out on screen. It is the same colour with its gamma applied twice:
	 * grass authored as 0.40 renders at 0.66 and the whole world reads as pale sage. Everything
	 * authored by eye goes through this.
	 */
	inline FLinearColor CelSrgb(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}

	/**
	 * Presentation palette: the whole cel look in one place.
	 * Never read by the simulation. These are the art recipe of docs/art/art.md SS2.3 -
	 * tuning the look means tuning this struct instead of hunting literals in the generators.
	 */
	struct FCelPalette
	{
		/** Cel stock and cut edges. */
		FLinearColor CutEdge = CelSrgb(0xDB, 0xD1, 0xBD);
		FLinearColor Ribbon = CelSrgb(0x59, 0x4D, 0x42);
		FLinearColor RawBare = CelSrgb(0xED, 0xE8, 0xDB);

		/** Ink. */
		FLinearColor InkWash = CelSrgb(0x29, 0x24, 0x21);

		/**
		 * Water, three stops: lagoon / mid / deep.
		 *
		 * Picked from the prototype's own water palette (`CLIM_PAL[].wat`, temperates:
		 * 0.42/0.86/0.84 - 0.10/0.56/0.78 - 0.03/0.22/0.52). The prototype is right and my
		 * hand-mixed greys were wrong: a cel sea is a saturated tropical lagoon shallowing into a
		 * dark blue, not a muted slate. Deep stays clearly lighter than the void (#0B1431) so the
		 * disc does not read as "the world ends here".
		 */
		FLinearColor Water = CelSrgb(0x6B, 0xDC, 0xD6);
		FLinearColor WaterMid = CelSrgb(0x1A, 0x8F, 0xC7);
		FLinearColor WaterDeep = CelSrgb(0x08, 0x38, 0x85);
		/** Surf: the pale rim on the water side of a shoreline (art.md SS2.3). */
		FLinearColor Surf = CelSrgb(0xDC, 0xF0, 0xF8);
		/** Ripple strokes on open water. */
		FLinearColor Ripple = CelSrgb(0xA9, 0xD8, 0xEA);

		/** Terrain surface states. */
		FLinearColor Moss = CelSrgb(0x66, 0x8A, 0x47);
		FLinearColor Ash = CelSrgb(0x4D, 0x45, 0x40);
		FLinearColor Ember = CelSrgb(0xFF, 0x6B, 0x1A);

		/** How strongly vegetation / fire tint the base colour (0..1). */
		float VegetationTintStrength = 0.75f;
		float BurnTintStrength = 0.85f;
	};

	// ------------------------------------------------------------------------
	// Standee recipes (docs/setting/SPECIES.md SS2, docs/art/ART-STYLE.md SS2).
	//
	// A standee is a few flat paper cards standing in a tile: a body cut in the XZ plane,
	// a crossed card in YZ to give it thickness, and horizontal cards for wings. All of it
	// is data - a species is a base tint plus a list of cards - so tuning a silhouette never
	// means touching the mesh builder.
	// ------------------------------------------------------------------------

	/** Plane a standee card is cut in. */
	enum class EStandeePlane : uint8
	{
		XZ,	// vertical rectangle facing +Y: the front view
		YZ,	// vertical rectangle facing +X: the crossed card
		XY	// horizontal rectangle: wings
	};

	/** One paper card of a standee, in local units relative to the tile centre. */
	struct FStandeeCard
	{
		EStandeePlane Plane = EStandeePlane::XZ;

		/** In-plane extents in UU (XZ / YZ use A = X or Y, B = Z; XY uses A = X, B = Y). */
		float A0 = 0.0f;
		float A1 = 0.0f;
		float B0 = 0.0f;
		float B1 = 0.0f;

		/** XY cards only: height above the tile surface, in UU. */
		float HeightOffset = 0.0f;

		/** Tint multiplier applied to whichever of the recipe's two colours is used. */
		float Shade = 1.0f;

		/** true = the recipe's accent colour (stem / underside), false = its base colour. */
		bool bUseAccent = false;
	};

	/** The paper recipe of one species: its tints plus the cards that shape it. */
	struct FStandeeRecipe
	{
		/** Dominant paper tint: foliage, hide, feathers, scales. */
		FLinearColor BaseColour = FLinearColor::White;
		/** Secondary tint for stems and undersides. */
		FLinearColor AccentColour = FLinearColor::White;
		TArray<FStandeeCard> Cards;
	};

	namespace Gen
	{
		FCelPalette DefaultPalette();

		/** Cards that make up one species' standee. Unknown kinds yield an empty recipe. */
		const FStandeeRecipe& StandeeRecipe(ESpeciesKind Kind);

		/** World-space position of a tile's surface, in Unreal units, relative to the realm actor. */
		FVector TileToLocal(const FSimKernel& Kernel, int32 TileX, int32 TileY, float HeightMeters);

		/**
		 * The realm edge, as a radius for a given direction.
		 *
		 * One curve decides where the world ends, and *everything* reads it: the sheet clip, the
		 * cut edge, and which standees are allowed to exist. Before this was shared, the sheet was
		 * culled per tile centre (a one-metre staircase), the plinth was a machined circle of a
		 * different radius (a beige ring peeking out from under the terrain), and the standees were
		 * not culled at all (specks hanging in the void past the edge).
		 *
		 * Low harmonics of the azimuth, so the cut is an arc rather than a perfect circle, and so
		 * it reads as "a piece of land lifted out" (docs/setting/WORLD.md SS1.1).
		 */
		float TornRadius(float BaseRadius, float X, float Y);

		/**
		 * Cel colour of one point on the ground.
		 *
		 * `HeightAboveSeaMetres` and `Slope01` (1 - N.z) are *per vertex*, not per tile: the bands
		 * this paints are contour lines and cliff faces, and both cut across tiles. Passing the
		 * tile's own height (what this used to take) could only ever paint one flat patch per
		 * tile, which is why the realm read as a continuous green wash with no readable relief.
		 *
		 * Metres, not a fraction of the peak: the area-quantile remap puts ~40% of the land in the
		 * lowest 2 m, so dividing by the peak (18 m here) pushed every tile under the silt
		 * threshold and painted the whole island as beach.
		 */
		FLinearColor TileColour(const FTile& Tile, float HeightAboveSeaMetres, float Slope01, const FCelPalette& Palette);

		/**
		 * One terrain chunk: the ground sheet, its vegetation and its stones.
		 *
		 * `OutHull` receives an enlarged ink copy of every *object* in the chunk (canopies, trunks,
		 * boulders). That is art.md SS2.1's Inverted Hull: the copy is drawn behind with the ink
		 * material, and only the ring where it escapes the original silhouette is visible. The
		 * ground itself gets no hull - a scaled-up terrain would swallow the frame.
		 */
		void BuildChunk(const FSimKernel& Kernel, int32 ChunkX, int32 ChunkY, const FCelPalette& Palette,
			FMeshBuilder& Out, FMeshBuilder& OutHull);

		/** The kraft base slab that turns the terrain into a physical object. */
		void BuildBase(const FSimKernel& Kernel, const FCelPalette& Palette, FMeshBuilder& Out);

		/**
		 * The open sea, out to `OuterRadius`.
		 *
		 * The realm's own water is tile-sized and stops at the rim; without this the frame ends in
		 * the void and the world reads as a plate floating in blue nothing. The ocean is a separate
		 * disc that runs past the frame edge, so the horizon is where the *water* ends, not where
		 * the world does - which is the picture the prototype paints.
		 *
		 * Colour is radial: lagoon near the shore, mid blue, deep blue, then a long fade into the
		 * sky's horizon tone so the two meet without a seam.
		 */
		void BuildOcean(float InnerRadiusUU, float OuterRadiusUU, float SeaLevelUU,
			const FCelPalette& Palette, const FLinearColor& HorizonHaze, FMeshBuilder& Out);

		/** Every living individual as a standee; printed outline cards land in OutOutline. */
		void BuildLife(const FSimKernel& Kernel, const FCelPalette& Palette, FMeshBuilder& Out, FMeshBuilder& OutOutline);

		/**
		 * One frame of painted sky (docs/art/art.md SS2.2).
		 *
		 * The sky is a *painting*, not a render of an atmosphere: a card perpendicular to the
		 * view axis with the whole composition - gradient, cirrus, cumulus, sun, moon, stars -
		 * baked into vertex colours. That is the only way a fixed-exposure cel frame gets the
		 * near-white horizon the style needs, and it is the same "flat layers" call art.md makes
		 * about clouds.
		 *
		 * Card space: X right, Y up, Z = distance from the camera. Both halves are given in the
		 * canonical unit card (Z = 1), so the realm scales the whole painting with one component
		 * scale and a zoom never has to rebuild it.
		 */
		struct FSkyPaint
		{
			/**
			 * Height of the bright horizon band, as a fraction of the card half-height,
			 * +1 = top of frame. The realm computes it from where the world's silhouette
			 * actually projects, so the glow hugs the realm at every camera preset instead
			 * of sitting on a fixed line in the frame.
			 */
			float HorizonV = 0.30f;

			/**
			 * Top of the *visible* card, in card units (1 / SkyCardOverscan).
			 *
			 * The gradient has to fill the strip that is actually on screen. Measuring it to the card's
			 * own top edge squeezed the whole sky into the top 11% of the overscan, so the frame got a
			 * pale band over blue with almost no gradient in between.
			 */
			float VisibleTop = 0.893f;

			/** Three-band gradient: zenith is the deepest, the horizon is nearly white. */
			FLinearColor Zenith = CelSrgb(0x2F, 0x63, 0xAE);
			FLinearColor Middle = CelSrgb(0x86, 0xB8, 0xE4);
			FLinearColor Horizon = CelSrgb(0xEA, 0xF3, 0xFA);
			/** Below the horizon band the sky goes back down a step, so the frame has an edge. */
			FLinearColor Under = CelSrgb(0x8A, 0xB4, 0xD6);

			/** Clouds: a high thin veil and a lower opaque bank with a lit top and a cool base. */
			FLinearColor Cirrus = CelSrgb(0xFF, 0xFF, 0xFF);
			FLinearColor CumulusLit = CelSrgb(0xFF, 0xFC, 0xF5);
			FLinearColor CumulusBase = CelSrgb(0xAE, 0xC2, 0xDA);

			FLinearColor SunCore = CelSrgb(0xFF, 0xF7, 0xDC);
			FLinearColor SunHalo = CelSrgb(0xFF, 0xE6, 0x9E);
			FLinearColor MoonCore = CelSrgb(0xF2, 0xF6, 0xFF);
			FLinearColor Star = CelSrgb(0xFF, 0xFA, 0xE6);

			/** 0..1. Clouds are *added* by coverage; the gradient itself never darkens. */
			float CloudCover = 0.3f;
			/** 0..1. Rain greys the clouds; it does not dim the sky (art.md SS2.2). */
			float Rain = 0.0f;
			/** 0..1, one full horizontal loop of the whole cloud deck. */
			float Drift = 0.0f;

			/** Night only: 0 hides the stars entirely. */
			float StarAlpha = 0.0f;

			bool bSunVisible = false;
			FVector2D SunPos = FVector2D::ZeroVector;
			float SunRadius = 0.0f;

			bool bMoonVisible = false;
			FVector2D MoonPos = FVector2D::ZeroVector;
			float MoonRadius = 0.0f;
		};

		/**
		 * Paint one sky card. All sizes are in the canonical unit card (Z = 1): half width and
		 * half height are the tangents of the camera's half-FOV times the overscan.
		 */
		void BuildSkyCard(float HalfWidth, float HalfHeight, const FSkyPaint& Paint, FMeshBuilder& Out);

		/** A flat paper disc, used for the sun and the moon. */
		void BuildDisc(const FVector& Centre, const FVector& Right, const FVector& Up, float RadiusUU, const FLinearColor& Colour, FMeshBuilder& Out);

		/**
		 * A low irregular cushion: an n-gon skirt rising to a slightly domed cap, corners
		 * jittered by the paper grain. Tree canopy layers, boulders and cloud puffs are all
		 * this shape at different scales - one solid, top-lit blob holds a round silhouette
		 * from any camera angle, where flat cards only read from one side.
		 */
		void Cushion(FMeshBuilder& Out, const FVector& Centre, int32 Sides, float RadiusUU, float HeightUU,
			const FLinearColor& SideColour, const FLinearColor& TopColour, int32 Seed,
			FVector UpAxis = FVector::UpVector);
	}
}
