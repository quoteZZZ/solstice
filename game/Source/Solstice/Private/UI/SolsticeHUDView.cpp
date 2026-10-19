// Solstice - UI layer: the Slate HUD.

#include "UI/SolsticeHUDView.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Core/SolsticeGameInstance.h"
#include "Core/SolsticeLocalization.h"
#include "God/SolsticePlayerController.h"
#include "Sim/SolsticeKernel.h"
#include "Styling/CoreStyle.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace Solstice;

namespace
{
	// ---- the paper chrome palette -------------------------------------------------
	const FLinearColor PanelBg(0.960f, 0.944f, 0.910f, 0.92f);
	const FLinearColor PanelBgDeep(0.930f, 0.906f, 0.860f, 0.95f);
	const FLinearColor PanelEdge(0.44f, 0.39f, 0.32f, 0.90f);
	const FLinearColor Ink(0.155f, 0.135f, 0.115f, 1.0f);
	const FLinearColor InkSoft(0.400f, 0.355f, 0.305f, 1.0f);
	const FLinearColor Accent(0.780f, 0.360f, 0.120f, 1.0f);

	// Cel chrome geometry, shared by every panel and button so the whole HUD cuts the same way.
	constexpr float CelCorner = 4.0f;
	constexpr float CelOutline = 1.25f;

	// Panels are cut-paper cards: warm fill *plus* the same hairline brown edge the buttons use.
	// A flat flood fill with no outline was the single biggest reason the chrome read as cheap.
	const FSlateBrush* PanelBrush()
	{
		static const FSlateRoundedBoxBrush Brush(PanelBg, CelCorner, PanelEdge, CelOutline);
		return &Brush;
	}

	const FSlateBrush* PanelDeepBrush()
	{
		static const FSlateRoundedBoxBrush Brush(PanelBgDeep, CelCorner, PanelEdge, CelOutline);
		return &Brush;
	}

