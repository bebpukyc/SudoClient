#include "botsymona.h"

#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <algorithm>
#include <cmath>

bool CBotSymona::PlanClimb(const CCharacterCore &Start, vec2 OwnerPos, int PredTick, vec2 *pGrab, vec2 *pLand, int *pHold, bool *pAirJump) const
{
	static constexpr int FAN = 48;
	static const int s_aHold[5] = {18, 30, 46, 68, 100};

	const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
	const float MinSlope = (float)Tuning.m_Gravity / std::max(0.01f, (float)Tuning.m_HookDragAccel);

	const vec2 BotPos = Start.m_Pos;
	const float OwnerFloorY = 32.0f * std::floor((OwnerPos.y + 15.0f) / 32.0f);

	float BestScore = -1e9f;
	bool Found = false;

	for(int i = 0; i < FAN; i++)
	{
		const float Angle = -3.14159265f + (3.14159265f * ((float)i + 0.5f) / (float)FAN);
		const vec2 Dir = vec2(std::cos(Angle), std::sin(Angle));

		vec2 Grab;
		if(!HookProbe(BotPos, Dir, &Grab))
			continue;
		if(Grab.y > OwnerFloorY + 46.0f)
			continue;
		if(Grab.y > BotPos.y - 32.0f)
			continue;
		if(IsRejected(Grab, PredTick))
			continue;

		const vec2 Rope = Grab - BotPos;
		const float RopeLen = length(Rope);
		if(RopeLen < 64.0f || Rope.y / RopeLen > -MinSlope)
			continue;

		vec2 Land;
		bool HaveLand = false;
		float BestLandDx = 1e9f;
		for(float dx = -160.0f; dx <= 160.0f; dx += 32.0f)
		{
			for(int Step = 0; Step <= 3; Step++)
			{
				const vec2 Try = vec2(32.0f * std::floor((OwnerPos.x + dx) / 32.0f) + 16.0f,
					32.0f * std::floor((OwnerPos.y + 15.0f + 32.0f * (float)Step) / 32.0f) - 15.0f);
				if(!SafeStand(Try))
					continue;
				const float Dx = absolute(Try.x - Grab.x);
				if(Dx < BestLandDx)
				{
					BestLandDx = Dx;
					Land = Try;
					HaveLand = true;
				}
				break;
			}
		}
		if(!HaveLand || BestLandDx > 64.0f)
			continue;

		const int WalkDir = Grab.x > BotPos.x ? 1 : -1;

		for(int h = 0; h < 5; h++)
		{
			for(int j = 0; j < 2; j++)
			{
				const int Hold = s_aHold[h];
				const bool AirJump = j == 1;

				SAction Action;
				Action.m_PreDir = WalkDir;
				Action.m_PreTicks = Hold;
				Action.m_Dir = Land.x > Grab.x ? 1 : -1;
				Action.m_HookFrom = 2;
				Action.m_HookUntil = Hold;
				Action.m_AirJumpAt = AirJump ? Hold + 2 : -1;
				Action.m_Aim = Rope;

				const SSimResult Res = Simulate(Start, Action, Land, Hold + 60);
				if(Res.m_Frozen || Res.m_Airborne)
					continue;
				if(Res.m_EndPos.y > OwnerFloorY - 8.0f)
					continue;
				if(Res.m_MinGoalDist > 24.0f || Res.m_EndGoalDist > 48.0f)
					continue;

				const float Score = -Res.m_EndGoalDist - 0.2f * RopeLen - 0.25f * (float)Hold - (AirJump ? 10.0f : 0.0f);
				if(Score <= BestScore)
					continue;

				BestScore = Score;
				*pGrab = Grab;
				*pLand = Land;
				*pHold = Hold;
				*pAirJump = AirJump;
				Found = true;
			}
		}
	}
	return Found;
}

bool CBotSymona::FreezeAhead(vec2 Pos, vec2 Vel, vec2 Pull, int Ticks) const
{
	const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
	vec2 Probe = Pos;
	vec2 V = Vel;

	for(int t = 0; t < Ticks; t++)
	{
		V.y += (float)Tuning.m_Gravity;
		V += Pull;
		const vec2 Next = Probe + V;
		if(SweepFreeze(Probe, Next))
			return true;
		Probe = Next;
	}
	return false;
}

bool CBotSymona::PlanLaunch(const CCharacterCore &Start, vec2 Goal, int PredTick, SLaunch *pOut) const
{
	static constexpr int FAN = 64;
	static constexpr int TOP_GRABS = 12;
	static const int s_aHold[7] = {8, 12, 18, 26, 34, 44, 56};

	const vec2 BotPos = Start.m_Pos;
	const int GoalDir = Goal.x < BotPos.x ? -1 : 1;
	const float StartDist = distance(BotPos, Goal);

	m_vLaunchGrabs.clear();
	m_vLaunchGrabs.reserve(64);

	for(int i = 0; i < FAN; i++)
	{
		const float Angle = -3.14159265f + (3.14159265f * ((float)i + 0.5f) / (float)FAN);
		const vec2 Dir = vec2(std::cos(Angle), std::sin(Angle));

		vec2 Grab;
		if(!HookProbe(BotPos, Dir, &Grab))
			continue;
		if(IsRejected(Grab, PredTick))
			continue;
		if(Grab.y > BotPos.y - 32.0f)
			continue;

		const float Rank = (BotPos.y - Grab.y) - 0.5f * absolute(Grab.x - Goal.x);
		m_vLaunchGrabs.push_back({Rank, Grab});
	}

	std::sort(m_vLaunchGrabs.begin(), m_vLaunchGrabs.end(), [](const SGrab &A, const SGrab &B) {
		return A.m_Rank > B.m_Rank;
	});

	float BestScore = -1e9f;
	bool Found = false;

	const int Limit = std::min((int)m_vLaunchGrabs.size(), TOP_GRABS);
	for(int g = 0; g < Limit; g++)
	{
		const vec2 Grab = m_vLaunchGrabs[g].m_Pos;
		for(int h = 0; h < 7; h++)
		{
			for(int d = 0; d < 2; d++)
			{
				SAction Action;
				Action.m_PreDir = d == 0 ? GoalDir : 0;
				Action.m_PreTicks = s_aHold[h] + 20;
				Action.m_Dir = 0;
				Action.m_HookFrom = 2;
				Action.m_HookUntil = 1 + s_aHold[h];
				Action.m_Aim = Grab - BotPos;

				const SSimResult Res = Simulate(Start, Action, Goal, 150);
				if(Res.m_Frozen || Res.m_Airborne)
					continue;
				if(!SafeStand(Res.m_EndPos))
					continue;
				if(Res.m_EndGoalDist > StartDist - 32.0f && Res.m_EndPos.y > Start.m_Pos.y - 32.0f)
					continue;

				const float Score = -Res.m_EndGoalDist - 0.5f * (float)s_aHold[h];
				if(Score <= BestScore)
					continue;

				BestScore = Score;
				pOut->m_Grab = Grab;
				pOut->m_Land = Res.m_EndPos;
				pOut->m_Hold = s_aHold[h];
				pOut->m_Dir = Action.m_PreDir;
				Found = true;
			}
		}
	}
	return Found;
}
