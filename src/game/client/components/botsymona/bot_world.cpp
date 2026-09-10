#include "botsymona.h"

#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>
#include <algorithm>
#include <cmath>

CBotSymona::SLimits CBotSymona::Limits() const
{
	const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];

	SLimits Limits;
	Limits.m_HookLength = Tuning.m_HookLength;
	Limits.m_HookReach = std::max(64.0f, Limits.m_HookLength - 24.0f);
	Limits.m_HookMinPull = 42.0f;
	Limits.m_HammerMax = 58.0f;

	const float FireSpeed = std::max(1.0f, (float)Tuning.m_HookFireSpeed);
	Limits.m_HookTravelTicks = std::clamp((int)std::ceil((Limits.m_HookLength - 42.0f) / FireSpeed) - 1, 2, 30);
	return Limits;
}

int CBotSymona::PredictHorizon() const
{
	const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
	const float G = std::max(0.05f, (float)Tuning.m_Gravity);
	const float Impulse = Tuning.m_GroundJumpImpulse;
	return std::clamp((int)std::ceil(1.0f + 2.0f * Impulse / G), 30, 90);
}

float CBotSymona::FollowDist() const
{
	return (float)g_Config.m_BotFollowDist;
}

static bool IsFreezeTileIndex(int Tile)
{
	return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE || Tile == TILE_DEATH;
}

bool CBotSymona::SwitchActive(int Index) const
{
	const int Number = Collision()->GetSwitchNumber(Index);
	if(Number == 0)
		return true;

	std::vector<SSwitchers> &vSwitchers = GameClient()->m_GameWorld.Switchers();
	if(Number < 0 || Number >= (int)vSwitchers.size())
		return true;

	const int BotId = BotClientId();
	int Team = 0;
	if(BotId >= 0)
		Team = GameClient()->m_Teams.Team(BotId);
	if(Team < 0 || Team >= NUM_DDRACE_TEAMS)
		return true;

	return vSwitchers[Number].m_aStatus[Team];
}

bool CBotSymona::FreezeAtIndex(int Index) const
{
	if(Index < 0)
		return false;
	if(IsFreezeTileIndex(Collision()->GetTileIndex(Index)) ||
		IsFreezeTileIndex(Collision()->GetFrontTileIndex(Index)))
		return true;

	const int SwitchType = Collision()->GetSwitchType(Index);
	if(SwitchType != TILE_FREEZE && SwitchType != TILE_DFREEZE && SwitchType != TILE_LFREEZE)
		return false;
	return SwitchActive(Index);
}

bool CBotSymona::FreezeAt(vec2 Pos) const
{
	return FreezeAtIndex(Collision()->GetPureMapIndex(Pos));
}

bool CBotSymona::SweepFreeze(vec2 From, vec2 To) const
{
	const float Len = distance(From, To);
	const int Steps = std::max(1, (int)(Len / 4.0f));
	for(int i = 0; i <= Steps; i++)
	{
		const vec2 P = mix(From, To, (float)i / (float)Steps);
		if(FreezeAt(P))
			return true;
	}
	return false;
}

bool CBotSymona::CoreFromSnap(int ClientId, CCharacterCore *pOut) const
{
	const CNetObj_Character *pChar = SnapChar(ClientId);
	if(!pChar)
		return false;

	pOut->Reset();
	pOut->Init(nullptr, Collision(), nullptr);
	pOut->m_Id = -1;
	pOut->m_Tuning = GameClient()->m_aTuning[BotConn()];

	for(auto &Weapon : pOut->m_aWeapons)
	{
		Weapon.m_AmmoRegenStart = 0;
		Weapon.m_Ammo = 0;
		Weapon.m_Ammocost = 0;
		Weapon.m_Got = false;
	}

	pOut->Read(pChar);

	const CNetObj_DDNetCharacter *pExt =
		(const CNetObj_DDNetCharacter *)BotSnapItem(NETOBJTYPE_DDNETCHARACTER, ClientId);
	if(pExt)
		pOut->ReadDDNet(pExt);
	return true;
}

bool CBotSymona::PredCore(int ClientId, CCharacterCore *pOut) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;

	const int Tick = Client()->PredGameTick(g_Config.m_ClDummy) - 1;
	const auto &Data = GameClient()->m_aClients[ClientId];

	if(g_Config.m_ClPredict && GameClient()->PredictDummy() && Data.m_Active &&
		Tick >= 0 && Data.m_aPredTick[Tick % 50] == Tick)
	{
		*pOut = Data.m_Predicted;
		pOut->SetCoreWorld(nullptr, Collision(), nullptr);
		pOut->m_Id = -1;
		pOut->m_Tuning = GameClient()->m_aTuning[BotConn()];
		return true;
	}
	return false;
}

