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
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTime.h"
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
	constexpr int32 CelRecipeVersion = 79;

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

		UMaterialExpressionCustom* GroundDetail = AddExpr<UMaterialExpressionCustom>(Material, -900, 640);
		GroundDetail->Description = TEXT("GroundDetail");
		GroundDetail->OutputType = CMOT_Float3;
		GroundDetail->Code = TEXT(R"(
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
return lerp(Damp, Dry, n);
)");
		{
			FCustomInput Input;
			Input.InputName = TEXT("P");
			Input.Input.Expression = DetailPos;
			GroundDetail->Inputs.Add(Input);
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
		}

		UMaterialExpressionMultiply* DetailedBase = AddExpr<UMaterialExpressionMultiply>(Material, -200, -60);
		DetailedBase->A.Expression = TintedBase;
		DetailedBase->B.Expression = DetailBlend;

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->BaseColor.Expression = DetailedBase;
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

// ---- domain warp ---------------------------------------------------
//
// Five plane waves that never stop being parallel are the reason this sea reads as brushed
// cloth. Amplitude was never the problem: the crests are straight lines, perspective compresses
// straight lines into horizontal bars, and no weighting of a straight line makes it a wave.
//
// So the coordinate is warped before the waves are evaluated. Two sines at different
// frequencies in x and y produce a non-repeating warp field, so the crests bend and pinch and
// the interference pattern stops having an axis. Cost: four extra sin() per pixel.
float2 warp = float2(sin(p.y * 1.7 + Time * 0.21), sin(p.x * 1.3 - Time * 0.17)) * 1.10;
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
g += d2 * cos(dot(pw, d2) * 1.70 * 6.28318 + Time * 0.52) * 0.94;
g += d3 * cos(dot(pw, d3) * 2.90 * 6.28318 + Time * 0.77) * 0.87;
g += d4 * cos(dot(pw, d4) * 4.30 * 6.28318 + Time * 1.10) * 0.69;
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
g += mg * 1.5;

