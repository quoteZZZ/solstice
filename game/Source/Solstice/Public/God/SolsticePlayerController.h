// Solstice - God layer: the player controller.
//
// The controller is the command surface. Holding a card, releasing it, taking it back while the
// paper is still wet, calming the disturbance, opening a pack - every one of those is a method
// here, so the Slate HUD and the keyboard are two ways into exactly the same calls
// (docs/CARDS.md SS1, docs/CARDS.md SS5).
//
// Input is polled once per frame rather than bound through action mappings: the HUD needs the
// mouse free for its own buttons, and polling keeps the two from fighting over capture.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Sim/SolsticeTypes.h"
#include "SolsticePlayerController.generated.h"

class ASolsticeObserver;
class ASolsticeRealm;
class USolsticeGameInstance;

UCLASS()
class SOLSTICE_API ASolsticePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASolsticePlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	ASolsticeRealm* Realm() const;
	ASolsticeObserver* Observer() const;
	USolsticeGameInstance* Game() const;

	// ------------------------------------------------------------------
	// Commands - called by the keyboard polling below and by the HUD
	// ------------------------------------------------------------------

	void SelectSlot(int32 Slot);
	void CycleSlot(int32 Delta);
	int32 GetSelectedSlot() const { return SelectedSlot; }
	bool IsSettleAsEntity() const { return bSettleAsEntity; }
	void ToggleSettle() { bSettleAsEntity = !bSettleAsEntity; }

	void ReleaseSelectedAtCursor();
	void ReleaseSelectedAt(int32 TileIndex);
	void RecycleSelected();
	void RerollHand();

	// ---- camera: eight azimuths x four elevations ----------------------------
	// Fixed stops, not free look. `[` / `]` step the azimuth, `V` steps the elevation,
	// F1-F4 jump straight to the four elevations, `Y` tours the azimuths (3 s dwell).
	void StepAzimuth(int32 Delta);
	void SetPitchStop(int32 Index);
	void CyclePitchStop();
	void ToggleTour();
	bool IsTourOn() const { return bTouring; }
	int32 GetAzimuthIndex() const { return AzimuthIndex; }
	int32 GetPitchIndex() const { return PitchIndex; }
	/** Snap the camera onto the current stop (used by shot.ps1 and on entry). */
	void ApplyCameraStop(bool bAnimate);
	/** 0-3 map to the four elevations; anything else is ignored (shot.ps1 compatibility). */
	void ApplyCameraPreset(int32 Index);

	void OpenPack(Solstice::EPackKind Kind);
	void OpenPackForElement(Solstice::EElement Element);
	void CalmDisturbance(int32 Points);
	void Reshape(int32 PresetIndex);
	void CyclePreset();
	void SowLifeSeed();

	void ToggleTimeMode();
	void TogglePause();
	void CycleSpeed();
	void CycleRealPace();
	void ToggleLanguage();

	// ---- panel visibility (the HUD also flips these) ----
	/**
	 * The full read-out (clock detail + realm census).
	 *
	 * Off by default: the two panels are 24 rows of numbers and they were covering a third of the
	 * frame, which is the whole reason the first screen after "new game" read as a debug overlay
	 * instead of as a place. The one-line status capsule carries what a player actually glances
	 * at; T opens the rest.
	 */
	void ToggleDetails() { bShowDetails = !bShowDetails; }
	bool IsDetailsOpen() const { return bShowDetails; }

	void ToggleChronicle() { bShowChronicle = !bShowChronicle; }
	void TogglePool() { bShowPool = !bShowPool; }
	void TogglePacks() { bShowPacks = !bShowPacks; }
	void ToggleCreator() { bShowCreator = !bShowCreator; }
	void ToggleHelp() { bShowHelp = !bShowHelp; }
	void ToggleUI() { bShowUI = !bShowUI; }

	bool IsChronicleOpen() const { return bShowChronicle; }
	bool IsPoolOpen() const { return bShowPool; }
	bool IsPacksOpen() const { return bShowPacks; }
	bool IsCreatorOpen() const { return bShowCreator; }
	bool IsHelpOpen() const { return bShowHelp; }
	bool IsUIOpen() const { return bShowUI; }

	/** Tile under the cursor, or INDEX_NONE. */
	int32 TileUnderCursor() const;

	const Solstice::FReleaseResult& GetLastRelease() const { return LastRelease; }
	const Solstice::FPackResult& GetLastPack() const { return LastPack; }
	const FString& GetStatusZh() const { return StatusZh; }
	const FString& GetStatusEn() const { return StatusEn; }
	bool HasStatus() const { return StatusAge < 6.0f; }

private:
	void PollCamera(float DeltaSeconds);
	void PollCommands(float DeltaSeconds);
	void SetStatus(const FString& Zh, const FString& En);

	ASolsticeRealm* CachedRealm = nullptr;

	int32 SelectedSlot = 0;
	bool bSettleAsEntity = false;
	bool bShowDetails = false;
	bool bShowChronicle = false;
	bool bShowPool = false;
	bool bShowPacks = false;
	bool bShowCreator = false;
	bool bShowHelp = false;
	bool bShowUI = true;

	bool bWasLeftDown = false;

	// ---- camera ----------------------------------------------------------
	/** 0..7, each 45 degrees, -45 is stop 0 (the three-quarter view). */
	int32 AzimuthIndex = 0;
	/** 0..3 into `CameraPitchDegrees` (14 / 35 / 62 / 88 above the horizon). */
	int32 PitchIndex = 1;
	bool bTouring = false;
	/** Seconds until the tour advances to the next azimuth stop. */
	float TourTimer = 0.0f;
	/** Set once the entry framing has been applied; the realm may not exist on the first frames. */
	bool bCameraFramed = false;
	/** Seconds left before the quit lands; negative means "not quitting". */
	float QuitCountdown = -1.0f;
	/** Seconds since the unattended-capture harness armed (see Dev/SolsticeShot.h). */
	float ShotAge = 0.0f;
	bool bShotCaptured = false;
	bool bShotPresetApplied = false;
	static constexpr int32 AzimuthStops = 8;
	static constexpr int32 ElevationStops = 4;
	static constexpr float TourDwellSeconds = 3.0f;
	static constexpr float TourTransitionSeconds = 0.8f;

private:

	Solstice::FReleaseResult LastRelease;
	Solstice::FPackResult LastPack;
	FString StatusZh;
	FString StatusEn;
	float StatusAge = 1000.0f;
};