bool CBotSymona::LaneClear(float FromX, float ToX, float WalkY) const
{
	const int Width = Collision()->GetWidth();
	const int A = std::clamp(round_to_int(std::min(FromX, ToX)) / 32, 0, Width - 1);
	const int B = std::clamp(round_to_int(std::max(FromX, ToX)) / 32, 0, Width - 1);

	for(int c = A; c <= B; c++)
	{
		const vec2 P = vec2(32.0f * c + 16.0f, WalkY);
		if(FreezeAt(P))
			return false;
		if(Collision()->TestBox(P, CCharacterCore::PhysicalSizeVec2()))
			return false;
		if(!Collision()->IsOnGround(P, CCharacterCore::PhysicalSize()))
			return false;
	}
	return true;
}

bool CBotSymona::SafeStand(vec2 Pos) const
{
	const float Half = CCharacterCore::PhysicalSize() / 2.0f;
	if(Collision()->TestBox(Pos, CCharacterCore::PhysicalSizeVec2()))
		return false;
	if(!Collision()->CheckPoint(Pos.x - Half, Pos.y + Half + 5.0f) ||
		!Collision()->CheckPoint(Pos.x + Half, Pos.y + Half + 5.0f))
		return false;
	if(FreezeAt(Pos) || FreezeAt(vec2(Pos.x, Pos.y - Half)))
		return false;
	return true;
}

bool CBotSymona::SafeGoalNear(vec2 OwnerPos, vec2 BotPos, vec2 *pOut) const
{
	static const int s_aRow[6] = {0, 1, 2, -1, 3, 4};

	const float Toward = BotPos.x < OwnerPos.x ? -1.0f : 1.0f;

	bool Found = false;
	float Best = 1e9f;

	for(int s = 0; s < 2; s++)
	{
		const float Sign = s == 0 ? Toward : -Toward;
		for(float Out = 16.0f; Out <= 128.0f; Out += 16.0f)
		{
			for(int r = 0; r < 6; r++)
			{
				const vec2 Try = vec2(OwnerPos.x + Sign * Out,
					32.0f * std::floor((OwnerPos.y + 15.0f + 32.0f * (float)s_aRow[r]) / 32.0f) - 15.0f);
				if(!SafeStand(Try))
					continue;
				const float D = distance(Try, BotPos);
				if(D >= Best)
					continue;
				Best = D;
				*pOut = Try;
				Found = true;
			}
		}
	}
	return Found;
}

bool CBotSymona::CorridorClear(vec2 From, vec2 To) const
{
	vec2 Collide, Before;
	const float Half = CCharacterCore::PhysicalSize() / 2.0f;

	if(Collision()->IntersectLine(From, To, &Collide, &Before))
		return false;
	if(Collision()->IntersectLine(From + vec2(0.0f, -Half), To + vec2(0.0f, -Half), &Collide, &Before))
		return false;
	if(Collision()->IntersectLine(From + vec2(0.0f, Half), To + vec2(0.0f, Half), &Collide, &Before))
		return false;

	return !SweepFreeze(From, To);
}

const CNetObj_Character *CBotSymona::SnapChar(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return nullptr;

	const CNetObj_Character *pChar =
		(const CNetObj_Character *)BotSnapItem(NETOBJTYPE_CHARACTER, ClientId);
	if(pChar)
		return pChar;

	const auto &Info = GameClient()->m_Snap.m_aCharacters[ClientId];
	return Info.m_Active ? &Info.m_Cur : nullptr;
}

bool CBotSymona::TeeState(int ClientId, vec2 *pPos, vec2 *pVel) const
{
	const CNetObj_Character *pChar = SnapChar(ClientId);
	if(!pChar)
		return false;

	*pPos = vec2(pChar->m_X, pChar->m_Y);
	*pVel = vec2(pChar->m_VelX / 256.0f, pChar->m_VelY / 256.0f);
	return true;
}

bool CBotSymona::IsFrozen(int ClientId, bool *pDeep) const
{
	*pDeep = false;

	const CNetObj_DDNetCharacter *pExt =
		(const CNetObj_DDNetCharacter *)BotSnapItem(NETOBJTYPE_DDNETCHARACTER, ClientId);
	if(pExt)
	{
		*pDeep = pExt->m_FreezeEnd == -1 || (pExt->m_Flags & CHARACTERFLAG_MOVEMENTS_DISABLED) != 0;
		return pExt->m_FreezeEnd != 0 || *pDeep;
	}

	const auto &Data = GameClient()->m_aClients[ClientId];
	*pDeep = Data.m_DeepFrozen || Data.m_LiveFrozen;
	return Data.m_FreezeEnd != 0 || *pDeep;
}

int CBotSymona::FreezeTicksLeft(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return 0;

	const CNetObj_DDNetCharacter *pExt =
		(const CNetObj_DDNetCharacter *)BotSnapItem(NETOBJTYPE_DDNETCHARACTER, ClientId);
	if(!pExt)
		return 0;
	if(pExt->m_FreezeEnd == -1)
		return 100000;
	if(pExt->m_FreezeEnd == 0)
		return 0;
	return std::max(0, pExt->m_FreezeEnd - Client()->GameTick(BotConn()));
}
