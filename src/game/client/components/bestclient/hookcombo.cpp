/* Copyright © 2026 BestProject Team */
#include "hookcombo.h"

#include <base/color.h>
#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/sounds.h>
#include <game/client/components/hud_layout.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>

static float ApproachAnim(float Current, float Target, float Delta, float Speed)
{
	const float Step = 1.0f - expf(-maximum(0.0f, Speed) * maximum(0.0f, Delta));
	return mix(Current, Target, std::clamp(Step, 0.0f, 1.0f));
}

static constexpr int s_BaseTextCount = 15;
static constexpr int s_VariantLimit = 100;
static constexpr int s_SoundCount = 7;
static constexpr int s_BrilliantSoundIndex = 5; // 0-based => sound #6
static constexpr int s_ModeHook = 0;
static constexpr int s_ModeHammer = 1;
static constexpr int s_ModeHookAndHammer = 2;

static constexpr const char *const s_apTexts[s_BaseTextCount] = {
	"cool",
	"nice",
	"great",
	"awesome",
	"excellent",
	"amazing",
	"fantastic",
	"incredible",
	"spectacular",
	"legendary",
	"mythic",
	"unstoppable",
	"dominant",
	"masterful",
	"BRILLIANT"};

static const ColorRGBA s_aColors[s_BaseTextCount] = {
	ColorRGBA(0.36f, 1.0f, 0.50f, 1.0f),
	ColorRGBA(0.28f, 0.78f, 1.0f, 1.0f),
	ColorRGBA(0.40f, 1.0f, 0.92f, 1.0f),
	ColorRGBA(1.0f, 0.75f, 0.26f, 1.0f),
	ColorRGBA(1.0f, 0.52f, 0.23f, 1.0f),
	ColorRGBA(1.0f, 0.40f, 0.70f, 1.0f),
	ColorRGBA(0.96f, 0.96f, 0.34f, 1.0f),
	ColorRGBA(0.65f, 0.90f, 1.0f, 1.0f),
	ColorRGBA(0.75f, 1.0f, 0.82f, 1.0f),
	ColorRGBA(1.0f, 0.66f, 0.38f, 1.0f),
	ColorRGBA(0.92f, 0.74f, 1.0f, 1.0f),
	ColorRGBA(1.0f, 0.58f, 0.58f, 1.0f),
	ColorRGBA(1.0f, 0.85f, 0.48f, 1.0f),
	ColorRGBA(0.80f, 0.95f, 0.50f, 1.0f),
	ColorRGBA(1.0f, 0.97f, 0.35f, 1.0f)};

static void FormatText(int Sequence, char *pBuf, int BufSize)
{
	if(Sequence <= s_BaseTextCount)
	{
		str_copy(pBuf, s_apTexts[Sequence - 1], BufSize);
		return;
	}

	if(Sequence <= s_VariantLimit)
	{
		static constexpr const char *const s_apAdvancedTitles[] = {
			"brilliant",
			"godlike",
			"unreal",
			"mythic",
			"supreme",
			"transcendent",
			"unstoppable",
			"devastating",
			"apex",
			"ascendant"};
		static constexpr int s_AdvancedTitleCount = 10;
		const int Step = Sequence - s_BaseTextCount - 1;
		const int GroupSize = 9;
		const int Group = std::min(Step / GroupSize, s_AdvancedTitleCount - 1);
		str_format(pBuf, BufSize, "%s %d", s_apAdvancedTitles[Group], Sequence);
		return;
	}

	str_copy(pBuf, "BRILLIANT", BufSize);
}

void CHookCombo::OnInit()
{
	LoadSounds();
	ResetState();
}

void CHookCombo::OnShutdown()
{
	ResetState();
	UnloadSounds();
}

void CHookCombo::OnReset()
{
	ResetState();
}

void CHookCombo::OnStateChange(int NewState, int OldState)
{
	(void)NewState;
	(void)OldState;
	ResetState();
}

