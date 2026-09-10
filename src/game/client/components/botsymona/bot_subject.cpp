#include "botsymona.h"

#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <algorithm>
#include <cmath>

int CBotSymona::FindSubject(int *pMode) const
{
	*pMode = SUBJ_NONE;

	const int BotId = BotClientId();
	if(BotId < 0)
		return -1;

	vec2 BotPos, BotVel;
	if(!TeeState(BotId, &BotPos, &BotVel))
		return -1;

	const int OwnerId = FindOwner();
	const float HelpRange = (float)g_Config.m_BotHelpRange;

	int HumanId = OwnerId;
	if(HumanId < 0)
	{
		const int Local = GameClient()->m_aLocalIds[0];
		if(Local >= 0 && Local < MAX_CLIENTS && Local != BotId && GameClient()->m_aClients[Local].m_Active)
			HumanId = Local;
	}

	int BestHelp = -1;
	int BestHelpTier = 0;
	float BestHelpDist = 1e9f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == BotId || !GameClient()->m_aClients[i].m_Active)
			continue;
		if(!GameClient()->m_Teams.CanCollide(BotId, i))
			continue;

		vec2 Pos, Vel;
		if(!TeeState(i, &Pos, &Vel))
			continue;

		const float Dist = distance(BotPos, Pos);
		if(Dist > HelpRange)
			continue;

		const char *pName = GameClient()->m_aClients[i].m_aName;

		bool Deep = false;
		if(!IsFrozen(i, &Deep) || Deep)
			continue;

		const int Role = RoleOfId(i);
		int Tier = 0;
		if(i == OwnerId || i == HumanId || Role == ROLE_OWNER)
			Tier = 4;
		else if(Role == ROLE_ADMIN)
			Tier = 3;
		else if(InList(LIST_TEAM, pName))
			Tier = 2;
		else if(g_Config.m_BotHelpAll && !FreezeAt(Pos))
			Tier = 1;

		if(Tier == 0)
			continue;

		if(Tier > BestHelpTier || (Tier == BestHelpTier && Dist < BestHelpDist))
		{
			BestHelpTier = Tier;
			BestHelpDist = Dist;
			BestHelp = i;
		}
	}

	if(BestHelp >= 0 && g_Config.m_BotRescue)
	{
		*pMode = SUBJ_RESCUE;
		return BestHelp;
	}

	if(m_FollowUntilTick > Client()->PredGameTick(g_Config.m_ClDummy) &&
		m_FollowId >= 0 && m_FollowId < MAX_CLIENTS && m_FollowId != BotId &&
		GameClient()->m_aClients[m_FollowId].m_Active)
	{
		vec2 Pos, Vel;
		if(TeeState(m_FollowId, &Pos, &Vel))
		{
			*pMode = SUBJ_FOLLOW;
			return m_FollowId;
		}
	}

	if(g_Config.m_BotFollowName[0] != '\0')
	{
		char aWanted[MAX_NAME_LENGTH];
		TrimName(aWanted, sizeof(aWanted), g_Config.m_BotFollowName);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == BotId || !GameClient()->m_aClients[i].m_Active)
				continue;

			char aName[MAX_NAME_LENGTH];
			TrimName(aName, sizeof(aName), GameClient()->m_aClients[i].m_aName);
			if(str_utf8_comp_nocase(aName, aWanted) != 0)
				continue;

			vec2 Pos, Vel;
			if(!TeeState(i, &Pos, &Vel))
				continue;

			*pMode = SUBJ_FOLLOW;
			return i;
		}
	}

	return -1;
}

bool CBotSymona::PickRoamGoal(vec2 BotPos, int PredTick, vec2 *pOut) const
{
	const float Radius = (float)g_Config.m_BotRoamRadius;
	const vec2 Home = m_HomeSet ? m_HomePos : BotPos;

	vec2 aCandidates[64];
	int Count = 0;
	int Seen = 0;

	for(float dx = -Radius; dx <= Radius; dx += 32.0f)
	{
		for(int Row = -4; Row <= 4; Row++)
		{
			const vec2 Try = vec2(32.0f * std::floor((Home.x + dx) / 32.0f) + 16.0f,
				32.0f * std::floor((Home.y + 32.0f * (float)Row) / 32.0f) - 15.0f);

			if(distance(Try, Home) > Radius)
				continue;
			if(distance(Try, BotPos) < 96.0f)
				continue;
			if(!SafeStand(Try))
				continue;
			if(!CorridorClear(BotPos, Try))
				continue;

			Seen++;
			if(Count < 64)
			{
				aCandidates[Count++] = Try;
			}
			else
			{
				const unsigned R = ((unsigned)PredTick * 2654435761u + (unsigned)Seen * 40503u) % (unsigned)Seen;
				if(R < 64)
					aCandidates[R] = Try;
			}
		}
	}

	if(Count == 0)
		return false;

	const unsigned Mix = (unsigned)PredTick * 2654435761u;
	*pOut = aCandidates[Mix % (unsigned)Count];
	return true;
}
