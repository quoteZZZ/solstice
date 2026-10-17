// Solstice - God layer: the observer.
//
// The player is not in the realm; the player looks at it. The camera hangs off a spring arm
// that orbits a focus point on the paper, which is the whole reason the diorama reads as an
// object rather than as a place (docs/art/ART-STYLE.md SS3).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SolsticeObserver.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class SOLSTICE_API ASolsticeObserver : public APawn
{
	GENERATED_BODY()

public:
	ASolsticeObserver();

	/** Orbit the camera. Yaw and pitch are in degrees, distance in Unreal units. */
	void Orbit(float DeltaYaw, float DeltaPitch);
	void Zoom(float DeltaDistance);

	void SetFocus(const FVector& InFocus) { Focus = InFocus; }
	FVector GetFocus() const { return Focus; }
	void SetDistance(float InDistance);
	void SetDefaults(float InDistance, float InYaw, float InPitch);

	float GetDistance() const { return Distance; }
	float GetYaw() const { return Yaw; }
	float GetPitch() const { return Pitch; }

	UCameraComponent* GetCamera() const { return Camera; }

	// ---- entry / exit move ---------------------------------------------------
	// The approved v3 move. It lives here because this class owns distance / yaw / pitch, and the
	// tween is nothing but those three numbers interpolated; whoever ticks the game just calls
	// TickMove. Any player input cancels it (see the controller), because stealing the camera back
	// from someone who has started dragging is the one thing a camera animation must never do.
	void PlayIntro(float TargetDistance);
	void PlayOutro(float TargetDistance);
	void CancelMove();
	void TickMove(float DeltaSeconds);
	bool IsMoving() const { return bMoving; }

protected:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY() TObjectPtr<UCameraComponent> Camera;

	float Distance = 17000.0f;
	float Yaw = -52.0f;
	float Pitch = -34.0f;
	FVector Focus = FVector::ZeroVector;

	bool bMoving = false;
	bool bOutro = false;
	float MoveTime = 0.0f;
	float MoveDuration = 2.6f;
	float FromDistance = 0.0f, ToDistance = 0.0f;
	float FromYaw = 0.0f, ToYaw = 0.0f;
	float FromPitch = 0.0f, ToPitch = 0.0f;

	void Apply();
};