return normalize(float3(-g * WaveStrengthInput, 1.0));
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
			WaveNormal->Inputs.Add(InPos);
			WaveNormal->Inputs.Add(InTime);
			WaveNormal->Inputs.Add(InStrength);
			WaveNormal->Inputs.Add(InScale);
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
float2 q = p * 3.1 + float2(T * 0.9, T * 0.35);
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
float hn = swell * 0.28 + chop * 1.25;
// Foam belongs to the swell's crests, not to the ripples riding on them, and it starts low:
// at 0.42 only the rare tallest crest qualified and no white ever reached the frame.
float foam = saturate((swell - 0.26) / 0.34);
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
			WaveDetail->Inputs.Add(InPos);
			WaveDetail->Inputs.Add(InTime);
			WaveDetail->Inputs.Add(InScale);
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

		UMaterialExpressionMultiply* TintedBase = AddExpr<UMaterialExpressionMultiply>(Material, -420, -60);
		TintedBase->A.Expression = VertexColour;
		TintedBase->B.Expression = SeaTint;

		// ---- 水体色：走自发光，不走光照 --------------------------------
		//
		// 这是风格化水面和 PBR 水面最重要的一处分歧。PBR 的水色 = albedo × 光照，而深水
		// albedo 只有几个百分点，9 lux 的太阳一照就压到 0（实测下半屏 R = 0.000），再把
		// 太阳调亮就整片过曝成荧光青（实测 (0.29, 0.89, 0.94)，饱和度 p90 = 1.0）。
		// 两头都不对，因为海的颜色本来就不是「被照亮的蓝」。
		//
		// 所以：色板里的 lagoon / shelf / deep 直接作为自发光落到屏幕上，漫反射只剩 18%
		// 用来接收一点日照的明暗；高光（sun glint）和菲涅尔天空反射照旧叠加在上面。
		// 这样海的颜色就恒等于色板，和岛的受光色不再互相拉扯。
		UMaterialExpressionScalarParameter* Scatter = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1160);
		Scatter->ParameterName = TEXT("WaterGlow");
		// 0.62, not 0.80.
		//
		// Measured on the frame: the sea's mean luminance was 0.58 against the sky's 0.63 - a
		// 5% separation across the 55% of the picture the water occupies. Nothing in a landscape
		// scene is that close in value, and the effect is unmistakable: the horizon line stops
		// reading, the island has no ground to stand on, and the whole frame flattens into two
		// similar pale blues. Real sea sits well under the sky it reflects, because what the eye
		// reads at this angle is the water column, not the surface.
		Scatter->DefaultValue = 0.60f;

		UMaterialExpressionScalarParameter* Diffuse = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1080);
		Diffuse->ParameterName = TEXT("WaterDiffuse");
		// 0.05, not 0.18.
		//
		// The lit part of the sea is not there to light it - the body colour is emissive and stays
		// exactly as authored - it is there so the surface still responds to the sun at all. At
		// 0.18 and a 560-unit sun the lit term overwhelmed the authored colour and the whole sea
		// went cyan-white (measured 2026-10-18). 0.05 keeps a little directional life and lets the
		// palette own the water.
		//
		// Note what this means for shadows: an emissive surface cannot receive one. Island
		// shadows will not land on the water in this style, and that is a deliberate trade - the
		// alternative is a lit sea whose colour changes with the hour and blows out at noon.
		Diffuse->DefaultValue = 0.05f;

		UMaterialExpressionMultiply* ScatterColour = AddExpr<UMaterialExpressionMultiply>(Material, -900, 1160);
		ScatterColour->A.Expression = TintedBase;
		ScatterColour->B.Expression = Scatter;

		UMaterialExpressionMultiply* DiffuseBase = AddExpr<UMaterialExpressionMultiply>(Material, -780, 1000);
		DiffuseBase->A.Expression = TintedBase;
		DiffuseBase->B.Expression = Diffuse;

		// ---- sky reflection (Fresnel) --------------------------------------
		// 海水最缺的一项，也是「下半屏 R = 0」的成因。
		//
		// 真实水面在掠射角下几乎不显示水的颜色，显示的是它反射的天空（Fresnel 趋近 1）。
		// 没有这一项时，远处的水只会随距离变暗变青，红通道被压到 0 —— 实测下半屏 46% 的
		// 行 R = 0.000，整片纯数字青，「廉价感」就来自这里。
		//
		// 有了它：俯视处仍是水的颜色，掠射处自然过渡成地平线的天色，于是海与天在水平线上
		// 是同一个颜色，接缝不需要另外去对。
		UMaterialExpressionVectorParameter* SkyReflect = AddExpr<UMaterialExpressionVectorParameter>(Material, -1200, 1320);
		SkyReflect->ParameterName = TEXT("SkyReflect");
		SkyReflect->DefaultValue = FLinearColor(0.42f, 0.62f, 0.80f, 1.0f);

		UMaterialExpressionScalarParameter* FresnelPower = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1440);
		FresnelPower->ParameterName = TEXT("SkyReflectPower");
		// 9, not 3.2.
		//
		// At this camera the sea sits between 3 and 30 degrees below the eye, i.e. dot(N, V) is
		// 0.05..0.5. A physical Fresnel (exponent ~3) is already at 30-80% reflection across that
		// entire band, so the whole sea collapses to the sky's colour and there is no water left -
		// measured: the near sea and the far sea came out within 0.05 of each other. Stylised
		// water uses a much steeper curve so the pale reflection is confined to the last few
		// degrees above the horizon, where it belongs and where it hides the seam.
				// 6, not 9.
		//
		// Structure of the problem, measured on the frame: perspective compresses the visible
		// near water into T = 0.154..0.210 of the ocean mesh's ring parameter - a 0.06 window.
		// Every colour the palette can produce is squeezed through that window, so the whole
		// lower third of the picture came out one flat #2E93B8 no matter what the palette said.
		//
		// The colour cannot vary with radius here, so it has to vary with *angle*, which is what
		// Fresnel already does: near the camera the eye looks steeply down and sees the water
		// column, far away it looks along the surface and sees the sky. At exponent 9 that
		// transition is squeezed into the last two degrees and the frame never shows it. At 6 the
		// band is wide enough to run across the near sea, and the surface gains a front-to-back
		// gradient instead of being a fill.
		// 7.2: 9 was a two-degree band that never reached the frame, 6 covered so much surface
		// that the sea turned grey (measured: saturation 0.305 -> 0.272). The usable window is
		// between those two, and it is narrow.
		FresnelPower->DefaultValue = 7.2f;

		UMaterialExpressionFresnel* SkyRim = AddExpr<UMaterialExpressionFresnel>(Material, -1000, 1380);
		SkyRim->ExponentIn.Expression = FresnelPower;
		SkyRim->BaseReflectFraction = 0.02f;

		UMaterialExpressionScalarParameter* ReflectStrength = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1560);
		ReflectStrength->ParameterName = TEXT("SkyReflectStrength");
		// 0.50, not 0.75: the reflected horizon is near-white, and at 0.75 the grazing band along
		// the waterline was brighter than the sky it was reflecting.
				// 0.42, with the wider Fresnel band above: the reflection now covers a much larger part
		// of the surface, so each unit of it has to be worth less or the sea becomes sky-coloured.
		ReflectStrength->DefaultValue = 0.42f;

		UMaterialExpressionMultiply* ReflectTerm = AddExpr<UMaterialExpressionMultiply>(Material, -780, 1420);
		ReflectTerm->A.Expression = SkyRim;
		ReflectTerm->B.Expression = ReflectStrength;

		// Emissive carries two things the lit base colour cannot: the water body's own scatter
		// (deep water is physically almost black, which reads as "not rendered") and the sky it
		// mirrors at grazing angles.
		// **Mix**, do not add.
		//
		// Adding the sky reflection to the water colour saturates instantly: base (0.03, 0.32,
		// 0.54) + reflection (0.15, 0.28, 0.40) = (0.18, 0.60, 0.94), which encodes as
		// (0.46, 0.80, 0.97) - the fluorescent cyan measured on the whole lower half. Water at a
		// grazing angle is not "water plus sky", it is *mostly sky*; a lerp says that and stays
		// inside the two authored colours.
		UMaterialExpressionMultiply* ReflectMix = AddExpr<UMaterialExpressionMultiply>(Material, -620, 1300);
		ReflectMix->A.Expression = SkyRim;
		ReflectMix->B.Expression = ReflectStrength;

		// ---- crest shading + foam -----------------------------------------
		// Crests catch more light and stand higher out of the water; troughs sit in the body's
		// own shade. A single multiply is enough because the water colour is authored, not lit:
		// this only has to give the surface a gradient to read.
		UMaterialExpressionScalarParameter* ShadeAmp = AddExpr<UMaterialExpressionScalarParameter>(Material, -700, 1020);
		ShadeAmp->ParameterName = TEXT("WaveShadeAmount");
		ShadeAmp->DefaultValue = 0.19f;

		UMaterialExpressionMultiply* ShadeOffset = AddExpr<UMaterialExpressionMultiply>(Material, -520, 1020);
		ShadeOffset->A.Expression = WaveHeight;
		ShadeOffset->B.Expression = ShadeAmp;

		UMaterialExpressionConstant* OneShade = AddExpr<UMaterialExpressionConstant>(Material, -520, 1140);
		OneShade->R = 1.0f;

		// At grazing angles the water is a mirror and its colour comes from the sky, so wave
		// shading must fade out there or the far sea gets stripes of its own body colour.
		UMaterialExpressionOneMinus* ShadeFacing = AddExpr<UMaterialExpressionOneMinus>(Material, -520, 1060);
		ShadeFacing->Input.Expression = SkyRim;

		UMaterialExpressionMultiply* ShadeGated = AddExpr<UMaterialExpressionMultiply>(Material, -380, 1020);
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
		// 0.15, not 0.32.
		//
		// On the entry framing the near sea is most of the lower half, and at 0.32 the foam read
		// as *white horizontal banding across the whole surface* rather than as breaking crests -
		// the single biggest reason the sea looked like painted silk instead of water. Foam is a
		// punctuation mark: a few crests, not a texture.
		FoamAmount->DefaultValue = 0.15f;

		// Foam only belongs where the surface faces the eye. At grazing angles the water is a
		// mirror and any white on it would read as a stripe of paint lying on the horizon.
		UMaterialExpressionOneMinus* FacingEye = AddExpr<UMaterialExpressionOneMinus>(Material, -520, 1380);
		FacingEye->Input.Expression = SkyRim;

		UMaterialExpressionMultiply* FoamGate = AddExpr<UMaterialExpressionMultiply>(Material, -380, 1300);
		FoamGate->A.Expression = WaveFoam;
		FoamGate->B.Expression = FacingEye;

		UMaterialExpressionMultiply* FoamAlpha = AddExpr<UMaterialExpressionMultiply>(Material, -240, 1300);
		FoamAlpha->A.Expression = FoamGate;
		FoamAlpha->B.Expression = FoamAmount;

		UMaterialExpressionLinearInterpolate* BodyWithFoam = AddExpr<UMaterialExpressionLinearInterpolate>(Material, -80, 1180);
		BodyWithFoam->A.Expression = ShadedBody;
		BodyWithFoam->B.Expression = FoamColour;
		BodyWithFoam->Alpha.Expression = FoamAlpha;

		UMaterialExpressionLinearInterpolate* EmissiveTotal = AddExpr<UMaterialExpressionLinearInterpolate>(Material, -420, 1240);
		EmissiveTotal->A.Expression = BodyWithFoam;
		EmissiveTotal->B.Expression = SkyReflect;
		EmissiveTotal->Alpha.Expression = ReflectMix;

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->BaseColor.Expression = DiffuseBase;
			EditorData->Roughness.Expression = Roughness;
			EditorData->Specular.Expression = Specular;
			EditorData->Normal.Expression = WaveNormal;
			EditorData->EmissiveColor.Expression = EmissiveTotal;
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
		// WorldPosition minus CameraPosition, not the vertex normal: the dome is a unit sphere
		// scaled to 400 km and re-centred on the camera every frame, so this is exactly the
		// direction the eye is looking, whatever the winding or the two-sided normal flip did.
		UMaterialExpressionWorldPosition* WorldPos = AddExpr<UMaterialExpressionWorldPosition>(Material, -1600, -300);
		UMaterialExpressionCameraPositionWS* CameraPos = AddExpr<UMaterialExpressionCameraPositionWS>(Material, -1600, -160);
		UMaterialExpressionSubtract* Rel = AddExpr<UMaterialExpressionSubtract>(Material, -1380, -240);
		Rel->A.Expression = WorldPos;
		Rel->B.Expression = CameraPos;
		UMaterialExpressionNormalize* Dir = AddExpr<UMaterialExpressionNormalize>(Material, -1200, -240);
		Dir->VectorInput.Expression = Rel;

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
float3 d = normalize(Dir);
float h = d.z;