void CHookCombo::LoadSounds(bool LogErrors)
{
	for(int &SoundId : m_aSoundIds)
		SoundId = -1;

	for(int i = 0; i < (int)m_aSoundIds.size(); ++i)
	{
		auto TryLoad = [this, i](const char *pPath, int StorageType) {
			if(m_aSoundIds[i] != -1)
				return;
			if(!Storage()->FileExists(pPath, StorageType))
				return;
			m_aSoundIds[i] = Sound()->LoadWV(pPath, StorageType);
		};

		char aPathWv[96];
		char aDataPathWv[128];
		char aParentRelativeDataPathWv[144];
		char aBinaryDataPathWv[IO_MAX_PATH_LENGTH];
		char aParentDataPathWv[IO_MAX_PATH_LENGTH];
		str_format(aPathWv, sizeof(aPathWv), "BestClient/combo/combo%d.wv", i + 1);
		str_format(aDataPathWv, sizeof(aDataPathWv), "data/BestClient/combo/combo%d.wv", i + 1);
		str_format(aParentRelativeDataPathWv, sizeof(aParentRelativeDataPathWv), "../%s", aDataPathWv);
		Storage()->GetBinaryPathAbsolute(aDataPathWv, aBinaryDataPathWv, sizeof(aBinaryDataPathWv));
		Storage()->GetBinaryPathAbsolute(aParentRelativeDataPathWv, aParentDataPathWv, sizeof(aParentDataPathWv));

		TryLoad(aPathWv, IStorage::TYPE_ALL);
		TryLoad(aDataPathWv, IStorage::TYPE_ALL);
		TryLoad(aBinaryDataPathWv, IStorage::TYPE_ABSOLUTE);
		TryLoad(aParentDataPathWv, IStorage::TYPE_ABSOLUTE);

		if(LogErrors && m_aSoundIds[i] == -1)
			log_warn("hook_combo", "Failed to load combo sound #%d (expected data/BestClient/combo/combo%d.wv)", i + 1, i + 1);
	}
}

void CHookCombo::UnloadSounds()
{
	for(int &SoundId : m_aSoundIds)
	{
		if(SoundId != -1)
		{
			Sound()->UnloadSample(SoundId);
			SoundId = -1;
		}
	}
}

void CHookCombo::ResetState()
{
	m_Counter = 0;
	m_LastHookTime = -1.0f;
	m_TrackedClientId = -1;
	m_LastHookedPlayer = -1;
	m_LastProcessedGameTick = -1;
	m_SoundErrorShown = false;
	m_vPopups.clear();
	m_vTogglePopups.clear();
	m_LastAaEnabled = -1;
	m_LastSymLaserEnable = -1;
	m_LastAaAutoAled = -1;
	int m_LastNwAvoidEnabled = -1;
	m_LastAaAutoHammer = -1;
	m_LastAaFastFire = -1;
	m_LastAaHookSpam = -1;
	m_LastAaSpamEmote = -1;
	m_LastAaMoonwalk = -1;
	m_ToggleCacheInit = false;

	// entering a server re-applies the saved config (tc_execute_on_connect,
	// the per-server config, ...), which would otherwise make every "X: OFF"
	// popup fire on join. Suppress toggle detection for a short grace period.
	m_ToggleSuppressFrames = 30;
	std::fill(m_aFunctionAges.begin(), m_aFunctionAges.end(), 0.0f);
}

