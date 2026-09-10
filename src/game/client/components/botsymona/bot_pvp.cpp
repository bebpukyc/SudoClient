#include "botsymona.h"

#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>

void CBotSymona::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType != NETMSGTYPE_SV_CHAT)
		return;

	CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
	if(pMsg->m_ClientId < 0 || pMsg->m_ClientId >= MAX_CLIENTS)
		return;
	if(!GameClient()->m_aClients[pMsg->m_ClientId].m_Active)
		return;
	if(pMsg->m_pMessage == 0 || pMsg->m_pMessage[0] == '\0')
		return;

	// a tagged message ("botname ...") can go to the AI even if it is not a command
	if(TryAiMention(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage))
		return;

	HandleChatCommand(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
}

bool CBotSymona::HandleChatCommand(int SenderId, int Team, const char *pText)
{
	// only the owner or an admin may drive the bot
	const int Role = RoleOfId(SenderId);
	if(Role != ROLE_OWNER && Role != ROLE_ADMIN)
		return false;

	const char *pLine = str_utf8_skip_whitespaces(pText);
	if(pLine[0] != '.' && pLine[0] != '!')
		return false;
	pLine = str_utf8_skip_whitespaces(pLine + 1);

	const char *pSpace = str_find(pLine, " ");
	char aCmd[32];
	if(pSpace)
	{
		str_copy(aCmd, pLine, minimum((int)(pSpace - pLine) + 1, (int)sizeof(aCmd)));
	}
	else
	{
		str_copy(aCmd, pLine, sizeof(aCmd));
	}
	const char *pArg = pSpace ? str_utf8_skip_whitespaces(pSpace) : "";

	if(str_utf8_comp_nocase(aCmd, "info") == 0 ||
		str_utf8_comp_nocase(aCmd, "help") == 0 || aCmd[0] == '\0')
		return InfoCommand(Team);
	if(str_utf8_comp_nocase(aCmd, "team") == 0)
		return TeamCommand(Team, pArg, true);
	if(str_utf8_comp_nocase(aCmd, "unteam") == 0)
		return TeamCommand(Team, pArg, false);
	if(str_utf8_comp_nocase(aCmd, "follow") == 0)
		return FollowCommand(Team, pArg, true);
	if(str_utf8_comp_nocase(aCmd, "unfollow") == 0)
		return FollowCommand(Team, pArg, false);
	if(str_utf8_comp_nocase(aCmd, "war") == 0)
		return WarCommand(Team, pArg);
	if(str_utf8_comp_nocase(aCmd, "unwar") == 0 || str_utf8_comp_nocase(aCmd, "peace") == 0)
		return UnWarCommand(Team, pArg);

	BotSay(Team, "не знаю команду .%s, напиши .info", aCmd);
	return true;
}

bool CBotSymona::InfoCommand(int Team)
{
	char aFollow[MAX_NAME_LENGTH];
	aFollow[0] = '\0';
	if(g_Config.m_BotFollowName[0] != '\0')
		str_copy(aFollow, g_Config.m_BotFollowName, sizeof(aFollow));
	if(m_FollowId >= 0 && m_FollowId < MAX_CLIENTS && GameClient()->m_aClients[m_FollowId].m_Active)
	{
		char aName[MAX_NAME_LENGTH];
		TrimName(aName, sizeof(aName), GameClient()->m_aClients[m_FollowId].m_aName);
		str_copy(aFollow, aName, sizeof(aFollow));
	}

	BotSay(Team, "%s | свой: %d | иду за: %s | воюю c: %s",
		ConnectionText(), ListCount(LIST_TEAM),
		aFollow[0] != '\0' ? aFollow : "никто",
		m_aPvpTarget[0] != '\0' ? m_aPvpTarget : "никто");

	BotSay(Team, "команды: .info | .team ник — спасать | .unteam ник — не спасать | .follow ник — идти за | .unfollow — не идти | .war ник — воевать | .unwar — закончить войну");
	return true;
}