	const FSlateBrush* SolidBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	// Same cut-paper edge, but with a white fill so the call sites that tint through
	// SBorder::BorderBackgroundColor keep working unchanged (white multiplies to their tint).
	const FSlateBrush* WhiteBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, CelCorner, PanelEdge, CelOutline);
		return &Brush;
	}

	// Chips float on coloured surfaces (hand cards), so they stay translucent white and only
	// gain a hairline ink outline. Hover darkens the outline, not the fill.
	const FButtonStyle& ChipStyle()
	{
		FLinearColor Edge = PanelEdge; Edge.A = 0.30f;
		FLinearColor EdgeHover = PanelEdge; EdgeHover.A = 0.70f;
		FLinearColor EdgePressed = PanelEdge; EdgePressed.A = 0.90f;
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.35f), CelCorner, Edge, CelOutline))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f), CelCorner, EdgeHover, CelOutline))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.95f), CelCorner, EdgePressed, CelOutline))
			.SetNormalPadding(FMargin(1.0f))
			.SetPressedPadding(FMargin(1.0f));
		return Style;
	}

	// Action buttons are cut-paper tiles: solid warm fill, brown edge, hover lifts toward full
	// paper white, press sinks to the deep panel tone with an ochre edge.
	const FButtonStyle& ActionStyle()
	{
		FLinearColor Edge = PanelEdge; Edge.A = 0.80f;
		const FLinearColor HoverFill(0.990f, 0.975f, 0.945f, 1.0f);
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(PanelBg, CelCorner, Edge, CelOutline))
			.SetHovered(FSlateRoundedBoxBrush(HoverFill, CelCorner, PanelEdge, CelOutline))
			.SetPressed(FSlateRoundedBoxBrush(PanelBgDeep, CelCorner, Accent, CelOutline))
			.SetNormalPadding(FMargin(11.0f, 5.0f))
			.SetPressedPadding(FMargin(11.0f, 6.0f));
		return Style;
	}

	// A hand card is not a chip: it is a paper card you pick up. The whole face is the button, so
	// hover lifts the stock and press sinks it, with the ochre edge reserved for the press.
	const FButtonStyle& CardStyle()
	{
		FLinearColor Edge = PanelEdge; Edge.A = 0.55f;
		const FLinearColor HoverFill(0.985f, 0.972f, 0.945f, 0.98f);
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(PanelBg, CelCorner, Edge, CelOutline))
			.SetHovered(FSlateRoundedBoxBrush(HoverFill, CelCorner, PanelEdge, CelOutline))
			.SetPressed(FSlateRoundedBoxBrush(PanelBgDeep, CelCorner, Accent, CelOutline))
			.SetNormalPadding(FMargin(0.0f))
			.SetPressedPadding(FMargin(0.0f));
		return Style;
	}

	// ---- resolution-independent metrics --------------------------------
	//
	// Everything below is authored at **1080p** and multiplied by one number taken from the
	// viewport height. This is the whole readability bug: a 2560x1440 window used to draw the
	// same pixel sizes as a 1280x720 one, so a 12 pt label arrived at two thirds of its intended
	// size and the entire HUD read as a thin strip of unreadable text. Slate's own DPI curve does
	// not apply to a widget added straight to the viewport, so the scale is done here.
	float GUiScale = 1.0f;

	void SetUiScaleFromViewport()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return;
		}
		FViewport* Viewport = GEngine->GameViewport->Viewport;
		if (!Viewport)
		{
			return;
		}
		const FIntPoint Size = Viewport->GetSizeXY();
		if (Size.Y >= 200)
		{
			// 0.85 is a floor for small windows; the ceiling keeps a 4K screen from turning the
			// hand into a wall of billboards.
			GUiScale = FMath::Clamp(static_cast<float>(Size.Y) / 1080.0f, 0.85f, 2.0f);
		}
	}

	/** Layout units -> pixels. */
	float Px(float Value) { return Value * GUiScale; }

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		const int32 Scaled = FMath::Max(9, FMath::RoundToInt(static_cast<float>(Size) * GUiScale));
		return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Scaled);
	}

	FText Tx(const FString& In) { return FText::FromString(In); }
	FText Num(int32 In) { return FText::FromString(FString::FromInt(In)); }
	FText Dec1(float In) { return FText::FromString(FString::Printf(TEXT("%.1f"), In)); }
	FText Percent(float In) { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(In * 100.0f))); }

	/** Weather as one word. Shared by the status capsule and the full clock panel. */
	FText WeatherLabel(const Solstice::FSkyState& Sky)
	{
		const TCHAR* Key =
			Sky.GetWeather() == Solstice::EWeather::Clear ? TEXT("weather.clear") :
			Sky.GetWeather() == Solstice::EWeather::Cloudy ? TEXT("weather.cloudy") :
			Sky.GetWeather() == Solstice::EWeather::LightRain ? TEXT("weather.light_rain") :
			Sky.GetWeather() == Solstice::EWeather::Storm ? TEXT("weather.storm") : TEXT("weather.fog");
		return FText::FromString(Solstice::Loc(Key));
	}

	TSharedRef<STextBlock> Txt(const FText& Text, int32 Size = 12, const FLinearColor& Colour = Ink, bool bBold = false)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font(Size, bBold))
			.ColorAndOpacity(FSlateColor(Colour))
			.Visibility(EVisibility::HitTestInvisible);
	}

	TSharedRef<STextBlock> TxtL(TAttribute<FText> Text, int32 Size = 12, const FLinearColor& Colour = Ink, bool bBold = false)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font(Size, bBold))
			.ColorAndOpacity(FSlateColor(Colour))
			.Visibility(EVisibility::HitTestInvisible);
	}

	/** Cream text for saturated fills. A 1 px shadow is what makes white legible on gold. */
	TSharedRef<STextBlock> TxtShadow(const FText& Text, int32 Size)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font(Size, true))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.99f, 0.96f, 1.0f)))
			.ShadowOffset(FVector2D(0.0f, 1.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.50f))
			.Visibility(EVisibility::HitTestInvisible);
	}

	TSharedRef<STextBlock> TxtShadowL(TAttribute<FText> Text, int32 Size)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font(Size, true))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.99f, 0.96f, 1.0f)))
			.ShadowOffset(FVector2D(0.0f, 1.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.50f))
			.Visibility(EVisibility::HitTestInvisible);
	}

	/** Key on the left in soft ink, live value on the right in full ink. */
	TSharedRef<SWidget> Row(const FString& Key, TAttribute<FText> Value)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, Px(3.0f))
			[
				Txt(Tx(Loc(Key)), 13, InkSoft)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Px(12.0f), Px(3.0f))
			[
				// R-09: a zero is not a reading. Dim it so the eye skips it and lands on whatever
				// actually moved - the panel used to give seven rows of "0" the same weight as a
				// live number, which is why it read as empty at minute three.
				SNew(STextBlock)
				.Text(Value)
				.Font(Font(13, true))
				.ColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([Value]()
				{
					const FString Text = Value.Get().ToString();
					const bool bZero = Text == TEXT("0") || Text == TEXT("0%") || Text == TEXT("0.0");
					return FSlateColor(bZero ? InkSoft : Ink);
				}))
				.Visibility(EVisibility::HitTestInvisible)
			];
	}

	/** A titled parchment panel. Everything inside is hit-test invisible. */
	TSharedRef<SWidget> Panel(const FText& Title, TSharedRef<SWidget> Body, float Width)
	{
		// R-03: 浅纸色世界上，面板纸背 + 1px 纸边的对比只有约 1.2–1.6:1（工单实测 1.17 / 1.56，
		// 低于 WCAG 非文本元素 3:1）。加深描边会破坏纸感，所以改成"抬起来"：外面套一层极淡的
		// 暖灰投影，只在下缘露出 2px，既有分离度又不抢纸的调子。
		static const FSlateRoundedBoxBrush ShadowBrush(
			FLinearColor(0.35f, 0.30f, 0.24f, 0.13f), CelCorner + 1.0f);

		return SNew(SBox)
			.WidthOverride(Px(Width))
			[
				SNew(SBorder)
				.BorderImage(&ShadowBrush)
				.Padding(FMargin(Px(1.0f), Px(1.0f), Px(1.0f), Px(2.0f)))
				[
				SNew(SBorder)
				.BorderImage(PanelBrush())
				.Padding(FMargin(Px(14.0f), Px(11.0f)))
				.Visibility(EVisibility::SelfHitTestInvisible)
				[
					SNew(SVerticalBox)
					// A rule under the title, not just a colour change: a paper panel with a
					// heading floating in the same field as its rows reads as one paragraph.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Px(4.0f))
					[
						Txt(Title, 15, Accent, true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Px(7.0f))
					[
						SNew(SBox).HeightOverride(Px(1.0f))
						[
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FSlateColor(FLinearColor(0.44f, 0.39f, 0.32f, 0.28f)))
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Body
					]
				]
				]
			];
	}

	/** A key cap: dark chip, cream legend. Sits inside the buttons that have a binding. */
	TSharedRef<SWidget> Keycap(const FString& Key)
	{
		static const FSlateRoundedBoxBrush CapBrush(
			FLinearColor(0.20f, 0.18f, 0.16f, 0.92f), 3.0f);
		return SNew(SBorder)
			.BorderImage(&CapBrush)
			.Padding(FMargin(Px(6.0f), Px(2.0f)))
			.Visibility(EVisibility::HitTestInvisible)
			[
				Txt(Tx(Key), 10, FLinearColor(0.97f, 0.95f, 0.90f, 1.0f), true)
			];
	}

	/** Action button: optional key cap, then the verb. */
	TSharedRef<SWidget> ActionButton(const FText& Label, FOnClicked OnClicked,
		const FString& Key = FString())
	{
		TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
		if (!Key.IsEmpty())
		{
			Content->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				Keycap(Key)
			];
		}
		Content->AddSlot().AutoWidth().VAlign(VAlign_Center)
			.Padding(Key.IsEmpty() ? FMargin(0.0f) : FMargin(Px(7.0f), 0.0f, 0.0f, 0.0f))
		[
			Txt(Label, 12, Ink, true)
		];

		return SNew(SButton)
			.ButtonStyle(&ActionStyle())
			.OnClicked(OnClicked)
			.ContentPadding(FMargin(Px(3.0f)))
			[
				Content
			];
	}

	TSharedRef<SWidget> SmallButton(const FText& Label, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.ButtonStyle(&ChipStyle())
			.OnClicked(OnClicked)
			.ContentPadding(FMargin(Px(6.0f), Px(3.0f)))
			[
				Txt(Label, 12, Ink, false)
			];
	}

	/** A hairline between action groups. */
	TSharedRef<SWidget> GroupDivider()
	{
		return SNew(SBox)
			.WidthOverride(Px(1.0f))
			.HeightOverride(Px(20.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.44f, 0.39f, 0.32f, 0.22f)))
			];
	}
}

FVector2D SSolsticeHUDView::ComputeDesiredSize(float) const
{
	return FVector2D::ZeroVector;
}

USolsticeGameInstance* SSolsticeHUDView::GI() const
{
	ASolsticePlayerController* PlayerCtl = PC();
	return PlayerCtl ? PlayerCtl->Game() : nullptr;
}

FSimKernel* SSolsticeHUDView::Sim() const
{
	USolsticeGameInstance* Instance = GI();
	return Instance ? &Instance->Kernel() : nullptr;
}

