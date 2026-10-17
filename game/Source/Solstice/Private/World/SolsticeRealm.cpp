// Solstice - World layer: the realm actor.

#include "World/SolsticeRealm.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Core/SolsticeGameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
#include "Solstice.h"
#include "Render/SolsticeCelMaterial.h"

using namespace Solstice;

namespace
{
	/** How often the standees and the sky are re-meshed, in seconds. */
	constexpr float PropInterval = 0.10f;
	constexpr float SkyInterval = 0.20f;

	// R-22: the shell has to contain the four camera presets, or there is no sky in them.
	// The presets sit at 1.45-1.55 realm radii and the shell used to be at 1.34 - so every preset
	// parked the camera *outside* the shell, the shell stepped aside (SetSkyVisible) and the frame
	// had no sky at all behind the realm. At 1.62 the presets are inside the dome: the shell is the
	// sky, the sun and moon sit under it, and the realm reads as being inside a world rather than
	// floating in the clear colour.
	constexpr float DomeRadiusFactor = 1.00f;

	// ------------------------------------------------------------------
	// Scene rig (docs/art/ART-STYLE.md SS2).
	// The cel material is unlit and paints its own bands; these rigs only supply a light
	// *direction*, a fill level, and the depth fog that sells the miniature scale.
	// ------------------------------------------------------------------
	constexpr float SunLightIntensity = 3.2f;
	const FLinearColor SunLightColour(1.0f, 0.97f, 0.92f);
	constexpr float SkyLightIntensity = 0.4f;

	/**
	 * R-41: fixed exposure, in stops of gain over the manual-mode default.
	 *
	 * Histogram exposure used to own this, and that is why every past attempt at "make the palette
	 * show" appeared to do nothing: a dim scene gets pushed up until the frame is nearly white and
	 * every band the material painted is flattened out of existence. A stylised frame cannot let
	 * the camera decide its own brightness, so this is a constant - the palette is authored as
	 * display-referred colour and one gain maps mid albedo onto mid display. Calibrated against
	 * `tools/run/shot.ps1 -Measure`.
	 */
	constexpr float ExposureStops = 1.0f;

	// Fog depth (optical depth at one fit-camera-distance) for clear and fully overcast sky.
	// Density is derived from these against the fit distance - see BuildStatics.
	constexpr float ClearFogDepth = 0.25f;
	constexpr float WetFogDepth = 1.20f;
	constexpr float FogHeightFalloff = 0.06f;
	// The void outside the realm is not fog-grey. WORLD SS1.1 pins it: a low-saturation deep blue
	// (#0B1431) field with distant stars, against which the lit disc reads as the only bright thing
	// in frame - that contrast is half of why reference shots look composed and this one looked
	// like a grey pancake floating in grey nothing.
	const FLinearColor FogInscatteringColour(0.043f, 0.078f, 0.192f);
}

