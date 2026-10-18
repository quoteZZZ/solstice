// Solstice - UI layer: the Slate HUD.

#include "UI/SolsticeHUDView.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Core/SolsticeGameInstance.h"
#include "Core/SolsticeLocalization.h"
#include "God/SolsticePlayerController.h"
#include "Sim/SolsticeKernel.h"
#include "Styling/CoreStyle.h"
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

	// R-04: the type ladder lives in exactly one place. Call sites ask for a semantic size
	// (9 / 10 / 11 / 12 / 13 / 22) and the scale is applied here, so the whole sheet can be
	// retuned without editing thirty literals. 1.18 lifts the smallest text (9) to ~11px, the
	// floor that stays readable on a 1080p window without the user zooming in.
	constexpr float UiFontScale = 1.18f;

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		const int32 Scaled = FMath::Max(8, FMath::RoundToInt(static_cast<float>(Size) * UiFontScale));
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

	/** Key on the left in soft ink, live value on the right in full ink. */
	TSharedRef<SWidget> Row(const FString& Key, TAttribute<FText> Value)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 2.0f)
			[
				Txt(Tx(Loc(Key)), 12, InkSoft)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 2.0f)
			[
				// R-09: a zero is not a reading. Dim it so the eye skips it and lands on whatever
				// actually moved - the panel used to give seven rows of "0" the same weight as a
				// live number, which is why it read as empty at minute three.
				SNew(STextBlock)
				.Text(Value)
				.Font(Font(12, true))
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
			.WidthOverride(Width)
			[
				SNew(SBorder)
				.BorderImage(&ShadowBrush)
				.Padding(FMargin(1.0f, 1.0f, 1.0f, 2.0f))
				[
				SNew(SBorder)
				.BorderImage(PanelBrush())
				.Padding(FMargin(13.0f, 10.0f))
				.Visibility(EVisibility::SelfHitTestInvisible)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)
					[
						Txt(Title, 14, Accent, true)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Body
					]
				]
				]
			];
	}

	TSharedRef<SWidget> ActionButton(const FText& Label, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.ButtonStyle(&ActionStyle())
			.OnClicked(OnClicked)
			.ContentPadding(FMargin(2.0f))
			[
				Txt(Label, 12, Ink, true)
			];
	}

	TSharedRef<SWidget> SmallButton(const FText& Label, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.ButtonStyle(&ChipStyle())
			.OnClicked(OnClicked)
			.ContentPadding(FMargin(2.0f))
			[
				Txt(Label, 11, Ink, false)
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
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(14.0f))
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

		// ---- top right: echo -------------------------------------------------
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(14.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildEchoPanel()
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
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(14.0f, 18.0f))
		[
			BuildStatusLine()
		]

		// ---- bottom centre: hand + actions ----------------------------------
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(14.0f))
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
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(14.0f))
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
	];
}

// ======================================================================
// Panels
// ======================================================================

TSharedRef<SWidget> SSolsticeHUDView::BuildStatusCapsule()
{
	// Three facts and nothing else: what season it is, what time it is, and what the sky is
	// doing. Everything the old two panels carried is still one key away (T) - it is just not
	// standing between the player and the island any more.
	TSharedRef<SHorizontalBox> Row3 = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			TxtL(TAttribute<FText>::CreateLambda([this]()
			{
				FSimKernel* Kernel = Sim();
				if (!Kernel) { return FText(); }
				return FText::FromString(Kernel->GetSeasonString());
			}), 12, InkSoft)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			TxtL(TAttribute<FText>::CreateLambda([this]()
			{
				FSimKernel* Kernel = Sim();
				if (!Kernel) { return FText(); }
				return FText::FromString(FString::Printf(TEXT("%02d:%02d"),
					Kernel->GetWorldHour(), Kernel->GetWorldMinuteOfHour()));
			}), 14, Ink, true)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
		[
			TxtL(TAttribute<FText>::CreateLambda([this]()
			{
				FSimKernel* Kernel = Sim();
				if (!Kernel) { return FText(); }
				const FString Weather = WeatherLabel(Kernel->GetSky()).ToString();
				return FText::FromString(FString::Printf(TEXT("%s  %.0f°"),
					*Weather, Kernel->GetSky().AirTempC));
			}), 12, InkSoft)
		];

	return SNew(SBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SBorder)
			.BorderImage(PanelBrush())
			.Padding(FMargin(14.0f, 7.0f))
			[
				Row3
			]
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

	return Panel(Tx(Solstice::Loc(TEXT("time.mode"))), Body, 244.0f);
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
	return Panel(Tx(Solstice::Loc(TEXT("realm.population"))), Body, 244.0f);
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
		.HeightOverride(7.0f)
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

	return Panel(Tx(Solstice::Loc(TEXT("echo.balance"))), Body, 244.0f);
}

