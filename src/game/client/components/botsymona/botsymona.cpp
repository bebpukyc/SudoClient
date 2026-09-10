#include "botsymona.h"

#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <base/time.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <algorithm>
#include <cstdarg>

void CBotSymona::TrimName(char *pDst, int Size, const char *pSrc)
{
	str_copy(pDst, str_utf8_skip_whitespaces(pSrc), Size);
	str_utf8_trim_right(pDst);
}

void CBotSymona::OnInit()
{
	LoadRoles();
}

void CBotSymona::ConBotJoin(IConsole::IResult *pResult, void *pUserData)
{
	((CBotSymona *)pUserData)->JoinServer();
}

void CBotSymona::ConBotLeave(IConsole::IResult *pResult, void *pUserData)
{
	((CBotSymona *)pUserData)->LeaveServer();
}

void CBotSymona::OnConsoleInit()
{
	Console()->Register("bot_join", "", CFGFLAG_CLIENT, ConBotJoin, this, "BotSymona: put the bot on the server");
	Console()->Register("bot_leave", "", CFGFLAG_CLIENT, ConBotLeave, this, "BotSymona: take the bot off the server");
}

void CBotSymona::RestoreDummyName()
{
	if(!m_DummyNameSaved)
		return;
	str_copy(g_Config.m_ClDummyName, m_aSavedDummyName);
	m_aSavedDummyName[0] = '\0';
	m_DummyNameSaved = false;
}

void CBotSymona::OnRender()
{
	PollAi();

	if(m_AiQueued && !m_pAiRequest)
	{
		m_AiQueued = false;
		SendAiRequest();
	}

	if(g_Config.m_BotAi && !m_AiWarmed && Client()->DummyConnected() && !m_AiWarming)
		SendWarmupRequest();

	if(m_ReturnCamera)
	{
		if(!Client()->DummyConnected() && !Client()->DummyConnecting())
		{
			m_ReturnCamera = false;
			RestoreDummyName();
		}
		else if(GameClient()->m_aLocalIds[1] >= 0)
		{
			g_Config.m_ClDummy = 0;
			m_ReturnCamera = false;
		}
	}
	if(g_Config.m_BotEnable && Client()->DummyConnected() && g_Config.m_ClDummy != 0 && !m_ReturnCamera)
		g_Config.m_ClDummy = 0;
	if(!Client()->DummyConnected() && !Client()->DummyConnecting())
		RestoreDummyName();

	if(g_Config.m_BotDebug && Client()->State() == IClient::STATE_ONLINE)
	{
		const int64_t Now = time_get();
		if(m_NextDebugLog == 0 || Now > m_NextDebugLog)
		{
			m_NextDebugLog = Now + time_freq();
			const int Id = BotClientId();
			if(Id >= 0)
			{
				const auto &D = GameClient()->m_aClients[Id];
				log_info("botsymona", "[%s] botid=%d ownerid=%d freeze=%d deep=%d live=%d tick=%d | %s",
					ConnectionText(), Id, FindOwner(), D.m_FreezeEnd, D.m_DeepFrozen ? 1 : 0, D.m_LiveFrozen ? 1 : 0,
					Client()->GameTick(BotConn()), m_aStatus);
			}
			else
			{
				log_info("botsymona", "[%s] botid=-1 ownerid=%d | %s", ConnectionText(), FindOwner(), m_aStatus);
			}
		}
	}

	if(!g_Config.m_BotEnable || !g_Config.m_BotAutoJoin)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	if(Client()->DummyConnected() || Client()->DummyConnecting())
		return;

	const int64_t Now = time_get();
	if(m_NextJoinTry != 0 && Now < m_NextJoinTry)
		return;
	m_NextJoinTry = Now + time_freq() * 5;

	const char *pReason;
	if(CanJoin(&pReason))
		JoinServer();
}

