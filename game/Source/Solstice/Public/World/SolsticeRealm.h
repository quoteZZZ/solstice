// Solstice - World layer: the realm actor.
//
// One actor owns the whole visible realm: the paper terrain (one procedural mesh per chunk,
// rebuilt only when the kernel marks it dirty), the standees, the kraft base, the paper dome
// and the sun. It drives the world clock and it is the only place where Sim data is turned
// into geometry.
//
// Layer rule (docs/TECH.md SS3): World may read Sim and Gen; Sim never sees either.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gen/SolsticeGenerators.h"
#include "Sim/SolsticeKernel.h"
#include "SolsticeRealm.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UProceduralMeshComponent;
class USceneComponent;
class USkyLightComponent;
class USolsticeGameInstance;

UCLASS()
class SOLSTICE_API ASolsticeRealm : public AActor
{
	GENERATED_BODY()

public:
	ASolsticeRealm();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	USolsticeGameInstance* Game() const;
	Solstice::FSimKernel* Kernel() const;

	// ---- geometry <-> tiles ----------------------------------------------

	/** World position of the top corner of a tile, in Unreal units. */
	FVector TileToWorld(int32 TileIndex) const;

	/** World position of a tile's surface, for UI anchoring and camera focus. */
	FVector TileSurfaceWorld(int32 TileX, int32 TileY) const;

	bool WorldToTile(const FVector& WorldLocation, int32& OutTileIndex) const;

	/** Trace from the cursor into the paper. Returns INDEX_NONE when nothing is hit. */
	int32 PickTile(APlayerController* Controller, FVector& OutHitLocation) const;

	/**
	 * Draw the landing marker on a tile, or hide it (INDEX_NONE).
	 *
	 * The player had no way to know where a card would land: the click raycast resolved a tile
	 * and applied the card, with nothing on screen between the cursor and the paper. That is the
	 * single largest usability gap in the loop - a placement game where you cannot see the
	 * placement.
	 */
	void SetCursorTile(int32 TileIndex, bool bValid);

	int32 GetCursorTile() const { return CursorTile; }

	FVector GetRealmCentre() const;
	float GetRealmRadiusUU() const;

protected:
	void BuildStatics();
	void BuildCursorMesh();
	void BuildSkyMesh();

	/**
	 * Where the sky card has to sit this frame: the plane the sky is painted on is
	 * perpendicular to the view axis, `SkyDistance` in front of the camera, and the component's
	 * basis is (right, up, forward) so card space and screen space are the same axes.
	 */
	void UpdateSkyView();
	void RebuildDirtyChunks(int32 Budget);
	void RebuildProps();
	void UpdateSun();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> BaseMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> SkyMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> PropMesh;
	/** Landing marker: a ring drawn flat on the paper under the cursor. */
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CursorMesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CursorMID;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> SunLight;
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyLight;
	/** Real atmospheric scattering; replaces the painted sky card (REBOOT B2). */
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	/** Unbound colour grade that gives the whole realm its warm-paper cast (see the constructor). */
	UPROPERTY() TObjectPtr<UPostProcessComponent> PostProcess;

	Solstice::FCelPalette Palette;
	Solstice::FMeshBuilder Scratch;
	/** Ink hulls for the chunk being rebuilt (section 1, ink material). */
	Solstice::FMeshBuilder HullScratch;
	Solstice::FMeshBuilder OutlineScratch;
	int32 ChunksPerAxis = 2;
	int32 CursorTile = INDEX_NONE;

	/**
	 * Debug hook: `-SolsticeSea=<diffuse>,<glow>,<shade>[,<foam>[,<specular>[,<reflect>]]]`
	 * overrides the water balance constants for one run. -1 leaves the authored value.
	 *
	 * The sea is the sum of two light paths - a lit base colour and an emissive body - and the
	 * split between them decides how much of the island's shadow the water is able to show at
	 * all. Searching that split by recompiling (25 s) and re-shooting (40 s) per candidate makes
	 * a three-number search cost ten minutes; this turns it into one run per candidate.
	 */
	TArray<float> SeaTuning;

	/**
	 * Debug hook: `-SolsticeEnv=<sun>,<sky>,<exposure>` scales the two light intensities and adds
	 * stops to the fixed exposure for one run. -1 keeps the shipped value.
	 *
	 * Sun and sky are not comparable quantities: the sun is set in lux-like units while the sky
	 * is a multiplier on a real-time capture that is itself in physical units, so "sun 14 vs sky
	 * 0.10" says nothing about which one owns the frame. Sweeping the ratio is the only way to
	 * find out, and this makes each candidate one shot instead of one build.
	 */
	FVector EnvTuning = FVector(-1.0, -1.0, -1.0);

	/**
	 * Debug hook: `-SolsticeCel=<gain>` scales the ground's albedo for one run, -1 to leave it.
	 *
	 * See the SurfaceBaseGain parameter in M_Cel. This is the fastest way to tell "the ground is
	 * lit but dark" apart from "the ground is not lit", which look identical on screen.
	 */
	float CelBaseGain = -1.0f;

	float SkyAccumulator = 0.0f;
	float PropAccumulator = 0.0f;
	bool bStaticsBuilt = false;
	float LastSolarAltitude = -999.0f;

	// Cached view, refreshed once a tick: the sky card is placed from these every frame and
	// painted from them when its signature moves.
	FVector SkyViewLocation = FVector::ZeroVector;
	FRotator SkyViewRotation = FRotator::ZeroRotator;
	FVector SkyViewForward = FVector::ForwardVector;
	FVector SkyViewRight = FVector::RightVector;
	FVector SkyViewUp = FVector::UpVector;
	float SkyHalfTanH = 0.344f;
	float SkyHalfTanV = 0.194f;
	float SkyDistance = 40000.0f;
	/** True once the card has been parented to the player camera. */
	bool bSkyAttached = false;

	// Change signatures. Both meshes used to be rebuilt on a timer whether or not anything had
	// changed; they are now rebuilt only when what they are made of actually differs.
	uint32 LastPropSignature = 0;
	uint32 LastSkySignature = 0;
	bool bPropsBuilt = false;
	bool bSkyBuilt = false;
};
