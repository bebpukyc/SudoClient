#include "botsymona.h"

#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <algorithm>
#include <cmath>

void CBotSymona::DropPlan()
{
	m_Plan.m_PreDir = 0;
	m_Plan.m_PreUntil = -1;
	m_Plan.m_Dir = 0;
	m_Plan.m_JumpAt = -1;
	m_Plan.m_AirJumpAt = -1;
	m_PlanUntil = -1;
	m_PlanCommitUntil = -1;
	m_PlanSafe = false;
	m_PlanCross = false;
	m_PlanGoal = vec2(0.0f, 0.0f);
	m_ProgressTick = -1;
	m_BestGoalDist = 1e9f;
}

CBotSymona::SSimResult CBotSymona::Simulate(const CCharacterCore &Start, const SAction &Action, vec2 Goal, int Ticks) const
{
	CCharacterCore Core = Start;
	Core.SetCoreWorld(nullptr, Collision(), nullptr);

	const bool UsesHook = Action.m_HookFrom >= 0;
	Core.m_HookState = HOOK_IDLE;
	Core.SetHookedPlayer(-1);
	Core.m_HookPos = Core.m_Pos;
	Core.m_HookTick = 0;

	CNetObj_PlayerInput In;
	mem_zero(&In, sizeof(In));
	In.m_PlayerFlags = PLAYERFLAG_PLAYING;
	In.m_TargetX = round_to_int(Action.m_Aim.x);
	In.m_TargetY = round_to_int(Action.m_Aim.y);
	if(In.m_TargetX == 0 && In.m_TargetY == 0)
		In.m_TargetX = 1;

	SSimResult Res;
	Res.m_Frozen = false;
	Res.m_FreezeTick = Ticks + 1;
	Res.m_EndPos = Core.m_Pos;
	Res.m_MinGoalDist = distance(Core.m_Pos, Goal);
	Res.m_EndGoalDist = Res.m_MinGoalDist;
	Res.m_MinGoalTick = 0;
	Res.m_BrushTicks = 0;
	Res.m_Airborne = false;

	const int HardCap = std::max(Ticks, 96);

	bool Arrived = false;

	for(int t = 1; t <= HardCap; t++)
	{
		In.m_Direction = Arrived ? 0 : (t <= Action.m_PreTicks ? Action.m_PreDir : Action.m_Dir);
		In.m_Jump = !Arrived && (t == Action.m_JumpAt || t == Action.m_AirJumpAt) ? 1 : 0;
		In.m_Hook = UsesHook && t >= Action.m_HookFrom && t <= Action.m_HookUntil ? 1 : 0;
		Core.m_Input = In;

		const vec2 Prev = Core.m_Pos;
		Core.Tick(true, false);
		Core.Move();
		Core.Quantize();

		if(SweepFreeze(Prev, Core.m_Pos))
		{
			Res.m_Frozen = true;
			Res.m_FreezeTick = t;
			Res.m_EndPos = Core.m_Pos;
			Res.m_EndGoalDist = distance(Core.m_Pos, Goal);
			return Res;
		}

		const float D = distance(Core.m_Pos, Goal);
		if(D < Res.m_MinGoalDist)
		{
			Res.m_MinGoalDist = D;
			Res.m_MinGoalTick = t;
		}
		if(!UsesHook && D < 12.0f)
			Arrived = true;
		if(Arrived && absolute(Core.m_Vel.x) < 1.0f &&
			Collision()->IsOnGround(Core.m_Pos, CCharacterCore::PhysicalSize()))
		{
			Res.m_EndPos = Core.m_Pos;
			Res.m_EndGoalDist = D;
			return Res;
		}

		const float Ahead = In.m_Direction != 0 ? 22.0f * (float)In.m_Direction : 0.0f;
		if(FreezeAt(Core.m_Pos + vec2(Ahead, 0.0f)) ||
			FreezeAt(Core.m_Pos + vec2(0.0f, 22.0f)) ||
			FreezeAt(Core.m_Pos + vec2(Ahead, 22.0f)))
			Res.m_BrushTicks++;

		if(t >= Ticks && Collision()->IsOnGround(Core.m_Pos, CCharacterCore::PhysicalSize()))
			break;
	}

	Res.m_EndPos = Core.m_Pos;
	Res.m_EndGoalDist = distance(Core.m_Pos, Goal);
	Res.m_Airborne = !Collision()->IsOnGround(Core.m_Pos, CCharacterCore::PhysicalSize());
	return Res;
}

