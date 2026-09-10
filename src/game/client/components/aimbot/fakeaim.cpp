#include "fakeaim.h"

#include <base/vmath.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/gamecore.h>

#include <cmath>
#include <cstdlib>

void CFakeAim::OnReset()
{
	m_RenderActive = false;
	m_RenderOffset = vec2(0.0f, 0.0f);
	m_RobotAim = vec2(0.0f, 0.0f);
	m_RobotInit = false;
	m_RandAngle = 0.0f;
	m_TickCounter = 0;
	m_SpinAngle = 0.0f;
	m_LagCounter = 0;
	m_LastHook = 0;
	m_LastFake = vec2(0.0f, 0.0f);
	m_HaveLastFake = false;
	m_SendFake = false;
	m_SendOffset = vec2(0.0f, 0.0f);
}

void CFakeAim::OnSnapInput(CNetObj_PlayerInput *pOut, CNetObj_PlayerInput *pLast)
{
	m_RenderActive = false;
	m_SendFake = false;
	if(!g_Config.m_AaFakeAim)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_pLocalCharacter)
		return;

	// Detect a real shot via 1-tick prediction: while fire is held the server
	// fires on reload cadence, and the fake must release on each of those
	// ticks so the shot lands. A rising edge alone only catches the press.
	bool WillFire = false;
	if(CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(LocalId))
	{
		const int PredTick = Client()->PredGameTick(Dummy);
		const int TickBefore = pChar->GetAttackTick();
		const bool WasFrozen = GameClient()->m_PredictedChar.m_DeepFrozen || GameClient()->m_PredictedChar.m_FreezeEnd > PredTick;
		CGameWorld FutureWorld;
		FutureWorld.CopyWorld(&GameClient()->m_PredictedWorld);
		if(CCharacter *pFuture = FutureWorld.GetCharacterById(LocalId))
		{
			CNetObj_PlayerInput FutureInput = *pOut;
			pFuture->OnDirectInput(&FutureInput);
			FutureWorld.m_GameTick = FutureWorld.GameTick() + 1;
			pFuture->OnPredictedInput(&FutureInput);
			FutureWorld.Tick();
			if(CCharacter *pAfter = FutureWorld.GetCharacterById(LocalId))
			{
				if(pAfter->GetAttackTick() != TickBefore)
					WillFire = true;
				const CCharacterCore &AfterCore = pAfter->GetCore();
				if(WasFrozen && !AfterCore.m_DeepFrozen && AfterCore.m_FreezeEnd <= FutureWorld.GameTick())
					WillFire = true;
			}
		}
	}

	// Hammer always hits at the cursor: force release while the hammer is
	// out and fire is held/requested (covers fast-fire spray, auto-hammer
	// and the weapon-switch tick the 1-tick sim can't see yet).
	if(!WillFire)
	{
		const int ActiveWeapon = GameClient()->m_PredictedChar.m_ActiveWeapon;
		if(ActiveWeapon == WEAPON_HAMMER && ((pOut->m_Fire & 1) != 0 || pOut->m_WantedWeapon == WEAPON_HAMMER + 1))
			WillFire = true;
	}

	// Hook press edge
	const bool HookPress = (pOut->m_Hook != 0 && m_LastHook == 0);
	m_LastHook = pOut->m_Hook;

	// Aimbot owns this tick (press/block/fire snap): yield to the snapped
	// aim so both systems run together instead of fighting over pData.
	const bool AimOwned = GameClient()->m_Aimbot.m_AimSnapped;
	const vec2 SnappedAim((float)pOut->m_TargetX, (float)pOut->m_TargetY);

	// Robot/Lag memory: last real (cursor) aim. Never touches m_aMousePos —
	// the cursor stays exactly where the player left it.
	const vec2 RealAim = GameClient()->m_Controls.m_aMousePos[Dummy];
	if(!m_RobotInit)
	{
		m_RobotAim = RealAim;
		m_RobotInit = true;
	}
	if(WillFire || HookPress || AimOwned)
		m_RobotAim = AimOwned ? SnappedAim : RealAim;

	bool FakeActive = false;
	vec2 FakeOffset(0.0f, 0.0f);
	bool FakeShowForMe = false;
	if(WillFire || HookPress || AimOwned)
	{
		// release tick: shoot/hook goes to the real (or snapped) aim
		FakeActive = true;
		FakeOffset = AimOwned ? SnappedAim : RealAim;
		FakeShowForMe = true;
	}
	else
	{
		int Mode = g_Config.m_AaFakeAimMode;
		int Speed = g_Config.m_AaFakeAimSpeed;
		if(Speed < 1)
			Speed = 1;
		if(Speed > 100)
			Speed = 100;
		static constexpr float AIM_DIST = 200.0f;

		if(Mode == MODE_RANDOM)
		{
			if(++m_TickCounter >= Speed)
			{
				m_RandAngle = ((float)rand() / (float)RAND_MAX) * pi * 2.0f;
				m_TickCounter = 0;
			}
			FakeOffset = vec2(cosf(m_RandAngle), sinf(m_RandAngle)) * AIM_DIST;
		}
		else if(Mode == MODE_ROBOT)
		{
			FakeOffset = m_RobotAim;
		}
		else if(Mode == MODE_LAG)
		{
			if(++m_LagCounter >= Speed)
			{
				m_RobotAim = RealAim;
				m_LagCounter = 0;
			}
			FakeOffset = m_RobotAim;
		}
		else // MODE_SPIN
		{
			m_SpinAngle += (Speed / 100.0f) * 0.5f;
			FakeOffset = vec2(cosf(m_SpinAngle), sinf(m_SpinAngle)) * AIM_DIST;
		}

		FakeActive = true;
		FakeShowForMe = g_Config.m_AaFakeAimShowForMe != 0;
	}

	if(FakeActive)
	{
		m_RenderActive = true;
		m_RenderOffset = FakeOffset;
		if(FakeShowForMe)
		{
			// visible: input itself carries the fake (server + prediction)
			pOut->m_TargetX = (int)FakeOffset.x;
			pOut->m_TargetY = (int)FakeOffset.y;
		}
		else
		{
			// server-only: patched into the send buffer later, mouse untouched
			m_SendFake = true;
			m_SendOffset = FakeOffset;
		}
	}

	// Hold the rendered mask across release ticks so the shown direction
	// doesn't flicker to the real aim for a frame. Server data untouched.
	// Skipped on aimbot-owned ticks: the shot genuinely goes snapped there.
	if(m_HaveLastFake && (WillFire || HookPress) && !AimOwned)
	{
		m_RenderActive = true;
		m_RenderOffset = m_LastFake;
	}
	if(FakeActive && !WillFire && !HookPress && !AimOwned)
	{
		m_LastFake = FakeOffset;
		m_HaveLastFake = true;
	}
}

void CFakeAim::PatchSendData(CNetObj_PlayerInput *pSend) const
{
	if(m_SendFake && pSend)
	{
		pSend->m_TargetX = (int)m_SendOffset.x;
		pSend->m_TargetY = (int)m_SendOffset.y;
	}
}

bool CFakeAim::RenderOffset(vec2 *pOut) const
{
	if(!g_Config.m_AaFakeAim || !g_Config.m_AaFakeAimShowForMe || !m_RenderActive)
		return false;
	*pOut = m_RenderOffset;
	return true;
}
