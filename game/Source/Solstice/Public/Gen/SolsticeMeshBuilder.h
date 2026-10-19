// Solstice - Gen layer: the triangle-soup accumulator.
//
// Every generator writes into one of these and nothing else. Keeping the accumulation in a
// single flat struct means chunk meshes, props, the paper dome and the plinth all follow the
// same path into a UProceduralMeshComponent, and the outline hull is just a second copy with
// a scale and a colour.
//
// Layer rule (docs/TECH.md SS3): Gen/ may read Sim data, but Sim/ never sees Gen/.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

namespace Solstice
{
	/** One metre in Unreal units. 1 tile = 1 m (docs/WORLD.md SS2.1). */
	constexpr float MeterUU = 100.0f;

	struct FMeshBuilder
	{
		TArray<FVector> Positions;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		/**
		 * Tangent basis, one per vertex.
		 *
		 * Without it UE cannot build a tangent frame, so *any* normal map or tangent-space
		 * normal - including the procedural wave normal the sea needs - silently does nothing.
		 * The sea is a flat sheet, so its tangent is just +X; the ground gets a sensible one too.
		 */
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;

		void Reset()
		{
			Positions.Reset();
			Normals.Reset();
			UVs.Reset();
			Colors.Reset();
			Triangles.Reset();
		}

		int32 Num() const { return Positions.Num(); }
		bool IsEmpty() const { return Triangles.Num() == 0; }

		/**
		 * Add one vertex.
		 *
		 * `Occlusion` is **baked into the colour** (and kept in alpha for anything that wants it).
		 * 1 = open sky, 0.5 = a crevice or the underside of a canopy.
		 *
		 * Why the colour and not a channel the shader reads:
		 *
		 * M_Cel is effectively unlit - measured 2026-10-19, zeroing the sun moves the frame by a
		 * fifth, and the island's value comes almost entirely from its emissive fill, which is
		 * `albedo * band * CelFill`. So albedo *is* the renderer here, and multiplying it by the
		 * occlusion is the only place a shadow can live. The alpha channel was carrying this
		 * value for months and nothing read it; feeding it to the material through a component
		 * mask failed to compile ("not enough components in (float3) for component mask 0001"),
		 * because the compiler had already narrowed the shared vertex-colour expression to RGB.
		 * Baking it in is one line here and needs no shader support at all.
		 */
		int32 Add(const FVector& Position, const FVector& Normal, const FVector2D& UV,
			const FLinearColor& Color, float Occlusion = 1.0f,
			const FVector& Tangent = FVector(1.0f, 0.0f, 0.0f))
		{
			const int32 Index = Positions.Add(Position);
			Normals.Add(Normal);
			UVs.Add(UV);
			const float Lit = FMath::Clamp(Occlusion, 0.0f, 1.0f);
			// A gentle curve, not the raw value: occlusion 0.45 is a deep hollow and should read
			// as roughly half the light, not as less than half of a value that is already dark.
			const float Bake = 0.45f + 0.55f * Lit;
			FLinearColor Stored = Color;
			Stored.R *= Bake;
			Stored.G *= Bake;
			Stored.B *= Bake;
			Stored.A = Lit;
			Colors.Add(Stored);
			Tangents.Add(FProcMeshTangent(Tangent, false));
			return Index;
		}

		void Tri(int32 A, int32 B, int32 C)
		{
			Triangles.Add(A);
			Triangles.Add(B);
			Triangles.Add(C);
		}

		/** Append a quad given four already-added vertex indices, in the order v0 v1 v2 v3. */
		void Quad(int32 A, int32 B, int32 C, int32 D)
		{
			Tri(A, B, C);
			Tri(A, C, D);
		}

		/** Add four corners and emit the quad. Corners run around the rim, not across it. */
		void QuadP(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3,
			const FLinearColor& C0, const FLinearColor& C1, const FLinearColor& C2, const FLinearColor& C3,
			float Occlusion = 1.0f)
		{
			const FVector Normal = FVector::CrossProduct(P1 - P0, P3 - P0).GetSafeNormal();
			const int32 A = Add(P0, Normal, FVector2D(0.0f, 0.0f), C0, Occlusion);
			const int32 B = Add(P1, Normal, FVector2D(1.0f, 0.0f), C1, Occlusion);
			const int32 C = Add(P2, Normal, FVector2D(1.0f, 1.0f), C2, Occlusion);
			const int32 D2 = Add(P3, Normal, FVector2D(0.0f, 1.0f), C3, Occlusion);
			Quad(A, B, C, D2);
		}

