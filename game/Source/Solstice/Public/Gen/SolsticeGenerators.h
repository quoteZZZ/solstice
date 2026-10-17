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

		/** Water sheet, shallow -> deep. Saturated deep blue: the reference pond reads as water,
			not as pale grey glass, and the dark sheet is what makes the lit land pop. */
		FLinearColor Water = CelSrgb(0x3D, 0x78, 0x9E);
		FLinearColor WaterDeep = CelSrgb(0x12, 0x30, 0x61);

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

		/** Cel colour of one tile: material tone pulled toward raw paper by the chaos state. */
		FLinearColor TileColour(const FTile& Tile, const FCelPalette& Palette);

		/** One terrain chunk: plates, cut edges, contour ribbons and the water sheet. */
		void BuildChunk(const FSimKernel& Kernel, int32 ChunkX, int32 ChunkY, const FCelPalette& Palette, FMeshBuilder& Out);

		/** The kraft base slab that turns the terrain into a physical object. */
		void BuildBase(const FSimKernel& Kernel, const FCelPalette& Palette, FMeshBuilder& Out);

		/** Every living individual as a standee; printed outline cards land in OutOutline. */
		void BuildLife(const FSimKernel& Kernel, const FCelPalette& Palette, FMeshBuilder& Out, FMeshBuilder& OutOutline);

		/** Lower half of a paper dome, normals pointing inward. */
		void BuildSkyDome(const FVector& Centre, float RadiusUU, const FLinearColor& Zenith, const FLinearColor& Horizon, FMeshBuilder& Out);

		/** The 12-16 petals that ring the dome rim. */
		void BuildPetals(const FVector& Centre, float RadiusUU, int32 Count, const FLinearColor& Inner, const FLinearColor& Outer, FMeshBuilder& Out);

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