ASolsticeRealm::ASolsticeRealm()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BaseMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(Root);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseMesh->bUseAsyncCooking = true;

	SkyMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SkyMesh"));
	SkyMesh->SetupAttachment(Root);
	SkyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyMesh->SetCastShadow(false);

	PropMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PropMesh"));
	PropMesh->SetupAttachment(Root);
	PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PropMesh->SetCastShadow(false);
	PropMesh->bUseAsyncCooking = true;

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(Root);
	SunLight->SetMobility(EComponentMobility::Movable);
	// The cel material is unlit, so this light shades nothing today. It stays because it is the
	// only thing that defines *which way the sun is* for the shadow pass and for the sky capture,
	// and its intensity is kept at the physical value (a white Lambertian facing it lands at ~1.0)
	// instead of the 50x bisect value that used to be here - that value plus histogram exposure is
	// what turned the whole frame into a white disc (R-41).
	SunLight->SetIntensity(SunLightIntensity);
	SunLight->SetCastShadows(true);
	SunLight->SetLightColor(SunLightColour);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetIntensity(SkyLightIntensity);
	// A captured skylight is only as good as what it captures: BuildStatics recaptures it once the
	// dome exists. The lower hemisphere stays open so the ground bounce is not clipped to black.
	SkyLight->bLowerHemisphereIsBlack = false;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetVisibility(true);
	Fog->SetFogDensity(ClearFogDepth / (6400.0f * 5.6f));	// placeholder; BuildStatics recalibrates
	Fog->SetFogHeightFalloff(FogHeightFalloff);
	Fog->SetFogInscatteringColor(FogInscatteringColour);
	Fog->SetStartDistance(6400.0f * 5.6f * 0.12f);

	// P2 "dusk card" grade. The target is studio/previews/style-fusion.html: warm key light, cool
	// shadows, a shallow focus band and a slightly lifted contrast. All of it is stock post-process
	// state - no custom material, no plugin, and every value is tunable at runtime.
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	{
		FPostProcessSettings& PP = PostProcess->Settings;

		// ---- exposure (R-41) -------------------------------------------------
		// Manual, not histogram: the frame must not re-expose itself when the camera moves, when
		// fog rolls in, or when a cloud crosses the sun. Everything else in this block is a
		// *grade* on top of a fixed exposure, which is the only combination where a palette value
		// reads on screen as the colour that was authored.
		PP.bOverride_AutoExposureMethod = true;
		PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = ExposureStops;
		// The physical-camera path would recompute the exposure from the lens, which is exactly
		// the coupling this project does not want.
		PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		PP.AutoExposureApplyPhysicalCameraExposure = false;

		// Grade: warm gain, cool gamma, a touch more saturation and contrast than neutral.
		PP.bOverride_ColorSaturation = true;
		// Reference frames for this game are saturated autumn: the old grade was almost neutral and
		// the terrain ramp underneath it read as grey-green mud. Push saturation and contrast, keep
		// the warm gain, and pull the vignette back so the edge is soft rather than theatrical.
		PP.ColorSaturation = FVector4(1.18f, 1.14f, 1.06f, 1.0f);
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.16f, 1.13f, 1.08f, 1.0f);
		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(0.97f, 0.99f, 1.05f, 1.0f);
		PP.bOverride_ColorGain = true;
		PP.ColorGain = FVector4(1.06f, 1.01f, 0.92f, 1.0f);

		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.26f;
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.65f;
		PP.bOverride_BloomThreshold = true;
		PP.BloomThreshold = 0.55f;
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.10f;

		// Shallow band: the far rim and the near foreground go soft, the realm stays sharp. The
		// focal distance itself is driven every tick from the actual camera distance (see Tick), so
		// zooming in does not turn the whole picture into mush.
		PP.bOverride_DepthOfFieldFocalDistance = true;
		PP.DepthOfFieldFocalDistance = 30000.0f;
		PP.bOverride_DepthOfFieldFocalRegion = true;
		PP.DepthOfFieldFocalRegion = 9000.0f;
		PP.bOverride_DepthOfFieldNearTransitionRegion = true;
		PP.DepthOfFieldNearTransitionRegion = 13500.0f;
		PP.bOverride_DepthOfFieldFarTransitionRegion = true;
		PP.DepthOfFieldFarTransitionRegion = 13500.0f;
		PP.bOverride_DepthOfFieldFstop = true;
		PP.DepthOfFieldFstop = 2.8f;
	}
}

USolsticeGameInstance* ASolsticeRealm::Game() const
{
	return GetGameInstance<USolsticeGameInstance>();
}

FSimKernel* ASolsticeRealm::Kernel() const
{
	USolsticeGameInstance* Instance = Game();
	return Instance ? &Instance->Kernel() : nullptr;
}

void ASolsticeRealm::BeginPlay()
{
	Super::BeginPlay();

	Palette = Gen::DefaultPalette();

	FSimKernel* Sim = Kernel();
	if (!Sim)
	{
		UE_LOG(LogSolstice, Error, TEXT("realm has no kernel; check that SolsticeGameInstance is set"));
		return;
	}

	ChunksPerAxis = FMath::Max(1, Sim->GetChunksPerAxis());

	ChunkMeshes.Reset();
	for (int32 Index = 0; Index < ChunksPerAxis * ChunksPerAxis; ++Index)
	{
		UProceduralMeshComponent* Chunk = NewObject<UProceduralMeshComponent>(this,
			*FString::Printf(TEXT("Chunk_%d"), Index));
		Chunk->SetupAttachment(Root);
		Chunk->RegisterComponent();
		Chunk->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Chunk->SetCollisionObjectType(ECC_WorldStatic);
		Chunk->SetCollisionResponseToAllChannels(ECR_Block);
		Chunk->bUseAsyncCooking = true;
		FCelMaterials::ApplyToMesh(Chunk);
		ChunkMeshes.Add(Chunk);
	}

	FCelMaterials::ApplyToMesh(BaseMesh);
	FCelMaterials::ApplyToMesh(SkyMesh);
	FCelMaterials::ApplyToMesh(PropMesh);

	BuildStatics();
	FCelMaterials::ApplySky(Sim->GetSky(), Sim->IsChaos());

	UE_LOG(LogSolstice, Log, TEXT("realm built: %dx%d tiles in %d chunk mesh(es); paper mode = %s"),
		Sim->GetSize(), Sim->GetSize(), ChunkMeshes.Num(),
		FCelMaterials::Get().bAuthored ? TEXT("authored") : TEXT("engine fallback"));
}

