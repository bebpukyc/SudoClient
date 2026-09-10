#include "autoaled.h"

#include <base/vmath.h>

#include <engine/shared/config.h>

#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

#include <algorithm>

static bool AledIsFreezeTile(const CCollision *pCol, int Index)
{
	if(Index < 0)
		return false;
	const int T = pCol->GetTileIndex(Index);
	const int F = pCol->GetFrontTileIndex(Index);
	if(T == TILE_FREEZE || T == TILE_DFREEZE || T == TILE_LFREEZE)
		return true;
	if(F == TILE_FREEZE || F == TILE_DFREEZE || F == TILE_LFREEZE)
		return true;
	if(pCol->SwitchLayer())
	{
		const int S = pCol->GetSwitchType(Index);
		if(S == TILE_FREEZE || S == TILE_DFREEZE || S == TILE_LFREEZE)
			return true;
	}
	return false;
}

static bool AledIsFreezeAt(const CCollision *pCol, vec2 Pos)
{
	const int Nx = std::clamp((int)Pos.x / 32, 0, pCol->GetWidth() - 1);
	const int Ny = std::clamp((int)Pos.y / 32, 0, pCol->GetHeight() - 1);
	const int Idx = Ny * pCol->GetWidth() + Nx;
	return AledIsFreezeTile(pCol, Idx);
}

