#include "aimbot.h"

#include "steal_fov.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/components/binds.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <cmath>

void CAimbot::OnReset()
{
	m_AaTargetId = -1;
	m_AaTargetPos = vec2(0.0f, 0.0f);
	m_AaHookAuto = false;
	m_AaHookAutoTick = 0;
	m_AaHookPlayerTarget = false;
	m_AaHookDropped = false;
	m_HookSpamActive = false;
	m_HookSpamTick = 0;
}

void CAimbot::OnPlayerInput(CNetObj_PlayerInput *pOut, CNetObj_PlayerInput *pPersistent)
{
	DoAimAssist(pOut, pPersistent);
	DoHookSpam(pOut);
}

// ===== steal/aimbot ported from 2.1 =====

// Prikoli: физическое удержание клавиши хука. m_Hook из инпута читать напрямую
// нельзя — наш собственный OFF-тик круга тоже пишет туда 0, и круг принял бы
// его за «отпустил кнопку» и умер после первого же обрыва.
bool CAimbot::IsHookKeyPhysicallyHeld()
{
	const int CurMask = CBinds::GetModifierMask(Input());
	for(int Key = KEY_FIRST; Key < KEY_LAST; Key++)
	{
		if(Key == KEY_LSHIFT || Key == KEY_RSHIFT || Key == KEY_LCTRL || Key == KEY_RCTRL || Key == KEY_LALT || Key == KEY_RALT || Key == KEY_LGUI || Key == KEY_RGUI)
			continue;
		if(!Input()->KeyIsPressed(Key))
			continue;
		if(str_comp(GameClient()->m_Binds.Get(Key, CurMask), "+hook") == 0)
			return true;
		if(CurMask != KeyModifier::NONE && str_comp(GameClient()->m_Binds.Get(Key, KeyModifier::NONE), "+hook") == 0)
			return true;
	}
	return false;
}