void ASolsticeRealm::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ChunkMeshes.Reset();
	Super::EndPlay(EndPlayReason);
}

void ASolsticeRealm::BuildStatics()
{
	FSimKernel* Sim = Kernel();
	if (!Sim)
	{
		return;
	}

	// The kraft base never changes once the realm size is fixed.
	Scratch.Reset();
	Gen::BuildBase(*Sim, Palette, Scratch);
	Scratch.PushTo(BaseMesh, 0, false);

	BuildSkyMesh();

	// The dome exists now, so the captured skylight finally has a sky to sample. Without this it
	// captures an empty black world and the fill light contributes nothing.
	if (SkyLight)
	{
		SkyLight->RecaptureSky();
	}

	// Fog is calibrated against the *viewing* distance, not the world size. The entry framing
	// parks the camera at about 5.6 realm radii (radius x 1.12 / sin(vertical half-FOV)); a
	// density of 0.012 over 36 k uu gives an optical depth of ~400, which turns the whole realm
	// into one flat fog-coloured disc - exactly the black pancake the first textured build
	// showed. What we actually want is a fixed fog *depth at one fit distance*: ~0.25 clear,
	// pushed towards 1.2 when the weather turns, so the far rim softens but the realm keeps
	// its colours. Density = depth / fit distance keeps that constant at any world size.
	if (Fog && Sim)
	{
		const float Radius = FMath::Max(1000.0f, GetRealmRadiusUU());
		const float FitDistance = Radius * 5.6f;	// keep in step with FitDistance() in the player controller
		Fog->SetFogDensity(ClearFogDepth / FitDistance);
		Fog->SetStartDistance(FitDistance * 0.12f);

		UE_LOG(LogSolstice, Warning,
			TEXT("[diag] fog: realmRadius=%.0f uu, fit=%.0f, density=%.8f, start=%.0f"),
			Radius, FitDistance, Fog->FogDensity, Fog->StartDistance);
	}

	bStaticsBuilt = true;
}

