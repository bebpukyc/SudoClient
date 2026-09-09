#include "botsymona.h"

#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>
#include <algorithm>
#include <cmath>

bool CBotSymona::HammerCanHit(vec2 From, vec2 Target) const
{
	const float Radius = CCharacterCore::PhysicalSize();

	vec2 Dir = Target - From;
	if(length(Dir) < 0.001f)
		Dir = vec2(1.0f, 0.0f);
	else
		Dir = normalize(Dir);

	const vec2 Muzzle = From + Dir * (Radius * 0.75f);
	return distance(Muzzle, Target) < Radius * 1.5f - 1.0f;
}

bool CBotSymona::PullSafe(vec2 From, vec2 OwnerPos) const
{
	if(!g_Config.m_BotSafePull)
		return true;

	if(FreezeAt(From) || FreezeAt(vec2(From.x, From.y - 16.0f)))
		return false;

	const float Len = distance(OwnerPos, From);
	if(Len < 1.0f)
		return true;

	const vec2 Dir = (From - OwnerPos) / Len;

	vec2 Over = From + Dir * 64.0f;
	vec2 Collide, Before;
	if(Collision()->IntersectLine(From, Over, &Collide, &Before))
		Over = Before;

	return !SweepFreeze(From, Over);
}

bool CBotSymona::HookAimPoint(vec2 From, vec2 OwnerPos, int BotId, int OwnerId, vec2 *pAim) const
{
	static const float s_aSide[7] = {0.0f, 11.0f, -11.0f, 18.0f, -18.0f, 25.0f, -25.0f};
	static const float s_aAlong[4] = {0.0f, -16.0f, -30.0f, 24.0f};

	const float Grab = CCharacterCore::PhysicalSize() + 2.0f;
	const float MaxLen = Limits().m_HookLength;

	const float Span = distance(From, OwnerPos);
	if(Span > MaxLen)
		return false;

	vec2 Ray = OwnerPos - From;
	if(Span < 1.0f)
		Ray = vec2(1.0f, 0.0f);
	else
		Ray = normalize(Ray);
	const vec2 Side = vec2(-Ray.y, Ray.x);

	const float Reach = std::min(MaxLen, Span + 48.0f);

	for(int a = 0; a < 4; a++)
	for(int s = 0; s < 7; s++)
	{
		const vec2 Target = OwnerPos + Side * s_aSide[s] + Ray * s_aAlong[a];

		vec2 Dir = Target - From;
		if(length(Dir) < 1.0f)
			continue;
		Dir = normalize(Dir);

		vec2 Collide, Before;
		int TeleNr = 0;
		const vec2 End = From + Dir * Reach;
		const int Hit = Collision()->IntersectLineTeleHook(From, End, &Collide, &Before, &TeleNr);
		const vec2 Stop = Hit != 0 ? Collide : End;

		vec2 Closest;
		if(!closest_point_on_line(From, Stop, OwnerPos, Closest))
			continue;
		if(distance(OwnerPos, Closest) >= Grab)
			continue;

		if(BotId >= 0)
		{
			const int First = FirstTeeOnRay(From, Stop, BotId);
			if(First >= 0 && First != OwnerId)
				continue;
		}

		if(pAim)
			*pAim = Target;
		return true;
	}
	return false;
}

int CBotSymona::FirstTeeOnRay(vec2 From, vec2 To, int SelfId) const
{
	const float Radius = CCharacterCore::PhysicalSize() + 2.0f;

	int Best = -1;
	float BestDist = 1e9f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == SelfId || !GameClient()->m_aClients[i].m_Active)
			continue;
		if(!GameClient()->m_Teams.CanCollide(SelfId, i))
			continue;

		vec2 Pos, Vel;
		if(!TeeState(i, &Pos, &Vel))
			continue;

		vec2 Closest;

		if(!closest_point_on_line(From, To, Pos, Closest))
		continue;

		if(distance(Pos, Closest) >= Radius)
			continue;

		const float D = distance(From, Pos);
		if(D < BestDist)
		{
			BestDist = D;
			Best = i;
		}
	}
	return Best;
}