void CHookCombo::DetectConfigToggles()
{
	if(g_Config.m_BcMenuHider)
		return;

	if(!m_ToggleCacheInit)
	{
		m_LastAaEnabled = g_Config.m_AaEnabled;
		m_LastSymLaserEnable = g_Config.m_SymLaserEnable;
		m_LastAaAutoAled = g_Config.m_AaAutoAled;
		m_LastNwAvoidEnabled = g_Config.m_NwAvoidEnabled;
		m_LastAaAutoHammer = g_Config.m_AaAutoHammer;
		m_LastAaFastFire = g_Config.m_AaFastFire;
		m_LastAaHookSpam = g_Config.m_AaHookSpam;
		m_LastAaSpamEmote = g_Config.m_AaSpamEmote;
		m_LastAaMoonwalk = g_Config.m_AaMoonwalk;
		m_ToggleCacheInit = true;
		return;
	}

	// right after connecting, configs get re-applied from the saved file; keep
	// re-baselining silently instead of bubbling up false "OFF" popups
	if(m_ToggleSuppressFrames > 0)
	{
		m_ToggleSuppressFrames--;
		m_LastAaEnabled = g_Config.m_AaEnabled;
		m_LastSymLaserEnable = g_Config.m_SymLaserEnable;
		m_LastAaAutoAled = g_Config.m_AaAutoAled;
		m_LastNwAvoidEnabled = g_Config.m_NwAvoidEnabled;
		m_LastAaAutoHammer = g_Config.m_AaAutoHammer;
		m_LastAaFastFire = g_Config.m_AaFastFire;
		m_LastAaHookSpam = g_Config.m_AaHookSpam;
		m_LastAaSpamEmote = g_Config.m_AaSpamEmote;
		m_LastAaMoonwalk = g_Config.m_AaMoonwalk;
		return;
	}

	auto CheckToggle = [this](int Current, int &Prev, const char *pName) {
		if(Current == Prev)
			return;
		Prev = Current;

		STogglePopup Popup;
		Popup.m_Age = 0.0f;
		str_format(Popup.m_aText, sizeof(Popup.m_aText), "%s: %s", pName, Current ? "ON" : "OFF");
		m_vTogglePopups.push_back(Popup);
		if(m_vTogglePopups.size() > 10)
			m_vTogglePopups.erase(m_vTogglePopups.begin());
	};

	CheckToggle(g_Config.m_AaEnabled, m_LastAaEnabled, "Aimbot");
	CheckToggle(g_Config.m_SymLaserEnable, m_LastSymLaserEnable, "Laser unfreeze");
	CheckToggle(g_Config.m_AaAutoAled, m_LastAaAutoAled, "AutoAled");
	CheckToggle(g_Config.m_NwAvoidEnabled, m_LastNwAvoidEnabled, "Avoid");
	CheckToggle(g_Config.m_AaAutoHammer, m_LastAaAutoHammer, "AutoHammer");
	CheckToggle(g_Config.m_AaFastFire, m_LastAaFastFire, "FastFire");
	CheckToggle(g_Config.m_AaHookSpam, m_LastAaHookSpam, "HookSpam");
	CheckToggle(g_Config.m_AaSpamEmote, m_LastAaSpamEmote, "SpamEmote");
	CheckToggle(g_Config.m_AaMoonwalk, m_LastAaMoonwalk, "Moonwalk");
}

void CHookCombo::ToggleMenuHider()
{
	if(!g_Config.m_BcMenuHider)
	{
		m_SavedAaEnabled = g_Config.m_AaEnabled;
		m_SavedSymLaserEnable = g_Config.m_SymLaserEnable;
		m_SavedAaAutoAled = g_Config.m_AaAutoAled;
		m_SavedAaAutoHammer = g_Config.m_AaAutoHammer;
		m_SavedAaFastFire = g_Config.m_AaFastFire;
		m_SavedAaHookSpam = g_Config.m_AaHookSpam;
		m_SavedAaSpamEmote = g_Config.m_AaSpamEmote;
		m_SavedAaMoonwalk = g_Config.m_AaMoonwalk;
		g_Config.m_AaEnabled = 0;
		g_Config.m_SymLaserEnable = 0;
		g_Config.m_AaAutoAled = 0;
		g_Config.m_AaAutoHammer = 0;
		g_Config.m_AaFastFire = 0;
		g_Config.m_AaHookSpam = 0;
		g_Config.m_AaSpamEmote = 0;
		g_Config.m_AaMoonwalk = 0;
		g_Config.m_BcMenuHider = 1;
	}
	else
	{
		g_Config.m_AaEnabled = m_SavedAaEnabled;
		g_Config.m_SymLaserEnable = m_SavedSymLaserEnable;
		g_Config.m_AaAutoAled = m_SavedAaAutoAled;
		g_Config.m_AaAutoHammer = m_SavedAaAutoHammer;
		g_Config.m_AaFastFire = m_SavedAaFastFire;
		g_Config.m_AaHookSpam = m_SavedAaHookSpam;
		g_Config.m_AaSpamEmote = m_SavedAaSpamEmote;
		g_Config.m_AaMoonwalk = m_SavedAaMoonwalk;
		g_Config.m_BcMenuHider = 0;
	}
}

int CHookCombo::GetActiveFunctionCount() const
{
	return m_vActiveFunctions.size();
}

