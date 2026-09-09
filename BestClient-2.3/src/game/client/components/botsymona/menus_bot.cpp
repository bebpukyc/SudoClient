#include "botsymona.h"

#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

static constexpr float BOT_LINE = 20.0f;
static constexpr float BOT_LABEL_W = 96.0f;
static constexpr float BOT_PAD = 10.0f;

static const ColorRGBA BOT_PANEL = ColorRGBA(1.0f, 1.0f, 1.0f, 0.055f);
static const ColorRGBA BOT_TITLE = ColorRGBA(0.78f, 0.84f, 0.95f, 1.0f);

static void BotPanel(CUi *pUi, ITextRender *pTextRender, CUIRect *pView, const char *pTitle, float Height, CUIRect *pBody)
{
	CUIRect Panel;
	pView->HSplitTop(Height, &Panel, pView);
	pView->HSplitTop(8.0f, nullptr, pView);
	Panel.Draw(BOT_PANEL, IGraphics::CORNER_ALL, 8.0f);
	Panel.Margin(BOT_PAD, &Panel);

	CUIRect Head;
	Panel.HSplitTop(16.0f, &Head, &Panel);
	Panel.HSplitTop(6.0f, nullptr, &Panel);
	pTextRender->TextColor(BOT_TITLE);
	pUi->DoLabel(&Head, pTitle, 13.0f, TEXTALIGN_ML);
	pTextRender->TextColor(pTextRender->DefaultTextColor());

	*pBody = Panel;
}

static void BotField(CUi *pUi, CUIRect *pBody, const char *pLabel, CLineInput *pInput)
{
	CUIRect Row, Label;
	pBody->HSplitTop(BOT_LINE, &Row, pBody);
	pBody->HSplitTop(6.0f, nullptr, pBody);
	Row.VSplitLeft(BOT_LABEL_W, &Label, &Row);
	pUi->DoLabel(&Label, pLabel, 12.0f, TEXTALIGN_ML);
	pUi->DoEditBox(pInput, &Row, 12.0f);
}