void ASolsticeRealm::BuildSkyMesh()
{
	FSimKernel* Sim = Kernel();
	if (!Sim)
	{
		return;
	}

	const FSkyState& Sky = Sim->GetSky();
	const FVector Centre(0.0f, 0.0f, 0.0f);
	const float Radius = GetRealmRadiusUU() * DomeRadiusFactor;

	// Day/night decides the dome's two ends; cloud flattens the difference.
	const float Altitude = FMath::Clamp(Sky.SolarAltitudeDeg, -18.0f, 90.0f);
	const float Daylight = FMath::Clamp((Altitude + 4.0f) / 10.0f, 0.0f, 1.0f);
	const float Cloud = FMath::Clamp(Sky.CloudCover, 0.0f, 1.0f);

	const FLinearColor DayZenith(0.34f, 0.52f, 0.78f);
	const FLinearColor NightZenith(0.04f, 0.05f, 0.11f);
	const FLinearColor WarmHorizon(0.94f, 0.72f, 0.50f);

	FLinearColor Zenith = FMath::Lerp(NightZenith, DayZenith, Daylight);
	FLinearColor Horizon = FMath::Lerp(FMath::Lerp(FLinearColor(0.10f, 0.11f, 0.16f), WarmHorizon, Daylight), FLinearColor(0.62f, 0.64f, 0.68f), Cloud * 0.75f);

	if (Sim->IsChaos())
	{
		// Chaos: the sky is plain paper too. The first colour in the world should be the player's.
		Zenith = FMath::Lerp(Zenith, FLinearColor(0.88f, 0.86f, 0.82f), 0.45f);
		Horizon = FMath::Lerp(Horizon, FLinearColor(0.92f, 0.90f, 0.86f), 0.45f);
	}

	Scratch.Reset();
	OutlineScratch.Reset();
	// The dome is an opaque shell right now, and from anywhere that is not inside it, it fills the
	// whole screen with pale grey - that is the "thick white bowl" in every screenshot. WORLD SS1.1
	// wants it read as an *air volume* by a brightness difference, which needs a translucent
	// material that does not exist yet, so it stays unbuilt until M_Sky lands.
	// Gen::BuildSkyDome(Centre, Radius, Zenith, Horizon, Scratch);
	// 第 38 轮：分瓣纸壳作废。穹顶是完整半球，没有瓣，也没有硬边。 (petals removed)

	// The sun and the moon are paper discs pinned to the dome.
	const float AltRad = FMath::DegreesToRadians(FMath::Clamp(Sky.SolarAltitudeDeg, -30.0f, 90.0f));
	const float AzRad = FMath::DegreesToRadians(Sky.SunAzimuthDeg);
	const FVector SunDir(FMath::Cos(AltRad) * FMath::Cos(AzRad), FMath::Cos(AltRad) * FMath::Sin(AzRad), FMath::Sin(AltRad));
	FVector SunRight = FVector::CrossProduct(SunDir, FVector::UpVector).GetSafeNormal();
	if (SunRight.IsNearlyZero())
	{
		SunRight = FVector(1.0f, 0.0f, 0.0f);
	}
	const FVector SunUp = FVector::CrossProduct(SunRight, SunDir).GetSafeNormal();

	// Celestial bodies go into their own builder - see the two-section note at the bottom.
	const FLinearColor DiscColour = FMath::Lerp(FLinearColor(0.98f, 0.86f, 0.62f), FLinearColor(1.0f, 0.97f, 0.90f), Daylight);
	Gen::BuildDisc(Centre + SunDir * (Radius * 0.80f), SunRight * (Radius * 0.09f), SunUp * (Radius * 0.09f), 1.0f, DiscColour * (0.35f + 0.65f * Daylight), OutlineScratch);

	const FVector MoonDir = -SunDir;
	const FVector MoonRight = FVector::CrossProduct(MoonDir, FVector::UpVector).GetSafeNormal();
	const FVector MoonUp = FVector::CrossProduct(MoonRight, MoonDir).GetSafeNormal();
	// Lunar phase from the world clock. Masking a crescent needs a second coplanar card, which would
	// z-fight, so the phase drives brightness and apparent size instead: a thin crescent reads as a
	// small dim disc, a full moon as a large bright one. 29.53 days per cycle, new moon at phase 0.
	const float LunarDay = static_cast<float>(Sim ? Sim->GetWorldMinute() : 0) / (60.0f * 24.0f);
	const float Phase01 = FMath::Frac(LunarDay / 29.53f);
	const float MoonFull = 0.5f - 0.5f * FMath::Cos(2.0f * PI * Phase01);
	const float MoonRadius = Radius * 0.105f * (0.6f + 0.4f * MoonFull);  // 视直径 0.21 R (WORLD SS1.1)
	const FLinearColor MoonColour = FLinearColor(0.86f, 0.88f, 0.92f)
		* ((0.25f + 0.75f * MoonFull) * (1.0f - Daylight * 0.75f));
	Gen::BuildDisc(Centre + MoonDir * (Radius * 0.66f), MoonRight * MoonRadius, MoonUp * MoonRadius, 1.0f, MoonColour, OutlineScratch);

	// Two sections on purpose: 0 = the shell dome and its petals, 1 = the sun and the moon.
	// The shell has to step aside when the camera is parked outside it (SetSkyVisible), and while
	// the two shared one section, hiding the dome also erased the only celestial bodies the realm
	// had - which is exactly what R-21 recorded as "there is no sun or moon".
	Scratch.PushTo(SkyMesh, 0, false);
	OutlineScratch.PushTo(SkyMesh, 1, false);

	// ---- section 2: clouds and stars (REVIEW R-20 / R-21) --------------------
	// Both are paper discs pinned just inside the shell, so they never sit between the camera and
	// the realm: at every preset the camera is inside the shell too, and anything on the shell is
	// overhead. Cloud count follows the sim's cloud_cover - the sky and the panel agree, which is
	// the whole point of a weather readout. Stars appear only at night and only as much as the
	// night is clear.
	Scratch.Reset();
	const int32 CloudCount = FMath::RoundToInt(Cloud * 16.0f);

	// Wind drift and rain. The clouds were pinned to fixed azimuths, which made a "weather" readout
	// sit under a sky that never moved; they now slide with wall-clock time, faster when the sim
	// says the wind is strong, and go grey/wet when it rains. Wall clock rather than world time on
	// purpose: the visual sky should breathe even while the sim is paused.
	const float WindScale = 0.35f + Sim->GetSky().WindSpeed * 0.9f;
	const float Drift = static_cast<float>(FMath::Fmod(FApp::GetCurrentTime(), 3600.0)) * 0.06f * WindScale;
	const float Wet = FMath::Clamp(Sim->GetSky().RainIntensity, 0.0f, 1.0f);

	for (int32 Index = 0; Index < CloudCount; ++Index)
	{
		const float Seed = static_cast<float>(Index) * 2.399963f;			// golden angle
		const float Azimuth = Seed * 3.7f + Drift;
		const float ElevationRad = FMath::DegreesToRadians(FMath::Lerp(18.0f, 78.0f, FMath::Frac(Seed * 0.517f)));
		const FVector Dir(
			FMath::Cos(ElevationRad) * FMath::Cos(Azimuth),
			FMath::Cos(ElevationRad) * FMath::Sin(Azimuth),
			FMath::Sin(ElevationRad));
		FVector Right = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			continue;
		}
		const FVector Up = FVector::CrossProduct(Right, Dir).GetSafeNormal();
		const float Spread = Radius * (0.055f + 0.045f * FMath::Frac(Seed * 1.31f));
		// Rain darkens and cools the cotton instead of merely adding more of it - an overcast sky has
		// to lose brightness, or "it is raining" reads as "there are more clouds".
		const FLinearColor DryTone = FMath::Lerp(
			FLinearColor(0.98f, 0.97f, 0.94f), FLinearColor(0.70f, 0.72f, 0.77f), Cloud * 0.55f);
		const FLinearColor Tone = FMath::Lerp(DryTone, FLinearColor(0.50f, 0.54f, 0.60f), Wet * 0.75f);
		// A cloud is three overlapping cushions, not one flat disc: a flat card only reads from
		// one side, while a puffed blob catches the light on its own shading from any angle.
		const float Puff = Spread * 0.55f;
		Gen::Cushion(Scratch, Centre + Dir * (Radius * 0.962f), 6, Spread, Spread * 0.42f,
			Tone * 0.88f, Tone, Index * 17 + 3, Dir);
		Gen::Cushion(Scratch, Centre + Dir * (Radius * 0.960f) + Right * Puff * 0.9f + Up * Puff * 0.2f, 6,
			Spread * 0.72f, Spread * 0.34f, Tone * 0.92f, Tone, Index * 17 + 31, Dir);
		Gen::Cushion(Scratch, Centre + Dir * (Radius * 0.960f) - Right * Puff * 0.85f - Up * Puff * 0.1f, 6,
			Spread * 0.66f, Spread * 0.30f, Tone * 0.90f, Tone, Index * 17 + 67, Dir);
	}

	const float StarAlpha = (1.0f - Daylight) * (1.0f - Cloud * 0.85f);
	if (StarAlpha > 0.06f)
	{
		constexpr int32 StarCount = 150;
		for (int32 Index = 0; Index < StarCount; ++Index)
		{
			const float Seed = static_cast<float>(Index) * 2.399963f;
			const float Azimuth = Seed * 7.13f;
			const float ElevationRad = FMath::DegreesToRadians(FMath::Lerp(16.0f, 87.0f, FMath::Frac(Seed * 0.314f)));
			const FVector Dir(
				FMath::Cos(ElevationRad) * FMath::Cos(Azimuth),
				FMath::Cos(ElevationRad) * FMath::Sin(Azimuth),
				FMath::Sin(ElevationRad));
			FVector Right = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
			if (Right.IsNearlyZero())
			{
				continue;
			}
			const FVector Up = FVector::CrossProduct(Right, Dir).GetSafeNormal();
			const float Size = Radius * (0.0035f + 0.0045f * FMath::Frac(Seed * 1.77f));
			Gen::BuildDisc(Centre + Dir * (Radius * 0.985f), Right * Size, Up * Size, 1.0f,
				FLinearColor(1.0f, 0.98f, 0.90f) * StarAlpha, Scratch);
		}
	}

	Scratch.PushTo(SkyMesh, 2, false);
	SkyMesh->SetMeshSectionVisible(2, true);
	SkyMesh->SetMeshSectionVisible(0, bSkyVisible);
	SkyMesh->SetMeshSectionVisible(1, true);
}

