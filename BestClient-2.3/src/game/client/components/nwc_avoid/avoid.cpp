#include "avoid.h"

#include "../../gameclient.h"

#if defined(CONF_FAMILY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <malloc.h>
#undef WIN32_LEAN_AND_MEAN
#endif

#include <base/math.h>

#include <algorithm>
#include <cmath>

#include <game/client/prediction/entities/character.h>

namespace
{
constexpr float gs_AimFallbackDistance = 128.0f;
}

CNwcAvoid::CNwcAvoid()
{
	OnReset();
}

void CNwcAvoid::ResetRuntime(int DummyIndex)
{
	if(!in_range(DummyIndex, NUM_DUMMIES - 1))
		return;

	SNwcAvoidRuntime &Runtime = m_aRuntime[DummyIndex];
	Runtime = {};
}

void CNwcAvoid::ClearDebugState(SNwcAvoidRuntime &Runtime)
{
	Runtime.m_vDebugTrace.clear();
	Runtime.m_HasDebugHookPoint = false;
	Runtime.m_DebugHookPoint = vec2(0.0f, 0.0f);
}

void CNwcAvoid::OnReset()
{
	for(int i = 0; i < NUM_DUMMIES; ++i)
		ResetRuntime(i);
}

void CNwcAvoid::OnStateChange(int NewState, int OldState)
{
	if(NewState != OldState)
		OnReset();
}

ENwcAvoidType CNwcAvoid::GetConfiguredType() const
{
	const int Type = std::clamp((int)g_Config.m_NwAvoidType, 0, (int)ENwcAvoidType::Count - 1);
	return (ENwcAvoidType)Type;
}

int CNwcAvoid::GetPredictTicks(ENwcAvoidType Type) const
{
	switch(Type)
	{
	case ENwcAvoidType::Legit:
		return std::clamp((int)g_Config.m_NwAvoidLegitPredictTicks, 1, 48);
	case ENwcAvoidType::Blatant:
		return std::clamp((int)g_Config.m_NwAvoidBlatantPredictTicks, 1, 30);
	case ENwcAvoidType::Horizontal:
		return std::clamp((int)g_Config.m_NwAvoidHorizontalPredictTicks, 1, 48);
	case ENwcAvoidType::Count:
		break;
	}
	return 8;
}

int CNwcAvoid::GetCommitTicks(ENwcAvoidType Type) const
{
	switch(Type)
	{
	case ENwcAvoidType::Legit:
		return std::clamp((int)g_Config.m_NwAvoidLegitForceTicks, 1, 24);
	case ENwcAvoidType::Blatant:
		return 0;
	case ENwcAvoidType::Horizontal:
		return std::clamp((int)g_Config.m_NwAvoidHorizontalCommitTicks, 0, 24);
	case ENwcAvoidType::Count:
		break;
	}
	return 0;
}

float CNwcAvoid::GetHookAssistRange(ENwcAvoidType Type) const
{
	switch(Type)
	{
	case ENwcAvoidType::Legit:
		return (float)std::clamp((int)g_Config.m_NwAvoidLegitHookRange, 32, 480);
	case ENwcAvoidType::Blatant:
		return 512.0f;
	case ENwcAvoidType::Horizontal:
		return (float)std::clamp((int)g_Config.m_NwAvoidHorizontalDistance, 16, 96);
	case ENwcAvoidType::Count:
		break;
	}
	return 128.0f;
}

bool CNwcAvoid::IsHookAssistEnabled(ENwcAvoidType Type) const
{
	switch(Type)
	{
	case ENwcAvoidType::Legit:
		return g_Config.m_NwAvoidLegitHookAssist != 0;
	case ENwcAvoidType::Blatant:
		return true;
	case ENwcAvoidType::Horizontal:
		return false;
	case ENwcAvoidType::Count:
		break;
	}
	return false;
}

bool CNwcAvoid::ShouldAllowJump(ENwcAvoidType Type) const
{
	switch(Type)
	{
	case ENwcAvoidType::Legit:
		return false;
	case ENwcAvoidType::Blatant:
		return g_Config.m_NwAvoidBlatantAutoJump != 0;
	case ENwcAvoidType::Horizontal:
		return g_Config.m_NwAvoidBlatantAutoJump != 0;
	case ENwcAvoidType::Count:
		break;
	}
	return false;
}

vec2 CNwcAvoid::NormalizeOr(const vec2 &Value, const vec2 &Fallback)
{
	const float Len = length(Value);
	return Len > 0.0001f ? Value / Len : Fallback;
}

vec2 CNwcAvoid::GetAimDir(const CNetObj_PlayerInput &Input) const
{
	return NormalizeOr(vec2((float)Input.m_TargetX, (float)Input.m_TargetY), vec2(1.0f, 0.0f));
}

float CNwcAvoid::GetAimDistance(const CNetObj_PlayerInput &Input) const
{
	return maximum(length(vec2((float)Input.m_TargetX, (float)Input.m_TargetY)), 1.0f);
}

