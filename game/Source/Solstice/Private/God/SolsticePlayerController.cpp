// Solstice - God layer: the player controller.

#include "God/SolsticePlayerController.h"

#include "Camera/CameraComponent.h"
#include "Core/SolsticeGameInstance.h"
#include "Dev/SolsticeShot.h"
#include "Engine/World.h"
#include "God/SolsticeObserver.h"
#include "Kismet/GameplayStatics.h"
#include "Render/SolsticeCelMaterial.h"
#include "Sim/SolsticeKernel.h"
#include "Solstice.h"
#include "World/SolsticeRealm.h"

using namespace Solstice;

namespace
{
	/** One wheel notch moves this fraction of the current distance (proportional zoom). */
	constexpr float ZoomPerNotch = 0.12f;
	/** WASD covers this fraction of the current view per second (proportional pan). */
	constexpr float PanPerSecond = 0.35f;
	/** Middle-drag dolly: full screen height of travel is ~0.8 of the current distance. */
	constexpr float MiddleDragDolly = 0.004f;

	/**
		The four elevations, degrees above the horizon (docs/ART.md SS7).

		8 / 24 / 48 / 88, and the first one is not a small change.

		The vertical field is 29.4 degrees, so the horizon only enters the picture while the
		camera looks down less than 14.7 degrees - at 35 degrees the top of the frame is 20
		degrees *below* horizontal and every pixel is sea. That is why the world read as a plate
		floating in a void: there was no sky in any frame to be a landscape with, and the eye
		gets no horizon, no distance and no scale. 8 degrees puts the horizon a fifth of the way
		down and gives the sky a third of the picture, which is the composition every photograph
		of an island actually has.
	*/
	constexpr float CameraPitchDegrees[4] = { 8.0f, 24.0f, 48.0f, 88.0f };
	/** Where the eight azimuth stops start: -45 puts a three-quarter view on stop 0. */
	constexpr float AzimuthBase = -45.0f;
	constexpr float AzimuthStep = 45.0f;

	float YawForAzimuth(int32 Index)
	{
		return AzimuthBase + AzimuthStep * static_cast<float>(Index % 8);
	}

	/**
		Distance at which a disc of RealmRadius exactly fills the camera's vertical field.

		The lens is deliberately long - ASolsticeObserver sets 50 degrees horizontal for the
		tilt-shift diorama read - and that makes every "radius * N" framing wrong: solving the
		field keeps the shot correct whatever lens and world size the project ends up with.
	*/
	float FitDistance(const ASolsticeObserver* Observer, float RealmRadius)
	{
		// UCameraComponent::FieldOfView is the *horizontal* angle, and a 16:9 viewport runs out of
		// vertical field long before it runs out of horizontal - that mismatch is exactly why the
		// old framing showed a slice of the world instead of the whole disc. Solve the vertical.
		const float FovH = (Observer && Observer->GetCamera()) ? Observer->GetCamera()->FieldOfView : 38.0f;
		const float HalfH = FMath::DegreesToRadians(FMath::Max(10.0f, FovH) * 0.5f);
		const float HalfV = FMath::Atan(FMath::Tan(HalfH) / (16.0f / 9.0f));
		return RealmRadius * 1.09f / FMath::Max(0.03f, FMath::Tan(HalfV));
	}

	/** Is this process the editor running PIE? `quit` there closes the whole editor (G-10). */
	bool IsPlayInEditor(const UWorld* World)
	{
#if WITH_EDITOR
		return World && World->WorldType == EWorldType::PIE;
#else
		(void)World;
		return false;
#endif
	}
}

ASolsticePlayerController::ASolsticePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ASolsticePlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameAndUI()
		.SetHideCursorDuringCapture(false)
		.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));

	if (ASolsticeObserver* ObserverPawn = Observer())
	{
		if (ASolsticeRealm* RealmActor = Realm())
		{
			const float Radius = RealmActor->GetRealmRadiusUU();
			const float Fit = FitDistance(ObserverPawn, Radius);
			// Far clamp follows the world: 512 tiles needs ~144k uu at the fitting distance, and a
			// hard 60k made the wide shot unrecoverable after one wheel notch (G-03 / R-36).
			ObserverPawn->SetDistanceLimits(2500.0f, Fit * 1.6f);
			ObserverPawn->SetFocus(RealmActor->GetRealmCentre());
			ApplyCameraStop(false);
		}
	}

	SetStatus(TEXT("左键在纸面上释放手牌；三秒内实体还湿着，可以点回收回。"),
		TEXT("Left-click the paper to release a card; an entity stays wet for three seconds and can be taken back."));
}

ASolsticeRealm* ASolsticePlayerController::Realm() const
{
	if (CachedRealm)
	{
		return CachedRealm;
	}
	ASolsticePlayerController* Mutable = const_cast<ASolsticePlayerController*>(this);
	Mutable->CachedRealm = GetWorld() ? Cast<ASolsticeRealm>(UGameplayStatics::GetActorOfClass(GetWorld(), ASolsticeRealm::StaticClass())) : nullptr;
	return CachedRealm;
}

