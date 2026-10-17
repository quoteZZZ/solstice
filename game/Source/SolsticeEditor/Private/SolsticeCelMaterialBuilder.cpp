// Solstice - editor module: the paper master material generator.
//
// The material is built from nodes in code rather than authored in a .uasset, so a fresh clone
// produces exactly the same shader every time. The graph is small and deliberate:
//
//   N       = VertexNormalWS
//   L       = SunDirection          (parameter, pushed by the game each frame)
//   d       = saturate(dot(N, L))
//   band    = clamp(floor(d * 4) / 3, 0, 1)      <- four hard steps, that is the cel look
//   light   = lerp(NightTint, SkyTint, band)
//   emissive= VertexColor * light
//
// Shading model is Unlit on purpose: real-time lights would fight the hard bands, and the whole
// art direction is "flat paper under a definite sun" (docs/art/ART-STYLE.md SS1).

#include "SolsticeEditor.h"

#include "Solstice.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
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

	// ------------------------------------------------------------------
	// Procedural textures. The meshes are vertex-coloured solids, which reads as flat
	// plastic without grain; two generated greyscale sheets - fine fibre and large
	// mottle - multiplied over the vertex colour give the "printed paper stock" feel
	// without any external asset. The value noise is lattice-periodic so both sheets
	// tile seamlessly under world-aligned triplanar projection.
	// ------------------------------------------------------------------

	uint32 Hash2D(int32 X, int32 Y, uint32 Salt)
	{
		uint32 H = static_cast<uint32>(X) * 374761393u + static_cast<uint32>(Y) * 668265263u + Salt;
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}

	float Hash01(int32 X, int32 Y, uint32 Salt)
	{
		return static_cast<float>(Hash2D(X, Y, Salt) & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
	}

	/** Value noise on a torus: lattice corners wrap modulo Period, so the sheet tiles. */
	float PeriodicValueNoise(float X, float Y, int32 Period, uint32 Salt)
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const float FX = X - static_cast<float>(X0);
		const float FY = Y - static_cast<float>(Y0);
		const float SX = FX * FX * (3.0f - 2.0f * FX);
		const float SY = FY * FY * (3.0f - 2.0f * FY);
		const auto Wrap = [Period](int32 V) { return ((V % Period) + Period) % Period; };
		const int32 X1 = Wrap(X0);
		const int32 Y1 = Wrap(Y0);
		const int32 X2 = Wrap(X0 + 1);
		const int32 Y2 = Wrap(Y0 + 1);

		const float N00 = Hash01(X1, Y1, Salt);
		const float N10 = Hash01(X2, Y1, Salt);
		const float N01 = Hash01(X1, Y2, Salt);
		const float N11 = Hash01(X2, Y2, Salt);
		return FMath::Lerp(
			FMath::Lerp(N00, N10, SX),
			FMath::Lerp(N01, N11, SX),
			SY);
	}

	float Fbm(float X, float Y, int32 Period, uint32 Salt, int32 Octaves)
	{
		float Sum = 0.0f;
		float Amplitude = 0.5f;
		float Total = 0.0f;
		int32 Cells = Period;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			Sum += Amplitude * PeriodicValueNoise(X * Cells, Y * Cells, Cells, Salt + Octave * 101u);
			Total += Amplitude;
			Amplitude *= 0.5f;
			Cells *= 2;
		}
		return Sum / Total;
	}

	/**
	 * Author a greyscale texture from a pixel function. PixelGen returns 0..1 for pixel
	 * (X, Y); the coordinates run over a unit square so the same generator tiles at any size.
	 */
	UTexture2D* BuildGreyTexture(const FString& PackageName, int32 Size,
		const TFunction<float(float, float)>& PixelGen, uint32 Salt)
	{
		const FString ShortName = FPackageName::GetShortName(PackageName);
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *(PackageName + TEXT(".") + ShortName)))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}
		UTexture2D* Texture = NewObject<UTexture2D>(Package, *ShortName, RF_Public | RF_Standalone);
		Texture->Source.Init(Size, Size, 1, 1, TSF_G8);
		if (uint8* Mip = Texture->Source.LockMip(0))
		{
			const float Scale = 1.0f / static_cast<float>(Size);
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					const float Value = FMath::Clamp(PixelGen(X * Scale, Y * Scale), 0.0f, 1.0f);
					Mip[Y * Size + X] = static_cast<uint8>(Value * 255.0f + 0.5f);
				}
			}
			Texture->Source.UnlockMip(0);
		}
		Texture->SRGB = true;
		Texture->CompressionSettings = TC_Grayscale;
		Texture->AddressX = TA_Wrap;
		Texture->AddressY = TA_Wrap;
		Texture->NeverStream = true;
		Texture->Filter = TF_Trilinear;
		Texture->PostEditChange();
		FAssetRegistryModule::AssetCreated(Texture);
		SaveAsset(Texture, PackageName);
		return Texture;
	}

	UTexture2D* BuildFibreTexture()
	{
		// Fine directional strokes: variation is slow along X and fast across Y, plus a
		// per-pixel speckle, which is what a sheet of laid paper looks like under a loupe.
		const TFunction<float(float, float)> Gen = [](float U, float V)
		{
			const float Strokes = Fbm(U, V * 6.0f, 8, 0x51EDu, 3);
			const float Speckle = Hash01(
				static_cast<int32>(U * 512.0f),
				static_cast<int32>(V * 512.0f), 0x77A1u) * 0.10f;
			const float Value = 0.94f + (Strokes - 0.5f) * 0.16f + Speckle - 0.05f;
			return Value;
		};
		return BuildGreyTexture(TEXT("/Game/Core/T_CelFibre"), 512, Gen, 1u);
	}

	UTexture2D* BuildMottleTexture()
	{
		// Large-scale stock variation: low-frequency blotches, brighter and darker by a few
		// percent, so big flat areas (water, meadow) stop reading as one uniform fill.
		const TFunction<float(float, float)> Gen = [](float U, float V)
		{
			const float Blotches = Fbm(U, V, 4, 0xB0D4u, 4);
			return 1.0f + (Blotches - 0.5f) * 0.18f;
		};
		return BuildGreyTexture(TEXT("/Game/Core/T_CelMottle"), 256, Gen, 2u);
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
		Material->TwoSided = true;

		Material->GetExpressionCollection().Empty();

		UMaterialExpressionVertexNormalWS* Normal = AddExpr<UMaterialExpressionVertexNormalWS>(Material, -1200, -200);

		UMaterialExpressionVectorParameter* SunDirection = AddExpr<UMaterialExpressionVectorParameter>(Material, -1200, 0);
		SunDirection->ParameterName = TEXT("SunDirection");
		SunDirection->DefaultValue = FLinearColor(0.4f, 0.4f, 0.82f, 0.0f);

		UMaterialExpressionVectorParameter* SkyTint = AddExpr<UMaterialExpressionVectorParameter>(Material, -1200, 260);
		SkyTint->ParameterName = TEXT("SkyTint");
		SkyTint->DefaultValue = FLinearColor(0.92f, 0.88f, 0.80f, 1.0f);

		UMaterialExpressionVectorParameter* NightTint = AddExpr<UMaterialExpressionVectorParameter>(Material, -1200, 460);
		NightTint->ParameterName = TEXT("NightTint");
		NightTint->DefaultValue = FLinearColor(0.24f, 0.25f, 0.30f, 1.0f);

		UMaterialExpressionDotProduct* Dot = AddExpr<UMaterialExpressionDotProduct>(Material, -920, -60);
		Dot->A.Expression = Normal;
		Dot->B.Expression = SunDirection;

		UMaterialExpressionSaturate* Saturate = AddExpr<UMaterialExpressionSaturate>(Material, -720, -60);
		Saturate->Input.Expression = Dot;

		UMaterialExpressionConstant* Bands = AddExpr<UMaterialExpressionConstant>(Material, -720, 120);
		Bands->R = 4.0f;

		UMaterialExpressionMultiply* Scale = AddExpr<UMaterialExpressionMultiply>(Material, -520, -40);
		Scale->A.Expression = Saturate;
		Scale->B.Expression = Bands;
		Scale->ConstB = 4.0f;

		UMaterialExpressionFloor* Floor = AddExpr<UMaterialExpressionFloor>(Material, -340, -40);
		Floor->Input.Expression = Scale;

		// A constant node cannot be divided into directly, so the 1/(Bands-1) step is a second
		// constant and a multiply. Drag the multiply's ConstB if the band count ever changes.
		UMaterialExpressionConstant* InvBands = AddExpr<UMaterialExpressionConstant>(Material, -340, 160);
		InvBands->R = 0.3333333f;

		UMaterialExpressionMultiply* Step = AddExpr<UMaterialExpressionMultiply>(Material, -160, -20);
		Step->A.Expression = Floor;
		Step->B.Expression = InvBands;
		Step->ConstB = 0.3333333f;

		UMaterialExpressionClamp* Clamp = AddExpr<UMaterialExpressionClamp>(Material, 20, -20);
		Clamp->Input.Expression = Step;
		Clamp->MinDefault = 0.0f;
		Clamp->MaxDefault = 1.0f;

		UMaterialExpressionLinearInterpolate* Shade = AddExpr<UMaterialExpressionLinearInterpolate>(Material, 220, 60);
		Shade->A.Expression = NightTint;
		Shade->B.Expression = SkyTint;
		Shade->Alpha.Expression = Clamp;

		UMaterialExpressionVertexColor* VertexColour = AddExpr<UMaterialExpressionVertexColor>(Material, 220, 300);

		UMaterialExpressionMultiply* Result = AddExpr<UMaterialExpressionMultiply>(Material, 440, 120);
		Result->A.Expression = Shade;
		Result->B.Expression = VertexColour;

		if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
		{
			// P2: the paper is LIT now, not self-lit.
			//
			// BaseColor is the *palette only* - the vertex colour. It used to be
			// Result = Shade(bands) x VertexColour, but Shade already paints a hard dark/light
			// ramp from dot(N, sun); feeding that into a lit material means the engine lights the
			// painted shading a second time. That double count is what made the terrain read as
			// crinkled paper with crushed darks, and no amount of light tuning could fix it -
			// turning the key down just darkened both copies together.
			//
			// The band graph is kept in the asset (SkyTint / NightTint / SunDirection are still
			// driven every frame by ApplySky) because it is what a stylised night tint will be
			// hung on when the anime shading pass lands; it just must not be the albedo.
			Material->SetShadingModel(MSM_DefaultLit);
			{
				UMaterialExpressionConstant* Roughness = AddExpr<UMaterialExpressionConstant>(Material, 120, 320);
				Roughness->R = 0.92f;		// matte stock: almost no specular highlight
				EditorData->Roughness.Expression = Roughness;
			}

			// Print grain onto the vertex colour: manual triplanar projection (UE 5.8 dropped
			// the WorldAlignedTexture expression class, so the graph is built from basics).
			// World position divided by the sheet's repeat size feeds three component-masked
			// samples - XY for tops, XZ/YZ for sides - blended by the absolute world normal,
			// then multiplied over the vertex colour. No mesh UVs needed, so every generator
			// mesh - terrain plates, cushions, plinth, clouds - gets the same paper stock at
			// the same world scale.
			UMaterialExpression* Albedo = VertexColour;
			if (UTexture2D* FibreTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Core/T_CelFibre.T_CelFibre")))
			{
				if (UTexture2D* MottleTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Core/T_CelMottle.T_CelMottle")))
				{
					// Shared triplanar weights: |N.x|, |N.y|, |N.z| normalised to sum 1.
					UMaterialExpressionComponentMask* MaskNX = AddExpr<UMaterialExpressionComponentMask>(Material, -1200, 760);
					MaskNX->Input.Expression = Normal;
					MaskNX->R = true;
					UMaterialExpressionComponentMask* MaskNY = AddExpr<UMaterialExpressionComponentMask>(Material, -1200, 880);
					MaskNY->Input.Expression = Normal;
					MaskNY->G = true;
					UMaterialExpressionComponentMask* MaskNZ = AddExpr<UMaterialExpressionComponentMask>(Material, -1200, 1000);
					MaskNZ->Input.Expression = Normal;
					MaskNZ->B = true;
					UMaterialExpressionAbs* WeightX = AddExpr<UMaterialExpressionAbs>(Material, -1020, 760);
					WeightX->Input.Expression = MaskNX;
					UMaterialExpressionAbs* WeightY = AddExpr<UMaterialExpressionAbs>(Material, -1020, 880);
					WeightY->Input.Expression = MaskNY;
					UMaterialExpressionAbs* WeightZ = AddExpr<UMaterialExpressionAbs>(Material, -1020, 1000);
					WeightZ->Input.Expression = MaskNZ;
					UMaterialExpressionAdd* WeightSumXY = AddExpr<UMaterialExpressionAdd>(Material, -840, 920);
					WeightSumXY->A.Expression = WeightX;
					WeightSumXY->B.Expression = WeightY;
					UMaterialExpressionAdd* WeightSum = AddExpr<UMaterialExpressionAdd>(Material, -660, 940);
					WeightSum->A.Expression = WeightSumXY;
					WeightSum->B.Expression = WeightZ;
					UMaterialExpressionDivide* BlendX = AddExpr<UMaterialExpressionDivide>(Material, -480, 760);
					BlendX->A.Expression = WeightX;
					BlendX->B.Expression = WeightSum;
					UMaterialExpressionDivide* BlendY = AddExpr<UMaterialExpressionDivide>(Material, -480, 880);
					BlendY->A.Expression = WeightY;
					BlendY->B.Expression = WeightSum;
					UMaterialExpressionDivide* BlendZ = AddExpr<UMaterialExpressionDivide>(Material, -480, 1000);
					BlendZ->A.Expression = WeightZ;
					BlendZ->B.Expression = WeightSum;

					// One blended triplanar sample per texture sheet.
					const auto SampleTriplanar = [&](UTexture2D* Texture, float RepeatUU, int32 BaseY) -> UMaterialExpression*
					{
						UMaterialExpressionWorldPosition* WorldPos = AddExpr<UMaterialExpressionWorldPosition>(Material, -1200, BaseY);
						UMaterialExpressionMultiply* Scaled = AddExpr<UMaterialExpressionMultiply>(Material, -1020, BaseY);
						Scaled->A.Expression = WorldPos;
						Scaled->ConstB = 1.0f / RepeatUU;

						UMaterialExpressionComponentMask* MaskXY = AddExpr<UMaterialExpressionComponentMask>(Material, -840, BaseY);
						MaskXY->Input.Expression = Scaled;
						MaskXY->R = true;
						MaskXY->G = true;
						UMaterialExpressionComponentMask* MaskXZ = AddExpr<UMaterialExpressionComponentMask>(Material, -840, BaseY + 120);
						MaskXZ->Input.Expression = Scaled;
						MaskXZ->R = true;
						MaskXZ->B = true;
						UMaterialExpressionComponentMask* MaskYZ = AddExpr<UMaterialExpressionComponentMask>(Material, -840, BaseY + 240);
						MaskYZ->Input.Expression = Scaled;
						MaskYZ->G = true;
						MaskYZ->B = true;

						UMaterialExpressionTextureSample* Top = AddExpr<UMaterialExpressionTextureSample>(Material, -660, BaseY);
						Top->Texture = Texture;
						Top->Coordinates.Expression = MaskXY;
						UMaterialExpressionTextureSample* SideA = AddExpr<UMaterialExpressionTextureSample>(Material, -660, BaseY + 120);
						SideA->Texture = Texture;
						SideA->Coordinates.Expression = MaskXZ;
						UMaterialExpressionTextureSample* SideB = AddExpr<UMaterialExpressionTextureSample>(Material, -660, BaseY + 240);
						SideB->Texture = Texture;
						SideB->Coordinates.Expression = MaskYZ;

						UMaterialExpressionLinearInterpolate* Sides = AddExpr<UMaterialExpressionLinearInterpolate>(Material, -300, BaseY + 120);
						Sides->A.Expression = SideA;
						Sides->B.Expression = SideB;
						Sides->Alpha.Expression = BlendY;
						UMaterialExpressionLinearInterpolate* Blended = AddExpr<UMaterialExpressionLinearInterpolate>(Material, -120, BaseY + 60);
						Blended->A.Expression = Sides;
						Blended->B.Expression = Top;
						Blended->Alpha.Expression = BlendZ;
						return Blended;
					};

					UMaterialExpression* Fibre = SampleTriplanar(FibreTex, 48.0f, 1240);		// repeat every 0.48 m
					UMaterialExpression* Mottle = SampleTriplanar(MottleTex, 900.0f, 1660);	// repeat every 9 m

					UMaterialExpressionMultiply* Grain = AddExpr<UMaterialExpressionMultiply>(Material, 120, 1400);
					Grain->A.Expression = Fibre;
					Grain->B.Expression = Mottle;

					UMaterialExpressionMultiply* Printed = AddExpr<UMaterialExpressionMultiply>(Material, 620, 240);
					Printed->A.Expression = VertexColour;
					Printed->B.Expression = Grain;
					Albedo = Printed;
				}
			}
			EditorData->BaseColor.Expression = Albedo;
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

	// Textures first: the material graph hard-references them, so they must exist before
	// BuildCel runs. Both are generated, so a fresh checkout authors the full set.
	const bool bFibreOk = BuildFibreTexture() != nullptr;
	const bool bMottleOk = BuildMottleTexture() != nullptr;
	if (!bFibreOk || !bMottleOk)
	{
		UE_LOG(LogSolstice, Error, TEXT("could not generate paper textures (fibre=%s, mottle=%s)"),
			bFibreOk ? TEXT("ok") : TEXT("fail"), bMottleOk ? TEXT("ok") : TEXT("fail"));
	}

	const bool bCelMissing = !FPaths::FileExists(CelPath);
	const bool bInkMissing = !FPaths::FileExists(InkPath);

	// A paper material from before the grain pass has no triplanar nodes; rebuild it in place
	// so an existing checkout upgrades without anyone deleting uassets by hand.
	bool bCelNeedsRebuild = bCelMissing;
	if (!bCelNeedsRebuild)
	{
		if (UMaterial* Cel = LoadObject<UMaterial>(nullptr, TEXT("/Game/Core/M_Cel.M_Cel")))
		{
			bCelNeedsRebuild = !Cel->GetExpressionCollection().Expressions.ContainsByPredicate(
				[](const TObjectPtr<UMaterialExpression>& Expression)
				{
					// The grain pass is the only thing in the graph that samples a texture.
					return Cast<UMaterialExpressionTextureSample>(Expression) != nullptr;
				});
		}
	}

	if (bCelNeedsRebuild)
	{
		if (BuildCel())
		{
			UE_LOG(LogSolstice, Display, TEXT("generated /Game/Core/M_Cel"));
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

	return bOk;
}
