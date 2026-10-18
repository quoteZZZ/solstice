// Solstice - World layer: the realm actor.

#include "World/SolsticeRealm.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
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

	// ------------------------------------------------------------------
	// The sky (docs/art/art.md SS2.2).
	//
	// There is no dome. A hemisphere around the realm only works when the camera is inside it,
	// and the framing puts the camera at 5.6 realm radii - outside by a factor of five, which is
	// why every "build the dome" attempt ended as either an invisible section or a white bowl
	// over the whole frame. What the style actually asks for is a *painted backdrop*: flat
	// layers at infinity, near-white at the horizon, deepening upward, with two cloud decks.
	//
	// So the sky is one card, perpendicular to the view axis, `card gap` behind the realm's far
	// rim, re-placed every frame and repainted only when its content changes. The distance grows
	// with the camera so the card is never in front of the terrain at any zoom, and the gap is
	// chosen to keep it inside the sharp part of the depth-of-field band - a blurred sky is a
	// smudge, not a matte.
	// ------------------------------------------------------------------
	constexpr float SkyCardGap = 1.35f;		// realm radii behind the realm centre

	/**
	 * How far the open sea runs, in realm radii.
	 *
	 * The sea has to leave the frame in every direction, otherwise the picture ends in blue nothing
	 * and the island reads as a plate. 24 radii is past the frame edge at the landscape framing and
	 * still inside the sky card, which is pushed out past it (see UpdateSkyView).
	 */
	// 60 radii, not 24.
	//
	// At 24 the sea's far edge subtended 1.35 degrees below the horizon, which at the landscape
	// framing is ~33 px - and in that gap the sky card showed through as a grey band lying on the
	// water. 60 radii drops the edge to 0.54 degrees (~13 px) where the fog swallows it.
	constexpr float OceanRadiusRadii = 60.0f;
	/**
	 * Far water fades into this.
	 *
	 * It is the sky card's own "under the horizon" tone, not a colour of its own: the card is what
	 * shows between the sea's far edge and the true horizon, so if the two differ by even a little
	 * the frame gets a pale band lying on the water.
	 */
	const FLinearColor OceanHaze = Solstice::CelSrgb(0x8A, 0xB4, 0xD6);

	/**
	 * The sea is drawn with M_Sky (raw vertex colour), while the realm's own water goes through
	 * M_Cel and picks up the lit band (~1.2x). Authoring the open sea at the same palette value as
	 * the tile water therefore left a dark ring where they meet; this factor puts the two on the
	 * same screen value without dragging the sea into the band/AO path.
	 */
	// 1.9, not 1.22.
	//
	// The sea's albedo is the only lever that behaves like the land's: raise it and the same sun
	// and the same exposure lift it. Raising emissive instead put the sea on a *second* lighting
	// path, and the two immediately fought - the water stayed dark while the island blew out.
	constexpr float OceanScreenGain = 0.47f;
	constexpr float SkyCardOverscan = 1.12f;

	// Sun and moon are painted, not projected: at a 35 degree down-pitch a physically placed sun
	// sits above the frame and is never seen. The azimuth still decides where on the card the
	// disc lands (so the light and the sun agree), the elevation decides how high up the card,
	// compressed into the part of the frame that is actually sky.
	constexpr float SkyBodyYawLimitDeg = 75.0f;
	// Angular *radius*. Both are deliberately larger than the real 0.26 deg - a cel sky wants a
	// readable disc - but 2.4/3.1 filled an eighth of the frame and, at preset 0, the strip of
	// sky above the peak is narrower than the disc.
	constexpr float SunAngularRadiusDeg = 1.9f;
	constexpr float MoonAngularRadiusDeg = 2.5f;
	/** 0..1 of a full horizontal loop per second - art.md asks for 0.5-2 deg/s of drift. */
	constexpr float SkyDriftRate = 0.02f;

	/** sRGB tones of the three-band gradient, per time of day (art.md SS2.4). */
	const FLinearColor DayZenith = Solstice::CelSrgb(0x22, 0x55, 0xA8);
	const FLinearColor DayMiddle = Solstice::CelSrgb(0x69, 0xA6, 0xDC);
	// A shade under the art note's #EAF3FA: with no tone curve there is no shoulder, so the
	// near-white has to be authored below the clip point or the band turns into flat paper.
	// #B9D8EE, not near-white.
	//
	// A near-white horizon is an *animation* convention (it reads as light scattering in a
	// painted background). In a photographic sea it becomes a white stripe lying across the
	// frame - measured at 60 px of luminance 0.91, the single most damaging element in the shot.
	// The photographic horizon is the brightest *blue*, not white.
	const FLinearColor DayHorizon = Solstice::CelSrgb(0x93, 0xC2, 0xE6);
	const FLinearColor DayUnder = Solstice::CelSrgb(0x8A, 0xB4, 0xD6);

	const FLinearColor DuskZenith = Solstice::CelSrgb(0x4A, 0x3F, 0x7A);
	const FLinearColor DuskMiddle = Solstice::CelSrgb(0xE0, 0x8A, 0x5A);
	const FLinearColor DuskHorizon = Solstice::CelSrgb(0xFF, 0xD0, 0x8A);
	// The band below the horizon is a *plane*, not sky: it has to sit clearly darker than the
	// realm's own shadow band or the two merge. Authored at #8C6A8E it was the same lightness as
	// the dusk shadow tint, and the disc's cut edge dissolved into the ground plane (2026-10-18).
	const FLinearColor DuskUnder = Solstice::CelSrgb(0x4A, 0x34, 0x54);

	const FLinearColor NightZenith = Solstice::CelSrgb(0x10, 0x1C, 0x33);
	const FLinearColor NightMiddle = Solstice::CelSrgb(0x1E, 0x35, 0x50);
	const FLinearColor NightHorizon = Solstice::CelSrgb(0x3A, 0x5F, 0x80);
	const FLinearColor NightUnder = Solstice::CelSrgb(0x16, 0x28, 0x3C);

	// ------------------------------------------------------------------
	// Scene rig.
	//
	// REBOOT B0 之前这里写着「cel 材质�?unlit，自己画色阶，灯只提供方向」——那句话是对的，
	// 也是画面永远像贴纸的原因：方向光投下的影子没有人接收�?	//
	// 现在 M_Cel �?DefaultLit，太阳真的照着世界。于是这组数字第一次有了物理含义：
	// 太阳/天光�?*比�?*决定阴影可见度，比值太低时环境光会把影子洗掉（实测 3.2/0.4 �?	// 树冠的投影几乎看不见）�?.0/0.25 �?36:1 是晴天该有的量级，也是让接触阴影出得来的
	// 最低比值�?	// ------------------------------------------------------------------
	// 100,000 lux, not 9.
	//
	// The atmosphere is evaluated in physical units, so a 9 lux sun and a physically-lit sky
	// differ by four orders of magnitude: the sky came out black with a single white line at the
	// horizon. Sunlight on a clear day is ~100,000 lux; with a manual EV100 that matches it, land
	// and sky finally sit on the same scale.
	// 5.5, not 9.0: at 9 the island's own albedo blew past the exposure - the framing
	// midpoint measured 0.675 against a 0.45-0.60 target. The sea is emissive and does not
	// take part, so this only affects the land.
	// 14, not 4.6, and the sky light below comes down with it.
	//
	// These two numbers do not set brightness - the exposure does - they set the *ratio*, and
	// the ratio is what decides whether the frame has a light direction at all. A directional
	// light of 4.6 against a captured sky at 0.25 looked reasonable on paper and measured as
	// nothing: the island's large-scale shading had a direction concentration of R = 0.006,
	// i.e. perfectly isotropic, so no face was brighter than any other and the world read as a
	// flat illustration. Ambient was carrying the frame and the sun was decoration.
	// 100, not 14, and the sky light comes back up with it.
	//
	// Every value in this block was calibrated while M_Cel was silently unlit: its vertex colour
	// was wired to EmissiveColor as well as BaseColor, so the ground showed the palette at full
	// brightness no matter what the lights did, and "tune the light until the island looks right"
	// was really "tune the light until nothing changes, then pick a number". Once that stale
	// connection was cleared (see FSolsticeCelMaterialBuilder::ResetGraph) a sun of 14 left the
	// island almost black, and the sweep that followed put the correct value an order of
	// magnitude higher: measured at noon, island luminance 144.9 with every light off, 146.0 at
	// sun=14, 180.8 at sun=140, and a readable mid-tone at sun=40 (560 in the units below).
	constexpr float SunLightIntensity = 100.0f;
	const FLinearColor SunLightColour(1.0f, 0.97f, 0.92f);
	// 1.0, not 0.10.
	//
	// The same miscalibration: with the ground unlit, ambient could be switched off without
	// changing a pixel (measured 1.3% over the whole island), so any value looked harmless. Now
	// that the ground is really lit, 0.10 leaves the shaded side of every hill at black. 1.0 is
	// the value at which the sky alone reads as skylight rather than as nothing.
	constexpr float SkyLightIntensity = 1.0f;

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
	// ------------------------------------------------------------------
	// Display-referred pipeline (R-44).
	//
	// The palette and the painted sky are authored as *screen* colours, so the frame has to get
	// out of their way: a filmic tone curve desaturates and rolls off every highlight, and one
	// stop of exposure doubles every value before it does. Together they turned the near-white
	// horizon the style asks for into a blown-out grey and pulled the blues toward cyan - the
	// sky was authored correctly and arrived wrong.
	//
	// So: no tone curve, minimal exposure, a grade close to neutral. Every colour on screen is
	// then the colour in the palette, which is the only way "tune the look" means "tune the
	// palette" instead of "tune the pipeline until the palette reappears".
	// ------------------------------------------------------------------
	// 0.75, not 1.25.
	//
	// `SkyDim = 0.5^ExposureStops` divides the sky so it arrives at its authored value; at 1.25
	// that is 2.4x, which left the sky a full stop darker than the sunlit land. The seam between
	// them is the grey band that looked like fog lying on the water.
	// EV compensation, not EV100 (the physical-camera path is off below, so the manual mode
	// treats this as a gain in stops, not as a camera EV).
	//
	// The sun went from 9 to 100,000 lux - 13.4 stops - so the compensation comes down by the
	// same 13.4: 0.75 - 13.4 = -12.65. Getting this wrong is not subtle: at +15 the frame is pure
	// white, at 0 it is black.
	constexpr float ExposureStops = -1.15f;

	// Fog depth (optical depth at one fit-camera-distance) for clear and fully overcast sky.
	// Density is derived from these against the fit distance - see BuildStatics.
	// 0.06, not 0.25.
	//
	// The fog was calibrated when the world was a 128 m disc with nothing behind it, where a
	// quarter of an optical depth at the fit distance was the only thing giving it air. Now the
	// camera looks at a 400 km sky dome, and exponential fog at that distance is fully opaque:
	// it multiplies the whole sky by the fog colour, which is why every gradient arrived as one
	// pale wash and the clouds vanished into it (measured 2026-10-18). Aerial perspective now
	// belongs to the sea's own haze ramp, which is authored per stop and does not flatten the sky.
	constexpr float ClearFogDepth = 0.06f;
	constexpr float WetFogDepth = 1.20f;
	constexpr float FogHeightFalloff = 0.06f;
	// The void outside the realm is not fog-grey. WORLD SS1.1 pins it: a low-saturation deep blue
	// (#0B1431) field with distant stars, against which the lit disc reads as the only bright thing
	// in frame - that contrast is half of why reference shots look composed and this one looked
	// like a grey pancake floating in grey nothing.
	const FLinearColor FogInscatteringColour = Solstice::CelSrgb(0xCF, 0xE6, 0xF2);
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
	// Hidden, not deleted: the painted card still owns the sun/moon/star discs and the cloud
	// shapes, and those are re-attached to the atmosphere in a later pass. Until then the
	// atmosphere supplies the gradient and the disc, and a hidden card cannot show an edge.
	SkyMesh->SetVisibility(true);

	PropMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PropMesh"));
	PropMesh->SetupAttachment(Root);
	PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Trees and rocks cast. Without this the island is a green carpet with standees printed on
	// it: 120 props, no shadow under any of them, and the eye reads that as flat art rather than
	// as objects standing on ground. The mesh is small (a few hundred triangles per prop).
	PropMesh->SetCastShadow(true);
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

	// ---- shadows have to reach the realm ----------------------------------
	//
	// The engine default for a movable directional light is 20000 uu = 200 m of cascaded shadow
	// range, and the framing puts the camera ~213 m from the realm's centre. Every square metre
	// of the island therefore fell *outside* the last cascade: `SetCastShadows(true)` was true,
	// every mesh was casting, and no shadow existed anywhere on screen. Measured: at a sun
	// altitude of 2.4 degrees - where a 21 m hill should throw a 500 m shadow across the water -
	// the frame contained no darker water at all.
	//
	// 900 m covers the realm, the lagoon and the near shelf, which is everything close enough to
	// read. Further out the fog has taken over anyway.
	SunLight->DynamicShadowDistanceMovableLight = 90000.0f;
	SunLight->DynamicShadowCascades = 3;
	// Push the cascade split towards the camera: the island is a compact object, and the default
	// exponent spends most of the range on the far cascades where there is only water.
	SunLight->CascadeDistributionExponent = 2.2f;
	// Contact shadows are OFF, and the reason is a measurement, not taste.
	//
	// ContactShadowLength is in *screen* space: 0.06 is 6% of the viewport height, which on a
	// terrain 213 m away is a very long world-space ray. It read a large part of every slope as
	// occluded, which is why raising the sun by 14x only moved the island's mean luminance from
	// 80 to 98 - the light was there and the shadow term was eating it. Start at 0 and only
	// reintroduce it with a per-preset length once there is a close-up framing to tune it against.
	SunLight->ContactShadowLength = 0.0f;
	SunLight->ContactShadowLengthInWS = false;

	// Tells the atmosphere which light is the sun: it drives the sky gradient and draws the disc.
	SunLight->bAtmosphereSunLight = true;
	SunLight->AtmosphereSunLightIndex = 0;

	// ---- atmosphere ---------------------------------------------------
	//
	// B2: the sky is no longer a painted card. A card is a finite plane at a finite distance, so
	// there is always a view angle where its edge shows - that edge is the grey band that lay
	// across the frame at eye level. Real atmospheric scattering has no edge at all, because it
	// is evaluated per pixel along the view ray.
	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);
	SkyAtmosphere->SetMobility(EComponentMobility::Movable);
	// A slightly thinner atmosphere than Earth's default: it keeps the horizon bright and the
	// zenith saturated, which is what the cel palette was authored against.
	// NOT ENABLED YET.
	//
	// `r.SupportSkyAtmosphere` is a shader permutation: flipping it in DefaultEngine.ini does not
	// take effect until the shaders are recompiled for this checkout. With the flag on and the
	// permutation missing, the sky renders pure black (measured 2026-10-18). The card stays until
	// a build that actually contains the atmosphere permutation exists.
	SkyAtmosphere->SetVisibility(false);
	SkyAtmosphere->SetRayleighScatteringScale(0.45f);
	SkyAtmosphere->SetMieScatteringScale(1.4f);
	SkyAtmosphere->SetMieAnisotropy(0.72f);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetIntensity(SkyLightIntensity);
	// Real-time capture: with an atmosphere there is no dome mesh to recapture against, and a
	// static capture would freeze whatever the sky looked like at build time.
	// Real-time capture is off while the atmosphere is off: it would capture a black sky and
	// the whole scene would lose its ambient (measured: the sea went to pure black).
	SkyLight->bRealTimeCapture = false;
	// The lower hemisphere stays open so the ground bounce is not clipped to black.
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
		// `-SolsticeEnv=` third component: extra stops, applied once here. Exposure is the one
		// knob that sets brightness outright; every other number in this block is a grade.
		{
			FString EnvSpec;
			if (FParse::Value(FCommandLine::Get(), TEXT("SolsticeEnv="), EnvSpec, false))
			{
				TArray<FString> Parts;
				EnvSpec.ParseIntoArray(Parts, TEXT(","), true);
				if (Parts.Num() == 3)
				{
					// -1 means "leave the exposure alone", matching the sun/sky components.
					const float Stops = static_cast<float>(FCString::Atof(*Parts[2]));
					if (!FMath::IsNearlyEqual(Stops, -1.0f))
					{
						PP.AutoExposureBias += Stops;
					}
				}
			}
		}
		// The physical-camera path would recompute the exposure from the lens, which is exactly
		// the coupling this project does not want.
		PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		PP.AutoExposureApplyPhysicalCameraExposure = false;

		// No filmic curve. A cel frame is drawn in display values; letting a shoulder roll off the
		// top of the gradient is what turned the sky into a grey wash (R-44).
		// 0.55, not 0.
		//
		// Turning the curve off entirely was a reaction to a filmic shoulder rolling the sky into
		// a grey wash - but it also removed the *highlight shoulder* every photograph has, so
		// bright sand and foam clipped to flat white and the frame read as raw linear output
		// rather than an image. A little curve keeps the shoulder without eating the gradient.
		PP.bOverride_ToneCurveAmount = true;
		PP.ToneCurveAmount = 0.55f;

		// Grade: warm gain, a touch more saturation and contrast than neutral.
		PP.bOverride_ColorSaturation = true;
		// Mild on purpose. The old grade was written to undo the filmic curve's desaturation; with
		// the curve gone it only has to add a little life, and a strong grade would clip the sky.
		PP.ColorSaturation = FVector4(1.18f, 1.14f, 1.08f, 1.0f);
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.14f, 1.12f, 1.08f, 1.0f);
		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(1.0f, 0.99f, 1.02f, 1.0f);
		PP.bOverride_ColorGain = true;
		PP.ColorGain = FVector4(1.05f, 1.00f, 0.94f, 1.0f);

		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.38f;
		// Bloom is for the sun disc and nothing else: with the threshold this high the sky's own
		// near-white never interferes, where a low threshold washed the whole frame in R-44.
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.45f;
		PP.bOverride_BloomThreshold = true;
		PP.BloomThreshold = 1.15f;
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.0f;

		// Shallow band: the far rim and the near foreground go soft, the realm stays sharp. The
		// focal distance itself is driven every tick from the actual camera distance (see Tick), so
		// zooming in does not turn the whole picture into mush.
		PP.bOverride_DepthOfFieldFocalDistance = true;
		PP.DepthOfFieldFocalDistance = 30000.0f;
		PP.bOverride_DepthOfFieldFocalRegion = true;
		PP.DepthOfFieldFocalRegion = 4200.0f;
		PP.bOverride_DepthOfFieldNearTransitionRegion = true;
		PP.DepthOfFieldNearTransitionRegion = 7000.0f;
		PP.bOverride_DepthOfFieldFarTransitionRegion = true;
		PP.DepthOfFieldFarTransitionRegion = 7000.0f;
		PP.bOverride_DepthOfFieldFstop = true;
		PP.DepthOfFieldFstop = 2.8f;
	}

	// ---- water balance, tunable for one run ------------------------------
	//
	// `-SolsticeSea=<diffuse>,<glow>,<shade>`. Any component left at -1 keeps the value the
	// material authors. See the member's comment in the header for why this exists.
	FString SeaSpec;
	// bShouldStopOnSeparator=false: the list *is* comma-separated, and the default behaviour
	// truncates at the first comma (measured: it produced "1.0" and rejected the override).
	if (FParse::Value(FCommandLine::Get(), TEXT("SolsticeSea="), SeaSpec, false))
	{
		TArray<FString> Parts;
		SeaSpec.ParseIntoArray(Parts, TEXT(","), true);
		SeaTuning.Reset();
		for (const FString& Part : Parts)
		{
			SeaTuning.Add(static_cast<float>(FCString::Atof(*Part)));
		}
		if (SeaTuning.Num() < 3)
		{
			UE_LOG(LogSolstice, Warning,
				TEXT("SolsticeSea needs at least three comma-separated numbers; got '%s'"), *SeaSpec);
			SeaTuning.Reset();
		}
		else
		{
			UE_LOG(LogSolstice, Display, TEXT("[diag] sea tuning override %s"), *SeaSpec);
		}
	}

	// ---- light ratio, tunable for one run --------------------------------
	//
	// `-SolsticeEnv=<sun>,<sky>,<exposure>`. Same idea as the sea override: the sun-to-sky ratio
	// is the number that decides whether the frame has a light direction, and guessing it from
	// the two intensities is impossible because they are in different units.
	FString EnvSpec;
	if (FParse::Value(FCommandLine::Get(), TEXT("SolsticeEnv="), EnvSpec, false))
	{
		TArray<FString> Parts;
		EnvSpec.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() == 3)
		{
			EnvTuning = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]),
				FCString::Atof(*Parts[2]));
			UE_LOG(LogSolstice, Display, TEXT("[diag] env tuning override sun=%.3f sky=%.3f exposure=%+.2f"),
				EnvTuning.X, EnvTuning.Y, EnvTuning.Z);
		}
		else
		{
			UE_LOG(LogSolstice, Warning,
				TEXT("SolsticeEnv needs three comma-separated numbers (sun,sky,exposure); got '%s'"),
				*EnvSpec);
		}
	}

	FString CelGainSpec;
	if (FParse::Value(FCommandLine::Get(), TEXT("SolsticeCel="), CelGainSpec, false))
	{
		CelBaseGain = static_cast<float>(FCString::Atof(*CelGainSpec));
		UE_LOG(LogSolstice, Display, TEXT("[diag] cel gain override %.2f"), CelBaseGain);
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
	FCelMaterials::ApplyToSky(SkyMesh);
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

	// Resolve the materials *before* anything asks for a MID. `ApplySky` also calls Ensure, but it
	// runs after BuildStatics, so on the first build every `Get().SeaMID` was still null and the
	// sea silently kept the sky material - a parameter change that produced zero pixel difference.
	FCelMaterials::Ensure();

	// The base is gone.
	//
	// It was a kraft plinth with an ink-dark cut edge - the "torn paper disc" of the old look.
	// Under Lit it turned into a pure black ring around the island, which is exactly the read
	// that was wrong: the land looked pasted onto a coaster instead of rising out of the sea.
	// The terrain sheet already dips below the water line at its rim, so the sea covers the edge
	// on its own; there is nothing left for a plinth to do.
	Scratch.Reset();
	if (BaseMesh)
	{
		BaseMesh->ClearMeshSection(0);
	}

	// ---- the open sea -------------------------------------------------------
	// Section 2 of the base mesh, drawn with the sky material: pure vertex colour, no bands, no
	// occlusion. It starts just under the realm's own water sheet (5 cm, so the two never z-fight)
	// and runs 24 radii out, which is past the frame edge at the landscape framing.
	{
		const float Radius = FMath::Max(1000.0f, GetRealmRadiusUU());
		Scratch.Reset();
		FCelPalette OceanPalette = Palette;
		OceanPalette.WaterDeep = Palette.WaterDeep * OceanScreenGain;
		OceanPalette.WaterMid = Palette.WaterMid * OceanScreenGain;
		OceanPalette.Water = Palette.Water * OceanScreenGain;
		// 0.96 R, not 1.02: at 1.02 the ring started just *outside* the tile water and left a
		// hairline gap at the shoreline that the sky card showed through (diagnosed by painting
		// the card red: a thin red ring appeared around every island).
		// Inner radius is the world's centre, not 0.96 R: the ocean is now the *only* water
		// surface (see the note in Gen::BuildChunk), so it has to reach under the island and
		// cover the sea bed. A 0.02 R hole at the centre is under the island and never visible.
		Gen::BuildOcean(Radius * 0.02f, Radius * OceanRadiusRadii,
			Sim->GetSky().WaterLevel * MeterUU - 5.0f, OceanPalette, OceanHaze, Scratch);
		Scratch.PushTo(BaseMesh, 2, false);
		UE_LOG(LogSolstice, Warning, TEXT("[diag] sea mat: asset=%d mid=%d sky_mid=%d"),
			FCelMaterials::Get().Sea ? 1 : 0,
			FCelMaterials::Get().SeaMID ? 1 : 0,
			FCelMaterials::Get().SkyMID ? 1 : 0);
		// The open sea is *lit*: it is the one surface whose whole read comes from reflected
		// sky and a sun glint. M_Sky would give it the flat vertex colour and nothing else.
		if (BaseMesh)
		{
			if (FCelMaterials::Get().SeaMID)
			{
				BaseMesh->SetMaterial(2, FCelMaterials::Get().SeaMID);
			}
			else if (FCelMaterials::Get().SkyMID)
			{
				BaseMesh->SetMaterial(2, FCelMaterials::Get().SkyMID);
			}
		}
	}

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

void ASolsticeRealm::UpdateSkyView()
{
	// The card's basis is (right, up, forward) rather than Unreal's (forward, right, up), so the
	// painting can be authored in plain screen coordinates: X right, Y up, Z distance. The basis
	// is orthonormal and right-handed, so its rotator is a plain rotation - no mirroring.
	FVector ViewLocation = GetRealmCentre();
	FRotator ViewRotation = FRotator(-35.0f, -45.0f, 0.0f);
	float FovH = 38.0f;
	float Aspect = 16.0f / 9.0f;

	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PlayerCtl = World->GetFirstPlayerController())
		{
			PlayerCtl->GetPlayerViewPoint(ViewLocation, ViewRotation);
			if (PlayerCtl->PlayerCameraManager)
			{
				FovH = PlayerCtl->PlayerCameraManager->GetFOVAngle();
			}
			int32 SizeX = 0;
			int32 SizeY = 0;
			PlayerCtl->GetViewportSize(SizeX, SizeY);
			if (SizeX > 0 && SizeY > 0)
			{
				Aspect = static_cast<float>(SizeX) / static_cast<float>(SizeY);
			}
		}
	}

	const FRotationMatrix Basis(ViewRotation);
	SkyViewLocation = ViewLocation;
	SkyViewRotation = ViewRotation;
	SkyViewForward = ViewRotation.Vector();
	SkyViewRight = Basis.GetUnitAxis(EAxis::Y);
	SkyViewUp = Basis.GetUnitAxis(EAxis::Z);

	SkyHalfTanH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FovH, 10.0f, 120.0f) * 0.5f));
	SkyHalfTanV = SkyHalfTanH / FMath::Max(0.2f, Aspect);

	// Behind the realm at every zoom: the far rim is one radius past the centre, and the extra
	// gap keeps the card inside the sharp part of the depth-of-field band.
	// The card has to sit *behind* the far edge of the sea, or the sea would be occluded by it.
	SkyDistance = FMath::Max(1000.0f,
		FMath::Max(FVector::Dist(ViewLocation, GetRealmCentre()) + GetRealmRadiusUU() * SkyCardGap,
			GetRealmRadiusUU() * OceanRadiusRadii * 1.06f));

	// Parent the card to the camera rather than re-placing it from the view point every tick.
	// The camera manager only publishes its POV on its own tick, so a card placed from it is
	// always one frame behind - harmless while panning slowly, and a visible skew the moment the
	// camera teleports to a preset (the card stays square to the old pitch and the sky renders as
	// a trapezoid). As a child of the camera, the alignment is exact by construction.
	if (!bSkyAttached)
	{
		const UWorld* World = GetWorld();
		APlayerController* PlayerCtl = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PlayerCtl ? PlayerCtl->GetPawn() : nullptr;
		UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
		if (Camera)
		{
			SkyMesh->AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform);
			// The dome is a sphere: it needs no basis rotation, only to sit on the camera. That
			// is also the whole point of the shape - centred on the eye, its equator *is* the
			// horizon, at every pitch and every preset, with nothing to align.
			SkyMesh->SetRelativeRotation(FQuat::Identity);
			bSkyAttached = true;
		}
	}

	if (bSkyAttached)
	{
		// No location offset: the geometry already sits at Z = 1 in card space, and the uniform
		// scale below turns that 1 into the real distance. Adding the distance *as well* placed
		// the card at twice the range and halved it on screen.
		SkyMesh->SetRelativeLocation(FVector::ZeroVector);
		SkyMesh->SetRelativeScale3D(FVector(SkyDistance));
	}
	else
	{
		// No camera yet (headless bench, or the very first frame): place it the manual way.
		SkyMesh->SetWorldLocationAndRotation(ViewLocation, FRotationMatrix::MakeFromXY(SkyViewRight, SkyViewUp).Rotator());
		SkyMesh->SetWorldScale3D(FVector(SkyDistance));
	}
}