// ---- vertical gradient ----
// The gradient is compressed into the arc that is actually on screen. At the landscape
// framing the visible sky spans roughly 0 to 15 degrees of elevation, i.e. h in [0, 0.26];
// a gradient that only starts deepening at h = 0.26 therefore arrives as one flat pale blue
// (measured: 0.10 of total variation across the whole sky, 2026-10-18).
float3 sky = lerp(Horizon, Mid, smoothstep(0.0, 0.075, h));
sky = lerp(sky, Zenith, smoothstep(0.055, 0.40, h));
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
float2 cp = (d.xy / (max(h, 0.0) + 0.55)) * CloudScale + float2(CloudDrift, CloudDrift * 0.37);

// Domain warp before sampling. Value noise lives on a square lattice, and after the deck
// projection those squares become parallelograms: unfiltered, every cloud edge runs in a few
// straight segments and the sky reads as folded paper. Pushing the sample point around with a
// low-frequency sine pair bends the lattice and the edges come out irregular.
// The warp has to be *noise*, not any arrangement of sines.
//
// Two rounds were spent on sines and both left parallel strokes across the sky, because
// sin(a*y) * sin(b*x) is 0.5*(cos(ay-bx) - cos(ay+bx)) - it is still two plane waves, just
// rotated onto the diagonals. Any sum of sinusoids has an axis, and any axis shows up as
// streaks once the result is thresholded.
//
// One octave of value noise has no axis at all. Four hashes and a quintic lerp buy a warp field
// that bends the lattice differently in every direction, and the streaks go with it.
float2 wq = cp * 0.45;
float2 wc = floor(wq);
float2 wf = frac(wq);
wf = wf * wf * wf * (wf * (wf * 6.0 - 15.0) + 10.0);

