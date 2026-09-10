#include "avoid.h"

#include "../../gameclient.h"

#include <base/math.h>

#include <algorithm>

#if defined(CONF_FAMILY_WINDOWS)
#include <malloc.h>
#endif

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

namespace
{
bool IsForbiddenFreezeHazard(ENwcAvoidHazard Type)
{
	return Type == ENwcAvoidHazard::Freeze || Type == ENwcAvoidHazard::Frozen;
}
}

bool CNwcAvoid::IsCharacterInHazardState(const CCharacter *pCharacter) const
{
	if(!pCharacter || !pCharacter->Core())
		return g_Config.m_NwAvoidKill != 0;

	const CCharacterCore *pCore = pCharacter->Core();
	if(g_Config.m_NwAvoidFreeze &&
		(pCharacter->m_FreezeTime > 0 || pCore->m_FreezeEnd != 0 || pCore->m_DeepFrozen || pCore->m_LiveFrozen))
	{
		return true;
	}
	if((g_Config.m_NwAvoidFreeze || g_Config.m_NwAvoidKill) && pCore->m_IsInFreeze)
		return true;

	return false;
}

SNwcAvoidCandidate CNwcAvoid::EvaluateAction(const SNwcAvoidContext &Context, const SNwcAvoidAction &Action, int PredictTicks, bool CaptureTrace) const
{
	return EvaluateDelayedAction(Context, Action, 0, PredictTicks, CaptureTrace);
}

