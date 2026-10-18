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
	constexpr int32 CelRecipeVersion = 49;

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

		Material->GetExpressionCollection().Empty();

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

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->BaseColor.Expression = VertexColour;
			EditorData->Roughness.Expression = Roughness;
			EditorData->Specular.Expression = Specular;
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

		Material->GetExpressionCollection().Empty();

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
		Roughness->DefaultValue = 0.38f;

		UMaterialExpressionScalarParameter* Specular = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 300);
		Specular->ParameterName = TEXT("WaterSpecular");
		Specular->DefaultValue = 0.45f;

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
		WaveScale->DefaultValue = 0.00032f;

		UMaterialExpressionCustom* WaveNormal = AddExpr<UMaterialExpressionCustom>(Material, -900, 760);
		WaveNormal->Description = TEXT("WaveNormal");
		WaveNormal->OutputType = CMOT_Float3;
		// Written out longhand instead of with a #define: the material compiler wraps this text
		// inside a function body, and a macro there was silently dropped (recipe v14 produced a
		// perfectly flat sea).
		WaveNormal->Code = TEXT(R"(
float2 p = P.xy * WaveScaleInput;
float2 g = float2(0.0, 0.0);
float2 d0 = normalize(float2(0.86, 0.51));
float2 d1 = normalize(float2(-0.42, 0.91));
float2 d2 = normalize(float2(0.97, -0.24));
float2 d3 = normalize(float2(0.31, -0.95));
g += d0 * cos(dot(p, d0) * 1.00 * 6.28318 + Time * 0.35) * 1.00;
g += d1 * cos(dot(p, d1) * 1.70 * 6.28318 + Time * 0.52) * 0.94;
g += d2 * cos(dot(p, d2) * 2.90 * 6.28318 + Time * 0.77) * 0.87;
g += d3 * cos(dot(p, d3) * 4.30 * 6.28318 + Time * 1.10) * 0.69;
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
float swell = 0.0;
swell += sin(dot(p, normalize(float2( 0.86,  0.51))) * 1.00 * 6.28318 + T * 0.35) * 1.00;
swell += sin(dot(p, normalize(float2(-0.42,  0.91))) * 1.70 * 6.28318 + T * 0.52) * 0.94;
swell += sin(dot(p, normalize(float2( 0.97, -0.24))) * 2.90 * 6.28318 + T * 0.77) * 0.87;
swell += sin(dot(p, normalize(float2( 0.31, -0.95))) * 4.30 * 6.28318 + T * 1.10) * 0.69;

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
float hn = swell / 3.5 + chop;
// Foam belongs to the swell's crests, not to the ripples riding on them, and it starts low:
// at 0.42 only the rare tallest crest qualified and no white ever reached the frame.
float foam = saturate((swell / 3.5 - 0.24) / 0.38);
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
		Scatter->DefaultValue = 1.0f;

		UMaterialExpressionScalarParameter* Diffuse = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1080);
		Diffuse->ParameterName = TEXT("WaterDiffuse");
		Diffuse->DefaultValue = 0.18f;

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
		FresnelPower->DefaultValue = 9.0f;

		UMaterialExpressionFresnel* SkyRim = AddExpr<UMaterialExpressionFresnel>(Material, -1000, 1380);
		SkyRim->ExponentIn.Expression = FresnelPower;
		SkyRim->BaseReflectFraction = 0.02f;

		UMaterialExpressionScalarParameter* ReflectStrength = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1560);
		ReflectStrength->ParameterName = TEXT("SkyReflectStrength");
		ReflectStrength->DefaultValue = 0.75f;

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
		// 0.40: at 0.55 the foam covered 17.6% of the near sea and the framing midpoint rose to
		// 0.618 against a 0.60 ceiling. Foam is a highlight, not a surface.
		FoamAmount->DefaultValue = 0.32f;

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

		Material->GetExpressionCollection().Empty();

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

// ---- sun ----
float cosA = dot(d, normalize(SunDir));
sky += SunHalo * pow(saturate(cosA), 160.0) * 0.30;
sky = lerp(sky, SunCore, smoothstep(SunDiscCos, SunDiscCos + 0.00035, cosA));

// ---- moon ----
float cosM = dot(d, normalize(MoonDirection));
sky += MoonCore * pow(saturate(cosM), 400.0) * MoonAlpha * 0.45;
sky = lerp(sky, MoonCore, smoothstep(MoonDiscCos, MoonDiscCos + 0.00030, cosM) * MoonAlpha);

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
// `d.xy / (h + c)` keeps the perspective convergence of a real deck and is finite everywhere.
float2 cp = (d.xy / (max(h, 0.0) + 0.30)) * CloudScale + float2(CloudDrift, CloudDrift * 0.37);

// Domain warp before sampling. Value noise lives on a square lattice, and after the deck
// projection those squares become parallelograms: unfiltered, every cloud edge runs in a few
// straight segments and the sky reads as folded paper. Pushing the sample point around with a
// low-frequency sine pair bends the lattice and the edges come out irregular.
float2 warp = float2(sin(cp.y * 0.61 + 1.3), sin(cp.x * 0.83 - 0.4));
cp += warp * 0.55;

float n = 0.0;
float camp = 0.55;
for (int i = 0; i < 3; i++)
{
    float2 cell = floor(cp);
    float2 f = frac(cp);
    f = f * f * (3.0 - 2.0 * f);

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
float2 cp2 = (d.xy / (max(h, 0.0) + 0.30)) * CloudScale + float2(CloudDrift, CloudDrift * 0.37)
	+ normalize(SunDir.xy + float2(1e-4, 0.0)) * 0.19;
float m = 0.0;
float camp2 = 0.55;
for (int j = 0; j < 2; j++)
{
    float2 cell = floor(cp2);
    float2 f = frac(cp2);
    f = f * f * (3.0 - 2.0 * f);

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

// Coverage varies slowly across the sky, so the deck forms clusters of large cloud and gaps of
// small ones instead of an even field of identical puffs. The modulation is gentle: at 0.42 it
// cut the deck into shreds.
float cover = CloudThreshold + (m - 0.42) * 0.22;
float mask = smoothstep(cover, cover + 0.20, n);

// The two samples differ along the sun direction, so their difference is the sun-facing slope.
// Amplified hard: at the noise's own contrast it is a silhouette, not a lit volume.
float lit = saturate((n - m) * 22.0 + 0.46);

// Bases sit in their own shade - a cumulus has a darker underside and a bright top.
lit *= 0.55 + 0.45 * smoothstep(0.0, 0.09, h);

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

		Material->GetExpressionCollection().Empty();

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

		Material->GetExpressionCollection().Empty();

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
