// Solstice - Core layer: the game instance.
//
// Owns the three things that outlive any single actor: the loaded content, the reality anchor
// and the simulation kernel. The realm actor ticks them, the UI reads them.
//
// The kernel lives here rather than on the actor so that a level change cannot silently reset
// a realm - "one account, one world" is a design commitment (docs/CARDS.md SS7.2).

#pragma once

#include "CoreMinimal.h"
#include "Core/SolsticeDataLoader.h"
#include "Core/SolsticeRealityAnchor.h"
#include "Engine/GameInstance.h"
#include "Sim/SolsticeKernel.h"
#include "SolsticeGameInstance.generated.h"

UCLASS()
class SOLSTICE_API USolsticeGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	USolsticeGameInstance();

	virtual void Init() override;
	virtual void Shutdown() override;

	Solstice::FSimKernel& Kernel() { return Sim; }
	const Solstice::FSimKernel& Kernel() const { return Sim; }
	Solstice::FRealityAnchor& Anchor() { return Reality; }
	const Solstice::FLoadedContent& Content() const { return Loaded; }

	bool IsReady() const { return bReady; }
	const TArray<FString>& Warnings() const { return Loaded.Warnings; }

	/**
	 * One frame of the world: the reality anchor is fed first so the clock and the weather the
	 * kernel sees are always the same sample (docs/WORLD.md SS4).
	 */
	void TickSimulation(float DeltaSeconds);

	/** Creator step 2: reshape the realm onto another preset (docs/WORLD.md SS3.3). */
	bool ReshapeToPreset(int32 PresetIndex);

	/**
	 * Creator step 8: sow the life seed. The realm starts in chaos with no life at all, so this
	 * is the one switch that turns an empty paper world into an ecology. It regenerates from the
	 * same seed, which is why the terrain does not change under the player.
	 */
	bool SowLifeSeed();

	int32 GetPresetCount() const { return Loaded.Catalog.Presets.Num(); }

	void ToggleLanguage();
	bool IsChinese() const;
	const TCHAR* LanguageCode() const;

private:
	Solstice::FSimKernel Sim;
	Solstice::FRealityAnchor Reality;
	Solstice::FLoadedContent Loaded;
	bool bReady = false;
};
