#ifndef VELVET_MODULES_MISC_MOONWALK_H
#define VELVET_MODULES_MISC_MOONWALK_H

// Moonwalk — alternates m_Direction between +1 and -1 every tick when the
// player physically holds BOTH left and right at the same time. Standalone
// extract from CVelvetMisc for reference / reuse.

#include <engine/client/enums.h>
#include <generated/protocol.h>

namespace velvet {

class CMoonwalk
{
public:
	// Reset all cross-tick state (call on OnReset / OnStateChange(!ONLINE)).
	void Reset()
	{
		for(int i = 0; i < NUM_DUMMIES; ++i)
			m_aDirection[i] = 0;
	}

	// Called from CControls::SnapInput on the current tick's outgoing input.
	//   Enabled       — cl_velvet_misc_moonwalk (0/1)
	//   Dummy         — current dummy index (0..NUM_DUMMIES-1)
	//   LeftPressed   — physical +left held
	//   RightPressed  — physical +right held
	//   LastDirection — pInput->m_Direction from the previous outgoing tick
	//   pOutDirection — modified in place when both L+R are held
	void Apply(bool Enabled, int Dummy, bool LeftPressed, bool RightPressed, int LastDirection, int *pOutDirection)
	{
		if(pOutDirection == nullptr || Dummy < 0 || Dummy >= NUM_DUMMIES)
			return;

		if(Enabled && LeftPressed && RightPressed)
		{
			if(m_aDirection[Dummy] == 0)
				m_aDirection[Dummy] = LastDirection < 0 ? 1 : -1;
			else
				m_aDirection[Dummy] = -m_aDirection[Dummy];
			*pOutDirection = m_aDirection[Dummy];
		}
		else
		{
			m_aDirection[Dummy] = 0;
		}
	}

private:
	int m_aDirection[NUM_DUMMIES] = {}; // 0 = inactive, else -1 / +1 alternating
};

} // namespace velvet

#endif