ASolsticeObserver* ASolsticePlayerController::Observer() const
{
	return Cast<ASolsticeObserver>(GetPawn());
}

USolsticeGameInstance* ASolsticePlayerController::Game() const
{
	return GetGameInstance<USolsticeGameInstance>();
}

void ASolsticePlayerController::SetStatus(const FString& Zh, const FString& En)
{
	StatusZh = Zh;
	StatusEn = En;
	StatusAge = 0.0f;
}

int32 ASolsticePlayerController::TileUnderCursor() const
{
	ASolsticeRealm* RealmActor = Realm();
	if (!RealmActor)
	{
		return INDEX_NONE;
	}
	FVector Hit;
	return RealmActor->PickTile(const_cast<ASolsticePlayerController*>(this), Hit);
}

// ======================================================================
// Commands
// ======================================================================

void ASolsticePlayerController::SelectSlot(int32 Slot)
{
	const FSimKernel* Sim = Game() ? &Game()->Kernel() : nullptr;
	if (!Sim)
	{
		return;
	}
	SelectedSlot = FMath::Clamp(Slot, 0, Tuning::HandSize - 1);
	if (Sim->GetHandCardIndex(SelectedSlot) == INDEX_NONE)
	{
		// Slide to the nearest occupied slot so the selection is never on an empty gap.
		for (int32 Step = 1; Step < Tuning::HandSize; ++Step)
		{
			const int32 Right = SelectedSlot + Step;
			if (Right < Tuning::HandSize && Sim->GetHandCardIndex(Right) != INDEX_NONE)
			{
				SelectedSlot = Right;
				return;
			}
			const int32 Left = SelectedSlot - Step;
			if (Left >= 0 && Sim->GetHandCardIndex(Left) != INDEX_NONE)
			{
				SelectedSlot = Left;
				return;
			}
		}
	}
}

void ASolsticePlayerController::CycleSlot(int32 Delta)
{
	for (int32 Step = 1; Step <= Tuning::HandSize; ++Step)
	{
		const int32 Candidate = ((SelectedSlot + Delta * Step) % Tuning::HandSize + Tuning::HandSize) % Tuning::HandSize;
		SelectSlot(Candidate);
		if (Candidate == SelectedSlot)
		{
			return;
		}
	}
}

void ASolsticePlayerController::UpdateCursor()
{
	ASolsticeRealm* RealmActor = Realm();
	if (!RealmActor)
	{
		return;
	}

	// Only while there is something to place. A marker under the cursor with an empty hand is a
	// promise the game cannot keep.
	const FSimKernel* Sim = Game() ? &Game()->Kernel() : nullptr;
	const bool bHasCard = Sim && Sim->GetHandCardIndex(SelectedSlot) != INDEX_NONE;
	if (!bHasCard)
	{
		RealmActor->SetCursorTile(INDEX_NONE, true);
		return;
	}

	// An offscreen capture has no pointer, so a shot may name a tile instead.
	int32 Tile = INDEX_NONE;
	const FIntPoint ShotCursor = Solstice::Shot::CursorTile();
	if (Solstice::Shot::Enabled() && ShotCursor.X >= 0 && Sim)
	{
		const int32 Size = Sim->GetSize();
		if (ShotCursor.X < Size && ShotCursor.Y < Size)
		{
			Tile = ShotCursor.Y * Size + ShotCursor.X;
		}
	}
	else
	{
		Tile = TileUnderCursor();
	}

	RealmActor->SetCursorTile(Tile, Tile != INDEX_NONE);
}

void ASolsticePlayerController::ReleaseSelectedAtCursor()
{
	const int32 Tile = TileUnderCursor();
	if (Tile == INDEX_NONE)
	{
		SetStatus(TEXT("先把光标移到纸面上"), TEXT("Move the cursor onto the paper first"));
		return;
	}
	ReleaseSelectedAt(Tile);
}

