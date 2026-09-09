#include "tas.h"

#include <base/system.h>
#include <engine/client.h>
#include <engine/console.h>
#include <engine/map.h>
#include <engine/storage.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <cstdio>
#include <cstring>

static const char TAS_MAGIC[8] = {'B', 'T', 'A', 'S', '1', '\0', '\0', '\0'};
static const unsigned TAS_VERSION = 1;

void CTas::GetTasFolder(char *pBuffer, int BufferSize)
{
	Storage()->CreateFolder("ddnet", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("ddnet/tas", IStorage::TYPE_SAVE);
	char aMapFolder[IO_MAX_PATH_LENGTH];
	str_format(aMapFolder, sizeof(aMapFolder), "ddnet/tas/%s", GameClient()->Map()->BaseName());
	Storage()->CreateFolder(aMapFolder, IStorage::TYPE_SAVE);
	Storage()->GetCompletePath(IStorage::TYPE_SAVE, aMapFolder, pBuffer, BufferSize);
}

void CTas::GetTasFilePath(const char *pName, char *pBuffer, int BufferSize)
{
	char aFolder[IO_MAX_PATH_LENGTH];
	GetTasFolder(aFolder, sizeof(aFolder));
	str_format(pBuffer, BufferSize, "%s/%s.tas", aFolder, pName);
}

bool CTas::SaveToFile(const char *pName)
{
	char aPath[IO_MAX_PATH_LENGTH];
	GetTasFilePath(pName, aPath, sizeof(aPath));

	IOHANDLE hFile = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!hFile)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: failed to open '%s' for writing", aPath);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "tas", aBuf);
		return false;
	}

	io_write(hFile, TAS_MAGIC, sizeof(TAS_MAGIC));

	unsigned Version = TAS_VERSION;
	io_write(hFile, &Version, sizeof(Version));

	unsigned FrameCount = (unsigned)m_vFrames.size();
	io_write(hFile, &FrameCount, sizeof(FrameCount));

	for(const STasFrame &f : m_vFrames)
	{
		io_write(hFile, &f.m_Input.m_Direction, sizeof(int));
		io_write(hFile, &f.m_Input.m_TargetX, sizeof(int));
		io_write(hFile, &f.m_Input.m_TargetY, sizeof(int));
		io_write(hFile, &f.m_Input.m_Jump, sizeof(int));
		io_write(hFile, &f.m_Input.m_Fire, sizeof(int));
		io_write(hFile, &f.m_Input.m_Hook, sizeof(int));
		io_write(hFile, &f.m_Input.m_WantedWeapon, sizeof(int));
		io_write(hFile, &f.m_Input.m_NextWeapon, sizeof(int));
		io_write(hFile, &f.m_Input.m_PrevWeapon, sizeof(int));
		io_write(hFile, &f.m_Direction, sizeof(int));
	}

	io_close(hFile);
	return true;
}

