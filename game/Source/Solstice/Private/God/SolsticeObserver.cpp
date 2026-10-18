// Solstice - God layer: the observer.

#include "God/SolsticeObserver.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

ASolsticeObserver::ASolsticeObserver()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(Root);
	Boom->SetUsingAbsoluteRotation(true);
	Boom->bDoCollisionTest = false;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 9.0f;
	Boom->bEnableCameraRotationLag = true;
	Boom->CameraRotationLagSpeed = 12.0f;
	Boom->TargetArmLength = Distance;
	Boom->SocketOffset = FVector(0.0f, 0.0f, 260.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	// A long lens compresses the realm the way a tilt-shift shot of a real diorama does. 38 is the
	// compromise: at 28 the whole realm could not be framed without pushing the camera so far out
	// that the height fog swallowed it, which is why the entry shot always looked cropped.
	// 50 degrees horizontal, not 38: at 16:9 the vertical field of 38 horizontal is only ~22
	// degrees, and a horizon cannot sit inside that unless the camera is almost level. The
	// landscape framing needs the sea's vanishing line about a third of the way down the frame.
	Camera->SetFieldOfView(50.0f);

	Apply();
}

void ASolsticeObserver::SetDefaults(float InDistance, float InYaw, float InPitch)
{
	Distance = FMath::Clamp(InDistance, 2000.0f, 600000.0f);
	Yaw = InYaw;
	// -1, not -4.
	//
	// The old ceiling existed for a floating disc: tilted any flatter, the camera saw under the
	// realm and the frame edge became the subject. With an ocean running to a 400 km sky dome
	// there is nothing to see under, and -4 degrees holds the horizon so high that the sky is
	// only 20% of the frame no matter what the preset asks for (measured: preset 0 requested
	// -1.8 and the log still reported -4.0).
	Pitch = FMath::Clamp(InPitch, -88.0f, -1.0f);
	Apply();
}

void ASolsticeObserver::Orbit(float DeltaYaw, float DeltaPitch)
{
	Yaw += DeltaYaw;
	Pitch = FMath::Clamp(Pitch + DeltaPitch, -88.0f, -1.0f);
	Apply();
}

void ASolsticeObserver::Zoom(float DeltaDistance)
{
	Distance = FMath::Clamp(Distance + DeltaDistance, 2500.0f, 60000.0f);
	Apply();
}

void ASolsticeObserver::SetDistance(float InDistance)
{
	Distance = FMath::Clamp(InDistance, 2500.0f, 600000.0f);
	Apply();
}

void ASolsticeObserver::Apply()
{
	if (Boom)
	{
		Boom->TargetArmLength = Distance;
		Boom->SetRelativeRotation(FRotator(Pitch, Yaw, 0.0f));
	}
	SetActorLocation(Focus);
}

// ---- entry / exit move (v3) --------------------------------------------------
// Numbers come straight from studio/previews/camera-move.html, which is the version that was
// approved: a 34 degree sweep rather than an orbit, and one ease over the whole move so the speed
// curve is a single hump - per-segment easing was what made the first draft stutter at every
// keyframe.

void ASolsticeObserver::PlayIntro(float TargetDistance)
{
	FromDistance = TargetDistance * 2.35f;
	ToDistance = TargetDistance;
	FromYaw = -86.0f;  ToYaw = -52.0f;
	FromPitch = -30.0f; ToPitch = -50.0f;
	MoveDuration = 2.6f;
	MoveTime = 0.0f;
	bOutro = false;
	bMoving = true;

	Distance = FromDistance;
	Yaw = FromYaw;
	Pitch = FromPitch;
	Apply();
}

void ASolsticeObserver::PlayOutro(float TargetDistance)
{
	FromDistance = Distance;
	ToDistance = TargetDistance * 2.6f;
	FromYaw = Yaw;      ToYaw = Yaw + 12.0f;
	FromPitch = Pitch;  ToPitch = -22.0f;
	MoveDuration = 1.8f;
	MoveTime = 0.0f;
	bOutro = true;
	bMoving = true;
}

void ASolsticeObserver::CancelMove()
{
	if (!bMoving)
	{
		return;
	}
	// Landing where the move was heading is less jarring than snapping back to where it started.
	if (!bOutro)
	{
		Distance = ToDistance;
		Yaw = ToYaw;
		Pitch = ToPitch;
		Apply();
	}
	bMoving = false;
}

void ASolsticeObserver::TickMove(float DeltaSeconds)
{
	if (!bMoving)
	{
		return;
	}

	MoveTime += DeltaSeconds;
	const float U = FMath::Clamp(MoveTime / FMath::Max(0.05f, MoveDuration), 0.0f, 1.0f);

	// Enter settles (ease-out), leave departs (ease-in). Reversing one curve for the other is what
	// makes an entry feel catapulted and an exit feel dragged back.
	const float E = bOutro
		? U * U * U
		: 1.0f - FMath::Pow(1.0f - U, 5.0f);

	Distance = FMath::Lerp(FromDistance, ToDistance, E);
	Yaw = FMath::Lerp(FromYaw, ToYaw, E);

	float P = FMath::Lerp(FromPitch, ToPitch, E);
	// A 1.2 degree dip and recovery in the last 9%: the shot "lands" instead of stopping.
	if (!bOutro && U > 0.91f)
	{
		P += 1.2f * FMath::Sin((U - 0.91f) / 0.09f * PI);
	}
	Pitch = P;
	Apply();

	if (U >= 1.0f)
	{
		bMoving = false;
	}
}