void CHookCombo::CollectActiveFunctions()
{
	m_vActiveFunctions.clear();

	auto AddFunction = [this](const char *pName, bool Enabled, ColorRGBA Color) {
		if(Enabled)
			m_vActiveFunctions.push_back({pName, true, Color});
	};

	AddFunction("Aimbot", g_Config.m_AaEnabled != 0, ColorRGBA(1.0f, 0.47f, 0.32f, 1.0f));
	AddFunction("Laser Unfreeze", g_Config.m_SymLaserEnable != 0, ColorRGBA(0.42f, 0.72f, 1.0f, 1.0f));
	AddFunction("AutoAled", g_Config.m_AaAutoAled != 0, ColorRGBA(0.36f, 1.0f, 0.55f, 1.0f));
	AddFunction("Avoid", g_Config.m_NwAvoidEnabled != 0, ColorRGBA(1.0f, 0.72f, 0.36f, 1.0f));
	AddFunction("AutoHammer", g_Config.m_AaAutoHammer != 0, ColorRGBA(0.96f, 0.44f, 0.66f, 1.0f));
	AddFunction("FastFire", g_Config.m_AaFastFire != 0, ColorRGBA(1.0f, 0.85f, 0.3f, 1.0f));
	AddFunction("HookSpam", g_Config.m_AaHookSpam != 0, ColorRGBA(0.75f, 0.55f, 1.0f, 1.0f));
	AddFunction("SpamEmote", g_Config.m_AaSpamEmote != 0, ColorRGBA(0.62f, 0.82f, 1.0f, 1.0f));
	AddFunction("Moonwalk", g_Config.m_AaMoonwalk != 0, ColorRGBA(0.72f, 0.95f, 0.32f, 1.0f));
}

float CHookCombo::GetFunctionsPlayerTargetWidth() const
{
	const float Scale = std::clamp(HudLayout::Get(HudLayout::MODULE_HOOK_COMBO, HudLayout::CANVAS_WIDTH, HudLayout::CANVAS_HEIGHT).m_Scale / 100.0f, 0.25f, 3.0f);
	const float FontSize = 10.0f * Scale;
	float MaxWidth = 96.0f * Scale;
	for(const auto &Entry : m_vActiveFunctions)
	{
		const float TextWidth = TextRender()->TextWidth(FontSize, Entry.m_pName, -1, -1.0f);
		MaxWidth = maximum(MaxWidth, TextWidth + 24.0f * Scale);
	}
	return MaxWidth;
}

float CHookCombo::GetFunctionsPlayerTargetHeight() const
{
	const float Scale = std::clamp(HudLayout::Get(HudLayout::MODULE_HOOK_COMBO, HudLayout::CANVAS_WIDTH, HudLayout::CANVAS_HEIGHT).m_Scale / 100.0f, 0.25f, 3.0f);
	const float HeaderHeight = 18.0f * Scale;
	const float RowHeight = 13.0f * Scale;
	if(m_vActiveFunctions.empty())
		return HeaderHeight;
	return HeaderHeight + RowHeight * m_vActiveFunctions.size() + 4.0f * Scale;
}

CUIRect CHookCombo::GetFunctionsPlayerRect(bool ForcePreview) const
{
	(void)ForcePreview;
	// реальные HUD-пиксели, как у всех модулей и у редактора — иначе драг
	// и отрисовка живут в разных координатах и панель «упирается»/«убегает»
	const float Height = HudLayout::CANVAS_HEIGHT;
	const float Width = Height * Graphics()->ScreenAspect();
	const auto Layout = HudLayout::Get(HudLayout::MODULE_HOOK_COMBO, Width, Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);

	CUIRect Rect;
	Rect.x = Layout.m_X;
	Rect.y = Layout.m_Y;
	Rect.w = GetFunctionsPlayerTargetWidth() + 10.0f * Scale;
	Rect.h = GetFunctionsPlayerTargetHeight();
	return Rect;
}

void CHookCombo::RenderFunctionsPreview()
{
	RenderFunctionsPlayer(true);
}