float CBotSymona::ScoreAction(const SSimResult &Res, const SAction &Action, float StartDist) const
{
	if(Res.m_Frozen)
	{
		float Score = -100000.0f + Res.m_FreezeTick * 500.0f;
		if(!g_Config.m_BotAvoidFreeze)
			Score += 2000.0f - Res.m_MinGoalDist * 4.0f;
		return Score;
	}

	float Score = 10000.0f + (StartDist - Res.m_MinGoalDist) * 6.0f - Res.m_EndGoalDist * 1.5f;

	if(Res.m_Airborne)
		Score -= 1500.0f;

	Score -= 12.0f * (float)Res.m_BrushTicks;

	Score += 3000.0f * std::max(0.0f, 1.0f - Res.m_MinGoalDist / 96.0f);

		if(Action.m_JumpAt >= 0)
		Score -= std::max(0.0f, 120.0f - m_JumpBias);
	if(Action.m_AirJumpAt >= 0)
		Score -= std::max(0.0f, 200.0f - m_JumpBias);

	return Score;
}

bool CBotSymona::SearchDirect(const CCharacterCore &Start, vec2 Goal, float StartDist, SAction *pOut, float *pScore) const
{
	static const int s_aDirs[3] = {0, -1, 1};
	static const int s_aJumpAt[6] = {-1, 1, 4, 8, 13, 19};

	const int Horizon = PredictHorizon();
	const bool AllowJump = g_Config.m_BotCrossGaps != 0;

	float BestScore = -1e9f;
	SAction Best = {0, 0, 0, -1, -1};
	bool AnySafe = false;

	for(int d = 0; d < 3; d++)
	{
		for(int j = 0; j < 6; j++)
		{
			const int JumpAt = s_aJumpAt[j];
			if(JumpAt > 1 && !AllowJump)
				continue;

			for(int a = 0; a < 2; a++)
			{
				SAction Action;
				Action.m_PreDir = 0;
				Action.m_PreTicks = 0;
				Action.m_Dir = s_aDirs[d];
				Action.m_JumpAt = JumpAt;
				Action.m_AirJumpAt = -1;
				if(a == 1)
				{
					if(JumpAt < 0 || !AllowJump)
						continue;
					Action.m_AirJumpAt = JumpAt + 10;
					if(Action.m_AirJumpAt >= Horizon)
						continue;
				}

				const SSimResult Res = Simulate(Start, Action, Goal, Horizon);
				if(!Res.m_Frozen)
					AnySafe = true;

				float Score = ScoreAction(Res, Action, StartDist);
				if(!Res.m_Frozen && Action.m_Dir != m_Plan.m_Dir)
					Score -= 120.0f;

				if(Score > BestScore)
				{
					BestScore = Score;
					Best = Action;
				}
			}
		}
	}

	for(int d = 1; d < 3; d++)
	{
		for(int Ticks = 8; Ticks < Horizon; Ticks += 8)
		{
			SAction Action;
			Action.m_PreDir = s_aDirs[d];
			Action.m_PreTicks = Ticks;
			Action.m_Dir = 0;
			Action.m_JumpAt = -1;
			Action.m_AirJumpAt = -1;

			const SSimResult Res = Simulate(Start, Action, Goal, Horizon);
			if(Res.m_Frozen)
				continue;
			AnySafe = true;

			const float Score = ScoreAction(Res, Action, StartDist);
			if(Score > BestScore)
			{
				BestScore = Score;
				Best = Action;
			}
		}
	}

	*pOut = Best;
	*pScore = BestScore;
	return AnySafe;
}

