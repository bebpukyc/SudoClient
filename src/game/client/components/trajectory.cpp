// (c) Kinetix. Trajectory prediction component — ported to BestClient.
//
// Implements the 5 per-type renderers + the "Show for current" entity iteration.

#include "trajectory.h"

#include <base/color.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#if defined(CONF_FAMILY_WINDOWS)
#include <malloc.h>
#endif

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/projectile.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

#include <algorithm>
#include <cmath>
#include <vector>

// Tuning access for grenade/laser prediction. TuningList() macro lives in
// gamecore.h (included via gameworld.h).
#include <game/gamecore.h>

// Line rendering settings for the trajectory (per-component Kinetix settings).
// Independent color: changing ESP color no longer touches the trajectory.
static inline unsigned KxTrajectoryColor() { return g_Config.m_KxLineTrajectoryColor; }
static inline int KxTrajectorySize() { return g_Config.m_KxLineTrajectorySize; }
static inline float KxTrajectoryAlpha() { return (float)g_Config.m_KxLineTrajectoryAlpha / 100.0f; }

void CTrajectory::SyncLegacyConfig()
{
	// m_KxShowTrajectory is the MASTER toggle (checked in OnRender). Only
	// m_KxTrajectoryTicks stays two-way synced with Tee's m_PredictionTicks.
	static int s_LastTicks = -1;
	static bool s_First = true;

	STypeSettings &tee = m_aTypes[TRAJ_TEE];

	// Мастер-тумблер применяется КАЖДЫЙ кадр: раньше tee.m_Show сидился
	// только на первом кадре, поэтому включение в меню было видно лишь
	// после перезапуска.
	tee.m_Show = g_Config.m_KxShowTrajectory != 0;

	if(s_First)
	{
		tee.m_PredictionTicks = g_Config.m_KxTrajectoryTicks;
		s_First = false;
	}
	else if(g_Config.m_KxTrajectoryTicks != s_LastTicks)
	{
		tee.m_PredictionTicks = g_Config.m_KxTrajectoryTicks;
	}

	g_Config.m_KxTrajectoryTicks = tee.m_PredictionTicks;
	s_LastTicks = tee.m_PredictionTicks;
}

void CTrajectory::DrawPolyline(const std::vector<vec2> &vPoints, bool AlphaGradient)
{
	if(vPoints.size() < 2)
		return;

	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	IGraphics *pGraphics = pGame->Graphics();
	if(!pGraphics)
		return;

	ColorRGBA BaseColor = color_cast<ColorRGBA>(ColorHSLA(KxTrajectoryColor(), true));
	float ConfigAlpha = KxTrajectoryAlpha();
	int LineSize = KxTrajectorySize();

	pGraphics->TextureClear();

	if(LineSize > 0)
	{
		std::vector<IGraphics::CFreeformItem> vQuads;
		vQuads.reserve(vPoints.size() - 1);
		float HalfWidth = 0.5f + (float)(LineSize - 1) * 0.25f;

		for(size_t i = 1; i < vPoints.size(); i++)
		{
			vec2 p0 = vPoints[i - 1];
			vec2 p1 = vPoints[i];
			vec2 Dir = normalize(p1 - p0);
			vec2 Perp = vec2(Dir.y, -Dir.x) * HalfWidth;

			vQuads.emplace_back(
				p0.x - Perp.x, p0.y - Perp.y,
				p0.x + Perp.x, p0.y + Perp.y,
				p1.x - Perp.x, p1.y - Perp.y,
				p1.x + Perp.x, p1.y + Perp.y);
		}

		pGraphics->QuadsBegin();
		for(size_t i = 0; i < vQuads.size(); i++)
		{
			float Alpha;
			if(AlphaGradient)
			{
				float t = (float)(i + 1) / (float)vQuads.size();
				Alpha = ConfigAlpha * (1.0f - t * 0.7f);
			}
			else
			{
				Alpha = ConfigAlpha;
			}
			pGraphics->SetColor(BaseColor.r, BaseColor.g, BaseColor.b, Alpha);
			pGraphics->QuadsDrawFreeform(&vQuads[i], 1);
		}
		pGraphics->QuadsEnd();
	}
	else
	{
		pGraphics->LinesBegin();
		for(size_t i = 1; i < vPoints.size(); i++)
		{
			float Alpha;
			if(AlphaGradient)
			{
				float t = (float)i / (float)vPoints.size();
				Alpha = ConfigAlpha * (1.0f - t * 0.7f);
			}
			else
			{
				Alpha = ConfigAlpha;
			}
			IGraphics::CLineItem Line(vPoints[i - 1], vPoints[i]);
			pGraphics->SetColor(BaseColor.r, BaseColor.g, BaseColor.b, Alpha);
			pGraphics->LinesDraw(&Line, 1);
		}
		pGraphics->LinesEnd();
	}
}