void CBotSymona::OnReset()
{
	m_State = S_FOLLOW;
	m_AnchorValid = false;
	m_JumpHeld = false;
	m_StuckTicks = 0;
	m_HookRetries = 0;
	m_LastPullDist = 0.0f;
	m_LastPos = vec2(0.0f, 0.0f);
	m_Anchor = vec2(0.0f, 0.0f);
	DropPlan();

	m_PlanTick = -1;
	m_CommitUntil = -1;
	m_ReleaseUntil = -1;
	m_HookFiredTick = -1;
	m_GrabbedTick = -1;
	m_LastProgressTick = -1;
	m_ProgressTick = -1;
	m_LastDebugTick = -1;
	m_LastHammerTick = -1;
	m_LastShotTick = -1;
	m_NoHammerUntil = -1;
	m_HammerEnterTick = -1;
	m_JumpPressTick = -1;
	m_AimPoint = vec2(1.0f, 0.0f);
	m_FollowUntilTick = -1;
	m_FollowId = -1;
	m_SubjectMode = SUBJ_NONE;
	m_HomeSet = false;
	m_RoamValid = false;
	m_RoamUntil = -1;
	m_LastHammerHit = false;
	m_NextBeamTick = -1;
	m_BeamUntil = -1;
	m_BeamWeapon = -1;
	m_JumpBias = 0.0f;
	m_EscapeUntil = -1;
	m_ClimbStart = -1;
	m_ClimbGainTick = -1;
	m_NextHookPlanTick = -1;
	m_NoHookUntil = -1;
	m_LaunchStart = -1;
	m_ClimbBestY = 0.0f;
	m_RejectedNext = 0;
	for(SRejected &R : m_aRejected)
		R.m_Tick = 0;

	m_OwnerId = -1;
	m_LastOwnerId = -2;
	m_OwnerStateWas = -1;
	m_NextTeamJoin = 0;
	m_Paused = false;
	m_PvpEnemyId = -1;
	m_PvpSwitchTick = -1;
	m_PvpStrafe = vec2(0.0f, 1.0f);
	m_PvpStrafeTick = -1;
	m_NoKillShotUntil = -1;
	m_aPvpTarget[0] = '\0';
	m_PvpTargetTick = -1;
	m_AiWarmed = false;
	m_AiWarming = false;
	m_AiQueued = false;
	str_copy(m_aStatus, g_Config.m_BotEnable ? "ждёт" : "выключен");
}

void CBotSymona::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_OFFLINE)
		OnReset();
}

int CBotSymona::BotClientId() const
{
	if(!Client()->DummyConnected())
		return -1;
	return GameClient()->m_aLocalIds[1];
}

bool CBotSymona::BotOnServer() const
{
	return Client()->DummyConnected() && BotClientId() >= 0;
}

int CBotSymona::BotConn() const
{
	return IClient::CONN_DUMMY;
}

int CBotSymona::PredTickNow() const
{
	return Client()->PredGameTick(g_Config.m_ClDummy);
}

const void *CBotSymona::BotSnapItem(int Type, int Id) const
{
	return Client()->SnapFindItem(IClient::SNAP_CURRENT, Type, Id);
}

bool CBotSymona::CanJoin(const char **ppReason) const
{
	if(Client()->State() != IClient::STATE_ONLINE)
	{
		*ppReason = "ты сам не на сервере";
		return false;
	}
	if(Client()->DummyConnected() || Client()->DummyConnecting())
	{
		*ppReason = "бот уже заходит / уже здесь";
		return false;
	}
	if(!Client()->DummyAllowed())
	{
		*ppReason = "сервер запретил второе соединение";
		return false;
	}
	if(Client()->DummyConnectingDelayed())
	{
		*ppReason = "слишком часто, подожди пару секунд";
		return false;
	}
	*ppReason = "";
	return true;
}

void CBotSymona::JoinServer()
{
	const char *pReason;
	if(!CanJoin(&pReason))
	{
		SetStatus("зайти нельзя: %s", pReason);
		return;
	}

	if(g_Config.m_BotName[0] != '\0')
	{
		if(!m_DummyNameSaved)
		{
			str_copy(m_aSavedDummyName, g_Config.m_ClDummyName);
			m_DummyNameSaved = true;
		}
		str_copy(g_Config.m_ClDummyName, g_Config.m_BotName);
	}

	str_copy(m_aStatus, "бот заходит на сервер ...");
	log_info("botsymona", "joining as '%s'", Client()->DummyName());
	m_ReturnCamera = true;
	Client()->DummyConnect();
}

void CBotSymona::LeaveServer()
{
	if(!Client()->DummyConnected() && !Client()->DummyConnecting())
		return;
	Client()->DummyDisconnect("bot left");
	m_ReturnCamera = false;
	RestoreDummyName();
	str_copy(m_aStatus, "бот ушёл с сервера");
}

const char *CBotSymona::ConnectionText() const
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return "ты не на сервере";
	if(Client()->DummyConnecting())
		return "бот заходит ...";
	if(!Client()->DummyConnected())
		return "бота нет на сервере";
	if(BotClientId() < 0)
		return "бот зашёл, ждёт снапшот";
	return "бот на сервере";
}

int CBotSymona::CountVisiblePlayers() const
{
	const int BotId = BotClientId();
	int Count = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i != BotId && GameClient()->m_aClients[i].m_Active)
			Count++;
	}
	return Count;
}