void ASolsticeRealm::BuildSkyMesh()
{
	FSimKernel* Sim = Kernel();
	if (!Sim)
	{
		return;
	}

	UpdateSkyView();

	const FSkyState& Sky = Sim->GetSky();
	const FVector Centre = GetRealmCentre();
	const float Radius = GetRealmRadiusUU();

	// ---- where the horizon band goes ------------------------------------
	// Project the world's silhouette into card space and put the bright band on it. A fixed line
	// in the frame would sit behind the terrain at one preset and above it at the next; anchored
	// to the silhouette, the realm is backlit at every framing.
	//
	// Two numbers come out of this and they are *not* the same one:
	//   RimV      - where the disc's cut edge lands. The horizon band sits here; the sky's
	//               gradient must start at the edge of the world, not at the top of a mountain.
	//   SkylineV  - the highest projected piece of terrain. Bodies must clear *this* or the moon
	//               gets drawn with its bottom inside the ridge.
	// Collapsing them into one number (adding the ridge samples to the rim) pushed the horizon
	// band to the top of the card and left one strip of sky above a full-frame island.
	float RimV = -1.0f;
	float SkylineV = -1.0f;
	const auto ProjectV = [&](const FVector& WorldPoint, bool bRim)
	{
		const FVector Rel = WorldPoint - SkyViewLocation;
		const float Depth = FVector::DotProduct(Rel, SkyViewForward);
		if (Depth < 1.0f)
		{
			return;
		}
		const float V = (FVector::DotProduct(Rel, SkyViewUp) / Depth) / FMath::Max(0.05f, SkyHalfTanV);
		SkylineV = FMath::Max(SkylineV, V);
		if (bRim)
		{
			RimV = FMath::Max(RimV, V);
		}
	};
	for (int32 Index = 0; Index < 40; ++Index)
	{
		const float Angle = 2.0f * PI * static_cast<float>(Index) / 40.0f;
		const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		ProjectV(Centre + Radial * Radius, true);											// the cut edge
		// Three ridge heights at two radii. One height at one radius missed the peak between the
		// samples, and the moon was drawn with its bottom edge inside the mountain (2026-10-18).
		ProjectV(Centre + Radial * (Radius * 0.74f) + FVector(0.0f, 0.0f, Radius * 0.42f), false);
		ProjectV(Centre + Radial * (Radius * 0.52f) + FVector(0.0f, 0.0f, Radius * 0.62f), false);
		ProjectV(Centre + Radial * (Radius * 0.30f) + FVector(0.0f, 0.0f, Radius * 0.80f), false);
	}
	// The horizon is where the *sea plane* vanishes, i.e. the camera's own horizontal direction.
	// It used to be the terrain silhouette, which is right for a floating disc in a void and wrong
	// the moment there is an ocean: a hill stands in front of the horizon, it does not define it.
	// With the sea running past the frame, the bright band also has to sit exactly on that line or
	// the far water and the sky meet in a seam.
	const float HorizonV = FMath::Clamp(
		FMath::Tan(FMath::DegreesToRadians(-SkyViewRotation.Pitch)) / FMath::Max(0.05f, SkyHalfTanV), -0.9f, 0.92f);
	SkylineV = FMath::Max(SkylineV, HorizonV);

	// ---- time of day ----------------------------------------------------
	// Three states, blended: daylight, the golden window around the horizon, and night. art.md
	// SS2.4 pins all three; nothing here reads the frame's own brightness (R-41).
	const float Altitude = FMath::Clamp(Sky.SolarAltitudeDeg, -18.0f, 90.0f);
	const float Daylight = FMath::Clamp((Altitude + 4.0f) / 10.0f, 0.0f, 1.0f);
	const float Golden = FMath::Clamp(1.0f - FMath::Abs(Altitude) / 14.0f, 0.0f, 1.0f);
	const float Cloud = FMath::Clamp(Sky.CloudCover, 0.0f, 1.0f);
	const float Wet = FMath::Clamp(Sky.RainIntensity, 0.0f, 1.0f);

	Gen::FSkyPaint Paint;
	// HorizonV is measured against the *screen* half-tan; the card is overscanned, so in card
	// coordinates the horizon sits lower by exactly that factor. Forgetting this division is why
	// the gradient was correct but the card edge showed: the paint and the geometry disagreed
	// about where the frame ended.
	Paint.HorizonV = HorizonV / SkyCardOverscan;
	Paint.VisibleTop = 1.0f / SkyCardOverscan;
	Paint.Zenith = FMath::Lerp(FMath::Lerp(NightZenith, DayZenith, Daylight), DuskZenith, Golden * 0.75f);
	Paint.Middle = FMath::Lerp(FMath::Lerp(NightMiddle, DayMiddle, Daylight), DuskMiddle, Golden * 0.75f);
	Paint.Horizon = FMath::Lerp(FMath::Lerp(NightHorizon, DayHorizon, Daylight), DuskHorizon, Golden * 0.70f);
	Paint.Under = FMath::Lerp(FMath::Lerp(NightUnder, DayUnder, Daylight), DuskUnder, Golden * 0.60f);
	// The card's "below the horizon" strip is only ever seen in the sliver between the sea's far
	// edge and the true horizon, so it must be the far sea's own colour. It used to be its own
	// grey-blue, which is why a band of it read as fog lying on the water.
	Paint.Under = FMath::Lerp(Paint.Under, Paint.Horizon, 0.85f);

	// Clouds take the same hour: gold on top and violet underneath at dusk, white on white at
	// noon, moonlit grey at night. Rain greys them without touching the sky above them.
	Paint.Cirrus = FMath::Lerp(FMath::Lerp(NightMiddle, FLinearColor::White, Daylight), DuskHorizon, Golden * 0.50f);
	Paint.CumulusLit = FMath::Lerp(FMath::Lerp(FLinearColor(0.42f, 0.46f, 0.54f), CelSrgb(0xFF, 0xFC, 0xF5), Daylight),
		DuskHorizon, Golden * 0.60f);
	Paint.CumulusBase = FMath::Lerp(FMath::Lerp(NightMiddle, CelSrgb(0xAE, 0xC2, 0xDA), Daylight),
		DuskZenith, Golden * 0.55f);
	Paint.SunCore = FMath::Lerp(FLinearColor(1.0f, 0.97f, 0.90f), CelSrgb(0xFF, 0xC8, 0x6E), Golden * 0.80f);
	Paint.SunHalo = FMath::Lerp(CelSrgb(0xFF, 0xE6, 0x9E), DuskHorizon, Golden * 0.70f);
	Paint.CloudCover = Cloud;
	Paint.Rain = Wet;
	Paint.StarAlpha = FMath::Clamp((1.0f - Daylight) * (1.0f - Cloud * 0.85f), 0.0f, 1.0f);

	// Chaos: the sky turns to blank paper too. The first colour in a dead world should be the
	// player's, so nothing in the frame may be more interesting than the ground.
	if (Sim->IsChaos())
	{
		const FLinearColor Paper = CelSrgb(0xDF, 0xDB, 0xD2);
		Paint.Zenith = FMath::Lerp(Paint.Zenith, Paper, 0.55f);
		Paint.Middle = FMath::Lerp(Paint.Middle, Paper, 0.55f);
		Paint.Horizon = FMath::Lerp(Paint.Horizon, Paper, 0.45f);
		Paint.Under = FMath::Lerp(Paint.Under, Paper, 0.55f);
		Paint.CloudCover *= 0.35f;
		Paint.StarAlpha = 0.0f;
	}

	// ---- the two bodies --------------------------------------------------
	// Azimuth decides where on the card a body lands, elevation decides how high up, compressed
	// into the part of the frame that is actually sky. Both are held above the silhouette so a
	// disc never hides behind the realm.
	const float AltRad = FMath::DegreesToRadians(Altitude);
	const float AzRad = FMath::DegreesToRadians(Sky.SunAzimuthDeg);
	const FVector SunDir(FMath::Cos(AltRad) * FMath::Cos(AzRad), FMath::Cos(AltRad) * FMath::Sin(AzRad), FMath::Sin(AltRad));
	const FVector FwdFlat = FVector(SkyViewForward.X, SkyViewForward.Y, 0.0f).GetSafeNormal();
	const FVector RightFlat = FVector(SkyViewRight.X, SkyViewRight.Y, 0.0f).GetSafeNormal();

	const float YawLimit = FMath::DegreesToRadians(SkyBodyYawLimitDeg);
	// The card is overscanned past the viewport, so V = 1 is off screen: the visible top edge sits
	// at 1 / SkyCardOverscan. Every "keep it in the frame" clamp below uses this, not 1.0.
	const float VisibleTop = 1.0f / SkyCardOverscan;
	// Radius is in the same tan units as the card, so it converts to a V/U offset directly.
	const auto PlaceBody = [&](const FVector& Direction, float Radius, FVector2D& OutPos, bool& bVisible)
	{
		const FVector Flat = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
		bVisible = false;
		if (Flat.IsNearlyZero() || HorizonV >= VisibleTop - 0.06f)
		{
			return;
		}
		const float YawDelta = FMath::Atan2(FVector::DotProduct(Flat, RightFlat), FVector::DotProduct(Flat, FwdFlat));
		if (FMath::Abs(YawDelta) >= YawLimit)
		{
			return;
		}
		// Leave room for the disc itself at both ends. The old clamp stopped the *centre* at 0.90
		// and 0.10 above the silhouette, which cut bodies in half twice over: a moon high in the
		// frame lost its top to the viewport edge, and one near the horizon lost its bottom to the
		// ridge it was supposed to clear.
		// Leave room for the disc itself at both ends, and never let "clear the terrain" push the
		// centre *above* the frame: at preset 0 the peak projects to ~0.9 of the card, so a body
		// that clears it entirely ends up entirely off screen (2026-10-18 - the sun was drawn past
		// the top edge and all that showed was its cold halo ring).
		const float CardHalfV = SkyHalfTanV * SkyCardOverscan;
		const float CardHalfH = SkyHalfTanH * SkyCardOverscan;
		const float RadiusV = Radius / FMath::Max(0.05f, CardHalfV);
		const float RadiusU = Radius / FMath::Max(0.05f, CardHalfH);
		const float VTop = VisibleTop - 0.02f - RadiusV;
		const float VBot = FMath::Min(SkylineV / SkyCardOverscan + 0.04f + RadiusV, VTop);
		const float U = FMath::Clamp(FMath::Tan(YawDelta) / FMath::Tan(YawLimit), -1.0f, 1.0f)
			* FMath::Max(0.20f, 0.84f - RadiusU);
		const float Alt01 = FMath::Clamp(Direction.Z / FMath::Sin(FMath::DegreesToRadians(70.0f)), 0.0f, 1.0f);
		const float V = FMath::Clamp(FMath::Lerp(VBot, VTop, Alt01), VBot, FMath::Max(VBot, VTop));
		OutPos = FVector2D(U * CardHalfH, V * CardHalfV);
		bVisible = true;
	};

	Paint.SunRadius = FMath::Tan(FMath::DegreesToRadians(SunAngularRadiusDeg));
	PlaceBody(SunDir, Paint.SunRadius, Paint.SunPos, Paint.bSunVisible);
	// A sun below the horizon is not painted; it survives civil twilight, which is when the
	// frame is at its most orange.
	Paint.bSunVisible = Paint.bSunVisible && Altitude > -6.0f;

	// Lunar phase from the world clock: a crescent reads as a small dim disc, because masking it
	// would need a second coplanar card and coplanar cards z-fight.
	const float LunarDay = static_cast<float>(Sim->GetWorldMinute()) / (60.0f * 24.0f);
	const float Phase01 = FMath::Frac(LunarDay / 29.53f);
	const float MoonFull = 0.5f - 0.5f * FMath::Cos(2.0f * PI * Phase01);
	Paint.MoonRadius = FMath::Tan(FMath::DegreesToRadians(MoonAngularRadiusDeg)) * (0.72f + 0.28f * MoonFull);
	PlaceBody(-SunDir, Paint.MoonRadius, Paint.MoonPos, Paint.bMoonVisible);
	Paint.MoonCore = FMath::Lerp(FLinearColor(0.55f, 0.60f, 0.70f), CelSrgb(0xF2, 0xF6, 0xFF), 0.35f + 0.65f * MoonFull);
	Paint.bMoonVisible = Paint.bMoonVisible && (Daylight < 0.65f) && Paint.StarAlpha > 0.05f;

	// Wind drift. Wall clock on purpose: the sky should breathe even while the sim is paused.
	const float WindScale = 0.35f + Sky.WindSpeed * 0.9f;
	Paint.Drift = static_cast<float>(FMath::Frac(FApp::GetCurrentTime() * SkyDriftRate * WindScale));

	// Undo the frame's exposure gain for the sky only.
	//
	// The sky is authored in *display* values, so it must arrive on screen unchanged - but the
	// whole frame is multiplied by the exposure before it is encoded, and the near-white horizon
	// then clips: the gradient flattens into one white field and that field blooms over the blue
	// above it (measured: #F4FFFF at 60% down the sky, against an authored #B8CDEE). Dividing the
	// sky by the same gain puts every value back where it was authored, and nothing clips.
	const float SkyDim = FMath::Pow(0.5f, ExposureStops);
	Paint.Zenith *= SkyDim;
	Paint.Middle *= SkyDim;
	Paint.Horizon *= SkyDim;
	Paint.Under *= SkyDim;
	Paint.Cirrus *= SkyDim;
	Paint.CumulusLit *= SkyDim;
	Paint.CumulusBase *= SkyDim;
	Paint.SunCore *= SkyDim;
	Paint.SunHalo *= SkyDim;
	Paint.MoonCore *= SkyDim;
	Paint.Star *= SkyDim;

	// ---- the dome ---------------------------------------------------------
	//
	// Replaces the painted card. The card was 12% oversized to hide its own edge, which is a
	// workaround for a plane being finite; a sphere centred on the camera has no edge to hide.
	// The geometry is a unit sphere and the component scale turns it into `SkyDistance`, so the
	// shading has to be per-pixel - which is what M_Sky now is.
	Scratch.Reset();
	Gen::BuildSkyDome(128, 64, Scratch);
	Scratch.PushTo(SkyMesh, 0, false);
	SkyMesh->SetMeshSectionVisible(0, true);

	// Everything the dome needs to know. All of it is authored here (FSkyPaint carries the hour),
	// so the material never decides what time it is.
	if (UMaterialInstanceDynamic* SkyMID = FCelMaterials::Get().SkyMID)
	{
		auto SetColour = [SkyMID](const TCHAR* Name, const FLinearColor& Value)
		{
			SkyMID->SetVectorParameterValue(Name, Value);
		};

		SetColour(TEXT("SunDirection"), FLinearColor(SunDir.X, SunDir.Y, SunDir.Z, 0.0f));
		SetColour(TEXT("SkyZenith"), Paint.Zenith);
		SetColour(TEXT("SkyMid"), Paint.Middle);
		SetColour(TEXT("SkyHorizon"), Paint.Horizon);
		SetColour(TEXT("CloudLit"), Paint.CumulusLit);
		SetColour(TEXT("CloudShadow"), Paint.CumulusBase);
		SetColour(TEXT("SunCore"), Paint.SunCore);
		SetColour(TEXT("SunHalo"), Paint.SunHalo);

		// Coverage -> threshold. The noise is centred near 0.47 with a spread around 0.12, so a
		// threshold of 0.58 is a barely-there sky and 0.36 is overcast; mapping the sim's 0..1
		// coverage onto that band keeps "clear" clear and "overcast" actually overcast.
		SkyMID->SetScalarParameterValue(TEXT("CloudThreshold"), 0.55f - 0.24f * Cloud);
		SkyMID->SetScalarParameterValue(TEXT("CloudScale"), 3.2f);
		SkyMID->SetScalarParameterValue(TEXT("SunDiscCos"),
			FMath::Cos(FMath::DegreesToRadians(SunAngularRadiusDeg)));

		// 0.02 units per second times the wind: slow enough to read as drifting, fast enough that
		// a still frame taken a minute later is a different sky.
		SkyMID->SetScalarParameterValue(TEXT("CloudDrift"),
			static_cast<float>(FApp::GetCurrentTime()) * 0.02f * (0.35f + Sky.WindSpeed * 0.9f));

		const FLinearColor MoonColour(SunDir.X * -1.0f, SunDir.Y * -1.0f, SunDir.Z * -1.0f, 0.0f);
		SetColour(TEXT("MoonDirection"), MoonColour);
		SetColour(TEXT("MoonCore"), Paint.MoonCore);
		SkyMID->SetScalarParameterValue(TEXT("MoonDiscCos"),
			FMath::Cos(FMath::DegreesToRadians(MoonAngularRadiusDeg * 0.85f)));
		SkyMID->SetScalarParameterValue(TEXT("MoonAlpha"),
			Paint.bMoonVisible ? FMath::Clamp(1.0f - Daylight, 0.0f, 1.0f) : 0.0f);
	}

	// ---- the sea's two runtime inputs ------------------------------------
	//
	// SkyReflect: the colour the water mirrors at grazing angles. It is the sky's own horizon
	// colour, which is what makes the far sea and the sky meet without a seam - one value, used
	// by both, instead of two gradients tuned to agree.
	//
	// SeaTint: the ocean mesh is built once, so its vertex colours are a photograph of noon.
	// Without this a dusk frame keeps a noon-blue sea under an orange sky.
	if (UMaterialInstanceDynamic* SeaMID = FCelMaterials::Get().SeaMID)
	{
		SeaMID->SetVectorParameterValue(TEXT("SkyReflect"), Paint.Horizon);

		const FLinearColor NightSea(0.20f, 0.26f, 0.42f, 1.0f);
		const FLinearColor DaySea(1.0f, 1.0f, 1.0f, 1.0f);
		const FLinearColor Tint = FMath::Lerp(NightSea, DaySea, Daylight);
		SeaMID->SetVectorParameterValue(TEXT("SeaTint"),
			FLinearColor(Tint.R * SkyDim, Tint.G * SkyDim, Tint.B * SkyDim, 1.0f));

		// One-run override (`-SolsticeSea=`), so the diffuse/emissive split can be searched
		// without a recompile per candidate. Order: diffuse, glow, shade, [foam], [specular],
		// [sky reflection strength]; trailing entries may be omitted.
		static const TCHAR* SeaParamNames[] = {
			TEXT("WaterDiffuse"),
			TEXT("WaterGlow"),
			TEXT("WaveShadeAmount"),
			TEXT("FoamAmount"),
			TEXT("WaterSpecular"),
			TEXT("SkyReflectStrength"),
		};
		for (int32 Index = 0; Index < SeaTuning.Num() && Index < UE_ARRAY_COUNT(SeaParamNames); ++Index)
		{
			if (SeaTuning[Index] >= 0.0f)
			{
				SeaMID->SetScalarParameterValue(SeaParamNames[Index], SeaTuning[Index]);
			}
		}
	}
	// Sections 1 and 2 held the world-space shell and its celestial bodies. The bodies are
	// painted into the card now and there is no shell; stale sections would draw a dome that no
	// longer matches anything.
	SkyMesh->ClearMeshSection(1);
	SkyMesh->ClearMeshSection(2);
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
			HullScratch.Reset();
			Gen::BuildChunk(*Sim, ChunkX, ChunkY, Palette, Scratch, HullScratch);
			Scratch.PushTo(ChunkMeshes[Index], 0, true);
			// Section 2 is the *one-sided* ink material: the inverted hull of every object in this
			// chunk. One-sided is the whole trick - with the two-sided ink the enlarged front faces
			// cover the object instead of ringing it. No collision: it exists to be a silhouette.
			HullScratch.PushTo(ChunkMeshes[Index], 2, false);
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

	// ---- the sun's colour has to follow its height ---------------------------------
	//
	// It used to be a constant warm white set once in the constructor. A dusk frame therefore
	// had a sun sitting 1.4 degrees above the horizon and *still* lighting the island like noon -
	// the whole reason evening looked like a purple filter over a midday photo. Light at low
	// angles passes through far more atmosphere, so it reddens and dims; that curve is most of
	// what makes a golden hour read as golden.
	const float SunHeight01 = FMath::Clamp((Sky.SolarAltitudeDeg + 4.0f) / 26.0f, 0.0f, 1.0f);
	const FLinearColor HorizonSun(1.0f, 0.42f, 0.18f);	// deep orange at the horizon
	const FLinearColor HighSun(1.0f, 0.97f, 0.92f);		// near-white overhead
	SunLight->SetLightColor(FMath::Lerp(HorizonSun, HighSun, FMath::Pow(SunHeight01, 0.65f)));

	// Ambient follows too: a low sun means less sky light in the frame, and it must not be
	// allowed to keep the ground at midday brightness on its own.
	const float Ambient01 = FMath::Clamp((Sky.SolarAltitudeDeg + 6.0f) / 22.0f, 0.06f, 1.0f);
			const float SkyScale = EnvTuning.Y >= 0.0 ? static_cast<float>(EnvTuning.Y) : 1.0f;
	SkyLight->SetIntensity(SkyLightIntensity * Ambient01 * SkyScale);

	if (CelBaseGain >= 0.0f)
	{
		if (UMaterialInstanceDynamic* CelMID = FCelMaterials::Get().CelMID)
		{
			CelMID->SetScalarParameterValue(TEXT("SurfaceBaseGain"), CelBaseGain);
		}
	}
	SkyLight->SetLightColor(FMath::Lerp(
		FLinearColor(0.42f, 0.50f, 0.72f),	// cold violet-blue at dusk
		FLinearColor(0.86f, 0.90f, 1.00f),	// neutral daylight
		Ambient01));
	// Daylight ramp only - the *colour* and the band direction are pushed to the material by
	// ApplySky, so this scalar never has to encode a look.
	//
	// Do not try to fix a dark hour by adding lights here. M_Cel is unlit (see its builder): the
	// ground is shaded by LitTint/MidTint/ShadowTint plus a band threshold on dot(normal, sun),
	// so light components cannot reach it at all. A fill light was added on 2026-10-18 for a dark
	// dusk frame and changed exactly zero pixels - the dusk ramp lives in FCelMaterials::ApplySky.
	SunLight->SetIntensity(
		FMath::Lerp(0.15f, 1.0f, FMath::Clamp((Sky.SolarAltitudeDeg + 8.0f) / 18.0f, 0.0f, 1.0f)) * SunLightIntensity
		* (EnvTuning.X >= 0.0 ? static_cast<float>(EnvTuning.X) : 1.0f));
	const float Wet = FMath::Clamp(Sky.RainIntensity * 0.7f + Sky.CloudCover * 0.3f, 0.0f, 1.0f);
	// The void outside the realm is pinned deep blue (WORLD SS1.1, #0B1431) in every weather -
	// weather only pushes it slightly towards grey-blue, it never returns to pale fog grey.
	// This lerp used to stomp the build-time colour back to (0.70, 0.73, 0.78), which is why the
	// surround stayed grey no matter what the constructor set.
	// The fog colour is the *horizon*, not the void.
	//
	// It used to be the deep-blue "nothing outside the realm" tone. That was correct when the
	// world was a disc floating in empty space; now that the sea runs to the horizon, a fog of
	// that colour painted the far water grey and the frame got a band lying across it at eye
	// level. Fog is aerial perspective: it must be the colour of the air the sea disappears into.
	const FLinearColor ClearVoid = Solstice::CelSrgb(0xCF, 0xE6, 0xF2);
	const FLinearColor WetVoid = Solstice::CelSrgb(0xB4, 0xC6, 0xD4);
	Fog->SetFogInscatteringColor(FMath::Lerp(ClearVoid, WetVoid, Wet));

	// Density keeps the fit-distance calibration from BuildStatics; weather deepens the fog
	// towards the wet depth on top of it.
	const float Radius = FMath::Max(1000.0f, GetRealmRadiusUU());
	const float FitDistance = Radius * 5.6f;
	Fog->SetFogDensity(FMath::Lerp(ClearFogDepth, WetFogDepth, Wet) / FitDistance);
	Fog->SetStartDistance(FitDistance * 0.12f);

	// Throttled: this used to print every tick, which buried the log it was meant to help read.
	// Post-increment so the *first* call prints: an offscreen capture at 1280x720 runs at a few
	// frames per second, so "every 120 ticks" never fired inside a six-second shot and the
	// diagnostic that was meant to explain a missing shadow never appeared.
	static int32 DiagCalls = 0;
	if ((DiagCalls++ % 120) == 0)
	{
		const FLinearColor FogNow = Fog->FogInscatteringLuminance;
		UE_LOG(LogSolstice, Log,
			TEXT("[diag] sun: alt=%.1f intensity=%.2f fog=(%.3f,%.3f,%.3f) sunDir=(%.2f,%.2f,%.2f)"),
			Sky.SolarAltitudeDeg, SunLight->Intensity, FogNow.R, FogNow.G, FogNow.B,
			SunDir.X, SunDir.Y, SunDir.Z);

		// Shadow plumbing. "The shadow is missing" has exactly four places to be wrong - the
		// light does not cast, the mesh does not cast, the cascade range does not reach the
		// subject, or the material is emissive and has nothing to shade - and printing all four
		// costs less than one guess-and-rebuild cycle.
		const float CamToCentre = [&]() -> float
		{
			if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				if (const APlayerCameraManager* Cam = PC->PlayerCameraManager)
				{
					return FVector::Dist(Cam->GetCameraLocation(), GetRealmCentre());
				}
			}
			return -1.0f;
		}();
		UE_LOG(LogSolstice, Log,
			TEXT("[diag] shadow: light_cast=%d light_dyn=%d range=%.0f cascades=%d ")
			TEXT("chunk_cast=%d prop_cast=%d cam_to_centre=%.0f"),
			SunLight->CastShadows ? 1 : 0, SunLight->CastDynamicShadows ? 1 : 0,
			SunLight->DynamicShadowDistanceMovableLight, SunLight->DynamicShadowCascades,
			(ChunkMeshes.Num() > 0 && ChunkMeshes[0]) ? (ChunkMeshes[0]->CastShadow ? 1 : 0) : -1,
			PropMesh ? (PropMesh->CastShadow ? 1 : 0) : -1, CamToCentre);
	}
}

