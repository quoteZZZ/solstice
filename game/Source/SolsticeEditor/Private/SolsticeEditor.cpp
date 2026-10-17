// Solstice - editor module.

#include "SolsticeEditor.h"

#include "Render/SolsticeCelMaterial.h"
#include "Solstice.h"

void FSolsticeEditorModule::StartupModule()
{
	// Hand the generator to the runtime module, which never links against UnrealEd. The runtime
	// calls this only when /Game/Core/M_Cel is missing, which is exactly the fresh-checkout case.
	//
	// Note we do *not* generate on startup: saving a package during PostEngineInit races the asset
	// registry's rescan, and an editor launch should not have side effects. Use the commandlet
	// below - that is the documented rebuild step - or let the runtime build lazily in PIE.
	Solstice::FCelMaterials::SetBuilder([]()
	{
		return FSolsticeCelMaterialBuilder::BuildAll();
	});

	UE_LOG(LogSolstice, Log, TEXT("editor module ready: paper material builder installed"));
}

void FSolsticeEditorModule::ShutdownModule()
{
	Solstice::FCelMaterials::SetBuilder(TFunction<bool()>());
}

USolsticeGenAssetsCommandlet::USolsticeGenAssetsCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 USolsticeGenAssetsCommandlet::Main(const FString& Params)
{
	// BuildAll is idempotent: both packages are skipped when their file is already on disk, so
	// running this on every build costs two FileExists calls.
	const bool bOk = FSolsticeCelMaterialBuilder::BuildAll();

	UE_LOG(LogSolstice, Display, TEXT("gen-assets: %s"), bOk ? TEXT("ok") : TEXT("FAILED"));

	return bOk ? 0 : 1;
}

IMPLEMENT_MODULE(FSolsticeEditorModule, SolsticeEditor)