SNwcAvoidAction CNwcAvoid::BuildBaseAction(const SNwcAvoidContext &Context) const
{
	SNwcAvoidAction Action;
	Action.m_Input = Context.m_BaseInput;
	Action.m_AimDir = Context.m_AimDir;
	Action.m_Id = 0;
	return Action;
}

void CNwcAvoid::ApplyAction(CNetObj_PlayerInput &Input, const SNwcAvoidContext &Context, const SNwcAvoidAction &Action) const
{
	Input = Action.m_Input;
	if(Action.m_ForceAim)
	{
		const float AimDistance = Action.m_UsesHookAssist ? maximum(Context.m_AimDistance, Context.m_HookLength) : maximum(Context.m_AimDistance, gs_AimFallbackDistance);
		const vec2 Aim = NormalizeOr(Action.m_AimDir, Context.m_AimDir) * AimDistance;
		Input.m_TargetX = round_to_int(Aim.x);
		Input.m_TargetY = round_to_int(Aim.y);
	}

	if(Input.m_TargetX == 0 && Input.m_TargetY == 0)
		Input.m_TargetY = -1;
}

void CNwcAvoid::PushUniqueAction(std::vector<SNwcAvoidAction> &vActions, const SNwcAvoidAction &Action) const
{
	for(const SNwcAvoidAction &Existing : vActions)
	{
		if(Existing.m_Input.m_Direction == Action.m_Input.m_Direction &&
			Existing.m_Input.m_Jump == Action.m_Input.m_Jump &&
			Existing.m_Input.m_Hook == Action.m_Input.m_Hook &&
			Existing.m_Input.m_Fire == Action.m_Input.m_Fire &&
			Existing.m_Input.m_WantedWeapon == Action.m_Input.m_WantedWeapon &&
			Existing.m_ForceAim == Action.m_ForceAim &&
			(!Existing.m_ForceAim || distance(Existing.m_AimDir, Action.m_AimDir) < 0.01f))
		{
			return;
		}
	}
	vActions.push_back(Action);
}

bool CNwcAvoid::GatherContext(const CNetObj_PlayerInput &Input, SNwcAvoidContext &OutContext) const
{
	const int DummyIndex = g_Config.m_ClDummy;
	if(!in_range(DummyIndex, NUM_DUMMIES - 1))
		return false;

	const int LocalId = GameClient()->m_aLocalIds[DummyIndex];
	CCharacter *pLocalChar = LocalId >= 0 ? GameClient()->m_PredictedWorld.GetCharacterById(LocalId) : nullptr;
	if(!pLocalChar || !pLocalChar->Core())
		return false;

	if(pLocalChar->m_FreezeTime > 0 || pLocalChar->Core()->m_DeepFrozen || pLocalChar->Core()->m_LiveFrozen)
		return false;

	OutContext.m_DummyIndex = DummyIndex;
	OutContext.m_LocalId = LocalId;
	OutContext.m_GameTick = GameClient()->m_PredictedWorld.GameTick();
	OutContext.m_BaseInput = Input;
	OutContext.m_LocalPos = pLocalChar->GetPos();
	OutContext.m_LocalVel = pLocalChar->Core()->m_Vel;
	OutContext.m_AimDir = GetAimDir(Input);
	OutContext.m_AimDistance = GetAimDistance(Input);
	OutContext.m_HookLength = (float)GameClient()->m_aTuning[DummyIndex].m_HookLength;
	OutContext.m_Grounded = pLocalChar->IsGrounded();
	OutContext.m_HasAirJump = (pLocalChar->Core()->m_Jumped & 2) == 0;
	OutContext.m_Hooking = Input.m_Hook != 0 || pLocalChar->Core()->m_HookState == HOOK_GRABBED || pLocalChar->Core()->m_HookState == HOOK_FLYING;
	OutContext.m_HookState = pLocalChar->Core()->m_HookState;
	OutContext.m_HookPos = pLocalChar->Core()->m_HookPos;
	OutContext.m_HookDir = pLocalChar->Core()->m_HookDir;
	OutContext.m_Super = pLocalChar->Core()->m_Super;
	OutContext.m_Invincible = pLocalChar->Core()->m_Invincible;
	DetectHazardAtPos(OutContext.m_LocalPos, false, OutContext.m_BaseHazard);
	return true;
}