void ASolsticeRealm::RebuildDirtyChunks(int32 Budget)
{
	FSimKernel* Sim = Kernel();
	if (!Sim || ChunkMeshes.Num() == 0)
	{
		return;
	}

	int32 Built = 0;
	for (int32 ChunkY = 0; ChunkY < ChunksPerAxis && Built < Budget; ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < ChunksPerAxis && Built < Budget; ++ChunkX)
		{
			const int32 Index = ChunkY * ChunksPerAxis + ChunkX;
			if (!ChunkMeshes.IsValidIndex(Index))
			{
				continue;
			}
			const bool bDirty = Sim->IsRegionDirty(ChunkX, ChunkY) || !Sim->IsRegionEverBuilt(ChunkX, ChunkY);
			if (!bDirty)
			{
				continue;
			}
			Scratch.Reset();
			Gen::BuildChunk(*Sim, ChunkX, ChunkY, Palette, Scratch);
			Scratch.PushTo(ChunkMeshes[Index], 0, true);
			UE_LOG(LogSolstice, Warning, TEXT("[diag] chunk %d,%d verts=%d tris=%d"),
				ChunkX, ChunkY, Scratch.Num(), Scratch.Triangles.Num() / 3);
			Sim->ClearRegionDirty(ChunkX, ChunkY);
			Sim->MarkRegionBuilt(ChunkX, ChunkY);
			++Built;
		}
	}
}