bool CTas::LoadFromFile(const char *pName)
{
	char aPath[IO_MAX_PATH_LENGTH];
	GetTasFilePath(pName, aPath, sizeof(aPath));

	IOHANDLE hFile = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!hFile)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: failed to open '%s' for reading", aPath);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "tas", aBuf);
		return false;
	}

	char Magic[8];
	if(io_read(hFile, Magic, sizeof(Magic)) != sizeof(Magic) || mem_comp(Magic, TAS_MAGIC, sizeof(TAS_MAGIC)) != 0)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "tas", "TAS: invalid file magic");
		io_close(hFile);
		return false;
	}

	unsigned Version = 0;
	if(io_read(hFile, &Version, sizeof(Version)) != sizeof(Version) || Version != TAS_VERSION)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "tas", "TAS: unsupported version");
		io_close(hFile);
		return false;
	}

	unsigned FrameCount = 0;
	if(io_read(hFile, &FrameCount, sizeof(FrameCount)) != sizeof(FrameCount))
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "tas", "TAS: failed to read frame count");
		io_close(hFile);
		return false;
	}

	m_vFrames.resize(FrameCount);
	for(unsigned i = 0; i < FrameCount; i++)
	{
		STasFrame &f = m_vFrames[i];
		mem_zero(&f, sizeof(f));

		if(io_read(hFile, &f.m_Input.m_Direction, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Input.m_TargetX, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Input.m_TargetY, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Input.m_Jump, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Input.m_Fire, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Input.m_Hook, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Input.m_WantedWeapon, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Input.m_NextWeapon, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Input.m_PrevWeapon, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
		if(io_read(hFile, &f.m_Direction, sizeof(int)) != sizeof(int)) { io_close(hFile); return false; }
	}

	io_close(hFile);
	return true;
}

// Console commands
void CTas::ConTasRecord(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);
	pSelf->m_State = EState::RECORDING;
	pSelf->m_vFrames.clear();
	pSelf->m_PlaybackTick = 0;
	pSelf->m_RewindTick = 0;
	pSelf->m_RecordStartTick = pSelf->Client()->GameTick(0);

	str_copy(pSelf->m_aCurrentName, pResult->GetString(0), sizeof(pSelf->m_aCurrentName));

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "TAS: recording '%s'", pSelf->m_aCurrentName);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
}

void CTas::ConTasStop(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_State == EState::IDLE)
		return;

	int Frames = (int)pSelf->m_vFrames.size();
	char aBuf[256];

	if(pSelf->m_State == EState::RECORDING)
	{
		str_format(aBuf, sizeof(aBuf), "TAS: stopped recording, %d frames", Frames);
	}
	else if(pSelf->m_State == EState::PLAYING)
	{
		str_format(aBuf, sizeof(aBuf), "TAS: stopped playback at frame %d/%d", pSelf->m_PlaybackTick, Frames);
	}
	else if(pSelf->m_State == EState::REWINDING)
	{
		str_format(aBuf, sizeof(aBuf), "TAS: stopped rewind at frame %d/%d", pSelf->m_RewindTick, Frames);
	}

	pSelf->m_State = EState::IDLE;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
}

void CTas::ConTasSave(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_vFrames.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: nothing to save, record first");
		return;
	}

	const char *pName = pResult->GetString(0);
	if(pSelf->SaveToFile(pName))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: saved '%s' (%d frames)", pName, (int)pSelf->m_vFrames.size());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: save failed");
	}
}

void CTas::ConTasLoad(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_State != EState::IDLE)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: stop current action first");
		return;
	}

	const char *pName = pResult->GetString(0);
	if(pSelf->LoadFromFile(pName))
	{
		str_copy(pSelf->m_aCurrentName, pName, sizeof(pSelf->m_aCurrentName));
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: loaded '%s' (%d frames)", pName, (int)pSelf->m_vFrames.size());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
	}
	else
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: failed to load '%s'", pName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
	}
}

void CTas::ConTasPlay(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_vFrames.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: no frames loaded");
		return;
	}

	if(pSelf->m_State == EState::PLAYING)
	{
		pSelf->m_State = EState::IDLE;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: playback stopped");
		return;
	}

	pSelf->m_State = EState::PLAYING;
	pSelf->m_PlaybackTick = 0;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "TAS: playing '%s' (%d frames)", pSelf->m_aCurrentName, (int)pSelf->m_vFrames.size());
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
}

void CTas::ConTasRewindKey(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);
	bool Pressed = pResult->GetInteger(0) != 0;
	pSelf->m_RewindHeld = Pressed;
}

