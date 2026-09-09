#include "avoid.h"

#include "../../gameclient.h"

#include <base/math.h>

#include <algorithm>
#include <array>
#include <cmath>

#include <game/collision.h>
#include <game/mapitems.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

namespace
{
static CGameWorld *s_pFutureWorld = nullptr;
constexpr int ZZ_BLATANT_AVOID_BASE_TICKS = 14;
constexpr int ZZ_BLATANT_AVOID_MIN_TICKS = 8;
constexpr int ZZ_BLATANT_AVOID_MAX_TICKS = 30;
constexpr int ZZ_BLATANT_AVOID_VALIDATE_EXTRA_TICKS = 10;
constexpr int ZZ_BLATANT_AVOID_TRIGGER_TICKS = 4;
constexpr int ZZ_BLATANT_AVOID_PANIC_TICKS = 2;
constexpr int ZZ_BLATANT_AVOID_ONE_TILE_TICKS = 1;
constexpr int ZZ_BLATANT_AVOID_MAX_CANDIDATES = 24;
constexpr float ZZ_BLATANT_AVOID_MIN_HOOK_DIST = 24.0f;
constexpr float ZZ_BLATANT_AVOID_PANIC_MIN_HOOK_DIST = 8.0f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_MIN_HOOK_DIST = 4.0f;
constexpr float ZZ_BLATANT_AVOID_DUPLICATE_DIST = 18.0f;
constexpr float ZZ_BLATANT_AVOID_TRIGGER_DIST = 40.0f;
constexpr float ZZ_BLATANT_AVOID_TRIGGER_MAX_DIST = 72.0f;
constexpr float ZZ_BLATANT_AVOID_PANIC_DIST = 24.0f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_DIST = 44.0f;
constexpr float ZZ_BLATANT_AVOID_CURSOR_DANGER_SOFT_DOT = 0.20f;
constexpr float ZZ_BLATANT_AVOID_CURSOR_DANGER_HARD_DOT = 0.60f;
constexpr float ZZ_BLATANT_AVOID_PANIC_CURSOR_DANGER_DOT = 0.45f;
constexpr float ZZ_BLATANT_AVOID_DIRECTION_SCORE = 96.0f;
constexpr float ZZ_BLATANT_AVOID_CURSOR_INPUT_SCORE = 1.15f;
constexpr float ZZ_BLATANT_AVOID_CURSOR_HOOK_SCORE = 84.0f;
constexpr float ZZ_BLATANT_AVOID_CURSOR_RAY_SCORE = 0.35f;
constexpr float ZZ_BLATANT_AVOID_CURSOR_PROGRESS_SCORE = 0.34f;
constexpr float ZZ_BLATANT_AVOID_PANIC_CURSOR_PROGRESS_SCORE = 0.50f;
constexpr float ZZ_BLATANT_AVOID_CURSOR_SIDE_SCORE = 0.05f;
constexpr float ZZ_BLATANT_AVOID_ESCAPE_PROGRESS_SCORE = 0.11f;
constexpr float ZZ_BLATANT_AVOID_PANIC_ESCAPE_PROGRESS_SCORE = 0.18f;
constexpr float ZZ_BLATANT_AVOID_BACKTRACK_SCORE = 1.45f;
constexpr float ZZ_BLATANT_AVOID_CORRIDOR_AXIS_SCORE = 0.18f;
constexpr float ZZ_BLATANT_AVOID_MANUAL_DIRECTION_SCORE = 0.18f;
constexpr float ZZ_BLATANT_AVOID_MANUAL_JUMP_SCORE = 0.32f;
constexpr float ZZ_BLATANT_AVOID_AUTO_JUMP_SCORE = 0.9f;
constexpr float ZZ_BLATANT_AVOID_FALLBACK_DIST_IMPROVEMENT = 12.0f;
constexpr float ZZ_BLATANT_AVOID_HOOK_MOVE_SCORE_FACTOR = 0.7f;
constexpr int ZZ_BLATANT_AVOID_HOOK_INPUT_VARIANTS = 3;
constexpr int ZZ_BLATANT_AVOID_PANIC_HOOK_INPUT_VARIANTS = 6;
constexpr float ZZ_BLATANT_AVOID_PANIC_RELEASE_SCORE = -0.9f;
constexpr float ZZ_BLATANT_AVOID_REHOOK_DIR_SCORE = 96.0f;
constexpr float ZZ_BLATANT_AVOID_REHOOK_PROGRESS_SCORE = 0.18f;
constexpr int ZZ_BLATANT_AVOID_REHOOK_MAX_CANDIDATES = 40;
constexpr int ZZ_BLATANT_AVOID_TAKEOVER_TICKS = 4;
constexpr int ZZ_BLATANT_AVOID_TAKEOVER_MAX_CANDIDATES = 56;
constexpr int ZZ_BLATANT_AVOID_TAKEOVER_HOOK_INPUT_VARIANTS = 8;
constexpr float ZZ_BLATANT_AVOID_TAKEOVER_RELEASE_SCORE = -1.2f;
constexpr int ZZ_BLATANT_AVOID_ONE_TILE_MAX_CANDIDATES = 96;
constexpr int ZZ_BLATANT_AVOID_ONE_TILE_HOOK_INPUT_VARIANTS = 14;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_RELEASE_SCORE = -2.2f;
constexpr float ZZ_BLATANT_AVOID_FALL_RESCUE_SPEED = 7.0f;
constexpr float ZZ_BLATANT_AVOID_FALL_TRAVEL_SCORE = 0.16f;
constexpr float ZZ_BLATANT_AVOID_REHOOK_LOWER_BLOCK_SCORE = 0.18f;
constexpr float ZZ_BLATANT_AVOID_HOOK_STABILITY_SCORE = 10.0f;
constexpr float ZZ_BLATANT_AVOID_NEW_HOOK_SAFE_PENALTY = 6.0f;
constexpr float ZZ_BLATANT_AVOID_HOOK_REPLACE_MARGIN = 14.0f;
constexpr float ZZ_BLATANT_AVOID_KEEP_HOOK_MARGIN = 6.0f;
constexpr float ZZ_BLATANT_AVOID_KEEP_HOOK_SAFE_BONUS = 2.6f;
constexpr float ZZ_BLATANT_AVOID_HOOK_CURSOR_VERTICAL_SCORE = 0.12f;
constexpr float ZZ_BLATANT_AVOID_HOOK_CURSOR_LOCK_PENALTY = 18.0f;
constexpr float ZZ_BLATANT_AVOID_HOOK_REVERSE_PENALTY = 24.0f;
constexpr int ZZ_BLATANT_AVOID_HOOK_COMMIT_TICKS = 6;
constexpr float ZZ_BLATANT_AVOID_SAFE_CURSOR_BACKTRACK_DIST = 6.0f;
constexpr float ZZ_BLATANT_AVOID_SAFE_CURSOR_ESCAPE_DIST = 18.0f;
constexpr float ZZ_BLATANT_AVOID_SAFE_CURSOR_HOOK_MIN_DOT = 0.12f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_CURSOR_PROGRESS_SCORE = 0.46f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_BACKTRACK_SCORE = 1.15f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_SAFE_CURSOR_BACKTRACK_DIST = 8.0f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_SAFE_CURSOR_ESCAPE_DIST = 12.0f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_HOOK_CURSOR_MIN_DOT = -0.08f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_OPPOSITE_HOOK_ESCAPE_DOT = 0.55f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_OPPOSITE_CURSOR_PROGRESS = -3.0f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_REHOOK_ESCAPE_DOT = 0.82f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_REHOOK_ESCAPE_PROGRESS = 14.0f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_REHOOK_REVERSE_MARGIN = 0.18f;
constexpr float ZZ_BLATANT_AVOID_ONE_TILE_VERTICAL_HOOK_MARGIN = 8.0f;
constexpr int ZZ_BLATANT_AVOID_LAST_JUMP_CHECK_TICKS = 22;
constexpr int ZZ_BLATANT_AVOID_JUMP_CHECK_TICKS = 5;
constexpr int ZZ_BLATANT_AVOID_PHASE_TICKS = 4;
constexpr float ZZ_BLATANT_AVOID_FOLLOWUP_SCORE_FACTOR = 0.55f;
constexpr int ZZ_BLATANT_AVOID_PHASE_BUDGET_SAFE = 2;
constexpr int ZZ_BLATANT_AVOID_PHASE_BUDGET_TAKEOVER = 6;
constexpr int ZZ_BLATANT_AVOID_PHASE_DANGER_TICKS = 6;

struct SZzAvoidHookCandidate
{
	vec2 m_HookPos;
	float m_Score;