void SSolsticeHUDView::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;

	// Before any widget below asks for a size. Everything in this file is authored at 1080p and
	// scaled here, so the HUD is the same physical size on a 1280 window and a 4K one.
	SetUiScaleFromViewport();

	ChildSlot
	[
		SNew(SOverlay)
		// The whole HUD collapses when the controller has hidden the UI (Tab), so screenshots
		// and inspection see the bare paper world.
		.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
		{
			ASolsticePlayerController* PlayerCtl = PC();
			return (!PlayerCtl || PlayerCtl->IsUIOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
		}))

		// ---- top left: one-line status, full read-out on demand ---------------
		//
		// The capsule is always there; the two panels behind it are not. They are 24 rows of
		// numbers in two 244px cards, and on the first screen after "new game" they covered the
		// left third of the frame - which is why the game read as a debug overlay rather than as
		// a place. T opens them for anyone who wants the numbers.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(Px(16.0f)))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildStatusCapsule()
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					ASolsticePlayerController* PlayerCtl = PC();
					return (PlayerCtl && PlayerCtl->IsDetailsOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
				}))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						BuildClockPanel()
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						BuildRealmPanel()
					]
				]
			]
		]

		// ---- top right: one number, and the table only on demand -----------------
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(Px(16.0f)))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				BuildEchoChip()
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, Px(8.0f), 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					ASolsticePlayerController* PlayerCtl = PC();
					return (PlayerCtl && PlayerCtl->IsDetailsOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
				}))
				[
					BuildEchoPanel()
				]
			]
			// Secondary panels are opt-in. The controller already owned these five flags; nothing
			// read them, so B / C / P / G did nothing and every panel was always on screen. This is
			// the missing half: the default view is clock + realm + echo + hand, nothing else.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					ASolsticePlayerController* PlayerCtl = PC();
					return (PlayerCtl && PlayerCtl->IsPacksOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
				}))
				[
					BuildPackPanel()
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					ASolsticePlayerController* PlayerCtl = PC();
					return (PlayerCtl && PlayerCtl->IsChronicleOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
				}))
				[
					BuildChroniclePanel()
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					ASolsticePlayerController* PlayerCtl = PC();
					return (PlayerCtl && PlayerCtl->IsPoolOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
				}))
				[
					BuildPoolPanel()
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
				{
					ASolsticePlayerController* PlayerCtl = PC();
					return (PlayerCtl && PlayerCtl->IsCreatorOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
				}))
				[
					BuildCreatorPanel()
				]
			]
		]

		// ---- top centre: status ---------------------------------------------
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(Px(16.0f), Px(18.0f)))
		[
			BuildStatusLine()
		]

		// ---- bottom centre: hand + actions ----------------------------------
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(Px(16.0f)))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				BuildHandBar()
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildActionBar()
			]
		]

		// ---- bottom left: controls ------------------------------------------
		// A-03: the help sheet is off by default. UI-UX §1 asks for "world first, chrome folded
		// away", and a thirteen-line key table parked on a pale paper world was the most visible
		// thing on screen; H brings it back.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(Px(16.0f)))
		[
			SNew(SBox)
			.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
			{
				ASolsticePlayerController* PlayerCtl = PC();
				return (PlayerCtl && PlayerCtl->IsHelpOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
			}))
			[
				BuildHelpPanel()
			]
		]

		// ---- the title card, above everything -------------------------------
		//
		// Last in the overlay, so it covers the chrome as well as the world: a player who has
		// never seen this build should not be reading a hand of cards before anyone has told
		// them what a card does.
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
			{
				ASolsticePlayerController* PlayerCtl = PC();
				return (PlayerCtl && PlayerCtl->IsStartOpen())
					? EVisibility::SelfHitTestInvisible
					: EVisibility::Collapsed;
			}))
			[
				BuildStartScreen()
			]
		]
	];
}

// ======================================================================
// Panels
// ======================================================================

