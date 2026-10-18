// Solstice - Gen layer: the triangle-soup accumulator.
//
// Every generator writes into one of these and nothing else. Keeping the accumulation in a
// single flat struct means chunk meshes, props, the paper dome and the plinth all follow the
// same path into a UProceduralMeshComponent, and the outline hull is just a second copy with
// a scale and a colour.
//
// Layer rule (docs/tech/STRUCTURE.md SS3): Gen/ may read Sim data, but Sim/ never sees Gen/.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

namespace Solstice
{
	/** One metre in Unreal units. 1 tile = 1 m (docs/setting/WORLD.md SS2.1). */
	constexpr float MeterUU = 100.0f;

	struct FMeshBuilder
	{
		TArray<FVector> Positions;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
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
		 * `Occlusion` rides in the vertex colour's **alpha** channel: the cel material multiplies the
		 * shading bands by it, which is how the world gets contact darkening without a shadow pass
		 * (M_Cel is unlit, so a real shadow map never reaches the surface). 1 = open sky, 0.5 = a
		 * crevice or the underside of a canopy.
		 */
		int32 Add(const FVector& Position, const FVector& Normal, const FVector2D& UV,
			const FLinearColor& Color, float Occlusion = 1.0f)
		{
			const int32 Index = Positions.Add(Position);
			Normals.Add(Normal);
			UVs.Add(UV);
			FLinearColor Stored = Color;
			Stored.A = FMath::Clamp(Occlusion, 0.0f, 1.0f);
			Colors.Add(Stored);
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
			static const TArray<FProcMeshTangent> NoTangents;
			Component->CreateMeshSection_LinearColor(Section, Positions, Triangles, Normals, UVs, Colors, NoTangents, bCreateCollision);
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