	bool operator<(const SZzAvoidHookCandidate &Other) const
	{
		return m_Score < Other.m_Score;
	}
};

struct SZzAvoidInputCandidate
{
	CNetObj_PlayerInput m_Input;
	float m_Score;

	bool operator<(const SZzAvoidInputCandidate &Other) const
	{
		return m_Score < Other.m_Score;
	}
};

struct SZzAvoidFallback
{
	bool m_Valid = false;
	CNetObj_PlayerInput m_Input{};
	float m_Score = 0.0f;
	int m_DangerTick = -1;
	float m_DangerDist = 0.0f;
};

float CandidateSafety(const SNwcAvoidCandidate &Candidate)
{
	return (float)Candidate.m_HazardTick / (float)maximum(1, Candidate.m_EvalTicks);
}

int CompareCandidates(const SNwcAvoidCandidate &Left, const SNwcAvoidCandidate &Right)
{
	const float LeftSafety = CandidateSafety(Left);
	const float RightSafety = CandidateSafety(Right);
	if(std::fabs(LeftSafety - RightSafety) > 0.0001f)
		return LeftSafety > RightSafety ? 1 : -1;
	if(Left.m_HazardTick != Right.m_HazardTick)
		return Left.m_HazardTick > Right.m_HazardTick ? 1 : -1;
	if(Left.m_TraceHasFrozen != Right.m_TraceHasFrozen)
		return !Left.m_TraceHasFrozen ? 1 : -1;
	if(Left.m_TraceHasFrozen && Right.m_TraceHasFrozen)
	{
		if(Left.m_TraceFrozenTickCount != Right.m_TraceFrozenTickCount)
			return Left.m_TraceFrozenTickCount < Right.m_TraceFrozenTickCount ? 1 : -1;
		if(Left.m_TraceFirstFrozenTick != Right.m_TraceFirstFrozenTick)
			return Left.m_TraceFirstFrozenTick > Right.m_TraceFirstFrozenTick ? 1 : -1;
	}
	if(std::fabs(Left.m_MinHazardDistance - Right.m_MinHazardDistance) > 0.1f)
		return Left.m_MinHazardDistance > Right.m_MinHazardDistance ? 1 : -1;
	if(std::fabs(Left.m_Score - Right.m_Score) > 0.001f)
		return Left.m_Score > Right.m_Score ? 1 : -1;
	return 0;
}

bool IsMeaningfullyBetter(const SNwcAvoidCandidate &Best, const SNwcAvoidCandidate &Base)
{
	if(CompareCandidates(Best, Base) <= 0)
		return false;
	if(!Base.m_HasHazard)
		return false;
	if(!Best.m_HasHazard)
		return true;
	return Best.m_HazardTick > Base.m_HazardTick ||
		Best.m_MinHazardDistance > Base.m_MinHazardDistance + 6.0f ||
		(Base.m_TraceHasFrozen && !Best.m_TraceHasFrozen);
}

bool ZzAvoidIsFrozenState(const CCharacter *pCharacter)
{
	return !pCharacter || pCharacter->m_FreezeTime > 0 || pCharacter->Core()->m_FreezeEnd != 0 || pCharacter->Core()->m_IsInFreeze || pCharacter->Core()->m_DeepFrozen;
}

bool ZzAvoidIsDangerTile(int Tile)
{
	return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE || Tile == TILE_DEATH;
}

vec2 ZzAvoidNormalizedOrZero(vec2 Dir)
{
	const float Len = length(Dir);
	if(Len < 0.001f)
		return vec2(0.0f, 0.0f);
	return Dir / Len;
}

float ZzAvoidCursorWeight(vec2 CursorDir, vec2 DangerDir)
{
	if(length(CursorDir) < 0.001f)
		return 0.0f;

	const float CursorDangerDot = dot(CursorDir, DangerDir);
	if(CursorDangerDot <= ZZ_BLATANT_AVOID_CURSOR_DANGER_SOFT_DOT)
		return 1.0f;
	if(CursorDangerDot >= ZZ_BLATANT_AVOID_CURSOR_DANGER_HARD_DOT)
		return 0.18f;

	const float T = (CursorDangerDot - ZZ_BLATANT_AVOID_CURSOR_DANGER_SOFT_DOT) / (ZZ_BLATANT_AVOID_CURSOR_DANGER_HARD_DOT - ZZ_BLATANT_AVOID_CURSOR_DANGER_SOFT_DOT);
	return mix(1.0f, 0.18f, T);
}

vec2 ZzAvoidPreferredCursorDir(vec2 CursorDir, vec2 DangerDir)
{
	if(length(CursorDir) < 0.001f)
		return vec2(0.0f, 0.0f);

	const float CursorDangerDot = maximum(0.0f, dot(CursorDir, DangerDir));
	if(CursorDangerDot <= ZZ_BLATANT_AVOID_CURSOR_DANGER_SOFT_DOT)
		return CursorDir;

	const vec2 SideDir = CursorDir - DangerDir * CursorDangerDot;
	return ZzAvoidNormalizedOrZero(SideDir);
}

bool ZzAvoidIsDangerIndex(const CCollision *pCollision, int Index)
{
	if(!pCollision || Index < 0)
		return false;

	if(ZzAvoidIsDangerTile(pCollision->GetTileIndex(Index)) ||
		ZzAvoidIsDangerTile(pCollision->GetFrontTileIndex(Index)) ||
		ZzAvoidIsDangerTile(pCollision->GetSwitchType(Index)))
	{
		return true;
	}

	return pCollision->IsTeleport(Index) != 0 ||
		pCollision->IsEvilTeleport(Index) != 0 ||
		pCollision->IsCheckTeleport(Index) ||
		pCollision->IsCheckEvilTeleport(Index);
}

bool ZzAvoidProbeDanger(const CCollision *pCollision, vec2 Pos, vec2 Offset)
{
	if(!pCollision)
		return false;

	for(const int Index : pCollision->GetMapIndices(Pos, Pos + Offset))
	{
		if(ZzAvoidIsDangerIndex(pCollision, Index))
			return true;
	}
	return false;
}

bool ZzAvoidFindDangerOnPath(const CCollision *pCollision, vec2 PrevPos, vec2 CurrentPos, vec2 *pDangerPos = nullptr)
{
	if(!pCollision)
		return false;

	for(const int Index : pCollision->GetMapIndices(PrevPos, CurrentPos))
	{
		if(ZzAvoidIsDangerIndex(pCollision, Index))
		{
			if(pDangerPos)
				*pDangerPos = pCollision->GetPos(Index);
			return true;
		}
	}
	return false;
}

float ZzAvoidInputScore(int Direction, bool Jump, vec2 DangerDir, vec2 CursorDir, float CursorWeight, int BaseDirection, bool BaseJump)
{
	vec2 MoveDir((float)Direction, Jump ? -1.0f : 0.0f);
	const float MoveLen = length(MoveDir);
	float Score = Jump ? ZZ_BLATANT_AVOID_AUTO_JUMP_SCORE : 0.0f;
	(void)CursorWeight;

	if(MoveLen < 0.001f)
	{
		Score += 0.25f;
	}
	else
	{
		const vec2 MoveDirNormalized = MoveDir / MoveLen;
		Score += 1.0f - dot(MoveDirNormalized, -DangerDir);
		if(length(CursorDir) > 0.001f)
			Score -= dot(MoveDirNormalized, CursorDir) * ZZ_BLATANT_AVOID_CURSOR_INPUT_SCORE;
	}

	if(Direction != BaseDirection)
		Score += ZZ_BLATANT_AVOID_MANUAL_DIRECTION_SCORE;
	if(Jump != BaseJump)
		Score += ZZ_BLATANT_AVOID_MANUAL_JUMP_SCORE;

	return Score;
}

float ZzAvoidHookScore(vec2 StartPos, vec2 HookPos, vec2 DangerDir, vec2 CursorDir, float CursorWeight)
{
	const float HookDist = distance(StartPos, HookPos);
	const vec2 HookDir = normalize(HookPos - StartPos);
	(void)CursorWeight;

	float Score = HookDist - dot(HookDir, -DangerDir) * ZZ_BLATANT_AVOID_DIRECTION_SCORE;
	if(length(CursorDir) > 0.001f)
	{
		Score -= dot(HookDir, CursorDir) * ZZ_BLATANT_AVOID_CURSOR_HOOK_SCORE;
		const vec2 ToHook = HookPos - StartPos;
		const float CursorProjection = maximum(0.0f, dot(ToHook, CursorDir));
		Score += distance(ToHook, CursorDir * CursorProjection) * ZZ_BLATANT_AVOID_CURSOR_RAY_SCORE;
	}
	if(dot(HookDir, DangerDir) > 0.97f)
		Score += ZZ_BLATANT_AVOID_DIRECTION_SCORE;

	return Score;
}

bool ZzAvoidIsFallbackImprovement(int BaseDangerTick, float BaseDangerDist, int CandidateDangerTick, float CandidateDangerDist)
{
	return CandidateDangerTick > BaseDangerTick ||
		(CandidateDangerTick == BaseDangerTick && CandidateDangerDist > BaseDangerDist + ZZ_BLATANT_AVOID_FALLBACK_DIST_IMPROVEMENT);
}

bool ZzAvoidIsBetterFallback(int CandidateDangerTick, float CandidateDangerDist, float CandidateScore, const SZzAvoidFallback &BestFallback)
{
	if(!BestFallback.m_Valid)
		return true;
	if(CandidateDangerTick != BestFallback.m_DangerTick)
		return CandidateDangerTick > BestFallback.m_DangerTick;
	if(std::abs(CandidateDangerDist - BestFallback.m_DangerDist) > 0.001f)
		return CandidateDangerDist > BestFallback.m_DangerDist;
	return CandidateScore < BestFallback.m_Score;
}

bool SimulateZzBlatantAvoid(CGameWorld *pSourceWorld, int ClientId, const CNetObj_PlayerInput &BaseInput, int Ticks, const vec2 *pHookPos, int *pFirstDangerTick = nullptr, vec2 *pFirstDangerPos = nullptr, vec2 *pFinalPos = nullptr)
{
	if(!s_pFutureWorld)
		s_pFutureWorld = new CGameWorld();
	s_pFutureWorld->CopyWorld(pSourceWorld);
	const CCollision *pCollision = s_pFutureWorld->Collision();

	// Strip all non-character entities — avoid only needs characters + collision.
	// This makes Tick() extremely lightweight: no projectile/laser/dragger simulation.
	for(int Type = 0; Type < CGameWorld::NUM_ENTTYPES; Type++)
	{
		if(Type == CGameWorld::ENTTYPE_CHARACTER)
			continue;
		CEntity *pEnt = s_pFutureWorld->FindFirst(Type);
		while(pEnt)
		{
			CEntity *pNext = pEnt->TypeNext();
			s_pFutureWorld->RemoveEntity(pEnt);
			delete pEnt;
			pEnt = pNext;
		}
	}

	bool Result = false;
	for(int Tick = 0; Tick < Ticks; Tick++)
	{
		CCharacter *pCharacter = s_pFutureWorld->GetCharacterById(ClientId);
		if(ZzAvoidIsFrozenState(pCharacter))
		{
			if(pFirstDangerTick) *pFirstDangerTick = Tick;
			if(pFirstDangerPos) *pFirstDangerPos = pCharacter ? pCharacter->GetPos() : vec2(0.0f, 0.0f);
			if(pFinalPos) *pFinalPos = pCharacter ? pCharacter->GetPos() : vec2(0.0f, 0.0f);
			Result = true;
			break;
		}

		const vec2 PreviousPos = pCharacter->GetPos();
		if(ZzAvoidFindDangerOnPath(pCollision, PreviousPos, PreviousPos, pFirstDangerPos))
		{
			if(pFirstDangerTick) *pFirstDangerTick = Tick;
			if(pFinalPos) *pFinalPos = PreviousPos;
			Result = true;
			break;
		}

		CNetObj_PlayerInput Input = BaseInput;
		if(pHookPos)
		{
			const vec2 Aim = *pHookPos - pCharacter->GetPos();
			Input.m_Hook = 1;
			Input.m_TargetX = round_to_int(Aim.x);
			Input.m_TargetY = round_to_int(Aim.y);
			if(Input.m_TargetX == 0 && Input.m_TargetY == 0)
				Input.m_TargetX = 1;
		}

		pCharacter->OnDirectInput(&Input);
		s_pFutureWorld->m_GameTick++;
		pCharacter->OnPredictedInput(&Input);
		s_pFutureWorld->Tick();

		pCharacter = s_pFutureWorld->GetCharacterById(ClientId);
		if(ZzAvoidIsFrozenState(pCharacter))
		{
			if(pFirstDangerTick) *pFirstDangerTick = Tick;
			if(pFirstDangerPos) *pFirstDangerPos = pCharacter ? pCharacter->GetPos() : vec2(0.0f, 0.0f);
			if(pFinalPos) *pFinalPos = pCharacter ? pCharacter->GetPos() : PreviousPos;
			Result = true;
			break;
		}
		if(!pCharacter)
			break;
		if(ZzAvoidFindDangerOnPath(pCollision, PreviousPos, pCharacter->GetPos(), pFirstDangerPos))
		{
			if(pFirstDangerTick) *pFirstDangerTick = Tick;
			if(pFinalPos) *pFinalPos = pCharacter->GetPos();
			Result = true;
			break;
		}
	}

	if(!Result && pFinalPos)
	{
		CCharacter *pCharacter = s_pFutureWorld->GetCharacterById(ClientId);
		*pFinalPos = pCharacter ? pCharacter->GetPos() : vec2(0.0f, 0.0f);
	}

	// Release all entities immediately, don't hold them
	s_pFutureWorld->Clear();
#if defined(CONF_FAMILY_WINDOWS)
	_heapmin();
#endif
	return Result;
}
}