void ASolsticePlayerController::ReleaseSelectedAt(int32 TileIndex)
{
	USolsticeGameInstance* Instance = Game();
	ASolsticeRealm* RealmActor = Realm();
	if (!Instance || !RealmActor || TileIndex == INDEX_NONE)
	{
		return;
	}

	FSimKernel& Sim = Instance->Kernel();

	// Read the card before releasing: the kernel clears the slot and refills it in the same call.
	const int32 CardIndexBefore = Sim.GetHandCardIndex(SelectedSlot);
	const FCardDef* CardBefore = Sim.GetCatalog().CardAt(CardIndexBefore);

	FReleaseResult Result;
	if (!Sim.ReleaseCard(SelectedSlot, TileIndex, bSettleAsEntity, Result))
	{
		SetStatus(Result.ReasonZh, Result.ReasonEn);
		return;
	}

	LastRelease = Result;

	const FString NameZh = CardBefore ? CardBefore->NameZh : FString();
	const FString NameEn = CardBefore ? CardBefore->NameEn : FString();

	// "落差 0.72" was the kernel's impact score. A player cannot do anything with a float; what
	// they need to know is whether the card bit, and what it paid.
	const TCHAR* GradeZh = Result.Offset >= 0.75f ? TEXT("这道口子开得很大")
		: (Result.Offset >= 0.45f ? TEXT("落地有声") : TEXT("轻轻擦了一下"));
	const TCHAR* GradeEn = Result.Offset >= 0.75f ? TEXT("it tore open")
		: (Result.Offset >= 0.45f ? TEXT("it landed hard") : TEXT("it barely touched"));
	SetStatus(
		FString::Printf(TEXT("「%s」%s · 回响 +%d"), *NameZh, GradeZh, Result.EchoGained),
		FString::Printf(TEXT("%s - %s - echo +%d"), *NameEn, GradeEn, Result.EchoGained));

	if (Result.bCanUndo)
	{
		StatusZh += TEXT(" · 三秒内可按 Z 撤回");
		StatusEn += TEXT(" - press Z within three seconds to take it back");
	}
	SelectSlot(SelectedSlot);
}

void ASolsticePlayerController::RecycleSelected()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	FSimKernel& Sim = Instance->Kernel();
	int32 Refund = 0;
	FString ReasonZh;
	FString ReasonEn;
	if (Sim.RecycleCard(SelectedSlot, Refund, ReasonZh, ReasonEn))
	{
		SetStatus(FString::Printf(TEXT("回收返还 %d 回响"), Refund),
			FString::Printf(TEXT("recycled for %d echo"), Refund));
	}
	else
	{
		SetStatus(ReasonZh, ReasonEn);
	}
	SelectSlot(SelectedSlot);
}

void ASolsticePlayerController::RerollHand()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	Instance->Kernel().RerollHand();
	SelectSlot(0);
	SetStatus(TEXT("换了一手"), TEXT("Hand rerolled"));
}

void ASolsticePlayerController::OpenPack(EPackKind Kind)
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	FSimKernel& Sim = Instance->Kernel();
	FPackResult Result;
	if (!Sim.OpenPack(Kind, static_cast<uint8>(EElement::None), Result))
	{
		SetStatus(TEXT("回响不够，或者这个包现在不开"), TEXT("Not enough echo, or that pack is closed right now"));
		return;
	}
	LastPack = Result;
	SetStatus(FString::Printf(TEXT("开出 %d 张卡，折算回响 %d"), Result.Cards.Num(), Result.ConvertedEcho),
		FString::Printf(TEXT("Opened %d cards, %d echo auto-converted"), Result.Cards.Num(), Result.ConvertedEcho));
	bShowPacks = true;
}

void ASolsticePlayerController::OpenPackForElement(EElement Element)
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	FSimKernel& Sim = Instance->Kernel();
	FPackResult Result;
	if (!Sim.OpenPack(EPackKind::SpecificElement, static_cast<uint8>(Element), Result))
	{
		SetStatus(TEXT("回响不够"), TEXT("Not enough echo"));
		return;
	}
	LastPack = Result;
	SetStatus(FString::Printf(TEXT("开出 %d 张卡"), Result.Cards.Num()),
		FString::Printf(TEXT("Opened %d cards"), Result.Cards.Num()));
	bShowPacks = true;
}

void ASolsticePlayerController::CalmDisturbance(int32 Points)
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	int32 Cost = 0;
	const int32 Lowered = Instance->Kernel().CalmDisturbance(Points, Cost);
	SetStatus(FString::Printf(TEXT("扰动 -%d（花费 %d 回响）"), Lowered, Cost),
		FString::Printf(TEXT("Disturbance -%d (cost %d echo)"), Lowered, Cost));
}

void ASolsticePlayerController::Reshape(int32 PresetIndex)
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	if (Instance->ReshapeToPreset(PresetIndex))
	{
		SetStatus(TEXT("形制已更换"), TEXT("Realm reshaped"));
	}
	else
	{
		SetStatus(TEXT("没有这个形制"), TEXT("No such preset"));
	}
}

void ASolsticePlayerController::CyclePreset()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	const int32 Count = Instance->GetPresetCount();
	if (Count <= 0)
	{
		return;
	}
	const int32 Current = Instance->Kernel().GetConfig().Algo;
	Reshape((Current + 1) % Count);
}

void ASolsticePlayerController::SowLifeSeed()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	if (Instance->SowLifeSeed())
	{
		SetStatus(TEXT("生命种子已播下，世界开始自己活着"), TEXT("A life seed is sown; the world starts living on its own"));
	}
	else
	{
		SetStatus(TEXT("生命已经在了"), TEXT("Life is already here"));
	}
	bShowCreator = true;
}

