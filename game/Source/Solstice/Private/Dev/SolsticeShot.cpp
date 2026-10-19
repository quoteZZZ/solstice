// Solstice - dev layer: unattended frame capture.

#include "Dev/SolsticeShot.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Solstice.h"
#include "UnrealClient.h"

namespace
{
	struct FShotOptions
	{
		bool bParsed = false;
		bool bEnabled = false;
		FString Path;
		int32 Preset = INDEX_NONE;
		int32 Azimuth = INDEX_NONE;
		int32 CursorX = INDEX_NONE;
		int32 CursorY = INDEX_NONE;
		float Delay = 4.0f;
		bool bWithUI = false;
	};

	FShotOptions& Options()
	{
		static FShotOptions Cached;
		if (Cached.bParsed)
		{
			return Cached;
		}
		Cached.bParsed = true;

		const TCHAR* Cmd = FCommandLine::Get();

		FString Requested;
		// FParse::Value matches the bare `Name=` token, so `-SolsticeShot="D:/x.png"` parses with or
		// without the leading dash. Quoted values come back unquoted.
		if (!FParse::Value(Cmd, TEXT("SolsticeShot="), Requested) || Requested.IsEmpty())
		{
			return Cached;
		}

		// Without an absolute path the engine would drop the file into Saved/Screenshots/<Platform>,
		// which is fine for a human but useless to a script that has to name the file up front.
		if (FPaths::IsRelative(Requested))
		{
			Requested = FPaths::Combine(FPaths::ProjectDir(), Requested);
		}
		Requested = FPaths::ConvertRelativePathToFull(Requested);

		Cached.bEnabled = true;
		Cached.Path = Requested;

		FParse::Value(Cmd, TEXT("SolsticeShotPreset="), Cached.Preset);
		FParse::Value(Cmd, TEXT("SolsticeShotAzimuth="), Cached.Azimuth);
		FString Cursor;
		if (FParse::Value(Cmd, TEXT("SolsticeShotCursor="), Cursor, false))
		{
			TArray<FString> Parts;
			Cursor.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 2)
			{
				Cached.CursorX = FCString::Atoi(*Parts[0]);
				Cached.CursorY = FCString::Atoi(*Parts[1]);
				UE_LOG(LogSolstice, Warning, TEXT("[shot] cursor tile=%d,%d"), Cached.CursorX, Cached.CursorY);
			}
		}
		FParse::Value(Cmd, TEXT("SolsticeShotDelay="), Cached.Delay);
		Cached.bWithUI = FParse::Param(Cmd, TEXT("SolsticeShotUI"));

		UE_LOG(LogSolstice, Warning, TEXT("[shot] armed: path=%s preset=%d delay=%.2fs ui=%d"),
			*Cached.Path, Cached.Preset, Cached.Delay, Cached.bWithUI ? 1 : 0);
		return Cached;
	}
}

namespace Solstice::Shot
{
	bool Enabled()
	{
		return Options().bEnabled;
	}

	const FString& Path()
	{
		return Options().Path;
	}

	int32 Preset()
	{
		return Options().Preset;
	}

	int32 Azimuth()
	{
		return Options().Azimuth;
	}

	FIntPoint CursorTile()
	{
		const FShotOptions& Opts = Options();
		return FIntPoint(Opts.CursorX, Opts.CursorY);
	}

	float Delay()
	{
		return Options().Delay;
	}

	bool WithUI()
	{
		return Options().bWithUI;
	}

	void Capture(APlayerController* Controller)
	{
		const FShotOptions& Opts = Options();
		if (!Opts.bEnabled)
		{
			return;
		}

		// bAddUniqueSuffix=false so the file lands exactly where the caller asked; the suffix path
		// would append _0001 and the measurement script would read yesterday's frame.
		FScreenshotRequest::RequestScreenshot(Opts.Path, Opts.bWithUI, /*bAddUniqueSuffix*/ false);

		UE_LOG(LogSolstice, Warning, TEXT("[shot] captured %s"), *Opts.Path);

		// The capture happens during the next viewport draw, so the exit has to wait a beat or the
		// process dies with the request still queued and no file on disk.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			FPlatformMisc::RequestExit(false);
			return false;
		}), 1.0f);
	}
}