bool CBotSymona::TeamCommand(int Team, const char *pArg, bool Add)
{
	const char *pName = str_utf8_skip_whitespaces(pArg);
	if(pName[0] == '\0')
	{
		BotSay(Team, "напиши ник: %s ник", Add ? ".team" : ".unteam");
		return false;
	}

	char aName[MAX_NAME_LENGTH];
	TrimName(aName, sizeof(aName), pName);
	if(aName[0] == '\0')
		return false;

	if(Add)
	{
		if(!InList(LIST_TEAM, aName) && ListAdd(LIST_TEAM, aName))
		{
			BotSay(Team, "%s теперь свой — спасаю из фриза", aName);
		}
		else if(InList(LIST_TEAM, aName))
		{
			BotSay(Team, "%s уже в списке своих", aName);
		}
		else
		{
			BotSay(Team, "не смогла добавить %s", aName);
		}
	}
	else
	{
		if(ListRemove(LIST_TEAM, aName))
		{
			BotSay(Team, "%s больше не свой, не спасаю", aName);
		}
		else
		{
			BotSay(Team, "%s и так не была в списке своих", aName);
		}
	}
	return true;
}

bool CBotSymona::FollowCommand(int Team, const char *pArg, bool Do)
{
	const char *pName = str_utf8_skip_whitespaces(pArg);
	if(Do && pName[0] == '\0')
	{
		BotSay(Team, "напиши ник: .follow ник");
		return false;
	}

	if(!Do)
	{
		m_FollowId = -1;
		m_FollowUntilTick = -1;
		g_Config.m_BotFollowName[0] = '\0';
		m_Paused = false;
		BotSay(Team, "больше ни за кем не хожу");
		return true;
	}

	char aName[MAX_NAME_LENGTH];
	TrimName(aName, sizeof(aName), pName);
	if(aName[0] == '\0')
		return false;

	const int Id = FindPlayerByName(aName);
	if(Id < 0)
	{
		BotSay(Team, "игрок \"%s\" не на сервере", aName);
		return false;
	}

	m_FollowId = Id;
	m_FollowUntilTick = Client()->PredGameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 3600;
	str_copy(g_Config.m_BotFollowName, aName);
	m_Paused = false;
	BotSay(Team, "иду за %s", aName);
	return true;
}

bool CBotSymona::WarCommand(int Team, const char *pArg)
{
	const char *pName = str_utf8_skip_whitespaces(pArg);
	if(pName[0] == '\0')
	{
		BotSay(Team, "напиши ник: .war ник");
		return false;
	}

	char aName[MAX_NAME_LENGTH];
	TrimName(aName, sizeof(aName), pName);
	if(aName[0] == '\0')
		return false;

	const int Id = FindPlayerByName(aName);
	if(Id < 0)
	{
		BotSay(Team, "игрок \"%s\" не на сервере", aName);
		return false;
	}

	str_copy(m_aPvpTarget, aName);
	m_PvpTargetTick = PredTickNow();
	g_Config.m_BotPvp = 1;
	BotSay(Team, "воевать с %s — буду бить молотком во фриз", aName);
	SetStatus("объявляю войну %s", aName);
	return true;
}

bool CBotSymona::UnWarCommand(int Team, const char *pArg)
{
	m_aPvpTarget[0] = '\0';
	g_Config.m_BotPvp = 0;
	m_PvpEnemyId = -1;
	m_PvpSwitchTick = -1;
	BotSay(Team, "война окончена");
	SetStatus("война окончена");
	return true;
}

void CBotSymona::BotSay(int Team, const char *pFormat, ...)
{
	char aBuf[256];
	va_list Args;
	va_start(Args, pFormat);
	str_format_v(aBuf, sizeof(aBuf), pFormat, Args);
	va_end(Args);

	if(!BotOnServer())
		return;

	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = aBuf;
	Client()->SendPackMsg(BotConn(), &Msg, MSGFLAG_VITAL);
}