void CHookCombo::RenderFunctionsPlayer(bool ForcePreview)
{
	if(!HudLayout::IsEnabled(HudLayout::MODULE_HOOK_COMBO) && !ForcePreview)
		return;

	if(g_Config.m_BcMenuHider)
		return;

	CollectActiveFunctions();

	// keep an age per function so rows animate in like freshly toggled popups
	{
		const float AgeDelta = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
		for(int i = 0; i < (int)m_vActiveFunctions.size() && i < (int)m_aFunctionAges.size(); ++i)
			m_aFunctionAges[i] += AgeDelta;
	}

	constexpr float HeaderHeight = 18.0f;
	constexpr float RowHeight = 13.0f;
	const float Height = HudLayout::CANVAS_HEIGHT;
	const float Width = Height * Graphics()->ScreenAspect();
	const auto Layout = HudLayout::Get(HudLayout::MODULE_HOOK_COMBO, Width, Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);

	const float TargetWidth = GetFunctionsPlayerTargetWidth() + 10.0f * Scale;
	const float TargetHeight = GetFunctionsPlayerTargetHeight();
	const float Delta = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
	if(m_FunctionsWidthAnim <= 0.0f)
		m_FunctionsWidthAnim = TargetWidth;
	if(m_FunctionsHeightAnim <= 0.0f)
		m_FunctionsHeightAnim = TargetHeight;
	m_FunctionsWidthAnim = ApproachAnim(m_FunctionsWidthAnim, TargetWidth, Delta, 8.0f);
	m_FunctionsHeightAnim = ApproachAnim(m_FunctionsHeightAnim, TargetHeight, Delta, 8.0f);

	const float BoxW = m_FunctionsWidthAnim;
	const float BoxH = m_FunctionsHeightAnim;
	// панель никогда не убегает за экран (смена aspect / старые конфиги)
	const float RectX = std::clamp(Layout.m_X, 0.0f, maximum(0.0f, Width - BoxW));
	const float RectY = std::clamp(Layout.m_Y, 0.0f, maximum(0.0f, Height - BoxH));

	// Player-style dark glass background
	Graphics()->BlendNormal();
	CUIRect BoxRect = {RectX, RectY, BoxW, BoxH};
	BoxRect.Draw(ColorRGBA(0.05f, 0.06f, 0.09f, 0.72f), IGraphics::CORNER_ALL, 8.0f * Scale);

	// Marquee-style header "ACTIVE FEATURES" with a pulsing equalizer dot
	ColorRGBA FunctionsColor(1.0f, 1.0f, 1.0f, 1.0f);
	const int ColorMode = std::clamp(g_Config.m_BcFunctionsColorMode, 0, 3);
	if(ColorMode == 1)
		FunctionsColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcFunctionsColor));
	else if(ColorMode == 2)
	{
		// Rainbow: hue cycles over time
		const float Hue = std::fmod(Client()->LocalTime() * 0.15f, 1.0f);
		FunctionsColor = color_cast<ColorRGBA>(ColorHSLA(Hue, 0.85f, 0.55f, 1.0f));
	}
	else if(ColorMode == 3)
		GameClient()->m_MusicPlayer.GetHudThemeColor(FunctionsColor);
	auto BlendColor = [](const ColorRGBA &A, const ColorRGBA &B, float t) {
		ColorRGBA R;
		R.r = A.r + (B.r - A.r) * t;
		R.g = A.g + (B.g - A.g) * t;
		R.b = A.b + (B.b - A.b) * t;
		R.a = A.a + (B.a - A.a) * t;
		return R;
	};
	const float HeaderFont = 7.0f * Scale;
	const float Pulse = 0.5f + 0.5f * std::sin(Client()->LocalTime() * 4.0f);
	ColorRGBA HeaderColor(0.96f, 0.96f, 0.98f, 0.55f + 0.25f * Pulse);
	TextRender()->TextColor(HeaderColor);
	TextRender()->Text(RectX + 8.0f * Scale, RectY + 4.0f * Scale, HeaderFont, "ACTIVE FEATURES", -1.0f);
	const ColorRGBA BarColorA = BlendColor(FunctionsColor, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), 0.15f).WithAlpha(0.9f);
	const ColorRGBA BarColorB = BlendColor(FunctionsColor, ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), 0.2f).WithAlpha(0.9f);
	const ColorRGBA BarColorC = FunctionsColor.WithAlpha(0.9f);
	Graphics()->DrawRect(RectX + BoxW - 24.0f * Scale, RectY + 5.0f * Scale, 3.0f * Scale, (HeaderFont - 2.0f * Scale) * Pulse, BarColorA, IGraphics::CORNER_ALL, 1.0f);
	Graphics()->DrawRect(RectX + BoxW - 19.0f * Scale, RectY + 3.0f * Scale, 3.0f * Scale, (HeaderFont - 1.0f * Scale) * (1.0f - Pulse), BarColorB, IGraphics::CORNER_ALL, 1.0f);
	Graphics()->DrawRect(RectX + BoxW - 14.0f * Scale, RectY + 5.0f * Scale, 3.0f * Scale, (HeaderFont - 2.0f * Scale) * (0.5f + 0.5f * Pulse), BarColorC, IGraphics::CORNER_ALL, 1.0f);

	// Equalizer-ish bar rows for each active function
	float RowY = RectY + HeaderHeight * Scale;
	for(const auto &Entry : m_vActiveFunctions)
	{
		// per-entry appear animation: each function fades in and slides down
		// like a freshly enabled toggle (same feel as the aimbot popups)
		const int AgeIndex = (&Entry - &m_vActiveFunctions.front());
		const float Age = (AgeIndex >= 0 && AgeIndex < (int)m_aFunctionAges.size()) ? m_aFunctionAges[AgeIndex] : 0.0f;
		float Alpha = std::clamp(Age / 0.35f, 0.0f, 1.0f);
		Alpha = Alpha * Alpha;
		ColorRGBA RowColor = FunctionsColor.WithAlpha(0.92f * Alpha);
		TextRender()->TextColor(RowColor);

		const float Slide = (1.0f - Alpha) * 8.0f * Scale;

		// leading color accent bar
		Graphics()->DrawRect(RectX + 4.0f * Scale, RowY + 3.0f * Scale, 2.4f * Scale, 7.0f * Scale, RowColor, IGraphics::CORNER_ALL, 1.0f * Scale);

		TextRender()->Text(RectX + 10.0f * Scale, RowY + 1.0f * Scale + Slide, 10.0f * Scale, Entry.m_pName, -1.0f);

		RowY += RowHeight * Scale;
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CHookCombo::TriggerStep()
{
	m_Counter++;

	SPopup Popup;
	Popup.m_Sequence = m_Counter;
	Popup.m_Age = 0.0f;
	m_vPopups.push_back(Popup);
	if(m_vPopups.size() > 16)
		m_vPopups.erase(m_vPopups.begin());

	if(!GameClient()->m_SuppressEvents && g_Config.m_SndEnable)
	{
		int SoundIndex = 0;
		if(m_Counter > s_VariantLimit)
			SoundIndex = s_BrilliantSoundIndex;
		else if(m_Counter <= s_SoundCount)
			SoundIndex = m_Counter - 1;
		else
			SoundIndex = (m_Counter - 1) % s_SoundCount;

		int SoundId = m_aSoundIds[SoundIndex];
		if(SoundId == -1)
		{
			// Retry at runtime, because startup path/audio init may differ from gameplay runtime.
			LoadSounds(false);
			SoundId = m_aSoundIds[SoundIndex];
		}
		if(SoundId != -1)
		{
			const float GameVol = (g_Config.m_SndGame && g_Config.m_SndGameVolume > 0) ? (float)g_Config.m_SndGameVolume : 0.0f;
			const float ChatVol = (g_Config.m_SndChat && g_Config.m_SndChatVolume > 0) ? (float)g_Config.m_SndChatVolume : 0.0f;
			const int Channel = GameVol >= ChatVol ? CSounds::CHN_GLOBAL : CSounds::CHN_GUI;
			const float ComboVol = std::clamp(g_Config.m_BcHookComboSoundVolume / 100.0f, 0.0f, 1.0f);
			if(ComboVol > 0.0f)
				Sound()->Play(Channel, SoundId, 0, ComboVol);
		}
		else if(!m_SoundErrorShown)
		{
			m_SoundErrorShown = true;
		GameClient()->Echo(Localize("Hook combo sounds not found. Put files as data/BestClient/combo/combo1.wv ... combo7.wv"));
		}
	}
}

bool CHookCombo::HasWork() const
{
	return g_Config.m_BcHookCombo != 0 || !m_vPopups.empty() || !m_vTogglePopups.empty();
}

void CHookCombo::OnRender()
{
	constexpr float PopupLifetime = 1.1f;
	const float FrameTime = Client()->RenderFrameTime();
	for(auto &Popup : m_vPopups)
		Popup.m_Age += FrameTime;
	m_vPopups.erase(std::remove_if(m_vPopups.begin(), m_vPopups.end(), [](const SPopup &Popup) {
		return Popup.m_Age >= PopupLifetime;
	}),
		m_vPopups.end());

	for(auto &Popup : m_vTogglePopups)
		Popup.m_Age += FrameTime;
	m_vTogglePopups.erase(std::remove_if(m_vTogglePopups.begin(), m_vTogglePopups.end(), [](const STogglePopup &Popup) {
		return Popup.m_Age >= PopupLifetime;
	}),
		m_vTogglePopups.end());

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		ResetState();
		return;
	}

	DetectConfigToggles();

	if(!g_Config.m_BcHookCombo)
	{
		m_vPopups.clear();
		return;
	}

	const bool IsDemoPlayback = Client()->State() == IClient::STATE_DEMOPLAYBACK;
	if(!IsDemoPlayback && GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	const int ComboMode = std::clamp(g_Config.m_BcHookComboMode, s_ModeHook, s_ModeHookAndHammer);
	int LocalId = -1;
	bool NewPlayerHook = false;
	bool NewHammerAttack = false;

	if(IsDemoPlayback)
	{
		if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			const int SpectatorId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;
			if(SpectatorId > SPEC_FREEVIEW && SpectatorId < MAX_CLIENTS && GameClient()->m_Snap.m_aCharacters[SpectatorId].m_Active)
				LocalId = SpectatorId;
		}
		else if(in_range(GameClient()->m_Snap.m_LocalClientId, 0, MAX_CLIENTS - 1) && GameClient()->m_Snap.m_aCharacters[GameClient()->m_Snap.m_LocalClientId].m_Active)
		{
			LocalId = GameClient()->m_Snap.m_LocalClientId;
		}

		if(LocalId < 0 || !GameClient()->m_aClients[LocalId].m_Active)
			return;

		const int CurrentGameTick = Client()->GameTick(0);
		if(m_LastProcessedGameTick > CurrentGameTick)
			ResetState();
		if(m_LastProcessedGameTick == CurrentGameTick)
			return;
		m_LastProcessedGameTick = CurrentGameTick;
	}
	else
	{
		const bool HammerEventFrame = GameClient()->m_aPredictedHammerHitEvent[g_Config.m_ClDummy];
		if(!GameClient()->m_NewPredictedTick && !(HammerEventFrame && ComboMode != s_ModeHook))
			return;

		LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
		if(LocalId < 0 || LocalId >= MAX_CLIENTS)
			LocalId = GameClient()->m_Snap.m_LocalClientId;
		if(LocalId < 0 || LocalId >= MAX_CLIENTS || !GameClient()->m_aClients[LocalId].m_Active)
			return;
	}

	if(LocalId != m_TrackedClientId)
	{
		m_TrackedClientId = LocalId;
		m_LastHookedPlayer = -1;
	}

	if(IsDemoPlayback)
	{
		const auto &TrackedCharacter = GameClient()->m_Snap.m_aCharacters[LocalId];
		const int HookedPlayer = TrackedCharacter.m_Cur.m_HookedPlayer;
		NewPlayerHook = HookedPlayer >= 0 && (m_LastHookedPlayer < 0 || HookedPlayer != m_LastHookedPlayer);
		m_LastHookedPlayer = HookedPlayer;
		NewHammerAttack = TrackedCharacter.m_Cur.m_AttackTick != TrackedCharacter.m_Prev.m_AttackTick &&
				  (TrackedCharacter.m_Cur.m_Weapon == WEAPON_HAMMER || TrackedCharacter.m_Prev.m_Weapon == WEAPON_HAMMER);
	}
	else
	{
		const int HookedPlayer = GameClient()->m_aClients[LocalId].m_Predicted.HookedPlayer();
		NewPlayerHook = HookedPlayer >= 0 && (m_LastHookedPlayer < 0 || HookedPlayer != m_LastHookedPlayer);
		m_LastHookedPlayer = HookedPlayer;
		NewHammerAttack = GameClient()->m_aPredictedHammerHitEvent[g_Config.m_ClDummy];
	}

	bool TriggerCombo = false;
	if(ComboMode == s_ModeHook)
		TriggerCombo = NewPlayerHook;
	else if(ComboMode == s_ModeHammer)
		TriggerCombo = NewHammerAttack;
	else
		TriggerCombo = NewPlayerHook || NewHammerAttack;

	if(TriggerCombo)
	{
		const float ResetTime = g_Config.m_BcHookComboResetTime / 1000.0f;
		const float Now = Client()->LocalTime();
		if(m_LastHookTime >= 0.0f && (Now - m_LastHookTime) > ResetTime)
			m_Counter = 0;
		m_LastHookTime = Now;
		TriggerStep();
	}
}