void ASolsticeRealm::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The sky card is re-placed every frame - the camera moves continuously when the player
	// zooms - but it is only *repainted* when its content changes (see the signature gate below).
	UpdateSkyView();

	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PlayerCtl = World->GetFirstPlayerController())
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			PlayerCtl->GetPlayerViewPoint(ViewLocation, ViewRotation);
			const float ViewDistance = FVector::Dist(ViewLocation, GetRealmCentre());

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
		// The card is painted in screen space, so the camera is part of its content: half a degree
		// of yaw or pitch repaints it. The drift term repaints about twice a second, which at the
		// spec's 0.5-2 deg/s is a slide of a pixel or two per step.
		Signature = Mix32(Signature, static_cast<uint32>(FMath::RoundToInt(SkyViewRotation.Yaw * 2.0) + 4096));
		Signature = Mix32(Signature, static_cast<uint32>(FMath::RoundToInt(SkyViewRotation.Pitch * 2.0) + 4096));
		Signature = Mix32(Signature, static_cast<uint32>(FMath::Frac(FApp::GetCurrentTime() * SkyDriftRate) * 96.0));
		// The horizon band is anchored to where the realm projects, so a zoom step moves it.
		Signature = Mix32(Signature, static_cast<uint32>(SkyDistance / 500.0f));

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