void CMenus::RenderSettingsBotWar(CUIRect MainView)
{
	CUIRect LeftView, RightView, Body, Row;
	MainView.VSplitMid(&LeftView, &RightView, 16.0f);

	BotPanel(Ui(), TextRender(), &LeftView, "Война", 118.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotPvp, "Воевать", &g_Config.m_BotPvp, &Body, BOT_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	Body.HSplitTop(BOT_LINE, &Row, &Body);
	Ui()->DoScrollbarOption(&g_Config.m_BotWarRange, &g_Config.m_BotWarRange, &Row, "Радиус войны", 200, 2000);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotWarAggro, "Кидаться на врага", &g_Config.m_BotWarAggro, &Body, BOT_LINE);

	BotPanel(Ui(), TextRender(), &RightView, "Таргет / оружие", 118.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotWarAll, "Воевать со всеми", &g_Config.m_BotWarAll, &Body, BOT_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotWarExecute, "Добить во фризе", &g_Config.m_BotWarExecute, &Body, BOT_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	Body.HSplitTop(BOT_LINE, &Row, &Body);
	Ui()->DoScrollbarOption(&g_Config.m_BotWarWeapon, &g_Config.m_BotWarWeapon, &Row, "Оружие: авто", 0, 4);
}

void CMenus::RenderSettingsBotBehaviour(CUIRect MainView)
{
	CUIRect LeftView, RightView, Body;
	MainView.VSplitMid(&LeftView, &RightView, 16.0f);

	CUIRect Row;
	BotPanel(Ui(), TextRender(), &LeftView, "Работа", 142.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotRescue, "Вытаскивать из фриза", &g_Config.m_BotRescue, &Body, BOT_LINE);
	Body.HSplitTop(4.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotHelpAll, "Помогать всем рядом", &g_Config.m_BotHelpAll, &Body, BOT_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	Body.HSplitTop(BOT_LINE, &Row, &Body);
	Ui()->DoScrollbarOption(&g_Config.m_BotFollowDist, &g_Config.m_BotFollowDist, &Row, "Дистанция", 60, 500);
	Body.HSplitTop(4.0f, nullptr, &Body);
	Body.HSplitTop(BOT_LINE, &Row, &Body);
	Ui()->DoScrollbarOption(&g_Config.m_BotHelpRange, &g_Config.m_BotHelpRange, &Row, "Радиус помощи", 200, 2000);

	BotPanel(Ui(), TextRender(), &LeftView, "Гуляние", 88.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotRoam, "Гулять самому", &g_Config.m_BotRoam, &Body, BOT_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	Body.HSplitTop(BOT_LINE, &Row, &Body);
	Ui()->DoScrollbarOption(&g_Config.m_BotRoamRadius, &g_Config.m_BotRoamRadius, &Row, "Радиус", 160, 2000);

	BotPanel(Ui(), TextRender(), &LeftView, "Чем спасает", 136.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotUseHammer, "Молот", &g_Config.m_BotUseHammer, &Body, BOT_LINE);
	Body.HSplitTop(4.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotUseHook, "Хук", &g_Config.m_BotUseHook, &Body, BOT_LINE);
	Body.HSplitTop(4.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotUseLaser, "Винтовка (анфриз издалека)", &g_Config.m_BotUseLaser, &Body, BOT_LINE);
	Body.HSplitTop(4.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotUseShotgun, "Дробовик (стягивать)", &g_Config.m_BotUseShotgun, &Body, BOT_LINE);

	BotPanel(Ui(), TextRender(), &RightView, "Осторожность", 118.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotGentle, "Аккуратный режим", &g_Config.m_BotGentle, &Body, BOT_LINE);
	Body.HSplitTop(4.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotSafePull, "Беречь владельца", &g_Config.m_BotSafePull, &Body, BOT_LINE);
	Body.HSplitTop(4.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotAvoidFreeze, "Не лезть во фриз", &g_Config.m_BotAvoidFreeze, &Body, BOT_LINE);

	BotPanel(Ui(), TextRender(), &RightView, "Манёвры", 92.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotCrossGaps, "Прыгать через фриз", &g_Config.m_BotCrossGaps, &Body, BOT_LINE);
	Body.HSplitTop(4.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotDebug, "Писать в лог", &g_Config.m_BotDebug, &Body, BOT_LINE);
}

void CMenus::RenderSettingsBot(CUIRect MainView)
{
	CUIRect TabBar, TabButton, StatusBar, Body, Row, LeftView, RightView;

	static int s_Page = 0;

	MainView.HSplitTop(24.0f, &TabBar, &MainView);
	{
		static const char *s_apTabs[4] = {"FunBot", "Поведение", "Война", "AI"};
		static CButtonContainer s_aTabButtons[4];
		const float Width = TabBar.w / 4.0f;
		CUIRect Rest = TabBar;
		for(int i = 0; i < 4; i++)
		{
			Rest.VSplitLeft(Width, &TabButton, &Rest);
			const int Corners = i == 0 ? IGraphics::CORNER_L : (i == 3 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
			if(DoButton_MenuTab(&s_aTabButtons[i], s_apTabs[i], s_Page == i, &TabButton, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
				s_Page = i;
		}
	}
	MainView.HSplitTop(12.0f, nullptr, &MainView);

	CBotSymona &Bot = GameClient()->m_BotSymona;
	char aBuf[256];

	MainView.HSplitBottom(26.0f, &MainView, &StatusBar);
	{
		StatusBar.HSplitTop(6.0f, nullptr, &StatusBar);
		StatusBar.Draw(BOT_PANEL, IGraphics::CORNER_ALL, 6.0f);
		StatusBar.VMargin(BOT_PAD, &StatusBar);
		str_format(aBuf, sizeof(aBuf), "%s   %s", Bot.ConnectionText(), Bot.StatusText());
		TextRender()->TextColor(0.72f, 0.86f, 0.74f, 1.0f);
		Ui()->DoLabel(&StatusBar, aBuf, 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	if(s_Page == 3)
	{
		RenderSettingsBotAi(MainView);
		return;
	}

	if(s_Page == 2)
	{
		RenderSettingsBotWar(MainView);
		return;
	}

	if(s_Page == 1)
	{
		RenderSettingsBotBehaviour(MainView);
		return;
	}

	MainView.VSplitMid(&LeftView, &RightView, 16.0f);

	BotPanel(Ui(), TextRender(), &LeftView, "Соединение", 128.0f, &Body);
	static CLineInput s_BotNameInput;
	s_BotNameInput.SetBuffer(g_Config.m_BotName, sizeof(g_Config.m_BotName));
	s_BotNameInput.SetEmptyText("funbot");
	BotField(Ui(), &Body, "Ник бота", &s_BotNameInput);

	Body.HSplitTop(24.0f, &Row, &Body);
	{
		CUIRect JoinButton, LeaveButton;
		Row.VSplitMid(&JoinButton, &LeaveButton, 8.0f);

		const char *pReason = "";
		const bool CanJoin = Bot.CanJoin(&pReason);
		const bool OnServer = Bot.BotOnServer();

		static CButtonContainer s_JoinButton;
		if(DoButton_Menu(&s_JoinButton, "Зайти", 0, &JoinButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f,
			   CanJoin ? ColorRGBA(0.3f, 0.7f, 0.3f, 0.6f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f)))
		{
			Bot.JoinServer();
		}

		static CButtonContainer s_LeaveButton;
		if(DoButton_Menu(&s_LeaveButton, "Убрать", 0, &LeaveButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f,
			   OnServer ? ColorRGBA(0.7f, 0.3f, 0.3f, 0.5f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f)))
		{
			Bot.LeaveServer();
		}
	}
	Body.HSplitTop(8.0f, nullptr, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotAutoJoin, "Заходить самому", &g_Config.m_BotAutoJoin, &Body, BOT_LINE);

	BotPanel(Ui(), TextRender(), &LeftView, "Питание", 64.0f, &Body);
	{
		CUIRect PowerButton;
		Body.HSplitTop(24.0f, &PowerButton, &Body);
		const bool On = g_Config.m_BotEnable != 0;
		static CButtonContainer s_PowerButton;
		if(DoButton_Menu(&s_PowerButton, On ? "Бот включён" : "Бот выключен", 0, &PowerButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f,
			   On ? ColorRGBA(0.28f, 0.68f, 0.34f, 0.75f) : ColorRGBA(0.70f, 0.27f, 0.27f, 0.6f)))
		{
			g_Config.m_BotEnable = On ? 0 : 1;
			GameClient()->m_BotSymona.OnReset();
		}
	}

	BotPanel(Ui(), TextRender(), &RightView, "Владелец", 58.0f, &Body);
	static CLineInput s_BotOwnerInput;
	s_BotOwnerInput.SetBuffer(g_Config.m_BotOwner, sizeof(g_Config.m_BotOwner));
	s_BotOwnerInput.SetEmptyText("твой ник");
	BotField(Ui(), &Body, "Ник", &s_BotOwnerInput);

	BotPanel(Ui(), TextRender(), &RightView, "За кем ходить", std::max(60.0f, RightView.h - 8.0f), &Body);
	{
		CUIRect Hint;
		Body.HSplitTop(14.0f, &Hint, &Body);
		Body.HSplitTop(4.0f, nullptr, &Body);
		TextRender()->TextColor(0.65f, 0.65f, 0.65f, 1.0f);
		Ui()->DoLabel(&Hint, g_Config.m_BotFollowName[0] != '\0' ? "клик по нику ещё раз — перестать ходить" : "выключено, бот гуляет сам", 10.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	static CButtonContainer s_aPlayerButtons[MAX_CLIENTS];
	const int BotId = Bot.BotClientId();
	int Shown = 0;
	for(int i = 0; i < MAX_CLIENTS && Body.h >= 18.0f; i++)
	{
		if(i == BotId || !GameClient()->m_aClients[i].m_Active)
			continue;

		CUIRect PlayerRow;
		Body.HSplitTop(17.0f, &PlayerRow, &Body);
		Body.HSplitTop(3.0f, nullptr, &Body);
		const char *pName = GameClient()->m_aClients[i].m_aName;
		const bool IsFollowed = g_Config.m_BotFollowName[0] != '\0' && str_comp(pName, g_Config.m_BotFollowName) == 0;
		if(DoButton_Menu(&s_aPlayerButtons[i], pName, 0, &PlayerRow, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 3.0f, 0.0f,
			   IsFollowed ? ColorRGBA(0.3f, 0.7f, 0.3f, 0.6f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.16f)))
		{
			if(IsFollowed)
				g_Config.m_BotFollowName[0] = '\0';
			else
				str_copy(g_Config.m_BotFollowName, pName);
		}
		Shown++;
	}
	if(Shown == 0)
	{
		Body.HSplitTop(16.0f, &Row, &Body);
		TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
		Ui()->DoLabel(&Row, "пусто", 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
}

static bool BotAiMultiLineEditBox(CUi *pUi, CLineInput *pInput, const CUIRect *pRect, float FontSize, float LineSpacing)
{
	return pUi->DoEditBox(pInput, pRect, FontSize, IGraphics::CORNER_ALL, {}, pRect->w - 8.0f, LineSpacing);
}

void CMenus::RenderSettingsBotAi(CUIRect MainView)
{
	CUIRect LeftView, RightView, Body, Row;

	CBotSymona &Bot = GameClient()->m_BotSymona;

	MainView.VSplitMid(&LeftView, &RightView, 16.0f);

	BotPanel(Ui(), TextRender(), &LeftView, "ИИ / Ollama", 104.0f, &Body);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BotAi, "Отвечать на тег в чате", &g_Config.m_BotAi, &Body, BOT_LINE);
	Body.HSplitTop(6.0f, nullptr, &Body);
	Body.HSplitTop(BOT_LINE, &Row, &Body);
	TextRender()->TextColor(0.65f, 0.65f, 0.65f, 1.0f);
	Ui()->DoLabel(&Row, "Напиши в чате ник бота и вопрос — ответ придёт в чат", 10.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	BotPanel(Ui(), TextRender(), &LeftView, "Провайдер (Ollama)", 116.0f, &Body);
	static CLineInput s_AiUrlInput;
	s_AiUrlInput.SetBuffer(g_Config.m_BotAiUrl, sizeof(g_Config.m_BotAiUrl));
	s_AiUrlInput.SetEmptyText("http://localhost:11434/v1/chat/completions");
	BotField(Ui(), &Body, "URL", &s_AiUrlInput);

	static CLineInput s_AiModelInput;
	s_AiModelInput.SetBuffer(g_Config.m_BotAiModel, sizeof(g_Config.m_BotAiModel));
	s_AiModelInput.SetEmptyText("llama3.2");
	BotField(Ui(), &Body, "Модель", &s_AiModelInput);

	static CLineInput s_AiKeyInput;
	s_AiKeyInput.SetBuffer(g_Config.m_BotAiKey, sizeof(g_Config.m_BotAiKey));
	s_AiKeyInput.SetEmptyText("ключ (необязательно)");
	BotField(Ui(), &Body, "API ключ", &s_AiKeyInput);

	BotPanel(Ui(), TextRender(), &LeftView, "Состояние", 88.0f, &Body);
	Body.HSplitTop(16.0f, &Row, &Body);
	Body.HSplitTop(4.0f, nullptr, &Body);
	const char *pConn = Bot.ConnectionText();
	TextRender()->TextColor(0.72f, 0.86f, 0.74f, 1.0f);
	Ui()->DoLabel(&Row, pConn, 11.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Body.HSplitTop(16.0f, &Row, &Body);
	const char *pAi = Bot.AiStatusText();
	TextRender()->TextColor(0.78f, 0.78f, 0.85f, 1.0f);
	Ui()->DoLabel(&Row, pAi, 11.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	BotPanel(Ui(), TextRender(), &RightView, "Промт для ИИ", std::max(140.0f, RightView.h - 8.0f), &Body);
	static CLineInput s_AiPromptInput;
	s_AiPromptInput.SetBuffer(g_Config.m_BotAiPrompt, sizeof(g_Config.m_BotAiPrompt));
	s_AiPromptInput.SetAllowNewline(true);
	Body.HSplitTop(80.0f, &Row, &Body);
	BotAiMultiLineEditBox(Ui(), &s_AiPromptInput, &Row, 11.0f, 2.0f);
}
