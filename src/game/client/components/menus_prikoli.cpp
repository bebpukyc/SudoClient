#include "menus.h"

#include <algorithm>
#include <base/color.h>
#include <base/str.h>

#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

namespace
{
constexpr float STEAL_LINE = 20.0f;
constexpr float STEAL_PAD = 10.0f;

const ColorRGBA STEAL_PANEL = ColorRGBA(1.0f, 1.0f, 1.0f, 0.055f);
const ColorRGBA STEAL_TITLE = ColorRGBA(0.78f, 0.84f, 0.95f, 1.0f);

enum ESudoTab
{
	SUDO_TAB_AIMBOT = 0,
	SUDO_TAB_VISUALS,
	SUDO_TAB_OTHER,
	SUDO_TAB_BINDS,
	NUM_SUDO_TABS,
};

void StealPanel(CUi *pUi, ITextRender *pTextRender, CUIRect *pView, const char *pTitle, float Height, CUIRect *pBody)
{
	CUIRect Panel;
	pView->HSplitTop(Height, &Panel, pView);
	pView->HSplitTop(8.0f, nullptr, pView);
	Panel.Draw(STEAL_PANEL, IGraphics::CORNER_ALL, 8.0f);
	Panel.Margin(STEAL_PAD, &Panel);

	CUIRect Head;
	Panel.HSplitTop(16.0f, &Head, &Panel);
	Panel.HSplitTop(6.0f, nullptr, &Panel);
	pTextRender->TextColor(STEAL_TITLE);
	pUi->DoLabel(&Head, pTitle, 13.0f, TEXTALIGN_ML);
	pTextRender->TextColor(pTextRender->DefaultTextColor());

	*pBody = Panel;
}
} // namespace

void CMenus::RenderSettingsStealAimbot(CUIRect MainView)
{
	// weapon chain, each card is a separate aimbot with its own background.
	// the hook (grabbing players) and the pistol (WEAPON_GUN, shooting) are
	// separate things in DDNet, so they each have their own config.
	struct SWeaponCard
	{
		const char *pName;
		const char *pHint;
		int *pEnabled;
		int *pFov;
		int *pSilent;
	};
	const SWeaponCard aWeapons[] = {
		{"Hook", "Hook", &g_Config.m_AaWeaponHook, &g_Config.m_AaFovHook, &g_Config.m_AaSilentHook},
		{"Hammer", "Hammer", &g_Config.m_AaWeaponHammer, &g_Config.m_AaFovHammer, &g_Config.m_AaSilentHammer},
		{"Pistol", "Pistol", &g_Config.m_AaWeaponPistol, &g_Config.m_AaFovPistol, &g_Config.m_AaSilentPistol},
		{"Shotgun", "Shotgun", &g_Config.m_AaWeaponShotgun, &g_Config.m_AaFovShotgun, &g_Config.m_AaSilentShotgun},
		{"Grenade", "Grenade", &g_Config.m_AaWeaponGrenade, &g_Config.m_AaFovGrenade, &g_Config.m_AaSilentGrenade},
		{"Laser", "Laser", &g_Config.m_AaWeaponLaser, &g_Config.m_AaFovLaser, &g_Config.m_AaSilentLaser},
	};
	const int NumWeapons = (int)(sizeof(aWeapons) / sizeof(aWeapons[0]));

	CUIRect LeftView, RightView, Body, Row;
	MainView.VSplitMid(&LeftView, &RightView, 16.0f);

	// scrollable weapon list: the cards grow when a weapon is enabled, so the
	// whole column is placed in a scroll region. The scrollbar follows the
	// expanding content automatically.
	static CScrollRegion s_WeaponScroll;
	vec2 ScrollOffset(0, 0);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 30.0f;
	ScrollParams.m_ScrollbarWidth = 12.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ClipBgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);

	CUIRect ScrollRect = LeftView;
	ScrollRect.HSplitTop(10.0f, nullptr, &ScrollRect);
	s_WeaponScroll.Begin(&ScrollRect, &ScrollOffset, &ScrollParams);

	CUIRect Content = ScrollRect;
	Content.y += ScrollOffset.y;

	// per-weapon cards: every aimbot is off by default. Each card shows only
	// its Enable checkbox; enabling it reveals the FOV slider and its Silent
	// option. The panel height depends on the enabled state.
	for(int i = 0; i < NumWeapons; ++i)
	{
		const bool Enabled = *aWeapons[i].pEnabled != 0;
		const float CardH = Enabled ? 150.0f : 60.0f;

		CUIRect Card;
		Content.HSplitTop(CardH, &Card, &Content);
		Content.HSplitTop(8.0f, nullptr, &Content);
		if(!s_WeaponScroll.AddRect(Card))
			continue;

		StealPanel(Ui(), TextRender(), &Card, aWeapons[i].pHint, CardH, &Body);

		DoButton_CheckBoxAutoVMarginAndSet(aWeapons[i].pEnabled, "Enable", aWeapons[i].pEnabled, &Body, STEAL_LINE);

		if(Enabled)
		{
			Body.HSplitTop(6.0f, nullptr, &Body);
			Body.HSplitTop(STEAL_LINE, &Row, &Body);
			Ui()->DoScrollbarOption(aWeapons[i].pFov, aWeapons[i].pFov, &Row, "FOV", 0, 360);
			Body.HSplitTop(6.0f, nullptr, &Body);
			DoButton_CheckBoxAutoVMarginAndSet(aWeapons[i].pSilent, "Silent", aWeapons[i].pSilent, &Body, STEAL_LINE);
		}
	}

	s_WeaponScroll.End();

	CUIRect RightPanel = RightView;
	RightPanel.HSplitTop(10.0f, nullptr, &RightPanel);
	StealPanel(Ui(), TextRender(), &RightPanel, "General", 280.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaEnabled, "Aimbot", &g_Config.m_AaEnabled, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaDrawFov, "Show FOV", &g_Config.m_AaDrawFov, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaPredict, "Prediction", &g_Config.m_AaPredict, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaHookVisible, "Visible targets only", &g_Config.m_AaHookVisible, &Body, STEAL_LINE);
	Body.HSplitTop(10.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaPreferBlock, "Prefer block", &g_Config.m_AaPreferBlock, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaClosest, "Closest target", &g_Config.m_AaClosest, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaEdge, "Hook edges", &g_Config.m_AaEdge, &Body, STEAL_LINE);
}

