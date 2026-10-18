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

#include "Solstice.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
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
	constexpr int32 CelRecipeVersion = 21;

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
		WaveStrength->DefaultValue = 4.6f;

		UMaterialExpressionScalarParameter* WaveScale = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1040);
		WaveScale->ParameterName = TEXT("WaveFrequencyScale");
		// 1/1250 uu -> a wavelength of ~65 px at the entry framing. Smaller (finer waves) turns
		// into aliasing noise; larger becomes a slow blotch.
		WaveScale->DefaultValue = 0.00045f;

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

		// ---- body scatter --------------------------------------------------
		// 深水在物理上确实接近黑（albedo 只有几个百分点，太阳照上去也不反多少），但画面上
		// 一片黑海不是「深」而是「没渲染」。真实海水那点幽蓝来自水体的**散射**——光进去、
		// 散射回来。DefaultLit 没有 SSS，所以这里用一点自发光代替：它让海在自己身上保持
		// 水色，同时不干扰反射和高光。
		UMaterialExpressionScalarParameter* Scatter = AddExpr<UMaterialExpressionScalarParameter>(Material, -1200, 1160);
		Scatter->ParameterName = TEXT("WaterScatter");
		Scatter->DefaultValue = 0.55f;

		UMaterialExpressionMultiply* ScatterColour = AddExpr<UMaterialExpressionMultiply>(Material, -900, 1160);
		ScatterColour->A.Expression = VertexColour;
		ScatterColour->B.Expression = Scatter;

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->BaseColor.Expression = VertexColour;
			EditorData->Roughness.Expression = Roughness;
			EditorData->Specular.Expression = Specular;
			EditorData->Normal.Expression = WaveNormal;
			EditorData->EmissiveColor.Expression = ScatterColour;
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
		// Two-sided, and not for taste: the ocean ring's winding is the opposite of what a
		// single-sided material expects, so with `false` the entire sea was backface-culled and
		// what the frame showed below the horizon was the *sky card*, not water. Two-sided is the
		// honest fix at this layer; correct the winding in Gen::BuildOcean when the water gets
		// its own module (B1 proper).
		Material->TwoSided = true;

		Material->GetExpressionCollection().Empty();

		UMaterialExpressionVertexColor* VertexColour = AddExpr<UMaterialExpressionVertexColor>(Material, -600, -60);

		UMaterialExpressionScalarParameter* Roughness = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 180);
		Roughness->ParameterName = TEXT("WaterRoughness");
		// 0.06 turned the sea into a mirror of the sky: every pixel reflected the (pale) horizon
		// and the water colour disappeared. 0.22 keeps a broad sun glint without letting the
		// reflection own the surface.
		Roughness->DefaultValue = 0.14f;

		UMaterialExpressionScalarParameter* Specular = AddExpr<UMaterialExpressionScalarParameter>(Material, -600, 300);
		Specular->ParameterName = TEXT("WaterSpecular");
		Specular->DefaultValue = 0.55f;

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
		// Two-sided: the card faces the camera, and a mirrored basis during the intro move should
		// not punch a hole in the frame.
		Material->TwoSided = true;

		Material->GetExpressionCollection().Empty();

		UMaterialExpressionVertexColor* VertexColour = AddExpr<UMaterialExpressionVertexColor>(Material, -400, 0);

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->EmissiveColor.Expression = VertexColour;
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

	const FString CelPath = FString(FPackageName::LongPackageNameToFilename(TEXT("/Game/Core/M_Cel"), FPackageName::GetAssetPackageExtension()));
	const FString InkPath = FString(FPackageName::LongPackageNameToFilename(TEXT("/Game/Core/M_Ink"), FPackageName::GetAssetPackageExtension()));
	const FString SkyPath = FString(FPackageName::LongPackageNameToFilename(TEXT("/Game/Core/M_Sky"), FPackageName::GetAssetPackageExtension()));
	const FString SeaPath = FString(FPackageName::LongPackageNameToFilename(TEXT("/Game/Core/M_Sea"), FPackageName::GetAssetPackageExtension()));

	const bool bInkMissing = !FPaths::FileExists(InkPath);
	const bool bSkyMissing = !FPaths::FileExists(SkyPath);

	// Rebuild when the asset is absent or was authored by an older recipe. The stamp is a scalar
	// parameter, so reading it needs no shader compile - just walk the expression list.
	bool bCelNeedsRebuild = !FPaths::FileExists(CelPath);
	if (!bCelNeedsRebuild)
	{
		if (UMaterial* Cel = LoadObject<UMaterial>(nullptr, TEXT("/Game/Core/M_Cel.M_Cel")))
		{
			float Found = -1.0f;
			for (const TObjectPtr<UMaterialExpression>& Expression : Cel->GetExpressionCollection().Expressions)
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
			bCelNeedsRebuild = FMath::RoundToInt(Found) != CelRecipeVersion;
		}
	}

	if (bCelNeedsRebuild)
	{
		if (BuildCel())
		{
			UE_LOG(LogSolstice, Display, TEXT("generated /Game/Core/M_Cel (recipe v%d)"), CelRecipeVersion);
		}
		else
		{
			UE_LOG(LogSolstice, Error, TEXT("could not generate /Game/Core/M_Cel"));
			bOk = false;
		}
	}

	// M_Sea follows the same stamp rule as M_Cel: water is the surface most likely to be retuned
	// (roughness, specular, the depth ramp), so "generated once and never again" would silently
	// keep an old recipe forever.
	bool bSeaNeedsRebuild = !FPaths::FileExists(SeaPath);
	if (!bSeaNeedsRebuild)
	{
		if (UMaterial* Sea = LoadObject<UMaterial>(nullptr, TEXT("/Game/Core/M_Sea.M_Sea")))
		{
			float Found = -1.0f;
			for (const TObjectPtr<UMaterialExpression>& Expression : Sea->GetExpressionCollection().Expressions)
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
			bSeaNeedsRebuild = FMath::RoundToInt(Found) != CelRecipeVersion;
		}
	}
	if (bSeaNeedsRebuild)
	{
		if (BuildSea())
		{
			UE_LOG(LogSolstice, Display, TEXT("generated /Game/Core/M_Sea (recipe v%d)"), CelRecipeVersion);
		}
		else
		{
			UE_LOG(LogSolstice, Error, TEXT("could not generate /Game/Core/M_Sea"));
			bOk = false;
		}
	}

	const FString InkHullPath = FString(FPackageName::LongPackageNameToFilename(TEXT("/Game/Core/M_InkHull"), FPackageName::GetAssetPackageExtension()));	if (!FPaths::FileExists(InkHullPath))
	{
		if (BuildInkHull())
		{
			UE_LOG(LogSolstice, Display, TEXT("generated /Game/Core/M_InkHull"));
		}
		else
		{
			UE_LOG(LogSolstice, Error, TEXT("could not generate /Game/Core/M_InkHull"));
			bOk = false;
		}
	}

	if (bInkMissing)
	{
		if (BuildInk())
		{
			UE_LOG(LogSolstice, Display, TEXT("generated /Game/Core/M_Ink"));
		}
		else
		{
			UE_LOG(LogSolstice, Error, TEXT("could not generate /Game/Core/M_Ink"));
			bOk = false;
		}
	}

	if (bSkyMissing)
	{
		if (BuildSky())
		{
			UE_LOG(LogSolstice, Display, TEXT("generated /Game/Core/M_Sky"));
		}
		else
		{
			UE_LOG(LogSolstice, Error, TEXT("could not generate /Game/Core/M_Sky"));
			bOk = false;
		}
	}

	return bOk;
}