void ASolsticePlayerController::ToggleTimeMode()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	FSimKernel& Sim = Instance->Kernel();
	const bool bReal = Sim.GetTimeMode() == ETimeMode::Real;
	Sim.SetTimeMode(bReal ? ETimeMode::Virtual : ETimeMode::Real);
	SetStatus(bReal ? TEXT("虚拟时间") : TEXT("现实 1:1 · 与系统时钟同步"),
		bReal ? TEXT("Virtual time") : TEXT("Real 1:1 - locked to the system clock"));
}

void ASolsticePlayerController::TogglePause()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	FSimKernel& Sim = Instance->Kernel();
	Sim.SetPaused(!Sim.IsPaused());
}

void ASolsticePlayerController::CycleSpeed()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	FSimKernel& Sim = Instance->Kernel();
	Sim.SetSpeedIndex((Sim.GetSpeedIndex() + 1) % 4);
}

void ASolsticePlayerController::CycleRealPace()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	FSimKernel& Sim = Instance->Kernel();
	static const float Paces[5] = { 1.0f, 3.0f, 6.0f, 12.0f, 30.0f };
	const float Current = Sim.GetRealMinutesPerWorldDay();
	int32 Index = 1;
	for (int32 Candidate = 0; Candidate < 5; ++Candidate)
	{
		if (FMath::IsNearlyEqual(Paces[Candidate], Current, 0.01f))
		{
			Index = (Candidate + 1) % 5;
			break;
		}
	}
	Sim.SetRealMinutesPerWorldDay(Paces[Index]);
}

void ASolsticePlayerController::ToggleLanguage()
{
	USolsticeGameInstance* Instance = Game();
	if (!Instance)
	{
		return;
	}
	Instance->ToggleLanguage();
}

// ======================================================================
// Input
// ======================================================================

void ASolsticePlayerController::PollCamera(float DeltaSeconds)
{
	ASolsticeObserver* ObserverPawn = Observer();
	if (!ObserverPawn)
	{
		return;
	}

	// ---- dolly: the only continuous axis ---------------------------------
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	GetInputMouseDelta(MouseX, MouseY);
	(void)MouseX;	// horizontal drag is deliberately unused: yaw is a stop, not a slider

	const bool bMiddleDown = IsInputKeyDown(EKeys::MiddleMouseButton);
	if (bMiddleDown)
	{
		// Middle drag *pulls* the camera in and out; it does not rotate. Any continuous yaw or
		// pitch entry is a regression (red line 4): the whole point of 32 stops is that nobody
		// can end up at an angle the art was never checked against.
		ObserverPawn->Zoom(MouseY * ObserverPawn->GetDistance() * MiddleDragDolly);
	}

	const float Wheel = GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	if (!FMath::IsNearlyZero(Wheel))
	{
		// Proportional: a notch is 12% of wherever you are, so framing the whole diorama and
		// reading the paper edge feel the same.
		ObserverPawn->Zoom(-Wheel * ObserverPawn->GetDistance() * ZoomPerNotch);
	}

	// Any deliberate input hands control back from the intro move instantly. An intro that keeps
	// fighting the player is worse than no intro at all.
	const bool bPlayerTookOver =
		IsInputKeyDown(EKeys::LeftMouseButton) || IsInputKeyDown(EKeys::RightMouseButton) ||
		bMiddleDown || !FMath::IsNearlyZero(Wheel) ||
		IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::A) ||
		IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::D);
	if (ObserverPawn->IsMoving() && bPlayerTookOver)
	{
		ObserverPawn->CancelMove();
	}

	// ---- pan: WASD moves the look-at point, never the angle ---------------
	FVector Focus = ObserverPawn->GetFocus();
	const float Pan = ObserverPawn->GetDistance() * PanPerSecond * DeltaSeconds;
	const float YawRad = FMath::DegreesToRadians(ObserverPawn->GetYaw());
	const FVector Forward(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.0f);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	if (IsInputKeyDown(EKeys::W)) { Focus += Forward * Pan; }
	if (IsInputKeyDown(EKeys::S)) { Focus -= Forward * Pan; }
	if (IsInputKeyDown(EKeys::D)) { Focus += Right * Pan; }
	if (IsInputKeyDown(EKeys::A)) { Focus -= Right * Pan; }
	ObserverPawn->SetFocus(Focus);

	// ---- tour: 3 s at a stop, 0.8 s between stops -------------------------
	// Not a continuous orbit. The tour exists so a player can find the world's best face, and
	// stopping is what makes each stop readable (CHARTER round 59).
	if (bTouring)
	{
		TourTimer -= DeltaSeconds;
		if (TourTimer <= 0.0f)
		{
			StepAzimuth(1);
			TourTimer = TourDwellSeconds;
		}
	}
}

