// Solstice - editor module: the master materials are built from nodes in code.
//
// Nothing here is authored in a .uasset by hand: a fresh clone runs this generator and gets a
// byte-identical shader. Five materials come out of it.
//
//   M_Cel      Lit. Vertex colour is the albedo; light does the rest. The palette is authored
//              as albedo, so what you write in Gen::TileColour is what the sun multiplies.
//              (It used to be Unlit with three painted bands. That looked flat no matter what
//              the palette said, because an unlit surface cannot receive a shadow.)
//   M_Sea      Lit, mirror-ish. Vertex colour + a procedural four-wave normal, so the sky
//              reflection breaks into glints instead of one flat sheet of blue.
//   M_Sky      Unlit. A matte painting: the horizon must arrive exactly as authored.
//   M_Ink      Unlit, two-sided. The ink cards under the standees.
//   M_InkHull  Unlit, one-sided. The inverted hulls; only their back faces may survive.
//
// Every material carries a `RecipeVersion` scalar. BuildAll compares it against CelRecipeVersion
// and rebuilds when it differs, so a graph change never leaves an old asset silently in place.

#include "SolsticeEditor.h"
#include "Templates/Function.h"

#include "Solstice.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStep.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	/**
	 * Bumped whenever the graph below changes shape. The generated asset carries the number in a
	 * scalar parameter, and BuildAll rebuilds any asset whose stamp does not match - otherwise a
	 * checkout that already has an M_Cel keeps rendering the old graph forever.
	 */
	constexpr int32 CelRecipeVersion = 149;

	UMaterial* FindOrCreateMaterial(const FString& PackageName)
	{
		if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName))))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}
		UMaterial* Material = NewObject<UMaterial>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone);
		if (Material)
		{
			FAssetRegistryModule::AssetCreated(Material);
		}
		return Material;
	}

	template <typename T>
	T* AddExpr(UMaterial* Material, int32 X, int32 Y)
	{
		T* Expr = NewObject<T>(Material);
		Expr->MaterialExpressionEditorX = X;
		Expr->MaterialExpressionEditorY = Y;
		Material->GetExpressionCollection().AddExpression(Expr);
		return Expr;
	}

	/**
	 * Drop the graph *and* every output connection.
	 *
	 * `GetExpressionCollection().Empty()` removes the nodes and leaves every
	 * `UMaterialEditorOnlyData` input still pointing at one of them, so a rebuild inherits the
	 * previous run's wiring for any output it does not explicitly set. This is not theoretical:
	 * after the unlit -> lit rewrite of M_Cel the asset reported `emissive=1` while the builder
	 * only ever wrote BaseColor, the vertex colour was therefore both the albedo *and* the
	 * emission, and the frame was the raw palette - the sun could be switched off entirely and
	 * not one pixel changed (measured 2026-10-18, sun=0 sky=0 vs sun=14 sky=0.10: mean island
	 * luminance 143.8 against 145.7).
	 *
	 * A material whose outputs still hold old links is worse than a stale asset: it *looks* lit
	 * and responds to nothing, which is indistinguishable from a lighting bug.
	 */
	void ResetGraph(UMaterial* Material)
	{
		if (!Material)
		{
			return;
		}
		Material->GetExpressionCollection().Empty();

		UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
		if (!EditorData)
		{
			return;
		}
		// Matched by struct name rather than by inheritance: these structs are declared
		// `USTRUCT(noexport)` and hand-mirrored in MaterialShared.h, so IsChildOf is not reliable.
		static const FName InputStructNames[] = {
			FName(TEXT("ExpressionInput")),
			FName(TEXT("ColorMaterialInput")),
			FName(TEXT("ScalarMaterialInput")),
			FName(TEXT("VectorMaterialInput")),
			FName(TEXT("Vector2MaterialInput")),
			FName(TEXT("MaterialAttributesInput")),
		};
		for (TFieldIterator<FStructProperty> It(UMaterialEditorOnlyData::StaticClass()); It; ++It)
		{
			FStructProperty* Property = *It;
			if (!Property->Struct)
			{
				continue;
			}
			const FName StructName = Property->Struct->GetFName();
			bool bIsInput = false;
			for (const FName& Candidate : InputStructNames)
			{
				if (StructName == Candidate)
				{
					bIsInput = true;
					break;
				}
			}
			if (!bIsInput)
			{
				continue;
			}
			// The four input structs all begin with FExpressionInput's members.
			FExpressionInput* Input = Property->ContainerPtrToValuePtr<FExpressionInput>(EditorData);
			Input->Expression = nullptr;
			Input->OutputIndex = 0;
		}
	}

	bool SaveAsset(UObject* Asset, const FString& PackageName)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();

		const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, nullptr, *FileName, Args);
	}

	bool BuildCel()
	{
		const FString PackageName = TEXT("/Game/Core/M_Cel");
		UMaterial* Material = FindOrCreateMaterial(PackageName);
		if (!Material)
		{
			return false;
		}

		Material->MaterialDomain = MD_Surface;
		Material->BlendMode = BLEND_Opaque;
		// ★ B0：Lit，不是 Unlit。
		//
		// 这是整个 REBOOT 的地基。Unlit 表面收不到方向光的阴影、天光的漫射、SSAO 的
		// 环境遮挡、SSR 的反射 —— 所以无论色板怎么调，世界永远像一张贴纸。
		// 改成 DefaultLit 之后，明暗第一次由「光」而不是「顶点色里的常量」决定。
		//
		// 卡通感没有被丢掉：它现在由 B1 的后处理色阶（ToonRamp）承担，而不是由
		// 顶点色伪造。两者只能选一个，而伪造的这个拿不到阴影。
		Material->SetShadingModel(MSM_DefaultLit);
		// Two-sided: the cut edge and the hull cards are seen from both sides.
		Material->TwoSided = true;

		ResetGraph(Material);

		// ---- albedo -------------------------------------------------------
		// 顶点色从「最终颜色」降级为「albedo」。生成器写进去的本来就是纯色（`TileColour`
		// 不带明暗），所以这一步不改任何一个色值，只改它的语义。
		UMaterialExpressionVertexColor* VertexColour = AddExpr<UMaterialExpressionVertexColor>(Material, -600, -60);

		UMaterialExpressionScalarParameter* Roughness = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 180);
		Roughness->ParameterName = TEXT("SurfaceRoughness");
		Roughness->DefaultValue = 0.88f;

		UMaterialExpressionScalarParameter* Specular = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 300);
		Specular->ParameterName = TEXT("SurfaceSpecular");
		Specular->DefaultValue = 0.10f;

		UMaterialExpressionScalarParameter* Recipe = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 420);
		Recipe->ParameterName = TEXT("RecipeVersion");
		Recipe->DefaultValue = static_cast<float>(CelRecipeVersion);

		// A live gain on the ground's albedo.
		//
		// The island's brightness is the product of three things that are all set far away from
		// it - the palette, the sun intensity, and the exposure - so "the grass is too dark" has
		// no single place to fix and every candidate costs a rebuild. This parameter lets one run
		// answer which of the three is wrong: sweep it, and if the ground brightens the albedo is
		// in range and the light is the problem; if nothing changes, the surface is not being lit
		// at all and the search is in the wrong place entirely.
		UMaterialExpressionScalarParameter* BaseGain = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 480);
		BaseGain->ParameterName = TEXT("SurfaceBaseGain");
		BaseGain->DefaultValue = 1.0f;

		UMaterialExpressionMultiply* TintedBase = AddExpr<UMaterialExpressionMultiply>(Material, -400, -60);
		TintedBase->A.Expression = VertexColour;
		TintedBase->B.Expression = BaseGain;

		// ---- ground detail -------------------------------------------------
		//
		// The generator writes one flat colour per tile: meadow is #7FBF5F everywhere, and a
		// hundred square metres of one green is what makes the island read as a plastic model
		// rather than as land. Nothing in the pipeline had any *surface* - no grain, no wear, no
		// change of soil - so the eye got a single hue and concluded "low-poly toy".
		//
		// Two overlapping noise fields, evaluated in world space so the pattern does not swim
		// when geometry is rebuilt, give each patch its own slightly different grass: a broad
		// one (about 50 m) that says "this hillside faces the weather" and a finer one (about
		// 6 m) that breaks the silhouette of a single slope. It multiplies the albedo, so it
		// survives any lighting change and costs one extra shader pair.
		UMaterialExpressionWorldPosition* DetailPos = AddExpr<UMaterialExpressionWorldPosition>(Material, -1200, 640);
		// Distance is the only reliable anti-aliasing input in this material - `ddx` was measured
		// to come back unusable (see the note in the sea's WaveNormal).
		UMaterialExpressionCameraPositionWS* DetailCam = AddExpr<UMaterialExpressionCameraPositionWS>(Material, -1400, 520);

		UMaterialExpressionCustom* GroundDetail = AddExpr<UMaterialExpressionCustom>(Material, -900, 640);
		GroundDetail->Description = TEXT("GroundDetail");
		GroundDetail->OutputType = CMOT_Float3;
		GroundDetail->Code = TEXT(R"(
// ---- anti-aliasing ----
//
// This noise is 0.55 m across. From the survey camera one pixel covers about 43 uu (0.43 m),
// i.e. more than one lattice cell, so the field is sampled far below its Nyquist limit and
// folds down into huge moiré swirls - the entire sea in that shot was marble. The sea there is
// not M_Sea at all: the survey framing covers 24000 uu and the world is 12800 uu across, so
// what fills the frame is these tiles.
//
// Same remedy as the water shader, and the same lesson: `ddx` measured no effect here either,
// so the input is camera distance. The entry framing sits 160 m from the island and keeps all
// of it; past 140 m the fine pattern fades out before it can alias into a checkerboard. 140 and
// not 180 because the survey camera sits at 260 m: at 180 the fade had only reached 67% there
// and the checkerboard was still legible.
const float DetailCamDist = length(P - CamPos.xyz);
const float DetailFade = 1.0 - saturate((DetailCamDist - 14000.0) / 14000.0);

float2 p = P.xy * 0.018;
float n = 0.0;
float a = 0.55;
for (int i = 0; i < 3; i++)
{
    float2 c = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);

    float3 h0 = frac(float3(c.x,        c.y,        c.x) * 0.1031);
    float3 h1 = frac(float3(c.x + 1.0,  c.y,        c.x + 1.0) * 0.1031);
    float3 h2 = frac(float3(c.x,        c.y + 1.0,  c.x) * 0.1031);
    float3 h3 = frac(float3(c.x + 1.0,  c.y + 1.0,  c.x + 1.0) * 0.1031);
    h0 += dot(h0, h0.yzx + 33.33);
    h1 += dot(h1, h1.yzx + 33.33);
    h2 += dot(h2, h2.yzx + 33.33);
    h3 += dot(h3, h3.yzx + 33.33);

    float v0 = frac((h0.x + h0.y) * h0.z);
    float v1 = frac((h1.x + h1.y) * h1.z);
    float v2 = frac((h2.x + h2.y) * h2.z);
    float v3 = frac((h3.x + h3.y) * h3.z);

    n += a * lerp(lerp(v0, v1, f.x), lerp(v2, v3, f.x), f.y);
    p = float2(p.x * 1.71 - p.y * 1.31, p.x * 1.31 + p.y * 1.71);
    a *= 0.5;
}
n = saturate((n - 0.5) * 1.9 + 0.5);