void CTrajectory::RenderTee()
{
	const STypeSettings &s = m_aTypes[TRAJ_TEE];
	if(!s.m_Show)
		return;

	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	int LocalClientId = pGame->m_Snap.m_LocalClientId;
	if(LocalClientId < 0 || !pGame->m_Snap.m_aCharacters[LocalClientId].m_Active)
		return;

	CCharacter *pLocalChar = pGame->m_PredictedWorld.GetCharacterById(LocalClientId);
	if(!pLocalChar)
		return;

	int NumTicks = s.m_PredictionTicks;
	if(NumTicks <= 0)
		return;

	// Determine which characters to predict.
	std::vector<int> vPredictIds;
	vPredictIds.reserve(MAX_CLIENTS);
	vPredictIds.push_back(LocalClientId);
	if(s.m_ShowForOtherPlayers)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == LocalClientId)
				continue;
			if(pGame->m_Snap.m_aCharacters[i].m_Active)
				vPredictIds.push_back(i);
		}
	}

	// Collect trajectory points for all predicted characters.
	// We use ONE cloned world for all characters (cheaper than per-character).
	static CGameWorld *s_pFutureWorld1 = nullptr;
	if(!s_pFutureWorld1)
		s_pFutureWorld1 = new CGameWorld();
	s_pFutureWorld1->Clear();
	s_pFutureWorld1->CopyWorld(&pGame->m_PredictedWorld);
	CGameWorld &FutureWorld = *s_pFutureWorld1;

	// If Simulate Players is OFF, remove other characters (keep local + entities).
	if(!s.m_SimulatePlayers)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == LocalClientId)
				continue;
			// Don't remove chars we want to predict.
			bool keep = false;
			for(int id : vPredictIds)
			{
				if(id == i)
				{
					keep = true;
					break;
				}
			}
			if(!keep)
			{
				if(CCharacter *pChar = FutureWorld.GetCharacterById(i))
				{
					FutureWorld.RemoveEntity(pChar);
					delete pChar;
				}
			}
		}
	}

	int StartTick = FutureWorld.GameTick();

	// Per-character point collection.
	std::vector<std::vector<vec2>> vPerCharPoints(vPredictIds.size());

	// Seed with the RENDERED position (m_aClients[cid].m_RenderPos) so the
	// trajectory start stays glued to the visible tee.
	for(size_t c = 0; c < vPredictIds.size(); c++)
	{
		int cid = vPredictIds[c];
		if(cid >= 0 && cid < MAX_CLIENTS)
			vPerCharPoints[c].push_back(pGame->m_aClients[cid].m_RenderPos);
	}

	// Simulate N ticks.
	for(int i = 0; i < NumTicks; i++)
	{
		int Tick = StartTick + i + 1;

		// Apply each character's last known input.
		for(size_t c = 0; c < vPredictIds.size(); c++)
		{
			CCharacter *pChar = FutureWorld.GetCharacterById(vPredictIds[c]);
			if(!pChar)
				continue;
			CNetObj_PlayerInput FutureInput = pChar->GetCore().m_Input;
			pChar->OnDirectInput(&FutureInput);
		}

		FutureWorld.m_GameTick = Tick;

		for(size_t c = 0; c < vPredictIds.size(); c++)
		{
			CCharacter *pChar = FutureWorld.GetCharacterById(vPredictIds[c]);
			if(!pChar)
				continue;
			CNetObj_PlayerInput FutureInput = pChar->GetCore().m_Input;
			pChar->OnPredictedInput(&FutureInput);
		}

		FutureWorld.Tick();

		// Collect positions (re-fetch in case a char was destroyed).
		for(size_t c = 0; c < vPredictIds.size(); c++)
		{
			CCharacter *pChar = FutureWorld.GetCharacterById(vPredictIds[c]);
			if(!pChar)
				continue;
			vPerCharPoints[c].push_back(pChar->m_Pos);
		}
	}

	// Draw each character's trajectory.
	for(size_t c = 0; c < vPerCharPoints.size(); c++)
	{
		DrawPolyline(vPerCharPoints[c], s.m_AlphaGradient);
	}
	s_pFutureWorld1->Clear();
}