int CBotSymona::FindOwner() const
{
	if(g_Config.m_BotOwner[0] == '\0')
	{
		m_OwnerId = -1;
		return -1;
	}

	char aWanted[MAX_NAME_LENGTH];
	TrimName(aWanted, sizeof(aWanted), g_Config.m_BotOwner);
	if(aWanted[0] == '\0')
	{
		m_OwnerId = -1;
		return -1;
	}

	const int BotId = BotClientId();

	const int SelfId = GameClient()->m_aLocalIds[0];
	if(SelfId >= 0 && SelfId != BotId && GameClient()->m_aClients[SelfId].m_Active)
	{
		char aName[MAX_NAME_LENGTH];
		TrimName(aName, sizeof(aName), GameClient()->m_aClients[SelfId].m_aName);
		if(str_utf8_comp_nocase(aName, aWanted) == 0)
		{
			m_OwnerId = SelfId;
			return SelfId;
		}
	}

	if(m_OwnerId >= 0 && m_OwnerId != BotId && GameClient()->m_aClients[m_OwnerId].m_Active)
	{
		char aName[MAX_NAME_LENGTH];
		TrimName(aName, sizeof(aName), GameClient()->m_aClients[m_OwnerId].m_aName);
		if(str_utf8_comp_nocase(aName, aWanted) == 0)
			return m_OwnerId;
	}

	int Match = -1;
	int Matches = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == BotId || !GameClient()->m_aClients[i].m_Active)
			continue;
		char aName[MAX_NAME_LENGTH];
		TrimName(aName, sizeof(aName), GameClient()->m_aClients[i].m_aName);
		if(str_utf8_comp_nocase(aName, aWanted) != 0)
			continue;
		Matches++;
		if(Match < 0)
			Match = i;
	}

	if(Matches > 1)
	{
		m_OwnerId = -1;
		return -1;
	}

	m_OwnerId = Match;
	return Match;
}

void CBotSymona::SetStatus(const char *pFormat, ...)
{
	va_list Args;
	va_start(Args, pFormat);
	str_format_v(m_aStatus, sizeof(m_aStatus), pFormat, Args);
	va_end(Args);
}

void CBotSymona::SetAiStatus(const char *pFormat, ...)
{
	va_list Args;
	va_start(Args, pFormat);
	str_format_v(m_aAiStatus, sizeof(m_aAiStatus), pFormat, Args);
	va_end(Args);
}

const char *CBotSymona::AiJsonEscape(char *pOut, int OutSize, const char *pIn)
{
	if(!pOut || OutSize <= 0)
		return "";
	int Out = 0;
	for(int i = 0; pIn && pIn[i] && Out + 1 < OutSize; i++)
	{
		const char C = pIn[i];
		if((C == '\\' || C == '"' || C == '\n' || C == '\r' || C == '\t') && Out + 2 >= OutSize)
			break;
		if(C == '\\' || C == '"')
		{
			pOut[Out++] = '\\';
			pOut[Out++] = C;
		}
		else if(C == '\n')
		{
			pOut[Out++] = '\\';
			pOut[Out++] = 'n';
		}
		else if(C == '\r')
		{
			pOut[Out++] = '\\';
			pOut[Out++] = 'r';
		}
		else if(C == '\t')
		{
			pOut[Out++] = '\\';
			pOut[Out++] = 't';
		}
		else
			pOut[Out++] = C;
	}
	pOut[Out] = '\0';
	return pOut;
}