SNwcAvoidCandidate CNwcAvoid::EvaluateDelayedAction(const SNwcAvoidContext &Context, const SNwcAvoidAction &Action, int DelayTicks, int PredictTicks, bool CaptureTrace) const
{
	SNwcAvoidCandidate Candidate;
	Candidate.m_Action = Action;
	Candidate.m_Valid = true;
	Candidate.m_EvalTicks = maximum(1, PredictTicks);
	Candidate.m_MinHazardDistance = MeasureHazardClearance(Context.m_LocalPos);
	Candidate.m_EndPos = Context.m_LocalPos;
	Candidate.m_EndVel = Context.m_LocalVel;
	Candidate.m_HazardPos = Context.m_LocalPos;
	if(Action.m_UsesHookAssist)
		Candidate.m_HasHookPoint = FindHookAssistPoint(Context, Action.m_AimDir, Candidate.m_HookPoint);
	if(CaptureTrace)
	{
		Candidate.m_vTrace.reserve(Candidate.m_EvalTicks + 1);
		Candidate.m_vTrace.push_back({Context.m_LocalPos, Context.m_BaseHazard.m_Triggered});
	}

	static CGameWorld *s_pSimWorld = nullptr;
	if(!s_pSimWorld)
		s_pSimWorld = new CGameWorld();
	s_pSimWorld->CopyWorld(&GameClient()->m_PredictedWorld);

	// Strip non-character entities — only characters + collision needed
	for(int Type = 0; Type < CGameWorld::NUM_ENTTYPES; Type++)
	{
		if(Type == CGameWorld::ENTTYPE_CHARACTER)
			continue;
		CEntity *pEnt = s_pSimWorld->FindFirst(Type);
		while(pEnt)
		{
			CEntity *pNext = pEnt->TypeNext();
			s_pSimWorld->RemoveEntity(pEnt);
			delete pEnt;
			pEnt = pNext;
		}
	}

	CCharacter *pSimChar = s_pSimWorld->GetCharacterById(Context.m_LocalId);
	if(!pSimChar)
	{
		Candidate.m_Valid = false;
		s_pSimWorld->Clear();
#if defined(CONF_FAMILY_WINDOWS)
		_heapmin();
#endif
		return Candidate;
	}

	CNetObj_PlayerInput SimInput;
	ApplyAction(SimInput, Context, Action);
	SNwcAvoidHazardState StartHazard;
	if(DetectHazardAtPos(pSimChar->GetPos(), IsCharacterInHazardState(pSimChar), StartHazard))
	{
		Candidate.m_HasHazard = true;
		Candidate.m_HazardTick = 0;
		Candidate.m_HazardType = StartHazard.m_Type;
		Candidate.m_HazardPos = StartHazard.m_Pos;
		Candidate.m_TraceHasFrozen = true;
		Candidate.m_TraceHasForbiddenFreeze = IsForbiddenFreezeHazard(StartHazard.m_Type);
		Candidate.m_TraceFirstFrozenTick = 0;
		Candidate.m_TraceFrozenTickCount = 1;
		s_pSimWorld->Clear();
#if defined(CONF_FAMILY_WINDOWS)
		_heapmin();
#endif
		return Candidate;
	}

	CNetObj_PlayerInput BaseInput;
	ApplyAction(BaseInput, Context, BuildBaseAction(Context));
	for(int Tick = 1; Tick <= Candidate.m_EvalTicks; ++Tick)
	{
		if(Tick <= DelayTicks)
			SimInput = BaseInput;
		const vec2 PrevPos = pSimChar->GetPos();
		pSimChar->OnDirectInput(&SimInput);
		s_pSimWorld->m_GameTick += 1;
		pSimChar->OnPredictedInput(&SimInput);
		s_pSimWorld->Tick();
		pSimChar = s_pSimWorld->GetCharacterById(Context.m_LocalId);

		if(!pSimChar)
		{
			Candidate.m_HasHazard = true;
			if(Candidate.m_HazardTick <= 0)
			{
				Candidate.m_HazardTick = Tick;
				Candidate.m_HazardType = ENwcAvoidHazard::Kill;
				Candidate.m_HazardPos = PrevPos;
			}
			Candidate.m_TraceHasFrozen = true;
			if(Candidate.m_TraceFirstFrozenTick < 0)
				Candidate.m_TraceFirstFrozenTick = Tick;
			Candidate.m_TraceFrozenTickCount += 1;
			break;
		}
		else if(Tick == DelayTicks)
		{
			ApplyAction(SimInput, Context, Action);
		}

		Candidate.m_EndPos = pSimChar->GetPos();
		Candidate.m_EndVel = pSimChar->Core()->m_Vel;
		Candidate.m_MinHazardDistance = minimum(Candidate.m_MinHazardDistance, MeasureHazardClearance(Candidate.m_EndPos));

		SNwcAvoidHazardState Hazard;
		bool Danger = DetectHazardOnPath(PrevPos, Candidate.m_EndPos, false, Hazard);
		if(!Danger && IsCharacterInHazardState(pSimChar))
			Danger = DetectHazardAtPos(Candidate.m_EndPos, true, Hazard);
		if(CaptureTrace)
			Candidate.m_vTrace.push_back({Candidate.m_EndPos, Danger});
		if(Danger)
		{
			if(!Candidate.m_HasHazard)
			{
				Candidate.m_HasHazard = true;
				Candidate.m_HazardTick = Tick;
				Candidate.m_HazardType = Hazard.m_Type;
				Candidate.m_HazardPos = Hazard.m_Pos;
			}
			Candidate.m_TraceHasFrozen = true;
			if(IsForbiddenFreezeHazard(Hazard.m_Type))
				Candidate.m_TraceHasForbiddenFreeze = true;
			if(Candidate.m_TraceFirstFrozenTick < 0)
				Candidate.m_TraceFirstFrozenTick = Tick;
			Candidate.m_TraceFrozenTickCount += 1;
		}
	}

	if(!Candidate.m_HasHazard)
		Candidate.m_HazardTick = Candidate.m_EvalTicks;

	const float SafeRatio = (float)Candidate.m_HazardTick / (float)maximum(1, Candidate.m_EvalTicks);
	Candidate.m_Score = SafeRatio * 2600.0f + Candidate.m_MinHazardDistance * 7.0f;
	if(!Candidate.m_HasHazard)
		Candidate.m_Score += 1000.0f;
	else
	{
		const float HazardDeficit = (float)(Candidate.m_EvalTicks - Candidate.m_HazardTick);
		Candidate.m_Score -= HazardDeficit * 90.0f;
		if(Candidate.m_HazardTick <= 2)
			Candidate.m_Score -= 240.0f;
	}
	if(Action.m_Input.m_Direction != Context.m_BaseInput.m_Direction)
		Candidate.m_Score -= 16.0f;
	if(Action.m_Input.m_Jump != 0)
		Candidate.m_Score -= 24.0f;
	if(Context.m_BaseInput.m_Hook != 0 && Action.m_Input.m_Hook == 0)
		Candidate.m_Score += 20.0f;
	if(Action.m_UsesHookAssist)
		Candidate.m_Score += Candidate.m_EndVel.y < Context.m_LocalVel.y ? 42.0f : 10.0f;
	if(Candidate.m_TraceHasFrozen)
	{
		Candidate.m_Score -= 2800.0f + (float)Candidate.m_TraceFrozenTickCount * 320.0f;
		if(Candidate.m_TraceFirstFrozenTick >= 0)
			Candidate.m_Score -= (float)maximum(0, Candidate.m_EvalTicks - Candidate.m_TraceFirstFrozenTick) * 55.0f;
	}
	if(Candidate.m_TraceHasForbiddenFreeze)
		Candidate.m_Score -= 10000.0f;
	if(Candidate.m_EndVel.y > 0.0f)
		Candidate.m_Score -= Candidate.m_EndVel.y * 10.0f;

	s_pSimWorld->Clear();
#if defined(CONF_FAMILY_WINDOWS)
	_heapmin();
#endif
	return Candidate;
}