int CBotSymona::FindPlayerByName(const char *pName) const
{
	if(!pName || pName[0] == '\0')
		return -1;

	char aWanted[MAX_NAME_LENGTH];
	TrimName(aWanted, sizeof(aWanted), pName);
	if(aWanted[0] == '\0')
		return -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;
		char aName[MAX_NAME_LENGTH];
		TrimName(aName, sizeof(aName), GameClient()->m_aClients[i].m_aName);
		if(str_utf8_comp_nocase(aName, aWanted) == 0)
			return i;
	}
	return -1;
}


bool CBotSymona::TryPvP(int PredTick, int BotId, vec2 BotPos, vec2 BotVel, CNetObj_PlayerInput *pInput)
{
	const bool Forced = m_aPvpTarget[0] != '\0' && PredTick - m_PvpTargetTick < Client()->GameTickSpeed() * 300;
	if(!g_Config.m_BotPvp && !Forced)
		return false;

	bool BotDeep = false;
	if(IsFrozen(BotId, &BotDeep))
		return false;

	vec2 EnemyPos, EnemyVel;
	const int EnemyId = FindWarEnemy(&EnemyPos, &EnemyVel);
	if(EnemyId < 0)
	{
		m_PvpEnemyId = -1;
		m_PvpSwitchTick = -1;
		return false;
	}

	if(EnemyId != m_PvpEnemyId)
	{
		m_PvpEnemyId = EnemyId;
		m_PvpSwitchTick = PredTick;
		m_PvpStrafeTick = PredTick;
		m_AimPoint = vec2(1.0f, 0.0f);
		DropPlan();
	}

	if(g_Config.m_BotWarExecute && PredTick >= m_NoKillShotUntil &&
		WarKillFrozen(PredTick, BotId, EnemyId, BotPos, EnemyPos, pInput))
	{
		return true;
	}

	return WarHandle(PredTick, BotId, BotPos, BotVel, EnemyId, EnemyPos, EnemyVel, pInput);
}

int CBotSymona::FindWarEnemy(vec2 *pPos, vec2 *pVel) const
{
	const int BotId = BotClientId();
	if(BotId < 0)
		return -1;

	vec2 BotPos, BotVel;
	if(!TeeState(BotId, &BotPos, &BotVel))
		return -1;

	const float Range = (float)g_Config.m_BotWarRange;
	const int OwnerId = FindOwner();

	int Best = -1;
	float BestDist = 1e9f;
	int ForcedId = -1;
	int ForcedCount = 0;

	if(m_aPvpTarget[0] != '\0')
	{
		char aWanted[MAX_NAME_LENGTH];
		TrimName(aWanted, sizeof(aWanted), m_aPvpTarget);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == BotId || !GameClient()->m_aClients[i].m_Active)
				continue;
			char aName[MAX_NAME_LENGTH];
			TrimName(aName, sizeof(aName), GameClient()->m_aClients[i].m_aName);
			if(str_utf8_comp_nocase(aName, aWanted) == 0)
			{
				ForcedId = i;
				ForcedCount++;
			}
		}
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == BotId || !GameClient()->m_aClients[i].m_Active)
			continue;
		if(ForcedCount > 0 && i != ForcedId)
			continue;
		if(i == OwnerId || RoleOfId(i) == ROLE_ADMIN)
			continue;
		if(!GameClient()->m_Teams.CanCollide(BotId, i))
			continue;

		if(!g_Config.m_BotWarAll)
		{
			if(!InList(LIST_WAR, GameClient()->m_aClients[i].m_aName))
				continue;
		}
		else if(InList(LIST_TEAM, GameClient()->m_aClients[i].m_aName))
		{
			continue;
		}

		vec2 Pos, Vel;
		if(!TeeState(i, &Pos, &Vel))
			continue;

		const float Dist = distance(BotPos, Pos);
		if(ForcedCount == 0 && Dist > Range)
			continue;

		if(Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}

	if(Best >= 0)
	{
		TeeState(Best, pPos, pVel);
	}
	return Best;
}