void CAimbot::DoAimAssist(CNetObj_PlayerInput *pOut, CNetObj_PlayerInput *pPersistent)
{
	const int Dummy = g_Config.m_ClDummy;
	if(!g_Config.m_AaEnabled)
	{
		m_AaHookAuto = false;
		m_AaHookAutoTick = 0;
		m_AaHookPlayerTarget = false;
		m_AaHookDropped = false;
		m_HookSpamActive = false;
		m_HookSpamTick = 0;
		return;
	}
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_pLocalCharacter)
		return;

	// steal: one aimbot for both actions. Hook snaps once on press (aimed
	// and thrown instantly); the spam circle below drives the re-throws
	// while held.
	const int ActiveWeapon = GameClient()->m_PredictedChar.m_ActiveWeapon;

	const bool RawHook = pOut->m_Hook != 0;
	// наш OFF-тик (m_AaHookDropped) — не «отпустил», смотрим физическую клавишу
	const bool HookHeld = IsHookKeyPhysicallyHeld() || (RawHook && !m_AaHookDropped);
	// в блок-ветке спама нет, так что m_Hook там всегда пользовательский
	const bool HookJustPressed = RawHook && (pPersistent->m_Hook == 0);
	const bool FireJustPressed = ((pOut->m_Fire & 1) != 0 && (pPersistent->m_Fire & 1) == 0);
	// HookSpam сам доводит хук до тишки: ему не нужна карточка Hook, иначе
	// хук просто держится (зажимает) вместо спама.
	const bool Hooking = HookHeld && (GetStealAimbotEnabled(ActiveWeapon, true) || g_Config.m_AaHookSpam != 0);
	// Огонь тоже только в тик нажатия: зажал ЛКМ — дальше прицел свободен.
	const bool Firing = FireJustPressed && GetStealAimbotEnabled(ActiveWeapon, false);
	if(!Hooking && !Firing)
	{
		// хук отпущен / нечего аимить — сбрасываем круг и лок, хук не трогаем
		m_AaHookAuto = false;
		m_AaHookAutoTick = 0;
		m_AaHookPlayerTarget = false;
		m_AaHookDropped = false;
		m_HookSpamActive = false;
		m_HookSpamTick = 0;
		m_AaTargetId = -1;
		m_AaTargetPos = vec2(0.0f, 0.0f);
		return;
	}

	const float AaCurFov = (float)GetStealAimbotFov(ActiveWeapon, Hooking);
	const bool AaSilent = GetStealAimbotSilent(ActiveWeapon, Hooking);

	const vec2 MyPos = GameClient()->m_PredictedChar.m_Pos;
	const float HookLen = GameClient()->m_aTuning[Dummy].m_HookLength;
	const vec2 AimDirInput = normalize(vec2((float)pOut->m_TargetX, (float)pOut->m_TargetY));

	// ---- Block candidate ----
	// Hook-only: scan for hookable tiles and find the EXACT pixel on the edge
	// (including corner pixels between two blocks) for pixel-perfect hooking.
	bool BlockCandidate = false;
	vec2 BlockPos = vec2(0.0f, 0.0f);
	float BlockScore = -1e9f;
	if(Hooking && g_Config.m_AaPreferBlock)
	{
		const vec2 Perp = vec2(-AimDirInput.y, AimDirInput.x);
		const float HalfFov = AaCurFov * 0.5f * pi / 180.0f;
		float MaxPerp = tan(HalfFov) * HookLen;
		if(MaxPerp < 16.0f)
			MaxPerp = 16.0f;
		if(MaxPerp > 128.0f)
			MaxPerp = 128.0f;

		// Phase 1: find solid tiles via ray-cast
		float aOffsets[17];
		aOffsets[0] = 0.0f;
		for(int o = 1; o < 17; o++)
		{
			const int Sign = (o % 2 == 1) ? 1 : -1;
			const int Step = (o + 1) / 2;
			aOffsets[o] = Sign * MaxPerp * ((float)Step / 8.0f);
		}

		for(int o = 0; o < 17; o++)
		{
			const vec2 RayStart = MyPos + Perp * aOffsets[o] + AimDirInput * 28.0f;
			vec2 BlockHit, BlockBefore;
			int TeleNr = 0;
			const int BlockTile = Collision()->IntersectLineTeleHook(RayStart, RayStart + AimDirInput * HookLen, &BlockHit, &BlockBefore, &TeleNr);
			if(BlockTile == 0 || BlockTile == TILE_NOHOOK)
				continue;
			const int BlockIndex = Collision()->GetMapIndex(BlockHit);
			const int TileIdx = BlockIndex >= 0 ? Collision()->GetTileIndex(BlockIndex) : 0;
			if(TileIdx == TILE_NOHOOK)
				continue;
			if(BlockIndex >= 0 && Collision()->GetFrontTileIndex(BlockIndex) == TILE_NOHOOK)
				continue;

			const vec2 BehindPos = BlockHit + AimDirInput * 32.0f;
			const int BehindIndex = Collision()->GetMapIndex(BehindPos);
			if(BehindIndex >= 0 && (Collision()->GetTileIndex(BehindIndex) == TILE_NOHOOK || Collision()->GetFrontTileIndex(BehindIndex) == TILE_NOHOOK))
				continue;

			const float DistRay = length(BlockHit - MyPos);
			if(DistRay > HookLen || DistRay < 40.0f)
				continue;

			vec2 FirstCol, FirstBefore;
			int TeleNr2 = 0;
			const int FirstTile = Collision()->IntersectLineTeleHook(MyPos, BlockHit, &FirstCol, &FirstBefore, &TeleNr2);
			if(FirstTile != 0 && distance(FirstCol, BlockHit) > 16.0f)
				continue;

			// Phase 2: refine to the EXACT pixel on the tile edge/corner.
			// Snap BlockHit to the nearest 32x32 tile grid corner or edge midpoint.
			// This is what makes the hook pixel-perfect in corner situations.
			const int TileX = (int)floor(BlockHit.x / 32.0f);
			const int TileY = (int)floor(BlockHit.y / 32.0f);

			// Check all 12 edge points of the hit tile (4 corners + 4 edge midpoints,
			// but only the ones facing the tee)
			vec2 BestEdgePos = BlockHit;
			float BestEdgeDist = 999.0f;
			for(int ey = 0; ey <= 2; ey++)
			{
				for(int ex = 0; ex <= 2; ex++)
				{
					if(ex == 1 && ey == 1)
						continue; // skip center of tile (inside solid)
					const vec2 EdgePt = vec2((float)(TileX * 32 + ex * 16), (float)(TileY * 32 + ey * 16));
					const float EdgeDist = length(EdgePt - MyPos);
					if(EdgeDist > HookLen || EdgeDist < 32.0f)
						continue;
					// The edge point must have line-of-sight from the tee
					vec2 ECol, EBefore;
					const int ETile = Collision()->IntersectLineTeleHook(MyPos, EdgePt, &ECol, &EBefore, &TeleNr);
					if(ETile != 0 && distance(ECol, EdgePt) > 8.0f)
						continue;
					// Score: prefer edges close to the aim direction
					const vec2 ToEdge = EdgePt - MyPos;
					const float Align = dot(normalize(AimDirInput), normalize(ToEdge));
					const float Score = Align * 2.0f - EdgeDist / HookLen * 0.5f;
					if(Score > BestEdgeDist)
					{
						BestEdgeDist = Score;
						BestEdgePos = EdgePt;
					}
				}
			}

			const float PerpOff = absolute(aOffsets[o]);
			const float NormOff = MaxPerp > 0.0f ? MaxPerp : 96.0f;
			float Score = 2.0f - PerpOff / NormOff - DistRay / HookLen;
			Score += BestEdgeDist * 0.5f;
			if(m_AaTargetId == -1 && distance(m_AaTargetPos, BestEdgePos) < 32.0f)
				Score += 1.0f;
			if(Score > BlockScore)
			{
				BlockScore = Score;
				BlockPos = BestEdgePos;
				BlockCandidate = true;
			}
		}
	}

	// ---- Find an enemy with aim preference ----
	// The aimbot prefers the player whose predicted pixel is closest to the current aim
	// direction. A small bonus is given to the previously chosen target so it keeps its
	// preference, and it switches when you re-aim at somebody else.
	float BestScore = -1e9f;
	int TargetId = -1;
	vec2 BestPredPos = vec2(0.0f, 0.0f);

	// Доводка СТРОГО внутри FOV от текущего прицела: увёл мышь в сторону —
	// лок слетает и прицел свободен. Небольшой бонус текущей цели (+0.25
	// ниже) держит её пока она в конусе, без приклеивания.
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == LocalId || !GameClient()->m_Snap.m_aCharacters[i].m_Active || GameClient()->IsOtherTeam(i))
			continue;
		const CGameClient::CClientData &Data = GameClient()->m_aClients[i];
		if(!Data.m_Active)
			continue;

		// current position of the target (no motion prediction)
		vec2 Pos = Data.m_Predicted.m_Pos;
		const vec2 ToTarget = Pos - MyPos;
		const float Dist = length(ToTarget);
		if(Dist > HookLen)
			continue;

		// FOV check
		const vec2 AimDir = normalize(vec2((float)pOut->m_TargetX, (float)pOut->m_TargetY));
		const float Align = dot(AimDir, ToTarget / Dist);
		if(length(AimDir) > 0.001f && Align < cos(AaCurFov * 0.5f * pi / 180.0f))
			continue;

		// Hook can pass through 1-tile gaps — use the actual hook collision function
		// so visibility matches what the hook engine sees.
		if(Hooking)
		{
			vec2 HookCol, HookBefore;
			int HookTeleNr = 0;
			const int HookHit = Collision()->IntersectLineTeleHook(MyPos, Pos, &HookCol, &HookBefore, &HookTeleNr);
			if(HookHit != 0 && distance(HookCol, Pos) > 4.0f)
				continue;
		}
		else
		{
			vec2 Col, Floor;
			if(Collision()->IntersectLine(MyPos, Pos, &Col, &Floor) != 0)
				continue;
		}

		float Score;
		if(g_Config.m_AaClosest)
		{
			// the closest tee wins in BOTH axes: whoever is nearest to your tee,
			// both horizontally and vertically, gets hooked
			float MaxHook = GameClient()->m_aTuning[Dummy].m_HookLength;
			if(MaxHook <= 0.0f)
				MaxHook = HookLen;
			Score = (1.0f - Dist / (MaxHook > 0.0f ? MaxHook : 1.0f)) * 4.0f;
		}
		else
			Score = Align * 2.0f + (1.0f - Dist / HookLen) * 0.25f;
		if(i == m_AaTargetId)
			Score += 0.25f; // keep preference on the current target

		if(Score > BestScore)
		{
			BestScore = Score;
			TargetId = i;
			BestPredPos = Pos;
		}
	}

	// ---- Decide ----
	if(Hooking)
	{
		// HOOK path: навёлся и хукнул в тик нажатия, дальше прицел свободен.
		if(TargetId >= 0)
		{
			m_AaTargetId = TargetId;
			m_AaTargetPos = BestPredPos;
			m_AaHookPlayerTarget = true;
			if(HookJustPressed)
			{
				// быстро навёлся на тишку и хукнул
				const vec2 Dir = normalize(BestPredPos - MyPos);
				const vec2 AimPos = vec2((int)(Dir.x * GameClient()->m_Controls.GetMaxMouseDistance()), (int)(Dir.y * GameClient()->m_Controls.GetMaxMouseDistance()));
				pOut->m_TargetX = (int)AimPos.x;
				pOut->m_TargetY = (int)AimPos.y;
				if(!AaSilent)
					GameClient()->m_Controls.m_aMousePos[Dummy] = AimPos;
			}

			if(!g_Config.m_AaHookSpam)
			{
				// спам выключен → обычный хук: держим пока зажата кнопка
				m_AaHookAuto = false;
				m_AaHookAutoTick = 0;
				m_AaHookDropped = false;
				m_HookSpamActive = false;
				m_HookSpamTick = 0;
				pOut->m_Hook = 1;
				return;
			}

			// спам-круг: хук летит пока не ДОТРОНЕТСЯ до тишки, потом
			// обрыв на 1 тик и хук заново — без задержек, пока держишь кнопку
			m_AaHookAuto = true;
			m_HookSpamActive = true;
			int PredId = LocalId;
			if(Dummy >= 0 && Dummy < NUM_DUMMIES && GameClient()->m_aLocalIds[Dummy] >= 0 && GameClient()->m_aLocalIds[Dummy] < MAX_CLIENTS)
				PredId = GameClient()->m_aLocalIds[Dummy];
			const int HookedPlayer = GameClient()->m_aClients[PredId].m_Predicted.HookedPlayer();
			const int MaxOnTicks = 18; // время полёта хука на макс. дальности
			const int OffTicks = 1; // отпустил → сразу хук заново
			if(m_AaHookAutoTick < 0)
			{
				// фаза обрыва: 0 уходит на сервер, флаг помнит что кнопка зажата
				pOut->m_Hook = 0;
				m_AaHookDropped = true;
				if(++m_AaHookAutoTick >= 0)
					m_AaHookAutoTick = 0;
			}
			else
			{
				pOut->m_Hook = 1;
				m_AaHookDropped = false;
				if(HookedPlayer >= 0)
					m_AaHookAutoTick = -OffTicks; // дотронулся до тишки → отпустить
				else if(++m_AaHookAutoTick > MaxOnTicks)
					m_AaHookAutoTick = -OffTicks; // промах/таймаут → перекинуть
			}
			return;
		}

		// тишки нет — блок или пустота: держим хук как есть, НЕ спамим
		m_AaHookAuto = false;
		m_AaHookAutoTick = 0;
		m_AaHookPlayerTarget = false;
		m_AaHookDropped = false;
		m_HookSpamActive = false;
		m_HookSpamTick = 0;
		if(BlockCandidate)
		{
			// БЛОК НЕ ДВИГАЕТСЯ: снапим на него ТОЛЬКО в тик нажатия. Пока
			// кнопка зажата прицел свободен — доводка к углу блока каждый
			// тик уводила прицел, когда тащишь мимо стен.
			m_AaTargetId = -1;
			if(HookJustPressed)
			{
				m_AaTargetPos = BlockPos;
				const vec2 Dir = normalize(BlockPos - MyPos);
				const vec2 AimPos = vec2((int)(Dir.x * GameClient()->m_Controls.GetMaxMouseDistance()), (int)(Dir.y * GameClient()->m_Controls.GetMaxMouseDistance()));
				pOut->m_TargetX = (int)AimPos.x;
				pOut->m_TargetY = (int)AimPos.y;
				if(!AaSilent)
				{
					GameClient()->m_Controls.m_aMousePos[Dummy] = AimPos;
					pOut->m_TargetX = (int)AimPos.x;
					pOut->m_TargetY = (int)AimPos.y;
				}
			}
		}
		else
		{
			m_AaTargetId = -1;
			m_AaTargetPos = vec2(0.0f, 0.0f);
		}
		return;
	}

	// FIRE path: players only, blocks never. One-shot on press, continuous
	// tracking while FastFire holds the trigger.
	if(TargetId < 0)
	{
		m_AaTargetId = -1;
		m_AaTargetPos = vec2(0.0f, 0.0f);
		return;
	}

	// pixel-precise aim at the predicted target position
	m_AaTargetId = TargetId;
	m_AaTargetPos = BestPredPos;
	const vec2 Dir = normalize(BestPredPos - MyPos);
	const vec2 AimPos = vec2((int)(Dir.x * GameClient()->m_Controls.GetMaxMouseDistance()), (int)(Dir.y * GameClient()->m_Controls.GetMaxMouseDistance()));

	pOut->m_TargetX = (int)AimPos.x;
	pOut->m_TargetY = (int)AimPos.y;
	if(!AaSilent)
	{
		GameClient()->m_Controls.m_aMousePos[Dummy] = AimPos;
		pOut->m_TargetX = (int)AimPos.x;
		pOut->m_TargetY = (int)AimPos.y;
	}
}

