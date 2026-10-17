// Solstice - Render layer: the paper material.
//
// The look is cel-shaded paper, not PBR (docs/art/ART-STYLE.md SS1): a handful of hard light
// bands, no specular, no Lumen, no Nanite. The master material is generated into
// /Game/Core/M_Cel by the editor module on first launch, so a fresh checkout rebuilds its
// own content. If generation is unavailable the renderer degrades to the engine's vertex
// colour material, which is unlit - the realm still reads as paper, it just loses the sun.
//
// Contract with the generated material:
//   SkyTint   (vector) - colour of a fully lit surface after dusk-to-noon tinting
//   NightTint (vector) - colour of a surface facing away from the sun
//   Bands     (scalar) - number of hard light bands

#pragma once

#include "CoreMinimal.h"
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
		UMaterialInstanceDynamic* CelMID = nullptr;
		UMaterialInstanceDynamic* InkMID = nullptr;
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

	private:
		static FCelMaterialSet Set;
		static bool bResolved;
		/** True once the builder has been invoked, so we never retry it every frame. */
		static bool bBuildAttempted;
	};
}
