#ifndef GAME_CLIENT_COMPONENTS_NWC_MULTIGAME_AVOID_AVOID_TYPES_H
#define GAME_CLIENT_COMPONENTS_NWC_MULTIGAME_AVOID_AVOID_TYPES_H

#include <generated/protocol.h>

#include <base/vmath.h>

#include <vector>

enum class ENwcAvoidType
{
	Legit = 0,
	Blatant,
	Horizontal,
	Count,
};

enum class ENwcAvoidHazard
{
	None = 0,
	Freeze,
	Kill,
	Tele,
	Unfreeze,
	Frozen,
	OutOfBounds,
};

struct SNwcAvoidHazardState
{
	bool m_Triggered = false;
	ENwcAvoidHazard m_Type = ENwcAvoidHazard::None;
	int m_Tick = -1;
	float m_Distance = 9999.0f;
	vec2 m_Pos = vec2(0.0f, 0.0f);
};

struct SNwcAvoidTracePoint
{
	vec2 m_Pos = vec2(0.0f, 0.0f);
	bool m_Danger = false;
};

struct SNwcAvoidAction
{
	CNetObj_PlayerInput m_Input{};
	vec2 m_AimDir = vec2(1.0f, 0.0f);
	int m_Id = 0;
	int m_CommitTicks = 0;
	bool m_ForceAim = false;
	bool m_UsesHookAssist = false;
};

struct SNwcAvoidCandidate
{
	SNwcAvoidAction m_Action{};
	bool m_Valid = false;
	bool m_HasHazard = false;
	int m_HazardTick = 0;
	int m_EvalTicks = 0;
	float m_Score = -1.0e9f;
	float m_MinHazardDistance = 9999.0f;
	vec2 m_EndPos = vec2(0.0f, 0.0f);
	vec2 m_EndVel = vec2(0.0f, 0.0f);
	ENwcAvoidHazard m_HazardType = ENwcAvoidHazard::None;
	vec2 m_HazardPos = vec2(0.0f, 0.0f);
	bool m_HasHookPoint = false;
	vec2 m_HookPoint = vec2(0.0f, 0.0f);
	bool m_TraceHasFrozen = false;
	bool m_TraceHasForbiddenFreeze = false;
	int m_TraceFirstFrozenTick = -1;
	int m_TraceFrozenTickCount = 0;
	std::vector<SNwcAvoidTracePoint> m_vTrace;
};

struct SNwcAvoidContext
{
	int m_DummyIndex = -1;
	int m_LocalId = -1;
	int m_GameTick = 0;
	CNetObj_PlayerInput m_BaseInput{};
	vec2 m_LocalPos = vec2(0.0f, 0.0f);
	vec2 m_LocalVel = vec2(0.0f, 0.0f);
	vec2 m_AimDir = vec2(1.0f, 0.0f);
	float m_AimDistance = 0.0f;
	float m_HookLength = 0.0f;
	bool m_Grounded = false;
	bool m_HasAirJump = false;
	bool m_Hooking = false;
	int m_HookState = 0;
	vec2 m_HookPos = vec2(0.0f, 0.0f);
	vec2 m_HookDir = vec2(0.0f, 0.0f);
	bool m_Super = false;
	bool m_Invincible = false;
	SNwcAvoidHazardState m_BaseHazard{};
};

struct SNwcAvoidRuntime
{
	bool m_HasCommittedAction = false;
	SNwcAvoidAction m_CommittedAction{};
	int m_CommitUntilTick = -1;
	int m_LastChosenId = -1;
	int m_LastChosenTick = -1;
	ENwcAvoidHazard m_LastHazardType = ENwcAvoidHazard::None;
	std::vector<SNwcAvoidTracePoint> m_vDebugTrace;
	bool m_HasDebugHookPoint = false;
	vec2 m_DebugHookPoint = vec2(0.0f, 0.0f);
};

#endif
