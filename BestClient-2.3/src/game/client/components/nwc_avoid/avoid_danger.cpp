#include "avoid.h"

#include "../../gameclient.h"

#include <base/math.h>

#include <game/mapitems.h>

#include <game/client/prediction/entities/character.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

bool CNwcAvoid::IsFreezeTileIndex(int Tile) const
{
	return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE;
}

bool CNwcAvoid::IsUnfreezeTileIndex(int Tile) const
{
	return Tile == TILE_UNFREEZE || Tile == TILE_DUNFREEZE || Tile == TILE_LUNFREEZE;
}

bool CNwcAvoid::IsTeleHazardIndex(int MapIndex) const
{
	return Collision()->IsTeleport(MapIndex) || Collision()->IsEvilTeleport(MapIndex) || Collision()->IsCheckTeleport(MapIndex) ||
		Collision()->IsCheckEvilTeleport(MapIndex) || Collision()->IsTeleportWeapon(MapIndex) || Collision()->IsTeleportHook(MapIndex) ||
		Collision()->IsTeleCheckpoint(MapIndex);
}

bool CNwcAvoid::DetectHazardAtMapIndex(int MapIndex, SNwcAvoidHazardState &OutHazard) const
{
	if(MapIndex < 0 || MapIndex >= Collision()->GetWidth() * Collision()->GetHeight())
	{
		OutHazard.m_Triggered = true;
		OutHazard.m_Type = ENwcAvoidHazard::OutOfBounds;
		OutHazard.m_Pos = vec2(0.0f, 0.0f);
		OutHazard.m_Distance = 0.0f;
		return true;
	}

	const int Tile = Collision()->GetTileIndex(MapIndex);
	const int FrontTile = Collision()->GetFrontTileIndex(MapIndex);
	const int SwitchType = Collision()->GetSwitchType(MapIndex);

	if(g_Config.m_NwAvoidKill && (Tile == TILE_DEATH || FrontTile == TILE_DEATH || SwitchType == TILE_DEATH))
	{
		OutHazard.m_Triggered = true;
		OutHazard.m_Type = ENwcAvoidHazard::Kill;
		OutHazard.m_Pos = Collision()->GetPos(MapIndex);
		OutHazard.m_Distance = 0.0f;
		return true;
	}

	if(g_Config.m_NwAvoidFreeze && (IsFreezeTileIndex(Tile) || IsFreezeTileIndex(FrontTile) || IsFreezeTileIndex(SwitchType)))
	{
		OutHazard.m_Triggered = true;
		OutHazard.m_Type = ENwcAvoidHazard::Freeze;
		OutHazard.m_Pos = Collision()->GetPos(MapIndex);
		OutHazard.m_Distance = 0.0f;
		return true;
	}

	if(g_Config.m_NwAvoidUnfreeze && (IsUnfreezeTileIndex(Tile) || IsUnfreezeTileIndex(FrontTile) || IsUnfreezeTileIndex(SwitchType)))
	{
		OutHazard.m_Triggered = true;
		OutHazard.m_Type = ENwcAvoidHazard::Unfreeze;
		OutHazard.m_Pos = Collision()->GetPos(MapIndex);
		OutHazard.m_Distance = 0.0f;
		return true;
	}

	if(g_Config.m_NwAvoidTele && IsTeleHazardIndex(MapIndex))
	{
		OutHazard.m_Triggered = true;
		OutHazard.m_Type = ENwcAvoidHazard::Tele;
		OutHazard.m_Pos = Collision()->GetPos(MapIndex);
		OutHazard.m_Distance = 0.0f;
		return true;
	}

	return false;
}