TSharedRef<SWidget> SSolsticeHUDView::BuildStartScreen()
{
	// ---- title card ------------------------------------------------------
	//
	// This is the answer to "我只知道一个游戏怎么做，但不知道怎么开始游玩". Until now the build
	// opened straight onto a live island with eight cards in a strip and no sentence anywhere
	// telling anyone what the cards were for. A player's first ten seconds were spent reading a
	// HUD that was never introduced.
	//
	// Three things, in the order they are needed: what this is, what you do, and how to start.
	// No key table - keys live in the help sheet, which this card links to.

	auto Step = [this](int32 Number, const FString& Zh, const FString& En) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[
				SNew(SBox).WidthOverride(Px(26.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%d"), Number)))
					.Font(Font(13, true))
					.ColorAndOpacity(FSlateColor(Accent))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Solstice::Loc2(Zh, En)))
				.Font(Font(13))
				.ColorAndOpacity(FSlateColor(Ink))
				.AutoWrapText(true)
			];
	};

	TSharedRef<SWidget> Card = SNew(SBorder)
		.BorderImage(PanelBrush())
		.Padding(FMargin(Px(34.0f), Px(30.0f), Px(34.0f), Px(26.0f)))
		[
			SNew(SVerticalBox)
			// title
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("SOLSTICE")))
				.Font(Font(30, true))
				.ColorAndOpacity(FSlateColor(Ink))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Px(2.0f), 0.0f, Px(14.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Solstice::Loc2(
					TEXT("至日 · 在一张纸上养一座岛"), TEXT("a world on a sheet of paper"))))
				.Font(Font(13))
				.ColorAndOpacity(FSlateColor(InkSoft))
			]
			// the loop
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Px(8.0f))
			[
				Step(1, TEXT("下面是一手牌。每张牌是一种干预：雨、火、种子、风。"),
					TEXT("The strip at the bottom is your hand. Each card is an intervention: rain, fire, seed, wind."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Px(8.0f))
			[
				Step(2, TEXT("点牌选中，再点地面放下。光圈会告诉你落点在哪一格。"),
					TEXT("Pick a card, then click the ground. The ring shows the tile it will land on."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Px(18.0f))
			[
				Step(3, TEXT("世界会自己往下走。你等的不是结算，是它长出什么。"),
					TEXT("The world runs on its own. What you wait for is not a score - it is what grows."))
			]
			// buttons
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&ActionStyle())
					.ContentPadding(FMargin(Px(10.0f), Px(8.0f)))
					.OnClicked(FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* PlayerCtl = PC()) { PlayerCtl->DismissStart(); }
						return FReply::Handled();
					}))
					[
						SNew(STextBlock)
						.Text(FText::FromString(Solstice::Loc2(TEXT("进入世界"), TEXT("enter the world"))))
						.Font(Font(14, true))
						.ColorAndOpacity(FSlateColor(Ink))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(Px(8.0f), 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&ChipStyle())
					.ContentPadding(FMargin(Px(10.0f), Px(8.0f)))
					.OnClicked(FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* PlayerCtl = PC())
						{
							PlayerCtl->DismissStart();
							PlayerCtl->ToggleHelp();
						}
						return FReply::Handled();
					}))
					[
						SNew(STextBlock)
						.Text(FText::FromString(Solstice::Loc2(TEXT("按键说明"), TEXT("controls"))))
						.Font(Font(13))
						.ColorAndOpacity(FSlateColor(Ink))
					]
				]
			]
		];

	return SNew(SBorder)
		.BorderImage(SolidBrush())
		// A dark wash over the world, not a black screen: the island stays visible behind the
		// card, so the first thing a new player sees is still the thing the game is about.
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.055f, 0.070f, 0.085f, 0.62f)))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(Px(560.0f))
			[
				Card
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildStatusCapsule()
{
	// No box.
	//
	// The top-left was a warm paper card with a 22 pt clock, a rule and two caption lines - a
	// window sitting on the landscape, which is the report "左上角同理也烂". A HUD read-out in a
	// game like this is type on the picture, not a widget on it: two lines, ink with a pale
	// shadow so it survives both the sky and the dark canopy, and nothing else.
	TSharedRef<SWidget> Clock = SNew(STextBlock)
		.Text(TAttribute<FText>::CreateLambda([this]()
		{
			FSimKernel* Kernel = Sim();
			if (!Kernel) { return FText(); }
			return FText::FromString(FString::Printf(TEXT("%02d:%02d"),
				Kernel->GetWorldHour(), Kernel->GetWorldMinuteOfHour()));
		}))
		.Font(Font(20, true))
		.ColorAndOpacity(FSlateColor(Ink))
		.ShadowOffset(FVector2D(0.0f, 1.0f))
		.ShadowColorAndOpacity(FLinearColor(1.0f, 0.98f, 0.94f, 0.55f))
		.Visibility(EVisibility::HitTestInvisible);

	TSharedRef<SWidget> Sub = SNew(STextBlock)
		.Text(TAttribute<FText>::CreateLambda([this]()
		{
			FSimKernel* Kernel = Sim();
			if (!Kernel) { return FText(); }
			const FString Day = Solstice::Loc2(
				FString::Printf(TEXT("第 %d 天"), Kernel->GetWorldDay()),
				FString::Printf(TEXT("day %d"), Kernel->GetWorldDay()));
			return FText::FromString(FString::Printf(TEXT("%s · %s · %s %.0f°"),
				*Day, *Kernel->GetSeasonString(),
				*WeatherLabel(Kernel->GetSky()).ToString(), Kernel->GetSky().AirTempC));
		}))
		.Font(Font(11))
		.ColorAndOpacity(FSlateColor(Ink))
		.ShadowOffset(FVector2D(0.0f, 1.0f))
		.ShadowColorAndOpacity(FLinearColor(1.0f, 0.98f, 0.94f, 0.55f))
		.Visibility(EVisibility::HitTestInvisible);

	return SNew(SVerticalBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SVerticalBox::Slot().AutoHeight()[ Clock ]
		+ SVerticalBox::Slot().AutoHeight()[ Sub ];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildEchoChip()
{
	// One number, top right.
	//
	// This used to be a six-row accounting table - from events, from sediment, from first hand,
	// from recycling, auto-converted, total - parked in the corner of every frame. That is a
	// debug panel wearing a game's clothes, and it was the single most "this is not a game UI"
	// element on screen. The breakdown still exists; it moved into the details overlay (I).
	TSharedRef<SWidget> Number = SNew(STextBlock)
		.Text(TAttribute<FText>::CreateLambda([this]()
		{
			FSimKernel* Kernel = Sim();
			return FText::FromString(Kernel ? FString::FromInt(Kernel->GetEcho()) : FString());
		}))
		.Font(Font(20, true))
		.ColorAndOpacity(FSlateColor(Ink))
		.ShadowOffset(FVector2D(0.0f, 1.0f))
		.ShadowColorAndOpacity(FLinearColor(1.0f, 0.98f, 0.94f, 0.55f))
		.Visibility(EVisibility::HitTestInvisible);

	TSharedRef<SWidget> Label = SNew(STextBlock)
		.Text(Tx(Solstice::Loc(TEXT("echo.balance"))))
		.Font(Font(10))
		.ColorAndOpacity(FSlateColor(InkSoft))
		.ShadowOffset(FVector2D(0.0f, 1.0f))
		.ShadowColorAndOpacity(FLinearColor(1.0f, 0.98f, 0.94f, 0.50f))
		.Visibility(EVisibility::HitTestInvisible);

	return SNew(SHorizontalBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Label ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Px(8.0f), 0.0f, 0.0f, 0.0f)
		[
			Number
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildClockPanel()
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	Body->AddSlot().AutoHeight()[ Row(TEXT("game.realm"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? FText::FromString(Solstice::Loc2(Kernel->GetConfig().RealmNameZh, Kernel->GetConfig().RealmNameEn)) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("time.clock"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? FText::FromString(Kernel->GetClockString()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("time.real_date"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? FText::FromString(Kernel->GetRealDateString()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("time.season"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? FText::FromString(Kernel->GetSeasonString()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("time.mode"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		if (!Kernel) { return FText::GetEmpty(); }
		const bool bReal = Kernel->GetTimeMode() == ETimeMode::Real;
		const FString Mode = bReal ? Solstice::Loc(TEXT("time.real")) : Solstice::Loc(TEXT("time.virtual"));
		const FString State = Kernel->IsPaused() ? Solstice::Loc(TEXT("time.paused")) : Solstice::Loc(TEXT("time.running"));
		if (bReal)
		{
			return FText::FromString(FString::Printf(TEXT("%s · %s"), *Mode, *State));
		}
		const int32 SpeedIndex = Kernel->GetSpeedIndex();
		const FString Speed = SpeedIndex >= 2 ? FString::Printf(TEXT("%s x%d"), *State, SpeedIndex == 3 ? 20 : 4) : State;
		return FText::FromString(FString::Printf(TEXT("%s · %s"), *Mode, *Speed));
	})) ];

	Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[ Row(TEXT("sky.weather"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		if (!Kernel) { return FText::GetEmpty(); }
		const FSkyState& Sky = Kernel->GetSky();
		const FString Glyph = Solstice::Loc(FString::Printf(TEXT("weather.%s"),
			Sky.GetWeather() == EWeather::Clear ? TEXT("clear") :
			Sky.GetWeather() == EWeather::Cloudy ? TEXT("cloudy") :
			Sky.GetWeather() == EWeather::LightRain ? TEXT("light_rain") :
			Sky.GetWeather() == EWeather::Storm ? TEXT("storm") : TEXT("fog")));
		return FText::FromString(Sky.bSnowing ? FString::Printf(TEXT("%s / %s"), *Glyph, *Solstice::Loc(TEXT("sky.snowing"))) : Glyph);
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("sky.temperature"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Tx(FString::Printf(TEXT("%.1f °C"), Kernel->GetSky().AirTempC)) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("sky.humidity"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Percent(Kernel->GetSky().Humidity) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("sky.wind"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Tx(FString::Printf(TEXT("%.1f m/s · %d°"), Kernel->GetSky().WindSpeed, FMath::RoundToInt(Kernel->GetSky().WindDirDeg))) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("sky.water_level"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Dec1(Kernel->GetSky().WaterLevel) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("sky.city"), TAttribute<FText>::CreateLambda([this]()
	{
		USolsticeGameInstance* Instance = GI();
		if (!Instance) { return FText::GetEmpty(); }
		const bool bOnline = Instance->Anchor().IsWeatherFromNetwork();
		return Tx(FString::Printf(TEXT("%s · %s"), *Instance->Anchor().GetCityName(),
			*Solstice::Loc(bOnline ? TEXT("sky.reality_online") : TEXT("sky.reality_offline"))));
	})) ];

	return Panel(Tx(Solstice::Loc(TEXT("time.mode"))), Body, 280.0f);
}

TSharedRef<SWidget> SSolsticeHUDView::BuildRealmPanel()
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	Body->AddSlot().AutoHeight()[ Row(TEXT("realm.population_total"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Num(Kernel->GetPopulationTotal()) : FText::GetEmpty();
	})) ];

	for (int32 Kind = 0; Kind < static_cast<int32>(ESpeciesKind::Count); ++Kind)
	{
		const ESpeciesKind Species = static_cast<ESpeciesKind>(Kind);
		// SpeciesId() is an *internal* id ("sp.moss-grass"); the l10n namespace is just
		// "species.moss-grass". Building the key straight from the internal id is what leaked
		// raw keys into the panel (REVIEW R-05), so drop the "sp." segment here.
		FString SpeciesName = Solstice::SpeciesId(Species);
		SpeciesName.RemoveFromStart(TEXT("sp."));
		const FString Key = FString::Printf(TEXT("species.%s"), *SpeciesName);
		Body->AddSlot().AutoHeight().Padding(12.0f, 0.0f, 0.0f, 0.0f)[ Row(Key, TAttribute<FText>::CreateLambda([this, Species]()
		{
			FSimKernel* Kernel = Sim();
			return Kernel ? Num(Kernel->GetPopulation(Species)) : FText::GetEmpty();
		})) ];
	}

	Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[ Row(TEXT("realm.vegetation"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Percent(Kernel->GetVegetationMean()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("realm.fertility"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Percent(Kernel->GetFertilityMean()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("realm.biomass"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Percent(Kernel->GetBiomassNorm()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("realm.diversity"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Percent(Kernel->GetDiversityIndex()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("realm.stability"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Percent(Kernel->GetStabilityIndex()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("realm.potential"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Dec1(Kernel->GetEcoPotential()) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("realm.painted"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		if (!Kernel) { return FText::GetEmpty(); }
		const FString State = Kernel->IsChaos() ? Solstice::Loc(TEXT("realm.chaos")) : Solstice::Loc(TEXT("realm.alive"));
		return Tx(FString::Printf(TEXT("%s · %s"), *Percent(Kernel->GetPaintedFraction()).ToString(), *State));
	})) ];

	// A-04: no Chinese game title, so this panel stops borrowing `game.title` (which would have
	// printed the project name as a heading over a population readout). `realm.population` was
	// already in l10n and unused - this is the line it was waiting for.
	return Panel(Tx(Solstice::Loc(TEXT("realm.population"))), Body, 280.0f);
}

TSharedRef<SWidget> SSolsticeHUDView::BuildEchoPanel()
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	Body->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			TxtL(TAttribute<FText>::CreateLambda([this]()
			{
				FSimKernel* Kernel = Sim();
				return Kernel ? Num(Kernel->GetEcho()) : Num(0);
			}), 22, Ink, true)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(8.0f, 0.0f, 0.0f, 3.0f)
		[
			Txt(Tx(Solstice::Loc(TEXT("echo.balance"))), 12, InkSoft)
		]
	];

	// At minute one every counter is zero and the panel is a column of nothing. One sentence
	// that says so is worth more than six dimmed digits; it disappears the moment anything moves.
	Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Px(6.0f))
	[
		TxtL(TAttribute<FText>::CreateLambda([this]()
		{
			FSimKernel* Kernel = Sim();
			if (!Kernel) { return FText::GetEmpty(); }
			const FEchoLedger& Ledger = Kernel->GetLedger();
			const int32 Total = Ledger.FromEvent + Ledger.FromSediment + Ledger.FromFirsthand
				+ Ledger.FromRecycle + Ledger.FromAutoConvert;
			return Total > 0 ? FText::GetEmpty() : Tx(Solstice::Loc(TEXT("echo.quiet")));
		}), 11, InkSoft)
	];

	Body->AddSlot().AutoHeight()[ Row(TEXT("echo.from_event"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Num(Kernel->GetLedger().FromEvent) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("echo.from_sediment"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Num(Kernel->GetLedger().FromSediment) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("echo.from_firsthand"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Num(Kernel->GetLedger().FromFirsthand) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("echo.from_recycle"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Num(Kernel->GetLedger().FromRecycle + Kernel->GetLedger().FromAutoConvert) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight()[ Row(TEXT("echo.per_minute"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		return Kernel ? Tx(FString::Printf(TEXT("%d / %d"), Kernel->GetEventEchoThisMinute(), Tuning::EchoPerMinuteCap)) : FText::GetEmpty();
	})) ];
	Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 2.0f)[ Row(TEXT("echo.disturbance"), TAttribute<FText>::CreateLambda([this]()
	{
		FSimKernel* Kernel = Sim();
		if (!Kernel) { return FText::GetEmpty(); }
		return Tx(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Kernel->GetDisturbance())));
	})) ];
	// R-10: a pale paper track with an ochre fill, instead of the engine's dark ProgressBar skin -
	// at zero disturbance that skin painted a solid black bar right under "扰动 0%", which read as
	// a broken widget. Now the fill is simply absent at zero and only the track shows.
	Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SBox)
		.HeightOverride(Px(10.0f))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.855f, 0.825f, 0.775f, 0.90f)))
			]
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]()
				{
					FSimKernel* Kernel = Sim();
					return Kernel ? FMath::Clamp(Kernel->GetDisturbance() / 100.0f, 0.0f, 1.0f) : 0.0f;
				}))
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(FSlateColor(Accent))
				]
				+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]()
				{
					FSimKernel* Kernel = Sim();
					return Kernel ? 1.0f - FMath::Clamp(Kernel->GetDisturbance() / 100.0f, 0.0f, 1.0f) : 1.0f;
				}))
				[
					SNullWidget::NullWidget
				]
			]
		]
	];
	Body->AddSlot().AutoHeight()
	[
		TxtL(TAttribute<FText>::CreateLambda([this]()
		{
			FSimKernel* Kernel = Sim();
			if (!Kernel || Kernel->GetDisturbance() < Tuning::DisturbancePenaltyThreshold)
			{
				return FText::GetEmpty();
			}
			return Tx(Solstice::Loc(TEXT("echo.disturbance_high")));
		}), 11, Accent)
	];

	// Calming is the only echo sink besides packs, so it lives next to the meter it drains.
	Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SmallButton(Tx(Solstice::Loc(TEXT("calm.one"))), FOnClicked::CreateLambda([this]()
			{
				if (ASolsticePlayerController* Controller = PC()) { Controller->CalmDisturbance(1); }
				return FReply::Handled();
			}))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SmallButton(Tx(Solstice::Loc(TEXT("calm.two"))), FOnClicked::CreateLambda([this]()
			{
				if (ASolsticePlayerController* Controller = PC()) { Controller->CalmDisturbance(2); }
				return FReply::Handled();
			}))
		]
	];

	return Panel(Tx(Solstice::Loc(TEXT("echo.balance"))), Body, 280.0f);
}

TSharedRef<SWidget> SSolsticeHUDView::BuildStatusLine()
{
	// Collapsed when there is nothing to say.
	//
	// It used to be an always-present pale pill with an empty string inside, which at the top
	// centre of the frame reads as a stray white rectangle floating in the sky. A toast that is
	// only there when it has news is the whole point of a toast.
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FSlateColor(PanelBgDeep))
		.Padding(FMargin(Px(14.0f), Px(8.0f), Px(18.0f), Px(8.0f)))
		.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
		{
			ASolsticePlayerController* Controller = PC();
			return (Controller && Controller->HasStatus())
				? EVisibility::SelfHitTestInvisible
				: EVisibility::Collapsed;
		}))
		[
			SNew(SHorizontalBox)
			// The ochre edge is the only saturated thing in the chrome, so the eye finds the
			// newest message without a second colour being spent on it.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(Px(3.0f)).HeightOverride(Px(18.0f))
				[
					SNew(SBorder)
					.BorderImage(SolidBrush())
					.BorderBackgroundColor(FSlateColor(Accent))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Px(10.0f), 0.0f, 0.0f, 0.0f)
			[
				TxtL(TAttribute<FText>::CreateLambda([this]()
				{
					ASolsticePlayerController* Controller = PC();
					if (!Controller || !Controller->HasStatus())
					{
						return FText::GetEmpty();
					}
					return FText::FromString(Solstice::Loc2(Controller->GetStatusZh(), Controller->GetStatusEn()));
				}), 13, Ink, true)
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::CardChip(int32 Slot)
{
	// A card, not a table cell.
	//
	// The old face was three 9-12 pt lines of the same weight inside a 128x72 box with a 7 px
	// colour sliver on the left: at a glance every card in the hand was the same grey rectangle,
	// which is the "crude UI" complaint in one sentence. The new face has four bands, each with
	// one job:
	//
	//   element strip   what kind of thing this is (colour + name, the only saturated field)
	//   value           the decision number, at 22 pt - the largest type in the whole HUD
	//   name            what it is, 14 pt bold
	//   tags            class and duration, 10 pt, soft - reference, not reading
	//
	// 150 x 140 at 1080p, so eight of them still fit under a 1920 frame.
	const FLinearColor Clear(0.0f, 0.0f, 0.0f, 0.0f);

	TAttribute<FSlateColor> FrameColour = TAttribute<FSlateColor>::CreateLambda([this, Slot, Clear]()
	{
		ASolsticePlayerController* Controller = PC();
		return FSlateColor((Controller && Controller->GetSelectedSlot() == Slot) ? Accent : Clear);
	});

	TAttribute<FSlateColor> StripColour = TAttribute<FSlateColor>::CreateLambda([this, Slot]()
	{
		FSimKernel* Kernel = Sim();
		const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
		if (CardIndex == INDEX_NONE)
		{
			return FSlateColor(FLinearColor(0.72f, 0.70f, 0.66f, 1.0f));
		}
		const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
		if (!Card)
		{
			return FSlateColor(FLinearColor(0.72f, 0.70f, 0.66f, 1.0f));
		}
		FLinearColor Band = Solstice::ElementColor(Card->GetElement());
		// Darkened before the white legend goes on it: the element colours are authored as fills,
		// and cream text on a 0.85-luminance yellow is unreadable.
		// 0.82, not 0.62: the label now carries its own shadow, so the band can keep the chroma
		// that identifies the element. At 0.62 every element arrived as the same mud.
		return FSlateColor(FLinearColor(Band.R * 0.82f, Band.G * 0.82f, Band.B * 0.82f, 1.0f));
	});

	// ---- element strip ---------------------------------------------------
	static const FSlateRoundedBoxBrush RibbonBrush(FLinearColor::White, 3.0f);
	TSharedRef<SWidget> Strip = SNew(SBorder)
		.BorderImage(&RibbonBrush)
		.BorderBackgroundColor(StripColour)
		.Padding(FMargin(Px(9.0f), Px(5.0f)))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				TxtShadowL(TAttribute<FText>::CreateLambda([this, Slot]()
				{
					FSimKernel* Kernel = Sim();
					const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
					if (CardIndex == INDEX_NONE) { return Tx(Solstice::Loc(TEXT("hand.empty"))); }
					const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
					if (!Card) { return FText::GetEmpty(); }
					return Tx(Solstice::Loc(FString::Printf(TEXT("element.%s"),
						Solstice::ElementId(Card->GetElement()))));
				}), 12)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNullWidget::NullWidget ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				TxtShadowL(TAttribute<FText>::CreateLambda([this, Slot]()
				{
					FSimKernel* Kernel = Sim();
					const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
					if (CardIndex == INDEX_NONE) { return FText::GetEmpty(); }
					const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
					if (!Card) { return FText::GetEmpty(); }
					// Circled numerals, not the raw tier index: the data is 0-based, so "0" was
					// being printed on every starter card and meant nothing to anyone.
					static const TCHAR* Glyphs[] = {
						TEXT("①"), TEXT("②"), TEXT("③"), TEXT("④"), TEXT("⑤"),
						TEXT("⑥"), TEXT("⑦"), TEXT("⑧"), TEXT("⑨"), TEXT("⑩")
					};
					const int32 Index = FMath::Clamp(Card->Tier, 0, 9);
					return Tx(Glyphs[Index]);
				}), 10)
			]
		];

	// ---- value -----------------------------------------------------------
	TSharedRef<SWidget> Value = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
		[
			TxtL(TAttribute<FText>::CreateLambda([this, Slot]()
			{
				FSimKernel* Kernel = Sim();
				const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
				if (CardIndex == INDEX_NONE) { return FText::GetEmpty(); }
				const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
				if (!Card) { return FText::GetEmpty(); }
				return Tx(FString::FromInt(
					Solstice::Tuning::CardValue[FMath::Clamp(static_cast<int32>(Card->Tier), 0, 11)]));
			}), 24, Ink, true)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
			.Padding(Px(5.0f), 0.0f, 0.0f, Px(3.0f))
		[
			Txt(Tx(Solstice::Loc(TEXT("echo.balance"))), 10, InkSoft)
		];

	TSharedRef<SWidget> Name = SNew(SBox).Clipping(EWidgetClipping::ClipToBounds)
		[
			TxtL(TAttribute<FText>::CreateLambda([this, Slot]()
			{
				FSimKernel* Kernel = Sim();
				const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
				if (CardIndex == INDEX_NONE) { return FText::GetEmpty(); }
				const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
				return Card ? FText::FromString(Solstice::Loc2(Card->NameZh, Card->NameEn)) : FText::GetEmpty();
			}), 15, Ink, true)
		];

	TSharedRef<SWidget> Tags = SNew(SBox).Clipping(EWidgetClipping::ClipToBounds)
		[
			TxtL(TAttribute<FText>::CreateLambda([this, Slot]()
			{
				FSimKernel* Kernel = Sim();
				const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
				if (CardIndex == INDEX_NONE) { return FText::GetEmpty(); }
				const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
				if (!Card) { return FText::GetEmpty(); }
				return Tx(FString::Printf(TEXT("%s %s"),
					*Solstice::Loc(FString::Printf(TEXT("class.%s"),
						Solstice::CardClassId(Card->GetCardClass()))),
					Solstice::DurationGlyph(Card->GetDuration())));
			}), 10, InkSoft)
		];

	return SNew(SBox)
		.WidthOverride(Px(150.0f))
		.HeightOverride(Px(142.0f))
		.Padding(FMargin(Px(3.0f)))
		[
			SNew(SBorder)
			.BorderImage(SolidBrush())
			.BorderBackgroundColor(FrameColour)
			.Padding(FMargin(Px(2.0f)))
			[
				SNew(SButton)
				.ButtonStyle(&CardStyle())
				.ContentPadding(FMargin(0.0f))
				.OnClicked(FOnClicked::CreateLambda([this, Slot]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->SelectSlot(Slot); }
					return FReply::Handled();
				}))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
						.Padding(Px(7.0f), Px(7.0f), Px(7.0f), 0.0f)[ Strip ]
					+ SVerticalBox::Slot().AutoHeight().Padding(Px(9.0f), Px(8.0f), Px(9.0f), 0.0f)[ Value ]
					+ SVerticalBox::Slot().AutoHeight().Padding(Px(9.0f), Px(1.0f), Px(9.0f), 0.0f)[ Name ]
					+ SVerticalBox::Slot().FillHeight(1.0f)[ SNullWidget::NullWidget ]
					+ SVerticalBox::Slot().AutoHeight().Padding(Px(9.0f), 0.0f, Px(9.0f), Px(9.0f))[ Tags ]
				]
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildHandBar()
{
	TSharedRef<SHorizontalBox> HandRow = SNew(SHorizontalBox);
	for (int32 Slot = 0; Slot < Solstice::Tuning::HandSize; ++Slot)
	{
		HandRow->AddSlot().AutoWidth()[ CardChip(Slot) ];
	}

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FSlateColor(PanelBg))
		.Padding(FMargin(Px(9.0f), Px(7.0f)))
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(Px(4.0f), 0.0f, Px(4.0f), Px(5.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Txt(Tx(Solstice::Loc(TEXT("hand.title"))), 13, Accent, true)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					.Padding(Px(10.0f), 0.0f, 0.0f, 0.0f)
				[
					TxtL(TAttribute<FText>::CreateLambda([this]()
					{
						ASolsticePlayerController* Controller = PC();
						FSimKernel* Kernel = Sim();
						if (!Controller || !Kernel) { return FText::GetEmpty(); }
						const FString Mode = Controller->IsSettleAsEntity()
							? Solstice::Loc(TEXT("hand.settle"))
							: Solstice::Loc(TEXT("hand.release"));
						return Tx(FString::Printf(TEXT("第 %d 张 · %s · %s"),
							Controller->GetSelectedSlot() + 1,
							*Mode,
							*Solstice::Loc(TEXT("hand.undo_window"))));
					}), 11, InkSoft)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNullWidget::NullWidget ]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				HandRow
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildActionBar()
{
	// Three groups, and the groups are the point: cards on the left, the clock in the middle,
	// language and help on the right. Before this it was eight identical grey buttons in a row -
	// the same weight for "release a card" and "switch language", which is a settings dialog,
	// not a game.
	auto Group = [](const TArray<TSharedRef<SWidget>>& Buttons) -> TSharedRef<SWidget>
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (const TSharedRef<SWidget>& Button : Buttons)
		{
			Row->AddSlot().AutoWidth().Padding(Px(1.5f))[ Button ];
		}
		return Row;
	};

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FSlateColor(PanelBg))
		.Padding(FMargin(Px(7.0f), Px(4.0f)))
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Group({
					ActionButton(Tx(Solstice::Loc(TEXT("hand.release"))), FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* Controller = PC()) { Controller->ReleaseSelectedAtCursor(); }
						return FReply::Handled();
					})),
					ActionButton(Tx(Solstice::Loc(TEXT("hand.recycle"))), FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* Controller = PC()) { Controller->RecycleSelected(); }
						return FReply::Handled();
					})),
					ActionButton(Tx(Solstice::Loc(TEXT("hand.reroll"))), FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* Controller = PC()) { Controller->RerollHand(); }
						return FReply::Handled();
					})),
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Px(7.0f), 0.0f)[ GroupDivider() ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Group({
					ActionButton(Tx(Solstice::Loc(TEXT("time.virtual"))), FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* Controller = PC()) { Controller->ToggleTimeMode(); }
						return FReply::Handled();
					})),
					ActionButton(Tx(Solstice::Loc(TEXT("ui.pause"))), FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* Controller = PC()) { Controller->TogglePause(); }
						return FReply::Handled();
					})),
					ActionButton(Tx(Solstice::Loc(TEXT("time.speed"))), FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* Controller = PC()) { Controller->CycleSpeed(); }
						return FReply::Handled();
					})),
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Px(7.0f), 0.0f)[ GroupDivider() ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Group({
					ActionButton(Tx(Solstice::Loc(TEXT("ui.language"))), FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* Controller = PC()) { Controller->ToggleLanguage(); }
						return FReply::Handled();
					})),
					ActionButton(Tx(Solstice::Loc(TEXT("help.title"))), FOnClicked::CreateLambda([this]()
					{
						if (ASolsticePlayerController* Controller = PC()) { Controller->ToggleHelp(); }
						return FReply::Handled();
					})),
				})
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildPackPanel()
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	Body->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SmallButton(Tx(FString::Printf(TEXT("%s %d"), *Solstice::Loc(TEXT("pack.random")), Tuning::PackPriceRandom)),
				FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->OpenPack(EPackKind::RandomElement); }
					return FReply::Handled();
				}))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SmallButton(Tx(FString::Printf(TEXT("%s %d"), *Solstice::Loc(TEXT("pack.special")), Tuning::PackPriceSpecial)),
				FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->OpenPack(EPackKind::Special); }
					return FReply::Handled();
				}))
		]
	];

	// Three element packs, priced at the chosen-element tariff.
	Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SmallButton(Tx(FString::Printf(TEXT("%s %d"), Solstice::ElementGlyph(EElement::Earth), Tuning::PackPriceSpecific)),
				FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->OpenPackForElement(EElement::Earth); }
					return FReply::Handled();
				}))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SmallButton(Tx(FString::Printf(TEXT("%s %d"), Solstice::ElementGlyph(EElement::Fire), Tuning::PackPriceSpecific)),
				FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->OpenPackForElement(EElement::Fire); }
					return FReply::Handled();
				}))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SmallButton(Tx(FString::Printf(TEXT("%s %d"), Solstice::ElementGlyph(EElement::Metal), Tuning::PackPriceSpecific)),
				FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->OpenPackForElement(EElement::Metal); }
					return FReply::Handled();
				}))
		]
	];

	Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
	[
		TxtL(TAttribute<FText>::CreateLambda([this]()
		{
			ASolsticePlayerController* Controller = PC();
			if (!Controller) { return FText::GetEmpty(); }
			const FPackResult& Result = Controller->GetLastPack();
			if (Result.Cards.Num() == 0) { return FText::GetEmpty(); }

			FSimKernel* Kernel = Sim();
			FString Built = FString::Printf(TEXT("%s: "), *Solstice::Loc(TEXT("pack.result")));
			for (int32 Index = 0; Index < Result.Cards.Num(); ++Index)
			{
				const FCardDef* Card = Kernel ? Kernel->GetCatalog().CardAt(Result.Cards[Index]) : nullptr;
				if (!Card) { continue; }
				Built += FString::Printf(TEXT("%s%s "), Index > 0 ? TEXT("· ") : TEXT(""),
					*Solstice::Loc2(Card->NameZh, Card->NameEn));
			}
			if (Result.ConvertedEcho > 0)
			{
				Built += FString::Printf(TEXT("\n%s (+%d)"), *Solstice::Loc(TEXT("pack.converted")), Result.ConvertedEcho);
			}
			return Tx(Built);
		}), 11, Ink)
	];

	Body->AddSlot().AutoHeight()
	[
		TxtL(TAttribute<FText>::CreateLambda([this]()
		{
			FSimKernel* Kernel = Sim();
			if (!Kernel || Kernel->AreSpecialPacksAvailable()) { return FText::GetEmpty(); }
			return Tx(Solstice::Loc(TEXT("pack.special_locked")));
		}), 10, InkSoft)
	];

	return SNew(SBox)
		.WidthOverride(244.0f)
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FSlateColor(PanelBg))
			.Padding(FMargin(11.0f, 8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Txt(Tx(Solstice::Loc(TEXT("pack.title"))), 13, Accent, true)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNullWidget::NullWidget ]
				]
				+ SVerticalBox::Slot().AutoHeight()[ Body ]
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildChroniclePanel()
{
	TSharedRef<SBox> Body = SNew(SBox).HeightOverride(190.0f)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			TxtL(TAttribute<FText>::CreateLambda([this]()
			{
				FSimKernel* Kernel = Sim();
				if (!Kernel) { return FText::GetEmpty(); }
				const TArray<FChronicleEntry>& Entries = Kernel->GetChronicle();
				if (Entries.Num() == 0) { return Tx(Solstice::Loc(TEXT("chronicle.empty"))); }

				FString Built;
				const int32 Shown = FMath::Min(Entries.Num(), 60);
				for (int32 Index = 0; Index < Shown; ++Index)
				{
					const FChronicleEntry& Entry = Entries[Entries.Num() - 1 - Index];
					Built += FString::Printf(TEXT("%d 日 %02d:%02d  %s\n"),
						Entry.WorldDay, Entry.MinuteOfDay / 60, Entry.MinuteOfDay % 60,
						*Solstice::Loc2(Entry.SummaryZh, Entry.SummaryEn));
				}
				return Tx(Built);
			}), 10, Ink)
		]
	];

	return SNew(SBox)
		.WidthOverride(300.0f)
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FSlateColor(PanelBg))
			.Padding(FMargin(11.0f, 8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					Txt(Tx(Solstice::Loc(TEXT("chronicle.title"))), 13, Accent, true)
				]
				+ SVerticalBox::Slot().AutoHeight()[ Body ]
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildPoolPanel()
{
	TSharedRef<SBox> Body = SNew(SBox).HeightOverride(150.0f)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			TxtL(TAttribute<FText>::CreateLambda([this]()
			{
				FSimKernel* Kernel = Sim();
				if (!Kernel) { return FText::GetEmpty(); }
				const FCatalog& Catalog = Kernel->GetCatalog();
				FString Built = FString::Printf(TEXT("%s %d / %d\n"),
					*Solstice::Loc(TEXT("pool.unlocked")), Kernel->GetUnlockedCardCount(), Catalog.Cards.Num());
				for (int32 Index = 0; Index < Catalog.Cards.Num(); ++Index)
				{
					if (!Kernel->IsCardUnlocked(Index))
					{
						continue;
					}
					const FCardDef& Card = Catalog.Cards[Index];
					Built += FString::Printf(TEXT("%s %s  %s%s   %s %d\n"),
						Solstice::ElementGlyph(Card.GetElement()),
						*Solstice::TierLabel(Card.Tier),
						*Solstice::Loc2(Card.NameZh, Card.NameEn),
						Card.bProvisional ? TEXT("*") : TEXT(""),
						*Solstice::Loc(TEXT("pool.copies")),
						Kernel->GetCopies(Index));
				}
				return Tx(Built);
			}), 10, Ink)
		]
	];

	return SNew(SBox)
		.WidthOverride(300.0f)
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FSlateColor(PanelBg))
			.Padding(FMargin(11.0f, 8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					Txt(Tx(Solstice::Loc(TEXT("pool.title"))), 13, Accent, true)
				]
				+ SVerticalBox::Slot().AutoHeight()[ Body ]
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildCreatorPanel()
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	Body->AddSlot().AutoHeight()
	[
		Txt(Tx(Solstice::Loc(TEXT("creator.stub"))), 11, InkSoft)
	];

	// Creator step 8: sow the life seed. Chaos has no life at all (WORLD SS3.3 step 5), so this
	// is the one switch that turns an empty paper world into an ecology.
	Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
	[
		SmallButton(Tx(Solstice::Loc(TEXT("creator.life_seed"))), FOnClicked::CreateLambda([this]()
		{
			if (ASolsticePlayerController* Controller = PC()) { Controller->SowLifeSeed(); }
			return FReply::Handled();
		}))
	];

	// Creator step 2: reshape. Every preset from world.json is offered, so the six terrain
	// algorithms are reachable without leaving the game.
	Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
	[
		Txt(Tx(Solstice::Loc(TEXT("creator.reshape"))), 11, InkSoft)
	];

	Body->AddSlot().AutoHeight()
	[
		TxtL(TAttribute<FText>::CreateLambda([this]()
		{
			USolsticeGameInstance* Instance = GI();
			if (!Instance) { return FText::GetEmpty(); }
			FString Built;
			const TArray<FWorldConfig>& Presets = Instance->Content().Catalog.Presets;
			for (int32 Index = 0; Index < Presets.Num(); ++Index)
			{
				Built += FString::Printf(TEXT("[%d] %s   "), Index + 1,
					*Solstice::Loc2(Presets[Index].RealmNameZh, Presets[Index].RealmNameEn));
				if ((Index + 1) % 2 == 0) { Built += TEXT("\n"); }
			}
			return Tx(Built);
		}), 11, Ink)
	];

	Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
	[
		TxtL(TAttribute<FText>::CreateLambda([this]()
		{
			USolsticeGameInstance* Instance = GI();
			if (!Instance) { return FText::GetEmpty(); }
			return Tx(FString::Printf(TEXT("%s: %s"),
				*Solstice::Loc(TEXT("game.seed")),
				*FString::FromInt(static_cast<int32>(Instance->Kernel().GetConfig().Seed))));
		}), 10, InkSoft)
	];

	return SNew(SBox)
		.WidthOverride(244.0f)
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FSlateColor(PanelBg))
			.Padding(FMargin(11.0f, 8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					Txt(Tx(Solstice::Loc(TEXT("creator.title"))), 13, Accent, true)
				]
				+ SVerticalBox::Slot().AutoHeight()[ Body ]
			]
		];
}

