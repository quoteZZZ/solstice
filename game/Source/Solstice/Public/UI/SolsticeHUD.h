// Solstice - UI layer: the HUD actor (ASolsticeHUD).
//
// This class owns nothing but the widget's lifetime; every pixel is drawn by SSolsticeHUDView in
// SolsticeHUDView.h. There is no UMG asset and no widget blueprint, so the entire interface
// rebuilds from code.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SolsticeHUD.generated.h"

class SSolsticeHUDView;

UCLASS()
class SOLSTICE_API ASolsticeHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<SSolsticeHUDView> Widget;
};