int CBotSymona::PickWarWeapon(float Dist) const
{
	const int Forced = g_Config.m_BotWarWeapon;
	if(Forced != 0)
	{
		static const int s_aMap[4] = {WEAPON_GUN, WEAPON_LASER, WEAPON_GRENADE, WEAPON_SHOTGUN};
		const int W = (Forced >= 1 && Forced <= 4) ? s_aMap[Forced - 1] : WEAPON_GUN;
		return HasWeapon(W) ? W : -1;
	}

	if(Dist <= 150.0f && HasWeapon(WEAPON_SHOTGUN))
		return WEAPON_SHOTGUN;
	if(HasWeapon(WEAPON_GUN))
		return WEAPON_GUN;
	if(Dist <= 420.0f && HasWeapon(WEAPON_GRENADE))
		return WEAPON_GRENADE;
	if(HasWeapon(WEAPON_LASER))
		return WEAPON_LASER;
	if(HasWeapon(WEAPON_SHOTGUN))
		return WEAPON_SHOTGUN;
	if(HasWeapon(WEAPON_GRENADE))
		return WEAPON_GRENADE;
	return -1;
}

int CBotSymona::WarLead(int Weapon, float Dist) const
{
	float PerTick = 1.0f;
	switch(Weapon)
	{
	case WEAPON_GUN: PerTick = 6.0f; break;
	case WEAPON_SHOTGUN: PerTick = 7.0f; break;
	case WEAPON_GRENADE: PerTick = 3.0f; break;
	case WEAPON_LASER:
	default: return 0;
	}
	return std::clamp((int)std::ceil(Dist / PerTick), 0, 24);
}

bool CBotSymona::WarCounterHook(vec2 BotPos, int BotId, int EnemyId, vec2 EnemyPos, CNetObj_PlayerInput *pInput)
{
	if(!g_Config.m_BotUseHook)
		return false;

	const CNetObj_Character *pEChar = SnapChar(EnemyId);
	if(!pEChar)
		return false;
	if(pEChar->m_HookState != HOOK_GRABBED || pEChar->m_HookedPlayer != BotId)
		return false;

	vec2 AimPos;
	if(!HookAimPoint(BotPos, EnemyPos, BotId, EnemyId, &AimPos))
		return false;

	pInput->m_TargetX = round_to_int(AimPos.x - BotPos.x);
	pInput->m_TargetY = round_to_int(AimPos.y - BotPos.y);
	if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
		pInput->m_TargetX = (EnemyPos.x < BotPos.x) ? -1 : 1;

	pInput->m_Hook = 1;
	m_AimPoint = vec2((float)pInput->m_TargetX, (float)pInput->m_TargetY);
	return true;
}

bool CBotSymona::WarKillFrozen(int PredTick, int BotId, int EnemyId, vec2 BotPos, vec2 EnemyPos, CNetObj_PlayerInput *pInput)
{
	bool EnemyDeep = false;
	if(!IsFrozen(EnemyId, &EnemyDeep) || EnemyDeep)
		return false;
	if(!HasWeapon(WEAPON_SHOTGUN))
		return false;
	if(distance(BotPos, EnemyPos) > 620.0f)
		return false;

	for(int dy = -1; dy <= 1; dy++)
	{
		for(int dx = -1; dx <= 1; dx++)
		{
			if(dx == 0 && dy == 0)
				continue;
			const vec2 Tile = vec2(32.0f * ((int)std::floor(EnemyPos.x / 32.0f) + dx) + 16.0f,
				32.0f * ((int)std::floor(EnemyPos.y / 32.0f) + dy) + 16.0f);
			if(!FreezeAt(Tile))
				continue;
			if(distance(EnemyPos, Tile) > 70.0f)
				continue;

			const vec2 Aim = Tile - EnemyPos;
			if(length(Aim) < 1.0f)
				continue;

			vec2 TargetPos = EnemyPos + normalize(Aim) * 60.0f;
			if(!ShotLineClear(BotPos, TargetPos, BotId, EnemyId, CCharacterCore::PhysicalSize()))
				continue;

			if(WarShot(pInput, WEAPON_SHOTGUN, BotPos, PredTick, TargetPos, BotId, EnemyId))
			{
				m_NoKillShotUntil = PredTick + 12;
				SetStatus("добивает дробовиком во фриз");
				return true;
			}
			return true;
		}
	}
	return false;
}

