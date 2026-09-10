#ifndef GAME_CLIENT_COMPONENTS_AIMBOT_AIMBOT_H
#define GAME_CLIENT_COMPONENTS_AIMBOT_AIMBOT_H

#include <base/vmath.h>

#include <generated/protocol.h>

#include <game/client/component.h>

// Prikoli/steal aimbot: hook snap on press, weapon aim while firing,
// hook-spam circle and fast-fire. Runs from CControls::SnapInput, after
// the laser unfreeze.
class CAimbot : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;

	// Aim assist + hook-spam circle. Call only when the laser unfreeze did
	// NOT claim the input (same gate as before).
	void OnPlayerInput(CNetObj_PlayerInput *pOut, CNetObj_PlayerInput *pPersistent);
	// Fast-fire: must ALSO run when the laser unfreeze claimed the input,
	// so it is a separate call, exactly like before.
	void DoFastFire(CNetObj_PlayerInput *pOut);
	// true while the physical hook key is held (ignores our own spam OFF-tick).
	// Used by auto-hammer/auto-aled so they never fight the hook hand.
	bool IsHookKeyPhysicallyHeld();
	// true when the assist overwrote the aim this tick (press/block/fire snap).
	// FakeAim yields those ticks so both can run together.
	bool m_AimSnapped = false;

private:
	void DoAimAssist(CNetObj_PlayerInput *pOut, CNetObj_PlayerInput *pPersistent);
	void DoHookSpam(CNetObj_PlayerInput *pOut);

	// last aimbot target (stickiness bonus while it stays in the FOV cone)
	int m_AaTargetId = -1;
	vec2 m_AaTargetPos = vec2(0.0f, 0.0f);

	// hook-spam circle (hold hook → snap, release 1 tick on touch, re-hook)
	bool m_AaHookAuto = false;
	int m_AaHookAutoTick = 0;
	// true while the aimbot sees a live tee (not a block) with hook held
	bool m_AaHookPlayerTarget = false;
	// true on the fake release tick of the circle (0 sent, key still held)
	bool m_AaHookDropped = false;
	// standalone spam fallback state
	bool m_HookSpamActive = false;
	int m_HookSpamTick = 0;
};

#endif
