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
	 * Presentation palette: the whole "paper diorama" look in one place.
	 * Never read by the simulation. These are the art recipe of docs/art/ART-STYLE.md SS2 -
	 * tuning the look means tuning this struct instead of hunting literals in the generators.
	 */
	struct FCelPalette
	{
		/** Cel stock and cut edges. */
		FLinearColor CutEdge = FLinearColor(0.86f, 0.82f, 0.74f);
		FLinearColor Ribbon = FLinearColor(0.35f, 0.30f, 0.26f);
		FLinearColor RawBare = FLinearColor(0.93f, 0.91f, 0.86f);

		/** Ink. */
		FLinearColor InkWash = FLinearColor(0.16f, 0.14f, 0.13f);

		/** Water sheet, shallow -> deep. Saturated deep blue: the reference pond reads as water,
			not as pale grey glass, and the dark sheet is what makes the lit land pop. */
		FLinearColor Water = FLinearColor(0.24f, 0.47f, 0.62f);
		FLinearColor WaterDeep = FLinearColor(0.07f, 0.19f, 0.38f);

		/** Terrain surface states. */
		FLinearColor Moss = FLinearColor(0.40f, 0.54f, 0.28f);
		FLinearColor Ash = FLinearColor(0.30f, 0.27f, 0.25f);
		FLinearColor Ember = FLinearColor(1.00f, 0.42f, 0.10f);

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
