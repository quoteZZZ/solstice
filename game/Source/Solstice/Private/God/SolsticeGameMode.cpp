// Solstice - God layer: the game mode.

#include "God/SolsticeGameMode.h"

#include "Engine/World.h"
#include "God/SolsticeObserver.h"
#include "God/SolsticePlayerController.h"
#include "UI/SolsticeHUD.h"
#include "World/SolsticeRealm.h"

ASolsticeGameMode::ASolsticeGameMode()
{
	DefaultPawnClass = ASolsticeObserver::StaticClass();
	PlayerControllerClass = ASolsticePlayerController::StaticClass();
	HUDClass = ASolsticeHUD::StaticClass();
	bStartPlayersAsSpectators = false;
}

void ASolsticeGameMode::StartPlay()
{
	UWorld* World = GetWorld();
	if (World && !Realm)
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Realm = World->SpawnActor<ASolsticeRealm>(ASolsticeRealm::StaticClass(), FTransform::Identity, Spawn);
	}

	// Super spawns the player, which is why the realm already exists by the time the controller
	// frames the camera on it.
	Super::StartPlay();
}