void CHookCombo::Render(bool ForcePreview)
{
	const bool FunctionsEnabled = HudLayout::IsEnabled(HudLayout::MODULE_HOOK_COMBO);
	if(!ForcePreview && !g_Config.m_BcHookCombo && m_vTogglePopups.empty() && !FunctionsEnabled)
		return;
	if(GameClient()->m_Scoreboard.IsActive() || (GameClient()->m_Menus.IsActive() && !ForcePreview))
		return;

	RenderFunctionsPlayer(ForcePreview);

	if(!ForcePreview && !g_Config.m_BcHookCombo && m_vTogglePopups.empty())
		return;

	constexpr float PopupLifetime = 1.1f;
	constexpr float FadeIn = 0.15f;
	constexpr float FadeOut = 0.25f;

	const float Width = 300.0f * Graphics()->ScreenAspect();
	const float Height = 300.0f;
	const float Scale = std::clamp(g_Config.m_BcHookComboSize / 100.0f, 0.5f, 2.0f);
	const float AnchorCenterX = Width * 0.5f;
	const float BaseY = Height * 0.84f;
	const float StackStep = 14.0f * Scale;

	int Stack = 0;
	SPopup PreviewPopup;
	PreviewPopup.m_Sequence = 7;
	PreviewPopup.m_Age = PopupLifetime * 0.35f;

	auto RenderPopup = [&](const SPopup &Popup) {
		const float Age = std::clamp(Popup.m_Age, 0.0f, PopupLifetime);
		const float In = std::clamp(Age / FadeIn, 0.0f, 1.0f);
		const float Out = Age > PopupLifetime - FadeOut ? std::clamp((PopupLifetime - Age) / FadeOut, 0.0f, 1.0f) : 1.0f;
		const float Alpha = ForcePreview ? 1.0f : In * Out;
		if(Alpha <= 0.0f)
			return;

		const int Sequence = std::max(Popup.m_Sequence, 1);
		const int ColorIndex = (Sequence - 1) % s_BaseTextCount;

		char aText[64];
		FormatText(Sequence, aText, sizeof(aText));
		char aBuf[96];
		str_format(aBuf, sizeof(aBuf), "%s (x%d)", aText, Sequence);

		ColorRGBA TextColor = s_aColors[ColorIndex];
		TextColor.a *= Alpha;
		TextRender()->TextColor(TextColor);

		const float FontSize = (ForcePreview ? 13.0f : (11.0f + In * 2.0f)) * Scale;
		const float TextWidth = TextRender()->TextWidth(FontSize, aBuf, -1, -1.0f);
		const float BoxWidth = TextWidth + 8.0f * Scale;
		const float BoxHeight = FontSize + 4.0f * Scale;
		const float Intro = 1.0f - (1.0f - In) * (1.0f - In);
		const float Rise = ForcePreview ? 0.0f : (20.0f * Intro + Age * 10.0f) * Scale;
		const float RectX = std::clamp(AnchorCenterX - BoxWidth * 0.5f, 0.0f, std::max(0.0f, Width - BoxWidth));
		const float RectY = std::clamp(ForcePreview ? BaseY : (BaseY + (1.0f - In) * 12.0f * Scale - Stack * StackStep - Rise), 0.0f, std::max(0.0f, Height - BoxHeight));
		TextRender()->Text(RectX + 4.0f * Scale, RectY + 2.0f * Scale, FontSize, aBuf, -1.0f);
	};

	if(ForcePreview)
	{
		RenderPopup(PreviewPopup);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		return;
	}

	for(auto It = m_vPopups.rbegin(); It != m_vPopups.rend(); ++It, ++Stack)
		RenderPopup(*It);

	if(!m_vTogglePopups.empty())
	{
		const float ToggleBaseY = 80.0f * Scale;
		const float ToggleFontSize = 11.0f * Scale;
		int ToggleStack = 0;
		for(auto It = m_vTogglePopups.rbegin(); It != m_vTogglePopups.rend(); ++It, ++ToggleStack)
		{
			const float Age = std::clamp(It->m_Age, 0.0f, PopupLifetime);
			const float In = std::clamp(Age / FadeIn, 0.0f, 1.0f);
			const float Out = Age > PopupLifetime - FadeOut ? std::clamp((PopupLifetime - Age) / FadeOut, 0.0f, 1.0f) : 1.0f;
			const float Alpha = In * Out;
			if(Alpha <= 0.0f)
				continue;

			TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
			const float TextWidth = TextRender()->TextWidth(ToggleFontSize, It->m_aText, -1, -1.0f);
			const float BoxWidth = TextWidth + 8.0f * Scale;
			const float Intro = 1.0f - (1.0f - In) * (1.0f - In);
			const float Rise = (20.0f * Intro + Age * 10.0f) * Scale;
			const float RectX = std::clamp(AnchorCenterX - BoxWidth * 0.5f, 0.0f, std::max(0.0f, Width - BoxWidth));
			const float RectY = std::clamp(ToggleBaseY + (1.0f - In) * 12.0f * Scale - ToggleStack * StackStep - Rise, 0.0f, std::max(0.0f, Height - ToggleFontSize));
			TextRender()->Text(RectX + 4.0f * Scale, RectY + 2.0f * Scale, ToggleFontSize, It->m_aText, -1.0f);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