bool CNwcAvoid::SelectLegitCandidate(const SNwcAvoidContext &Context, const SNwcAvoidCandidate &BaseCandidate, SNwcAvoidCandidate &OutCandidate)
{
	std::vector<SNwcAvoidAction> vActions;
	const int CommitTicks = GetCommitTicks(ENwcAvoidType::Legit);
	const int MaxActions = std::clamp((int)g_Config.m_NwAvoidLegitQuality, 2, 20);
	const int AttemptsPerDir = std::clamp((int)g_Config.m_NwAvoidLegitAttemptsPerDirection, 1, 3);
	const int BiasDir = GetDangerBiasDirection(Context, 32.0f);
	const int VelAwayDir = Context.m_LocalVel.x >= 0.0f ? -1 : 1;
	const bool PreferBias = g_Config.m_NwAvoidLegitDirectionPriority >= 50;
	const std::array<int, 3> aPrimaryDirs = PreferBias ?
		std::array<int, 3>{BiasDir != 0 ? BiasDir : VelAwayDir, 0, BiasDir != 0 ? -BiasDir : -VelAwayDir} :
		std::array<int, 3>{0, BiasDir != 0 ? BiasDir : VelAwayDir, BiasDir != 0 ? -BiasDir : -VelAwayDir};

	for(int Dir : aPrimaryDirs)
	{
		for(int Attempt = 0; Attempt < AttemptsPerDir; ++Attempt)
		{
			SNwcAvoidAction Action = BuildBaseAction(Context);
			Action.m_Input.m_Direction = Dir;
			Action.m_Id = 10 + Dir * 10 + Attempt;
			Action.m_CommitTicks = CommitTicks;
			if(Attempt > 0 && Context.m_Hooking)
				Action.m_Input.m_Hook = 0;
			PushUniqueAction(vActions, Action);
			if((int)vActions.size() >= MaxActions)
				break;
		}
		if((int)vActions.size() >= MaxActions)
			break;
	}

	BuildHookAssistActions(Context, vActions, CommitTicks, MaxActions);
	SNwcAvoidCandidate BestCandidate = BaseCandidate;
	for(const SNwcAvoidAction &Action : vActions)
	{
		SNwcAvoidCandidate Candidate = EvaluateAction(Context, Action, GetPredictTicks(ENwcAvoidType::Legit), false);
		if(CompareCandidates(Candidate, BestCandidate) > 0)
			BestCandidate = Candidate;
	}

	if(!IsMeaningfullyBetter(BestCandidate, BaseCandidate))
		return false;

	OutCandidate = EvaluateAction(Context, BestCandidate.m_Action, GetPredictTicks(ENwcAvoidType::Legit), g_Config.m_NwAvoidDebug != 0);
	return true;
}