void CTas::ConTasRewindHold(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_vFrames.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: no frames to rewind");
		return;
	}

	bool Pressed = pResult->GetInteger(0) != 0;
	pSelf->m_RewindHeld = Pressed;

	if(Pressed)
	{
		if(pSelf->m_State == EState::IDLE)
		{
			if(pSelf->m_PlaybackTick > 0)
				pSelf->m_RewindTick = pSelf->m_PlaybackTick;
			else
				pSelf->m_RewindTick = (int)pSelf->m_vFrames.size();
		}
		else if(pSelf->m_State == EState::PLAYING)
		{
			pSelf->m_RewindTick = pSelf->m_PlaybackTick;
		}
		else if(pSelf->m_State == EState::REWINDING)
		{
			return;
		}

		pSelf->m_State = EState::REWINDING;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: rewinding from frame %d", pSelf->m_RewindTick);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
	}
	else
	{
		if(pSelf->m_State == EState::REWINDING)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "TAS: rewind stopped at frame %d", pSelf->m_RewindTick);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
			pSelf->m_PlaybackTick = pSelf->m_RewindTick;
			pSelf->m_State = EState::IDLE;
		}
	}
}

void CTas::ConTasForward(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_vFrames.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: no frames to forward");
		return;
	}

	bool Pressed = pResult->GetInteger(0) != 0;
	pSelf->m_ForwardHeld = Pressed;

	if(Pressed)
	{
		if(pSelf->m_State == EState::IDLE)
		{
			pSelf->m_RewindTick = pSelf->m_PlaybackTick;
		}
		else if(pSelf->m_State == EState::PLAYING)
		{
			pSelf->m_RewindTick = pSelf->m_PlaybackTick;
		}
		else if(pSelf->m_State == EState::REWINDING && pSelf->m_IsForwarding)
		{
			return;
		}

		pSelf->m_IsForwarding = true;
		pSelf->m_State = EState::REWINDING;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: forward from frame %d", pSelf->m_RewindTick);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
	}
	else
	{
		if(pSelf->m_State == EState::REWINDING && pSelf->m_IsForwarding)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "TAS: forward stopped at frame %d", pSelf->m_RewindTick);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
			pSelf->m_PlaybackTick = pSelf->m_RewindTick;
			pSelf->m_State = EState::IDLE;
			pSelf->m_IsForwarding = false;
		}
	}
}

void CTas::ConTasToggleRecord(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_State == EState::RECORDING)
	{
		int Frames = (int)pSelf->m_vFrames.size();
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: stopped recording, %d frames", Frames);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
		pSelf->m_State = EState::IDLE;
	}
	else if(pSelf->m_State == EState::IDLE)
	{
		pSelf->m_State = EState::RECORDING;
		pSelf->m_vFrames.clear();
		pSelf->m_PlaybackTick = 0;
		pSelf->m_RewindTick = 0;
		pSelf->m_RecordStartTick = pSelf->Client()->GameTick(0);
		str_copy(pSelf->m_aCurrentName, "quick", sizeof(pSelf->m_aCurrentName));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: recording 'quick'");
	}
}

void CTas::ConTasClear(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);
	pSelf->m_State = EState::IDLE;
	pSelf->m_vFrames.clear();
	pSelf->m_PlaybackTick = 0;
	pSelf->m_RewindTick = 0;
	pSelf->m_RewindHeld = false;
	pSelf->m_ForwardHeld = false;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: cleared");
}

void CTas::ConTasLoadLast(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_State != EState::IDLE)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: stop current action first");
		return;
	}

	if(pSelf->m_aCurrentName[0] == '\0')
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: no file to load");
		return;
	}

	if(pSelf->LoadFromFile(pSelf->m_aCurrentName))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "TAS: loaded '%s' (%d frames)", pSelf->m_aCurrentName, (int)pSelf->m_vFrames.size());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);
	}
}

void CTas::ConTasStep(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	if(pSelf->m_vFrames.empty())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: no frames");
		return;
	}

	if(pSelf->m_PlaybackTick < (int)pSelf->m_vFrames.size())
	{
		pSelf->m_PlaybackTick += pSelf->m_StepTicks;
		if(pSelf->m_PlaybackTick > (int)pSelf->m_vFrames.size())
			pSelf->m_PlaybackTick = (int)pSelf->m_vFrames.size();
	}
}

