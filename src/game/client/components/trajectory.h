// (c) Kinetix. Trajectory prediction component — ported to BestClient.
//
// Five trajectory types (Tee/Pistol/Shotgun/Grenade/Laser) each have
// independent Show / Prediction Ticks / Alpha Gradient / Simulate Players /
// Show for other players / Show for current settings.
//
// g_Config.m_KxShowTrajectory is the MASTER toggle (checked at the top of
// OnRender). Per-type m_Show is independent.
//
// Types:
//   0 = Tee      — predict tee movement N ticks forward
//   1 = Pistol   — spawn CProjectile(WEAPON_GUN) in cloned world, simulate N ticks
//   2 = Shotgun  — spawn CProjectile(WEAPON_SHOTGUN) x5 (vanilla) or CLaser (DDRace), simulate
//   3 = Grenade  — spawn CProjectile(WEAPON_GRENADE), simulate N ticks (real arc + bounces)
//   4 = Laser    — spawn CLaser(WEAPON_LASER), simulate N ticks (real bounces off walls)
//
// "Show for current" (Pistol/Shotgun/Grenade/Laser only) iterates the
// PREDICTED world's existing CProjectile / CLaser entities and draws their
// remaining trajectory. ShowForOtherPlayers works for show-for-current too.
//
// Settings are runtime-only (NOT in config) — same as the original. The
// legacy m_KxTrajectoryTicks config var stays two-way synced with m_aTypes[0]
// (Tee) m_PredictionTicks for backward compat with binds.

#ifndef GAME_CLIENT_COMPONENTS_TRAJECTORY_H
#define GAME_CLIENT_COMPONENTS_TRAJECTORY_H

#include <base/vmath.h>
#include <game/client/component.h>

#include <vector> // std::vector in DrawPolyline signature

class CTrajectory : public CComponent
{
public:
	// Trajectory type indices.
	static constexpr int TRAJ_TEE = 0;
	static constexpr int TRAJ_PISTOL = 1;
	static constexpr int TRAJ_SHOTGUN = 2;
	static constexpr int TRAJ_GRENADE = 3;
	static constexpr int TRAJ_LASER = 4;
	static constexpr int NUM_TRAJ_TYPES = 5;

	struct STypeSettings
	{
		bool m_Show = false;
		int m_PredictionTicks = 10;
		bool m_AlphaGradient = true; // ON = 1-t*0.7 (existing fade), OFF = uniform alpha
		bool m_SimulatePlayers = false; // ON = keep other players in sim, OFF = remove them
		bool m_ShowForOtherPlayers = false; // predict for other players too (Tee type)
		bool m_ShowForCurrent = false; // draw existing projectiles (Pistol/Shotgun/Grenade/Laser)
	};

	STypeSettings m_aTypes[NUM_TRAJ_TYPES];
	int m_SelectedType = 0;

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

private:
	// Per-type renderers. Each reads its STypeSettings and draws if m_Show.
	void RenderTee();
	// Spawns a real CProjectile/CLaser in a cloned CGameWorld and simulates
	// N ticks, collecting positions each tick.
	void RenderWeaponPredict(int WeaponType, int ClientId);

	// "Show for current" helpers — iterate prediction-world entities.
	void RenderCurrentProjectiles(int WeaponType);
	void RenderCurrentLasers();

	// Shared line drawing helper — draws a polyline with optional alpha gradient.
	void DrawPolyline(const std::vector<vec2> &vPoints, bool AlphaGradient);

	// Sync legacy m_KxShowTrajectory / m_KxTrajectoryTicks <-> m_aTypes[TRAJ_TEE].
	void SyncLegacyConfig();
};

#endif // GAME_CLIENT_COMPONENTS_TRAJECTORY_H
