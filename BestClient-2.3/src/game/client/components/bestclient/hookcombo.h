/* Copyright © 2026 BestProject Team */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_HOOKCOMBO_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_HOOKCOMBO_H

#include <game/client/component.h>

#include <array>
#include <array>
#include <string>
#include <vector>

#include <base/color.h>
#include <game/client/ui.h>

class CHookCombo : public CComponent
{
	class SPopup
	{
	public:
		int m_Sequence = 1;
		float m_Age = 0.0f;
	};

	class STogglePopup
	{
	public:
		float m_Age = 0.0f;
		char m_aText[64];
	};

	std::array<int, 7> m_aSoundIds{};
	std::vector<SPopup> m_vPopups;
	std::vector<STogglePopup> m_vTogglePopups;
	int m_Counter = 0;
	float m_LastHookTime = -1.0f;
	int m_TrackedClientId = -1;
	int m_LastHookedPlayer = -1;
	int m_LastProcessedGameTick = -1;
	bool m_SoundErrorShown = false;

	int m_LastAaEnabled = -1;
	int m_LastSymLaserEnable = -1;
	int m_LastAaAutoAled = -1;
	int m_LastNwAvoidEnabled = -1;
	int m_LastAaAutoHammer = -1;
	int m_LastAaFastFire = -1;
	int m_LastAaHookSpam = -1;
	int m_LastAaSpamEmote = -1;
	int m_LastAaMoonwalk = -1;
	bool m_ToggleCacheInit = false;
	int m_ToggleSuppressFrames = 0;

	struct SFunctionEntry
	{
		const char *m_pName;
		bool m_Enabled;
		ColorRGBA m_Color;
		float m_Age = 0.0f;
	};

	std::vector<SFunctionEntry> m_vActiveFunctions;
	std::array<float, 9> m_aFunctionAges {};
	float m_FunctionsHeightAnim = 0.0f;
	float m_FunctionsWidthAnim = 0.0f;

	int GetActiveFunctionCount() const;
	void CollectActiveFunctions();
	float GetFunctionsPlayerTargetWidth() const;
	float GetFunctionsPlayerTargetHeight() const;
	void RenderFunctionsPlayer(bool ForcePreview);

	void LoadSounds(bool LogErrors = true);
	void UnloadSounds();
	void ResetState();
	void TriggerStep();
	bool HasWork() const;
	void DetectConfigToggles();

	int m_SavedAaEnabled = -1;
	int m_SavedSymLaserEnable = -1;
	int m_SavedAaAutoAled = -1;
	int m_SavedAaAutoHammer = -1;
	int m_SavedAaFastFire = -1;
	int m_SavedAaHookSpam = -1;
	int m_SavedAaSpamEmote = -1;
	int m_SavedAaMoonwalk = -1;

public:
	CUIRect GetFunctionsPlayerRect(bool ForcePreview) const;
	void RenderFunctionsPreview();
	void ToggleMenuHider();

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnReset() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;

	void Render(bool ForcePreview = false);
};

#endif