float3 wa = frac(float3(wc.x,        wc.y,        wc.x) * 0.1031);
float3 wb = frac(float3(wc.x + 1.0,  wc.y,        wc.x + 1.0) * 0.1031);
float3 wd = frac(float3(wc.x,        wc.y + 1.0,  wc.x) * 0.1031);
float3 we = frac(float3(wc.x + 1.0,  wc.y + 1.0,  wc.x + 1.0) * 0.1031);
wa += dot(wa, wa.yzx + 33.33);
wb += dot(wb, wb.yzx + 33.33);
wd += dot(wd, wd.yzx + 33.33);
we += dot(we, we.yzx + 33.33);

float2 oa = float2(frac((wa.x + wa.y) * wa.z), frac((wa.x + wa.z) * wa.y)) - 0.5;
float2 ob = float2(frac((wb.x + wb.y) * wb.z), frac((wb.x + wb.z) * wb.y)) - 0.5;
float2 od = float2(frac((wd.x + wd.y) * wd.z), frac((wd.x + wd.z) * wd.y)) - 0.5;
float2 oe = float2(frac((we.x + we.y) * we.z), frac((we.x + we.z) * we.y)) - 0.5;

cp += 1.6 * lerp(lerp(oa, ob, wf.x), lerp(od, oe, wf.x), wf.y);