// Helper: find a projectile owned by `ownerId` with matching `startTick`.
static CProjectile *FindOwnedProjectile(CGameWorld *pWorld, int ownerId, int startTick)
{
	for(CEntity *pEnt = pWorld->FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pEnt; pEnt = pEnt->TypeNext())
	{
		CProjectile *pProj = static_cast<CProjectile *>(pEnt);
		if(pProj->GetOwner() == ownerId && pProj->GetStartTick() == startTick)
			return pProj;
	}
	return nullptr;
}

// Helper: find a laser owned by `ownerId`.
static CLaser *FindOwnedLaser(CGameWorld *pWorld, int ownerId)
{
	for(CEntity *pEnt = pWorld->FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pEnt->TypeNext())
	{
		CLaser *pLaser = static_cast<CLaser *>(pEnt);
		if(pLaser->GetOwner() == ownerId)
			return pLaser;
	}
	return nullptr;
}

// Spawns the weapon entity in a cloned world, simulates N ticks, draws the path.
// ClientId selects which player to predict for. The aim source differs:
//   - LocalClientId: m_Controls.m_aMousePos (live mouse, most responsive)
//   - Other players: prediction character's m_Core.m_Input.m_TargetX/Y (from snapshots)
void CTrajectory::RenderWeaponPredict(int WeaponType, int ClientId)
{
	// Map WEAPON_* to TRAJ_* index for settings lookup.
	int trajIdx;
	switch(WeaponType)
	{
	case WEAPON_GUN: trajIdx = TRAJ_PISTOL; break;
	case WEAPON_SHOTGUN: trajIdx = TRAJ_SHOTGUN; break;
	case WEAPON_GRENADE: trajIdx = TRAJ_GRENADE; break;
	case WEAPON_LASER: trajIdx = TRAJ_LASER; break;
	default: return;
	}
	const STypeSettings &s = m_aTypes[trajIdx];
	if(!s.m_Show)
		return;

	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	int LocalClientId = pGame->m_Snap.m_LocalClientId;
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	if(!pGame->m_Snap.m_aCharacters[ClientId].m_Active)
		return;

	CCharacter *pChar = pGame->m_PredictedWorld.GetCharacterById(ClientId);
	if(!pChar)
		return;

	// Use the RENDERED position so the trajectory start aligns with the visible tee.
	vec2 MyPos = pGame->m_aClients[ClientId].m_RenderPos;
	vec2 AimPos;
	if(ClientId == LocalClientId)
		AimPos = pGame->m_Controls.m_aMousePos[g_Config.m_ClDummy];
	else
	{
		CNetObj_PlayerInput input = pChar->GetCore().m_Input;
		AimPos = vec2(input.m_TargetX, input.m_TargetY);
	}
	vec2 Dir = normalize(AimPos);
	if(length(Dir) < 1e-6f)
		Dir = vec2(1, 0); // fallback: aim right

	vec2 ProjStartPos = MyPos + Dir * pChar->GetProximityRadius() * 0.75f;

	// Clone the predicted world.
	static CGameWorld *s_pFutureWorld2 = nullptr;
	if(!s_pFutureWorld2)
		s_pFutureWorld2 = new CGameWorld();
	s_pFutureWorld2->Clear();
	s_pFutureWorld2->CopyWorld(&pGame->m_PredictedWorld);
	CGameWorld &FutureWorld = *s_pFutureWorld2;

	// Respect SimulatePlayers. When OFF (default), remove other characters.
	if(!s.m_SimulatePlayers)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == ClientId)
				continue;
			if(CCharacter *pOther = FutureWorld.GetCharacterById(i))
			{
				FutureWorld.RemoveEntity(pOther);
				delete pOther;
			}
		}
	}

	int StartTick = FutureWorld.GameTick();
	int TickSpeed = FutureWorld.GameTickSpeed();
	int TuneZone = pChar->GetOverriddenTuneZone();

	// Determine if shotgun uses laser (DDRace) or projectile (vanilla).
	const bool isShotgunLaser = (WeaponType == WEAPON_SHOTGUN && FutureWorld.m_WorldConfig.m_IsDDRace);

	// For vanilla shotgun: spawn 5 projectiles with spread.
	// For everything else: spawn 1 entity.
	struct SSpawned
	{
		int entType; // CGameWorld::ENTTYPE_PROJECTILE or ENTTYPE_LASER
		int startTick; // for projectile matching
	};
	std::vector<SSpawned> vSpawned;
	vSpawned.reserve(8);

	if(WeaponType == WEAPON_LASER || isShotgunLaser)
	{
		// Spawn CLaser. LaserReach from tuning.
		float LaserReach = FutureWorld.GetTuning(TuneZone)->m_LaserReach;
		int laserType = (WeaponType == WEAPON_LASER) ? WEAPON_LASER : WEAPON_SHOTGUN;
		new CLaser(&FutureWorld, MyPos, Dir, LaserReach, ClientId, laserType);
		vSpawned.push_back({CGameWorld::ENTTYPE_LASER, StartTick});
	}
	else if(WeaponType == WEAPON_GUN)
	{
		int Lifetime = (int)(TickSpeed * FutureWorld.GetTuning(TuneZone)->m_GunLifetime);
		new CProjectile(&FutureWorld, WEAPON_GUN, ClientId, ProjStartPos, Dir, Lifetime, false, false, -1);
		vSpawned.push_back({CGameWorld::ENTTYPE_PROJECTILE, StartTick});
	}
	else if(WeaponType == WEAPON_GRENADE)
	{
		int Lifetime = (int)(TickSpeed * FutureWorld.GetTuning(TuneZone)->m_GrenadeLifetime);
		new CProjectile(&FutureWorld, WEAPON_GRENADE, ClientId, ProjStartPos, Dir, Lifetime, false, true, SOUND_GRENADE_EXPLODE);
		vSpawned.push_back({CGameWorld::ENTTYPE_PROJECTILE, StartTick});
	}
	else if(WeaponType == WEAPON_SHOTGUN)
	{
		// Vanilla shotgun: 5 projectiles with spread.
		const float aSpreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
		int ShotSpread = 2;
		int Lifetime = (int)(TickSpeed * FutureWorld.GetTuning(TuneZone)->m_ShotgunLifetime);
		for(int i = -ShotSpread; i <= ShotSpread; ++i)
		{
			float a = angle(Dir) + aSpreading[i + 2];
			float v = 1 - (absolute(i) / (float)ShotSpread);
			float Speed = mix((float)FutureWorld.GlobalTuning()->m_ShotgunSpeeddiff, 1.0f, v);
			vec2 SpreadDir = direction(a) * Speed;
			new CProjectile(&FutureWorld, WEAPON_SHOTGUN, ClientId, ProjStartPos, SpreadDir, Lifetime, false, false, -1);
		}
		// All 5 projectiles share the same StartTick — draw the first one's path.
		vSpawned.push_back({CGameWorld::ENTTYPE_PROJECTILE, StartTick});
	}

	// Simulate N ticks, collecting positions for each spawned entity.
	std::vector<std::vector<vec2>> vPerEntityPoints(vSpawned.size());
	for(size_t e = 0; e < vSpawned.size(); e++)
	{
		vPerEntityPoints[e].push_back(ProjStartPos); // seed with spawn pos
	}

	for(int i = 0; i < s.m_PredictionTicks; i++)
	{
		FutureWorld.m_GameTick = StartTick + i + 1;
		FutureWorld.Tick();

		// Collect current position of each spawned entity.
		bool anyAlive = false;
		for(size_t e = 0; e < vSpawned.size(); e++)
		{
			vec2 pos;
			bool alive = false;
			if(vSpawned[e].entType == CGameWorld::ENTTYPE_PROJECTILE)
			{
				if(CProjectile *pProj = FindOwnedProjectile(&FutureWorld, ClientId, vSpawned[e].startTick))
				{
					// CProjectile::Tick does NOT update m_Pos for non-bouncing
					// projectiles (pistol/grenade). Use GetPos(Ct) for the real pos.
					float Ct = (FutureWorld.GameTick() - pProj->GetStartTick()) / (float)FutureWorld.GameTickSpeed();
					pos = pProj->GetPos(Ct);
					alive = true;
				}
			}
			else
			{
				if(CLaser *pLaser = FindOwnedLaser(&FutureWorld, ClientId))
				{
					pos = pLaser->m_Pos;
					alive = true;
				}
			}
			if(alive)
			{
				vPerEntityPoints[e].push_back(pos);
				anyAlive = true;
			}
		}
		if(!anyAlive)
			break;
	}

	// Draw each entity's trajectory.
	for(size_t e = 0; e < vPerEntityPoints.size(); e++)
	{
		DrawPolyline(vPerEntityPoints[e], s.m_AlphaGradient);
	}
	s_pFutureWorld2->Clear();
}