void CAutoAled::OnPlayerInput(CNetObj_PlayerInput *pOut, CNetObj_PlayerInput *pPersistent)
{
	if(!g_Config.m_AaAutoAled)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(!GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
		return;

	const CCharacterCore &Me = GameClient()->m_PredictedChar;
	if(Me.m_HammerHitDisabled || !Me.m_aWeapons[WEAPON_HAMMER].m_Got)
		return;

	const int PredTick = Client()->PredGameTick(Dummy);
	if(Me.m_DeepFrozen || Me.m_FreezeEnd > PredTick)
		return;

	if(m_AledLastTick >= 0 && PredTick - m_AledLastTick < 4)
		return;

	const float Size = CCharacterCore::PhysicalSize();
	const float Strength = (float)Me.m_Tuning.m_HammerStrength;

	// исполнение удара на сервере позже на ~полпинга+тик
	const int AledPingMs = GameClient()->m_Snap.m_pLocalInfo ? GameClient()->m_Snap.m_pLocalInfo->m_Latency : 40;
	int ExecLeadTicks = 1 + (AledPingMs + 20) / 40;
	if(ExecLeadTicks < 1)
		ExecLeadTicks = 1;
	if(ExecLeadTicks > 5)
		ExecLeadTicks = 5;
	const int ActiveWeapon = GameClient()->m_PredictedChar.m_ActiveWeapon;
	// экстра-счётчик нажатий не копим между тиками, иначе раз в 64 свинга
	// счётчик оборачивается в ноль и удар тихо пропадает
	m_AledFireExtra = 0;
	bool SawFrozen = false;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == LocalId || !GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		if(GameClient()->IsOtherTeam(i))
			continue;
		// сервер скипает удар по тем, с кем нет коллизии, — не машем вхолостую
		if(!GameClient()->m_Teams.CanCollide(LocalId, i))
			continue;
		// Чужого берём из m_RegularPredicted, а НЕ из m_Predicted: дальний
		// предикт симулируется с чужими пре-инпутами/оверпредиктом
		// (FinalTickOthers) и врёт про позицию/скорость/фриз — отсюда мисы.
		// У себя предикт точный. Так же делают и фризбары.
		const CCharacterCore &T = GameClient()->m_aClients[i].m_RegularPredicted;

		if(T.m_FreezeEnd <= PredTick || T.m_DeepFrozen)
			continue;
		SawFrozen = true;

		// молот в руки заранее: замороженный виден в радиусе подхода —
		// первый удар не уйдёт из пистолета, пока свитч долетает до сервера
		if(ActiveWeapon != WEAPON_HAMMER && distance(Me.m_Pos, T.m_Pos) <= 100.0f)
		{
			vec2 FloorPre, ColPre;
			if(Collision()->IntersectLine(Me.m_Pos, T.m_Pos, &ColPre, &FloorPre) == 0 || distance(FloorPre, T.m_Pos) < 32.0f)
			{
				if(m_AledPrevWeapon < 0)
					m_AledPrevWeapon = ActiveWeapon;
				pOut->m_WantedWeapon = WEAPON_HAMMER + 1;
				m_AledSwitchTick = PredTick;
			}
		}


		// Ведём вперёд только дальних: вблизи на скорости лид пролетает мимо
		// цели и инвертирует прицел — там сырая геометрия и так в допуске 42.
		vec2 LeadMyPos = Me.m_Pos;
		vec2 LeadTPos = T.m_Pos;
		if(distance(T.m_Pos, Me.m_Pos) >= 35.0f)
		{
			const vec2 TryMy = Me.m_Pos + Me.m_Vel * (float)ExecLeadTicks;
			const vec2 TryT = T.m_Pos + T.m_Vel * (float)ExecLeadTicks;
			if(distance(TryT, TryMy) >= 25.0f)
			{
				LeadMyPos = TryMy;
				LeadTPos = TryT;
			}
		}
		const float Dist = distance(LeadMyPos, LeadTPos);
		if(Dist < 0.001f || Dist > Size * 0.75f + Size * 0.5f + Size)
			continue;

		// фриз-гейта нет: замороженного бьём где угодно — в чистом поле,
		// через стену, рядом. Стены удару не мешают (милишник), а посадку
		// проверяет симуляция ниже — плохой рескью она зарубит.

		// старт вылета считаем уже из лид-позиций (см. выше)
		vec2 Aim = vec2(0.0f, -1.0f);
		if(distance(LeadTPos, LeadMyPos) >= 1.0f)
			Aim = normalize(LeadTPos - LeadMyPos);
		const vec2 ProjStart = LeadMyPos + Aim * (Size * 0.75f);
		if(distance(LeadTPos, ProjStart) >= Size * 0.5f + Size)
			continue;

		const vec2 Temp = normalize(Aim + vec2(0.0f, -1.1f)) * 10.0f;
		const vec2 Force = (vec2(0.0f, -1.0f) + Temp) * Strength;

		CCharacterCore Sim = T;
		Sim.SetCoreWorld(nullptr, Collision(), nullptr);
		Sim.m_Vel += Force;
		Sim.m_FreezeEnd = 0;

		CNetObj_PlayerInput Empty = {};
		Empty.m_TargetY = -1;
		Sim.m_Input = Empty;
		bool Clear = true;
		vec2 Prev = Sim.m_Pos;
		const int Ticks = std::clamp(g_Config.m_AaAutoAledTicks, 5, 100);
		for(int t = 0; t < Ticks && Clear; t++)
		{
			Sim.Tick(true);
			Sim.Move();
			Sim.Quantize();

			const float d = distance(Prev, Sim.m_Pos);
			const int Steps = (d == 0.0f) ? 1 : (int)(d + 1.0f);
			for(int k = 0; k < Steps; k++)
			{
				if(AledIsFreezeAt(Collision(), mix(Prev, Sim.m_Pos, (d == 0.0f) ? 0.0f : (float)k / d)))
				{
					Clear = false;
					break;
				}
			}
			Prev = Sim.m_Pos;
		}
		if(!Clear)
			continue;

		// make sure the hammer is the active weapon (ActiveWeapon взят выше цикла)
		if(ActiveWeapon != WEAPON_HAMMER)
		{
			if(m_AledPrevWeapon < 0)
				m_AledPrevWeapon = ActiveWeapon;
			pOut->m_WantedWeapon = WEAPON_HAMMER + 1;
			m_AledSwitchTick = PredTick;
			const int Tx = (int)(Aim.x * GameClient()->m_Controls.GetMaxMouseDistance());
			const int Ty = (int)(Aim.y * GameClient()->m_Controls.GetMaxMouseDistance());
			if(Tx == 0 && Ty == 0)
				return;
			pOut->m_TargetX = Tx;
			pOut->m_TargetY = Ty;
			return;
		}

		const int Tx = (int)(Aim.x * GameClient()->m_Controls.GetMaxMouseDistance());
		const int Ty = (int)(Aim.y * GameClient()->m_Controls.GetMaxMouseDistance());
		if(Tx == 0 && Ty == 0)
			return;
		pOut->m_TargetX = Tx;
		pOut->m_TargetY = Ty;

		// свитч долетает до сервера с задержкой пинга: бьём каплю позже,
		// иначе первый удар уйдёт из старого оружия вхолостую
		if(m_AledSwitchTick >= 0 && PredTick - m_AledSwitchTick < ExecLeadTicks)
			return;

		m_AledFireExtra += 2;
		{
			int Fire = (pPersistent->m_Fire + m_AledFireExtra) & INPUT_STATE_MASK;
			if((pPersistent->m_Fire & 1) && (Fire & 1) == 0)
				Fire = (Fire + 1) & INPUT_STATE_MASK;
			pOut->m_Fire = Fire;
		}
		m_AledLastTick = PredTick;
		return;
	}

	// никого не бьём — вернуть оружие, если брали мы (вперёд-назад).
	// Только когда замороженных рядом вообще нет, иначе будет дёргать
	// свитч туда-сюда каждый тик.
	if(!SawFrozen && m_AledPrevWeapon >= 0)
	{
		if(ActiveWeapon == WEAPON_HAMMER)
			pOut->m_WantedWeapon = m_AledPrevWeapon + 1;
		else
			m_AledPrevWeapon = -1;
	}
}