bool CNwcAvoid::DetectHazardAtPos(const vec2 &Pos, bool FrozenState, SNwcAvoidHazardState &OutHazard) const
{
	OutHazard = {};
	if(FrozenState && g_Config.m_NwAvoidFreeze)
	{
		OutHazard.m_Triggered = true;
		OutHazard.m_Type = ENwcAvoidHazard::Frozen;
		OutHazard.m_Pos = Pos;
		OutHazard.m_Distance = 0.0f;
		return true;
	}

	const float Radius = CCharacterCore::PhysicalSize() / 3.0f;
	const std::array<vec2, 9> aOffsets = {
		vec2(0.0f, 0.0f),
		vec2(Radius, -Radius),
		vec2(Radius, Radius),
		vec2(-Radius, -Radius),
		vec2(-Radius, Radius),
		vec2(Radius, 0.0f),
		vec2(-Radius, 0.0f),
		vec2(0.0f, Radius),
		vec2(0.0f, -Radius)};

	for(const vec2 &Offset : aOffsets)
	{
		const vec2 ProbePos = Pos + Offset;
		if(ProbePos.x < 0.0f || ProbePos.y < 0.0f ||
			ProbePos.x >= (float)Collision()->GetWidth() * 32.0f ||
			ProbePos.y >= (float)Collision()->GetHeight() * 32.0f)
		{
			OutHazard.m_Triggered = true;
			OutHazard.m_Type = ENwcAvoidHazard::OutOfBounds;
			OutHazard.m_Pos = ProbePos;
			OutHazard.m_Distance = 0.0f;
			return true;
		}

		const int MapIndex = Collision()->GetPureMapIndex(ProbePos);
		if(DetectHazardAtMapIndex(MapIndex, OutHazard))
		{
			return true;
		}
	}

	return false;
}

bool CNwcAvoid::DetectHazardOnPath(const vec2 &From, const vec2 &To, bool FrozenState, SNwcAvoidHazardState &OutHazard) const
{
	OutHazard = {};
	if(FrozenState && g_Config.m_NwAvoidFreeze)
	{
		OutHazard.m_Triggered = true;
		OutHazard.m_Type = ENwcAvoidHazard::Frozen;
		OutHazard.m_Pos = To;
		OutHazard.m_Distance = 0.0f;
		return true;
	}

	const float Radius = CCharacterCore::PhysicalSize() / 3.0f;
	const std::array<vec2, 9> aOffsets = {
		vec2(0.0f, 0.0f),
		vec2(Radius, -Radius),
		vec2(Radius, Radius),
		vec2(-Radius, -Radius),
		vec2(-Radius, Radius),
		vec2(Radius, 0.0f),
		vec2(-Radius, 0.0f),
		vec2(0.0f, Radius),
		vec2(0.0f, -Radius)};

	for(const vec2 &Offset : aOffsets)
	{
		const vec2 ProbeFrom = From + Offset;
		const vec2 ProbeTo = To + Offset;
		if(ProbeTo.x < 0.0f || ProbeTo.y < 0.0f ||
			ProbeTo.x >= (float)Collision()->GetWidth() * 32.0f ||
			ProbeTo.y >= (float)Collision()->GetHeight() * 32.0f)
		{
			OutHazard.m_Triggered = true;
			OutHazard.m_Type = ENwcAvoidHazard::OutOfBounds;
			OutHazard.m_Pos = ProbeTo;
			OutHazard.m_Distance = distance(From, To);
			return true;
		}

		const std::vector<int> vIndices = Collision()->GetMapIndices(ProbeFrom, ProbeTo, 64);
		for(int MapIndex : vIndices)
		{
			if(DetectHazardAtMapIndex(MapIndex, OutHazard))
			{
				OutHazard.m_Distance = distance(From, OutHazard.m_Pos);
				return true;
			}
		}
	}

	return DetectHazardAtPos(To, false, OutHazard);
}