int CBotSymona::BlockerAhead(vec2 From, vec2 To, int SelfId) const
{
	const float Radius = CCharacterCore::PhysicalSize() * 1.6f;
	const float Span = distance(From, To);

	int Best = -1;
	float BestDist = 1e9f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == SelfId || !GameClient()->m_aClients[i].m_Active)
			continue;
		if(!GameClient()->m_Teams.CanCollide(SelfId, i))
			continue;

		vec2 Pos, Vel;
		if(!TeeState(i, &Pos, &Vel))
			continue;

		vec2 Closest;

		if(!closest_point_on_line(From, To, Pos, Closest))
		continue;

		if(distance(Pos, Closest) >= Radius)
			continue;

		const float D = distance(From, Pos);
		if(D >= Span || D >= BestDist)
			continue;

		BestDist = D;
		Best = i;
	}
	return Best;
}

bool CBotSymona::HookPathToOwner(vec2 From, vec2 OwnerPos, int BotId, int OwnerId, vec2 *pAim) const
{
	return HookAimPoint(From, OwnerPos, BotId, OwnerId, pAim);
}

int CBotSymona::HookOutcome(int PredTick, int HookState, int HookedPlayer, int OwnerId) const
{
	const int Lag = std::clamp(PredTick - Client()->GameTick(BotConn()), 0, 20);
	const int Settle = m_UsingPrediction ? 1 : Lag + 2;
	if(m_HookFiredTick < 0 || PredTick < m_HookFiredTick + Settle)
		return HK_PENDING;

	switch(HookState)
	{
	case HOOK_GRABBED:
		return HookedPlayer == OwnerId ? HK_OWNER : HK_TERRAIN;
	case HOOK_FLYING:
		return HK_FLYING;
	case HOOK_RETRACTED:
	case HOOK_IDLE:
	case HOOK_RETRACT_START:
	case HOOK_RETRACT_END:
	case 2:
		return HK_MISS;
	}
	return HK_FLYING;
}

bool CBotSymona::FindAnchor(vec2 OwnerPos, vec2 BotPos, float Reach, int PredTick, vec2 *pOut) const
{
	static constexpr float MIN_D = 150.0f;
	static constexpr int MAX_RAYS = 16;

	const float MaxD = std::max(MIN_D + 32.0f, Reach - 30.0f);

	m_vAnchorCandidates.clear();
	m_vAnchorCandidates.reserve(256);

	for(int Sign = -1; Sign <= 1; Sign += 2)
	{
		for(float dx = MIN_D; dx <= MaxD; dx += 32.0f)
		{
			for(float dy = -MaxD; dy <= MaxD; dy += 32.0f)
			{
				const float RawY = OwnerPos.y + dy;
				vec2 P = vec2(OwnerPos.x + Sign * dx, RawY);

				bool Stands = false;
				for(int Step = 0; Step <= 3 && !Stands; Step++)
				{
					P.y = 32.0f * std::floor((RawY + 32.0f * Step) / 32.0f) - 15.0f;
					Stands = SafeStand(P);
				}
				if(!Stands)
					continue;

				const float D = distance(P, OwnerPos);
				if(D < MIN_D || D > MaxD)
					continue;
				if(IsRejected(P, PredTick))
					continue;
				if(!PullSafe(P, OwnerPos))
					continue;
				if((P.x - OwnerPos.x) * (BotPos.x - OwnerPos.x) < 0.0f)
					continue;

				m_vAnchorCandidates.push_back({-AnchorEta(P, OwnerPos, BotPos, Limits()), P});
			}
		}
	}

	std::sort(m_vAnchorCandidates.begin(), m_vAnchorCandidates.end(), [](const SCandidate &A, const SCandidate &B) {
		return A.m_Score > B.m_Score;
	});

	int Rays = 0;
	for(const SCandidate &Cand : m_vAnchorCandidates)
	{
		if(Rays >= MAX_RAYS)
			break;
		if(!AnchorReachable(BotPos, Cand.m_Pos))
			continue;
		Rays++;

		const vec2 Aim = OwnerPos - Cand.m_Pos;
		if(length(Aim) < 1.0f)
			continue;
		if(!HookAimPoint(Cand.m_Pos + normalize(Aim) * 42.0f, OwnerPos, -1, -1, nullptr))
			continue;

		*pOut = Cand.m_Pos;
		return true;
	}
	return false;
}