		/** A horizontal rectangle at a constant Z. Corners are given in X/Y. */
		void FlatQuad(float X0, float Y0, float X1, float Y1, float Z, const FLinearColor& Color)
		{
			const FVector Normal(0.0f, 0.0f, 1.0f);
			const int32 A = Add(FVector(X0, Y0, Z), Normal, FVector2D(0.0f, 0.0f), Color);
			const int32 B = Add(FVector(X1, Y0, Z), Normal, FVector2D(1.0f, 0.0f), Color);
			const int32 C = Add(FVector(X1, Y1, Z), Normal, FVector2D(1.0f, 1.0f), Color);
			const int32 D = Add(FVector(X0, Y1, Z), Normal, FVector2D(0.0f, 1.0f), Color);
			Quad(A, B, C, D);
		}

		/** A vertical ribbon between two rim points, spanning Z0..Z1. Used for paper cut edges. */
		void Wall(const FVector& A, const FVector& B, float Z0, float Z1,
			const FVector& Normal, const FLinearColor& BottomColor, const FLinearColor& TopColor,
			float BottomOcclusion = 1.0f, float TopOcclusion = 1.0f)
		{
			const int32 I0 = Add(FVector(A.X, A.Y, Z0), Normal, FVector2D(0.0f, 0.0f), BottomColor, BottomOcclusion);
			const int32 I1 = Add(FVector(B.X, B.Y, Z0), Normal, FVector2D(1.0f, 0.0f), BottomColor, BottomOcclusion);
			const int32 I2 = Add(FVector(B.X, B.Y, Z1), Normal, FVector2D(1.0f, 1.0f), TopColor, TopOcclusion);
			const int32 I3 = Add(FVector(A.X, A.Y, Z1), Normal, FVector2D(0.0f, 1.0f), TopColor, TopOcclusion);
			Quad(I0, I1, I2, I3);
		}

		/** Push the accumulated soup into a procedural mesh section. */
		void PushTo(UProceduralMeshComponent* Component, int32 Section, bool bCreateCollision) const
		{
			if (!Component)
			{
				return;
			}
			if (Triangles.Num() == 0)
			{
				Component->ClearMeshSection(Section);
				return;
			}
			// Empty is legal (UE then derives a frame), but a supplied basis is what makes the
			// sea's normal map possible at all.
			Component->CreateMeshSection_LinearColor(Section, Positions, Triangles, Normals, UVs, Colors, Tangents, bCreateCollision);
		}

		static void ClearSection(UProceduralMeshComponent* Component, int32 Section)
		{
			if (Component && Component->GetNumSections() > Section)
			{
				Component->ClearMeshSection(Section);
			}
		}

		/**
		 * Append a transformed copy of another builder. The printed outline card behind a standee
		 * is exactly this call: same shape, a little larger, flat ink colour.
		 */
		void AppendTransformed(const FMeshBuilder& Other, const FTransform& Transform, float Scale,
			bool bOverrideColor, const FLinearColor& FlatColor)
		{
			if (Other.IsEmpty())
			{
				return;
			}
			const int32 Base = Positions.Num();
			for (int32 Index = 0; Index < Other.Positions.Num(); ++Index)
			{
				Positions.Add(Transform.TransformPosition(Other.Positions[Index] * Scale));
				Normals.Add(Transform.TransformVectorNoScale(Other.Normals[Index]).GetSafeNormal());
				UVs.Add(Other.UVs[Index]);
				Colors.Add(bOverrideColor ? FlatColor : Other.Colors[Index]);
			}
			for (int32 Index = 0; Index < Other.Triangles.Num(); ++Index)
			{
				Triangles.Add(Base + Other.Triangles[Index]);
			}
		}
	};
}