TSharedRef<SWidget> SSolsticeHUDView::BuildStatusLine()
{
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FSlateColor(PanelBgDeep))
		.Padding(FMargin(14.0f, 6.0f))
		.Visibility(EVisibility::HitTestInvisible)
		[
			TxtL(TAttribute<FText>::CreateLambda([this]()
			{
				ASolsticePlayerController* Controller = PC();
				if (!Controller || !Controller->HasStatus())
				{
					return FText::GetEmpty();
				}
				return FText::FromString(Solstice::Loc2(Controller->GetStatusZh(), Controller->GetStatusEn()));
			}), 12, Ink, true)
		];
}

TSharedRef<SWidget> SSolsticeHUDView::CardChip(int32 Slot)
{
	// R-04: the card grew with the type ladder (the old 112x72 was tuned against 9px text). Its
	// three lines keep the same order for now; the hierarchy swap is R-13.
	return SNew(SBox)
		.WidthOverride(128.0f)
		.HeightOverride(82.0f)
		.Padding(FMargin(3.0f))
		[
			SNew(SButton)
			.ButtonStyle(&CardStyle())
			.ContentPadding(FMargin(5.0f))
			.OnClicked(FOnClicked::CreateLambda([this, Slot]()
			{
				if (ASolsticePlayerController* Controller = PC()) { Controller->SelectSlot(Slot); }
				return FReply::Handled();
			}))
			[
				SNew(SHorizontalBox)
				// The element band replaces the old "whole card tinted" treatment: the face stays
				// paper, and colour is spent where it carries information.
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(7.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([this, Slot]()
						{
							ASolsticePlayerController* Controller = PC();
							const bool bSelected = Controller && Controller->GetSelectedSlot() == Slot;
							FSimKernel* Kernel = Sim();
							const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
							if (CardIndex == INDEX_NONE)
							{
								return FSlateColor(FLinearColor(0.82f, 0.81f, 0.78f, bSelected ? 0.75f : 0.40f));
							}
							const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
							FLinearColor Band = Card ? Solstice::ElementColor(Card->GetElement()) : FLinearColor::White;
							if (Card && Card->Tier == 0)
							{
								Band = FMath::Lerp(FLinearColor(0.96f, 0.94f, 0.90f), Band, 0.45f);
							}
							Band.A = bSelected ? 1.0f : 0.80f;
							return FSlateColor(Band);
						}))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(8.0f, 1.0f, 1.0f, 1.0f))
				[
					SNew(SVerticalBox)
					// R-13: 决策用数在最上、最重。回收价值是经济决策要读的那个数，它以前排在第三行、
					// 9px、最淡的颜色；识别用的卡名居中；元素 / 环级 / 卡类 / 持续性这些静态标识
					// 全部压到最下面一行——「决策用数 > 识别用名 > 静态标识」。
					+ SVerticalBox::Slot().AutoHeight()
					[
						TxtL(TAttribute<FText>::CreateLambda([this, Slot]()
						{
							FSimKernel* Kernel = Sim();
							const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
							if (CardIndex == INDEX_NONE) { return FText::GetEmpty(); }
							const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
							if (!Card) { return FText::GetEmpty(); }
							return Tx(FString::Printf(TEXT("%s %d"), *Solstice::Loc(TEXT("hand.recycle")),
								Solstice::Tuning::CardValue[FMath::Clamp(static_cast<int32>(Card->Tier), 0, 11)]));
						}), 12, Ink, true)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						TxtL(TAttribute<FText>::CreateLambda([this, Slot]()
						{
							FSimKernel* Kernel = Sim();
							const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
							if (CardIndex == INDEX_NONE) { return Tx(Solstice::Loc(TEXT("hand.empty"))); }
							const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
							return Card ? FText::FromString(Solstice::Loc2(Card->NameZh, Card->NameEn)) : FText::GetEmpty();
						}), 11, Ink)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						TxtL(TAttribute<FText>::CreateLambda([this, Slot]()
						{
							FSimKernel* Kernel = Sim();
							const int32 CardIndex = Kernel ? Kernel->GetHandCardIndex(Slot) : INDEX_NONE;
							if (CardIndex == INDEX_NONE) { return FText::GetEmpty(); }
							const FCardDef* Card = Kernel->GetCatalog().CardAt(CardIndex);
							if (!Card) { return FText::GetEmpty(); }
							const FString ElementKey = FString::Printf(TEXT("element.%s"),
								Solstice::ElementId(Card->GetElement()));
							return Tx(FString::Printf(TEXT("%s · %s · %s %s"),
								*Solstice::Loc(ElementKey),
								*Solstice::TierLabel(Card->Tier),
								*Solstice::Loc(FString::Printf(TEXT("class.%s"), Solstice::CardClassId(Card->GetCardClass()))),
								Solstice::DurationGlyph(Card->GetDuration())));
						}), 9, InkSoft)
					]
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
		.Padding(FMargin(8.0f, 6.0f))
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Txt(Tx(Solstice::Loc(TEXT("hand.title"))), 12, Accent, true)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.0f, 0.0f, 0.0f, 0.0f)
				[
					TxtL(TAttribute<FText>::CreateLambda([this]()
					{
						ASolsticePlayerController* Controller = PC();
						FSimKernel* Kernel = Sim();
						if (!Controller || !Kernel) { return FText::GetEmpty(); }
						const FString Mode = Controller->IsSettleAsEntity()
							? Solstice::Loc(TEXT("hand.settle"))
							: Solstice::Loc(TEXT("hand.release"));
						return Tx(FString::Printf(TEXT("[%s] %s · %s"),
							*FString::FromInt(Controller->GetSelectedSlot() + 1),
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
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FSlateColor(PanelBgDeep))
		.Padding(FMargin(10.0f, 6.0f))
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ActionButton(Tx(Solstice::Loc(TEXT("hand.release"))), FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->ReleaseSelectedAtCursor(); }
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ActionButton(Tx(Solstice::Loc(TEXT("hand.recycle"))), FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->RecycleSelected(); }
					return FReply::Handled();
				}))
			]
			// A-02: there is no undo button either. "The paper is still wet" is a window in which
			// the release has not settled, not a take-back - the button was a third reading of the
			// same rule, so it goes with the Z binding.
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ActionButton(Tx(Solstice::Loc(TEXT("hand.reroll"))), FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->RerollHand(); }
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 2.0f, 2.0f, 2.0f)
			[
				ActionButton(Tx(Solstice::Loc(TEXT("time.virtual"))), FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->ToggleTimeMode(); }
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				// Semantic labels, not glyphs: "||", ">>" and "中/EN" told a first-time reader
				// nothing (REVIEW R-07). Same three actions, named.
				ActionButton(Tx(Solstice::Loc(TEXT("ui.pause"))), FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->TogglePause(); }
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ActionButton(Tx(Solstice::Loc(TEXT("time.speed"))), FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->CycleSpeed(); }
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 2.0f, 2.0f, 2.0f)
			[
				ActionButton(Tx(Solstice::Loc(TEXT("ui.language"))), FOnClicked::CreateLambda([this]()
				{
					if (ASolsticePlayerController* Controller = PC()) { Controller->ToggleLanguage(); }
					return FReply::Handled();
				}))
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
	auto Line = [](const FString& Key) -> TSharedRef<SWidget>
	{
		return Txt(Tx(Solstice::Loc(Key)), 11, InkSoft);
	};

	// Order and content mirror UI-UX §2.2 line for line (REVIEW R-01). The old sheet listed a
	// binding that no longer exists (`help.z`) and left the camera, time and language keys out.
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.lmb")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.rmb")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.slots")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.x")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.r")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.orbit")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.wheel")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.f1")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.f2")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.f3")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.f4")) ];
	Body->AddSlot().AutoHeight()[ Line(TEXT("help.y")) ];
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

	return Panel(Tx(Solstice::Loc(TEXT("help.title"))), Body, 236.0f);
}
