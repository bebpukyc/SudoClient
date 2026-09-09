#ifndef GAME_CLIENT_COMPONENTS_NWC_AVOID_AVOID_H
#define GAME_CLIENT_COMPONENTS_NWC_AVOID_AVOID_H

#include "avoid_types.h"

#include <game/client/component.h>

#include <engine/client/enums.h>

#include <array>
#include <vector>

class CCharacter;

class CNwcAvoid final : public CComponent
{
public:
	CNwcAvoid();

	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;

	void OnControlsSnapInput(CNetObj_PlayerInput &Input, bool &Send);
	void OnControlsSnapInputImpl(CNetObj_PlayerInput &Input, bool &Send);

private:
	void ResetRuntime(int DummyIndex);
	void ClearDebugState(SNwcAvoidRuntime &Runtime);
	void StoreChosenCandidate(const SNwcAvoidContext &Context, const SNwcAvoidCandidate &Candidate);
	bool TryReuseCommittedAction(const SNwcAvoidContext &Context, CNetObj_PlayerInput &Input, bool &Send);

	bool GatherContext(const CNetObj_PlayerInput &Input, SNwcAvoidContext &OutContext) const;

	ENwcAvoidType GetConfiguredType() const;
	int GetPredictTicks(ENwcAvoidType Type) const;
	int GetCommitTicks(ENwcAvoidType Type) const;
	float GetHookAssistRange(ENwcAvoidType Type) const;
	bool IsHookAssistEnabled(ENwcAvoidType Type) const;
	bool ShouldAllowJump(ENwcAvoidType Type) const;

	SNwcAvoidAction BuildBaseAction(const SNwcAvoidContext &Context) const;
	vec2 GetAimDir(const CNetObj_PlayerInput &Input) const;
	float GetAimDistance(const CNetObj_PlayerInput &Input) const;
	void ApplyAction(CNetObj_PlayerInput &Input, const SNwcAvoidContext &Context, const SNwcAvoidAction &Action) const;
	void PushUniqueAction(std::vector<SNwcAvoidAction> &vActions, const SNwcAvoidAction &Action) const;

	bool IsFreezeTileIndex(int Tile) const;
	bool IsUnfreezeTileIndex(int Tile) const;
	bool IsTeleHazardIndex(int MapIndex) const;
	bool DetectHazardAtMapIndex(int MapIndex, SNwcAvoidHazardState &OutHazard) const;
	bool DetectHazardAtPos(const vec2 &Pos, bool FrozenState, SNwcAvoidHazardState &OutHazard) const;
	bool DetectHazardOnPath(const vec2 &From, const vec2 &To, bool FrozenState, SNwcAvoidHazardState &OutHazard) const;
	float MeasureHazardClearance(const vec2 &Pos) const;
	int GetDangerBiasDirection(const SNwcAvoidContext &Context, float ProbeDistance) const;
	bool IsHorizontalThreat(const SNwcAvoidContext &Context) const;

	bool IsCharacterInHazardState(const CCharacter *pCharacter) const;
	SNwcAvoidCandidate EvaluateAction(const SNwcAvoidContext &Context, const SNwcAvoidAction &Action, int PredictTicks, bool CaptureTrace) const;
	SNwcAvoidCandidate EvaluateDelayedAction(const SNwcAvoidContext &Context, const SNwcAvoidAction &Action, int DelayTicks, int PredictTicks, bool CaptureTrace) const;

	bool IsHookAimViable(const SNwcAvoidContext &Context, const vec2 &AimDir, vec2 *pOutHookPoint = nullptr) const;
	bool FindHookAssistPoint(const SNwcAvoidContext &Context, const vec2 &AimDir, vec2 &OutHookPoint) const;
	void BuildHookAssistActions(const SNwcAvoidContext &Context, std::vector<SNwcAvoidAction> &vActions, int CommitTicks, int MaxActions) const;

	bool SelectBestCandidate(const SNwcAvoidContext &Context, SNwcAvoidCandidate &OutCandidate);
	bool SelectLegitCandidate(const SNwcAvoidContext &Context, const SNwcAvoidCandidate &BaseCandidate, SNwcAvoidCandidate &OutCandidate);
	bool SelectBlatantCandidate(const SNwcAvoidContext &Context, const SNwcAvoidCandidate &BaseCandidate, SNwcAvoidCandidate &OutCandidate);
	bool SelectHorizontalCandidate(const SNwcAvoidContext &Context, const SNwcAvoidCandidate &BaseCandidate, SNwcAvoidCandidate &OutCandidate);

	void RenderDebug() const;

	static vec2 NormalizeOr(const vec2 &Value, const vec2 &Fallback);

	std::array<SNwcAvoidRuntime, NUM_DUMMIES> m_aRuntime;
};

#endif