void ASolsticePlayerController::PollCommands(float DeltaSeconds)
{
	ASolsticeObserver* ObserverPawn = Observer();
	if (!ObserverPawn)
	{
		return;
	}

	// ---- commands ----------------------------------------------------
	const bool bLeftDown = IsInputKeyDown(EKeys::LeftMouseButton);
	if (bLeftDown && !bWasLeftDown)
	{
		ReleaseSelectedAtCursor();
	}
	bWasLeftDown = bLeftDown;

	if (WasInputKeyJustPressed(EKeys::RightMouseButton)) { ToggleSettle(); }
	if (WasInputKeyJustPressed(EKeys::SpaceBar)) { ReleaseSelectedAtCursor(); }
	if (WasInputKeyJustPressed(EKeys::X)) { RecycleSelected(); }
	if (WasInputKeyJustPressed(EKeys::R)) { RerollHand(); }
	// Z (undo) is gone, by decision A-02: "there is no undo." INTERVENTION's no-undo red line is
	// the hard constraint - the three-second window is "the paper is still wet", not a take-back.
	// The binding, the button and the function itself are all gone now (structure cleanup), so
	// nothing can quietly re-expose an undo path.
	if (WasInputKeyJustPressed(EKeys::V)) { CyclePitchStop(); }
	if (WasInputKeyJustPressed(EKeys::LeftBracket)) { StepAzimuth(-1); }
	if (WasInputKeyJustPressed(EKeys::RightBracket)) { StepAzimuth(1); }
	if (WasInputKeyJustPressed(EKeys::Q)) { CycleSlot(-1); }
	if (WasInputKeyJustPressed(EKeys::E)) { CycleSlot(1); }

	if (WasInputKeyJustPressed(EKeys::One)) { SelectSlot(0); }
	if (WasInputKeyJustPressed(EKeys::Two)) { SelectSlot(1); }
	if (WasInputKeyJustPressed(EKeys::Three)) { SelectSlot(2); }
	if (WasInputKeyJustPressed(EKeys::Four)) { SelectSlot(3); }
	if (WasInputKeyJustPressed(EKeys::Five)) { SelectSlot(4); }
	if (WasInputKeyJustPressed(EKeys::Six)) { SelectSlot(5); }
	if (WasInputKeyJustPressed(EKeys::Seven)) { SelectSlot(6); }
	if (WasInputKeyJustPressed(EKeys::Eight)) { SelectSlot(7); }
	if (WasInputKeyJustPressed(EKeys::Nine)) { SelectSlot(8); }
	if (WasInputKeyJustPressed(EKeys::Zero)) { SelectSlot(9); }

	// ---- panels (A-01 重排) --------------------------------------------------
	// One scheme, three places: this block, the help.* strings and UI-UX SS2. The card library
	// moves from P to L (Library) so P is free for the ornament / screenshot mode that UI-UX
	// SS2.2 always specified on P - it was simply occupied by the library.
	if (WasInputKeyJustPressed(EKeys::C)) { ToggleChronicle(); }
	if (WasInputKeyJustPressed(EKeys::B)) { TogglePacks(); }
	if (WasInputKeyJustPressed(EKeys::G)) { ToggleCreator(); }
	if (WasInputKeyJustPressed(EKeys::H)) { ToggleHelp(); }
	if (WasInputKeyJustPressed(EKeys::L)) { TogglePool(); }
	if (WasInputKeyJustPressed(EKeys::P)) { bShowUI = false; }	// 摆件模式第一步：隐藏界面，Tab 复原

	// ---- camera --------------------------------------------------------------
	if (WasInputKeyJustPressed(EKeys::F1)) { SetPitchStop(0); }
	if (WasInputKeyJustPressed(EKeys::F2)) { SetPitchStop(1); }
	if (WasInputKeyJustPressed(EKeys::F3)) { SetPitchStop(2); }
	if (WasInputKeyJustPressed(EKeys::F4)) { SetPitchStop(3); }
	if (WasInputKeyJustPressed(EKeys::Y)) { ToggleTour(); }

	// ---- time & language ------------------------------------------------------
	if (WasInputKeyJustPressed(EKeys::T)) { ToggleTimeMode(); }
	if (WasInputKeyJustPressed(EKeys::K)) { TogglePause(); }
	if (WasInputKeyJustPressed(EKeys::J)) { CycleSpeed(); }
	if (WasInputKeyJustPressed(EKeys::O)) { ToggleLanguage(); }

	// ---- debug / creator ------------------------------------------------------
	if (WasInputKeyJustPressed(EKeys::F5)) { CycleRealPace(); }
	if (WasInputKeyJustPressed(EKeys::F6)) { OpenPack(EPackKind::RandomElement); }
	if (WasInputKeyJustPressed(EKeys::F7)) { OpenPack(EPackKind::Special); }
	if (WasInputKeyJustPressed(EKeys::F8)) { CyclePreset(); }
	if (WasInputKeyJustPressed(EKeys::N)) { SowLifeSeed(); }	// 播种生命：L 让给卡库

	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		// In PIE, `quit` closes the editor, not the game - that is G-10, and it is why play
		// sessions kept "crashing". Esc is a no-op in the editor; the standalone build keeps the
		// 1.8 s pull-back exit.
		if (IsPlayInEditor(GetWorld()))
		{
			SetStatus(TEXT("编辑器内 Esc 不退出（会连编辑器一起关）"), TEXT("Esc does nothing in PIE - it would close the editor"));
		}
		else
		{
			ASolsticeRealm* EscapeRealm = Realm();
			if (ObserverPawn && EscapeRealm)
			{
				ObserverPawn->PlayOutro(FitDistance(ObserverPawn, EscapeRealm->GetRealmRadiusUU()));
				QuitCountdown = 1.9f;
			}
			else
			{
				ConsoleCommand(TEXT("quit"));
			}
		}
	}
}

