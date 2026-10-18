// Solstice - UI layer: the Slate HUD widget (SSolsticeHUDView).
//
// Bound to the viewport by ASolsticeHUD (SolsticeHUD.h); the two names are deliberately distinct
// so "HUD the actor" and "HUD the widget" never collide again.
//
// Plain C++ Slate, no UMG assets, no blueprint: the HUD has to rebuild itself from nothing on a
// fresh checkout like everything else. Every number on screen is an attribute lambda, so the
// panel repaints from the kernel each frame and language switching is instant.
//
// Only buttons hit-test. Panels and text are HitTestInvisible on purpose, so a left click that
// is not on a button still lands on the paper.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ASolsticePlayerController;
class USolsticeGameInstance;
namespace Solstice { class FSimKernel; }

class SSolsticeHUDView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSolsticeHUDView) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ASolsticePlayerController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return false; }
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	ASolsticePlayerController* PC() const { return Controller.Get(); }
	USolsticeGameInstance* GI() const;
	Solstice::FSimKernel* Sim() const;

	/**
	 * The only thing the default view shows on the left: season, clock and weather in one line.
	 *
	 * It replaces two panels of twenty-four rows that together covered a third of the frame.
	 * Everything it leaves out is one keystroke away (T); nothing a player glances at is.
	 */
	TSharedRef<SWidget> BuildStatusCapsule();
	TSharedRef<SWidget> BuildClockPanel();
	TSharedRef<SWidget> BuildRealmPanel();
	TSharedRef<SWidget> BuildEchoPanel();
	TSharedRef<SWidget> BuildStatusLine();
	TSharedRef<SWidget> BuildHandBar();
	TSharedRef<SWidget> BuildActionBar();
	TSharedRef<SWidget> BuildHelpPanel();
	TSharedRef<SWidget> BuildPackPanel();
	TSharedRef<SWidget> BuildChroniclePanel();
	TSharedRef<SWidget> BuildPoolPanel();
	TSharedRef<SWidget> BuildCreatorPanel();
	TSharedRef<SWidget> CardChip(int32 Slot);

	TWeakObjectPtr<ASolsticePlayerController> Controller;
};
