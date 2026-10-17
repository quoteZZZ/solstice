// Solstice - UI layer: the HUD actor.

#include "UI/SolsticeHUD.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "God/SolsticePlayerController.h"
#include "UI/SolsticeHUDView.h"

void ASolsticeHUD::BeginPlay()
{
	Super::BeginPlay();

	ASolsticePlayerController* Controller = Cast<ASolsticePlayerController>(PlayerOwner);
	if (!Controller || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	Widget = SNew(SSolsticeHUDView).Controller(Controller);
	GEngine->GameViewport->AddViewportWidgetContent(Widget.ToSharedRef(), 10);
}

void ASolsticeHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	Widget.Reset();

	Super::EndPlay(EndPlayReason);
}