void CTas::ConTasSetStepTicks(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);
	pSelf->m_StepTicks = pResult->GetInteger(0);
	if(pSelf->m_StepTicks < 1)
		pSelf->m_StepTicks = 1;
	if(pSelf->m_StepTicks > 1000)
		pSelf->m_StepTicks = 1000;
}

void CTas::ConTasList(IConsole::IResult *pResult, void *pUserData)
{
	CTas *pSelf = static_cast<CTas *>(pUserData);

	char aFolder[IO_MAX_PATH_LENGTH];
	pSelf->GetTasFolder(aFolder, sizeof(aFolder));

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "TAS: listing files in '%s'", aFolder);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aBuf);

	// List .tas files
	auto Callback = [](const char *pName, int IsDir, int DirType, void *pUser) -> int
	{
		CTas *pSelf2 = static_cast<CTas *>(pUser);
		if(!IsDir)
		{
			char aMsg[256];
			str_format(aMsg, sizeof(aMsg), "  %s", pName);
			pSelf2->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", aMsg);
		}
		return 0;
	};

	char aSearchPath[IO_MAX_PATH_LENGTH];
	char aMapFolder[IO_MAX_PATH_LENGTH];
	str_format(aMapFolder, sizeof(aMapFolder), "ddnet/tas/%s", pSelf->GameClient()->Map()->BaseName());
	pSelf->Storage()->ListDirectory(IStorage::TYPE_SAVE, aMapFolder, Callback, pSelf);
}

void CTas::OnConsoleInit()
{
	Console()->Register("bc_tas_record", "s[name]", CFGFLAG_CLIENT, ConTasRecord, this, "Record TAS inputs with given name");
	Console()->Register("bc_tas_stop", "", CFGFLAG_CLIENT, ConTasStop, this, "Stop TAS recording/playback/rewind");
	Console()->Register("bc_tas_save", "s[name]", CFGFLAG_CLIENT, ConTasSave, this, "Save current TAS recording to file");
	Console()->Register("bc_tas_load", "s[name]", CFGFLAG_CLIENT, ConTasLoad, this, "Load TAS recording from file");
	Console()->Register("bc_tas_play", "", CFGFLAG_CLIENT, ConTasPlay, this, "Play loaded TAS recording");
	Console()->Register("bc_tas_rewind_key", "i[pressed]", CFGFLAG_CLIENT, ConTasRewindKey, this, "TAS rewind key state (for binds)");
	Console()->Register("bc_tas_rewind_hold", "i[pressed]", CFGFLAG_CLIENT, ConTasRewindHold, this, "Hold to rewind TAS, release to stop");
	Console()->Register("bc_tas_forward", "i[pressed]", CFGFLAG_CLIENT, ConTasForward, this, "Hold to forward TAS, release to stop");
	Console()->Register("bc_tas_list", "", CFGFLAG_CLIENT, ConTasList, this, "List saved TAS files for current map");
	Console()->Register("bc_tas_toggle_record", "", CFGFLAG_CLIENT, ConTasToggleRecord, this, "Toggle TAS recording");
	Console()->Register("bc_tas_clear", "", CFGFLAG_CLIENT, ConTasClear, this, "Clear TAS recording");
	Console()->Register("bc_tas_load_last", "", CFGFLAG_CLIENT, ConTasLoadLast, this, "Reload last loaded TAS file");
	Console()->Register("bc_tas_step", "", CFGFLAG_CLIENT, ConTasStep, this, "Step forward by N ticks");
	Console()->Register("bc_tas_set_step_ticks", "i[ticks]", CFGFLAG_CLIENT, ConTasSetStepTicks, this, "Set step tick count");
}

void CTas::OnReset()
{
	if(m_State == EState::REWINDING || m_State == EState::PLAYING)
	{
		m_State = EState::IDLE;
	}
	m_RewindHeld = false;
	m_ForwardHeld = false;
	m_IsForwarding = false;
}

