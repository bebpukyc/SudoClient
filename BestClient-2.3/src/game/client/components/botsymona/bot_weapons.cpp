#include "botsymona.h"

#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <algorithm>
#include <cmath>

bool CBotSymona::BotCore(CCharacterCore *pOut) const
{
	const int BotId = BotClientId();
	if(BotId < 0)
		return false;
	return CoreFromSnap(BotId, pOut);
}

bool CBotSymona::HasWeapon(int Weapon) const
{
	if(Weapon < 0 || Weapon >= NUM_WEAPONS)
		return false;
	if(!BotSnapItem(NETOBJTYPE_DDNETCHARACTER, BotClientId()))
		return false;

	CCharacterCore Core;
	if(!BotCore(&Core))
		return false;
	if(!Core.m_aWeapons[Weapon].m_Got)
		return false;

	switch(Weapon)
	{
	case WEAPON_LASER:
		return !Core.m_LaserHitDisabled;
	case WEAPON_SHOTGUN:
		return !Core.m_ShotgunHitDisabled;
	case WEAPON_GRENADE:
		return !Core.m_GrenadeHitDisabled;
	case WEAPON_HAMMER:
		return !Core.m_HammerHitDisabled;
	}
	return true;
}

int CBotSymona::WeaponFireDelay(int Weapon) const
{
	const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
	float Ms = 125.0f;
	switch(Weapon)
	{
	case WEAPON_HAMMER: Ms = Tuning.m_HammerFireDelay; break;
	case WEAPON_GUN: Ms = Tuning.m_GunFireDelay; break;
	case WEAPON_SHOTGUN: Ms = Tuning.m_ShotgunFireDelay; break;
	case WEAPON_GRENADE: Ms = Tuning.m_GrenadeFireDelay; break;
	case WEAPON_LASER: Ms = Tuning.m_LaserFireDelay; break;
	case WEAPON_NINJA: Ms = Tuning.m_NinjaFireDelay; break;
	}
	return std::max(1, (int)std::ceil(Ms * (float)Client()->GameTickSpeed() / 1000.0f));
}

bool CBotSymona::WeaponReady(int Weapon, int PredTick) const
{
	const int BotId = BotClientId();
	if(BotId < 0)
		return false;

	const CNetObj_Character *pChar = SnapChar(BotId);
	if(!pChar)
		return false;
	if(pChar->m_Weapon != Weapon)
		return false;

	(void)PredTick;
	return Client()->GameTick(BotConn()) - pChar->m_AttackTick >= WeaponFireDelay(Weapon);
}

bool CBotSymona::ShotLineClear(vec2 From, vec2 To, int SelfId, int TargetId, float TeeRadius) const
{
	vec2 Collide, Before;
	int TeleNr = 0;
	if(Collision()->IntersectLineTeleWeapon(From, To, &Collide, &Before, &TeleNr) != 0)
		return false;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == SelfId || i == TargetId || !GameClient()->m_aClients[i].m_Active)
			continue;
		if(!GameClient()->m_Teams.CanCollide(SelfId, i))
			continue;

		vec2 Pos, Vel;
		if(!TeeState(i, &Pos, &Vel))
			continue;

		vec2 Closest;
		if(!closest_point_on_line(From, To, Pos, Closest))
			continue;
		if(distance(Pos, Closest) < TeeRadius)
			return false;
	}
	return true;
}