void ASolsticePlayerController::ApplyCameraStop(bool bAnimate)
{
	ASolsticeObserver* ObserverPawn = Observer();
	ASolsticeRealm* RealmActor = Realm();
	if (!ObserverPawn || !RealmActor)
	{
		return;
	}

	const float Radius = RealmActor->GetRealmRadiusUU();
	const float Fit = FitDistance(ObserverPawn, Radius);
	const float Yaw = YawForAzimuth(AzimuthIndex);
	const float Pitch = -CameraPitchDegrees[FMath::Clamp(PitchIndex, 0, ElevationStops - 1)];

	// Distance is the one continuous axis: a stop change keeps whatever dolly the player set,
	// clamped to the two bounds that are defined in world units (ART SS7).
	// Default framing is 0.72 of the fitting distance at the two survey stops.
	//
	// `Fit` frames the whole disc, which on a 128 m world leaves two thirds of the picture as
	// open sea and turns every tree into an eight-pixel dot. 0.72 still shows the realm's edge
	// and lets the disc span ~70% of the frame.
	//
	// Only the *snap* applies it: once the player has dollied, changing elevation must keep
	// their distance (distance is the one continuous axis), so an animated stop change clamps
	// whatever they had instead of resetting it.
	const float DefaultDistance = (PitchIndex == 0) ? Fit * 0.55f
		: ((PitchIndex == 1) ? Fit * 0.72f : Fit);
	const float Distance = bAnimate
		? FMath::Clamp(ObserverPawn->GetDistance(), 2500.0f, Fit * 1.6f)
		: FMath::Clamp(DefaultDistance, 2500.0f, Fit * 1.6f);

	// A low camera needs its aim lifted or the world fills the frame edge to edge and the sky is
	// a seam; the high survey stops want the realm centred. This is the only per-stop offset.
	const float AimLift = (PitchIndex == 0) ? 0.045f : ((PitchIndex == 1) ? 0.02f : 0.0f);
	ObserverPawn->SetFocus(RealmActor->GetRealmCentre() + FVector(0.0f, 0.0f, Radius * AimLift));

	if (bAnimate)
	{
		ObserverPawn->MoveTo(Distance, Yaw, Pitch, TourTransitionSeconds);
	}
	else
	{
		ObserverPawn->SetDefaults(Distance, Yaw, Pitch);
	}
}

void ASolsticePlayerController::StepAzimuth(int32 Delta)
{
	AzimuthIndex = ((AzimuthIndex + Delta) % AzimuthStops + AzimuthStops) % AzimuthStops;
	ApplyCameraStop(true);
}

void ASolsticePlayerController::SetPitchStop(int32 Index)
{
	PitchIndex = FMath::Clamp(Index, 0, ElevationStops - 1);
	ApplyCameraStop(true);
}

void ASolsticePlayerController::CyclePitchStop()
{
	SetPitchStop((PitchIndex + 1) % ElevationStops);
}

void ASolsticePlayerController::ToggleTour()
{
	bTouring = !bTouring;
	TourTimer = TourDwellSeconds;
}

void ASolsticePlayerController::ApplyCameraPreset(int32 Index)
{
	// Unattended capture (tools/run/shot.ps1) names the four elevations 0-3. Older callers passed
	// 4 and 5 for extra framings; those stops no longer exist, so they are ignored rather than
	// silently mapped onto an angle the art gate never checks.
	if (Index >= 0 && Index < ElevationStops)
	{
		PitchIndex = Index;
		ApplyCameraStop(false);
	}
}

void ASolsticePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	StatusAge += DeltaSeconds;

	// ---- entry framing and the intro move --------------------------------------
	// Applied on the first frame where the realm actually exists. Doing it in BeginPlay looked
	// right but silently did nothing whenever the GameMode had not spawned the realm yet, and the
	// camera then stayed at the pawn's constructor distance (17000) - which is the "I only see a
	// slice of the world" the framing was supposed to fix in the first place.
	// The framing is handed over as the *end* of the intro move, so entering the realm is one
	// gesture: sweep in, settle, then the player owns the camera.
	if (!bCameraFramed)
	{
		if (ASolsticeObserver* ObserverPawn = Observer())
		{
			if (ASolsticeRealm* RealmActor = Realm())
			{
				const float Fit = FitDistance(ObserverPawn, RealmActor->GetRealmRadiusUU());
				ObserverPawn->SetFocus(RealmActor->GetRealmCentre());
				// The intro lands on the default grid stop, not on a bespoke angle: the entry shot is
				// one of the 32 poses the art gate checks, so it can never drift into an unchecked one.
				ObserverPawn->PlayIntro(Fit * (PitchIndex == 0 ? 0.55f : 0.72f), YawForAzimuth(AzimuthIndex),
					-CameraPitchDegrees[PitchIndex]);
				bCameraFramed = true;

				// Diagnostics, not decoration: every framing bug in this loop came from a number
				// nobody had printed - a radius that was 4x the assumed one, a distance clamp below
				// the required distance, a lens read as vertical when it is horizontal.
				UE_LOG(LogSolstice, Warning,
					TEXT("[diag] camera: realmRadius=%.0f uu, fov=%.1f deg (horizontal), fit=%.0f, introFrom=%.0f, clamp now=%.0f"),
					RealmActor->GetRealmRadiusUU(),
					ObserverPawn->GetCamera() ? ObserverPawn->GetCamera()->FieldOfView : -1.0f,
					Fit, Fit * 2.35f, 600000.0f);
			}
		}
	}

	if (ASolsticeObserver* ObserverPawn = Observer())
	{
		ObserverPawn->TickMove(DeltaSeconds);

		// Where the camera actually is, every two seconds. Every framing bug so far came down to a
		// number nobody had printed: the arm length, the world location and the angles together say
		// immediately whether the camera is outside the shell, inside it, or under the ground.
		static int32 DiagFrames = 0;
		if ((++DiagFrames % 120) == 0)
		{
			FVector CamLoc = FVector::ZeroVector;
			if (const UCameraComponent* Cam = ObserverPawn->GetCamera())
			{
				CamLoc = Cam->GetComponentLocation();
			}
			UE_LOG(LogSolstice, Warning,
				TEXT("[diag] cam: dist=%.0f pitch=%.1f yaw=%.1f loc=(%.0f,%.0f,%.0f) focus=(%.0f,%.0f,%.0f)"),
				ObserverPawn->GetDistance(), ObserverPawn->GetPitch(), ObserverPawn->GetYaw(),
				CamLoc.X, CamLoc.Y, CamLoc.Z,
				ObserverPawn->GetFocus().X, ObserverPawn->GetFocus().Y, ObserverPawn->GetFocus().Z);
		}
	}

	// Quitting is two beats now: Esc starts the exit move, the quit lands when it finishes.
	if (QuitCountdown >= 0.0f)
	{
		QuitCountdown -= DeltaSeconds;
		if (QuitCountdown < 0.0f)
		{
			ConsoleCommand(TEXT("quit"));
		}
	}

	// ---- unattended capture (tools/run/shot.ps1) --------------------------------
	// Deliberately last: the frame that gets measured has to be one where the intro move has
	// finished and the preset, if any, has been applied. Capturing earlier measured the entry
	// animation instead of the picture.
	if (Solstice::Shot::Enabled() && !bShotCaptured)
	{
		ShotAge += DeltaSeconds;
		// The preset is applied on the *first* shot frame, not on the capture frame: applying it
		// and capturing in the same tick left every frame-relative thing (the sky card, the depth
		// of field, the projected box) describing the previous camera pose.
		if (!bShotPresetApplied)
		{
			bShotPresetApplied = true;
			if (ASolsticeObserver* ObserverPawn = Observer())
			{
				// A running intro move would keep overwriting the preset on the following tick,
				// so the move is cancelled first (it lands on its own end pose, then the preset
				// takes over).
				ObserverPawn->CancelMove();
			}
			if (Solstice::Shot::Azimuth() != INDEX_NONE)
			{
				AzimuthIndex = ((Solstice::Shot::Azimuth() % AzimuthStops) + AzimuthStops) % AzimuthStops;
			}
			if (Solstice::Shot::Preset() != INDEX_NONE)
			{
				ApplyCameraPreset(Solstice::Shot::Preset());
			}
			else
			{
				ApplyCameraStop(false);
			}
		}
		if (ShotAge >= Solstice::Shot::Delay())
		{
			bShotCaptured = true;
			// Force the lens to update this frame rather than at the next camera tick, so the PNG
			// and the numbers in the log always describe the same picture.
			if (ASolsticeObserver* ObserverPawn = Observer())
			{
				ObserverPawn->GetCamera()->MarkRenderStateDirty();
			}
			// The camera manager caches its POV and only rebuilds it on the next tick, so the
			// screen-space box below used to be projected with the *previous* framing - it
			// reported the same box for two different camera distances. Update it now.
			if (PlayerCameraManager)
			{
				PlayerCameraManager->UpdateCamera(0.0f);
			}
			Solstice::Shot::Capture(this);
			UE_LOG(LogSolstice, Warning, TEXT("[shot] framing: preset=%d dist=%.0f yaw=%.1f pitch=%.1f"),
				Solstice::Shot::Preset(),
				Observer() ? Observer()->GetDistance() : -1.0f,
				Observer() ? Observer()->GetYaw() : 0.0f,
				Observer() ? Observer()->GetPitch() : 0.0f);

			// State readback. Every "I changed it and nothing happened" in this project came from
			// not knowing which of these three was true at capture time.
			// The clock and the sun go in here too: a dusk frame that silently came out daylit
			// because the sky still held FSkyState's default 45-degree sun looked like a shading
			// problem from the outside, and nothing in the log contradicted it.
			if (const USolsticeGameInstance* Instance = Game())
			{
				const FSimKernel& Sim = Instance->Kernel();
				const FSkyState& Sky = Sim.GetSky();
				UE_LOG(LogSolstice, Warning,
					TEXT("[shot] state: painted=%.2f chaos=%d pop=%d material=%s authored=%d clock=%02d:%02d alt=%.1f cloud=%.2f"),
					Sim.GetPaintedFraction(), Sim.IsChaos() ? 1 : 0, Sim.GetPopulationTotal(),
					*FCelMaterials::Get().Cel->GetPathName(),
					FCelMaterials::Get().bAuthored ? 1 : 0,
					Sim.GetWorldHour(), Sim.GetWorldMinuteOfHour(),
					Sky.SolarAltitudeDeg, Sky.CloudCover);
			}

			// Screen-space box around the realm disc, so a measurement script knows which pixels
			// are world and which are the void behind it. Guessing the crop by eye was the reason
			// the first measurement read 0.44 median on a frame that was visually pure white - the
			// blue surround dominated the sample.
			if (ASolsticeRealm* RealmActor = Realm())
			{
				const FVector Centre = RealmActor->GetRealmCentre();
				const float Radius = RealmActor->GetRealmRadiusUU();
				FVector2D Min(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
				FVector2D Max(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
				bool bAny = false;
				for (int32 Step = 0; Step < 16; ++Step)
				{
					const float Angle = 2.0f * PI * static_cast<float>(Step) / 16.0f;
					for (const float Lift : {0.0f, 0.12f})
					{
						const FVector Rim = Centre + FVector(
							FMath::Cos(Angle) * Radius,
							FMath::Sin(Angle) * Radius,
							Lift * Radius);
						FVector2D Screen;
						if (UGameplayStatics::ProjectWorldToScreen(this, Rim, Screen, true))
						{
							Min = FVector2D(FMath::Min(Min.X, Screen.X), FMath::Min(Min.Y, Screen.Y));
							Max = FVector2D(FMath::Max(Max.X, Screen.X), FMath::Max(Max.Y, Screen.Y));
							bAny = true;
						}
					}
				}
				if (bAny)
				{
					UE_LOG(LogSolstice, Warning, TEXT("[shot] box=%.0f,%.0f,%.0f,%.0f"),
						FMath::Max(0.0, Min.X), FMath::Max(0.0, Min.Y),
						Max.X, Max.Y);
				}
			}
		}
	}

	// Tab lives outside the bShowUI gate: a hidden HUD must still be able to unhide itself.
	if (WasInputKeyJustPressed(EKeys::Tab)) { ToggleUI(); }
	// T opens the full read-out. Outside the gate for the same reason as Tab, but also because
	// the capsule that replaces it is the only thing on screen in the default view: a player who
	// cannot find the numbers would otherwise have no way to ask for them.
	// I, not T: T is the time-mode key (A-01). Two actions on one key meant every press changed
	// the clock and the read-out together.
	if (WasInputKeyJustPressed(EKeys::I)) { ToggleDetails(); }

	// Camera input is outside the UI gate: hiding the HUD (P) must not freeze the dolly or the
	// stops - that was G-04, where ornament mode left the player unable to move at all.
	PollCamera(DeltaSeconds);

	if (bShowUI)
	{
		UpdateCursor();
		PollCommands(DeltaSeconds);
	}
	else
	{
		// Ornament mode draws no chrome, so it draws no cursor either - a ring left on the paper
		// in a screenshot is exactly the thing P exists to avoid.
		if (ASolsticeRealm* RealmActor = Realm())
		{
			RealmActor->SetCursorTile(INDEX_NONE, true);
		}
	}
}