void CTrajectory::RenderCurrentProjectiles(int WeaponType)
{
	// "Show for current" for pistol/shotgun/grenade — clone the predicted world,
	// locate the matching projectile by (owner, startTick), simulate N ticks
	// forward, collect positions. Only the REMAINING path is drawn.
	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	CGameWorld *pWorld = &pGame->m_PredictedWorld;
	if(!pWorld)
		return;

	// Map WEAPON_* to TRAJ_* index.
	int trajIdx;
	switch(WeaponType)
	{
	case WEAPON_GUN: trajIdx = TRAJ_PISTOL; break;
	case WEAPON_SHOTGUN: trajIdx = TRAJ_SHOTGUN; break;
	case WEAPON_GRENADE: trajIdx = TRAJ_GRENADE; break;
	default: return; // unsupported weapon type
	}
	const STypeSettings &s = m_aTypes[trajIdx];

	int LocalClientId = pGame->m_Snap.m_LocalClientId;

	// collect matching projectiles as (owner, startTick) pairs.
	struct SProjKey { int owner; int startTick; };
	std::vector<SProjKey> vKeys;
	vKeys.reserve(32);
	for(CEntity *pEnt = pWorld->FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pEnt; pEnt = pEnt->TypeNext())
	{
		CProjectile *pProj = static_cast<CProjectile *>(pEnt);
		if(pProj->GetType() != WeaponType)
			continue;
		if(!s.m_ShowForOtherPlayers && pProj->GetOwner() != LocalClientId)
			continue;
		vKeys.push_back({pProj->GetOwner(), pProj->GetStartTick()});
	}
	if(vKeys.empty())
		return;

	// Clone the predicted world once for all matching projectiles.
	static CGameWorld *s_pFutureWorld3 = nullptr;
	if(!s_pFutureWorld3)
		s_pFutureWorld3 = new CGameWorld();
	s_pFutureWorld3->Clear();
	s_pFutureWorld3->CopyWorld(pWorld);
	CGameWorld &FutureWorld = *s_pFutureWorld3;

	// build keep set — local + owners of tracked projectiles.
	bool keepClient[MAX_CLIENTS] = {false};
	keepClient[LocalClientId] = true;
	for(const auto &k : vKeys)
		if(k.owner >= 0 && k.owner < MAX_CLIENTS)
			keepClient[k.owner] = true;

	if(!s.m_SimulatePlayers)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(keepClient[i])
				continue;
			if(CCharacter *pChar = FutureWorld.GetCharacterById(i))
			{
				FutureWorld.RemoveEntity(pChar);
				delete pChar;
			}
		}
	}

	int StartTick = FutureWorld.GameTick();

	// Per-projectile point collection. Seed with the projectile's CURRENT position.
	std::vector<std::vector<vec2>> vPerEntityPoints(vKeys.size());
	for(size_t e = 0; e < vKeys.size(); e++)
	{
		if(CProjectile *pProj = FindOwnedProjectile(&FutureWorld, vKeys[e].owner, vKeys[e].startTick))
		{
			float Ct = (FutureWorld.GameTick() - pProj->GetStartTick()) / (float)FutureWorld.GameTickSpeed();
			vPerEntityPoints[e].push_back(pProj->GetPos(Ct));
		}
	}

	// Simulate N ticks forward, collecting positions.
	for(int i = 0; i < s.m_PredictionTicks; i++)
	{
		FutureWorld.m_GameTick = StartTick + i + 1;
		FutureWorld.Tick();

		bool anyAlive = false;
		for(size_t e = 0; e < vKeys.size(); e++)
		{
			if(CProjectile *pProj = FindOwnedProjectile(&FutureWorld, vKeys[e].owner, vKeys[e].startTick))
			{
				float Ct = (FutureWorld.GameTick() - pProj->GetStartTick()) / (float)FutureWorld.GameTickSpeed();
				vPerEntityPoints[e].push_back(pProj->GetPos(Ct));
				anyAlive = true;
			}
		}
		if(!anyAlive)
			break;
	}

	// Draw each projectile's remaining trajectory.
	for(size_t e = 0; e < vPerEntityPoints.size(); e++)
	{
		DrawPolyline(vPerEntityPoints[e], s.m_AlphaGradient);
	}
	s_pFutureWorld3->Clear();
}

