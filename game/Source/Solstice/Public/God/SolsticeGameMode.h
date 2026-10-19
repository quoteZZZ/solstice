// Solstice - God layer: the game mode.
//
// There is no level content. The realm is spawned from code, which is what keeps the promise in
// docs/TECH.md SS0: everything in Content/ can be rebuilt from seed + generator +
// contract, so nothing that matters is a binary asset.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SolsticeGameMode.generated.h"

class ASolsticeRealm;

UCLASS()
class SOLSTICE_API ASolsticeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASolsticeGameMode();

	virtual void StartPlay() override;

	ASolsticeRealm* GetRealm() const { return Realm; }

protected:
	UPROPERTY(Transient) TObjectPtr<ASolsticeRealm> Realm;
};
