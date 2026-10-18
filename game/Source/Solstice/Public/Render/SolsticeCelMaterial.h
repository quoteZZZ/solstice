// Solstice - Render layer: the cel material.
//
// The look is flat animation cel, not PBR (docs/art/art.md SS2.1): three hard light bands, no
// specular, no Lumen, no Nanite, no paper grain. The master material is generated into
// /Game/Core/M_Cel by the editor module on first launch, so a fresh checkout rebuilds its
// own content. If generation is unavailable the renderer degrades to the engine's vertex
// colour material, which is also unlit - the realm still reads, it just loses the bands.
//
// Contract with the generated material (all pushed from ApplySky every frame):
//   SunDirection (vector) - unit vector pointing at the sun; the bands are dot(normal, this)
//   LitTint      (vector) - colour of a surface facing the sun
//   MidTint      (vector) - colour of a surface roughly side-on to it
//   ShadowTint   (vector) - colour of a surface facing away
//   BandLow/High (scalar) - the two hard cut points, 0.30 / 0.62 per art.md
//   RecipeVersion(scalar) - graph revision stamp; a mismatch makes the builder rewrite the asset

#pragma once

#include "CoreMinimal.h"
#include "Components/PostProcessComponent.h"
#include "Sim/SolsticeTypes.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;

namespace Solstice
{
	/**
	 * Set by SolsticeEditor at startup. The runtime module only ever calls it; it never links
	 * against UnrealEd, which keeps the headless target free of editor dependencies.
	 */
	extern SOLSTICE_API TFunction<bool()> GBuildCelMaterials;

	struct SOLSTICE_API FCelMaterialSet
	{
		UMaterialInterface* Cel = nullptr;
		UMaterialInterface* Ink = nullptr;
		/** One-sided ink for inverted hulls: the enlarged copy must show only its back faces. */
		UMaterialInterface* InkHull = nullptr;
		/** Pure vertex colour, no bands: the painted sky must arrive exactly as authored. */
		UMaterialInterface* Sky = nullptr;
		UMaterialInstanceDynamic* CelMID = nullptr;
		UMaterialInstanceDynamic* InkMID = nullptr;
		UMaterialInstanceDynamic* InkHullMID = nullptr;
		UMaterialInstanceDynamic* SkyMID = nullptr;
		/** True when our own master material was found and generated, false when we fell back. */
		bool bAuthored = false;
	};

	class SOLSTICE_API FCelMaterials
	{
	public:
		/**
		 * Handed in by SolsticeEditor. Returns true if the assets were written. Kept as a plain
		 * function so the runtime module never has to link against UnrealEd.
		 */
		static void SetBuilder(TFunction<bool()> InBuilder);

		/** Resolve (and, on a fresh checkout, generate) the materials. Safe to call repeatedly. */
		static bool Ensure();

		static const FCelMaterialSet& Get() { return Set; }

		/** Push the current sky into the material parameters. Cheap; call once per frame. */
		static void ApplySky(const FSkyState& Sky, bool bChaos);

		/** Attach the paper and ink materials to a procedural mesh as section 0 and section 1. */
		static void ApplyToMesh(UProceduralMeshComponent* Mesh);

		/** Attach the unlit sky material. Separate from ApplyToMesh: no bands, no ink card. */
		static void ApplyToSky(UProceduralMeshComponent* Mesh);

	private:
		static FCelMaterialSet Set;
		static bool bResolved;
		/** True once the builder has been invoked, so we never retry it every frame. */
		static bool bBuildAttempted;
	};
}
