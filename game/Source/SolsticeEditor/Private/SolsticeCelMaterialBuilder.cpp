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
	constexpr int32 CelRecipeVersion = 3;

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
		Material->SetShadingModel(MSM_Unlit);
		// Two-sided: the sky shell is seen from inside, the outline cards from behind.
		Material->TwoSided = true;

		Material->GetExpressionCollection().Empty();

		// ---- inputs -------------------------------------------------------
		UMaterialExpressionVertexNormalWS* Normal = AddExpr<UMaterialExpressionVertexNormalWS>(Material, -1400, -200);

		UMaterialExpressionVectorParameter* SunVector = AddExpr<UMaterialExpressionVectorParameter>(Material, -1400, 0);
		SunVector->ParameterName = TEXT("SunDirection");
		SunVector->DefaultValue = FLinearColor(0.4f, 0.4f, 0.82f, 0.0f);

		UMaterialExpressionVectorParameter* ShadowTint = AddExpr<UMaterialExpressionVectorParameter>(Material, -1400, 220);
		ShadowTint->ParameterName = TEXT("ShadowTint");
		ShadowTint->DefaultValue = FLinearColor(0.34f, 0.38f, 0.52f, 1.0f);

		UMaterialExpressionVectorParameter* MidTint = AddExpr<UMaterialExpressionVectorParameter>(Material, -1400, 400);
		MidTint->ParameterName = TEXT("MidTint");
		MidTint->DefaultValue = FLinearColor(0.78f, 0.78f, 0.78f, 1.0f);

		UMaterialExpressionVectorParameter* LitTint = AddExpr<UMaterialExpressionVectorParameter>(Material, -1400, 580);
		LitTint->ParameterName = TEXT("LitTint");
		LitTint->DefaultValue = FLinearColor(1.0f, 0.98f, 0.94f, 1.0f);

		// Band edges are parameters, not constants: SS2.1 calls 0.30 / 0.62 a starting point that
		// gets retuned at the style gate, and retuning should not mean recompiling C++.
		UMaterialExpressionScalarParameter* BandLow = AddExpr<UMaterialExpressionScalarParameter>(Material, -1400, 760);
		BandLow->ParameterName = TEXT("BandLow");
		BandLow->DefaultValue = 0.30f;

		UMaterialExpressionScalarParameter* BandHigh = AddExpr<UMaterialExpressionScalarParameter>(Material, -1400, 900);
		BandHigh->ParameterName = TEXT("BandHigh");
		BandHigh->DefaultValue = 0.62f;

		UMaterialExpressionScalarParameter* Recipe = AddExpr<UMaterialExpressionScalarParameter>(Material, -1400, 1040);
		Recipe->ParameterName = TEXT("RecipeVersion");
		Recipe->DefaultValue = static_cast<float>(CelRecipeVersion);

		// ---- bands --------------------------------------------------------
		UMaterialExpressionDotProduct* Dot = AddExpr<UMaterialExpressionDotProduct>(Material, -1120, -60);
		Dot->A.Expression = Normal;
		Dot->B.Expression = SunVector;

		UMaterialExpressionSaturate* Facing = AddExpr<UMaterialExpressionSaturate>(Material, -920, -60);
		Facing->Input.Expression = Dot;

		UMaterialExpressionStep* AboveLow = AddExpr<UMaterialExpressionStep>(Material, -720, -120);
		AboveLow->Y.Expression = BandLow;
		AboveLow->X.Expression = Facing;

		UMaterialExpressionStep* AboveHigh = AddExpr<UMaterialExpressionStep>(Material, -720, 40);
		AboveHigh->Y.Expression = BandHigh;
		AboveHigh->X.Expression = Facing;

		UMaterialExpressionSubtract* MidRise = AddExpr<UMaterialExpressionSubtract>(Material, -520, -120);
		MidRise->A.Expression = MidTint;
		MidRise->B.Expression = ShadowTint;

		UMaterialExpressionSubtract* LitRise = AddExpr<UMaterialExpressionSubtract>(Material, -520, 40);
		LitRise->A.Expression = LitTint;
		LitRise->B.Expression = MidTint;

		UMaterialExpressionMultiply* MidStep = AddExpr<UMaterialExpressionMultiply>(Material, -320, -120);
		MidStep->A.Expression = AboveLow;
		MidStep->B.Expression = MidRise;

		UMaterialExpressionMultiply* LitStep = AddExpr<UMaterialExpressionMultiply>(Material, -320, 40);
		LitStep->A.Expression = AboveHigh;
		LitStep->B.Expression = LitRise;

		UMaterialExpressionAdd* ShadeMid = AddExpr<UMaterialExpressionAdd>(Material, -120, -60);
		ShadeMid->A.Expression = ShadowTint;
		ShadeMid->B.Expression = MidStep;

		UMaterialExpressionAdd* Shade = AddExpr<UMaterialExpressionAdd>(Material, 80, -20);
		Shade->A.Expression = ShadeMid;
		Shade->B.Expression = LitStep;

		// ---- albedo -------------------------------------------------------
		UMaterialExpressionVertexColor* VertexColour = AddExpr<UMaterialExpressionVertexColor>(Material, 80, 180);

		UMaterialExpressionMultiply* Result = AddExpr<UMaterialExpressionMultiply>(Material, 300, 60);
		Result->A.Expression = VertexColour;
		Result->B.Expression = Shade;

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			EditorData->EmissiveColor.Expression = Result;
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
