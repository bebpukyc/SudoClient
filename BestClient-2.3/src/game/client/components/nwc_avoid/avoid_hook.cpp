#include "avoid.h"

#include "../../gameclient.h"

#include <base/math.h>

#include <game/mapitems.h>

#include <algorithm>
#include <array>
#include <cmath>

bool CNwcAvoid::IsHookAimViable(const SNwcAvoidContext &Context, const vec2 &AimDir, vec2 *pOutHookPoint) const
{
	const vec2 Dir = NormalizeOr(AimDir, vec2(0.0f, -1.0f));
	vec2 HitPos;
	int TeleNr = 0;
	const float ScanLen = minimum(Context.m_HookLength, GetHookAssistRange(GetConfiguredType()));
	const int Hit = Collision()->IntersectLineTeleHook(Context.m_LocalPos, Context.m_LocalPos + Dir * ScanLen, &HitPos, nullptr, &TeleNr);
	if(Hit == 0 || Hit == TILE_NOHOOK || Hit == TILE_TELEINHOOK)
		return false;
	if(distance(Context.m_LocalPos, HitPos) < CCharacterCore::PhysicalSize())
		return false;

	if(pOutHookPoint != nullptr)
		*pOutHookPoint = HitPos;
	return true;
}

bool CNwcAvoid::FindHookAssistPoint(const SNwcAvoidContext &Context, const vec2 &AimDir, vec2 &OutHookPoint) const
{
	return IsHookAimViable(Context, AimDir, &OutHookPoint);
}

void CNwcAvoid::BuildHookAssistActions(const SNwcAvoidContext &Context, std::vector<SNwcAvoidAction> &vActions, int CommitTicks, int MaxActions) const
{
	if(!IsHookAssistEnabled(GetConfiguredType()) || MaxActions <= 0)
		return;

	const int BiasDir = GetDangerBiasDirection(Context, 36.0f);
	const float Bias = (float)(BiasDir == 0 ? 1 : BiasDir);
	const std::array<vec2, 8> aDirs = {
		normalize(vec2(0.0f, -1.0f)),
		normalize(vec2(Bias * 0.25f, -1.0f)),
		normalize(vec2(Bias * 0.55f, -1.0f)),
		normalize(vec2(Bias * 0.85f, -0.90f)),
		normalize(vec2(-Bias * 0.25f, -1.0f)),
		normalize(vec2(-Bias * 0.55f, -1.0f)),
		normalize(vec2(-Bias * 0.85f, -0.90f)),
		normalize(vec2(Bias * 1.05f, -0.72f))};

	for(const vec2 &Dir : aDirs)
	{
		if((int)vActions.size() >= MaxActions)
			break;

		vec2 HookPoint;
		if(!FindHookAssistPoint(Context, Dir, HookPoint))
			continue;

		SNwcAvoidAction Action = BuildBaseAction(Context);
		Action.m_Input.m_Hook = 1;
		Action.m_AimDir = Dir;
		Action.m_ForceAim = true;
		Action.m_UsesHookAssist = true;
		Action.m_CommitTicks = CommitTicks;
		PushUniqueAction(vActions, Action);
	}
}

