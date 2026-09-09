#ifndef GAME_CLIENT_COMPONENTS_AIMBOT_AUTOALED_H
#define GAME_CLIENT_COMPONENTS_AIMBOT_AUTOALED_H

#include <generated/protocol.h>

#include <game/client/component.h>

// Prikoli: auto-aled - hammer a frozen teammate to push them out of freeze.
// Simulates the physics of a hammer hit and only fires if the teammate would
// end up on non-freeze ground after the knockback trajectory.
class CAutoAled : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }

	void OnPlayerInput(CNetObj_PlayerInput *pOut, CNetObj_PlayerInput *pPersistent);

private:
	int m_AledSwingToggle = 0;
	int m_AledLastTick = 0;
	int m_AledFireExtra = 0;
	// pred tick when the hammer was requested, fire waits until the switch
	// reaches the server (-1 = no pending switch)
	int m_AledSwitchTick = -1;
	// weapon we switched away from, given back once no frozen mate remains
	// (back-and-forth, -1 = none)
	int m_AledPrevWeapon = -1;
};

#endif