// Prikoli: hook-spam circle. The aimbot (DoAimAssist) owns the hook while it
// tracks a tee — this function never fights it. Standalone it only spams when
// the aimbot has a live player locked; blocks/walls/empty air → plain hold.
void CAimbot::DoHookSpam(CNetObj_PlayerInput *pOut)
{
	// аимбот ведёт круг сам — не перезаписываем его хук
	if(m_AaHookAuto)
	{
		m_HookSpamActive = true;
		return;
	}
	if(!g_Config.m_AaHookSpam)
	{
		// выключено — полностью выходим, хук не трогаем
		m_HookSpamActive = false;
		m_HookSpamTick = 0;
		m_AaHookDropped = false;
		return;
	}
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_pLocalCharacter)
		return;

	const bool RawHook = pOut->m_Hook != 0;
	const bool HookHeld = IsHookKeyPhysicallyHeld() || (RawHook && !m_AaHookDropped);
	if(!HookHeld)
	{
		m_HookSpamActive = false;
		m_HookSpamTick = 0;
		m_AaHookDropped = false;
		return;
	}

	// спам только если аимбот реально залочил живую тишку.
	// Блок/стена/пустота → выходим, хук держится как есть.
	if(!m_AaHookPlayerTarget || m_AaTargetId < 0 || !GameClient()->m_Snap.m_aCharacters[m_AaTargetId].m_Active)
	{
		m_HookSpamActive = false;
		m_HookSpamTick = 0;
		m_AaHookDropped = false;
		return;
	}

	// запасной цикл (обычно аимбот уже всё сделал выше): 3 ON / 1 OFF
	m_HookSpamActive = true;
	const int HOOK_ON_TICKS = 3;
	m_HookSpamTick++;
	if(m_HookSpamTick > HOOK_ON_TICKS)
	{
		pOut->m_Hook = 0;
		m_AaHookDropped = true;
		m_HookSpamTick = 0;
	}
	else
	{
		pOut->m_Hook = 1;
		m_AaHookDropped = false;
	}
}

// Prikoli: fast-fire - while the fire button is held, re-press it every frame
// so the server keeps seeing new presses, firing as fast as the weapon reload
// allows (~every tick instead of waiting for a human click).
void CAimbot::DoFastFire(CNetObj_PlayerInput *pOut)
{
	if(!g_Config.m_AaFastFire)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_Snap.m_pLocalCharacter)
		return;

	// only when the player is actively holding the fire button
	const bool FireHeld = (pOut->m_Fire & 1) != 0;
	if(!FireHeld)
		return;

	const int PredTick = Client()->PredGameTick(Dummy);
	// keep the fire bit set but roll the counter forwards every tick so each
	// frame counts as a fresh press (CountInput on the server counts presses)
	pOut->m_Fire = (pOut->m_Fire + 2) & INPUT_STATE_MASK;
	(void)PredTick;
}