bool CBotSymona::TryAiMention(int SenderId, int Team, const char *pText)
{
	if(!g_Config.m_BotEnable || !g_Config.m_BotAi)
		return false;
	if(!BotOnServer())
		return false;
	if(SenderId < 0 || SenderId >= MAX_CLIENTS || SenderId == BotClientId())
		return false;
	if(!GameClient()->m_aClients[SenderId].m_Active || GameClient()->m_aClients[SenderId].m_aName[0] == '\0')
		return false;

	const int64_t Now = time_get();
	if(Now < m_NextAiReply)
		return false;

	// the message has to contain the bot's own name to count as a tag
	const char *apNames[2] = {nullptr, nullptr};
	if(g_Config.m_BotName[0] != '\0')
		apNames[0] = g_Config.m_BotName;
	char aDummyName[MAX_NAME_LENGTH];
	aDummyName[0] = '\0';
	const int BotId = BotClientId();
	if(BotId >= 0 && GameClient()->m_aClients[BotId].m_Active)
	{
		TrimName(aDummyName, sizeof(aDummyName), GameClient()->m_aClients[BotId].m_aName);
		if(aDummyName[0] != '\0')
			apNames[1] = aDummyName;
	}

	const char *pTagged = nullptr;
	for(const char *pName : apNames)
	{
		if(!pName || pName[0] == '\0')
			continue;
		const char *pFound = str_find_nocase(pText, pName);
		if(pFound)
		{
			pTagged = pName;
			break;
		}
	}
	if(!pTagged)
		return false;

	// something has to be written besides the tag itself
	if(str_length(pText) <= str_length(pTagged) + 1)
		return false;

	// strip the leading tag so the AI gets just what was asked
	const char *pQue = str_utf8_skip_whitespaces(pText);
	if(str_utf8_comp_nocase_num(pQue, pTagged, str_length(pTagged)) == 0)
	{
		pQue += str_length(pTagged);
		pQue = str_utf8_skip_whitespaces(pQue);
		if(*pQue == ':' || *pQue == ',')
			pQue = str_utf8_skip_whitespaces(pQue + 1);
	}
	str_copy(m_aAiQuestion, pQue, sizeof(m_aAiQuestion));
	if(m_aAiQuestion[0] == '\0')
		return false;

	TrimName(m_aAiReplyName, sizeof(m_aAiReplyName), GameClient()->m_aClients[SenderId].m_aName);
	m_AiReplyTeam = Team;
	m_NextAiReply = Now + time_freq() * 5;
	SetAiStatus("ИИ думает над ответом (%s)...", m_aAiQuestion);
	if(g_Config.m_BotDebug)
		log_info("botsymona", "AI tag from '%s': '%s'", m_aAiReplyName, m_aAiQuestion);
	if(m_pAiRequest && !m_pAiRequest->Done())
	{
		// a request is already in flight (e.g. model warmup), answer after it
		m_AiQueued = true;
	}
	else
		SendAiRequest();
	return true;
}

void CBotSymona::SendAiRequest(bool Warmup)
{
	if(m_pAiRequest)
		return;
	if(g_Config.m_BotAiUrl[0] == '\0')
	{
		SetAiStatus("ИИ выключен (нужен адрес Ollama)");
		return;
	}

	// Ollama listens on plain http://; the curl engine refuses http:// unless
	// insecure HTTP is allowed, so enable it automatically for local AI.
	if(str_startswith_nocase(g_Config.m_BotAiUrl, "http://"))
		g_Config.m_HttpAllowInsecure = 1;

	char aEscModel[96], aEscPrompt[1152], aEscMsg[384];
	AiJsonEscape(aEscModel, sizeof(aEscModel), g_Config.m_BotAiModel[0] ? g_Config.m_BotAiModel : "llama3.2");
	AiJsonEscape(aEscPrompt, sizeof(aEscPrompt), g_Config.m_BotAiPrompt);
	AiJsonEscape(aEscMsg, sizeof(aEscMsg), Warmup ? "ping" : (m_aAiQuestion[0] ? m_aAiQuestion : "..."));

	// max_tokens keeps the answer short (faster generation), keep_alive keeps
	// the model loaded in VRAM between calls so cold-load delays only hit once.
	char aJson[1792];
	str_format(aJson, sizeof(aJson),
		"{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"%s\"}],"
		"\"max_tokens\":120,\"keep_alive\":\"30m\",\"options\":{\"temperature\":0.7}}",
		aEscModel, aEscPrompt, aEscMsg);

	auto pReq = HttpPostJson(g_Config.m_BotAiUrl, aJson);
	pReq->FailOnErrorStatus(false);
	// Ollama can take a while to load the model into VRAM (first request can be
	// ~10s+ on a slow GPU), so give it a generous timeout instead of the default.
	pReq->Timeout(CTimeout{5000, 120000, 500, 120});
	pReq->LogProgress(HTTPLOG::NONE);
	if(g_Config.m_BotAiKey[0])
	{
		char aAuth[200];
		str_format(aAuth, sizeof(aAuth), "Bearer %s", g_Config.m_BotAiKey);
		pReq->HeaderString("Authorization", aAuth);
	}
	SetAiStatus(Warmup ? "ИИ: прогрев модели..." : "ИИ: запрос отправлен ...");
	m_AiWarming = Warmup;
	m_pAiRequest = std::move(pReq);
	Http()->Run(m_pAiRequest);
}

void CBotSymona::SendWarmupRequest()
{
	// Warm the model up right after connecting so the first real chat reply
	// doesn't have to wait for the (slow) VRAM load on first use.
	if(g_Config.m_BotAiUrl[0] == '\0' || m_pAiRequest)
		return;
	SendAiRequest(true);
}