// Warm/dry on one side of the field, cool/damp on the other - soil and grass, not a filter.
float3 Dry  = float3(1.13, 1.04, 0.86);
float3 Damp = float3(0.86, 0.98, 0.93);
return lerp(float3(1.0, 1.0, 1.0), lerp(Damp, Dry, n), DetailFade);
)");
		{
			FCustomInput Input;
			Input.InputName = TEXT("P");
			Input.Input.Expression = DetailPos;
			GroundDetail->Inputs.Add(Input);
			FCustomInput InCam;
			InCam.InputName = TEXT("CamPos");
			InCam.Input.Expression = DetailCam;
			GroundDetail->Inputs.Add(InCam);
		}

		// ---- tiled detail textures -----------------------------------------
		//
		// Everything above paints one flat colour per tile. This is the layer that turns that
		// into a surface: three tileable albedo maps (grass / rock / sand) sampled in world
		// space and selected by what the vertex colour already says the ground is.
		//
		// World-space UVs, not the mesh's: the terrain writes 0..1 into every quad, so the mesh
		// UV would repeat the texture once per metre. Four metres per tile is the scale at which
		// the entry framing reads grass as grass rather than as noise.
		UMaterialExpressionScalarParameter* TileSize = AddExpr<UMaterialExpressionScalarParameter>(Material, -1600, 760);
		TileSize->ParameterName = TEXT("GroundTileScale");
		TileSize->DefaultValue = 0.0025f;	// 1/400 uu = one tile per 4 m

		UMaterialExpressionMultiply* TileScale = AddExpr<UMaterialExpressionMultiply>(Material, -1400, 700);
		TileScale->A.Expression = DetailPos;
		TileScale->B.Expression = TileSize;

		// Mask down to RG. A texture sample takes float2 and will *not* narrow a float3 for you:
		// feeding the world position straight in fails the whole material with
		// "Cannot cast from larger type float3 to smaller type float2", the asset falls back to
		// the engine default material, and the island renders as a white cut-out that looks for
		// all the world like a texture problem rather than a graph problem.
		UMaterialExpressionComponentMask* TileUV = AddExpr<UMaterialExpressionComponentMask>(Material, -1250, 700);
		TileUV->Input.Expression = TileScale;
		TileUV->R = true;
		TileUV->G = true;
		TileUV->B = false;
		TileUV->A = false;

		UTexture2D* White = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		auto SampleTex = [&](const TCHAR* Parameter, int32 X, int32 Y)
		{
			UMaterialExpressionTextureSampleParameter2D* Node =
				AddExpr<UMaterialExpressionTextureSampleParameter2D>(Material, X, Y);
			Node->ParameterName = Parameter;
			Node->SamplerType = SAMPLERTYPE_Color;
			Node->Texture = White;
			Node->Coordinates.Expression = TileUV;
			return Node;
		};

		UMaterialExpressionTextureSampleParameter2D* GrassTex = SampleTex(TEXT("GroundAlbedo"), -1200, 600);
		UMaterialExpressionTextureSampleParameter2D* RockTex = SampleTex(TEXT("RockAlbedo"), -1200, 760);
		UMaterialExpressionTextureSampleParameter2D* SandTex = SampleTex(TEXT("SandAlbedo"), -1200, 920);

		UMaterialExpressionCustom* DetailBlend = AddExpr<UMaterialExpressionCustom>(Material, -900, 760);
		DetailBlend->Description = TEXT("DetailBlend");
		DetailBlend->OutputType = CMOT_Float3;
		DetailBlend->Code = TEXT(R"(
// Pick the map the vertex colour already voted for, then use it as brightness detail.
//
// The selection is a *classification*, not a blend weight: meadow green is grass, the
// straw band is sand, everything else is rock. Blending all three everywhere would wash
// the three surfaces into one grey average, which is the state this is meant to fix.
// Custom nodes are separate function bodies: a local defined in GroundDetail does not exist
// here. This cost one full rebuild - the first attempt referenced `DetailFade` from the other
// node, M_Cel failed to compile, fell back to DefaultMaterial, and the island disappeared.
const float BlendCamDist = length(VW - CamPos.xyz);
const float BlendFade = 1.0 - saturate((BlendCamDist - 14000.0) / 14000.0);

float3 C = V.rgb;
float green = saturate((C.g - max(C.r, C.b)) * 6.0);
float straw = saturate((min(C.r, C.g) - C.b) * 5.0) * (1.0 - green);
float stone = saturate(1.0 - green - straw);

float3 detail = G.rgb * green + S.rgb * straw + R.rgb * stone;
float lum = dot(detail, float3(0.299, 0.587, 0.114));

// Tree bark and anything else this dark is not terrain and must not be re-lit by a ground
// map: the classification above calls a brown trunk "stone", pulls the rock map's brightness
// over it, and a 0.03-linear albedo turns into a white stick under a green crown.
float peak = max(max(C.r, C.g), C.b);
float passthrough = step(peak, 0.055);

// Neutral 0.52, and the divisor keeps the *mean* of the map at 1.0 so this never becomes an
// exposure change that has to be re-tuned against the sun.
// 0.20, not 0.52: the maps are sampled with sRGB on, so they arrive *linear* - a mid-green
// authored at 0.50 sRGB is 0.21 here. Dividing by the sRGB number would darken the whole
// island by half and the search would start all over again on the exposure.
float mul = clamp(lum / 0.20, 0.45, 1.90);
mul = lerp(mul, 1.0, passthrough);
// Fade the *texture* too, not just the noise: the 4 m checkerboard seen from a survey camera is
// this sample repeating, and it is the half of the pattern that survived every other attempt.
mul = lerp(1.0, mul, BlendFade);
return float3(mul, mul, mul);
)");
		{
			auto Add = [&DetailBlend](const TCHAR* Name, UMaterialExpression* Expression)
			{
				FCustomInput Input;
				Input.InputName = Name;
				Input.Input.Expression = Expression;
				DetailBlend->Inputs.Add(Input);
			};
			Add(TEXT("G"), GrassTex);
			Add(TEXT("R"), RockTex);
			Add(TEXT("S"), SandTex);
			Add(TEXT("V"), VertexColour);
			Add(TEXT("VW"), DetailPos);
			Add(TEXT("CamPos"), DetailCam);
		}

		UMaterialExpressionMultiply* DetailedBase = AddExpr<UMaterialExpressionMultiply>(Material, -200, -60);
		DetailedBase->A.Expression = TintedBase;
		DetailedBase->B.Expression = DetailBlend;

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			// ---- night desaturation --------------------------------------
			//
			// Under moonlight the island's foliage stayed *green* - saturated, almost luminous,
			// like moss lit by a UV lamp. The cause is multiplicative: a lit tint of
			// (0.40, 0.50, 0.80) multiplied by leaf green (0.02, 0.09, 0.03) still leaves green
			// dominant (0.008, 0.045, 0.024), so no amount of cooling the light removes the hue.
			//
			// Real night vision is scotopic: the cones stop contributing and the world goes to
			// luminance with a slight blue cast. That is a change to the *surface*, not the
			// light, so it belongs here, driven by a scalar the hour controls.
			UMaterialExpressionScalarParameter* NightDesat = AddExpr<UMaterialExpressionScalarParameter>(Material, -420, 120);
			NightDesat->ParameterName = TEXT("NightDesat");
			NightDesat->DefaultValue = 0.0f;

			UMaterialExpressionCustom* DesatNode = AddExpr<UMaterialExpressionCustom>(Material, -220, 120);
			DesatNode->Description = TEXT("NightDesat");
			DesatNode->OutputType = CMOT_Float3;
			DesatNode->Code = TEXT(R"(
float l = dot(C, float3(0.299, 0.587, 0.114));
// Value AND hue. Desaturating alone turned the canopy into pale grey - the foliage's lit band
// is bright enough that a neutral version of it reads as snow at night. Scotopic vision loses
// roughly two stops of sensitivity along with the cones, so the grey has to be darker than the
// colour it replaces, not merely colourless.
float3 night = float3(l, l, l) * float3(0.60, 0.66, 0.84);
return lerp(C, night, saturate(Desat));
)");
			{
				FCustomInput InColour;
				InColour.InputName = TEXT("C");
				InColour.Input.Expression = DetailedBase;
				DesatNode->Inputs.Add(InColour);
				FCustomInput InDesat;
				InDesat.InputName = TEXT("Desat");
				InDesat.Input.Expression = NightDesat;
				DesatNode->Inputs.Add(InDesat);
			}

			// ---- three-band toon shading ------------------------------------
			//
			// The look this project has been documented as having since the start, and the
			// material has never actually had it.
			//
			// art.md SS2.1 specifies three bands - shadow / mid / lit - cut hard on
			// dot(normal, sun), with authored colours per band rather than a physical response.
			// SolsticeCelMaterial has been pushing LitTint, MidTint, ShadowTint, BandLow and
			// BandHigh into this material every frame since it was written. **None of those five
			// parameters exists in M_Cel.** They were lost in the rewrite that turned the material
			// from unlit into DefaultLit, and a material instance ignores a parameter it does not
			// declare without a word, so nothing anywhere reported it.
			//
			// The consequence is the whole complaint: the island has been plain physically-lit
			// geometry. Sunlit faces get 100 units of sun, shaded faces get nothing, and a third
			// of the subject renders below 0.12 luminance. No palette change can fix that.
			//
			// Two halves:
			//   1. BaseColor is multiplied by the band tint, so the albedo itself carries which
			//      side of the model is lit.
			//   2. The *shadow band only* gets an emissive floor. Without it step 1 can only
			//      darken: the physical sun still hands a shaded face a zero to multiply.
			UMaterialExpressionVertexNormalWS* ToonNormal = AddExpr<UMaterialExpressionVertexNormalWS>(Material, -900, 340);

			auto VecParam = [&](const TCHAR* Name, const FLinearColor& Value, int32 X, int32 Y)
			{
				UMaterialExpressionVectorParameter* Node = AddExpr<UMaterialExpressionVectorParameter>(Material, X, Y);
				Node->ParameterName = Name;
				Node->DefaultValue = Value;
				return Node;
			};
			auto ScaParam = [&](const TCHAR* Name, float Value, int32 X, int32 Y)
			{
				UMaterialExpressionScalarParameter* Node = AddExpr<UMaterialExpressionScalarParameter>(Material, X, Y);
				Node->ParameterName = Name;
				Node->DefaultValue = Value;
				return Node;
			};

			UMaterialExpressionVectorParameter* SunDirParam =
				VecParam(TEXT("SunDirection"), FLinearColor(0.0f, 0.0f, 1.0f, 0.0f), -900, 440);
			UMaterialExpressionVectorParameter* LitTintParam =
				VecParam(TEXT("LitTint"), FLinearColor(1.0f, 0.98f, 0.94f, 1.0f), -900, 560);
			UMaterialExpressionVectorParameter* MidTintParam =
				VecParam(TEXT("MidTint"), FLinearColor(0.72f, 0.75f, 0.86f, 1.0f), -900, 680);
			UMaterialExpressionVectorParameter* ShadowTintParam =
				VecParam(TEXT("ShadowTint"), FLinearColor(0.56f, 0.66f, 0.78f, 1.0f), -900, 800);
			UMaterialExpressionScalarParameter* BandLowParam =
				ScaParam(TEXT("BandLow"), 0.30f, -900, 920);
			UMaterialExpressionScalarParameter* BandHighParam =
				ScaParam(TEXT("BandHigh"), 0.62f, -900, 1000);
			// 1.45, after measurement. The renderer's own lights land a couple of percent of the
			// picture's range on the island, so this scalar *is* the sun: `albedo * litTint * Fill
			// * 0.406` (the exposure gain) has to put the meadow's linear green near 0.33, which
			// is the 155/255 that reads as sunlit grass against a 0.22-luminance sky. 2.4
			// overshoots into neon, 1.0 leaves it at the 90 it sat at for three rounds.
			UMaterialExpressionScalarParameter* CelFillParam =
				ScaParam(TEXT("CelFill"), 1.45f, -900, 1080);
			// How far the surface detail is allowed to move the band edge.
			//
			// Without it every band boundary is a perfectly smooth curve following the vertex
			// normals, so a 64 m hillside is four flat colour fields no matter how good the
			// albedo texture is - the "flat / low-poly" read is the *shading* being smooth, not
			// the mesh being coarse. 0.05 of a cosine is about 3 degrees of tilt: enough to
			// roughen the edge, far too little to turn the bands into a gradient.
			UMaterialExpressionScalarParameter* BandBumpParam =
				ScaParam(TEXT("BandBump"), 0.05f, -900, 1140);

			UMaterialExpressionCustom* ToonBand = AddExpr<UMaterialExpressionCustom>(Material, -620, 560);
			ToonBand->Description = TEXT("ToonBand");
			ToonBand->OutputType = CMOT_Float3;
			ToonBand->Code = TEXT(R"(
// Hard-edged bands on the incidence angle: below BandLow the shadow colour, above BandHigh the
// lit colour, and a narrow mid band between. A soft ramp here is what makes "cel-shaded" renders
// look like ordinary PBR with the saturation turned down.
float ndl = dot(normalize(N), normalize(S));
// Surface detail nudges the band edge. `Detail` is the same albedo detail the base colour
// uses, so the band boundary and the texture describe the same surface.
float det = dot(Detail, float3(0.299, 0.587, 0.114));
ndl += (det - 0.5) * Bump;
float lo = smoothstep(BandLow - 0.05, BandLow + 0.05, ndl);
float hi = smoothstep(BandHigh - 0.05, BandHigh + 0.05, ndl);
return lerp(lerp(Shadow, Mid, lo), Lit, hi);
)");
			{
				auto Add = [&ToonBand](const TCHAR* Name, UMaterialExpression* Expr)
				{
					FCustomInput In;
					In.InputName = Name;
					In.Input.Expression = Expr;
					ToonBand->Inputs.Add(In);
				};
				Add(TEXT("N"), ToonNormal);
				Add(TEXT("S"), SunDirParam);
				Add(TEXT("Detail"), DetailBlend);
				Add(TEXT("Bump"), BandBumpParam);
				Add(TEXT("Lit"), LitTintParam);
				Add(TEXT("Mid"), MidTintParam);
				Add(TEXT("Shadow"), ShadowTintParam);
				Add(TEXT("BandLow"), BandLowParam);
				Add(TEXT("BandHigh"), BandHighParam);
			}

			UMaterialExpressionMultiply* ToonBase = AddExpr<UMaterialExpressionMultiply>(Material, -330, 120);
			ToonBase->A.Expression = DesatNode;
			ToonBase->B.Expression = ToonBand;

			UMaterialExpressionCustom* ShadowFillNode = AddExpr<UMaterialExpressionCustom>(Material, -330, 300);
			ShadowFillNode->Description = TEXT("ShadowFill");
			ShadowFillNode->OutputType = CMOT_Float3;
			ShadowFillNode->Code = TEXT(R"(
// `hi` is the same lit factor the band uses, so the fill vanishes exactly where the sun takes
// over and there is no double count at the edge.
//
// `(1 - hi)` was removed from this line, and that is the whole of the "the island does not
// respond to the sun" result.
//
// Everything above still describes the *intent*. What the frame does is different: the world's
// only lights are a 100-unit sun and a sky light captured from a deliberately dimmed sky, and
// against a manual exposure of -1.3 stops neither of them lands more than a couple of percent
// of the picture's linear range on a lit surface. Measured three ways on 2026-10-19: the island's
// median linear luminance stayed at 0.045 with the sun at 1x, 3x and 20x (only the sea's
// specular moved), with dynamic shadows off, and with the sky light at 2.6x. What did move it,
// linearly and predictably, was `SurfaceBaseGain` - i.e. it was albedo times a *constant*, and
// that constant was this emissive.
//
// So the `(1 - hi)` factor was working exactly backwards: it gave the *shadow* band the only
// light in the scene and left the lit band with nothing, which is why the island read as a dark
// mass with a bright rim on every crown.
//
// The second half is the one that actually fixes the picture. Since the renderer contributes
// nothing measurable, `BaseColor = albedo * band` is multiplied by zero and the bands are
// invisible - the island's whole value came from this node, uniformly, and a uniform fill has no
// light-to-dark structure at all. So the band goes *into the emissive*: the three tints are
// re-expressed here as a fill ramp with a wider spread than the base-colour ramp, because the
// fill is now the only thing drawing the form. `Sh * 0.55` against `Lit * 1.05` is a 3:1 value
// ratio at the same hue, which is what a cel band has to be to read at all.
float ndl = dot(normalize(N), normalize(S));
float det = dot(Detail, float3(0.299, 0.587, 0.114));
ndl += (det - 0.5) * Bump;
float hi = smoothstep(BandHigh - 0.05, BandHigh + 0.05, ndl);
float3 fillTint = lerp(Sh * 0.80, Lit * 1.05, hi);
return A * fillTint * Fill;
)");
			{
				auto Add = [&ShadowFillNode](const TCHAR* Name, UMaterialExpression* Expr)
				{
					FCustomInput In;
					In.InputName = Name;
					In.Input.Expression = Expr;
					ShadowFillNode->Inputs.Add(In);
				};
				Add(TEXT("A"), DesatNode);
				Add(TEXT("N"), ToonNormal);
				Add(TEXT("S"), SunDirParam);
				Add(TEXT("Detail"), DetailBlend);
				Add(TEXT("Bump"), BandBumpParam);
				Add(TEXT("Sh"), ShadowTintParam);
				Add(TEXT("Lit"), LitTintParam);
				Add(TEXT("BandHigh"), BandHighParam);
				Add(TEXT("Fill"), CelFillParam);
			}

			EditorData->BaseColor.Expression = ToonBase;
			EditorData->EmissiveColor.Expression = ShadowFillNode;
			EditorData->Roughness.Expression = Roughness;
			EditorData->Specular.Expression = Specular;

			// Read the graph back. "The ground ignores the sun" has three possible causes and
			// they are indistinguishable from the frame: the asset is not the shading model the
			// builder asked for, the base colour is not connected, or something is emitting.
			UE_LOG(LogSolstice, Display,
				TEXT("[diag] M_Cel model=%d blend=%d two_sided=%d base=%d emissive=%d rough=%d spec=%d nodes=%d gain=%.2f"),
				static_cast<int32>(Material->GetShadingModels().GetShadingModelField()),
				static_cast<int32>(Material->BlendMode),
				Material->TwoSided ? 1 : 0,
				EditorData->BaseColor.Expression ? 1 : 0,
				EditorData->EmissiveColor.Expression ? 1 : 0,
				EditorData->Roughness.Expression ? 1 : 0,
				EditorData->Specular.Expression ? 1 : 0,
				Material->GetExpressionCollection().Expressions.Num(),
				BaseGain->DefaultValue);
		}

		Material->PostEditChange();
		return SaveAsset(Material, PackageName);
	}

	/**
	 * M_Sky - the painted backdrop, and nothing else.
	 *
	 * The sky is the one surface in the frame whose colour must arrive *exactly* as authored: it
	 * is a matte painting, not a lit object, so it gets no bands at all. Running it through M_Cel
	 * would multiply the gradient by a shading band picked from the card's normal, which is how a
	 * near-white horizon turns into a blue-grey stripe depending on which way the camera faces.
	 */

	/**
	 * M_Sea - the open water.
	 *
	 * This is the first *reflective* surface in the project: roughness 0.06 + high specular make
	 * it pick the sky up through SSR and throw a sun glint, which is the entire difference between
	 * "a blue sheet" and "water". DefaultLit's own Fresnel then does the rest for free - grazing
	 * angles reflect and looking straight down does not, so the far water brightens toward the
	 * horizon and the near water stays deep, exactly like the reference photographs.
	 *
	 * It shares nothing with M_Cel on purpose: no triplanar detail, no vertex AO, no bands. The
	 * sea is one flat sheet, and detail on it would only moire.
	 */
	bool BuildSea()
	{
		const FString PackageName = TEXT("/Game/Core/M_Sea");
		UMaterial* Material = FindOrCreateMaterial(PackageName);
		if (!Material)
		{
			return false;
		}

		Material->MaterialDomain = MD_Surface;
		Material->BlendMode = BLEND_Opaque;
		Material->SetShadingModel(MSM_DefaultLit);
		// One-sided. The winding is now correct (Gen::BuildOcean) and it *has* to be one-sided:
		// a two-sided material flips the normal on back faces, and the Fresnel below reads that
		// normal. With the old winding the sea reported total reflection at every angle and came
		// out as flat sky-coloured water.
		Material->TwoSided = false;

		ResetGraph(Material);

		UMaterialExpressionVertexColor* VertexColour = AddExpr<UMaterialExpressionVertexColor>(Material, -600, -60);

		UMaterialExpressionScalarParameter* Roughness = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 180);
		Roughness->ParameterName = TEXT("WaterRoughness");
		// 0.06 turned the sea into a mirror of the sky: every pixel reflected the (pale) horizon
		// and the water colour disappeared. 0.22 keeps a broad sun glint without letting the
		// reflection own the surface.
		// 0.38, not 0.10.
		//
		// A tight lobe turns every wave crest into its own little sun: the sea reads as a field of
		// bright squiggles floating on flat colour (the "oil on water" look measured 2026-10-18).
		// A broad lobe merges them into one sheen and lets the water colour carry the surface.
		Roughness->DefaultValue = 0.46f;

		UMaterialExpressionScalarParameter* Specular = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 300);
		Specular->ParameterName = TEXT("WaterSpecular");
		// 0.08, not 0.45 and not 0.16.
		//
		// At 0.45 the glint covered the near sea in a white sheet (midpoint 0.682 against a 0.60
		// ceiling). At 0.16 the noon frame passed - but only because the noon sun sits almost
		// overhead and its reflection misses the camera. At 09:30 the same 0.16 put the glare
		// straight down the view axis and blew the whole lower half to white (midpoint 0.841,
		// headroom 0.45 - both FAIL). The glint has to survive being aimed at the camera, and
		// that is what 0.08 is for.
		Specular->DefaultValue = 0.01f;

		UMaterialExpressionScalarParameter* Recipe = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 420);
		Recipe->ParameterName = TEXT("RecipeVersion");
		Recipe->DefaultValue = static_cast<float>(CelRecipeVersion);

		// ---- wave normal ---------------------------------------------------
		// 平静的水面就是一面镜子：物理上没错（低角度看海本来就反射天空），但读起来不是水。
		// 真正让海成为海的是**法线扰动** —— 它把镜面反射打碎成一条条光带，也把天空的倒影
		// 揉成波纹。所以这一步不是装饰，是「水」这个材质成立的唯一理由。
		//
		// 四层方向波用一段 HLSL 写在一个 Custom 节点里：连二十个节点做同样的事，改一次要动
		// 半个图；这里改一个数字就行。参数（方向/频率/振幅/速度）取的是深海风浪的典型比值，
		// 高频层振幅小、速度快，低频层决定大的起伏。
		UMaterialExpressionWorldPosition* WorldPos = AddExpr<UMaterialExpressionWorldPosition>(Material, -1200, 640);
		UMaterialExpressionTime* Clock = AddExpr<UMaterialExpressionTime>(Material, -1200, 800);
		// Used for the one thing `ddx` could not do here: measure how far away the water is and
		// drop the high-frequency detail before it aliases. (See the note in WaveNormal.)
		UMaterialExpressionCameraPositionWS* CamPos = AddExpr<UMaterialExpressionCameraPositionWS>(Material, -1400, 1700);

		UMaterialExpressionScalarParameter* WaveStrength = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 920);
		WaveStrength->ParameterName = TEXT("WaveNormalStrength");
		// 0.18, not 1.35.
		//
		// The number multiplies the *summed slope* of four cos waves, whose weights add to ~3.5,
		// so 1.35 was asking for slopes near 1.0 - a 45 degree tilt. Physically water never tilts
		// more than 10-20 degrees outside a storm, and at 45 degrees the specular lobe covers a
		// huge part of the frame: the sea turns into crumpled foil lit from inside, which is
		// exactly what it looked like. 0.18 gives ~20 degrees at the crests.
		WaveStrength->DefaultValue = 0.18f;

		UMaterialExpressionScalarParameter* WaveScale = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1040);
		WaveScale->ParameterName = TEXT("WaveFrequencyScale");
		// 1/1250 uu -> a wavelength of ~65 px at the entry framing. Smaller (finer waves) turns
		// into aliasing noise; larger becomes a slow blotch.
		// 0.00058, not 0.00032.
		//
		// The wavelength is authored against the *entry framing*, and the entry framing moved
		// from 213 m to 160 m when the camera was brought in. At the old value every crest grew
		// by a third on screen and the interference pattern of the four direction waves turned
		// into long horizontal bands. Scaling with the camera keeps the water at the same
		// apparent grain it was tuned at.
		WaveScale->DefaultValue = 0.00058f;

		UMaterialExpressionCustom* WaveNormal = AddExpr<UMaterialExpressionCustom>(Material, -900, 760);
		WaveNormal->Description = TEXT("WaveNormal");
		WaveNormal->OutputType = CMOT_Float3;
		// Written out longhand instead of with a #define: the material compiler wraps this text
		// inside a function body, and a macro there was silently dropped (recipe v14 produced a
		// perfectly flat sea).
		WaveNormal->Code = TEXT(R"(
float2 p = P.xy * WaveScaleInput;

// ---- anti-aliasing: fade the short waves with distance --------------
//
// The single worst defect found by shooting the other camera presets (2026-10-18): from a
// raised camera the whole sea becomes a marble pattern - huge Bezier swirls several hundred
// pixels across, like polished stone. It is **moiré**: at that angle one pixel covers tens of
// wavelengths, so the warp field and the short waves are sampled far below their Nyquist limit
// and fold down into low-frequency interference. Confirmed by swapping the material's emissive
// for flat magenta: the swirls survived, so they live in the lighting through this normal.
//
// The textbook fix is to measure wave periods per pixel with `ddx` and fade accordingly. That
// was tried first and **measured no change at all** - the derivatives come back unusable in
// this material, which is worth recording so nobody spends another round on it.
//
// Distance is the robust proxy: one pixel spans `dist / focal` world units, so a fixed
// threshold in metres is equivalent up to the lens, and the lens does not change between shots.
// Near water (33-100 m in the entry framing) keeps every layer; past ~260 m the surface is
// smooth, which is also what a distant sea looks like.
const float CamDist = length(P - CamPos.xyz);
// 4 km to 12 km, not 8 to 26.
//
// Everything here was tuned when the sea only reached 3.8 km and the camera sat 30 m up. With
// the horizon in frame the visible water now runs to the horizon, and out there the swell does
// not read as waves - it reads as *bands*: a 78 m crest seen at a 3 degree grazing angle covers
// fifty pixels and the surface turns into a set of soft concentric stripes around the camera's
// nadir. Fading the whole slope field out by 12 km leaves the near water alive and the far water
// a clean gradient, which is also what a real sea looks like from a low shore.
const float NearFade = 1.0 - saturate((CamDist - 4000.0) / 8000.0);

// ---- domain warp ---------------------------------------------------
//
// Five plane waves that never stop being parallel are the reason this sea reads as brushed
// cloth. Amplitude was never the problem: the crests are straight lines, perspective compresses
// straight lines into horizontal bars, and no weighting of a straight line makes it a wave.
//
// So the coordinate is warped before the waves are evaluated. Two sines at different
// frequencies in x and y produce a non-repeating warp field, so the crests bend and pinch and
// the interference pattern stops having an axis. Cost: four extra sin() per pixel.
// Faded with distance like the waves it bends: the warp is itself a 1.7 Hz sine field, so at
// a raised camera it aliases just as badly as the crests do.
float2 warp = float2(sin(p.y * 1.7 + Time * 0.21), sin(p.x * 1.3 - Time * 0.17)) * 1.10 * NearFade;
float2 pw = p + warp;

float2 g = float2(0.0, 0.0);
float2 d0 = normalize(float2(0.60, -0.80));
float2 d1 = normalize(float2(0.86, 0.51));
float2 d2 = normalize(float2(-0.42, 0.91));
float2 d3 = normalize(float2(0.97, -0.24));
float2 d4 = normalize(float2(0.31, -0.95));
// L0 is new: a 78 m swell. Without it every wave in the frame is the same size, so the near
// water has no large shape to sit in front of and the surface reads as one uniform texture.
g += d0 * cos(dot(pw, d0) * 0.22 * 6.28318 + Time * 0.13) * 0.95;
g += d1 * cos(dot(pw, d1) * 1.00 * 6.28318 + Time * 0.35) * 1.00;
// The three short waves go first, then everything above the swell. What is left at the horizon
// is the 78 m and 17 m swell, which is what a distant sea actually shows.
const float HiFade = NearFade;
g += d2 * cos(dot(pw, d2) * 1.70 * 6.28318 + Time * 0.52) * 0.94 * HiFade;
g += d3 * cos(dot(pw, d3) * 2.90 * 6.28318 + Time * 0.77) * 0.87 * HiFade;
g += d4 * cos(dot(pw, d4) * 4.30 * 6.28318 + Time * 1.10) * 0.69 * HiFade;
g /= 4.45;

// ---- micro ripples -------------------------------------------------
//
// Everything above lives at 2-78 m. The nearest water in the entry framing is 33 m away,
// so even the finest of those octaves lands at about 80 px: the whole surface is built from
// "large" features and therefore reads as smooth, which is the second half of why the sea
// looked like painted silk after the stripes were dealt with.
//
// Real water puts its character at centimetres - not as waves but as *glints*, tiny facets
// that catch the sky at slightly different angles. Two octaves of interpolated random slopes
// at a 55 uu (0.55 m) cell put structure back at 10-25 px, and they move slowly with the
// current so the surface does not look frozen.
// 0.026 and quintic, not 0.018 and smoothstep.
//
// At 0.018 the cell was 0.55 m and 23 px at the nearest water, and a value-noise grid that
// large is *visible as a grid*: the interpolant is constant at every lattice point, so the eye
// picks out the diamonds even though no edge exists anywhere. Measured on the frame: a regular
// diamond pattern across the whole lower half, worse than the smooth surface it replaced.
// Three changes kill it - a finer cell, quintic interpolation (which flattens the second
// derivative at the lattice so the diamonds stop being corners), and a third octave so no
// single scale dominates.
float2 mq = P.xy * 0.026 + float2(Time * 0.06, Time * 0.045);

// The micro ripples are 45x the frequency of the swell, so they alias first and worst - at a
// raised camera they are the source of the finest scale of the marble pattern.
// Tighter than the swell's: 0.385 m ripples alias at a third of the distance.
const float MicroFade = 1.0 - saturate((CamDist - 1500.0) / 5500.0);

float2 mg = float2(0.0, 0.0);
float mamp = 1.0;
for (int m = 0; m < 3; m++)
{
    float2 mc = floor(mq);
    float2 mf = frac(mq);
    mf = mf * mf * mf * (mf * (mf * 6.0 - 15.0) + 10.0);

    float3 h0 = frac(float3(mc.x,        mc.y,        mc.x) * 0.1031);
    float3 h1 = frac(float3(mc.x + 1.0,  mc.y,        mc.x + 1.0) * 0.1031);
    float3 h2 = frac(float3(mc.x,        mc.y + 1.0,  mc.x) * 0.1031);
    float3 h3 = frac(float3(mc.x + 1.0,  mc.y + 1.0,  mc.x + 1.0) * 0.1031);
    h0 += dot(h0, h0.yzx + 33.33);
    h1 += dot(h1, h1.yzx + 33.33);
    h2 += dot(h2, h2.yzx + 33.33);
    h3 += dot(h3, h3.yzx + 33.33);

    float2 s0 = float2(frac((h0.x + h0.y) * h0.z), frac((h0.x + h0.z) * h0.y)) - 0.5;
    float2 s1 = float2(frac((h1.x + h1.y) * h1.z), frac((h1.x + h1.z) * h1.y)) - 0.5;
    float2 s2 = float2(frac((h2.x + h2.y) * h2.z), frac((h2.x + h2.z) * h2.y)) - 0.5;
    float2 s3 = float2(frac((h3.x + h3.y) * h3.z), frac((h3.x + h3.z) * h3.y)) - 0.5;

    mg += mamp * lerp(lerp(s0, s1, mf.x), lerp(s2, s3, mf.x), mf.y);
    mq = float2(mq.x * 1.83 - mq.y * 1.31, mq.x * 1.31 + mq.y * 1.83);
    mamp *= 0.52;
}
g += mg * 1.5 * MicroFade;

// And the slope itself goes flat with distance, not just the short octaves.
//
// NearFade only removes the high frequencies; the 17-78 m swell keeps tilting the surface out
// to the horizon, where a 1% tilt swings the reflected ray through several degrees of sky and
// draws the interference lattice of those plane waves as arcs. At 24 degrees that lattice was
// the "concentric rings" - not a colour ramp, not quantisation, and not the mesh.
const float SlopeFade = 1.0 - saturate((CamDist - 4000.0) / 8000.0);
return normalize(float3(-g * WaveStrengthInput * SlopeFade, 1.0));
)");
		{
			FCustomInput InPos;
			InPos.InputName = TEXT("P");
			InPos.Input.Expression = WorldPos;
			FCustomInput InTime;
			InTime.InputName = TEXT("Time");
			InTime.Input.Expression = Clock;
			FCustomInput InStrength;
			InStrength.InputName = TEXT("WaveStrengthInput");
			InStrength.Input.Expression = WaveStrength;
			FCustomInput InScale;
			InScale.InputName = TEXT("WaveScaleInput");
			InScale.Input.Expression = WaveScale;
			FCustomInput InCam;
			InCam.InputName = TEXT("CamPos");
			InCam.Input.Expression = CamPos;
			WaveNormal->Inputs.Add(InPos);
			WaveNormal->Inputs.Add(InTime);
			WaveNormal->Inputs.Add(InStrength);
			WaveNormal->Inputs.Add(InScale);
			WaveNormal->Inputs.Add(InCam);
		}

		// ---- wave height ---------------------------------------------------
		// The normal alone is not enough.
		//
		// The near sea is 44% of the frame and sits 3-30 degrees below the eye, where Fresnel is
		// near zero and the specular lobe points away: neither reflection nor highlight carries
		// anything there, so the surface was one flat emissive colour. Measured on the frame, the
		// near sea had a Laplacian variance of 0.37 against the island's 19.2 - fifty times less
		// detail precisely where half the picture is.
		//
		// So the *colour* has to move with the waves too: crests lighter, troughs darker, and a
		// band of white on the tallest crests. That is what stylised water does, and it is the
		// only thing that can put information on a surface facing away from the light.
		UMaterialExpressionCustom* WaveDetail = AddExpr<UMaterialExpressionCustom>(Material, -900, 880);
		WaveDetail->Description = TEXT("WaveDetail");
		WaveDetail->OutputType = CMOT_Float3;
		// Same four waves as WaveNormal, same directions and phases (a height field and its
		// gradient must agree or the highlight slides against the shading). Packed as
		// (height, foam, 0) and unpacked with dot products, so no extra node type is needed.
		WaveDetail->Code = TEXT(R"(
float2 p = P.xy * Scale;

// Distance fade, same reason and same curve as WaveNormal.
//
// This is the layer that actually produced the marble pattern, and it took a magenta probe to
// find: the swirls survived a flat emissive, so they were in the *lighting*, and they survived
// the wave normal being faded out, so they were in the body colour. They are the 5.5 m chop
// noise aliasing into a large-scale interference pattern - value noise has no band limit, and at
// a raised camera one pixel covers it twenty times over.
const float DetailCamDist = length(P - CamPos.xyz);
const float DetailCameraFade = 1.0 - saturate((DetailCamDist - 4000.0) / 8000.0);

// Same warp as WaveNormal, same constants, same order. A height field and the gradient it is
// shaded from must be the *same* field or the highlight slides against the shading - which was
// the bug that made an earlier version of this material look like oil on water.
float2 warp = float2(sin(p.y * 1.7 + T * 0.21), sin(p.x * 1.3 - T * 0.17)) * 1.10;
float2 pw = p + warp;

float swell = 0.0;
swell += sin(dot(pw, normalize(float2( 0.60, -0.80))) * 0.22 * 6.28318 + T * 0.13) * 0.95;
swell += sin(dot(pw, normalize(float2( 0.86,  0.51))) * 1.00 * 6.28318 + T * 0.35) * 1.00;
swell += sin(dot(pw, normalize(float2(-0.42,  0.91))) * 1.70 * 6.28318 + T * 0.52) * 0.94;
swell += sin(dot(pw, normalize(float2( 0.97, -0.24))) * 2.90 * 6.28318 + T * 0.77) * 0.87;
swell += sin(dot(pw, normalize(float2( 0.31, -0.95))) * 4.30 * 6.28318 + T * 1.10) * 0.69;
swell /= 4.45;

// Chop is *noise*, not more sine waves.
//
// Two more directional sines were the obvious next step and they failed for a structural
// reason: every term is a plane wave, so they interfere into a single dominant direction.
// Measured on the near sea, the horizontal gradient was 0.0018 against a vertical 0.0055 -
// the ripples were horizontal bars, i.e. wood grain, and no amount of amplitude fixes a
// direction that should not exist. Isotropic value noise (Hoskins hash, no `sin`) has no
// preferred axis, which is what water looks like from above.
// 9.0, not 3.1.
//
// At 3.1 the lattice cell was 5.5 m, which is larger than a boat: from any camera that looks
// down at the water the chop rendered as a handful of enormous blobs, and moving at 0.9 units a
// second they read as slow swirls rather than as ripples. The distance fade hides it at range,
// so the fix is to make the near water actually fine: 1.9 m.
float2 q = p * 9.0 + float2(T * 1.6, T * 0.7);
float chop = 0.0;
float camp = 0.5;
for (int i = 0; i < 3; i++)
{
    float2 cell = floor(q);
    float2 f = frac(q);
    f = f * f * (3.0 - 2.0 * f);

    float3 g0 = frac(float3(cell.x,        cell.y,        cell.x) * 0.1031);
    float3 g1 = frac(float3(cell.x + 1.0,  cell.y,        cell.x + 1.0) * 0.1031);
    float3 g2 = frac(float3(cell.x,        cell.y + 1.0,  cell.x) * 0.1031);
    float3 g3 = frac(float3(cell.x + 1.0,  cell.y + 1.0,  cell.x + 1.0) * 0.1031);
    g0 += dot(g0, g0.yzx + 33.33);
    g1 += dot(g1, g1.yzx + 33.33);
    g2 += dot(g2, g2.yzx + 33.33);
    g3 += dot(g3, g3.yzx + 33.33);

    float v0 = frac((g0.x + g0.y) * g0.z);
    float v1 = frac((g1.x + g1.y) * g1.z);
    float v2 = frac((g2.x + g2.y) * g2.z);
    float v3 = frac((g3.x + g3.y) * g3.z);

    chop += camp * lerp(lerp(v0, v1, f.x), lerp(v2, v3, f.x), f.y);
    q = float2(q.x * 1.66 - q.y * 1.24, q.x * 1.24 + q.y * 1.66);
    camp *= 0.5;
}
chop = (chop - 0.44) * 1.9;

// Chop is weighted above the swell, not below it. These ripples are the only structure the
// nearest 20% of the frame can show (the swell is longer than the view there), and at equal
// weight they moved the picture by 8.6% brightness - under the noise floor of the swell.
// Chop leads, swell follows.
//
// Both terms were weighted equally and the swell won: four plane waves at a fixed set of
// directions interfere into long parallel bands, and once perspective compresses them toward
// the horizon the whole sea reads as brushed silk - the single most artificial thing in the
// frame, and it covers half of it. Isotropic noise has no such axis, so it goes on top; the
// swell keeps the large shape, at 60%.
// 0.28, not 0.55.
//
// The 78 m swell is bigger than the near water is deep in the view: at a 3-30 degree look-down
// angle one of its crests covers a fifth of the frame, so as a *shade* term it does not read as
// a wave, it reads as a dark stain sliding across the surface (measured 2026-10-18 - the near
// sea came out with large soft blotches that looked like clouds shadows on the water). The big
// swell belongs in the normal and in the silhouette; the body colour is driven by the chop,
// which is the scale the eye can actually resolve at this distance.
float hn = (swell * 0.28 + chop * 1.25) * DetailCameraFade;
// Foam belongs to the swell's crests, not to the ripples riding on them, and it starts low:
// at 0.42 only the rare tallest crest qualified and no white ever reached the frame.
float foam = saturate((swell - 0.26) / 0.34) * DetailCameraFade;
return float3(hn, foam, 0.0);
)");
		{
			FCustomInput InPos;
			InPos.InputName = TEXT("P");
			InPos.Input.Expression = WorldPos;
			FCustomInput InTime;
			InTime.InputName = TEXT("T");
			InTime.Input.Expression = Clock;
			FCustomInput InScale;
			InScale.InputName = TEXT("Scale");
			InScale.Input.Expression = WaveScale;
			FCustomInput InCam;
			InCam.InputName = TEXT("CamPos");
			InCam.Input.Expression = CamPos;
			WaveDetail->Inputs.Add(InPos);
			WaveDetail->Inputs.Add(InTime);
			WaveDetail->Inputs.Add(InScale);
			WaveDetail->Inputs.Add(InCam);
		}

		UMaterialExpressionConstant3Vector* AxisHeight = AddExpr<UMaterialExpressionConstant3Vector>(Material, -700, 760);
		AxisHeight->Constant = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		UMaterialExpressionConstant3Vector* AxisFoam = AddExpr<UMaterialExpressionConstant3Vector>(Material, -700, 880);
		AxisFoam->Constant = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

		UMaterialExpressionDotProduct* WaveHeight = AddExpr<UMaterialExpressionDotProduct>(Material, -520, 760);
		WaveHeight->A.Expression = WaveDetail;
		WaveHeight->B.Expression = AxisHeight;

		UMaterialExpressionDotProduct* WaveFoam = AddExpr<UMaterialExpressionDotProduct>(Material, -520, 880);
		WaveFoam->A.Expression = WaveDetail;
		WaveFoam->B.Expression = AxisFoam;

		// The ocean mesh is built once (it is 60 realm radii across), so its vertex colours are a
		// photograph of noon. A tint parameter lets the hour move it without rebuilding 40k
		// triangles: night sea is not "noon sea with the lights off", it is dark and blue.
		UMaterialExpressionVectorParameter* SeaTint = AddExpr<UMaterialExpressionVectorParameter>(Material, -600, 1000);
		SeaTint->ParameterName = TEXT("SeaTint");
		SeaTint->DefaultValue = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

		// ---- palette -------------------------------------------------------
		//
		// The sea is the largest object in every frame (45-60% of the picture at the two low
		// stops), so it carries the picture's value structure. Its colour is authored as display
		// values and goes straight to the emissive body: water is not "a lit blue", and trying to
		// light it physically produced either a black sheet or fluorescent cyan (see the note on
		// Scatter below).
		UMaterialExpressionWorldPosition* SeaPos = AddExpr<UMaterialExpressionWorldPosition>(Material, -1600, -260);
		UMaterialExpressionCameraPositionWS* SeaCam = AddExpr<UMaterialExpressionCameraPositionWS>(Material, -1600, -400);

		auto Vec = [&](const TCHAR* Name, const FLinearColor& Value, int32 X, int32 Y)
		{
			UMaterialExpressionVectorParameter* Node = AddExpr<UMaterialExpressionVectorParameter>(Material, X, Y);
			Node->ParameterName = Name;
			Node->DefaultValue = Value;
			return Node;
		};
		auto Sca = [&](const TCHAR* Name, float Value, int32 X, int32 Y)
		{
			UMaterialExpressionScalarParameter* Node = AddExpr<UMaterialExpressionScalarParameter>(Material, X, Y);
			Node->ParameterName = Name;
			Node->DefaultValue = Value;
			return Node;
		};

		UMaterialExpressionVectorParameter* SeaShallow = Vec(TEXT("SeaShallow"), FLinearColor(0.045f, 0.360f, 0.395f, 1.0f), -1600, -540);
		UMaterialExpressionVectorParameter* SeaMid = Vec(TEXT("SeaMid"), FLinearColor(0.018f, 0.150f, 0.185f, 1.0f), -1600, -610);
		UMaterialExpressionVectorParameter* SeaDeep = Vec(TEXT("SeaDeep"), FLinearColor(0.004f, 0.038f, 0.135f, 1.0f), -1600, -680);
		UMaterialExpressionScalarParameter* SeaShallowR = Sca(TEXT("SeaShallowRadius"), 8000.0f, -1600, -820);
		UMaterialExpressionScalarParameter* SeaDeepR = Sca(TEXT("SeaDeepRadius"), 18000.0f, -1600, -960);
		UMaterialExpressionScalarParameter* SeaHaze = Sca(TEXT("SeaHazeSpan"), 60000.0f, -1600, -1100);
		UMaterialExpressionScalarParameter* SeaHazeStart = Sca(TEXT("SeaHazeStart"), 16000.0f, -1600, -1030);
		UMaterialExpressionVectorParameter* SeaHazeColour =
			Vec(TEXT("SeaHazeColour"), FLinearColor(0.62f, 0.78f, 0.86f, 1.0f), -1600, -1240);
		UMaterialExpressionScalarParameter* ShoreRadius = Sca(TEXT("ShoreRadius"), 6400.0f, -1600, -1320);

		UMaterialExpressionCustom* SeaDepth = AddExpr<UMaterialExpressionCustom>(Material, -1100, -400);
		SeaDepth->Description = TEXT("SeaDepth");
		SeaDepth->OutputType = CMOT_Float3;
		// Depth is a *monotonic* ramp with two soft steps and nothing else.
		//
		// This used to have three legs and a "reef break" - a one-pixel bright line at the shelf
		// edge, added because a photograph of a reef has one. In a 3D scene it is a locus where
		// the colour function's slope jumps, and since every visible metre of water is drawn from
		// this one function, the jump became a hard oval painted around the island, a hundred
		// pixels above the shore, with no geometry to explain it. Every "the sea has rings"
		// report since then was this line and its two siblings - the band boundaries.
		//
		// A depth ramp the eye accepts needs no boundaries: bright sand shelf, then a long fade
		// to open water, then aerial perspective. Smoothsteps overlap so there is no edge to see.
		SeaDepth->Code = TEXT(R"(
float r = length(P.xy);
float t = saturate((r - NearRadius) / max(FarRadius - NearRadius, 1000.0));

// Lagoon -> open water. The two smoothsteps overlap, so the derivative never jumps and no
// contour can appear no matter how slowly the camera moves.
float3 col = lerp(Shallow, Mid, smoothstep(0.0, 0.55, t));
col = lerp(col, Deep, smoothstep(0.45, 1.0, t));

// Aerial perspective, driven by distance to the *eye*.
//
// Distance to the island was the wrong axis: the near shore and the far shore are the same
// distance from the camera, so a radius-based haze leaves a bright ring of near water on the
// far side. Distance to the eye is monotonic across the whole frame by construction, which is
// exactly what a haze ramp needs to avoid banding.
float d = length(P - CamPos.xyz);
col = lerp(col, Haze, smoothstep(HazeStart, max(HazeSpan, HazeStart + 1.0), d));
return col;
)");
		{
			auto Add = [&SeaDepth](const TCHAR* Name, UMaterialExpression* Expr)
			{
				FCustomInput In;
				In.InputName = Name;
				In.Input.Expression = Expr;
				SeaDepth->Inputs.Add(In);
			};
			Add(TEXT("P"), SeaPos);
			Add(TEXT("CamPos"), SeaCam);
			Add(TEXT("Shallow"), SeaShallow);
			Add(TEXT("Mid"), SeaMid);
			Add(TEXT("Deep"), SeaDeep);
			Add(TEXT("NearRadius"), SeaShallowR);
			Add(TEXT("FarRadius"), SeaDeepR);
			Add(TEXT("HazeSpan"), SeaHaze);
			Add(TEXT("HazeStart"), SeaHazeStart);
			Add(TEXT("Haze"), SeaHazeColour);
		}

		UMaterialExpressionMultiply* TintedBase = AddExpr<UMaterialExpressionMultiply>(Material, -420, -60);
		TintedBase->A.Expression = SeaDepth;
		TintedBase->B.Expression = SeaTint;

		// ---- body colour: emissive, not lit --------------------------------
		//
		// PBR water is albedo x light, and deep water's albedo is a few percent - at this
		// exposure the sun multiplies it to zero (measured: R = 0.000 across the lower half of
		// the frame) and turning the sun up instead blows the whole sea to fluorescent cyan.
		// Neither end is right, because the colour of the sea is not "a blue being lit".
		//
		// So the palette lands on screen as emissive and the diffuse path keeps 1% - just enough
		// that the surface still answers the sun at all. The consequence, recorded honestly: an
		// emissive surface cannot receive a shadow, so island shadows do not fall on the water.
		UMaterialExpressionScalarParameter* Scatter = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1160);
		Scatter->ParameterName = TEXT("WaterGlow");
		Scatter->DefaultValue = 0.52f;

		UMaterialExpressionScalarParameter* Diffuse = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1080);
		Diffuse->ParameterName = TEXT("WaterDiffuse");
		Diffuse->DefaultValue = 0.010f;

		UMaterialExpressionMultiply* ScatterColour = AddExpr<UMaterialExpressionMultiply>(Material, -900, 1160);
		ScatterColour->A.Expression = TintedBase;
		ScatterColour->B.Expression = Scatter;

		UMaterialExpressionMultiply* DiffuseBase = AddExpr<UMaterialExpressionMultiply>(Material, -780, 1000);
		DiffuseBase->A.Expression = TintedBase;
		DiffuseBase->B.Expression = Diffuse;

		// ---- the sky in the water ------------------------------------------
		//
		// The single most important term on this surface, and until now the crudest one: the
		// reflection was *one constant colour* set from C++ (the sky's mid tone), mixed in by a
		// Fresnel node. A constant cannot tilt with the waves, cannot carry a sun, and cannot go
		// from pale at the horizon to deep overhead - so the sea reflected nothing and read as a
		// painted sheet.
		//
		// Here the reflected ray is evaluated against the *same gradient the sky material uses*,
		// per pixel. Two consequences worth stating:
		//   - the horizon matches by construction, because both sides evaluate one formula;
		//   - the sun's mirror image appears on its own, and the wave normals break it into a
		//     glitter path. That path is the thing that makes a still frame read as water.
		UMaterialExpressionVectorParameter* SunDirSea = Vec(TEXT("SunDirection"), FLinearColor(0.4f, 0.4f, 0.82f, 0.0f), -1600, 1620);
		UMaterialExpressionVectorParameter* SkyZenithSea = Vec(TEXT("SkyZenith"), FLinearColor(0.02f, 0.12f, 0.42f, 1.0f), -1600, 1700);
		UMaterialExpressionVectorParameter* SkyMidSea = Vec(TEXT("SkyMid"), FLinearColor(0.15f, 0.40f, 0.72f, 1.0f), -1600, 1780);
		UMaterialExpressionVectorParameter* SkyHorizonSea = Vec(TEXT("SkyHorizon"), FLinearColor(0.52f, 0.72f, 0.86f, 1.0f), -1600, 1860);
		UMaterialExpressionVectorParameter* SunHaloSea = Vec(TEXT("SunHalo"), FLinearColor(1.0f, 0.90f, 0.62f, 1.0f), -1600, 1940);
		UMaterialExpressionScalarParameter* SunPath = Sca(TEXT("SunPathStrength"), 2.2f, -1600, 2020);

		UMaterialExpressionCustom* SeaSky = AddExpr<UMaterialExpressionCustom>(Material, -1000, 1500);
		SeaSky->Description = TEXT("SeaSky");
		SeaSky->OutputType = CMOT_Float3;
		SeaSky->Code = TEXT(R"(
float3 n = normalize(N);
float3 v = normalize(CamPos.xyz - P);

// Flatten the normal with distance before reflecting.
//
// The wave field is a sum of plane waves; a few kilometres out, one pixel covers dozens of
// wavelengths, and what survives is the *interference* of those waves - a lattice of arcs that
// reads as concentric rings around the island. It is the same aliasing that was solved twice in
// the body colour and never in the reflection, which is where the sea's brightness actually
// comes from at a grazing angle. A mirror-smooth far field is also what distant water looks
// like, so the fix and the truth agree here.
const float Dist = length(P - CamPos.xyz);
const float Flat = saturate((Dist - 5000.0) / 17000.0);
n = normalize(lerp(n, float3(0.0, 0.0, 1.0), Flat));

float3 refl = reflect(-v, n);
float h = refl.z;

// Same three-stop gradient as M_Sky (see BuildSky). Kept in sync by hand, and that is
// deliberate: two materials that must agree are cheaper to keep aligned than to make one
// sample the other, and a divergence shows up immediately as a seam on the horizon.
float3 sky = lerp(Horizon, Mid, smoothstep(0.0, 0.13, h));
sky = lerp(sky, Zenith, smoothstep(0.11, 0.42, h));
// Below the horizon there is no sky to reflect - the ray hits water. Tilt toward the haze.
sky = lerp(sky, Horizon * 0.92, saturate(-h * 5.0));

float cosA = dot(refl, normalize(SunDir.xyz));
// Broad warm glow around the sun, then the mirror of the disc itself. The tight lobe is the
// glitter path: without it the sun has no presence on the water at all.
sky += SunHalo * pow(saturate(cosA), 6.0) * 0.10;
sky += SunHalo * pow(saturate(cosA), 140.0) * SunPath;

// No horizon boost in the *reflection*.
//
// The gradient already ends at the horizon colour, and the sea's grazing pixels mirror exactly
// that band - so any extra brightening here makes the water brighter than the sky it is
// reflecting, and the horizon becomes a line with a bright edge underneath it. The sky material
// keeps its own band (it also has the sun's halo); the reflection must not add a second one.
sky = lerp(sky, Horizon * 1.005, pow(saturate(1.0 - h * 3.5), 6.0) * 0.06);
return sky;
)");
		{
			auto Add = [&SeaSky](const TCHAR* Name, UMaterialExpression* Expr)
			{
				FCustomInput In;
				In.InputName = Name;
				In.Input.Expression = Expr;
				SeaSky->Inputs.Add(In);
			};
			Add(TEXT("N"), WaveNormal);
			Add(TEXT("P"), SeaPos);
			Add(TEXT("CamPos"), SeaCam);
			Add(TEXT("SunDir"), SunDirSea);
			Add(TEXT("Zenith"), SkyZenithSea);
			Add(TEXT("Mid"), SkyMidSea);
			Add(TEXT("Horizon"), SkyHorizonSea);
			Add(TEXT("SunHalo"), SunHaloSea);
			Add(TEXT("SunPath"), SunPath);
		}

		UMaterialExpressionScalarParameter* ReflectStrength = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1560);
		ReflectStrength->ParameterName = TEXT("SkyReflectStrength");
		ReflectStrength->DefaultValue = 1.15f;

		UMaterialExpressionCustom* SeaFresnel = AddExpr<UMaterialExpressionCustom>(Material, -1000, 1680);
		SeaFresnel->Description = TEXT("SeaFresnel");
		SeaFresnel->OutputType = CMOT_Float3;
		// Schlick, then a gain.
		//
		// A physical Fresnel (n = 1.33) is 2% looking straight down and 100% at grazing, which is
		// correct and - at this camera - makes the water a mirror over most of the frame, because
		// the visible sea sits between 2 and 40 degrees below the eye. The gain lets the two ends
		// be tuned independently of the exponent: the sea keeps its colour in the near field and
		// still meets the sky at the horizon, which is the only band where the two must agree.
		SeaFresnel->Code = TEXT(R"(
float3 n = normalize(N);
float3 v = normalize(CamPos.xyz - P);
float ndv = saturate(dot(n, v));
float f = 0.02 + 0.98 * pow(1.0 - ndv, 5.0);
return float3(saturate(f * Gain), 0.0, 0.0);
)");
		{
			auto Add = [&SeaFresnel](const TCHAR* Name, UMaterialExpression* Expr)
			{
				FCustomInput In;
				In.InputName = Name;
				In.Input.Expression = Expr;
				SeaFresnel->Inputs.Add(In);
			};
			Add(TEXT("N"), WaveNormal);
			Add(TEXT("P"), SeaPos);
			Add(TEXT("CamPos"), SeaCam);
			Add(TEXT("Gain"), ReflectStrength);
		}

		UMaterialExpressionConstant3Vector* FresnelAxis = AddExpr<UMaterialExpressionConstant3Vector>(Material, -780, 1700);
		FresnelAxis->Constant = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

		UMaterialExpressionDotProduct* ReflectMix = AddExpr<UMaterialExpressionDotProduct>(Material, -620, 1660);
		ReflectMix->A.Expression = SeaFresnel;
		ReflectMix->B.Expression = FresnelAxis;

		// ---- crest shading -------------------------------------------------
		// The body colour has to move with the waves, because the near sea sits 3-30 degrees
		// below the eye where neither reflection nor highlight carries anything. Crests lighter,
		// troughs darker: the only thing that can put information on a surface facing away from
		// the light.
		UMaterialExpressionScalarParameter* ShadeAmp = AddExpr<UMaterialExpressionScalarParameter>(Material, -700, 1020);
		ShadeAmp->ParameterName = TEXT("WaveShadeAmount");
		ShadeAmp->DefaultValue = 0.19f;

		UMaterialExpressionMultiply* ShadeOffset = AddExpr<UMaterialExpressionMultiply>(Material, -520, 1020);
		ShadeOffset->A.Expression = WaveHeight;
		ShadeOffset->B.Expression = ShadeAmp;

		UMaterialExpressionConstant* OneShade = AddExpr<UMaterialExpressionConstant>(Material, -520, 1140);
		OneShade->R = 1.0f;

		// Crest shading has to disappear where the surface is a mirror.
		//
		// At a low camera the sea is seen almost edge-on: a 78 m swell then projects into a
		// thin horizontal line every 78 m of depth, and the body shading draws it. The result
		// is a stack of parallel stripes across half the frame - "brushed silk", the exact
		// artefact the wave code spent three rounds removing, reintroduced from the other side
		// by a shading term that had no facing gate.
		UMaterialExpressionOneMinus* ShadeFacing = AddExpr<UMaterialExpressionOneMinus>(Material, -420, 1120);
		ShadeFacing->Input.Expression = ReflectMix;

		UMaterialExpressionMultiply* ShadeGated = AddExpr<UMaterialExpressionMultiply>(Material, -320, 1080);
		ShadeGated->A.Expression = ShadeOffset;
		ShadeGated->B.Expression = ShadeFacing;

		UMaterialExpressionAdd* ShadeScale = AddExpr<UMaterialExpressionAdd>(Material, -240, 1060);
		ShadeScale->A.Expression = OneShade;
		ShadeScale->B.Expression = ShadeGated;

		UMaterialExpressionMultiply* ShadedBody = AddExpr<UMaterialExpressionMultiply>(Material, -100, 1140);
		ShadedBody->A.Expression = ScatterColour;
		ShadedBody->B.Expression = ShadeScale;

		UMaterialExpressionVectorParameter* FoamColour = AddExpr<UMaterialExpressionVectorParameter>(Material, -700, 1260);
		FoamColour->ParameterName = TEXT("FoamColour");
		FoamColour->DefaultValue = FLinearColor(0.78f, 0.88f, 0.90f, 1.0f);

		UMaterialExpressionScalarParameter* FoamAmount = AddExpr<UMaterialExpressionScalarParameter>(Material, -700, 1380);
		FoamAmount->ParameterName = TEXT("FoamAmount");
		FoamAmount->DefaultValue = 0.14f;

		// ---- shore foam ----------------------------------------------------
		//
		// The island had a pale ring painted into the *terrain* (a 35 cm rim on the water tiles)
		// and nothing on the water itself, so the surf stopped exactly at the beach and the open
		// sea began like a table. A shoreline without white water is the single clearest "this is
		// a model" signal in the frame - every photograph of a coast has it.
		//
		// It is driven by distance to the island centre rather than by the tile data: the realm
		// is a disc, so radius *is* the shore up to the torn edge's noise, which is smaller than
		// the band. The band is broken up by an angular wave so it does not read as a drawn ring.
		UMaterialExpressionCustom* ShoreFoam = AddExpr<UMaterialExpressionCustom>(Material, -1000, 1800);
		ShoreFoam->Description = TEXT("ShoreFoam");
		ShoreFoam->OutputType = CMOT_Float3;
		ShoreFoam->Code = TEXT(R"(
float r = length(P.xy);
float2 dir = P.xy / max(r, 1.0);

// Two bands: the swash right at the beach, and a weaker line a little further out where the
// last wave broke. The second one is what reads as "the water has been moving".
float inner = 1.0 - saturate((r - R * 1.00) / (R * 0.10));
float outer = 1.0 - saturate(abs(r - R * 1.16) / (R * 0.05));

// A hard guard, independent of R.
//
// The two bands above are divisions by R, so they are only correct while R is the island's
// radius. Whatever happened on 2026-10-19, the foam covered the whole sea - a few thousand
// bright ripples radiating to the horizon, which is the "concentric rings" defect that was
// chased through the depth ramp, the wave shading, the sky reflection, the mesh ring count,
// the film grain and the 8-bit dither, each of which changed nothing. The probe that found it
// was zeroing *every* sea parameter at once: the rings got brighter, because the body went
// dark and the foam did not.
//
// This mask is a distance from the island in metres, and it does not depend on any parameter
// being right: past 1.5 radii there is no surf, full stop.
// Absolute metres, not a multiple of R: the parameter is the thing under suspicion, and a guard
// that can be scaled by a bad parameter is not a guard. 1.6 x a 64 m realm is 102 m.
const float shoreMask = 1.0 - smoothstep(90.0, 115.0, r / 6400.0 * 64.0);

// Angular breakup with a wave running along the coast, so neither band is a circle.
float ang = atan2(dir.y, dir.x);
float ripple = 0.55 + 0.45 * sin(ang * 11.0 + r * 0.021 + T * 0.9);
ripple *= 0.65 + 0.35 * sin(ang * 29.0 - r * 0.012 - T * 1.6);

float foam = saturate(inner * (0.55 + 0.45 * ripple) + outer * 0.55 * ripple) * shoreMask;
return float3(foam, 0.0, 0.0);
)");
		{
			auto Add = [&ShoreFoam](const TCHAR* Name, UMaterialExpression* Expr)
			{
				FCustomInput In;
				In.InputName = Name;
				In.Input.Expression = Expr;
				ShoreFoam->Inputs.Add(In);
			};
			Add(TEXT("P"), SeaPos);
			Add(TEXT("T"), Clock);
			Add(TEXT("R"), ShoreRadius);
		}

		UMaterialExpressionDotProduct* ShoreFoamScalar = AddExpr<UMaterialExpressionDotProduct>(Material, -780, 1800);
		ShoreFoamScalar->A.Expression = ShoreFoam;
		ShoreFoamScalar->B.Expression = FresnelAxis;

		UMaterialExpressionMultiply* CrestFoam = AddExpr<UMaterialExpressionMultiply>(Material, -380, 1300);
		CrestFoam->A.Expression = WaveFoam;
		CrestFoam->B.Expression = FoamAmount;

		UMaterialExpressionAdd* FoamSum = AddExpr<UMaterialExpressionAdd>(Material, -240, 1400);
		FoamSum->A.Expression = CrestFoam;
		FoamSum->B.Expression = ShoreFoamScalar;

		UMaterialExpressionLinearInterpolate* BodyWithFoam = AddExpr<UMaterialExpressionLinearInterpolate>(Material, -80, 1180);
		BodyWithFoam->A.Expression = ShadedBody;
		BodyWithFoam->B.Expression = FoamColour;
		BodyWithFoam->Alpha.Expression = FoamSum;

		UMaterialExpressionLinearInterpolate* EmissiveTotal = AddExpr<UMaterialExpressionLinearInterpolate>(Material, -420, 1240);
		EmissiveTotal->A.Expression = BodyWithFoam;
		EmissiveTotal->B.Expression = SeaSky;
		EmissiveTotal->Alpha.Expression = ReflectMix;

		// ---- dither ------------------------------------------------------
		//
		// Even a monotonic ramp quantises at 8 bits, and a slow ramp over 2000 px shows every
		// step as a contour. A stable +/-1.5% world-space hash moves each pixel across the
		// boundary unpredictably. It is far below the noise floor of a compressed screenshot and
		// it is the only thing that removes contours without touching the colour.
		// Screen position, not world position.
		//
		// A world-space hash has a one-metre cell; at 20 km one pixel covers twenty of them, so
		// the hash is sampled far below its frequency and averages straight back to the value it
		// was meant to break. Screen space is the only lattice guaranteed to be at pixel pitch,
		// which is what a dither has to be.
		UMaterialExpressionScreenPosition* DitherPos = AddExpr<UMaterialExpressionScreenPosition>(Material, -640, 1360);
		UMaterialExpressionCustom* SeaDither = AddExpr<UMaterialExpressionCustom>(Material, -200, 1320);
		SeaDither->Description = TEXT("SeaDither");
		SeaDither->OutputType = CMOT_Float3;
		SeaDither->Code = TEXT(R"(
// Hoskins hash on the integer lattice, NOT sin(dot(...)).
//
// The previous version hashed with `frac(sin(dot(P.xy, k)) * 43758.5453)`, and P.xy here is a
// world position in the hundreds of thousands: the sine argument is ~1e7, float32 keeps about
// seven digits, and every pixel on the sea rounded to the same value. The dither was a
// constant. It is the same failure the sky's cloud hash was fixed for - the comment there
// even says so - and it is why the sea kept its contour rings through three rounds of
// "I added a dither".
float2 c = floor(P * 4096.0);
float3 h = frac(float3(c.x, c.y, c.x) * 0.1031);
h += dot(h, h.yzx + 33.33);
float n = frac((h.x + h.y) * h.z);
return C * (1.0 + (n - 0.5) * 0.055);
)");
		{
			FCustomInput InC;
			InC.InputName = TEXT("C");
			InC.Input.Expression = EmissiveTotal;
			SeaDither->Inputs.Add(InC);
			FCustomInput InP;
			InP.InputName = TEXT("P");
			InP.Input.Expression = DitherPos;
			SeaDither->Inputs.Add(InP);
		}

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->BaseColor.Expression = DiffuseBase;
			EditorData->Roughness.Expression = Roughness;
			EditorData->Specular.Expression = Specular;
			EditorData->Normal.Expression = WaveNormal;
			EditorData->EmissiveColor.Expression = SeaDither;
		}

		Material->PostEditChange();
		return SaveAsset(Material, PackageName);
	}

	bool BuildSky()
	{
		const FString PackageName = TEXT("/Game/Core/M_Sky");
		UMaterial* Material = FindOrCreateMaterial(PackageName);
		if (!Material)
		{
			return false;
		}

		Material->MaterialDomain = MD_Surface;
		Material->BlendMode = BLEND_Opaque;
		Material->SetShadingModel(MSM_Unlit);
		// The dome is seen from inside. Two-sided so that a wrong winding can never punch a hole
		// in the background - it is the one surface whose failure is a black frame.
		Material->TwoSided = true;

		ResetGraph(Material);

		// ---- view direction -------------------------------------------------
		//
		// The per-pixel view ray, not WorldPosition minus CameraPosition.
		//
		// WorldPosition is interpolated *linearly* across a triangle, and on a curved dome a
		// triangle is a chord, not an arc - so the reconstructed direction is smooth inside a
		// triangle and bends at every edge. Any threshold on a field derived from it, which is
		// exactly what a cloud mask is, draws that bend as a straight line. The view ray has no
		// such problem: it is exact per pixel, and since the dome is centred on the camera it
		// *is* the dome direction.
		UMaterialExpressionCameraVectorWS* Dir = AddExpr<UMaterialExpressionCameraVectorWS>(Material, -1380, -240);

		UMaterialExpressionTime* Clock = AddExpr<UMaterialExpressionTime>(Material, -1600, 20);

		// ---- parameters ------------------------------------------------------
		// Every colour is authored in C++ (FSimKernel's sky state -> FSkyPaint) and pushed here;
		// nothing in the material decides what time of day it is.
		auto Vector = [&](const TCHAR* Name, const FLinearColor& Default, int32 X, int32 Y)
		{
			UMaterialExpressionVectorParameter* Node = AddExpr<UMaterialExpressionVectorParameter>(Material, X, Y);
			Node->ParameterName = Name;
			Node->DefaultValue = Default;
			return Node;
		};
		auto Scalar = [&](const TCHAR* Name, float Default, int32 X, int32 Y)
		{
			UMaterialExpressionScalarParameter* Node = AddExpr<UMaterialExpressionScalarParameter>(Material, X, Y);
			Node->ParameterName = Name;
			Node->DefaultValue = Default;
			return Node;
		};

		UMaterialExpressionVectorParameter* SunDir = Vector(TEXT("SunDirection"), FLinearColor(0.4f, 0.4f, 0.82f, 0.0f), -1200, 60);
		UMaterialExpressionVectorParameter* Zenith = Vector(TEXT("SkyZenith"), FLinearColor(0.02f, 0.12f, 0.42f, 1.0f), -1200, 200);
		UMaterialExpressionVectorParameter* Mid = Vector(TEXT("SkyMid"), FLinearColor(0.15f, 0.40f, 0.72f, 1.0f), -1200, 340);
		UMaterialExpressionVectorParameter* Horizon = Vector(TEXT("SkyHorizon"), FLinearColor(0.52f, 0.72f, 0.86f, 1.0f), -1200, 480);
		UMaterialExpressionVectorParameter* CloudLit = Vector(TEXT("CloudLit"), FLinearColor(1.0f, 0.99f, 0.96f, 1.0f), -1200, 620);
		UMaterialExpressionVectorParameter* CloudShadow = Vector(TEXT("CloudShadow"), FLinearColor(0.42f, 0.50f, 0.63f, 1.0f), -1200, 760);
		UMaterialExpressionVectorParameter* SunCore = Vector(TEXT("SunCore"), FLinearColor(1.0f, 0.97f, 0.86f, 1.0f), -1200, 900);
		UMaterialExpressionVectorParameter* SunHalo = Vector(TEXT("SunHalo"), FLinearColor(1.0f, 0.90f, 0.62f, 1.0f), -1200, 1040);

		UMaterialExpressionScalarParameter* Threshold = Scalar(TEXT("CloudThreshold"), 0.547f, -1200, 1180);
		UMaterialExpressionScalarParameter* CloudScale = Scalar(TEXT("CloudScale"), 3.2f, -1200, 1300);
		UMaterialExpressionScalarParameter* Drift = Scalar(TEXT("CloudDrift"), 0.0f, -1200, 1420);
		UMaterialExpressionScalarParameter* SunDiscCos = Scalar(TEXT("SunDiscCos"), 0.99985f, -1200, 1540);
		// The moon is the same disc test as the sun, at a colder colour and a sharper falloff.
		// It has to live here: the old card painted one, and dropping it would leave every night
		// frame with an empty sky.
		UMaterialExpressionVectorParameter* MoonDir = Vector(TEXT("MoonDirection"), FLinearColor(-0.4f, -0.4f, 0.6f, 0.0f), -1200, 1620);
		UMaterialExpressionVectorParameter* MoonCore = Vector(TEXT("MoonCore"), FLinearColor(0.85f, 0.89f, 1.0f, 1.0f), -1200, 1740);
		UMaterialExpressionScalarParameter* MoonDiscCos = Scalar(TEXT("MoonDiscCos"), 0.99990f, -1200, 1860);
		UMaterialExpressionScalarParameter* MoonAlpha = Scalar(TEXT("MoonAlpha"), 0.0f, -1200, 1980);
		UMaterialExpressionScalarParameter* Recipe = Scalar(TEXT("RecipeVersion"), static_cast<float>(CelRecipeVersion), -1200, 2100);

		UMaterialExpressionCustom* Sky = AddExpr<UMaterialExpressionCustom>(Material, 300, 200);
		Sky->Description = TEXT("ProceduralSky");
		Sky->OutputType = CMOT_Float3;
		// Gradient, sun and clouds all in one block.
		//
		// The old sky was painted per vertex on a 64x96 card, which is why the frame had 254
		// rows with a visible step and a hard edge wherever the card ended: a gradient sampled at
		// 96 points is a stack of 96 flat bands, and a plane has an edge. Per-pixel evaluation
		// removes both by construction rather than by tuning.
		//
		// No #define and no helper functions: the material compiler wraps this text inside a
		// generated function body, so only statements and loops survive (learned the hard way on
		// the sea's wave normal, recipe v14).
		// Split across two adjacent literals on purpose: this is a *wide* string (TEXT), and MSVC
// truncates a wide literal at 8192 characters (C2026) - not 16380, which is the narrow limit.
// The scattering block pushed the single literal to 8254 and the build failed with a message
// that reads like a syntax error. Adjacent literals concatenate, so the shader is unchanged.
Sky->Code = TEXT(R"(
float3 d = normalize(-Dir);	// CameraVector points from the pixel towards the eye
float h = d.z;

// ---- vertical gradient ----
// The gradient is compressed into the arc that is actually on screen. At the landscape
// framing the visible sky spans roughly 0 to 15 degrees of elevation, i.e. h in [0, 0.26];
// a gradient that only starts deepening at h = 0.26 therefore arrives as one flat pale blue
// (measured: 0.10 of total variation across the whole sky, 2026-10-18).
// The band that is actually on screen. The two low stops look at 0 to 9 degrees of elevation,
// so a gradient that only deepens above h = 0.26 arrives as one flat pale blue. Horizon to
// mid completes inside 6 degrees and mid to zenith runs from 8 degrees up, which is what a
// real sky does and what puts a visible gradient inside the frame.
// The pale band is *wide* on purpose: a real sky is still hazy at five degrees, and the sea's
// own aerial perspective keeps it pale for about the same arc. A fast ramp here (0.13) put a
// saturated blue three degrees above a near-white sea, which reads as a line even when the two
// colours meet exactly at the horizon.
float3 sky = lerp(Horizon, Mid, smoothstep(0.0, 0.24, h));
sky = lerp(sky, Zenith, smoothstep(0.20, 0.55, h));
sky = lerp(sky, Horizon * 0.93, saturate(-h * 6.0));

// ---- directional scattering ----
//
// The decisive missing term. Every version of this sky so far was a *vertical* gradient:
// colour as a function of elevation and nothing else. A real sky is a lit volume - the side
// the sun is on is brighter and warmer, the opposite side is cooler, and the difference is
// visible across the whole frame, not just around the disc.
//
// Without it the sun could be at the left edge or the right edge and the sky would be
// identical, which is exactly what the frame looked like: two flat colour fields (sky, sea)
// with a model sitting between them. Measured before this change: the sky's brightness across
// 1280 px of width varied by 3% - noise, in other words.
//
// Three lobes, because one cannot do the job: a wide one (around 60 degrees) that lights the
// entire sun half, a medium one that shapes the glow around the disc, and a very tight one
// that the sun term proper takes over from.
float cosA = dot(d, normalize(SunDir));
float mu = saturate(cosA);
sky += SunHalo * pow(mu, 3.0) * 0.09;
sky += SunHalo * pow(mu, 24.0) * 0.15;

// The horizon band. Looking sideways through the atmosphere traverses ~38 times the airmass
// of the zenith, so it is both brighter and warmer than what is above it - which is why a
// photographed sky almost always has a pale, slightly yellow line where it meets the water.
// Ours had a hard edge between two blues instead.
sky = lerp(sky, Horizon + SunHalo * 0.10 * mu, pow(saturate(1.0 - h * 2.6), 4.0) * 0.24);

sky = lerp(sky, SunCore, smoothstep(SunDiscCos, SunDiscCos + 0.00035, cosA));

// ---- horizon haze ------------------------------------------------------
//
// A hard line between water and sky is the single most "rendered" thing in a landscape: in a
// photograph the last degree above the sea is always a washed, slightly warm band, because the
// line of sight grazes the densest air there. Two degrees wide, 55% strength - enough to break
// the edge, not enough to look like fog.
// 0.18, not 0.55, and a wider falloff.
//
// At 0.55 the band had a *lower edge* as well as an upper one: it made the last five degrees of
// sky a paler slab than the sky above it, so the horizon gained a second line instead of losing
// the first. The gradient already ends at the horizon colour; this only has to thicken it.
float hz = pow(saturate(1.0 - h * 6.0), 2.0);
sky = lerp(sky, Horizon * 1.02, hz * 0.18);

// ---- moon ----
float cosM = dot(d, normalize(MoonDirection));
sky += MoonCore * pow(saturate(cosM), 400.0) * MoonAlpha * 0.45;
sky = lerp(sky, MoonCore, smoothstep(MoonDiscCos, MoonDiscCos + 0.00030, cosM) * MoonAlpha);

)") TEXT(R"(
// ---- clouds ----
//
// Two-dimensional value noise, projected onto a horizontal deck with a **soft denominator**.
//
// Two parameterisations were tried and rejected, and the reasons are worth keeping:
//   - `d.xy / h` (pure deck projection) has a pole at the horizon; the noise stretches into
//     long white slivers lying along the sky, and shaping the noise cannot fix a stretch that
//     lives in the coordinate.
//   - sampling 3D value noise at `d * scale` removes the pole but fails here for the opposite
//     reason: the visible sky spans only about 7 degrees of elevation, so d.z moves through
//     0..0.13 while d.xy moves across its whole range. The noise is effectively constant
//     vertically and every cloud becomes a vertical column (measured, 2026-10-18).
//
// The soft denominator was the fix, but 0.30 was far too soft. It capped the deck radius at
// 8.0/4.14 across the whole visible sky - a ratio of 1.9 - so a cloud near the horizon was
// drawn at almost the same size as one overhead. That is not a cloud *layer*, it is a painted
// shell, and the giveaway is the pattern the frame actually showed: radial spokes fanning out
// from the zenith, because the noise was being squashed along the only axis that changed.
//
// A plane-parallel layer at height Hc projects to `d.xy * Hc / h`, i.e. radius ~1/h. 0.10 keeps
// the pole bounded while letting the radius run 4.4x across the visible sky: clouds now shrink
// and foreshorten toward the horizon the way a real deck does, and the spokes are gone.
//
// But 0.10 is also *too* physical for this framing, and that is the interesting part. The
// camera sees 0 to 15 degrees of elevation - a fifteen-degree slice of a projection whose radius
// goes as 1/h. Inside that slice every cloud is squashed into a horizontal streak a few pixels
// tall, which is correct and unusable: a real photograph at this angle only looks right because
// a cumulus is two kilometres thick and subtends tens of degrees on its own. A flat noise sheet
// has no thickness to give.
//
// 0.55 is the compromise: the radius still runs 1.5x across the sky, so there is foreshortening
// and the horizon reads as further away than the zenith, but a cloud keeps enough of its own
// shape to be a shape. It is a stylised deck, not a physical one, and it is the only thing that
// works without marching rays.
//
// `d.xy / (h + c)` keeps the perspective convergence of a real deck and is finite everywhere.
// ---- sky coordinate --------------------------------------------------
//
// Azimuth and elevation, NOT a projected cloud deck.
//
// The deck was `d.xy / (h + 0.55)`, and at this framing the camera sees only 0-15 degrees of
// elevation: the projection converts that into a radial compression of about 4:1, shearing the
// noise lattice into long thin parallelograms that all point at the horizon vanishing point. A
// threshold on a sheared lattice draws its edges, so the clouds came out as straight wedges
// radiating from a point - and the effect got *worse* every time the cloud frequency went up.
// Nothing about the noise, the interpolation or the filtering could fix it, because the shear
// lives in the coordinate.
//
// In (azimuth, elevation) the grid is isotropic, so a level set has no preferred direction. What
// is lost is the deck's perspective convergence; what is gained is a sky that reads as painted
// rather than as projected, which is the register the rest of the frame is in.
//
// The 0.20 on elevation makes a cell about 12 degrees wide and 2.5 tall: wide, low masses, the
// way a cumulus band along a horizon actually sits.
float el = asin(clamp(d.z, -1.0, 1.0));
float az = atan2(d.y, d.x) * 0.15915494;	// turns per revolution, -0.5..0.5
// 0.40, not 0.20.
//
// This is the *aspect* of a noise cell, and it was set when CloudScale was 55 and the sky
// carried about three cells vertically. A cell 1.4 degrees wide and 0.7 tall is a lozenge;
// thresholding a field made of lozenges that large gives back the lozenges, which is what the
// frame showed: half a dozen flat white slabs with straight edges, two hundred pixels across.
// The cells have to be small enough that the *isoline* inside them, not the cell itself, is
// what the eye sees - 0.40 at the current scale puts roughly fifteen cells on the visible
// band, so a cumulus is three or four cells and its border is a curve.
float2 cp0 = float2(az, el * 0.40) * CloudScale + float2(CloudDrift, CloudDrift * 0.37);

// ---- domain warp ------------------------------------------------------
//
// The lattice has to be bent before it is thresholded, or the threshold returns the lattice.
//
// Raising the frequency did exactly that, and the result is the clearest negative in the log:
// at 150 cells per turn the sky became a field of small square puffs on a regular grid. The
// reason is structural - value noise is separable, so its isolines are the lattice's own
// horizontal and vertical lines wherever two cells disagree; with big cells the lattice lines
// were hidden inside a few large blobs, and with small cells they became the clouds.
//
// Displacing the sample point by a *slower* noise field before evaluating the cloud field is
// the standard fix and the only one that works here: the shapes stop being level sets of the
// lattice and become level sets of whatever the warp field says. 0.34 of the base frequency
// and 1.6 cells of amplitude - slower than the field or it just roughens the edges, and far
// enough to close a lattice line into a curve.
float2 wq = cp0 * 0.34 + float2(11.3, 4.7);
float2 wc = floor(wq);
float2 wf = frac(wq);
wf = wf * wf * (3.0 - 2.0 * wf);
float wa = 0.0;
float wb = 0.0;
for (int k = 0; k < 2; k++)
{
    float3 c0 = frac(float3(wc.x,        wc.y,        wc.x) * 0.1031);
    float3 c1 = frac(float3(wc.x + 1.0,  wc.y,        wc.x + 1.0) * 0.1031);
    float3 c2 = frac(float3(wc.x,        wc.y + 1.0,  wc.x) * 0.1031);
    float3 c3 = frac(float3(wc.x + 1.0,  wc.y + 1.0,  wc.x + 1.0) * 0.1031);
    c0 += dot(c0, c0.yzx + 33.33);
    c1 += dot(c1, c1.yzx + 33.33);
    c2 += dot(c2, c2.yzx + 33.33);
    c3 += dot(c3, c3.yzx + 33.33);
    float v0 = frac((c0.x + c0.y) * c0.z);
    float v1 = frac((c1.x + c1.y) * c1.z);
    float v2 = frac((c2.x + c2.y) * c2.z);
    float v3 = frac((c3.x + c3.y) * c3.z);
    float v = lerp(lerp(v0, v1, wf.x), lerp(v2, v3, wf.x), wf.y);
    if (k == 0) { wa = v; }
    else { wb = v; }
    // Integer shift: frac() is unchanged, the hash is not, so the second sample is independent.
    wc += float2(37.0, 19.0);
}
float2 warp = float2(wa, wb) - float2(0.5, 0.5);
// 1.0 cell, not 1.6. At 1.6 the displacement was comparable to a cloud's whole width, so the
// isolines closed into sharp feathered lozenges with crystalline corners - it read as torn
// paper, which is a different wrong look rather than a fix. A warp slightly smaller than the
// cell only has to *bend* the lattice line, not move it to another cell.
float2 cp = cp0 + warp * 1.0;

)") TEXT(R"(
// ---- the cloud field -------------------------------------------------
//
// Four octaves, and the reason is a shape bug that took three rounds to name.
//
// The field used to be three octaves at a base frequency that put the *entire visible sky*
// inside about two lattice cells. A bilinear/quintic patch has a saddle wherever two opposite
// corners are high and the other two low, and the level set of a saddle is two curves that
// cross. With only two cells on screen, one saddle was the whole picture: the sky grew a
// perfectly straight X forty percent of the frame wide, and no amount of softening the
// threshold could help, because a threshold cannot remove a feature of the field itself.
//
// The fix is not "more filter", it is "the large scale must not be one cell". The base
// frequency is raised 2.2x (see CloudScale in SolsticeRealm) so the sky spans six cells, and
// four octaves put detail inside each one, so the level sets are broken curves rather than
// straight ones. The sum is normalised, which matters for every threshold below: without the
// division the mean drifted with the octave count and each retune invalidated the last.
float n = 0.0;
float nLow = 0.0;
float norm = 0.0;
float amp = 1.0;
float2 q = cp;
for (int i = 0; i < 4; i++)
{
    float2 cell = floor(q);
    float2 f = frac(q);
    // Quintic, not smoothstep: smoothstep is C1 but not C2, the second derivative jumps on
    // every lattice line, and a threshold follows those lines. See above - the straight-X
    // problem is the same family of bug.
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    // Hoskins hash. No `sin`: at these coordinates a sine argument loses its fractional
    // precision in float32, the hash collapses to a constant and the mask comes out exactly 0.
    float3 h0 = frac(float3(cell.x,        cell.y,        cell.x) * 0.1031);
    float3 h1 = frac(float3(cell.x + 1.0,  cell.y,        cell.x + 1.0) * 0.1031);
    float3 h2 = frac(float3(cell.x,        cell.y + 1.0,  cell.x) * 0.1031);
    float3 h3 = frac(float3(cell.x + 1.0,  cell.y + 1.0,  cell.x + 1.0) * 0.1031);
    h0 += dot(h0, h0.yzx + 33.33);
    h1 += dot(h1, h1.yzx + 33.33);
    h2 += dot(h2, h2.yzx + 33.33);
    h3 += dot(h3, h3.yzx + 33.33);

    float v0 = frac((h0.x + h0.y) * h0.z);
    float v1 = frac((h1.x + h1.y) * h1.z);
    float v2 = frac((h2.x + h2.y) * h2.z);
    float v3 = frac((h3.x + h3.y) * h3.z);

    float v = lerp(lerp(v0, v1, f.x), lerp(v2, v3, f.x), f.y);
    n += amp * v;
    if (i < 2) { nLow += amp * v; }
    norm += amp;
    // Rotate between octaves: scaling alone keeps them on the same axes and the eye reads the
    // result as a tiled scale pattern.
    q = float2(q.x * 1.62 - q.y * 1.19, q.x * 1.19 + q.y * 1.62);
    amp *= 0.55;
}
n /= norm;
nLow /= 1.55;

)") TEXT(R"(
// ---- sun-facing slope -------------------------------------------------
//
// The same deck sampled a little further along the sun direction. Only the two low octaves are
// used: they carry the shape, so their difference is the slope of the cloud, whereas the fine
// octaves would add a per-pixel difference that has nothing to do with where the light is.
// 0.25 of a cell, and this offset IS the shading: `n - m` is the slope along the light
// direction, so its size sets a cloud's internal contrast. At a twenty-eighth of a cell (which
// is what a unit change left behind) `n - m` is noise, `lit` sits at its midpoint, and every
// cloud renders as one flat white shape.
float2 cpSun = cp + normalize(SunDir.xy + float2(1e-4, 0.0)) * 0.25;
float m = 0.0;
float mNorm = 0.0;
float mAmp = 1.0;
float2 qs = cpSun;
for (int j = 0; j < 2; j++)
{
    float2 cell = floor(qs);
    float2 f = frac(qs);
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float3 h0 = frac(float3(cell.x,        cell.y,        cell.x) * 0.1031);
    float3 h1 = frac(float3(cell.x + 1.0,  cell.y,        cell.x + 1.0) * 0.1031);
    float3 h2 = frac(float3(cell.x,        cell.y + 1.0,  cell.x) * 0.1031);
    float3 h3 = frac(float3(cell.x + 1.0,  cell.y + 1.0,  cell.x + 1.0) * 0.1031);
    h0 += dot(h0, h0.yzx + 33.33);
    h1 += dot(h1, h1.yzx + 33.33);
    h2 += dot(h2, h2.yzx + 33.33);
    h3 += dot(h3, h3.yzx + 33.33);

    float v0 = frac((h0.x + h0.y) * h0.z);
    float v1 = frac((h1.x + h1.y) * h1.z);
    float v2 = frac((h2.x + h2.y) * h2.z);
    float v3 = frac((h3.x + h3.y) * h3.z);

    m += mAmp * lerp(lerp(v0, v1, f.x), lerp(v2, v3, f.x), f.y);
    mNorm += mAmp;
    qs = float2(qs.x * 1.62 - qs.y * 1.19, qs.x * 1.19 + qs.y * 1.62);
    mAmp *= 0.55;
}
m /= mNorm;

// Coverage varies slowly across the sky: the deck forms clusters and gaps rather than an even
// field of identical puffs. Driven by the low-frequency part of the field itself, so a cluster
// is where the field is high - the two stay registered.
float cover = CloudThreshold + (m - 0.5) * 0.30;

// A small per-pixel jitter on the threshold. A threshold turns any residual straightness into a
// crisp edge, and the human eye finds straight lines in sky immediately. One hash, and the
// border becomes ragged the way a cloud border is.
// A per-pixel jitter on this threshold was tried to break the straight seams and measured no
// effect at all - which was the clue that they were a geometry problem, not a threshold one.

// ---- shape from the low octaves, edge from the high ones --------------
//
// A mask cut on the *full* four-octave field shreds along whichever octave happens to have the
// steepest local gradient, and that is almost always the finest one - which is why the deck
// read as torn paper rather than as cloud. A cumulus silhouette is set by its low frequencies;
// the fine octaves belong at the border, where they roughen a boundary that already exists.
//
// So the mask is cut on a field that is 60% the two low octaves and 40% everything, and the
// fine detail is left to `lit` and `body` below, which modulate the interior instead of the
// outline.
// The 1.5 restores the spread the two low octaves alone do not have. Blending down to a
// smoother field also shrank its variance, so the same threshold covered almost nothing - the
// first attempt after this change left five wisps in the whole sky. Rescaling about the mean
// keeps the coverage where CloudThreshold says it is while changing only the *smoothness*.
float nShape = (n * 0.40 + (nLow / 1.55) * 0.60 - 0.5) * 2.0 + 0.5;

// A transition wider than the signal turns the deck into fog - that mistake is written down in
// round 16 and it cost a full rebuild.
// 0.24. 0.18 was tried with the raised frequency and the edge became a cut - a printed sticker
// on the sky. The transition only has to be narrower than the cloud spacing, not as narrow as
// the rim colour: at 0.24 of the field's ~0.55 range an edge is two or three pixels at this
// framing, which is a soft but definite border.
float mask = smoothstep(cover, cover + 0.24, nShape);

// 0 at the rim, 1 in the thickest core. A cloud is not a flat cut-out: the rim is a thin sheet
// and only the core has enough body to shade itself.
float density = saturate((nShape - cover) / 0.30);

// 14, not 10: with the field normalised the low-octave difference is smaller, so the same
// response needs more gain. Clamped inside its range on purpose - the earlier 22 clipped at
// both ends and made every cloud a two-tone stencil.
float lit = saturate((nLow - m) * 14.0 + 0.50);

)") TEXT(R"(
// ---- vertical body -------------------------------------------------
//
// The term that turns a white shape into a cloud with thickness.
//
// `lit` above is the sun-facing slope: it says which side of a cloud the light is on, but
// nothing about where you are *inside* the cloud, so a whole cumulus comes out one value and
// reads as a cut-out however good its rim is.
//
// A cumulus is thicker at the top and thinner underneath, so sampling the same field a little
// higher up the column (a smaller deck radius, i.e. the same d.xy over a larger height) and
// differencing says where this pixel sits: positive means the cloud thins above it, so it is a
// top and catches the light; negative means there is more cloud above it, so it is a base.
float2 cpu = cp * 0.90;
float3 cu0 = frac(float3(floor(cpu).x, floor(cpu).y, floor(cpu).x) * 0.1031);
float3 cu1 = frac(float3(floor(cpu).x + 1.0, floor(cpu).y, floor(cpu).x + 1.0) * 0.1031);
float3 cu2 = frac(float3(floor(cpu).x, floor(cpu).y + 1.0, floor(cpu).x) * 0.1031);
float3 cu3 = frac(float3(floor(cpu).x + 1.0, floor(cpu).y + 1.0, floor(cpu).x + 1.0) * 0.1031);
cu0 += dot(cu0, cu0.yzx + 33.33);
cu1 += dot(cu1, cu1.yzx + 33.33);
cu2 += dot(cu2, cu2.yzx + 33.33);
cu3 += dot(cu3, cu3.yzx + 33.33);
float2 cf = frac(cpu);
cf = cf * cf * cf * (cf * (cf * 6.0 - 15.0) + 10.0);
float nu = lerp(lerp(frac((cu0.x + cu0.y) * cu0.z), frac((cu1.x + cu1.y) * cu1.z), cf.x),
                lerp(frac((cu2.x + cu2.y) * cu2.z), frac((cu3.x + cu3.y) * cu3.z), cf.x), cf.y);

float body = saturate((n - nu) * 18.0 + 0.50);
lit *= lerp(0.46, 1.08, body);

// Thin edges let light through; they read brighter than the body, not darker.
lit = saturate(lit + (1.0 - density) * 0.10);

// Aerial perspective on the deck: clouds near the horizon sit behind several kilometres of
// air and take the sky's colour. Without this the deck is pasted onto the sky as one flat
// white; with it the layer has depth, which is most of what "a sky" means.
//
// `h * 14.0`, not `h * 5.0`, and that exponent was worth the whole cloud layer.
//
// The entry framing sees 0 to 15 degrees of elevation, i.e. h in [0, 0.26]. At `1 - h*5` the
// term only reaches zero at h = 0.20 - inside the frame - so every cloud the camera could
// actually see was blended between 25% and 55% toward the sky colour. The deck measured as a
// set of pale lozenges that were *lighter* than the sky at their cores and had no base at all;
// the reason was not the noise, not the threshold and not the shading, it was this line
// washing out the whole visible band. At `1 - h*14` the term is spent by h = 0.071 (four
// degrees), which is the strip where a deck really does dissolve into haze.
float3 cloudCol = lerp(CloudShadow, CloudLit, lit);
cloudCol = lerp(cloudCol, sky * 1.06, pow(saturate(1.0 - h * 14.0), 2.0) * 0.55);

// Fade the deck out over the last couple of degrees: there a cloud layer is a few pixels tall
// and would only add noise.
float fade = smoothstep(0.012, 0.062, h);
sky = lerp(sky, cloudCol, mask * fade);

return sky;
)");
		{
			auto Add = [&Sky](const TCHAR* Name, UMaterialExpression* Expression)
			{
				FCustomInput Input;
				Input.InputName = Name;
				Input.Input.Expression = Expression;
				Sky->Inputs.Add(Input);
			};
			Add(TEXT("Dir"), Dir);
			Add(TEXT("SunDir"), SunDir);
			Add(TEXT("Horizon"), Horizon);
			Add(TEXT("Mid"), Mid);
			Add(TEXT("Zenith"), Zenith);
			Add(TEXT("SunHalo"), SunHalo);
			Add(TEXT("SunCore"), SunCore);
			Add(TEXT("SunDiscCos"), SunDiscCos);
			Add(TEXT("CloudScale"), CloudScale);
			Add(TEXT("CloudDrift"), Drift);
			Add(TEXT("CloudThreshold"), Threshold);
			Add(TEXT("CloudLit"), CloudLit);
			Add(TEXT("CloudShadow"), CloudShadow);
			Add(TEXT("MoonDirection"), MoonDir);
			Add(TEXT("MoonCore"), MoonCore);
			Add(TEXT("MoonDiscCos"), MoonDiscCos);
			Add(TEXT("MoonAlpha"), MoonAlpha);
		}

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->EmissiveColor.Expression = Sky;
		}

		Material->PostEditChange();
		return SaveAsset(Material, PackageName);
	}

	/**
	 * M_InkHull - the ink for inverted hulls: **one-sided**.
	 *
	 * The hull is a slightly enlarged copy of an object. With a two-sided ink material its front
	 * faces sit in front of the object and simply paint it black (measured: every canopy came out
	 * a brown blob). One-sided + enlarged means only the back faces of the copy survive, and the
	 * only part of them not hidden by the object is the ring outside its silhouette - which is
	 * exactly the outline.
	 *
	 * The standee ink cards keep the two-sided M_Ink: they are flat cards and need both faces.
	 */
	bool BuildInkHull()
	{
		const FString PackageName = TEXT("/Game/Core/M_InkHull");
		UMaterial* Material = FindOrCreateMaterial(PackageName);
		if (!Material)
		{
			return false;
		}

		Material->MaterialDomain = MD_Surface;
		Material->BlendMode = BLEND_Opaque;
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = false;

		ResetGraph(Material);

		UMaterialExpressionConstant3Vector* Ink = AddExpr<UMaterialExpressionConstant3Vector>(Material, -400, 0);
		Ink->Constant = FLinearColor(0.055f, 0.048f, 0.042f, 1.0f);

		UMaterialExpressionScalarParameter* Recipe = AddExpr<UMaterialExpressionScalarParameter>(Material, -400, 200);
		Recipe->ParameterName = TEXT("RecipeVersion");
		Recipe->DefaultValue = static_cast<float>(CelRecipeVersion);

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->EmissiveColor.Expression = Ink;
		}

		Material->PostEditChange();
		return SaveAsset(Material, PackageName);
	}

	bool BuildInk()
	{
		const FString PackageName = TEXT("/Game/Core/M_Ink");
		UMaterial* Material = FindOrCreateMaterial(PackageName);
		if (!Material)
		{
			return false;
		}

		Material->MaterialDomain = MD_Surface;
		Material->BlendMode = BLEND_Opaque;
		Material->SetShadingModel(MSM_Unlit);
		// Two-sided so the printed outline card reads from either side without winding games.
		Material->TwoSided = true;

		ResetGraph(Material);

		UMaterialExpressionConstant3Vector* Ink = AddExpr<UMaterialExpressionConstant3Vector>(Material, -400, 0);
		Ink->Constant = FLinearColor(0.055f, 0.048f, 0.042f, 1.0f);

		UMaterialExpressionScalarParameter* Recipe = AddExpr<UMaterialExpressionScalarParameter>(Material, -400, 200);
		Recipe->ParameterName = TEXT("RecipeVersion");
		Recipe->DefaultValue = static_cast<float>(CelRecipeVersion);

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->EmissiveColor.Expression = Ink;
		}

		Material->PostEditChange();
		return SaveAsset(Material, PackageName);
	}
}