void CTrajectory::RenderCurrentLasers()
{
	// "Show for current" for laser — clone world, find the laser, simulate
	// N ticks (1 bounce/tick), collect positions. Only the REMAINING path is drawn.
	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	CGameWorld *pWorld = &pGame->m_PredictedWorld;
	if(!pWorld)
		return;

	const STypeSettings &s = m_aTypes[TRAJ_LASER];

	int LocalClientId = pGame->m_Snap.m_LocalClientId;

	// build keep set from matching lasers' owners.
	bool keepClient[MAX_CLIENTS] = {false};
	keepClient[LocalClientId] = true;
	int matchCount = 0;
	for(CEntity *pEnt = pWorld->FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pEnt->TypeNext())
	{
		CLaser *pLaser = static_cast<CLaser *>(pEnt);
		if(!s.m_ShowForOtherPlayers && pLaser->GetOwner() != LocalClientId)
			continue;
		if(pLaser->GetOwner() >= 0 && pLaser->GetOwner() < MAX_CLIENTS)
			keepClient[pLaser->GetOwner()] = true;
		matchCount++;
	}
	if(matchCount == 0)
		return;

	// Clone the predicted world.
	static CGameWorld *s_pFutureWorld4 = nullptr;
	if(!s_pFutureWorld4)
		s_pFutureWorld4 = new CGameWorld();
	s_pFutureWorld4->Clear();
	s_pFutureWorld4->CopyWorld(pWorld);
	CGameWorld &FutureWorld = *s_pFutureWorld4;

	// remove characters not in keep set (unless SimulatePlayers is on).
	if(!s.m_SimulatePlayers)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(keepClient[i])
				continue;
			if(CCharacter *pChar = FutureWorld.GetCharacterById(i))
			{
				FutureWorld.RemoveEntity(pChar);
				delete pChar;
			}
		}
	}

	// Collect matching lasers from the CLONE.
	std::vector<vec2> vInitialPos;
	vInitialPos.reserve(16);
	for(CEntity *pEnt = FutureWorld.FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pEnt->TypeNext())
	{
		CLaser *pLaser = static_cast<CLaser *>(pEnt);
		if(!s.m_ShowForOtherPlayers && pLaser->GetOwner() != LocalClientId)
			continue;
		vInitialPos.push_back(pLaser->m_Pos);
	}
	if(vInitialPos.empty())
		return;

	// Per-laser point collection. Seed with current position.
	std::vector<std::vector<vec2>> vPerLaserPoints(vInitialPos.size());
	for(size_t e = 0; e < vInitialPos.size(); e++)
		vPerLaserPoints[e].push_back(vInitialPos[e]);

	int StartTick = FutureWorld.GameTick();

	// Simulate N ticks forward. CLaser::Tick calls DoBounce() once per tick,
	// so each tick produces one bounce point.
	for(int i = 0; i < s.m_PredictionTicks; i++)
	{
		FutureWorld.m_GameTick = StartTick + i + 1;
		FutureWorld.Tick();

		bool anyAlive = false;
		size_t e = 0;
		for(CEntity *pEnt = FutureWorld.FindFirst(CGameWorld::ENTTYPE_LASER); pEnt && e < vPerLaserPoints.size(); pEnt = pEnt->TypeNext())
		{
			CLaser *pLaser = static_cast<CLaser *>(pEnt);
			if(!s.m_ShowForOtherPlayers && pLaser->GetOwner() != LocalClientId)
				continue;
			vPerLaserPoints[e].push_back(pLaser->m_Pos);
			anyAlive = true;
			e++;
		}
		if(!anyAlive)
			break;
	}

	// Draw each laser's remaining bounce path.
	for(size_t e = 0; e < vPerLaserPoints.size(); e++)
	{
		DrawPolyline(vPerLaserPoints[e], s.m_AlphaGradient);
	}
	s_pFutureWorld4->Clear();
}