void CBotSymona::PollAi()
{
	if(!m_pAiRequest)
		return;
	if(!m_pAiRequest->Done())
		return;

	auto pReq = m_pAiRequest;
	m_pAiRequest = nullptr;

	if(pReq->State() != EHttpState::DONE)
	{
		m_AiWarming = false;
		SetAiStatus("ИИ: сеть недоступна");
		return;
	}

	json_value *pRoot = pReq->ResultJson();
	const int Code = pReq->StatusCode();
	{
		unsigned char *pBody = nullptr;
		size_t BodyLen = 0;
		if(pReq->State() == EHttpState::DONE)
			pReq->Result(&pBody, &BodyLen);
		char aBody[200] = "";
		if(pBody && BodyLen > 0)
		{
			size_t Clamp = minimum(BodyLen, (size_t)sizeof(aBody) - 1);
			mem_copy(aBody, pBody, Clamp);
			aBody[Clamp] = '\0';
		}
		log_info("botsymona", "AI poll: code=%d body_len=%d body='%s'", Code, (int)BodyLen, aBody);
	}
	if(!pRoot)
	{
		m_AiWarming = false;
		SetAiStatus("ИИ: пустой ответ");
		return;
	}
	if(Code >= 400)
	{
		const json_value *pErr = json_object_get(pRoot, "error");
		const json_value *pMsg = json_object_get(pRoot, "message");
		if(pErr && pErr->type == json_string && pErr->u.string.ptr[0])
			SetAiStatus("ИИ: ошибка %d: %s", Code, pErr->u.string.ptr);
		else if(pMsg && pMsg->type == json_string && pMsg->u.string.ptr[0])
			SetAiStatus("ИИ: ошибка %d: %s", Code, pMsg->u.string.ptr);
		else
			SetAiStatus("ИИ: ошибка %d", Code);
		json_value_free(pRoot);
		m_AiWarming = false;
		return;
	}

	const json_value *pChoices = json_object_get(pRoot, "choices");
	const char *pContent = "";
	if(pChoices && pChoices->type == json_array && pChoices->u.array.length > 0)
	{
		const json_value *pChoice = pChoices->u.array.values[0];
		if(pChoice && pChoice->type == json_object)
		{
			const json_value *pMessage = json_object_get(pChoice, "message");
			if(pMessage && pMessage->type == json_object)
			{
				const json_value *pC = json_object_get(pMessage, "content");
				if(pC && pC->type == json_string && pC->u.string.ptr[0])
					pContent = pC->u.string.ptr;
			}
		}
	}
	log_info("botsymona", "AI content parsed: choices=%s content_len=%d", (pChoices && pChoices->type == json_array) ? "yes" : "no", (int)str_length(pContent));

	if(pContent[0] == '\0')
	{
		m_AiWarming = false;
		SetAiStatus("ИИ: пустой ответ");
		json_value_free(pRoot);
		return;
	}

	if(m_AiWarming)
	{
		// Warmup request: model is now loaded, don't post "pong" to chat.
		m_AiWarming = false;
		m_AiWarmed = true;
		SetAiStatus("ИИ: модель прогрета, готова к работе");
		json_value_free(pRoot);
		return;
	}

	// keep the reply short enough and turn newlines into spaces
	char aReply[240];
	int O = 0;
	for(const char *p = pContent; *p && O + 1 < (int)sizeof(aReply) - 56; p++)
	{
		if(*p == '\n' || *p == '\r' || *p == '\t')
		{
			if(O > 0 && aReply[O - 1] != ' ')
				aReply[O++] = ' ';
		}
		else if(!(*p == ' ' && O == 0))
			aReply[O++] = *p;
	}
	aReply[O] = '\0';
	str_utf8_truncate(aReply, sizeof(aReply), aReply, 200);

	log_info("botsymona", "AI reply to '%s' (%d bytes): '%s'", m_aAiReplyName, str_length(aReply), aReply);
	if(aReply[0] == '\0')
	{
		SetAiStatus("ИИ: пустой ответ");
		json_value_free(pRoot);
		return;
	}

	const int Team = m_AiReplyTeam;
	char aName[MAX_NAME_LENGTH];
	str_copy(aName, m_aAiReplyName, sizeof(aName));
	SetAiStatus("ИИ: сеть доступна, отвечаю");
	BotSay(Team, "@%s %s", aName[0] ? aName : "кто-то", aReply);
	json_value_free(pRoot);
	m_NextAiReply = time_get() + time_freq() * 5;
}