float2 cpBase = cp;

float n = 0.0;
float camp = 0.55;
for (int i = 0; i < 3; i++)
{
    float2 cell = floor(cp);
    float2 f = frac(cp);
    // Quintic, not smoothstep.
    //
    // smoothstep interpolation of a value-noise lattice is C1 but not C2: the second derivative
    // jumps at every lattice line, and along those lines the *gradient* of the field is
    // discontinuous. A threshold on such a field produces edges that snap to the lattice, so a
    // cloud larger than a cell comes out as a tilted quadrilateral - measured on the frame as
    // long straight seams running diagonally across the sky, worse than the folded-paper look
    // this warp was added to fix. Quintic is C2, the lattice vanishes, and the cells cost the
    // same.
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    // Hoskins hash. No `sin`: at these coordinates a sine argument loses its fractional
    // precision in float32, the hash collapses to a constant and the cloud mask comes out
    // exactly 0 over the whole sky.
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

    n += camp * lerp(lerp(v0, v1, f.x), lerp(v2, v3, f.x), f.y);
    // Rotate between octaves: scaling alone keeps them on the same axes and the eye reads
    // the result as a tiled scale pattern.
    cp = float2(cp.x * 1.66 - cp.y * 1.24, cp.x * 1.24 + cp.y * 1.66);
    camp *= 0.5;
}

