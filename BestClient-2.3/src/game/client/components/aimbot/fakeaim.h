#ifndef GAME_CLIENT_COMPONENTS_AIMBOT_FAKEAIM_H
#define GAME_CLIENT_COMPONENTS_AIMBOT_FAKEAIM_H

#include <base/vmath.h>

#include <generated/protocol.h>

#include <game/client/component.h>

// Prikoli fake aim (kinetix-style): while enabled, everyone else sees a fake
// aim direction (Random / Robot / Spin / Lag), while your cursor, crosshair
// and prediction stay on the real aim. Releases to the real aim on the exact
// tick a shot fires or hook is pressed, so hits still land.
// Runs from CControls::SnapInput, after the other assists.
class CFakeAim : public CComponent
{
public:
	enum
	{
		MODE_RANDOM = 0,
		MODE_ROBOT = 1,
		MODE_SPIN = 2,
		MODE_LAG = 3,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;

	void OnSnapInput(CNetObj_PlayerInput *pOut, CNetObj_PlayerInput *pLast);
	// Server-only fake (show-for-me OFF): patch the already-copied send
	// buffer, prediction keeps the real aim. Call right after mem_copy.
	void PatchSendData(CNetObj_PlayerInput *pSend) const;
	// Rendered aim override for players.cpp (show-for-me ON only).
	bool RenderOffset(vec2 *pOut) const;

private:
	bool m_RenderActive = false;
	vec2 m_RenderOffset = vec2(0.0f, 0.0f);

	vec2 m_RobotAim = vec2(0.0f, 0.0f);
	bool m_RobotInit = false;
	float m_RandAngle = 0.0f;
	int m_TickCounter = 0;
	float m_SpinAngle = 0.0f;
	int m_LagCounter = 0;
	int m_LastHook = 0;

	vec2 m_LastFake = vec2(0.0f, 0.0f);
	bool m_HaveLastFake = false;

	bool m_SendFake = false;
	vec2 m_SendOffset = vec2(0.0f, 0.0f);
};

#endif