float CNwcAvoid::MeasureHazardClearance(const vec2 &Pos) const
{
	static const std::array<vec2, 16> s_aDirs = {
		normalize(vec2(1.0f, 0.0f)),
		normalize(vec2(-1.0f, 0.0f)),
		normalize(vec2(0.0f, 1.0f)),
		normalize(vec2(0.0f, -1.0f)),
		normalize(vec2(1.0f, 1.0f)),
		normalize(vec2(1.0f, -1.0f)),
		normalize(vec2(-1.0f, 1.0f)),
		normalize(vec2(-1.0f, -1.0f)),
		normalize(vec2(2.0f, 1.0f)),
		normalize(vec2(2.0f, -1.0f)),
		normalize(vec2(-2.0f, 1.0f)),
		normalize(vec2(-2.0f, -1.0f)),
		normalize(vec2(1.0f, 2.0f)),
		normalize(vec2(1.0f, -2.0f)),
		normalize(vec2(-1.0f, 2.0f)),
		normalize(vec2(-1.0f, -2.0f))};

	float BestDistance = 96.0f;
	for(const vec2 &Dir : s_aDirs)
	{
		for(float Dist = 4.0f; Dist <= 96.0f; Dist += 4.0f)
		{
			SNwcAvoidHazardState Hazard;
			if(DetectHazardAtPos(Pos + Dir * Dist, false, Hazard))
			{
				BestDistance = minimum(BestDistance, Dist);
				break;
			}
		}
	}
	return BestDistance;
}

int CNwcAvoid::GetDangerBiasDirection(const SNwcAvoidContext &Context, float ProbeDistance) const
{
	int LeftRisk = 0;
	int RightRisk = 0;
	for(int Step = 1; Step <= 3; ++Step)
	{
		const float Scale = (float)Step / 3.0f;
		SNwcAvoidHazardState Hazard;
		if(DetectHazardAtPos(Context.m_LocalPos + vec2(ProbeDistance * Scale, 0.0f), false, Hazard))
			RightRisk++;
		if(DetectHazardAtPos(Context.m_LocalPos + vec2(-ProbeDistance * Scale, 0.0f), false, Hazard))
			LeftRisk++;
	}

	if(RightRisk == LeftRisk)
	{
		if(Context.m_LocalVel.x > 1.0f)
			return -1;
		if(Context.m_LocalVel.x < -1.0f)
			return 1;
		if(Context.m_BaseInput.m_Direction > 0)
			return -1;
		if(Context.m_BaseInput.m_Direction < 0)
			return 1;
		return 0;
	}

	return RightRisk > LeftRisk ? -1 : 1;
}

bool CNwcAvoid::IsHorizontalThreat(const SNwcAvoidContext &Context) const
{
	if(!g_Config.m_NwAvoidFreeze)
		return false;

	const float ProbeDistance = (float)std::clamp((int)g_Config.m_NwAvoidHorizontalDistance, 16, 96);
	const float VelThreshold = maximum(0.25f, g_Config.m_NwAvoidHorizontalSensitivity / 100.0f);
	SNwcAvoidHazardState LeftHazard;
	SNwcAvoidHazardState RightHazard;
	const bool HazardLeft = DetectHazardAtPos(Context.m_LocalPos + vec2(-ProbeDistance, 0.0f), false, LeftHazard) &&
		(LeftHazard.m_Type == ENwcAvoidHazard::Freeze || LeftHazard.m_Type == ENwcAvoidHazard::Frozen);
	const bool HazardRight = DetectHazardAtPos(Context.m_LocalPos + vec2(ProbeDistance, 0.0f), false, RightHazard) &&
		(RightHazard.m_Type == ENwcAvoidHazard::Freeze || RightHazard.m_Type == ENwcAvoidHazard::Frozen);

	if((Context.m_LocalVel.x > VelThreshold && HazardRight) || (Context.m_LocalVel.x < -VelThreshold && HazardLeft))
		return true;
	if((Context.m_BaseInput.m_Direction > 0 && HazardRight) || (Context.m_BaseInput.m_Direction < 0 && HazardLeft))
		return true;
	return HazardLeft || HazardRight;
}