TSharedRef<SWidget> SSolsticeHUDView::BuildHelpPanel()
{
	// Grouped, because it is now the *only* place the bindings live.
	//
	// The action bar used to print a key cap inside every button - 空格 / X / R / T / K / J /
	// O / H, eight key legends parked across the bottom of the picture. That is what a settings
	// page is for; a game screen should say "释放" and let the player learn the binding if they
	// want it. The binds moved here, and here they are ordered by what the player is doing
	// rather than by which key happens to be free.
	auto Line = [](const FString& Key) -> TSharedRef<SWidget>
	{
		return Txt(Tx(Solstice::Loc(Key)), 12, InkSoft);
	};

	auto Section = [this](const FString& Zh, const FString& En) -> TSharedRef<SWidget>
	{
		return SNew(SBox).Padding(0.0f, Px(9.0f), 0.0f, Px(2.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(Solstice::Loc2(Zh, En)))
			.Font(Font(11, true))
			.ColorAndOpacity(FSlateColor(Accent))
		];
	};

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight()[ Section(TEXT("手牌"), TEXT("CARDS")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.lmb")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.rmb")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.slots")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.x")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.r")) ];
	Body->AddSlot().AutoHeight()[ Section(TEXT("视角"), TEXT("CAMERA")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.orbit")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.wheel")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.azimuth")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.f1")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.f2")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.f3")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.f4")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.y")) ];
	Body->AddSlot().AutoHeight()[ Section(TEXT("时间与界面"), TEXT("TIME AND HUD")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.t")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.k")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.j")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.c")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.b")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.l")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.g")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.h")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.p")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.tab")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.esc")) ];

	return Panel(Tx(Solstice::Loc(TEXT("help.title"))), Body, 320.0f);
}

