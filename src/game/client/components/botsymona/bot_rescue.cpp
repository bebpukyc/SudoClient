#include "botsymona.h"

#include <base/log.h>
#include <base/time.h>
#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <algorithm>
#include <cmath>

bool CBotSymona::StopDriving()
{
	const bool WasDriving = m_Driving;
	m_Driving = false;
	return WasDriving;
}

bool CBotSymona::ReleaseInput(CNetObj_PlayerInput *pInput)
{
	if(!m_Driving)
		return false;
	m_Driving = false;

	const int KeepFire = pInput->m_Fire;
	mem_zero(pInput, sizeof(*pInput));
	pInput->m_PlayerFlags = PLAYERFLAG_PLAYING;
	pInput->m_Fire = KeepFire;
	pInput->m_TargetX = 1;
	return true;
}

bool CBotSymona::OnDummyInput(CNetObj_PlayerInput *pInput)
{
	if(g_Config.m_ClDummy != 0)
		return false;

	if(!g_Config.m_BotEnable)
	{
		str_copy(m_aStatus, "выключен");
		return ReleaseInput(pInput);
	}
	if(m_Paused)
	{
		str_copy(m_aStatus, "стоит (команда стоп)");
		return ReleaseInput(pInput);
	}

	g_Config.m_ClDummyCopyMoves = 0;
	g_Config.m_ClDummyControl = 0;
	g_Config.m_ClDummyHammer = 0;
	g_Config.m_ClPredictDummy = 1;

	const int KeepFire = pInput->m_Fire;
	mem_zero(pInput, sizeof(*pInput));
	pInput->m_PlayerFlags = PLAYERFLAG_PLAYING;
	pInput->m_Fire = KeepFire;
	pInput->m_TargetX = 1;

	const int BotId = BotClientId();
	if(BotId < 0)
	{
		str_copy(m_aStatus, "бота нет на сервере");
		return StopDriving();
	}

	vec2 BotPos, BotVel, OwnerPos, OwnerVel;
	if(!TeeState(BotId, &BotPos, &BotVel))
	{
		if(GameClient()->m_aClients[BotId].m_Team == TEAM_SPECTATORS &&
			(m_NextTeamJoin == 0 || time_get() > m_NextTeamJoin))
		{
			m_NextTeamJoin = time_get() + time_freq() * 5;
			CNetMsg_Cl_SetTeam Msg;
			Msg.m_Team = TEAM_RED;
			Client()->SendPackMsg(BotConn(), &Msg, MSGFLAG_VITAL);
		}
		str_copy(m_aStatus, "бот мёртв / в спеке");
		return StopDriving();
	}

	const SLimits L = Limits();
	const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);
	const CNetObj_Character &BotChar = *SnapChar(BotId);

	if(!m_HomeSet)
	{
		m_HomePos = BotPos;
		m_HomeSet = true;
	}

	int SubjectMode = SUBJ_NONE;
	const int OwnerId = FindSubject(&SubjectMode);
	if(OwnerId != m_LastOwnerId || SubjectMode != m_SubjectMode)
	{
		m_LastOwnerId = OwnerId;
		m_SubjectMode = SubjectMode;
		m_AnchorValid = false;
		m_StuckTicks = 0;
		DropPlan();
	}

	// PVP first: if nobody is waiting to be rescued, go fight
	if(SubjectMode != SUBJ_RESCUE && TryPvP(PredTick, BotId, BotPos, BotVel, pInput))
	{
		m_Driving = true;
		return true;
	}

	if(OwnerId < 0 || !TeeState(OwnerId, &OwnerPos, &OwnerVel))
	{
		if(!g_Config.m_BotRoam)
		{
			str_copy(m_aStatus, "некому помогать, стоит");
			return StopDriving();
		}

		bool RoamDeep = false;
		if(IsFrozen(BotId, &RoamDeep))
		{
			str_copy(m_aStatus, RoamDeep ? "бот в deep freeze" : "бот сам во фризе");
			m_RoamValid = false;
			return StopDriving();
		}

		if(distance(BotPos, m_LastPos) < 3.0f)
			m_StuckTicks++;
		else
			m_StuckTicks = 0;
		m_LastPos = BotPos;

		m_State = S_ROAM;
		m_JumpBias = 0.0f;

		if(!m_HomeSet || distance(BotPos, m_HomePos) > (float)g_Config.m_BotRoamRadius * 1.5f)
		{
			m_HomePos = BotPos;
			m_HomeSet = true;
			m_RoamValid = false;
		}

		const bool Arrived = m_RoamValid && distance(BotPos, m_RoamGoal) < 40.0f;
		if(Arrived || PredTick >= m_RoamUntil || m_StuckTicks > 50)
		{
			vec2 Goal;
			if(PickRoamGoal(BotPos, PredTick, &Goal))
			{
				m_RoamGoal = Goal;
				m_RoamValid = true;
				m_RoamUntil = PredTick + Client()->GameTickSpeed() * 8;
				m_StuckTicks = 0;
				DropPlan();
			}
			else
			{
				m_RoamValid = false;
				m_RoamUntil = PredTick + Client()->GameTickSpeed() / 2;
			}
		}

		CCharacterCore RoamCore;
		bool HaveRoamCore = PredCore(BotId, &RoamCore);
		if(HaveRoamCore)
		{
			RoamCore.m_HookState = HOOK_IDLE;
			RoamCore.SetHookedPlayer(-1);
		}
		else
		{
			HaveRoamCore = CoreFromSnap(BotId, &RoamCore);
		}
		if(HaveRoamCore)
		{
			BotPos = RoamCore.m_Pos;
			BotVel = RoamCore.m_Vel;
		}

		if(!m_RoamValid || !HaveRoamCore)
		{
			SetStatus("гуляет, но идти некуда (%d px от дома)", (int)distance(BotPos, m_HomePos));
			return StopDriving();
		}

		const bool RoamGrounded = Collision()->IsOnGround(BotPos, CCharacterCore::PhysicalSize());

		Steer(RoamCore, m_RoamGoal, PredTick, pInput);

		if(RoamGrounded && pInput->m_Direction != 0)
		{
			SAction Emit;
			Emit.m_Dir = pInput->m_Direction;
			Emit.m_JumpAt = pInput->m_Jump != 0 ? 1 : -1;
			const SSimResult EmitRes = Simulate(RoamCore, Emit, m_RoamGoal, 26);
			if(EmitRes.m_Frozen && EmitRes.m_FreezeTick <= 10)
			{
				pInput->m_Direction = 0;
				pInput->m_Jump = 0;
				m_JumpHeld = false;
				m_RoamValid = false;
				SetStatus("тормозит у фриза на прогулке");
			}
		}

		const float Face = pInput->m_Direction != 0 ? (float)pInput->m_Direction : (m_AimPoint.x < 0.0f ? -1.0f : 1.0f);
		const float Phase = (float)PredTick * 0.05f;
		const vec2 Want = vec2(Face * 140.0f + 26.0f * std::sin(Phase * 0.37f),
			10.0f + 34.0f * std::sin(Phase * 0.23f + 1.7f));
		m_AimPoint += (Want - m_AimPoint) * 0.12f;
		pInput->m_TargetX = round_to_int(m_AimPoint.x);
		pInput->m_TargetY = round_to_int(m_AimPoint.y);
		if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
			pInput->m_TargetX = Face > 0.0f ? 1 : -1;

		SetStatus("гуляет (%d px от дома)", (int)distance(BotPos, m_HomePos));
		m_Driving = true;
		return true;
	}

	if(m_State == S_ROAM)
	{
		m_State = S_FOLLOW;
		m_RoamValid = false;
		DropPlan();
	}

	CCharacterCore PredBot;
	m_UsingPrediction = PredCore(BotId, &PredBot);

	int HookState = BotChar.m_HookState;
	int HookedPlayer = BotChar.m_HookedPlayer;
	if(m_UsingPrediction)
	{
		BotPos = PredBot.m_Pos;
		BotVel = PredBot.m_Vel;
		HookState = PredBot.m_HookState;
		HookedPlayer = PredBot.HookedPlayer();

		CCharacterCore PredOwner;
		if(PredCore(OwnerId, &PredOwner))
		{
			OwnerPos = PredOwner.m_Pos;
			OwnerVel = PredOwner.m_Vel;
		}
	}

	bool OwnerDeep = false, BotDeep = false;
	const bool OwnerFrozen = IsFrozen(OwnerId, &OwnerDeep);
	const bool BotFrozen = IsFrozen(BotId, &BotDeep);
	const bool OwnerOnFreeze = FreezeAt(OwnerPos);

	const int OwnerState = (OwnerFrozen ? 1 : 0) | (OwnerOnFreeze ? 2 : 0) | (OwnerDeep ? 4 : 0);
	if(OwnerState != m_OwnerStateWas)
	{
		m_OwnerStateWas = OwnerState;
		m_AnchorValid = false;
		m_StuckTicks = 0;
		m_NoHookUntil = -1;
		m_NoHammerUntil = -1;
		m_HammerEnterTick = -1;
		m_EscapeUntil = -1;
		DropPlan();
	}

	const vec2 Delta = OwnerPos - BotPos;
	const float Dist = length(Delta);
	const bool Grounded = Collision()->IsOnGround(BotPos, CCharacterCore::PhysicalSize());
	const bool Hooked = HookState == HOOK_GRABBED && HookedPlayer == OwnerId;

	pInput->m_TargetX = round_to_int(Delta.x);
	pInput->m_TargetY = round_to_int(Delta.y);
	if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
		pInput->m_TargetX = 1;

	if(distance(BotPos, m_LastPos) < 3.0f)
		m_StuckTicks++;
	else
		m_StuckTicks = 0;
	m_LastPos = BotPos;

	if(BotFrozen)
	{
		m_State = S_SELF;
		m_AnchorValid = false;
		DropPlan();
		str_copy(m_aStatus, BotDeep ? "бот в deep freeze" : "бот сам во фризе");
		return StopDriving();
	}
	if(m_State == S_SELF)
	{
		m_State = S_FOLLOW;
		m_AnchorValid = false;
		m_StuckTicks = 0;
		DropPlan();
	}

	if(!GameClient()->m_Teams.CanCollide(BotId, OwnerId))
	{
		str_copy(m_aStatus, "solo / другая команда — ни хук, ни молот");
		return StopDriving();
	}

	const bool CanRescue = g_Config.m_BotRescue && OwnerFrozen && !OwnerDeep &&
			       (g_Config.m_BotUseHook || g_Config.m_BotUseHammer);
	const float HookBias = g_Config.m_BotGentle ? 60.0f : 0.0f;
	const float LetGo = g_Config.m_BotUseHammer && !OwnerOnFreeze
				    ? L.m_HammerMax * (g_Config.m_BotGentle ? 1.5f : 1.0f)
				    : 0.0f;
	const float HookMin = std::max(L.m_HookMinPull, LetGo + 16.0f);
	const bool HammerHelps = g_Config.m_BotUseHammer && (!OwnerOnFreeze || Dist <= L.m_HookMinPull);
	const int OldState = m_State;

	const bool Locomotion = m_State == S_CLIMB || m_State == S_LAUNCH || m_State == S_UNREACH;

	if(!OwnerFrozen && !Locomotion)
	{
		m_State = S_FOLLOW;
		m_AnchorValid = false;
	}
	else if(m_State == S_FOLLOW && CanRescue)
	{
		const float WalkHammer = HammerHelps && CorridorClear(BotPos, OwnerPos - normalize(Delta) * L.m_HammerMax)
						 ? WalkTicks(std::max(0.0f, Dist - L.m_HammerMax)) + 6.0f
						 : 1e9f;

		float HookDirect = 1e9f;
		if(g_Config.m_BotUseHook && Dist > HookMin && Dist <= L.m_HookReach &&
			PullSafe(BotPos, OwnerPos) &&
			HookPathToOwner(BotPos + normalize(Delta) * L.m_HookMinPull, OwnerPos, BotId, OwnerId))
			HookDirect = AnchorEta(BotPos, OwnerPos, BotPos, L);

		if(OwnerOnFreeze && HookDirect < 1e9f)
		{
			m_State = S_GOTO;
			m_AnchorValid = false;
		}
		else if(WalkHammer < 1e9f && WalkHammer <= HookDirect + HookBias && PredTick >= m_NoHammerUntil)
		{
			m_State = S_HAMMER;
		}
		else if(g_Config.m_BotUseHook)
		{
			m_State = S_GOTO;
			m_AnchorValid = false;
		}
		else
		{
			m_State = S_HAMMER;
		}
		m_HookRetries = 0;
	}
	else if(m_State != S_FOLLOW && !CanRescue && !Locomotion)
	{
		m_State = S_FOLLOW;
		m_AnchorValid = false;
	}

	if(m_State == S_PULL && !Hooked)
		m_State = S_GOTO;
	if(Hooked && m_State != S_PULL && m_ReleaseUntil <= PredTick)
	{
		m_State = S_PULL;
		m_GrabbedTick = PredTick;
		m_LastPullDist = Dist;
		m_LastProgressTick = PredTick;
		m_HookRetries = 0;
	}

	if(m_State != OldState)
	{
		DropPlan();
		m_StuckTicks = 0;
		if(m_State != S_HAMMER)
			m_HammerEnterTick = -1;
	}

	const bool RangedWorth = Dist > (float)g_Config.m_BotRangedMin ||
				 !CorridorClear(BotPos, OwnerPos - normalize(Delta) * L.m_HammerMax);
	const bool CanRescueRanged = g_Config.m_BotRescue && OwnerFrozen && !OwnerDeep &&
				     (g_Config.m_BotUseLaser || g_Config.m_BotUseShotgun);

	if((CanRescue || CanRescueRanged) && !OwnerDeep && RangedWorth &&
		m_State != S_PULL && HookState != HOOK_GRABBED && pInput->m_Hook == 0)
	{
		if(m_BeamWeapon >= 0 &&
			(distance(BotPos, m_BeamFrom) > 8.0f || distance(OwnerPos, m_BeamTarget) > 8.0f))
		{
			m_BeamWeapon = -1;
			m_BeamUntil = -1;
			m_NextBeamTick = PredTick;
		}

		if(m_BeamUntil <= PredTick && PredTick >= m_NextBeamTick)
		{
			m_NextBeamTick = PredTick + 4;
			m_BeamWeapon = -1;

			vec2 Aim;
			if(!OwnerOnFreeze && LaserAim(BotPos, OwnerPos, BotId, OwnerId, &Aim))
			{
				m_BeamWeapon = WEAPON_LASER;
				m_BeamAim = Aim;
				m_BeamUntil = PredTick + 25;
				m_BeamFrom = BotPos;
				m_BeamTarget = OwnerPos;
			}
			else if(OwnerOnFreeze && ShotgunDragAim(BotPos, OwnerPos, BotId, OwnerId, &Aim))
			{
				m_BeamWeapon = WEAPON_SHOTGUN;
				m_BeamAim = Aim;
				m_BeamUntil = PredTick + 25;
				m_BeamFrom = BotPos;
				m_BeamTarget = OwnerPos;
			}
		}

		if(m_BeamWeapon >= 0 && m_BeamUntil > PredTick && length(m_BeamAim) > 0.001f)
		{
			const int Want = m_BeamWeapon;
			const vec2 AimAt = BotPos + normalize(m_BeamAim) * 200.0f;
			if(AimAndFire(pInput, BotPos, AimAt, Want, PredTick))
			{
				m_BeamUntil = -1;
				m_BeamWeapon = -1;
				SetStatus(Want == WEAPON_LASER ? "размораживает винтовкой (%d px)" : "стягивает дробовиком (%d px)", (int)Dist);
				m_Driving = true;
				return true;
			}
			if(BotChar.m_Weapon == Want)
			{
				SetStatus(Want == WEAPON_LASER ? "целится винтовкой (%d px)" : "целится дробовиком (%d px)", (int)Dist);
				m_Driving = true;
				return true;
			}
		}
	}
	else
	{
		m_BeamWeapon = -1;
		m_BeamUntil = -1;
	}

	int ManualDir = 0;
	bool ManualJump = false;
	bool UsePlanner = false;
	bool AimPrecise = false;
	vec2 Goal = OwnerPos;

	m_JumpBias = 0.0f;
	const int Blocker = BlockerAhead(BotPos, OwnerPos, BotId);
	if(Blocker >= 0 && Blocker != OwnerId)
	{
		vec2 BlockPos, BlockVel;
		if(TeeState(Blocker, &BlockPos, &BlockVel) && distance(BotPos, BlockPos) < 160.0f)
			m_JumpBias = 140.0f;
	}

	if(CanRescue && g_Config.m_BotUseHammer && Dist <= L.m_HammerMax * 3.0f)
	{
		pInput->m_WantedWeapon = WEAPON_HAMMER + 1;
		AimPrecise = true;
	}

	CCharacterCore SimCore;
	bool HaveCore = m_UsingPrediction;
	if(HaveCore)
	{
		SimCore = PredBot;
		SimCore.m_HookState = HOOK_IDLE;
		SimCore.SetHookedPlayer(-1);
	}
	else
	{
		HaveCore = CoreFromSnap(BotId, &SimCore);
	}

	switch(m_State)
	{
	case S_FOLLOW:
	{
		const float Want = FollowDist();

		if(m_SubjectMode != SUBJ_FOLLOW)
		{
			Goal = BotPos;
			UsePlanner = true;
			SetStatus("ждёт (%d px)", (int)Dist);
			break;
		}

		const float Near = std::max(40.0f, Want - 50.0f);
		const float Far = Want + 50.0f;
		const bool OtherFloor = absolute(Delta.y) > 48.0f;
		m_FollowMoving = OtherFloor || (m_FollowMoving ? (Dist > Near) : (Dist > Far));

		if(!m_FollowMoving)
		{
			Goal = BotPos;
			UsePlanner = true;
			SetStatus("держит дистанцию (%d px)", (int)Dist);
			break;
		}

		const bool NoProgress = (m_ProgressTick >= 0 && PredTick - m_ProgressTick > 25) || m_StuckTicks > 25;
		if(g_Config.m_BotUseHook && HaveCore && Grounded && NoProgress &&
			PredTick >= m_NoHookUntil && PredTick >= m_NextHookPlanTick)
		{
			m_NextHookPlanTick = PredTick + 8;

			vec2 Grab, Land;
			int Hold;
			bool AirJump;
			if(OwnerPos.y < BotPos.y - 48.0f &&
				PlanClimb(SimCore, OwnerPos, PredTick, &Grab, &Land, &Hold, &AirJump))
			{
				m_ClimbGrab = Grab;
				m_ClimbLand = Land;
				m_ClimbHold = Hold;
				m_ClimbAirJump = AirJump;
				m_State = S_CLIMB;
				m_ClimbStart = PredTick;
				m_ClimbBestY = BotPos.y;
				m_ClimbGainTick = PredTick;
				m_HookFiredTick = -1;
				SetStatus("пешком не подняться, лезет на хуке (%d px)", (int)Dist);
				break;
			}

			if(PlanLaunch(SimCore, OwnerPos, PredTick, &m_Launch))
			{
				m_State = S_LAUNCH;
				m_LaunchStart = PredTick;
				m_LaunchStartDist = Dist;
				m_HookFiredTick = -1;
				SetStatus("догоняет владельца через хук (%d px)", (int)Dist);
				break;
			}
		}

		Goal = OwnerPos;
		UsePlanner = true;
		SetStatus("идёт к владельцу (%d px)", (int)Dist);
		break;
	}

	case S_GOTO:
	{
		if(!CanRescue)
		{
			SetStatus(OwnerDeep ? "владелец в deep freeze, молот не поможет" : "спасение выключено");
			break;
		}
		if(Dist <= HookMin && HammerHelps && PredTick >= m_NoHammerUntil)
		{
			m_State = S_HAMMER;
			break;
		}

		if(g_Config.m_BotUseHammer && !m_AnchorValid && PredTick >= m_NoHammerUntil &&
			CorridorClear(BotPos, OwnerPos - normalize(Delta) * L.m_HammerMax))
		{
			const float WalkHammer = WalkTicks(std::max(0.0f, Dist - L.m_HammerMax)) + 6.0f;
			if(WalkHammer < AnchorEta(BotPos, OwnerPos, BotPos, L) + HookBias)
			{
				m_State = S_HAMMER;
				Goal = OwnerPos;
				UsePlanner = true;
				SetStatus("пешком быстрее хука (%d px)", (int)Dist);
				break;
			}
		}

		if(g_Config.m_BotUseHook && PredTick >= m_NoHookUntil &&
			Dist > HookMin && Dist <= L.m_HookReach &&
			PullSafe(BotPos, OwnerPos) &&
			HookPathToOwner(BotPos + normalize(Delta) * L.m_HookMinPull, OwnerPos, BotId, OwnerId))
		{
			m_State = S_HOOK;
			m_CommitUntil = PredTick + 16;
			m_HookFiredTick = -1;
			m_HookRetries = 0;
			SetStatus("хукает прямо отсюда (%d px)", (int)Dist);
			break;
		}

		const int Recheck = 3;
		if(!m_AnchorValid || m_PlanTick < 0 || PredTick - m_PlanTick >= Recheck)
		{
			m_PlanTick = PredTick;
			vec2 Anchor;
			m_AnchorValid = g_Config.m_BotUseHook && FindAnchor(OwnerPos, BotPos, L.m_HookReach, PredTick, &Anchor);
			if(m_AnchorValid)
				m_Anchor = Anchor;
		}

		if(!m_AnchorValid)
		{
			const bool CanPlanHook = g_Config.m_BotUseHook && HaveCore &&
						 PredTick >= m_NoHookUntil && PredTick >= m_NextHookPlanTick;
			if(CanPlanHook)
				m_NextHookPlanTick = PredTick + 8;

			if(CanPlanHook && OwnerPos.y < BotPos.y - 48.0f &&
				PlanClimb(SimCore, OwnerPos, PredTick, &m_ClimbGrab, &m_ClimbLand, &m_ClimbHold, &m_ClimbAirJump))
			{
				m_State = S_CLIMB;
				m_ClimbStart = PredTick;
				m_ClimbBestY = BotPos.y;
				m_ClimbGainTick = PredTick;
				m_HookFiredTick = -1;
				SetStatus("цепляется за стену, лезет наверх (%d px)", (int)Dist);
				break;
			}

			if(CanPlanHook && PlanLaunch(SimCore, OwnerPos, PredTick, &m_Launch))
			{
				m_State = S_LAUNCH;
				m_LaunchStart = PredTick;
				m_LaunchStartDist = Dist;
				m_HookFiredTick = -1;
				SetStatus("цепляет стену и разгоняется через фриз (%d px)", (int)Dist);
				break;
			}

			m_State = S_UNREACH;
			Goal = OwnerPos;
			UsePlanner = true;
			SetStatus("хукать неоткуда, идёт пешком (%d px)", (int)Dist);
			break;
		}

		if(distance(BotPos, m_Anchor) < 24.0f && Grounded)
		{
			m_State = S_HOOK;
			m_CommitUntil = PredTick + 16;
			m_HookFiredTick = -1;
			m_HookRetries = 0;
			break;
		}
		if(m_StuckTicks > 25)
		{
			Reject(m_Anchor, PredTick);
			m_AnchorValid = false;
			m_StuckTicks = 0;
			m_State = S_UNREACH;
			Goal = OwnerPos;
			UsePlanner = true;
			SetStatus("до позиции хука не дойти, идёт пешком (%d px)", (int)Dist);
			break;
		}
		Goal = m_Anchor;
		UsePlanner = true;
		SetStatus("идёт на позицию для хука (%d px)", (int)Dist);
		break;
	}

	case S_HOOK:
	{
		if(!g_Config.m_BotUseHook)
		{
			m_State = S_HAMMER;
			break;
		}
		if(m_ReleaseUntil > PredTick)
		{
			pInput->m_Hook = 0;
			SetStatus("перезаряжает хук (%d px)", (int)Dist);
			break;
		}
		if(Dist <= HookMin && HammerHelps && PredTick >= m_NoHammerUntil)
		{
			m_State = S_HAMMER;
			m_HookFiredTick = -1;
			break;
		}

		const vec2 Muzzle = BotPos + normalize(Delta) * L.m_HookMinPull;
		vec2 AimPos = OwnerPos;
		const bool LineBlocked = !HookPathToOwner(Muzzle, OwnerPos, BotId, OwnerId, &AimPos);
		const bool TooFar = Dist > L.m_HookReach;
		const bool Unsafe = !PullSafe(BotPos, OwnerPos);
		if((LineBlocked || TooFar || Unsafe) && PredTick >= m_CommitUntil)
		{
			if(m_AnchorValid)
			{
				Reject(m_Anchor, PredTick);
			}
			if(Unsafe)
				Reject(BotPos, PredTick);
			m_AnchorValid = false;
			m_HookFiredTick = -1;
			m_State = TooFar ? S_UNREACH : S_GOTO;
			if(TooFar)
				SetStatus("владелец дальше хука, идёт ближе (%d px)", (int)Dist);
			else if(Unsafe)
				SetStatus("отсюда выдернет во фриз, ищет другое место (%d px)", (int)Dist);
			else
				SetStatus("линия хука перекрыта (%d px)", (int)Dist);
			break;
		}

		const float Flight = std::ceil(std::max(0.0f, Dist - L.m_HookMinPull) / 80.0f);
		vec2 AimOff = AimPos - OwnerPos;
		if(length(AimOff) < 1.0f)
			AimOff = vec2(0.0f, -10.0f * (float)(m_HookRetries % 4));
		const float AimCap = CCharacterCore::PhysicalSize();
		if(length(AimOff) > AimCap)
			AimOff = normalize(AimOff) * AimCap;
		const vec2 Lead = OwnerPos + AimOff + OwnerVel * Flight - BotPos;
		pInput->m_TargetX = round_to_int(Lead.x);
		pInput->m_TargetY = round_to_int(Lead.y);
		if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
			pInput->m_TargetX = 1;

		pInput->m_Hook = 1;
		AimPrecise = true;

		const int SnapLag = std::clamp(PredTick - Client()->GameTick(BotConn()), 0, 20);
		const int MissAfter = L.m_HookTravelTicks + (m_UsingPrediction ? 1 : SnapLag + 2);
		const int Outcome = HookOutcome(PredTick, HookState, HookedPlayer, OwnerId);

		if(m_HookFiredTick < 0)
		{
			m_HookFiredTick = PredTick;
			SetStatus("выстрел хуком (%d px)", (int)Dist);
			break;
		}

		const bool Missed = Outcome == HK_MISS || Outcome == HK_TERRAIN ||
				    PredTick - m_HookFiredTick > MissAfter;
		if(Missed)
		{
			pInput->m_Hook = 0;
			m_ReleaseUntil = PredTick + 1 + (m_UsingPrediction ? 0 : std::clamp(PredTick - Client()->GameTick(BotConn()), 0, 20) + 2);
			m_HookFiredTick = -1;
			m_HookRetries++;

			if(m_HookRetries >= 6)
			{
				if(m_AnchorValid)
				{
					Reject(m_Anchor, PredTick);
				}
				Reject(BotPos, PredTick);
				m_AnchorValid = false;
				m_HookRetries = 0;
				m_NoHookUntil = PredTick + 50;
				m_State = S_GOTO;
				SetStatus("шесть промахов, меняет позицию (%d px)", (int)Dist);
			}
			else
			{
				SetStatus("промах %d, бьёт снова (%d px)", m_HookRetries, (int)Dist);
			}
			break;
		}
		SetStatus("хукает владельца (%d px)", (int)Dist);
		break;
	}

	case S_PULL:
	{
		if(g_Config.m_BotUseHammer && Dist <= LetGo)
		{
			pInput->m_Hook = 0;
			m_ReleaseUntil = PredTick + 1 + (m_UsingPrediction ? 0 : std::clamp(PredTick - Client()->GameTick(BotConn()), 0, 20) + 2);
			m_State = S_HAMMER;
			m_AnchorValid = false;
			SetStatus("дотянул, бьёт молотом (%d px)", (int)Dist);
			break;
		}

		pInput->m_Hook = 1;
		AimPrecise = true;

		const bool Yank = !g_Config.m_BotGentle || Dist > 128.0f || OwnerOnFreeze;
		ManualDir = Yank ? (Delta.x > 0.0f ? -1 : 1) : 0;

		const int Since = PredTick - m_GrabbedTick;
		if(Yank && Since >= 2 && Since <= 4 && Grounded)
		{
			const CTuningParams &JumpTune = GameClient()->m_aTuning[BotConn()];
			const vec2 Rope = Dist > 1.0f ? normalize(Delta) * (float)JumpTune.m_HookDragAccel : vec2(0.0f, 0.0f);
			const vec2 Launched = vec2(BotVel.x, -(float)JumpTune.m_GroundJumpImpulse);
			if(!FreezeAhead(BotPos, Launched, Rope, 18))
				ManualJump = true;
		}

		bool FreezeAhead = false;
		{
			const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
			vec2 Probe = BotPos;
			vec2 Vel = BotVel;
			for(int t = 0; t < 8 && !FreezeAhead; t++)
			{
				Vel.y += (float)Tuning.m_Gravity;
				const vec2 Next = Probe + Vel;
				if(SweepFreeze(Probe, Next))
					FreezeAhead = true;
				Probe = Next;
			}
		}
		bool Release = FreezeAhead;
		if(Dist > 64.0f && !PullSafe(BotPos, OwnerPos))
			Release = true;
		if(Dist < m_LastPullDist - 8.0f)
		{
			m_LastPullDist = Dist;
			m_LastProgressTick = PredTick;
		}
		else if(PredTick - m_LastProgressTick > 25)
		{
			Release = true;
		}
		if(Since > 50)
			Release = true;

		if(Release)
		{
			pInput->m_Hook = 0;
			m_ReleaseUntil = PredTick + 1 + (m_UsingPrediction ? 0 : std::clamp(PredTick - Client()->GameTick(BotConn()), 0, 20) + 2);
			m_State = Dist <= HookMin ? S_HAMMER : S_GOTO;
			m_AnchorValid = false;
			if(FreezeAhead)
			{
				Reject(BotPos, PredTick);
				m_NoHookUntil = PredTick + 50;
			}
			str_copy(m_aStatus, "отпускает хук");
			break;
		}
		SetStatus("тянет владельца (%d px)", (int)Dist);
		break;
	}

	case S_HAMMER:
	{
		if(!OwnerFrozen)
		{
			m_State = S_FOLLOW;
			break;
		}
		if(!g_Config.m_BotUseHammer)
		{
			m_State = g_Config.m_BotUseHook ? S_GOTO : S_FOLLOW;
			break;
		}
		if(OwnerDeep)
		{
			SetStatus("deep freeze — молотом не выбить");
			break;
		}
		if(m_HammerEnterTick < 0)
			m_HammerEnterTick = PredTick;

		if(HammerCanHit(BotPos, OwnerPos))
		{
			m_StuckTicks = 0;
			m_HammerEnterTick = PredTick;
			SetStatus("бьёт молотом (%d px)", (int)Dist);
			break;
		}

		if((m_StuckTicks > 40 || PredTick - m_HammerEnterTick > 150) && g_Config.m_BotUseHook)
		{
			m_StuckTicks = 0;
			m_AnchorValid = false;
			m_HammerEnterTick = -1;
			m_NoHammerUntil = PredTick + 100;
			m_State = S_UNREACH;
			SetStatus("не подойти для удара, ищет другой путь (%d px)", (int)Dist);
			break;
		}

		if(Dist > L.m_HammerMax * 2.5f && g_Config.m_BotUseHook &&
			!CorridorClear(BotPos, OwnerPos - normalize(Delta) * L.m_HammerMax))
		{
			m_State = S_GOTO;
			m_AnchorValid = false;
			Goal = OwnerPos;
			UsePlanner = true;
			break;
		}

		if(Dist <= L.m_HammerMax * 2.0f && OwnerPos.y < BotPos.y - 24.0f)
		{
			const float Rise = std::min(OwnerPos.y - (BotPos.y - 180.0f), 180.0f);
			const bool Worth = Rise > 0.0f && HammerCanHit(BotPos - vec2(0.0f, Rise), OwnerPos);

			ManualDir = Delta.x > 6.0f ? 1 : (Delta.x < -6.0f ? -1 : 0);
			ManualJump = Grounded && Worth;
			SetStatus(Worth ? "подпрыгивает, чтобы достать (%d px)" : "подходит под него (%d px)", (int)Dist);
			break;
		}

		{
			const float Side = BotPos.x < OwnerPos.x ? -1.0f : 1.0f;
			const float Reach = L.m_HammerMax - 8.0f;

			static const int s_aRow[6] = {0, 1, 2, -1, 3, 4};

			Goal = vec2(OwnerPos.x + Side * Reach, OwnerPos.y);
			bool HaveSpot = false;
			float BestSpot = 1e9f;

			for(int s = 0; s < 2 && !HaveSpot; s++)
			{
				const float Sign = s == 0 ? Side : -Side;
				for(float Out = 16.0f; Out <= L.m_HammerMax + 8.0f; Out += 4.0f)
				{
					for(int r = 0; r < 6; r++)
					{
						const vec2 Try = vec2(OwnerPos.x + Sign * Out,
							32.0f * std::floor((OwnerPos.y + 15.0f + 32.0f * (float)s_aRow[r]) / 32.0f) - 15.0f);
						if(!SafeStand(Try) || !HammerCanHit(Try, OwnerPos))
							continue;
						const float D = distance(Try, BotPos);
						if(D >= BestSpot)
							continue;
						BestSpot = D;
						Goal = Try;
						HaveSpot = true;
					}
				}
			}

			if(HaveSpot && BestSpot < 26.0f)
			{
				const float Step = Goal.x - BotPos.x;
				ManualDir = absolute(Step) > 2.0f ? (Step > 0.0f ? 1 : -1) : (Delta.x > 0.0f ? 1 : -1);
				SetStatus("доворачивает под удар (%d px)", (int)Dist);
				break;
			}

			if(!HaveSpot)
			{
				for(float Out = Reach; Out <= Reach + 96.0f && !HaveSpot; Out += 32.0f)
				{
					for(int r = 0; r < 6 && !HaveSpot; r++)
					{
						const vec2 Try = vec2(OwnerPos.x + Side * Out,
							32.0f * std::floor((OwnerPos.y + 15.0f + 32.0f * (float)s_aRow[r]) / 32.0f) - 15.0f);
						if(SafeStand(Try))
						{
							Goal = Try;
							HaveSpot = true;
						}
					}
				}
			}

			UsePlanner = true;
			SetStatus("подходит для удара (%d px)", (int)Dist);
			break;
		}
	}

	case S_CLIMB:
	{
		const float OwnerFloorY = 32.0f * std::floor((OwnerPos.y + 15.0f) / 32.0f);
		const vec2 Rope = m_ClimbGrab - BotPos;
		const float RopeLen = length(Rope);

		if(BotPos.y < m_ClimbBestY - 32.0f)
		{
			m_ClimbBestY = BotPos.y;
			m_ClimbGainTick = PredTick;
		}

		const bool Arrived = BotPos.y <= m_ClimbLand.y + 4.0f && absolute(BotPos.x - m_ClimbLand.x) < 24.0f;
		const bool Stalled = PredTick - m_ClimbGainTick > 60 || PredTick - m_ClimbStart > 200;

		bool FreezeAhead = false;
		{
			const CTuningParams &Tuning = GameClient()->m_aTuning[BotConn()];
			vec2 Probe = BotPos;
			vec2 Vel = BotVel;
			for(int t = 0; t < 8 && !FreezeAhead; t++)
			{
				Vel.y += (float)Tuning.m_Gravity;
				if(RopeLen > 46.0f)
					Vel += normalize(Rope) * (float)Tuning.m_HookDragAccel;
				const vec2 Next = Probe + Vel;
				if(SweepFreeze(Probe, Next))
					FreezeAhead = true;
				Probe = Next;
			}
		}

		if(HookState == HOOK_RETRACTED || HookState == HOOK_RETRACT_START || HookState == HOOK_RETRACT_END)
		{
			pInput->m_Hook = 0;
			m_ClimbStart = PredTick;
			m_HookRetries++;
			if(m_HookRetries >= 3)
			{
				Reject(m_ClimbGrab, PredTick);
				m_HookRetries = 0;
				m_State = S_UNREACH;
				SetStatus("хук не цепляет стену");
			}
			else
			{
				SetStatus("перецепляется за стену");
			}
			break;
		}

		if(Stalled || FreezeAhead)
		{
			pInput->m_Hook = 0;
			m_ReleaseUntil = PredTick + 1 + (m_UsingPrediction ? 0 : std::clamp(PredTick - Client()->GameTick(BotConn()), 0, 20) + 2);
			m_State = S_UNREACH;
			m_AnchorValid = false;
			m_HookRetries = 0;
			Reject(m_ClimbGrab, PredTick);
			SetStatus("подъём сорвался");
			break;
		}

		const bool HoldDone = PredTick - m_ClimbStart >= m_ClimbHold;
		if(Arrived || HoldDone)
		{
			pInput->m_Hook = 0;
			m_ReleaseUntil = PredTick + 1 + (m_UsingPrediction ? 0 : std::clamp(PredTick - Client()->GameTick(BotConn()), 0, 20) + 2);
			const bool NearTop = BotPos.y <= OwnerFloorY + 48.0f;
			if(!Arrived && m_ClimbAirJump && NearTop && !FreezeAt(BotPos + vec2(0.0f, -24.0f)))
				ManualJump = true;
			m_ClimbAirJump = false;
			m_AnchorValid = false;
			m_HookRetries = 0;
			if(Arrived || NearTop)
			{
				m_State = S_GOTO;
				SetStatus("залез, добивает спасение (%d px)", (int)Dist);
			}
			else
			{
				Reject(m_ClimbGrab, PredTick);
				m_State = S_UNREACH;
				SetStatus("подъём не дотянул (%d px)", (int)Dist);
			}
			break;
		}

		pInput->m_Hook = 1;
		pInput->m_TargetX = round_to_int(Rope.x);
		pInput->m_TargetY = round_to_int(Rope.y);
		if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
			pInput->m_TargetY = -1;
		AimPrecise = true;

		ManualDir = m_ClimbGrab.x > BotPos.x ? 1 : -1;
		SetStatus("лезет по верёвке (%d px до цепа)", (int)RopeLen);
		break;
	}

	case S_LAUNCH:
	{
		const int Since = PredTick - m_LaunchStart;
		const vec2 Rope = m_Launch.m_Grab - BotPos;
		const float RopeLen = length(Rope);
		const bool Holding = Since < m_Launch.m_Hold && RopeLen > 46.0f;

		if(HookState == HOOK_RETRACTED || HookState == HOOK_RETRACT_START || HookState == HOOK_RETRACT_END)
		{
			Reject(m_Launch.m_Grab, PredTick);
			m_State = S_UNREACH;
			SetStatus("стена не цепляется, идёт пешком");
			break;
		}

		const vec2 Pull = Holding && RopeLen > 1.0f
					  ? normalize(Rope) * (float)GameClient()->m_aTuning[BotConn()].m_HookDragAccel
					  : vec2(0.0f, 0.0f);

		if(Holding && !FreezeAhead(BotPos, BotVel, Pull, 10))
		{
			pInput->m_Hook = 1;
			pInput->m_TargetX = round_to_int(Rope.x);
			pInput->m_TargetY = round_to_int(Rope.y);
			if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
				pInput->m_TargetY = -1;
			AimPrecise = true;
			ManualDir = m_Launch.m_Dir;
			SetStatus("разгоняется на верёвке (%d тик)", Since);
			break;
		}

		if(Since > 200 || (Grounded && Since > m_Launch.m_Hold + 8))
		{
			m_State = S_GOTO;
			m_AnchorValid = false;
			if(distance(BotPos, OwnerPos) > m_LaunchStartDist - 32.0f)
				Reject(m_Launch.m_Grab, PredTick);
			SetStatus("полёт закончен");
			break;
		}

		Goal = m_Launch.m_Land;
		UsePlanner = true;
		SetStatus("летит через фриз (%d px)", (int)Dist);
		break;
	}

	case S_UNREACH:
	{
		if(Dist <= HookMin && HammerHelps && PredTick >= m_NoHammerUntil)
		{
			m_State = S_HAMMER;
			break;
		}
		if(g_Config.m_BotUseHook && Dist > HookMin && Dist <= L.m_HookReach &&
			PullSafe(BotPos, OwnerPos) &&
			HookPathToOwner(BotPos + normalize(Delta) * L.m_HookMinPull, OwnerPos, BotId, OwnerId))
		{
			m_State = S_GOTO;
			m_AnchorValid = false;
			break;
		}
		if(m_EscapeUntil > PredTick)
		{
			if(distance(BotPos, m_EscapeGoal) > 28.0f)
			{
				Goal = m_EscapeGoal;
				UsePlanner = true;
				m_StuckTicks = 0;
				SetStatus("выбирается из тупика (%d px)", (int)Dist);
				break;
			}
			m_EscapeUntil = -1;
			m_StuckTicks = 0;
		}

		const bool HookAsLegs = g_Config.m_BotUseHook && HaveCore && Grounded &&
					PredTick >= m_NoHookUntil && PredTick >= m_NextHookPlanTick &&
					(!m_PlanSafe || PredTick - m_ProgressTick > 10);
		if(HookAsLegs)
			m_NextHookPlanTick = PredTick + 8;

		if(HookAsLegs && OwnerPos.y < BotPos.y - 48.0f)
		{
			vec2 Grab, Land;
			int Hold;
			bool AirJump;
			if(PlanClimb(SimCore, OwnerPos, PredTick, &Grab, &Land, &Hold, &AirJump))
			{
				m_ClimbGrab = Grab;
				m_ClimbLand = Land;
				m_ClimbHold = Hold;
				m_ClimbAirJump = AirJump;
				m_State = S_CLIMB;
				m_ClimbStart = PredTick;
				m_ClimbBestY = BotPos.y;
				m_ClimbGainTick = PredTick;
				m_HookFiredTick = -1;
				SetStatus("пешком не подняться, лезет на хуке (%d px)", (int)Dist);
				break;
			}
		}

		if(HookAsLegs && PlanLaunch(SimCore, OwnerPos, PredTick, &m_Launch))
		{
			m_State = S_LAUNCH;
			m_LaunchStart = PredTick;
			m_LaunchStartDist = Dist;
			m_HookFiredTick = -1;
			SetStatus("бега не хватает, идёт через хук (%d px)", (int)Dist);
			break;
		}

		if(HookAsLegs && !CorridorClear(BotPos, OwnerPos))
		{
			vec2 Grab, Land;
			int Hold;
			bool AirJump;
			if(PlanClimb(SimCore, OwnerPos, PredTick, &Grab, &Land, &Hold, &AirJump))
			{
				m_ClimbGrab = Grab;
				m_ClimbLand = Land;
				m_ClimbHold = Hold;
				m_ClimbAirJump = AirJump;
				m_State = S_CLIMB;
				m_ClimbStart = PredTick;
				m_ClimbBestY = BotPos.y;
				m_ClimbGainTick = PredTick;
				m_HookFiredTick = -1;
				SetStatus("между нами стена, лезет через неё (%d px)", (int)Dist);
				break;
			}
		}

		if(m_StuckTicks > 25)
		{
			const float Side = Delta.x > 0.0f ? -1.0f : 1.0f;
			for(float Step = 128.0f; Step >= 32.0f; Step -= 32.0f)
			{
				const vec2 Try = vec2(BotPos.x + Side * Step, BotPos.y);
				if(!SafeStand(Try) || !CorridorClear(BotPos, Try))
					continue;
				m_EscapeGoal = Try;
				m_EscapeUntil = PredTick + 50;
				m_StuckTicks = 0;
				break;
			}
		}
		UsePlanner = true;
		SetStatus("идёт к владельцу пешком (%d px)", (int)Dist);
		break;
	}

	default:
		m_State = S_FOLLOW;
		break;
	}

	bool CanAutoHammer;
	if(m_State == S_HOOK || m_State == S_CLIMB || m_State == S_LAUNCH || m_State == S_SELF || m_State == S_PULL)
		CanAutoHammer = false;
	else if(OwnerOnFreeze)
		CanAutoHammer = m_State == S_HAMMER;
	else
		CanAutoHammer = true;

	const int SnapTick = Client()->GameTick(BotConn());
	const int HammerReload = m_LastHammerHit ? 16 : 7;
	const bool HammerFree = BotChar.m_AttackTick <= 0 || SnapTick - BotChar.m_AttackTick >= HammerReload;
	const bool AimLocked = HookState == HOOK_FLYING;

	if(CanRescue && g_Config.m_BotUseHammer && CanAutoHammer &&
		!AimLocked && BotChar.m_Weapon == WEAPON_HAMMER && HammerFree &&
		(m_LastHammerTick < 0 || PredTick - m_LastHammerTick >= 3) &&
		HammerCanHit(BotPos, OwnerPos))
	{
		pInput->m_TargetX = round_to_int(Delta.x);
		pInput->m_TargetY = round_to_int(Delta.y);
		if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
			pInput->m_TargetX = 1;
		AimPrecise = true;

		pInput->m_Fire = (pInput->m_Fire + 2) & INPUT_STATE_MASK;
		m_LastHammerTick = PredTick;
		m_LastHammerHit = true;
	}

	if(UsePlanner && OwnerOnFreeze && distance(Goal, OwnerPos) < 1.0f)
	{
		vec2 Spot;
		if(SafeGoalNear(OwnerPos, BotPos, &Spot))
			Goal = Spot;
	}

	if(UsePlanner && HaveCore)
	{
		Steer(SimCore, Goal, PredTick, pInput);
		if(!m_PlanSafe)
			SetStatus("везде фриз, идёт по самому живучему пути (%d px)", (int)Dist);
	}
	else
	{
		const bool Roped = HookState == HOOK_GRABBED || pInput->m_Hook != 0;

		if((ManualDir != 0 || ManualJump) && HaveCore && !Roped &&
			m_State != S_CLIMB && m_State != S_LAUNCH)
		{
			SAction Probe = {0, 0, ManualDir, ManualJump ? 1 : -1, -1};
			const SSimResult ProbeRes = Simulate(SimCore, Probe, Goal, 45);
			if(ProbeRes.m_Frozen && ProbeRes.m_FreezeTick <= 45)
			{
				bool Saved = false;
				if(ManualJump)
				{
					SAction NoJump = Probe;
					NoJump.m_JumpAt = -1;
					const SSimResult Walk = Simulate(SimCore, NoJump, Goal, 45);
					if(!Walk.m_Frozen || Walk.m_FreezeTick > 45)
					{
						ManualJump = false;
						Saved = true;
						SetStatus("не прыгает, там фриз (%d px)", (int)Dist);
					}
				}
				if(!Saved)
				{
					ManualJump = false;
					ManualDir = 0;
					SetStatus("впереди фриз, не лезет (%d px)", (int)Dist);
				}
			}
		}
		pInput->m_Direction = ManualDir;

		const bool JumpAgain = Grounded && m_JumpPressTick >= 0 && PredTick - m_JumpPressTick >= 10;
		if(ManualJump && (!m_JumpHeld || JumpAgain))
		{
			pInput->m_Jump = 1;
			m_JumpHeld = true;
			m_JumpPressTick = PredTick;
		}
		else
		{
			pInput->m_Jump = 0;
			if(!ManualJump)
				m_JumpHeld = false;
		}
	}

	const bool Committed = HookState == HOOK_GRABBED || pInput->m_Hook != 0 ||
			       m_State == S_CLIMB || m_State == S_LAUNCH;

	if(HaveCore && Grounded && !Committed && pInput->m_Direction != 0)
	{
		SAction Emit;
		Emit.m_Dir = pInput->m_Direction;
		Emit.m_JumpAt = pInput->m_Jump != 0 ? 1 : -1;

		const SSimResult EmitRes = Simulate(SimCore, Emit, Goal, 26);
		if(EmitRes.m_Frozen && EmitRes.m_FreezeTick <= 10 && EmitRes.m_MinGoalDist >= 48.0f)
		{
			SAction Alt;
			Alt.m_Dir = 0;
			if(!Simulate(SimCore, Alt, Goal, 26).m_Frozen)
			{
				pInput->m_Direction = 0;
				pInput->m_Jump = 0;
				m_JumpHeld = false;
				m_PlanUntil = -1;
				m_PlanCommitUntil = -1;
				m_PlanSafe = false;
				SetStatus("тормозит у фриза (%d px)", (int)Dist);
			}
			else
			{
				Alt.m_Dir = -pInput->m_Direction;
				if(!Simulate(SimCore, Alt, Goal, 26).m_Frozen)
				{
					pInput->m_Direction = Alt.m_Dir;
					pInput->m_Jump = 0;
					m_JumpHeld = false;
					m_PlanUntil = -1;
					m_PlanCommitUntil = -1;
					m_PlanSafe = false;
					SetStatus("уходит от фриза (%d px)", (int)Dist);
				}
			}
		}
	}

	if(AimPrecise)
	{
		m_AimPoint = vec2((float)pInput->m_TargetX, (float)pInput->m_TargetY);
	}
	else
	{
		const float Face = pInput->m_Direction != 0 ? (float)pInput->m_Direction : (m_AimPoint.x < 0.0f ? -1.0f : 1.0f);

		const float Phase = (float)PredTick * 0.05f;
		const vec2 Want = vec2(Face * 140.0f + 26.0f * std::sin(Phase * 0.37f),
			10.0f + 34.0f * std::sin(Phase * 0.23f + 1.7f));

		m_AimPoint += (Want - m_AimPoint) * 0.12f;

		pInput->m_TargetX = round_to_int(m_AimPoint.x);
		pInput->m_TargetY = round_to_int(m_AimPoint.y);
		if(pInput->m_TargetX == 0 && pInput->m_TargetY == 0)
			pInput->m_TargetX = Face > 0.0f ? 1 : -1;
	}

	if(g_Config.m_BotDebug && (m_LastDebugTick < 0 || PredTick - m_LastDebugTick >= 50))
	{
		m_LastDebugTick = PredTick;
		static const char *s_apStates[] = {"FOLLOW", "GOTO", "HOOK", "PULL", "HAMMER", "UNREACH", "CLIMB", "LAUNCH", "SELF", "ROAM"};
		log_info("botsymona", "%s bot=%d owner=%d dist=%d dir=%d jump=%d hook=%d grounded=%d safe=%d cross=%d pred=%d snaplag=%d jumpin=%d stuck=%d vel=%.1f",
			s_apStates[m_State], BotId, OwnerId, (int)Dist, pInput->m_Direction, pInput->m_Jump,
			pInput->m_Hook, Grounded ? 1 : 0, m_PlanSafe ? 1 : 0, m_PlanCross ? 1 : 0,
			m_UsingPrediction ? 1 : 0, PredTick - Client()->GameTick(BotConn()),
			m_Plan.m_JumpAt >= 0 ? m_Plan.m_JumpAt - PredTick : -1,
			m_ProgressTick >= 0 ? PredTick - m_ProgressTick : 0, BotVel.x);
	}

	m_Driving = true;
	return true;
}