void ASolsticeRealm::RebuildProps()
{
	FSimKernel* Sim = Kernel();
	if (!Sim)
	{
		return;
	}
	Scratch.Reset();
	OutlineScratch.Reset();
	Gen::BuildLife(*Sim, Palette, Scratch, OutlineScratch);

	Scratch.PushTo(PropMesh, 0, false);
	OutlineScratch.PushTo(PropMesh, 1, false);
}

void ASolsticeRealm::UpdateSun()
{
	FSimKernel* Sim = Kernel();
	if (!Sim)
	{
		return;
	}
	const FSkyState& Sky = Sim->GetSky();

	// The light must travel FROM the visual sun INTO the ground. Derive the direction from the
	// same unit vector the sky card uses (SunDir points at the sun), so the lit side of the
	// realm and the bright disc on the dome can never disagree: a directional light emits along
	// its +X, so its rotation is -SunDir. The old straight (-altitude, azimuth) mapping pointed
	// the emission back up at the sky and the whole sheet sat in its own ambient - the black
	// pancake the textured build showed.
	const float AltRad = FMath::DegreesToRadians(FMath::Clamp(Sky.SolarAltitudeDeg, -60.0f, 90.0f));
	const float AzRad = FMath::DegreesToRadians(Sky.SunAzimuthDeg);
	const FVector SunDir(
		FMath::Cos(AltRad) * FMath::Cos(AzRad),
		FMath::Cos(AltRad) * FMath::Sin(AzRad),
		FMath::Sin(AltRad));
	// Rotation from -SunDir: the component's +X is its emission axis.
	const FRotator LightRotation = (-SunDir).Rotation();
	SunLight->SetWorldRotation(LightRotation);
	// Daylight ramp only - the *colour* and the band direction are pushed to the material by
	// ApplySky, so this scalar never has to encode a look.
	SunLight->SetIntensity(
		FMath::Lerp(0.15f, 1.0f, FMath::Clamp((Sky.SolarAltitudeDeg + 8.0f) / 18.0f, 0.0f, 1.0f)) * SunLightIntensity);

	const float Wet = FMath::Clamp(Sky.RainIntensity * 0.7f + Sky.CloudCover * 0.3f, 0.0f, 1.0f);
	// The void outside the realm is pinned deep blue (WORLD SS1.1, #0B1431) in every weather -
	// weather only pushes it slightly towards grey-blue, it never returns to pale fog grey.
	// This lerp used to stomp the build-time colour back to (0.70, 0.73, 0.78), which is why the
	// surround stayed grey no matter what the constructor set.
	const FLinearColor ClearVoid(0.043f, 0.078f, 0.192f);
	const FLinearColor WetVoid(0.16f, 0.19f, 0.27f);
	Fog->SetFogInscatteringColor(FMath::Lerp(ClearVoid, WetVoid, Wet));

	// Density keeps the fit-distance calibration from BuildStatics; weather deepens the fog
	// towards the wet depth on top of it.
	const float Radius = FMath::Max(1000.0f, GetRealmRadiusUU());
	const float FitDistance = Radius * 5.6f;
	Fog->SetFogDensity(FMath::Lerp(ClearFogDepth, WetFogDepth, Wet) / FitDistance);
	Fog->SetStartDistance(FitDistance * 0.12f);

	// Throttled: this used to print every tick, which buried the log it was meant to help read.
	static int32 DiagCalls = 0;
	if ((++DiagCalls % 120) == 0)
	{
		const FLinearColor FogNow = Fog->FogInscatteringLuminance;
		UE_LOG(LogSolstice, Log,
			TEXT("[diag] sun: alt=%.1f intensity=%.2f fog=(%.3f,%.3f,%.3f) sunDir=(%.2f,%.2f,%.2f)"),
			Sky.SolarAltitudeDeg, SunLight->Intensity, FogNow.R, FogNow.G, FogNow.B,
			SunDir.X, SunDir.Y, SunDir.Z);
	}
}

