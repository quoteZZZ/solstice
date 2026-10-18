// Solstice - editor module: the cel master material generator.
//
// The material is built from nodes in code rather than authored in a .uasset, so a fresh clone
// produces exactly the same shader every time. The graph is small and deliberate:
//
//   N       = VertexNormalWS
//   L       = SunDirection          (parameter, pushed by the game each frame)
//   d       = saturate(dot(N, L))
//   t1      = step(BandLow,  d)     <- 0.30
//   t2      = step(BandHigh, d)     <- 0.62
//   shade   = Shadow + t1 * (Mid - Shadow) + t2 * (Lit - Mid)
//   emissive= VertexColor * shade
//
// Three stops, hard edges: d < BandLow is the shadow band, BandLow <= d < BandHigh the middle
// one, above that the lit one. This is the whole cel recipe (docs/art/art.md SS2.1) and it is
// authored here rather than left to the lighting model because a lit surface smooths every band
// it is given - the steps have to be painted, not lit.
//
// Shading model is Unlit on purpose: real-time lights would fight the hard bands, and the
// unlit path also makes the frame independent of eye adaptation, which is what kept washing the
// palette out to white (R-41).

#include "SolsticeEditor.h"

#include "Solstice.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
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
	constexpr int32 CelRecipeVersion = 12;

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

	const FString InkHullPath = FString(FPackageName::LongPackageNameToFilename(TEXT("/Game/Core/M_InkHull"), FPackageName::GetAssetPackageExtension()));
	if (!FPaths::FileExists(InkHullPath))
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