bool CBotSymona::WarShot(CNetObj_PlayerInput *pInput, int Weapon, vec2 BotPos, int PredTick, vec2 TargetPos, int BotId, int EnemyId)
{
	pInput->m_WantedWeapon = Weapon + 1;

	const vec2 Delta = TargetPos - BotPos;
	pInput->m_TargetX = round_to_int(Delta.x);
	pInput->m_TargetY = round_to_int(Delta.y);
	if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
		pInput->m_TargetX = (TargetPos.x < BotPos.x) ? -1 : 1;

	if(!WeaponReady(Weapon, PredTick))
		return false;
	if(m_LastShotTick >= 0 && PredTick - m_LastShotTick < 3)
		return false;

	pInput->m_Fire = (pInput->m_Fire + 2) & INPUT_STATE_MASK;
	m_LastShotTick = PredTick;
	m_AimPoint = vec2((float)pInput->m_TargetX, (float)pInput->m_TargetY);
	return true;
}

bool CBotSymona::WarHandle(int PredTick, int BotId, vec2 BotPos, vec2 BotVel, int EnemyId, vec2 EnemyPos, vec2 EnemyVel, CNetObj_PlayerInput *pInput)
{
	const float Range = (float)g_Config.m_BotWarRange;
	const vec2 Delta = EnemyPos - BotPos;
	const float Dist = length(Delta);

	// never fight right next to an ally or the owner
	const int OwnerId = FindOwner();
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;
		if(i == BotId || i == EnemyId)
			continue;
		if(i != OwnerId && !InList(LIST_TEAM, GameClient()->m_aClients[i].m_aName))
			continue;

		vec2 Pos, Vel;
		if(!TeeState(i, &Pos, &Vel))
			continue;
		if(distance(BotPos, Pos) < 90.0f)
		{
			SetStatus("не дерётся рядом со своими");
			return false;
		}
	}

	// give up the chase if the enemy got away (unless the owner war-wanted them)
	const bool Forced = m_aPvpTarget[0] != '\0';
	if(!Forced && Dist > Range + 160.0f)
	{
		SetStatus("враг ушёл из зоны войны (%d px)", (int)Dist);
		return false;
	}

	if(Dist > Range + 2000.0f)
	{
		SetStatus("цель слишком далеко, жду (%d px)", (int)Dist);
		return false;
	}

	CCharacterCore BotCore;
	if(!CoreFromSnap(BotId, &BotCore))
		return false;

	bool BotDeep = false;
	if(IsFrozen(BotId, &BotDeep))
		return false;

	const CNetObj_Character *pBotChar = SnapChar(BotId);
	if(!pBotChar)
		return false;

	const int Weapon = PickWarWeapon(Dist);
	const bool Grounded = Collision()->IsOnGround(BotPos, CCharacterCore::PhysicalSize());

	// the enemy hooked us - throw our hook right back
	if(WarCounterHook(BotPos, BotId, EnemyId, EnemyPos, pInput))
	{
		SetStatus("встречный хук (%d px)", (int)Dist);
		m_Driving = true;
		return true;
	}

	const int HookState = pBotChar->m_HookState;
	const bool HookFlying = HookState == HOOK_FLYING;
	const bool HookReady = g_Config.m_BotUseHook && !HookFlying && pInput->m_Hook == 0;
	const float HookReach = Limits().m_HookLength;
	const bool HammerRange = HammerCanHit(BotPos, EnemyPos);
	const float Lead = WarLead(Weapon, Dist);
	vec2 TargetPos = EnemyPos + EnemyVel * (float)Lead;

	// pick a side to strafe towards
	if(PredTick >= m_PvpStrafeTick)
	{
		m_PvpStrafeTick = PredTick + 24;
		const unsigned Mix = (unsigned)PredTick * 2654435761u;
		m_PvpStrafe = vec2((Mix % 3 == 0) ? -1.0f : ((Mix % 3 == 1) ? 1.0f : 0.0f),
			(Mix % 2 == 0) ? -0.5f : 0.5f);
	}

	// chase the enemy relentlessly
	vec2 Goal = EnemyPos;
	if(distance(BotPos, Goal) > 26.0f)
	{
		Goal += m_PvpStrafe * 20.0f;
		Steer(BotCore, Goal, PredTick, pInput);
	}
	else if(!HammerRange)
	{
		pInput->m_Direction = (m_PvpStrafe.x > 0.0f) ? 1 : 0;
		if(Grounded)
			pInput->m_Jump = (PredTick % 40 > 20) ? 1 : 0;
	}

	// hammer the target point-blank, bouncing it toward freeze
	if(HammerRange)
	{
		pInput->m_WantedWeapon = WEAPON_HAMMER + 1;

		vec2 AimDir = normalize(EnemyVel) * 0.3f + normalize(Delta) * 0.7f;
		for(int dy = -2; dy <= 2; dy++)
		{
			const vec2 Tile = vec2(32.0f * ((int)std::floor(BotPos.x / 32.0f) + (Delta.x > 0.0f ? 1 : -1)) + 16.0f,
				32.0f * ((int)std::floor(EnemyPos.y / 32.0f) + dy) + 16.0f);
			if(FreezeAt(Tile))
			{
				const vec2 ToFreeze = Tile - EnemyPos;
				if(length(ToFreeze) > 1.0f)
					AimDir = normalize(normalize(ToFreeze) * 0.8f + normalize(EnemyVel) * 0.2f);
				break;
			}
		}
		if(length(AimDir) < 1.0f)
			AimDir = normalize(Delta);

		pInput->m_TargetX = round_to_int(AimDir.x * 200.0f);
		pInput->m_TargetY = round_to_int(AimDir.y * 200.0f);
		if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
			pInput->m_TargetX = (EnemyPos.x < BotPos.x) ? -1 : 1;

		if(pBotChar->m_Weapon == WEAPON_HAMMER && WeaponReady(WEAPON_HAMMER, PredTick) &&
			(m_LastShotTick < 0 || PredTick - m_LastShotTick >= 10))
		{
			pInput->m_Fire = (pInput->m_Fire + 2) & INPUT_STATE_MASK;
			m_LastShotTick = PredTick;
			SetStatus("бьёт молотом по врагу (%d px)", (int)Dist);
		}
		else
		{
			SetStatus("молот: подносит удар (%d px)", (int)Dist);
		}
	}
	// hook the target to lock it / drag it back into range
	else if(HookReady && Dist <= HookReach + 40.0f)
	{
		vec2 AimPos;
		if(HookAimPoint(BotPos, EnemyPos, BotId, EnemyId, &AimPos))
		{
			pInput->m_TargetX = round_to_int(AimPos.x - BotPos.x);
			pInput->m_TargetY = round_to_int(AimPos.y - BotPos.y);
			if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
				pInput->m_TargetX = (EnemyPos.x < BotPos.x) ? -1 : 1;
			pInput->m_Hook = 1;
			SetStatus("хукает врага (%d px)", (int)Dist);
		}
	}
	// ranged: shoot with lead
	if(!HammerRange)
	{
		if(!ShotLineClear(BotPos, TargetPos, BotId, EnemyId, CCharacterCore::PhysicalSize()))
		{
			if(Grounded)
				pInput->m_Jump = 1;
			TargetPos = EnemyPos;
		}

		if(Weapon >= 0)
		{
			const bool Fired = WarShot(pInput, Weapon, BotPos, PredTick, TargetPos, BotId, EnemyId);
			if(Fired)
				SetStatus("война: бьёт по врагу (%d px)", (int)Dist);
			else if(pBotChar->m_Weapon != Weapon)
				SetStatus("война: берёт оружие (%d px)", (int)Dist);
			else
				SetStatus("война: целится (%d px)", (int)Dist);
		}
		else
		{
			SetStatus("война: нет оружия, ждёт (%d px)", (int)Dist);
		}
	}

	m_Driving = true;
	return true;
}