void CMenus::RenderSettingsStealOther(CUIRect MainView)
{
	CUIRect LeftView, RightView, Body, Row;
	MainView.VSplitMid(&LeftView, &RightView, 16.0f);

	StealPanel(Ui(), TextRender(), &LeftView, "Other", g_Config.m_NwAvoidEnabled ? 336.0f : 230.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaAutoAled, "AutoAled", &g_Config.m_AaAutoAled, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaAutoHammer, "AutoHammer", &g_Config.m_AaAutoHammer, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaHookSpam, "HookSpam", &g_Config.m_AaHookSpam, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaFastFire, "Fast Fire", &g_Config.m_AaFastFire, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaSpamEmote, "Spam Emotes", &g_Config.m_AaSpamEmote, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaMoonwalk, "Moonwalk", &g_Config.m_AaMoonwalk, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_NwAvoidEnabled, "Avoid", &g_Config.m_NwAvoidEnabled, &Body, STEAL_LINE);
	if(g_Config.m_NwAvoidEnabled)
	{
		Body.HSplitTop(6.0f, nullptr, &Body);
		Body.HSplitTop(STEAL_LINE, &Row, &Body);
		Ui()->DoScrollbarOption(&g_Config.m_NwAvoidType, &g_Config.m_NwAvoidType, &Row, "Mode (0 legit, 1 blatant, 2 horiz)", 0, 2);
		Body.HSplitTop(6.0f, nullptr, &Body);
		Body.HSplitTop(STEAL_LINE, &Row, &Body);
		Ui()->DoScrollbarOption(&g_Config.m_NwAvoidLegitPredictTicks, &g_Config.m_NwAvoidLegitPredictTicks, &Row, "Predict Ticks", 1, 48);
		Body.HSplitTop(6.0f, nullptr, &Body);
		Body.HSplitTop(STEAL_LINE, &Row, &Body);
		Ui()->DoScrollbarOption(&g_Config.m_NwAvoidBlatantAutoJump, &g_Config.m_NwAvoidBlatantAutoJump, &Row, "Auto-jump blatant", 0, 1);
		Body.HSplitTop(6.0f, nullptr, &Body);
		Body.HSplitTop(STEAL_LINE, &Row, &Body);
		Ui()->DoScrollbarOption(&g_Config.m_NwAvoidDebug, &g_Config.m_NwAvoidDebug, &Row, "Debug trace", 0, 1);
	}

	StealPanel(Ui(), TextRender(), &RightView, "Laser Unfreeze", 90.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_SymLaserEnable, "Auto laser unfreeze", &g_Config.m_SymLaserEnable, &Body, STEAL_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_SymLaserSilent, "Silent", &g_Config.m_SymLaserSilent, &Body, STEAL_LINE);

	// ------------------------------------------------ Fake Aim
	// Collapsible like ESP/Trajectory: only the enable box when off.
	{
		const bool FakeOn = g_Config.m_AaFakeAim != 0;
		StealPanel(Ui(), TextRender(), &RightView, "Fake Aim", FakeOn ? 172.0f : 60.0f, &Body);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaFakeAim, "Fake Aim", &g_Config.m_AaFakeAim, &Body, STEAL_LINE);
		if(FakeOn)
		{
			Body.HSplitTop(6.0f, nullptr, &Body);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_AaFakeAimShowForMe, "Visible", &g_Config.m_AaFakeAimShowForMe, &Body, STEAL_LINE);
			Body.HSplitTop(6.0f, nullptr, &Body);
			Body.HSplitTop(STEAL_LINE, &Row, &Body);
			static CButtonContainer s_FakeModeRandom, s_FakeModeRobot, s_FakeModeSpin, s_FakeModeLag;
			CUIRect MRow = Row, B0, B1, B2, B3;
			MRow.VSplitLeft(MRow.w / 4.0f, &B0, &MRow);
			MRow.VSplitLeft(MRow.w / 3.0f, &B1, &MRow);
			MRow.VSplitLeft(MRow.w / 2.0f, &B2, &B3);
			const int FakeMode = g_Config.m_AaFakeAimMode;
			if(DoButton_Menu(&s_FakeModeRandom, "Random", FakeMode == 0, &B0, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
				g_Config.m_AaFakeAimMode = 0;
			if(DoButton_Menu(&s_FakeModeRobot, "Robot", FakeMode == 1, &B1, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
				g_Config.m_AaFakeAimMode = 1;
			if(DoButton_Menu(&s_FakeModeSpin, "Spin", FakeMode == 2, &B2, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
				g_Config.m_AaFakeAimMode = 2;
			if(DoButton_Menu(&s_FakeModeLag, "Lag", FakeMode == 3, &B3, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
				g_Config.m_AaFakeAimMode = 3;
			Body.HSplitTop(6.0f, nullptr, &Body);
			Body.HSplitTop(STEAL_LINE, &Row, &Body);
			Ui()->DoScrollbarOption(&g_Config.m_AaFakeAimSpeed, &g_Config.m_AaFakeAimSpeed, &Row, "Speed", 1, 100);
		}
	}
}

void CMenus::RenderSettingsStealVisuals(CUIRect MainView)
{
	CUIRect LeftView, RightView, Body, Row;
	MainView.VSplitMid(&LeftView, &RightView, 16.0f);

	// ------------------------------------------------ HUD Color
	// Рулит ТОЛЬКО переменными Active Features (bc_functions_color*).
	// Раньше панель писала в bc_music_player_color_mode (у него max=1, свой
	// смысл 0/1 для визуалайзера), а плеер клампил обратно — режимы
	// Rainbow/Music не держались и всё дёргалось.
	{
		g_Config.m_BcFunctionsColorMode = std::clamp(g_Config.m_BcFunctionsColorMode, 0, 3);
		const int HudColorMode = g_Config.m_BcFunctionsColorMode;
		const bool HudColorStatic = HudColorMode == 1;

		StealPanel(Ui(), TextRender(), &LeftView, "HUD Color", HudColorStatic ? 194.0f : 150.0f, &Body);

		Body.HSplitTop(STEAL_LINE, &Row, &Body);
		static CButtonContainer s_HudColorModeStatic;
		static CButtonContainer s_HudColorModeRainbow;
		static CButtonContainer s_HudColorModeMusic;
		CUIRect ModeRow = Row, StaticButton, RainbowButton, MusicButton;
		ModeRow.VSplitLeft(ModeRow.w / 3.0f, &StaticButton, &ModeRow);
		ModeRow.VSplitLeft(ModeRow.w / 2.0f, &RainbowButton, &MusicButton);
		if(DoButton_Menu(&s_HudColorModeStatic, "Static", HudColorMode == 1, &StaticButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
		{
			g_Config.m_BcFunctionsColorMode = 1;
		}
		if(DoButton_Menu(&s_HudColorModeRainbow, "Rainbow", HudColorMode == 2, &RainbowButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
		{
			g_Config.m_BcFunctionsColorMode = 2;
		}
		if(DoButton_Menu(&s_HudColorModeMusic, "Music", HudColorMode == 3, &MusicButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
		{
			g_Config.m_BcFunctionsColorMode = 3;
		}

		if(HudColorStatic)
		{
			Body.HSplitTop(6.0f, nullptr, &Body);
			static CButtonContainer s_HudColorStaticButton;
			DoLine_ColorPicker(&s_HudColorStaticButton, 25.0f, 13.0f, 5.0f, &Body, "Static color", &g_Config.m_BcFunctionsColor, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false, nullptr, true);
		}

		Body.HSplitTop(6.0f, nullptr, &Body);
		Body.HSplitTop(STEAL_LINE, &Row, &Body);
		Ui()->DoScrollbarOption(&g_Config.m_BcMusicPlayerHudColorAlpha, &g_Config.m_BcMusicPlayerHudColorAlpha, &Row, "HUD color transparency", 0, 100);
	}

	// ------------------------------------------------ ESP
	{
		const bool EspOn = g_Config.m_KxEsp != 0;
		StealPanel(Ui(), TextRender(), &RightView, "ESP", EspOn ? 170.0f : 60.0f, &Body);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_KxEsp, "ESP", &g_Config.m_KxEsp, &Body, STEAL_LINE);
		if(EspOn)
		{
			Body.HSplitTop(8.0f, nullptr, &Body);
			Body.HSplitTop(STEAL_LINE, &Row, &Body);
			Ui()->DoScrollbarOption(&g_Config.m_KxLineEspSize, &g_Config.m_KxLineEspSize, &Row, "Line size", 0, 20);
			Body.HSplitTop(8.0f, nullptr, &Body);
			{
				static CButtonContainer s_EspColorPicker;
				const ColorRGBA Def = color_cast<ColorRGBA>(ColorHSLA(4294967295u, true));
				DoLine_ColorPicker(&s_EspColorPicker, 25.0f, 13.0f, 5.0f, &Body, "Color", &g_Config.m_KxLineEspColor, Def, false, nullptr, true);
			}
		}
	}

	// ------------------------------------------------ Trajectory
	{
		const bool TrajOn = g_Config.m_KxShowTrajectory != 0;
		StealPanel(Ui(), TextRender(), &RightView, "Trajectory", TrajOn ? 210.0f : 60.0f, &Body);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_KxShowTrajectory, "Trajectory", &g_Config.m_KxShowTrajectory, &Body, STEAL_LINE);
		if(TrajOn)
		{
			Body.HSplitTop(8.0f, nullptr, &Body);
			Body.HSplitTop(STEAL_LINE, &Row, &Body);
			Ui()->DoScrollbarOption(&g_Config.m_KxTrajectoryTicks, &g_Config.m_KxTrajectoryTicks, &Row, "Prediction ticks", 1, 50);
			Body.HSplitTop(8.0f, nullptr, &Body);
			Body.HSplitTop(STEAL_LINE, &Row, &Body);
			Ui()->DoScrollbarOption(&g_Config.m_KxLineTrajectorySize, &g_Config.m_KxLineTrajectorySize, &Row, "Line size", 0, 20);
			Body.HSplitTop(8.0f, nullptr, &Body);
			{
				static CButtonContainer s_TrajColorPicker;
				const ColorRGBA Def = color_cast<ColorRGBA>(ColorHSLA(4294967295u, true));
				DoLine_ColorPicker(&s_TrajColorPicker, 25.0f, 13.0f, 5.0f, &Body, "Color", &g_Config.m_KxLineTrajectoryColor, Def, false, nullptr, true);
			}
		}
	}
}

void CMenus::RenderSettingsStealBinds(CUIRect MainView)
{
	CUIRect LeftView, RightView, Body;
	MainView.VSplitMid(&LeftView, &RightView, 16.0f);

	static CButtonContainer s_BindAaToggle, s_BindAaToggleClear;
	static CButtonContainer s_BindLaserToggle, s_BindLaserToggleClear;
	static CButtonContainer s_BindAledToggle, s_BindAledToggleClear;
	static CButtonContainer s_BindHammerToggle, s_BindHammerToggleClear;
	static CButtonContainer s_BindHookSpamToggle, s_BindHookSpamToggleClear;
	static CButtonContainer s_BindFastFireToggle, s_BindFastFireToggleClear;
	static CButtonContainer s_BindSpamEmoteToggle, s_BindSpamEmoteToggleClear;
	static CButtonContainer s_BindMoonwalkToggle, s_BindMoonwalkToggleClear;
	static CButtonContainer s_BindAvoidToggle, s_BindAvoidToggleClear;
	static CButtonContainer s_BindAvoidDebugToggle, s_BindAvoidDebugToggleClear;
	static CButtonContainer s_BindEspToggle, s_BindEspToggleClear;

	StealPanel(Ui(), TextRender(), &LeftView, "Aimbot", 350.0f, &Body);
	DoLine_KeyReader(Body, s_BindAaToggle, s_BindAaToggleClear, "Aimbot", "toggle aa_enabled 0 1");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindAvoidToggle, s_BindAvoidToggleClear, "Avoid", "toggle nw_avoid_enabled 1 0");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindEspToggle, s_BindEspToggleClear, "ESP", "toggle kx_esp 1 0");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindAledToggle, s_BindAledToggleClear, "AutoAled", "toggle aa_auto_aled 0 1");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindHammerToggle, s_BindHammerToggleClear, "AutoHammer", "toggle aa_auto_hammer 0 1");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindHookSpamToggle, s_BindHookSpamToggleClear, "HookSpam", "toggle aa_hook_spam 0 1");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindFastFireToggle, s_BindFastFireToggleClear, "FastFire", "toggle aa_fast_fire 0 1");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindSpamEmoteToggle, s_BindSpamEmoteToggleClear, "SpamEmote", "toggle aa_spam_emote 0 1");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindMoonwalkToggle, s_BindMoonwalkToggleClear, "Moonwalk", "toggle aa_moonwalk 0 1");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindLaserToggle, s_BindLaserToggleClear, "Laser unfreeze", "toggle sym_laser_enable 0 1");

	StealPanel(Ui(), TextRender(), &RightView, "Avoid", 100.0f, &Body);
	DoLine_KeyReader(Body, s_BindAvoidToggle, s_BindAvoidToggleClear, "Avoid", "toggle nw_avoid_enabled 1 0");
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoLine_KeyReader(Body, s_BindAvoidDebugToggle, s_BindAvoidDebugToggleClear, "Avoid debug", "toggle nw_avoid_debug 0 1");
}

void CMenus::RenderSettingsSteal(CUIRect MainView)
{
	static int s_CurTab = SUDO_TAB_AIMBOT;
	static CButtonContainer s_aSubTabButtons[NUM_SUDO_TABS];

	const char *apTabNames[NUM_SUDO_TABS] = {
		"Aimbot",
		"Visuals",
		"Other",
		"Binds",
	};

	CUIRect TabBar, TabButton;
	MainView.HSplitTop(8.0f, nullptr, &MainView);
	MainView.HSplitTop(24.0f, &TabBar, &MainView);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	if(s_CurTab < 0 || s_CurTab >= NUM_SUDO_TABS)
		s_CurTab = SUDO_TAB_AIMBOT;

	const float TabWidth = TabBar.w / (float)NUM_SUDO_TABS;
	for(int i = 0; i < NUM_SUDO_TABS; ++i)
	{
		TabBar.VSplitLeft(TabWidth, &TabButton, &TabBar);
		const int Corners = i == 0 ? IGraphics::CORNER_L : (i == NUM_SUDO_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
		if(DoButton_MenuTab(&s_aSubTabButtons[i], apTabNames[i], s_CurTab == i, &TabButton, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
			s_CurTab = i;
	}

	// BISECT2: dispatch disabled
	switch(s_CurTab)
	{
	case SUDO_TAB_AIMBOT:
		RenderSettingsStealAimbot(MainView);
		break;
	case SUDO_TAB_VISUALS:
		RenderSettingsStealVisuals(MainView);
		break;
	case SUDO_TAB_OTHER:
		RenderSettingsStealOther(MainView);
		break;
	case SUDO_TAB_BINDS:
		RenderSettingsStealBinds(MainView);
		break;
	default:
		break;
	}
}
