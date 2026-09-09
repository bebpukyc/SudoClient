#ifndef GAME_CLIENT_COMPONENTS_AIMBOT_AUTOHAMMER_H
#define GAME_CLIENT_COMPONENTS_AIMBOT_AUTOHAMMER_H

#include <generated/protocol.h>

#include <game/client/component.h>

// Prikoli: auto-hammer - beat any tee in reach as fast as the server allows.
class CAutoHammer : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;

	void OnPlayerInput(CNetObj_PlayerInput *pOut);

private:
	// synthetic fire press that we must release ourselves, since no real
	// +fire/-fire event will come for it
	bool m_AutoHammerHolding = false;
	// weapon we switched away from while beating, given back once no
	// target remains (-1 = none)
	int m_AutoHammerPrevWeapon = -1;
};

#endif