bool CTas::Tick(CNetObj_PlayerInput *pInput, int Dummy)
{
	if(Dummy != 0)
		return false;

	if(m_State == EState::IDLE)
		return false;

	if(m_State == EState::RECORDING)
	{
		static constexpr int MAX_TAS_FRAMES = 50000; // ~16 minutes at 50 tps
		if((int)m_vFrames.size() >= MAX_TAS_FRAMES)
		{
			m_State = EState::IDLE;
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: recording stopped — max frames reached");
			return false;
		}

		STasFrame Frame;
		mem_copy(&Frame.m_Input, pInput, sizeof(CNetObj_PlayerInput));
		Frame.m_Direction = pInput->m_Direction;
		m_vFrames.push_back(Frame);
		return false;
	}

	if(m_State == EState::PLAYING)
	{
		if(m_PlaybackTick >= (int)m_vFrames.size())
		{
			m_State = EState::IDLE;
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: playback finished");
			return false;
		}

		const STasFrame &Frame = m_vFrames[m_PlaybackTick];
		pInput->m_Direction = Frame.m_Input.m_Direction;
		pInput->m_TargetX = Frame.m_Input.m_TargetX;
		pInput->m_TargetY = Frame.m_Input.m_TargetY;
		pInput->m_Jump = Frame.m_Input.m_Jump;
		pInput->m_Fire = Frame.m_Input.m_Fire;
		pInput->m_Hook = Frame.m_Input.m_Hook;
		pInput->m_WantedWeapon = Frame.m_Input.m_WantedWeapon;
		pInput->m_NextWeapon = Frame.m_Input.m_NextWeapon;
		pInput->m_PrevWeapon = Frame.m_Input.m_PrevWeapon;

		m_PlaybackTick++;
		return true;
	}

	if(m_State == EState::REWINDING)
	{
		if(m_IsForwarding)
		{
			if(!m_ForwardHeld || m_RewindTick >= (int)m_vFrames.size())
			{
				m_PlaybackTick = m_RewindTick;
				m_State = EState::IDLE;
				m_IsForwarding = false;
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: forward stopped");
				return false;
			}

			m_RewindTick++;

			const STasFrame &Frame = m_vFrames[m_RewindTick];
			pInput->m_Direction = Frame.m_Input.m_Direction;
			pInput->m_TargetX = Frame.m_Input.m_TargetX;
			pInput->m_TargetY = Frame.m_Input.m_TargetY;
			pInput->m_Jump = Frame.m_Input.m_Jump;
			pInput->m_Fire = Frame.m_Input.m_Fire;
			pInput->m_Hook = Frame.m_Input.m_Hook;
			pInput->m_WantedWeapon = Frame.m_Input.m_WantedWeapon;
			pInput->m_NextWeapon = Frame.m_Input.m_NextWeapon;
			pInput->m_PrevWeapon = Frame.m_Input.m_PrevWeapon;

			return true;
		}
		else
		{
			if(!m_RewindHeld || m_RewindTick <= 0)
			{
				m_PlaybackTick = m_RewindTick;
				m_State = EState::IDLE;
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tas", "TAS: rewind stopped");
				return false;
			}

			m_RewindTick--;

			const STasFrame &Frame = m_vFrames[m_RewindTick];
			pInput->m_Direction = Frame.m_Input.m_Direction;
			pInput->m_TargetX = Frame.m_Input.m_TargetX;
			pInput->m_TargetY = Frame.m_Input.m_TargetY;
			pInput->m_Jump = Frame.m_Input.m_Jump;
			pInput->m_Fire = Frame.m_Input.m_Fire;
			pInput->m_Hook = Frame.m_Input.m_Hook;
			pInput->m_WantedWeapon = Frame.m_Input.m_WantedWeapon;
			pInput->m_NextWeapon = Frame.m_Input.m_NextWeapon;
			pInput->m_PrevWeapon = Frame.m_Input.m_PrevWeapon;

			return true;
		}
	}

	return false;
}