bool CNwcAvoid::SelectBlatantCandidate(const SNwcAvoidContext &Context, const SNwcAvoidCandidate &BaseCandidate, SNwcAvoidCandidate &OutCandidate)
{
	(void)BaseCandidate;
	CGameWorld *pSourceWorld = GameClient()->Predict() ? &GameClient()->m_PredictedWorld : &GameClient()->m_GameWorld;
	CCharacter *pCharacter = pSourceWorld->GetCharacterById(Context.m_LocalId);
	if(!pCharacter || ZzAvoidIsFrozenState(pCharacter))
		return false;

	const int LookAheadTicks = std::clamp(ZZ_BLATANT_AVOID_BASE_TICKS + round_to_int(length(pCharacter->Core()->m_Vel) / 4.0f), ZZ_BLATANT_AVOID_BASE_TICKS, ZZ_BLATANT_AVOID_MAX_TICKS);
	const vec2 StartPos = pCharacter->GetPos();
	const bool Grounded = pCharacter->IsGrounded();
	const int AvailableJumps = pCharacter->Core()->m_Jumps;
	const int JumpedFlags = pCharacter->Core()->m_Jumped;
	const bool HasAirJumpAvailable = !Grounded && !(JumpedFlags & 2);
	const bool LastJumpState = (Grounded && AvailableJumps == 1) || (HasAirJumpAvailable && AvailableJumps <= 2);
	const float CeilingProbeDist = pCharacter->GetProximityRadius() + 24.0f;
	const bool DangerAboveImmediate = ZzAvoidProbeDanger(Collision(), StartPos, vec2(0.0f, -CeilingProbeDist));

	CNetObj_PlayerInput OutputInput = Context.m_BaseInput;
	bool HasOutput = false;
	if(Context.m_BaseInput.m_Jump != 0 && LastJumpState)
	{
		CNetObj_PlayerInput NoJumpInput = Context.m_BaseInput;
		NoJumpInput.m_Jump = 0;

		int JumpDangerTick = -1;
		int NoJumpDangerTick = -1;
		vec2 JumpDangerPos(0.0f, 0.0f);
		vec2 NoJumpDangerPos(0.0f, 0.0f);
		const int LastJumpCheckTicks = minimum(ZZ_BLATANT_AVOID_LAST_JUMP_CHECK_TICKS, maximum(LookAheadTicks, ZZ_BLATANT_AVOID_JUMP_CHECK_TICKS + 4));
		const bool JumpDanger = SimulateZzBlatantAvoid(pSourceWorld, Context.m_LocalId, Context.m_BaseInput, LastJumpCheckTicks, nullptr, &JumpDangerTick, &JumpDangerPos);
		const bool NoJumpDanger = SimulateZzBlatantAvoid(pSourceWorld, Context.m_LocalId, NoJumpInput, LastJumpCheckTicks, nullptr, &NoJumpDangerTick, &NoJumpDangerPos);
		const float JumpDangerDist = distance(StartPos, JumpDangerPos);
		const float NoJumpDangerDist = distance(StartPos, NoJumpDangerPos);

		if(JumpDanger && (!NoJumpDanger || ZzAvoidIsFallbackImprovement(JumpDangerTick, JumpDangerDist, NoJumpDangerTick, NoJumpDangerDist)))
		{
			OutputInput = NoJumpInput;
			HasOutput = true;
		}
	}

	if(!HasOutput && Context.m_BaseInput.m_Jump != 0 && DangerAboveImmediate)
	{
		CNetObj_PlayerInput NoJumpInput = Context.m_BaseInput;
		NoJumpInput.m_Jump = 0;

		int JumpDangerTick = -1;
		int NoJumpDangerTick = -1;
		vec2 JumpDangerPos(0.0f, 0.0f);
		vec2 NoJumpDangerPos(0.0f, 0.0f);
		const int CeilingJumpCheckTicks = minimum(ZZ_BLATANT_AVOID_LAST_JUMP_CHECK_TICKS, maximum(LookAheadTicks, ZZ_BLATANT_AVOID_JUMP_CHECK_TICKS + 6));
		const bool JumpDanger = SimulateZzBlatantAvoid(pSourceWorld, Context.m_LocalId, Context.m_BaseInput, CeilingJumpCheckTicks, nullptr, &JumpDangerTick, &JumpDangerPos);
		const bool NoJumpDanger = SimulateZzBlatantAvoid(pSourceWorld, Context.m_LocalId, NoJumpInput, CeilingJumpCheckTicks, nullptr, &NoJumpDangerTick, &NoJumpDangerPos);
		const float JumpDangerDist = distance(StartPos, JumpDangerPos);
		const float NoJumpDangerDist = distance(StartPos, NoJumpDangerPos);

		if(JumpDanger && (!NoJumpDanger || ZzAvoidIsFallbackImprovement(JumpDangerTick, JumpDangerDist, NoJumpDangerTick, NoJumpDangerDist)))
		{
			OutputInput = NoJumpInput;
			HasOutput = true;
		}
	}

	if(!HasOutput)
	{
		int DangerTick = -1;
		vec2 FirstDangerPos(0.0f, 0.0f);
		if(!SimulateZzBlatantAvoid(pSourceWorld, Context.m_LocalId, Context.m_BaseInput, LookAheadTicks, nullptr, &DangerTick, &FirstDangerPos))
			return false;

		vec2 DangerDir = pCharacter->Core()->m_Vel;
		if(length(DangerDir) < 0.001f)
			DangerDir = FirstDangerPos - pCharacter->GetPos();
		const float DangerLen = length(DangerDir);
		if(DangerLen < 0.001f)
			return false;
		DangerDir /= DangerLen;
		const vec2 CursorDir = ZzAvoidNormalizedOrZero(vec2((float)Context.m_BaseInput.m_TargetX, (float)Context.m_BaseInput.m_TargetY));
		const float BaseDangerDist = distance(StartPos, FirstDangerPos);
		const bool OneTileMode = DangerTick <= ZZ_BLATANT_AVOID_ONE_TILE_TICKS || BaseDangerDist <= ZZ_BLATANT_AVOID_ONE_TILE_DIST;
		const bool HasCurrentHook = Context.m_BaseInput.m_Hook != 0 && (pCharacter->Core()->m_HookState == HOOK_FLYING || pCharacter->Core()->m_HookState == HOOK_GRABBED) &&
			distance(StartPos, pCharacter->Core()->m_HookPos) > 1.0f;
		const vec2 CurrentHookDir = HasCurrentHook ? normalize(pCharacter->Core()->m_HookPos - StartPos) : vec2(0.0f, 0.0f);
		const bool CurrentHookIntoDanger = HasCurrentHook && dot(CurrentHookDir, DangerDir) > 0.2f;
		const float TriggerDist = minimum(ZZ_BLATANT_AVOID_TRIGGER_MAX_DIST, maximum(ZZ_BLATANT_AVOID_TRIGGER_DIST, length(pCharacter->Core()->m_Vel) * 1.75f));
		if(DangerTick > ZZ_BLATANT_AVOID_TRIGGER_TICKS && distance(StartPos, FirstDangerPos) > TriggerDist)
			return false;

		const int SimTicks = std::clamp(DangerTick + ZZ_BLATANT_AVOID_VALIDATE_EXTRA_TICKS, ZZ_BLATANT_AVOID_MIN_TICKS, ZZ_BLATANT_AVOID_MAX_TICKS);
		CNetObj_PlayerInput WorkingInput = Context.m_BaseInput;
		bool HasSafeCandidate = false;
		CNetObj_PlayerInput BestSafeInput = Context.m_BaseInput;
		float BestSafeScore = 0.0f;
		bool BestSafeUsesNewHook = false;
		bool BestSafeKeepsCurrentHook = false;
		SZzAvoidFallback BestFallback;

		auto TryStoreSafeCandidate = [&](const CNetObj_PlayerInput &FinalInput, float CandidateScore, bool UsesNewHook, bool KeepsCurrentHook) {
			if(!HasSafeCandidate)
			{
				HasSafeCandidate = true;
				BestSafeInput = FinalInput;
				BestSafeScore = CandidateScore;
				BestSafeUsesNewHook = UsesNewHook;
				BestSafeKeepsCurrentHook = KeepsCurrentHook;
				return true;
			}

			if(OneTileMode && !CurrentHookIntoDanger)
			{
				if(!UsesNewHook && BestSafeUsesNewHook && CandidateScore <= BestSafeScore + 6.0f)
				{
					BestSafeInput = FinalInput;
					BestSafeScore = CandidateScore;
					BestSafeUsesNewHook = false;
					BestSafeKeepsCurrentHook = KeepsCurrentHook;
					return true;
				}
				if(KeepsCurrentHook && !BestSafeKeepsCurrentHook && CandidateScore <= BestSafeScore + 4.0f)
				{
					BestSafeInput = FinalInput;
					BestSafeScore = CandidateScore;
					BestSafeUsesNewHook = UsesNewHook;
					BestSafeKeepsCurrentHook = true;
					return true;
				}
				if(!KeepsCurrentHook && BestSafeKeepsCurrentHook && CandidateScore >= BestSafeScore - 4.0f)
					return false;
			}

			if(CandidateScore < BestSafeScore)
			{
				BestSafeInput = FinalInput;
				BestSafeScore = CandidateScore;
				BestSafeUsesNewHook = UsesNewHook;
				BestSafeKeepsCurrentHook = KeepsCurrentHook;
				return true;
			}
			return false;
		};

		auto RegisterCandidate = [&](const CNetObj_PlayerInput &CandidateInput, float CandidateScore, const vec2 *pHookPos = nullptr) {
			int CandidateDangerTick = -1;
			vec2 CandidateDangerPos(0.0f, 0.0f);
			vec2 CandidateFinalPos = StartPos;
			const bool HasDanger = SimulateZzBlatantAvoid(pSourceWorld, Context.m_LocalId, CandidateInput, SimTicks, pHookPos, &CandidateDangerTick, &CandidateDangerPos, &CandidateFinalPos);

			CNetObj_PlayerInput FinalInput = CandidateInput;
			if(pHookPos)
			{
				FinalInput.m_Hook = 1;
				const vec2 Aim = *pHookPos - StartPos;
				FinalInput.m_TargetX = round_to_int(Aim.x);
				FinalInput.m_TargetY = round_to_int(Aim.y);
				if(FinalInput.m_TargetX == 0 && FinalInput.m_TargetY == 0)
					FinalInput.m_TargetX = 1;
			}
			const bool UsesNewHook = pHookPos != nullptr;
			const bool KeepsCurrentHook = !UsesNewHook && HasCurrentHook && CandidateInput.m_Hook != 0;
			float AdjustedScore = CandidateScore;
			if(OneTileMode)
			{
				const vec2 Travel = CandidateFinalPos - StartPos;
				const float EscapeProgress = dot(Travel, -DangerDir);
				AdjustedScore -= EscapeProgress * (ZZ_BLATANT_AVOID_ESCAPE_PROGRESS_SCORE * 1.7f);

				if(length(CursorDir) > 0.001f)
				{
					const float CursorProgress = dot(Travel, CursorDir);
					AdjustedScore -= CursorProgress * (ZZ_BLATANT_AVOID_ONE_TILE_CURSOR_PROGRESS_SCORE * 0.85f);
					if(CursorProgress < 0.0f && EscapeProgress < 20.0f)
						AdjustedScore += -CursorProgress * ZZ_BLATANT_AVOID_ONE_TILE_BACKTRACK_SCORE;
				}

				if(UsesNewHook && !CurrentHookIntoDanger)
					AdjustedScore += ZZ_BLATANT_AVOID_NEW_HOOK_SAFE_PENALTY * 0.35f;
				if(KeepsCurrentHook && !CurrentHookIntoDanger)
					AdjustedScore -= ZZ_BLATANT_AVOID_KEEP_HOOK_SAFE_BONUS * 0.55f;
			}

			if(!HasDanger)
			{
				TryStoreSafeCandidate(FinalInput, AdjustedScore, UsesNewHook, KeepsCurrentHook);
				return;
			}

			const float CandidateDangerDist = distance(StartPos, CandidateDangerPos);
			if(!ZzAvoidIsFallbackImprovement(DangerTick, BaseDangerDist, CandidateDangerTick, CandidateDangerDist))
				return;

			if(ZzAvoidIsBetterFallback(CandidateDangerTick, CandidateDangerDist, AdjustedScore, BestFallback))
			{
				BestFallback.m_Valid = true;
				BestFallback.m_Input = FinalInput;
				BestFallback.m_Score = AdjustedScore;
				BestFallback.m_DangerTick = CandidateDangerTick;
				BestFallback.m_DangerDist = CandidateDangerDist;
			}
		};

		if(Context.m_BaseInput.m_Hook != 0)
		{
			RegisterCandidate(Context.m_BaseInput, CurrentHookIntoDanger ? 0.0f : -0.35f);
			CNetObj_PlayerInput ReleaseInput = Context.m_BaseInput;
			ReleaseInput.m_Hook = 0;
			RegisterCandidate(ReleaseInput, OneTileMode ? -0.35f : -0.25f);
			if(CurrentHookIntoDanger)
				WorkingInput = ReleaseInput;
		}

		std::vector<SZzAvoidInputCandidate> vInputCandidates;
		vInputCandidates.reserve(6);
		const bool AllowAutoJump = g_Config.m_NwAvoidBlatantAutoJump != 0;

		auto AddInputCandidate = [&](int Direction, int Jump) {
			if(Jump != 0 && !AllowAutoJump)
				return;

			CNetObj_PlayerInput Candidate = WorkingInput;
			Candidate.m_Direction = Direction;
			Candidate.m_Jump = Jump;

			if(Jump != 0)
			{
				CNetObj_PlayerInput NonJumpCandidate = Candidate;
				NonJumpCandidate.m_Jump = 0;
				int JumpDangerTick = -1;
				int NonJumpDangerTick = -1;
				vec2 DummyPos(0.0f, 0.0f);
				const int JumpCheckTicks = LastJumpState ? SimTicks : minimum(SimTicks, maximum(ZZ_BLATANT_AVOID_JUMP_CHECK_TICKS, DangerTick + 2));
				const bool JumpDanger = SimulateZzBlatantAvoid(pSourceWorld, Context.m_LocalId, Candidate, JumpCheckTicks, nullptr, &JumpDangerTick, &DummyPos);
				const bool NonJumpDanger = SimulateZzBlatantAvoid(pSourceWorld, Context.m_LocalId, NonJumpCandidate, JumpCheckTicks, nullptr, &NonJumpDangerTick, &DummyPos);
				if(JumpDanger && (LastJumpState || !NonJumpDanger || JumpDangerTick <= NonJumpDangerTick + 1))
					return;
			}

			if(Candidate.m_Direction == WorkingInput.m_Direction && Candidate.m_Jump == WorkingInput.m_Jump)
				return;

			for(const SZzAvoidInputCandidate &Existing : vInputCandidates)
			{
				if(Existing.m_Input.m_Direction == Candidate.m_Direction && Existing.m_Input.m_Jump == Candidate.m_Jump)
					return;
			}

			float Score = ZzAvoidInputScore(Direction, Jump != 0, DangerDir, CursorDir, 1.0f, Context.m_BaseInput.m_Direction, Context.m_BaseInput.m_Jump != 0);
			if(Jump != 0 && !AllowAutoJump)
				Score += 100000.0f;
			vInputCandidates.push_back({Candidate, Score});
		};

		AddInputCandidate(0, 0);
		AddInputCandidate(-1, 0);
		AddInputCandidate(1, 0);
		const bool ShouldTryJump = AllowAutoJump && (Context.m_BaseInput.m_Jump != 0 || ((pCharacter->Core()->m_Vel.y > 4.0f || DangerDir.y > 0.25f) && DangerTick <= ZZ_BLATANT_AVOID_JUMP_CHECK_TICKS));
		if(ShouldTryJump)
		{
			AddInputCandidate(0, 1);
			AddInputCandidate(-1, 1);
			AddInputCandidate(1, 1);
		}

		std::sort(vInputCandidates.begin(), vInputCandidates.end());

		for(const SZzAvoidInputCandidate &Candidate : vInputCandidates)
			RegisterCandidate(Candidate.m_Input, Candidate.m_Score);

		if(HasSafeCandidate && OneTileMode && (!BestSafeUsesNewHook || BestSafeKeepsCurrentHook))
		{
			OutputInput = BestSafeInput;
			HasOutput = true;
		}

		if(!HasOutput)
		{
			const float HookLength = (float)pCharacter->GetTuning()->m_HookLength;
			const int TileRadius = round_to_int(std::ceil((HookLength + 32.0f) / 32.0f));
			const int CenterX = round_to_int(StartPos.x) / 32;
			const int CenterY = round_to_int(StartPos.y) / 32;
			const int Width = Collision()->GetWidth();
			const int Height = Collision()->GetHeight();

			std::vector<SZzAvoidHookCandidate> vHookCandidates;
			vHookCandidates.reserve((TileRadius * 2 + 1) * (TileRadius * 2 + 1));

			for(int y = maximum(0, CenterY - TileRadius); y <= minimum(Height - 1, CenterY + TileRadius); y++)
			{
				for(int x = maximum(0, CenterX - TileRadius); x <= minimum(Width - 1, CenterX + TileRadius); x++)
				{
					const vec2 TilePos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
					if(distance(StartPos, TilePos) > HookLength + 32.0f)
						continue;

					vec2 HookPos;
					vec2 BeforeCollision;
					const int Hit = Collision()->IntersectLineTeleHook(StartPos, TilePos, &HookPos, &BeforeCollision);
					if(Hit == 0 || Hit == TILE_NOHOOK)
						continue;

					const float HookDist = distance(StartPos, HookPos);
					const float MinHookDist = OneTileMode ? ZZ_BLATANT_AVOID_ONE_TILE_MIN_HOOK_DIST : ZZ_BLATANT_AVOID_MIN_HOOK_DIST;
					if(HookDist < MinHookDist || HookDist > HookLength + 4.0f)
						continue;

					const vec2 ToHookDir = normalize(HookPos - StartPos);
					if(OneTileMode && length(CursorDir) > 0.001f)
					{
						const float HookCursorDot = dot(ToHookDir, CursorDir);
						const float HookEscapeDot = dot(ToHookDir, -DangerDir);
						if(HookCursorDot < -0.22f && HookEscapeDot < 0.45f)
							continue;
						if(CursorDir.y < -0.15f && HookPos.y > StartPos.y + 10.0f && HookEscapeDot < 0.58f)
							continue;
						if(CursorDir.y > 0.15f && HookPos.y < StartPos.y - 10.0f && HookEscapeDot < 0.58f)
							continue;
					}

					vHookCandidates.push_back({HookPos, ZzAvoidHookScore(StartPos, HookPos, DangerDir, CursorDir, 1.0f)});
				}
			}

			const bool AllowHookSearch = !Grounded || OneTileMode || CurrentHookIntoDanger || Context.m_BaseInput.m_Hook != 0;
			if(AllowHookSearch && !vHookCandidates.empty())
			{
				std::sort(vHookCandidates.begin(), vHookCandidates.end());

				std::vector<vec2> vAcceptedHookPositions;
				const int MaxHookCandidates = OneTileMode ? 48 : ZZ_BLATANT_AVOID_MAX_CANDIDATES;
				vAcceptedHookPositions.reserve(MaxHookCandidates);

				for(const SZzAvoidHookCandidate &Candidate : vHookCandidates)
				{
					bool Duplicate = false;
					for(const vec2 &Existing : vAcceptedHookPositions)
					{
						if(distance(Existing, Candidate.m_HookPos) < ZZ_BLATANT_AVOID_DUPLICATE_DIST)
						{
							Duplicate = true;
							break;
						}
					}
					if(Duplicate)
						continue;

					vAcceptedHookPositions.push_back(Candidate.m_HookPos);
					if((int)vAcceptedHookPositions.size() >= MaxHookCandidates)
						break;

					RegisterCandidate(WorkingInput, Candidate.m_Score, &Candidate.m_HookPos);
					if(OneTileMode)
					{
						int HookVariants = 0;
						for(const SZzAvoidInputCandidate &InputCandidate : vInputCandidates)
						{
							CNetObj_PlayerInput HookMoveInput = InputCandidate.m_Input;
							HookMoveInput.m_Hook = 0;
							RegisterCandidate(HookMoveInput, Candidate.m_Score + InputCandidate.m_Score * 0.7f, &Candidate.m_HookPos);
							HookVariants++;
							if(HookVariants >= 4)
								break;
						}
					}
				}
			}

			if(HasSafeCandidate)
			{
				OutputInput = BestSafeInput;
				HasOutput = true;
			}

			if(!HasOutput && BestFallback.m_Valid)
			{
				OutputInput = BestFallback.m_Input;
				HasOutput = true;
			}
		}
	}

	if(!HasOutput)
		return false;

	SNwcAvoidAction Action = BuildBaseAction(Context);
	Action.m_Input = OutputInput;
	Action.m_Id = 1000;
	Action.m_ForceAim = false;
	Action.m_AimDir = GetAimDir(OutputInput);
	Action.m_UsesHookAssist = false;
	Action.m_CommitTicks = 0;

	OutCandidate = EvaluateAction(Context, Action, LookAheadTicks, g_Config.m_NwAvoidDebug != 0);
	if(!OutCandidate.m_Valid)
	{
		OutCandidate = {};
		OutCandidate.m_Action = Action;
		OutCandidate.m_Valid = true;
	}

	return true;
}

