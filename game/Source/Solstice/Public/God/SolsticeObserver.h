// Solstice - God layer: the observer.
//
// The player is not in the realm; the player looks at it. The camera hangs off a spring arm
// that looks at a focus point on the paper, which is the whole reason the diorama reads as an
// object rather than as a place (docs/ART.md SS7).
//
// **The camera never orbits freely.** Yaw is one of eight 45-degree stops and pitch one of four
// fixed elevations; distance is the only continuous axis. `Orbit()` used to live here and was
// removed on purpose - any continuous yaw/pitch entry point turns the art gate from "these 32
// poses must hold" into "every angle must hold", and every framing defect this project has
// shipped came from an angle nobody designed (CHARTER round 59, red line 4).

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

	/** Dolly in/out. The only continuous camera axis. */
	void Zoom(float DeltaDistance);

	void SetFocus(const FVector& InFocus) { Focus = InFocus; }
	FVector GetFocus() const { return Focus; }
	void SetDistance(float InDistance);
	void SetDefaults(float InDistance, float InYaw, float InPitch);

	/**
	 * Near / far clamp for `Zoom`.
	 *
	 * The far end has to follow the world: 512 tiles at the fitting distance is ~144,000 uu, and
	 * a hard-coded 60,000 meant one wheel notch near the wide shot could never be undone.
	 */
	void SetDistanceLimits(float NearUU, float FarUU);

	float GetDistance() const { return Distance; }
	float GetYaw() const { return Yaw; }
	float GetPitch() const { return Pitch; }

	UCameraComponent* GetCamera() const { return Camera; }

	// ---- camera moves --------------------------------------------------------
	// Distance / yaw / pitch are the only three numbers; a move is just those interpolated.
	// Every entity that changes the camera (intro, grid stop, tour) goes through MoveTo, so the
	// spring arm only ever sees one eased change at a time.
	void PlayIntro(float TargetDistance, float TargetYaw, float TargetPitch);
	void PlayOutro(float TargetDistance);
	void MoveTo(float InDistance, float InYaw, float InPitch, float Duration);
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
	float NearDistance = 2500.0f;
	float FarDistance = 600000.0f;
	FVector Focus = FVector::ZeroVector;

	bool bMoving = false;
	bool bOutro = false;
	/** The intro adds a 1.2 degree dip in its last 9% so the shot lands rather than stops. */
	bool bIntroDip = false;
	float MoveTime = 0.0f;
	float MoveDuration = 2.6f;
	float FromDistance = 0.0f, ToDistance = 0.0f;
	float FromYaw = 0.0f, ToYaw = 0.0f;
	float FromPitch = 0.0f, ToPitch = 0.0f;

	void Apply();
};