bool CBotSymona::BeamAim(vec2 BotPos, vec2 TargetPos, int SelfId, int TargetId, int Weapon, vec2 *pAim) const
{
	const float Reach = GameClient()->m_aTuning[BotConn()].m_LaserReach;
	const float Grab = CCharacterCore::PhysicalSize();
	const bool NeedDrag = Weapon == WEAPON_SHOTGUN;

	if(distance(BotPos, TargetPos) <= Reach &&
		ShotLineClear(BotPos, TargetPos, SelfId, TargetId, Grab) &&
		(!NeedDrag || (distance(BotPos, TargetPos) >= 64.0f && DragFrees(TargetPos, TargetId, BotPos))))
	{
		*pAim = TargetPos - BotPos;
		return true;
	}

	static constexpr int ANGLES = 64;
	static constexpr int MAX_BOUNCES = 3;

	for(int i = 0; i < ANGLES; i++)
	{
		const float Angle = -3.14159265f + 2.0f * 3.14159265f * ((float)i / (float)ANGLES);
		const vec2 Start = vec2(std::cos(Angle), std::sin(Angle));

		if(!ShotLineClear(BotPos, BotPos + Start * 40.0f, SelfId, TargetId, Grab))
			continue;

		vec2 Pos = BotPos;
		vec2 Dir = Start;
		float Energy = Reach;

		for(int b = 0; b <= MAX_BOUNCES && Energy > 0.0f; b++)
		{
			const vec2 SegFrom = Pos;
			vec2 To = Pos + Dir * Energy;

			vec2 Coltile;
			int TeleNr = 0;
			const int Res = Collision()->IntersectLineTeleWeapon(Pos, To, &Coltile, &To, &TeleNr);

			vec2 Closest;
			if(closest_point_on_line(SegFrom, To, TargetPos, Closest) && distance(TargetPos, Closest) < Grab)
			{
				if(!NeedDrag || DragFrees(TargetPos, TargetId, SegFrom))
				{
					*pAim = Start;
					return true;
				}
				break;
			}

			if(!Res)
				break;

			vec2 TempPos = To;
			vec2 TempDir = Dir * 4.0f;
			Collision()->MovePoint(&TempPos, &TempDir, 1.0f, nullptr);
			if(length(TempDir) < 0.001f)
				break;

			Energy -= distance(SegFrom, TempPos);
			Pos = TempPos;
			Dir = normalize(TempDir);
		}
	}
	return false;
}

bool CBotSymona::LaserAim(vec2 BotPos, vec2 TargetPos, int SelfId, int TargetId, vec2 *pAim) const
{
	if(!g_Config.m_BotUseLaser || !HasWeapon(WEAPON_LASER))
		return false;
	return BeamAim(BotPos, TargetPos, SelfId, TargetId, WEAPON_LASER, pAim);
}

bool CBotSymona::DragFrees(vec2 TargetPos, int TargetId, vec2 PullOrigin) const
{
	const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
	const float Strength = Tuning.m_ShotgunStrength;

	if(distance(PullOrigin, TargetPos) < 1.0f)
		return false;

	vec2 TargetVel = vec2(0.0f, 0.0f);
	{
		vec2 Ignore;
		TeeState(TargetId, &Ignore, &TargetVel);
	}

	vec2 Probe = TargetPos;
	vec2 Vel = TargetVel + normalize(PullOrigin - TargetPos) * Strength;

	for(int t = 0; t < 16; t++)
	{
		Vel.x *= Collision()->IsOnGround(Probe, CCharacterCore::PhysicalSize()) ? (float)Tuning.m_GroundFriction : (float)Tuning.m_AirFriction;
		Vel.y += (float)Tuning.m_Gravity;

		const vec2 Next = Probe + Vel;

		vec2 Hit, Before;
		if(Collision()->IntersectLine(Probe, Next, &Hit, &Before))
			return !FreezeAt(Before);
		if(!FreezeAt(Next))
			return true;
		Probe = Next;
	}
	return false;
}

bool CBotSymona::ShotgunDragAim(vec2 BotPos, vec2 TargetPos, int SelfId, int TargetId, vec2 *pAim) const
{
	if(!g_Config.m_BotUseShotgun || !HasWeapon(WEAPON_SHOTGUN))
		return false;
	return BeamAim(BotPos, TargetPos, SelfId, TargetId, WEAPON_SHOTGUN, pAim);
}

bool CBotSymona::AimAndFire(CNetObj_PlayerInput *pInput, vec2 BotPos, vec2 TargetPos, int Weapon, int PredTick)
{
	pInput->m_WantedWeapon = Weapon + 1;

	const vec2 Delta = TargetPos - BotPos;
	pInput->m_TargetX = round_to_int(Delta.x);
	pInput->m_TargetY = round_to_int(Delta.y);
	if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
		pInput->m_TargetX = 1;

	if(!WeaponReady(Weapon, PredTick))
		return false;
	if(m_LastShotTick >= 0 && PredTick - m_LastShotTick < 3)
		return false;

	pInput->m_Fire = (pInput->m_Fire + 2) & INPUT_STATE_MASK;
	m_LastShotTick = PredTick;
	return true;
}
