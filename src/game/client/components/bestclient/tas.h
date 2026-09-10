#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_TAS_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_TAS_H

#include <engine/console.h>
#include <game/client/component.h>

#include <generated/protocol.h>

#include <string>
#include <vector>

class CTas : public CComponent
{
public:
	enum class EState
	{
		IDLE,
		RECORDING,
		PLAYING,
		REWINDING,
	};

private:
	struct STasFrame
	{
		CNetObj_PlayerInput m_Input;
		int m_Direction;
	};

	EState m_State = EState::IDLE;
	std::vector<STasFrame> m_vFrames;
	int m_PlaybackTick = 0;
	int m_RewindTick = 0;
	int m_RecordStartTick = 0;
	char m_aCurrentName[256] = {};
	bool m_RewindHeld = false;
	bool m_ForwardHeld = false;
	bool m_IsForwarding = false;
	int m_StepTicks = 1;

	// Save/load helpers
	void GetTasFilePath(const char *pName, char *pBuffer, int BufferSize);
	bool SaveToFile(const char *pName);
	bool LoadFromFile(const char *pName);

	static void ConTasRecord(IConsole::IResult *pResult, void *pUserData);
	static void ConTasStop(IConsole::IResult *pResult, void *pUserData);
	static void ConTasSave(IConsole::IResult *pResult, void *pUserData);
	static void ConTasLoad(IConsole::IResult *pResult, void *pUserData);
	static void ConTasPlay(IConsole::IResult *pResult, void *pUserData);
	static void ConTasRewindKey(IConsole::IResult *pResult, void *pUserData);
	static void ConTasRewindHold(IConsole::IResult *pResult, void *pUserData);
	static void ConTasForward(IConsole::IResult *pResult, void *pUserData);
	static void ConTasList(IConsole::IResult *pResult, void *pUserData);
	static void ConTasToggleRecord(IConsole::IResult *pResult, void *pUserData);
	static void ConTasClear(IConsole::IResult *pResult, void *pUserData);
	static void ConTasLoadLast(IConsole::IResult *pResult, void *pUserData);
	static void ConTasStep(IConsole::IResult *pResult, void *pUserData);
	static void ConTasSetStepTicks(IConsole::IResult *pResult, void *pUserData);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnReset() override;

	void GetTasFolder(char *pBuffer, int BufferSize);

	// Called from CControls::SnapInput to override/record input
	// Returns true if input was overridden (playback/rewind active)
	bool Tick(CNetObj_PlayerInput *pInput, int Dummy);

	EState GetState() const { return m_State; }
	const char *GetCurrentName() const { return m_aCurrentName; }
	int GetFrameCount() const { return (int)m_vFrames.size(); }
	int GetPlaybackTick() const { return m_PlaybackTick; }
	int GetStepTicks() const { return m_StepTicks; }
};

#endif