// Sun-facing slope: the same field sampled a little further along the sun direction.
float2 cp2 = (d.xy / (max(h, 0.0) + 0.55)) * CloudScale + float2(CloudDrift, CloudDrift * 0.37)
	+ normalize(SunDir.xy + float2(1e-4, 0.0)) * 0.19;
float m = 0.0;
float camp2 = 0.55;
for (int j = 0; j < 2; j++)
{
    float2 cell = floor(cp2);
    float2 f = frac(cp2);
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

    m += camp2 * lerp(lerp(v0, v1, f.x), lerp(v2, v3, f.x), f.y);
    cp2 = float2(cp2.x * 1.66 - cp2.y * 1.24, cp2.x * 1.24 + cp2.y * 1.66);
    camp2 *= 0.5;
}

)") TEXT(R"(
// Coverage varies slowly across the sky, so the deck forms clusters of large cloud and gaps of
// small ones instead of an even field of identical puffs. The modulation is gentle: at 0.42 it
// cut the deck into shreds.
float cover = CloudThreshold + (m - 0.42) * 0.22;
// 0.24, not 0.20 - and never wider than the noise itself.
//
// The band is measured in noise units and the three-octave field only spans about 0.30 of them
// in practice. A first attempt at 0.34 was wider than the whole signal, so `mask` could never
// reach 1: every cloud became a translucent sheet and the sky turned into fog. 0.24 softens the
// rim without capping coverage.
// 0.28: 0.24 still left visible corners on the larger clouds. The ceiling is the noise's own
// dynamic range (~0.30); wider than that and `mask` can never reach 1, which turns the deck
// into fog - that mistake is recorded above and it cost a full round.
float mask = smoothstep(cover, cover + 0.28, n);

// How far past the threshold this pixel is, 0 at the rim and 1 in the thickest core. A cloud is
// not a flat cut-out: the rim is a thin sheet you can nearly see through, and only the core has
// enough body to cast its own shade. Without this the whole silhouette gets the same internal
// contrast and reads as painted cardboard.
float density = saturate((n - cover) / 0.30);

// The two samples differ along the sun direction, so their difference is the sun-facing slope.
// Amplified hard: at the noise's own contrast it is a silhouette, not a lit volume.
// 10, not 22.
//
// At 22 the slope term was clipped at both ends across most of every cloud, so the lit result
// was effectively binary: one flat white body and one flat grey rim, with the middle of the
// range never appearing. The eye reads that as a stencil. 10 keeps the term inside its range so
// the body has a gradient from the sunward side to the shadowed one.
float lit = saturate((n - m) * 10.0 + 0.48);

)") TEXT(R"(
// ---- vertical body -------------------------------------------------
//
// The term that turns a white shape into a cloud with thickness.
//
// `lit` above is the sun-facing slope: it says which side of a cloud the light is on, but it
// says nothing about where you are *inside* the cloud, so a whole cumulus comes out one value
// and reads as a cut-out, however good its rim is.
//
// A cumulus is thicker at the top and thinner underneath, so sampling the same field a little
// higher (a slightly smaller deck radius, i.e. the same d.xy divided by a larger height) and
// differencing tells you where this pixel sits in the column: positive means the cloud thins
// above it, so this is the top and it catches the light; negative means there is more cloud
// above it, so this is the shaded base.
float2 cpu = cpBase * 0.92;
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

float body = saturate((n - nu) * 16.0 + 0.50);
lit *= lerp(0.52, 1.06, body);

// Thin edges let light through; they should read brighter than the body, not darker. This is the
// second half of the volume cue - the first is the internal shade above.
lit = saturate(lit + (1.0 - density) * 0.14);

float3 cloudCol = lerp(CloudShadow, CloudLit, lit);

// Fade the deck out over the last couple of degrees: at the horizon a cloud layer is a few
// pixels tall and would only add noise.
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