bool CBotSymona::AnchorReachable(vec2 BotPos, vec2 P) const
{
	const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
	const float G = std::max(0.05f, (float)Tuning.m_Gravity);
	const float GroundJump = Tuning.m_GroundJumpImpulse;
	const float AirJump = Tuning.m_AirJumpImpulse;

	const float Rise = BotPos.y - P.y;
	const float SingleApex = GroundJump * GroundJump / (2.0f * G) - 16.0f;
	const float DoubleApex = SingleApex + AirJump * AirJump / (2.0f * G);

	if(Rise > DoubleApex)
		return false;

	if(absolute(Rise) <= 20.0f)
		return CorridorClear(BotPos, P);

	if(Rise > 0.0f)
	{
		const vec2 Across = vec2(P.x, BotPos.y);
		const vec2 Up = vec2(BotPos.x, P.y - 20.0f);
		return (CorridorClear(BotPos, Across) && CorridorClear(Across, P)) ||
		       (CorridorClear(BotPos, Up) && CorridorClear(Up, P));
	}

	const vec2 Across = vec2(P.x, BotPos.y);
	const vec2 Down = vec2(BotPos.x, P.y);
	return (CorridorClear(BotPos, Across) && CorridorClear(Across, P)) ||
	       (CorridorClear(BotPos, Down) && CorridorClear(Down, P));
}

bool CBotSymona::IsRejected(vec2 P, int PredTick) const
{
	for(const SRejected &R : m_aRejected)
	{
		if(R.m_Tick <= 0 || PredTick - R.m_Tick > 250)
			continue;
		if(distance(P, R.m_Pos) < 40.0f)
			return true;
	}
	return false;
}

void CBotSymona::Reject(vec2 P, int PredTick)
{
	m_aRejected[m_RejectedNext].m_Pos = P;
	m_aRejected[m_RejectedNext].m_Tick = std::max(1, PredTick);
	m_RejectedNext = (m_RejectedNext + 1) % (int)(sizeof(m_aRejected) / sizeof(m_aRejected[0]));
}

float CBotSymona::WalkTicks(float Dist)
{
	const float D = absolute(Dist);
	if(D <= 30.0f)
		return std::ceil(std::sqrt(std::max(1.0f, D)));
	return 5.0f + (D - 30.0f) / 10.0f;
}

float CBotSymona::AnchorEta(vec2 P, vec2 OwnerPos, vec2 BotPos, const SLimits &L) const
{
	const float D = distance(P, OwnerPos);
	if(D <= L.m_HookMinPull)
		return 1e9f;

	const float H = OwnerPos.y - P.y;
	const float Flight = std::ceil(std::max(0.0f, D - L.m_HookMinPull) / 80.0f);
	const float Drag = H > 64.0f ? (D - L.m_HammerMax) / 7.5f + 8.0f
				     : 42.2f * std::log(std::max(1.05f, D / L.m_HammerMax));

	return WalkTicks(distance(P, BotPos)) + Flight + Drag;
}

bool CBotSymona::HookableAt(vec2 Pos) const
{
	if(!Collision()->CheckPoint(Pos.x, Pos.y))
		return false;

	const int Index = Collision()->GetPureMapIndex(Pos);
	if(Index < 0)
		return true;
	if(Collision()->GetTileIndex(Index) == TILE_NOHOOK)
		return false;
	if(Collision()->GetFrontTileIndex(Index) == TILE_NOHOOK)
		return false;
	return true;
}

bool CBotSymona::HookProbe(vec2 From, vec2 Dir, vec2 *pGrab) const
{
	const SLimits L = Limits();
	const float Fire = std::max(1.0f, (float)GameClient()->m_aTuning[BotConn()].m_HookFireSpeed);

	const float Muzzle = CCharacterCore::PhysicalSize() * 1.5f;
	const int Steps = std::clamp((int)std::ceil((L.m_HookLength - Muzzle) / Fire), 1, 64);

	vec2 Tip = From + Dir * Muzzle;
	for(int t = 1; t <= Steps; t++)
	{
		vec2 New = Tip + Dir * Fire;
		bool Last = false;
		if(distance(From, New) > L.m_HookLength)
		{
			New = From + Dir * L.m_HookLength;
			Last = true;
		}

		vec2 Before;
		int TeleNr = 0;
		const int Hit = Collision()->IntersectLineTeleHook(Tip, New, &New, &Before, &TeleNr);
		if(Hit == TILE_NOHOOK || Hit == TILE_TELEINHOOK)
			return false;
		if(Hit != 0)
		{
			if(!HookableAt(New))
				return false;
			*pGrab = New;
			return true;
		}
		if(Last)
			return false;
		Tip = New;
	}
	return false;
}
