// Solstice - runtime module entry point.
// Module layering and the Sim isolation rule live in docs/TECH.md SS2.1.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

/**
 * Project log category. LogTemp is filtered out of a packaged/headless run, so anything a
 * headless verification run has to prove has to go through a real category.
 */
SOLSTICE_API DECLARE_LOG_CATEGORY_EXTERN(LogSolstice, Log, All);