void ASolsticeRealm::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The shell is a container, not an obstruction. It is opaque paper, so whenever the camera is
	// parked outside it the dome becomes the only thing on screen - which is exactly what the old
	// default framing produced (arm = realm radius * 2.1 against a shell at radius * 1.34).
	// Outside: step aside. Inside: it is the realm's wall again.
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PlayerCtl = World->GetFirstPlayerController())
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			PlayerCtl->GetPlayerViewPoint(ViewLocation, ViewRotation);
			const float ShellRadius = GetRealmRadiusUU() * DomeRadiusFactor;
			const float ViewDistance = FVector::Dist(ViewLocation, GetRealmCentre());
			SetSkyVisible(ViewDistance < ShellRadius);

			// The focus band follows the camera, so the realm centre is always the sharp plane and
			// the near foreground / far rim soften by the same proportion at any zoom. A fixed focal
			// distance would either do nothing at the entry framing or blur everything up close.
			if (PostProcess)
			{
				FPostProcessSettings& PP = PostProcess->Settings;
				PP.DepthOfFieldFocalDistance = ViewDistance;
				PP.DepthOfFieldFocalRegion = FMath::Max(4000.0f, ViewDistance * 0.30f);
				const float Transition = FMath::Max(6000.0f, ViewDistance * 0.45f);
				PP.DepthOfFieldNearTransitionRegion = Transition;
				PP.DepthOfFieldFarTransitionRegion = Transition;
			}
		}
	}

	USolsticeGameInstance* Instance = Game();
	FSimKernel* Sim = Kernel();
	if (!Instance || !Sim)
	{
		return;
	}

	if (!bStaticsBuilt)
	{
		BuildStatics();
	}

	Instance->TickSimulation(DeltaSeconds);

	// The camera's tile drives SimLOD (docs/tech/ARCHITECTURE.md SS2.2.3).
	if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		int32 FocusTile = INDEX_NONE;
		if (WorldToTile(ViewLocation, FocusTile))
		{
			Sim->SetFocusTile(FocusTile);
		}
	}

	RebuildDirtyChunks(2);

	// ---- rebuild gates ---------------------------------------------------------
	// Both meshes used to be rebuilt on a timer whether or not anything had changed: the props ten
	// times a second (one section per standee plus its ink card) and the sky five times a second
	// (dome, petals, sun and moon discs). In a still scene that is fifteen full vertex-buffer
	// uploads a second for no visual change at all. Each now carries a cheap signature and only
	// rebuilds when the signature moves.
	auto Mix32 = [](uint32 Hash, uint32 Value) -> uint32
	{
		Hash ^= Value + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
		return Hash;
	};

	PropAccumulator += DeltaSeconds;
	if (PropAccumulator >= PropInterval)
	{
		PropAccumulator = 0.0f;

		uint32 Signature = 0x811c9dc5u;
		const TArray<FIndividual>& Individuals = Sim->GetIndividuals();
		Signature = Mix32(Signature, static_cast<uint32>(Individuals.Num()));
		for (const FIndividual& Individual : Individuals)
		{
			Signature = Mix32(Signature, static_cast<uint32>(Individual.Tile));
			Signature = Mix32(Signature, static_cast<uint32>(Individual.GetKind()));
			Signature = Mix32(Signature, static_cast<uint32>(Individual.Facing));
		}

		if (!bPropsBuilt || Signature != LastPropSignature)
		{
			LastPropSignature = Signature;
			bPropsBuilt = true;
			RebuildProps();
		}
	}

	SkyAccumulator += DeltaSeconds;
	{
		const FSkyState& SkyState = Sim->GetSky();
		uint32 Signature = 0xf00dbabeu;
		// Quantised so that a millimetre of drift is not a rebuild: half a degree of sun, two
		// degrees of azimuth, five percent of cloud and rain.
		Signature = Mix32(Signature, static_cast<uint32>(FMath::Max(0.0f, SkyState.SolarAltitudeDeg + 90.0f) * 2.0f));
		Signature = Mix32(Signature, static_cast<uint32>(SkyState.SunAzimuthDeg * 0.5f));
		Signature = Mix32(Signature, static_cast<uint32>(SkyState.CloudCover * 20.0f));
		Signature = Mix32(Signature, static_cast<uint32>(SkyState.RainIntensity * 20.0f));
		Signature = Mix32(Signature, Sim->IsChaos() ? 1u : 0u);

		if (!bSkyBuilt || Signature != LastSkySignature)
		{
			LastSkySignature = Signature;
			bSkyBuilt = true;
			BuildSkyMesh();
		}
	}

	FCelMaterials::ApplySky(Sim->GetSky(), Sim->IsChaos());
	UpdateSun();
}