void CTrajectory::OnRender()
{
	SyncLegacyConfig();

	// Master toggle — when OFF, nothing renders.
	if(!g_Config.m_KxShowTrajectory)
		return;

	CGameClient *pGame = GameClient();
	if(!pGame)
		return;

	int LocalClientId = pGame->m_Snap.m_LocalClientId;
	if(LocalClientId < 0 || !pGame->m_Snap.m_aCharacters[LocalClientId].m_Active)
		return;

	// Set up screen transform so world-space coordinates map correctly.
	Graphics()->MapScreenToInterface(pGame->m_Camera.m_Center.x, pGame->m_Camera.m_Center.y, pGame->m_Camera.m_Zoom);

	RenderTee();

	// Show the weapon trajectory PREVIEW for the currently held weapon + other
	// players when their weapon type's Show+ShowForOtherPlayers are both ON.
	{
		CCharacter *pLocalChar = pGame->m_PredictedWorld.GetCharacterById(LocalClientId);
		if(pLocalChar)
		{
			int weapon = pLocalChar->GetActiveWeapon();
			RenderWeaponPredict(weapon, LocalClientId);
		}

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == LocalClientId)
				continue;
			if(!pGame->m_Snap.m_aCharacters[i].m_Active)
				continue;
			CCharacter *pOtherChar = pGame->m_PredictedWorld.GetCharacterById(i);
			if(!pOtherChar)
				continue;
			int otherWeapon = pOtherChar->GetActiveWeapon();
			// Map weapon to traj index.
			int otherTrajIdx;
			switch(otherWeapon)
			{
			case WEAPON_GUN: otherTrajIdx = TRAJ_PISTOL; break;
			case WEAPON_SHOTGUN: otherTrajIdx = TRAJ_SHOTGUN; break;
			case WEAPON_GRENADE: otherTrajIdx = TRAJ_GRENADE; break;
			case WEAPON_LASER: otherTrajIdx = TRAJ_LASER; break;
			default: continue; // hammer/ninja: no preview
			}
			const STypeSettings &os = m_aTypes[otherTrajIdx];
			if(!os.m_Show || !os.m_ShowForOtherPlayers)
				continue;
			RenderWeaponPredict(otherWeapon, i);
		}
	}

	// "Show for current" — iterate existing entities.
	if(m_aTypes[TRAJ_PISTOL].m_ShowForCurrent)
		RenderCurrentProjectiles(WEAPON_GUN);
	if(m_aTypes[TRAJ_SHOTGUN].m_ShowForCurrent)
		RenderCurrentProjectiles(WEAPON_SHOTGUN);
	if(m_aTypes[TRAJ_GRENADE].m_ShowForCurrent)
		RenderCurrentProjectiles(WEAPON_GRENADE);
	if(m_aTypes[TRAJ_LASER].m_ShowForCurrent)
		RenderCurrentLasers();
}