bool CNwcAvoid::SelectHorizontalCandidate(const SNwcAvoidContext &Context, const SNwcAvoidCandidate &BaseCandidate, SNwcAvoidCandidate &OutCandidate)
{
	if(!IsHorizontalThreat(Context))
		return false;

	const int CommitTicks = GetCommitTicks(ENwcAvoidType::Horizontal);
	const int CounterDir = Context.m_LocalVel.x > 0.25f ? -1 : (Context.m_LocalVel.x < -0.25f ? 1 : -GetDangerBiasDirection(Context, 24.0f));
	const std::array<int, 3> aDirs = {CounterDir, 0, -CounterDir};

	SNwcAvoidCandidate BestCandidate = BaseCandidate;
	for(int Dir : aDirs)
	{
		SNwcAvoidAction Action = BuildBaseAction(Context);
		Action.m_Input.m_Direction = Dir;
		Action.m_Id = 200 + Dir;
		Action.m_CommitTicks = CommitTicks;
		if(Context.m_Hooking && Dir == 0)
			Action.m_Input.m_Hook = 0;

		SNwcAvoidCandidate Candidate = EvaluateAction(Context, Action, GetPredictTicks(ENwcAvoidType::Horizontal), false);
		Candidate.m_Score -= std::fabs(Candidate.m_EndVel.x) * 35.0f;
		if((Dir > 0 && Context.m_LocalVel.x < 0.0f) || (Dir < 0 && Context.m_LocalVel.x > 0.0f))
			Candidate.m_Score += 40.0f;
		if(CompareCandidates(Candidate, BestCandidate) > 0)
			BestCandidate = Candidate;

		if(ShouldAllowJump(ENwcAvoidType::Horizontal))
		{
			SNwcAvoidAction JumpAction = Action;
			JumpAction.m_Input.m_Jump = 1;
			JumpAction.m_Id += 10;
			Candidate = EvaluateAction(Context, JumpAction, GetPredictTicks(ENwcAvoidType::Horizontal), false);
			Candidate.m_Score -= std::fabs(Candidate.m_EndVel.x) * 28.0f;
			Candidate.m_Score += 18.0f;
			if(CompareCandidates(Candidate, BestCandidate) > 0)
				BestCandidate = Candidate;
		}
	}

	if(!IsMeaningfullyBetter(BestCandidate, BaseCandidate))
		return false;

	OutCandidate = EvaluateAction(Context, BestCandidate.m_Action, GetPredictTicks(ENwcAvoidType::Horizontal), g_Config.m_NwAvoidDebug != 0);
	return true;
}

bool CNwcAvoid::SelectBestCandidate(const SNwcAvoidContext &Context, SNwcAvoidCandidate &OutCandidate)
{
	const ENwcAvoidType Type = GetConfiguredType();
	if(Type == ENwcAvoidType::Blatant)
	{
		SNwcAvoidCandidate BaseCandidate;
		return SelectBlatantCandidate(Context, BaseCandidate, OutCandidate);
	}

	const int PredictTicks = GetPredictTicks(Type);
	const SNwcAvoidCandidate BaseCandidate = EvaluateAction(Context, BuildBaseAction(Context), PredictTicks, g_Config.m_NwAvoidDebug != 0);
	if(!BaseCandidate.m_HasHazard)
		return false;

	switch(Type)
	{
	case ENwcAvoidType::Legit:
		return SelectLegitCandidate(Context, BaseCandidate, OutCandidate);
	case ENwcAvoidType::Horizontal:
		return SelectHorizontalCandidate(Context, BaseCandidate, OutCandidate);
	case ENwcAvoidType::Blatant:
	case ENwcAvoidType::Count:
		break;
	}
	return false;
}

