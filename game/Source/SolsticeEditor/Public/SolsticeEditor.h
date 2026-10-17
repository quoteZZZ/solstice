// Solstice - editor module.
//
// Two jobs, both of them "make a fresh checkout able to run":
//   - generate the paper master materials into /Game/Core (this is the only binary content the
//     game actually needs, and docs/tech/STRUCTURE.md SS0 says content must be rebuildable);
//   - own the headless entry points for the M0 regression.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Modules/ModuleInterface.h"

#include "SolsticeEditor.generated.h"

class FSolsticeEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

/** Builds /Game/Core/M_Cel and /Game/Core/M_Ink if they are missing. */
class FSolsticeCelMaterialBuilder
{
public:
	/** Returns true when both masters exist afterwards. Skips any package already on disk. */
	static bool BuildAll();
};

/**
 * -run=SolsticeGenAssets
 *
 * The canonical way to rebuild the generated content. A fresh clone has no .uasset at all (see
 * docs/tech/STRUCTURE.md SS0), so the first step after cloning is:
 *
 *   UnrealEditor-Cmd.exe game/Solstice.uproject -run=SolsticeGenAssets -unattended -nopause
 *
 * Exit code 0 means both masters are on disk. The runtime also builds them lazily when it is
 * loaded by an editor (PIE), but -game and cooked builds cannot link UnrealEd, so they need this.
 */
UCLASS()
class USolsticeGenAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USolsticeGenAssetsCommandlet();

	virtual int32 Main(const FString& Params) override;
};

/**
 * -run=SolsticeBench [-Days=N] [-Size=N] [-Seed=N]
 *
 * The M0 headless regression, and the only automated proof of the determinism contract in
 * docs/tech/ARCHITECTURE.md SS2.2 rule 3. It runs the kernel twice from the same seed in
 * virtual time and compares a structural digest of the resulting realm; exit code 1 means the
 * two runs diverged, which is a hard failure, not a tolerance.
 *
 * It touches no RHI, no world and no game mode, so it also demonstrates the layering claim
 * that Sim/ is headless-capable. Prints world-minutes/second so it doubles as a perf number.
 */
UCLASS()
class USolsticeBenchCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USolsticeBenchCommandlet();

	virtual int32 Main(const FString& Params) override;
};