FVector ASolsticeRealm::GetRealmCentre() const
{
	return GetActorLocation();
}

float ASolsticeRealm::GetRealmRadiusUU() const
{
	const FSimKernel* Sim = Kernel();
	const int32 Size = Sim ? Sim->GetSize() : 128;
	return static_cast<float>(Size) * 0.5f * MeterUU;
}

FVector ASolsticeRealm::TileToWorld(int32 TileIndex) const
{
	const FSimKernel* Sim = Kernel();
	if (!Sim || TileIndex < 0)
	{
		return GetActorLocation();
	}
	int32 X = 0;
	int32 Y = 0;
	Sim->IndexToXY(TileIndex, X, Y);
	return TileSurfaceWorld(X, Y);
}

FVector ASolsticeRealm::TileSurfaceWorld(int32 TileX, int32 TileY) const
{
	const FSimKernel* Sim = Kernel();
	if (!Sim)
	{
		return GetActorLocation();
	}
	const FTile* Tile = Sim->TileAt(TileX, TileY);
	const float Surface = Tile ? FMath::Max(Tile->Height, Tile->IsWater() ? Sim->GetSky().WaterLevel : Tile->Height) : 0.0f;
	return GetActorLocation() + Gen::TileToLocal(*Sim, TileX, TileY, Surface);
}

bool ASolsticeRealm::WorldToTile(const FVector& WorldLocation, int32& OutTileIndex) const
{
	OutTileIndex = INDEX_NONE;
	const FSimKernel* Sim = Kernel();
	if (!Sim)
	{
		return false;
	}
	const FVector Local = WorldLocation - GetActorLocation();
	const float Half = static_cast<float>(Sim->GetSize()) * 0.5f;
	const int32 X = FMath::FloorToInt(Local.X / MeterUU + Half + 0.5f);
	const int32 Y = FMath::FloorToInt(Local.Y / MeterUU + Half + 0.5f);
	if (!Sim->InBounds(X, Y))
	{
		return false;
	}
	OutTileIndex = Sim->ToIndex(X, Y);
	return true;
}

int32 ASolsticeRealm::PickTile(APlayerController* Controller, FVector& OutHitLocation) const
{
	OutHitLocation = FVector::ZeroVector;
	if (!Controller || !GetWorld())
	{
		return INDEX_NONE;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!Controller->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return INDEX_NONE;
	}

	const float Reach = GetRealmRadiusUU() * 6.0f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SolsticePick), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, RayOrigin, RayOrigin + RayDirection * Reach, ECC_Visibility, Params);
	if (!bHit)
	{
		return INDEX_NONE;
	}

	OutHitLocation = Hit.ImpactPoint;
	int32 Tile = INDEX_NONE;
	return WorldToTile(Hit.ImpactPoint, Tile) ? Tile : INDEX_NONE;
}

void ASolsticeRealm::SetSkyVisible(bool bVisible)
{
	bSkyVisible = bVisible;

	// Only the shell (section 0) steps aside. The sun and the moon live in section 1 and stay in
	// the sky at every camera position - a realm whose sky can go empty reads as a prop rather
	// than a world (REVIEW R-21).
	SkyMesh->SetMeshSectionVisible(0, bVisible);
}