bool FSolsticeCelMaterialBuilder::BuildAll()
{
	bool bOk = true;

	// ---- one rule for all five materials --------------------------------
	//
	// Every master material carries a `RecipeVersion` scalar. An asset whose stamp is older than
	// CelRecipeVersion is rebuilt; anything else is left alone.
	//
	// This used to hold only for M_Cel and M_Sea - M_Sky rebuilt *only when the file was
	// missing*, so a complete rewrite of the sky graph silently kept the old asset on disk and
	// the frame came out pure white from a dome whose material still expected painted vertex
	// colours. Found by measuring the asset: 4882 bytes before and after a 200-line change.
	// A build step that can silently not rebuild is worse than no build step.
	auto NeedsRebuild = [](const TCHAR* PackageName)
	{
		const FString File = FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetAssetPackageExtension());
		if (!FPaths::FileExists(File))
		{
			return true;
		}
		UMaterial* Existing = LoadObject<UMaterial>(nullptr, PackageName);
		if (!Existing)
		{
			return true;
		}
		float Found = -1.0f;
		for (const TObjectPtr<UMaterialExpression>& Expression : Existing->GetExpressionCollection().Expressions)
		{
			if (const UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
			{
				if (Scalar->ParameterName == TEXT("RecipeVersion"))
				{
					Found = Scalar->DefaultValue;
					break;
				}
			}
		}
		return FMath::RoundToInt(Found) != CelRecipeVersion;
	};

	auto Report = [&bOk](const TCHAR* PackageName, bool bBuilt)
	{
		if (bBuilt)
		{
			UE_LOG(LogSolstice, Display, TEXT("generated %s"), PackageName);
		}
		else
		{
			UE_LOG(LogSolstice, Error, TEXT("could not generate %s"), PackageName);
			bOk = false;
		}
	};

	if (NeedsRebuild(TEXT("/Game/Core/M_Cel")))     { Report(TEXT("/Game/Core/M_Cel"), BuildCel()); }
	if (NeedsRebuild(TEXT("/Game/Core/M_Sea")))     { Report(TEXT("/Game/Core/M_Sea"), BuildSea()); }
	if (NeedsRebuild(TEXT("/Game/Core/M_Sky")))     { Report(TEXT("/Game/Core/M_Sky"), BuildSky()); }
	if (NeedsRebuild(TEXT("/Game/Core/M_Ink")))     { Report(TEXT("/Game/Core/M_Ink"), BuildInk()); }
	if (NeedsRebuild(TEXT("/Game/Core/M_InkHull"))) { Report(TEXT("/Game/Core/M_InkHull"), BuildInkHull()); }

	UE_LOG(LogSolstice, Display, TEXT("master materials at recipe v%d"), CelRecipeVersion);

	return bOk;
}
