// Moonwalk — reference / reuse extract. The active Velvet implementation
// lives inside CVelvetMisc::OnPlayerInput (src/velvet/modules/misc/misc.cpp);
// this pair of files is the standalone version that only contains the
// Moonwalk state machine, so it can be dropped into another project.
//
// Usage sketch:
//
//   #include "moonwalk.h"
//
//   velvet::CMoonwalk g_Moonwalk;
//
//   // On reset / disconnect / state change:
//   g_Moonwalk.Reset();
//
//   // Inside CControls::SnapInput() after the physical direction has been
//   // resolved and BEFORE the outgoing input is sent:
//   const int Dummy = g_Config.m_ClDummy;
//   CControls &C  = GameClient()->m_Controls;
//   const bool L  = C.m_aInputDirectionLeft[Dummy]  != 0;
//   const bool R  = C.m_aInputDirectionRight[Dummy] != 0;
//   g_Moonwalk.Apply(
//       g_Config.m_ClVelvetMiscMoonwalk != 0, // or your own toggle
//       Dummy, L, R,
//       C.m_aLastData[Dummy].m_Direction,
//       &C.m_aInputData[Dummy].m_Direction);
//
// The class stores no dependency on Velvet's config layer — the toggle bool
// is passed in explicitly so this extract compiles against a clean DDNet
// tree without any Velvet symbols.

#include "moonwalk.h"

// Everything is defined inline in moonwalk.h to keep this a header-only
// module. The .cpp is only here so build systems that expect a matching
// translation unit for every .h have one to compile.
