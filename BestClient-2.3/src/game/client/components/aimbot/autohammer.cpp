#include "autohammer.h"

#include <base/vmath.h>

#include <engine/shared/config.h>

#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/collision.h>
#include <game/gamecore.h>

// and visible, switch to the hammer and swing.  Leads the target by his
// velocity (1 tick) so the swing connects where he WILL be, and verifies
// the predicted position is still within the hammer hit zone before firing.
void CAutoHammer::OnReset()
{
	m_AutoHammerHolding = false;
	m_AutoHammerPrevWeapon = -1;
}

void CAutoHammer::OnPlayerInput(CNetObj_PlayerInput *pOut)
{
	if(!g_Config.m_AaAutoHammer)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_pLocalCharacter)
		return;

	// release a previous synthetic press so the fire counter is back to "not
	// held"; otherwise the bit stays 1 and we would only ever fire once.
	if(m_AutoHammerHolding)
	{
		if((pOut->m_Fire & 1) != 0)
			pOut->m_Fire = (pOut->m_Fire + 1) & INPUT_STATE_MASK;
		m_AutoHammerHolding = false;
	}

	const int PredTick = Client()->PredGameTick(Dummy);

	if(!GameClient()->m_PredictedChar.m_aWeapons[WEAPON_HAMMER].m_Got)
		return;
	if(GameClient()->m_PredictedChar.m_DeepFrozen || GameClient()->m_PredictedChar.m_FreezeEnd > PredTick)
		return;

	const vec2 MyPos = GameClient()->m_PredictedChar.m_Pos;
	const vec2 MyVel = GameClient()->m_PredictedChar.m_Vel;
	const int ActiveWeapon = GameClient()->m_PredictedChar.m_ActiveWeapon;

	// Геометрия молотка 1в1 как на сервере (FireWeapon, hammer):
	// старт ProjStart = MyPos + Dir * 28*0.75 (21px), хит если центр цели
	// в радиусе 28*0.5 + 28 (14+28=42px) от старта. Итого бьёт до 63px.
	// Оценка — по СНАПШОТУ, а не по предикту: предикт душит скорость чужих
	// трением (инпутов не знает) и на скорости даёт систематический недолёт.
	// Снапшот — серверная правда: позиция как есть, скорость полная.
	// Анализируем пару (своя+чужая скорости) и бьём в момент, когда цель
	// входит в зону: сближающуюся ждём, уходящую добиваем сразу.
	const float HammerProjStart = 28.0f * 0.75f;
	const float HammerHitRadius = 28.0f * 0.5f + 28.0f;
	const float HammerSolidRadius = HammerHitRadius - 12.0f; // твёрдая зона: запас на ошибку лида
	const float HammerSwitchRange = 80.0f; // молот в руки заранее, без задержек
	const int PingMs = GameClient()->m_Snap.m_pLocalInfo ? GameClient()->m_Snap.m_pLocalInfo->m_Latency : 40;
	int OnewayTicks = (PingMs + 20) / 40;
	if(OnewayTicks < 0)
		OnewayTicks = 0;
	if(OnewayTicks > 4)
		OnewayTicks = 4;
	// себя ведём от свежего предикта (свои инпуты известны — он точный)
	const vec2 LeadMyPos = MyPos + MyVel * (float)(OnewayTicks + 1);

	float BestDist = 1e9f;
	int TargetId = -1;
	vec2 TargetPos = vec2(0.0f, 0.0f);
	vec2 BestTargetVel = vec2(0.0f, 0.0f);
	float BestHitDist = 1e9f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == LocalId || !GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		const CGameClient::CClientData &Data = GameClient()->m_aClients[i];
		if(!Data.m_Active)
			continue;
		if(!GameClient()->m_Teams.CanCollide(LocalId, i))
			continue;
		if(Data.m_FreezeEnd != 0 || Data.m_DeepFrozen)
			continue;

		// снапшот — серверная правда на тик m_Cur.m_Tick; ведём её до момента
		// исполнения удара: возраст снапа + полёт инпута + тик исполнения
		const CNetObj_Character &SnapCur = GameClient()->m_Snap.m_aCharacters[i].m_Cur;
		const vec2 SnapTPos((float)SnapCur.m_X, (float)SnapCur.m_Y);
		const vec2 SnapTVel((float)SnapCur.m_VelX / 256.0f, (float)SnapCur.m_VelY / 256.0f);
		int SnapAge = Client()->GameTick(Dummy) - SnapCur.m_Tick;
		if(SnapAge < 0)
			SnapAge = 0;
		if(SnapAge > 10)
			continue; // слишком старый снап — не анализируем, будет мис
		const int H = SnapAge + OnewayTicks + 1;

		// check current position is visible (no wall between us right now)
		vec2 Floor, Col;
		if(Collision()->IntersectLine(MyPos, SnapTPos, &Col, &Floor) != 0 && distance(Floor, SnapTPos) >= 32.0f)
			continue;

		// где цель будет в момент исполнения удара
		const vec2 LeadTPos = SnapTPos + SnapTVel * (float)H;
		const float Dist = distance(LeadMyPos, LeadTPos);
		if(Dist > HammerSwitchRange)
			continue;

		// хит-тест как на сервере: старт 21px по направлению, радиус 42
		vec2 AimDir = vec2(0.0f, -1.0f);
		if(Dist >= 1.0f)
			AimDir = normalize(LeadTPos - LeadMyPos);
		const vec2 ProjStart = LeadMyPos + AimDir * HammerProjStart;
		const float HitDist = distance(ProjStart, LeadTPos);
		if(HitDist > HammerHitRadius)
			continue;

		if(Dist < BestDist)
		{
			BestDist = Dist;
			TargetId = i;
			TargetPos = LeadTPos;
			BestTargetVel = SnapTVel;
			BestHitDist = HitDist;
		}
	}

	if(TargetId < 0)
	{
		if(m_AutoHammerPrevWeapon >= 0 && ActiveWeapon == WEAPON_HAMMER)
			pOut->m_WantedWeapon = m_AutoHammerPrevWeapon + 1;
		else if(m_AutoHammerPrevWeapon >= 0)
			m_AutoHammerPrevWeapon = -1;
		return;
	}

	vec2 AimDir = vec2(0.0f, -1.0f);
	if(distance(TargetPos, LeadMyPos) >= 1.0f)
		AimDir = normalize(TargetPos - LeadMyPos);
	const vec2 AimPos = AimDir * GameClient()->m_Controls.GetMaxMouseDistance();

	if(ActiveWeapon != WEAPON_HAMMER)
	{
		if(m_AutoHammerPrevWeapon < 0)
			m_AutoHammerPrevWeapon = ActiveWeapon;
		pOut->m_WantedWeapon = WEAPON_HAMMER + 1;
		pOut->m_TargetX = (int)AimPos.x;
		pOut->m_TargetY = (int)AimPos.y;
		return;
	}

	// автохаммер всегда сайлент: мышь не трогаем, тумблера нет
	pOut->m_TargetX = (int)AimPos.x;
	pOut->m_TargetY = (int)AimPos.y;

	// бьём только надёжно: на грани хит-зоны сближающуюся цель ждём до
	// твёрдого момента (иначе свист вхолостую), уходящую добиваем сразу —
	// лучшего момента не будет. Так быстрые тишки не мажут.
	if(BestHitDist > HammerSolidRadius)
	{
		const vec2 RelPos = TargetPos - LeadMyPos;
		const vec2 RelVel = BestTargetVel - MyVel;
		if(dot(RelPos, RelVel) < 0.0f)
			return; // сближается — подождём, не мажем
	}

	// fire immediately — no cooldown delay, hammer as fast as server allows
	if(!m_AutoHammerHolding && (pOut->m_Fire & 1) == 0)
	{
		pOut->m_Fire = (pOut->m_Fire + 1) & INPUT_STATE_MASK;
		m_AutoHammerHolding = true;
	}
}
