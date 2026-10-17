// Solstice - dev layer: unattended frame capture.
//
// The refactor loop runs without anyone at the keyboard, so "how does the frame look" has to be
// answered by a file on disk rather than by eye. This harness launches the game, lets the world
// settle, frames the camera, writes one PNG and quits:
//
//   SolsticeEditor.exe game/Solstice.uproject -game -RenderOffscreen -ResX=1280 -ResY=720 \
//     -SolsticeShot="D:/GitCode/solstice/tmp/review/shot.png" -SolsticeShotPreset=0
//
// `tools/run/shot.ps1` wraps the above, and `tools/checks/measure-frame.py` turns the PNG into
// pass/fail numbers. Nothing here runs unless -SolsticeShot was passed, so the shipping build is
// unaffected (the parser leaves `Enabled()` false and every call returns immediately).

#pragma once

#include "CoreMinimal.h"

class APlayerController;

namespace Solstice::Shot
{
	/** True when -SolsticeShot=<png> was on the command line. */
	bool Enabled();

	/** Absolute path to write, or empty when disabled. */
	const FString& Path();

	/** Camera preset to jump to before capturing; INDEX_NONE keeps the entry framing. */
	int32 Preset();

	/** Seconds to wait before capturing: the intro move alone is 2.6 s. */
	float Delay();

	/** Also draw the Slate HUD. Off by default - the HUD would bias every pixel measurement. */
	bool WithUI();

	/** Requests the screenshot and schedules the quit. Called once, by the owning controller. */
	void Capture(APlayerController* Controller);
}