void CBotSymona::Steer(const CCharacterCore &Start, vec2 Goal, int PredTick, CNetObj_PlayerInput *pInput)
{
	const int Interval = 1;
	const float StartDist = distance(Start.m_Pos, Goal);
	const bool Grounded = Collision()->IsOnGround(Start.m_Pos, CCharacterCore::PhysicalSize());

	if(StartDist < 12.0f && Grounded && absolute(Start.m_Vel.x) < 2.0f)
	{
		DropPlan();
		m_PlanSafe = true;
		m_PlanGoal = Goal;
		m_BestGoalDist = StartDist;
		m_ProgressTick = PredTick;
		pInput->m_Direction = 0;
		pInput->m_Jump = 0;
		m_JumpHeld = false;
		return;
	}

	if(StartDist < m_BestGoalDist - 8.0f || distance(Goal, m_PlanGoal) > 48.0f ||
		StartDist <= 48.0f || m_ProgressTick < 0)
	{
		m_BestGoalDist = StartDist;
		m_ProgressTick = PredTick;
	}

	bool Replan = PredTick >= m_PlanUntil || distance(Goal, m_PlanGoal) > 48.0f;
	if(m_PlanSafe && PredTick < m_PlanCommitUntil)
		Replan = false;
	if(m_PlanCross && Grounded && m_Plan.m_JumpAt < 0 && m_Plan.m_AirJumpAt < 0 && m_Plan.m_PreUntil <= PredTick)
		Replan = true;
	if(m_PlanSafe && !Grounded && m_Plan.m_AirJumpAt >= PredTick)
		Replan = false;

	if(!Replan && m_PlanSafe && m_PlanCross)
	{
		SAction Tail;
		Tail.m_PreDir = m_Plan.m_PreDir;
		Tail.m_PreTicks = std::max(0, m_Plan.m_PreUntil - PredTick);
		Tail.m_Dir = m_Plan.m_Dir;
		Tail.m_JumpAt = m_Plan.m_JumpAt >= 0 ? m_Plan.m_JumpAt - PredTick + 1 : -1;
		Tail.m_AirJumpAt = m_Plan.m_AirJumpAt >= 0 ? m_Plan.m_AirJumpAt - PredTick + 1 : -1;

		if(Simulate(Start, Tail, Goal, std::max(20, m_PlanCommitUntil - PredTick)).m_Frozen)
		{
			DropPlan();
			Replan = true;
		}
	}

	if(Replan)
	{
		SAction Direct;
		float DirectScore = -1e9f;
		const bool DirectSafe = SearchDirect(Start, Goal, StartDist, &Direct, &DirectScore);

		SAction Chosen = Direct;
		bool Safe = DirectSafe;
		bool Cross = false;
		int CrossLength = 0;

		const bool WantsToMove = StartDist > 48.0f;
		const bool NeedCross = WantsToMove &&
				       (!DirectSafe || PredTick - m_ProgressTick > 12);
		if(g_Config.m_BotCrossGaps && Grounded && NeedCross)
		{
			SAction Jump;
			if(PlanCrossing(Start, Goal, &Jump, &CrossLength))
			{
				Chosen = Jump;
				Safe = true;
				Cross = true;
			}
		}

		m_PlanSafe = Safe;
		m_PlanCross = Cross;
		m_Plan.m_PreDir = Chosen.m_PreDir;
		m_Plan.m_PreUntil = Chosen.m_PreTicks > 0 ? PredTick + Chosen.m_PreTicks : -1;
		m_Plan.m_Dir = Chosen.m_Dir;
		m_Plan.m_JumpAt = Chosen.m_JumpAt >= 0 ? PredTick + Chosen.m_JumpAt - 1 : -1;
		m_Plan.m_AirJumpAt = Chosen.m_AirJumpAt >= 0 ? PredTick + Chosen.m_AirJumpAt - 1 : -1;
		m_PlanGoal = Goal;

		m_PlanUntil = PredTick + Interval;
		m_PlanCommitUntil = -1;
		if(Cross)
		{
			m_PlanUntil = PredTick + CrossLength;
			m_PlanCommitUntil = m_PlanUntil;
			m_ProgressTick = PredTick;
			m_BestGoalDist = StartDist;
		}
		else
		{
			if(m_Plan.m_JumpAt >= 0)
				m_PlanUntil = std::max(m_PlanUntil, m_Plan.m_JumpAt + 1);
			if(m_Plan.m_AirJumpAt >= 0)
				m_PlanUntil = std::max(m_PlanUntil, m_Plan.m_AirJumpAt + 1);
		}
	}

	pInput->m_Direction = (m_Plan.m_PreUntil > PredTick) ? m_Plan.m_PreDir : m_Plan.m_Dir;

	const bool DueGround = m_Plan.m_JumpAt >= 0 && PredTick >= m_Plan.m_JumpAt;
	const bool DueAir = !DueGround && m_Plan.m_AirJumpAt >= 0 && PredTick >= m_Plan.m_AirJumpAt;

	if((DueGround || DueAir) && !m_JumpHeld)
	{
		pInput->m_Jump = 1;
		m_JumpHeld = true;
		if(DueGround)
			m_Plan.m_JumpAt = -1;
		else
			m_Plan.m_AirJumpAt = -1;
	}
	else
	{
		pInput->m_Jump = 0;
		m_JumpHeld = false;
	}
}