void CNwcAvoid::StoreChosenCandidate(const SNwcAvoidContext &Context, const SNwcAvoidCandidate &Candidate)
{
	SNwcAvoidRuntime &Runtime = m_aRuntime[Context.m_DummyIndex];
	Runtime.m_LastChosenId = Candidate.m_Action.m_Id;
	Runtime.m_LastChosenTick = Context.m_GameTick;
	Runtime.m_LastHazardType = Candidate.m_HazardType;
	Runtime.m_CommittedAction = Candidate.m_Action;
	Runtime.m_HasCommittedAction = Candidate.m_Action.m_CommitTicks > 0;
	Runtime.m_CommitUntilTick = Runtime.m_HasCommittedAction ? Context.m_GameTick + Candidate.m_Action.m_CommitTicks - 1 : -1;

	if(g_Config.m_NwAvoidDebug)
	{
		Runtime.m_vDebugTrace = Candidate.m_vTrace;
		Runtime.m_HasDebugHookPoint = Candidate.m_HasHookPoint;
		Runtime.m_DebugHookPoint = Candidate.m_HookPoint;
	}
	else
	{
		ClearDebugState(Runtime);
	}
}

bool CNwcAvoid::TryReuseCommittedAction(const SNwcAvoidContext &Context, CNetObj_PlayerInput &Input, bool &Send)
{
	SNwcAvoidRuntime &Runtime = m_aRuntime[Context.m_DummyIndex];
	if(!Runtime.m_HasCommittedAction || Context.m_GameTick > Runtime.m_CommitUntilTick)
	{
		Runtime.m_HasCommittedAction = false;
		return false;
	}

	const int EvalTicks = std::clamp(GetPredictTicks(GetConfiguredType()), 2, 12);
	const SNwcAvoidCandidate BaseCandidate = EvaluateAction(Context, BuildBaseAction(Context), EvalTicks, false);
	const SNwcAvoidCandidate ReuseCandidate = EvaluateAction(Context, Runtime.m_CommittedAction, EvalTicks, g_Config.m_NwAvoidDebug != 0);
	if(!ReuseCandidate.m_Valid)
	{
		Runtime.m_HasCommittedAction = false;
		return false;
	}
	if(ReuseCandidate.m_TraceHasForbiddenFreeze || ReuseCandidate.m_HazardType == ENwcAvoidHazard::Freeze || ReuseCandidate.m_HazardType == ENwcAvoidHazard::Frozen)
	{
		Runtime.m_HasCommittedAction = false;
		return false;
	}

	const bool BaseSafe = !BaseCandidate.m_HasHazard;
	const bool ReuseSafe = !ReuseCandidate.m_HasHazard;
	const bool ReuseNotWorse = BaseSafe ? ReuseSafe : (ReuseSafe || ReuseCandidate.m_HazardTick >= BaseCandidate.m_HazardTick);
	if(!ReuseNotWorse)
	{
		Runtime.m_HasCommittedAction = false;
		return false;
	}

	const int ExistingCommitUntilTick = Runtime.m_CommitUntilTick;
	ApplyAction(Input, Context, Runtime.m_CommittedAction);
	Send = true;
	StoreChosenCandidate(Context, ReuseCandidate);
	Runtime.m_CommitUntilTick = ExistingCommitUntilTick;
	return true;
}

void CNwcAvoid::OnControlsSnapInput(CNetObj_PlayerInput &Input, bool &Send)
{
#if defined(CONF_FAMILY_WINDOWS)
	__try
	{
		OnControlsSnapInputImpl(Input, Send);
	}
	__except(BestClientWriteCrashTrace((void *)GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER)
	{
		dbg_msg("avoid", "avoid computation crashed; trace written to bestclient_crash.txt");
		return;
	}
#else
	OnControlsSnapInputImpl(Input, Send);
#endif
}

void CNwcAvoid::OnControlsSnapInputImpl(CNetObj_PlayerInput &Input, bool &Send)
{
	const int DummyIndex = g_Config.m_ClDummy;
	if(!in_range(DummyIndex, NUM_DUMMIES - 1))
		return;

	if(!g_Config.m_NwAvoidEnabled ||
		(Input.m_PlayerFlags & PLAYERFLAG_PLAYING) == 0 ||
		Client()->State() != IClient::STATE_ONLINE ||
		GameClient()->m_Snap.m_SpecInfo.m_Active ||
		!GameClient()->m_Snap.m_pLocalCharacter ||
		(GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) != 0))
	{
		ResetRuntime(DummyIndex);
		return;
	}

	SNwcAvoidContext Context;
	if(!GatherContext(Input, Context))
	{
		ResetRuntime(DummyIndex);
		return;
	}

	SNwcAvoidRuntime &Runtime = m_aRuntime[DummyIndex];
	if(GetConfiguredType() == ENwcAvoidType::Blatant)
		Runtime.m_HasCommittedAction = false;

	if(GetConfiguredType() != ENwcAvoidType::Blatant && TryReuseCommittedAction(Context, Input, Send))
		return;

	SNwcAvoidCandidate BestCandidate;
	if(!SelectBestCandidate(Context, BestCandidate))
	{
		Runtime.m_HasCommittedAction = false;
		ClearDebugState(Runtime);
		return;
	}

	ApplyAction(Input, Context, BestCandidate.m_Action);
	Send = true;
	StoreChosenCandidate(Context, BestCandidate);

#if defined(CONF_FAMILY_WINDOWS)
	_heapmin();
#endif
}

