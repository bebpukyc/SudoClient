#include "menus.h"

#include <base/system.h>
#include <engine/console.h>
#include <engine/font_icons.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/client/components/bestclient/tas.h>
#include <game/client/gameclient.h>
#include <game/client/ui_scrollregion.h>
#include <game/client/lineinput.h>
#include <game/localization.h>

#include <algorithm>
#include <string>
#include <vector>

static constexpr float TAS_FONTSIZE = 14.0f;
static constexpr float TAS_LINESIZE = 20.0f;
static constexpr float TAS_BTNH = 22.0f;
static constexpr float TAS_MARGIN = 4.0f;
static constexpr float TAS_KEYREADER_SPACING = 2.5f;

void CMenus::RenderSettingsStealTas(CUIRect MainView)
{
	CTas &Tas = GameClient()->m_Tas;

	if(!GameClient()->Map())
	{
		Ui()->DoLabel(&MainView, "TAS доступен только на сервере", 14.0f, TEXTALIGN_MC);
		return;
	}

	CUIRect LeftCol, RightCol;
	MainView.VSplitMid(&LeftCol, &RightCol, TAS_MARGIN * 2);

	// ═══ LEFT COLUMN ══════════════════════════════════════════

	// ── Main ──────────────────────────────────────────────────
	{
		CUIRect Header;
		LeftCol.HSplitTop(TAS_LINESIZE, &Header, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		Ui()->DoLabel(&Header, "Основное", TAS_FONTSIZE, TEXTALIGN_ML);

		CUIRect TpsRow;
		LeftCol.HSplitTop(TAS_LINESIZE, &TpsRow, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		Ui()->DoLabel(&TpsRow, "TPS: 50", 11.0f, TEXTALIGN_ML);

		CUIRect BtnRow;
		LeftCol.HSplitTop(TAS_BTNH, &BtnRow, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);

		const float BtnW = (BtnRow.w - TAS_MARGIN * 2) / 3.0f;
		CUIRect Btn;
		static CButtonContainer s_PlayBtn, s_PauseBtn, s_ReplayBtn;

		BtnRow.VSplitLeft(BtnW, &Btn, &BtnRow);
		BtnRow.VSplitLeft(TAS_MARGIN, nullptr, &BtnRow);
		if(DoButton_Menu(&s_PlayBtn, ">", 0, &Btn))
		{
			if(Tas.GetState() == CTas::EState::PLAYING)
				Console()->ExecuteLine("bc_tas_stop", IConsole::CLIENT_ID_UNSPECIFIED);
			else
				Console()->ExecuteLine("bc_tas_play", IConsole::CLIENT_ID_UNSPECIFIED);
		}

		BtnRow.VSplitLeft(BtnW, &Btn, &BtnRow);
		BtnRow.VSplitLeft(TAS_MARGIN, nullptr, &BtnRow);
		if(DoButton_Menu(&s_PauseBtn, "Пауза", 0, &Btn))
		{
			Console()->ExecuteLine("bc_tas_play", IConsole::CLIENT_ID_UNSPECIFIED);
		}

		BtnRow.VSplitLeft(BtnW, &Btn, &BtnRow);
		if(DoButton_Menu(&s_ReplayBtn, "Воспроизвести демо", 0, &Btn))
		{
			Console()->ExecuteLine("bc_tas_play", IConsole::CLIENT_ID_UNSPECIFIED);
		}
	}

	// ── Настройки ─────────────────────────────────────────────
	{
		CUIRect Header;
		LeftCol.HSplitTop(TAS_LINESIZE, &Header, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		Ui()->DoLabel(&Header, "Настройки", TAS_FONTSIZE, TEXTALIGN_ML);

		static int s_EnableSound = 0, s_AutoReplay = 0;
		static int s_EnableEffects = 0, s_AutoSaveReplay = 0;
		static int s_ShowRealAim = 0, s_PredictPlayers = 0;

		CUIRect Row1;
		LeftCol.HSplitTop(TAS_LINESIZE, &Row1, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		CUIRect C1, C2;
		Row1.VSplitMid(&C1, &C2, TAS_MARGIN);
		DoButton_CheckBoxAutoVMarginAndSet(&s_EnableSound, "Звук", &s_EnableSound, &C1, TAS_LINESIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&s_AutoReplay, "Авто-воспроизведение", &s_AutoReplay, &C2, TAS_LINESIZE);

		CUIRect Row2;
		LeftCol.HSplitTop(TAS_LINESIZE, &Row2, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		Row2.VSplitMid(&C1, &C2, TAS_MARGIN);
		DoButton_CheckBoxAutoVMarginAndSet(&s_EnableEffects, "Эффекты", &s_EnableEffects, &C1, TAS_LINESIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&s_AutoSaveReplay, "Авто-сохранение демо", &s_AutoSaveReplay, &C2, TAS_LINESIZE);

		CUIRect Row3;
		LeftCol.HSplitTop(TAS_LINESIZE, &Row3, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		Row3.VSplitMid(&C1, &C2, TAS_MARGIN);
		DoButton_CheckBoxAutoVMarginAndSet(&s_ShowRealAim, "Показывать настоящий аим", &s_ShowRealAim, &C1, TAS_LINESIZE);
		DoButton_CheckBoxAutoVMarginAndSet(&s_PredictPlayers, "Предсказание игроков", &s_PredictPlayers, &C2, TAS_LINESIZE);
	}

	// ── Hotkeys (DoLine_KeyReader) ───────────────────────────
	{
		CUIRect Header;
		LeftCol.HSplitTop(TAS_LINESIZE, &Header, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		Ui()->DoLabel(&Header, "Горячие клавиши", TAS_FONTSIZE, TEXTALIGN_ML);

		LeftCol.HSplitTop(TAS_KEYREADER_SPACING, nullptr, &LeftCol);

		static CButtonContainer s_RecReader, s_RecClear;
		static CButtonContainer s_LoadReader, s_LoadClear;
		static CButtonContainer s_ClearReader, s_ClearClear;
		static CButtonContainer s_PauseReader, s_PauseClear;
		static CButtonContainer s_RewindReader, s_RewindClear;
		static CButtonContainer s_ForwardReader, s_ForwardClear;

		DoLine_KeyReader(LeftCol, s_RecReader, s_RecClear, "Запись", "bc_tas_toggle_record");
		DoLine_KeyReader(LeftCol, s_LoadReader, s_LoadClear, "Загрузить", "bc_tas_load_last");
		DoLine_KeyReader(LeftCol, s_ClearReader, s_ClearClear, "Очистить", "bc_tas_clear");
		DoLine_KeyReader(LeftCol, s_PauseReader, s_PauseClear, "Пауза", "bc_tas_play");
		DoLine_KeyReader(LeftCol, s_RewindReader, s_RewindClear, "Назад", "+bc_tas_rewind_hold");
		DoLine_KeyReader(LeftCol, s_ForwardReader, s_ForwardClear, "Вперёд", "+bc_tas_forward");
	}

	// ── Saved TAS + file list ─────────────────────────────────
	{
		CUIRect Header;
		LeftCol.HSplitTop(TAS_MARGIN * 2, nullptr, &LeftCol);
		LeftCol.HSplitTop(TAS_LINESIZE, &Header, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		Ui()->DoLabel(&Header, "Сохранённые TAS", TAS_FONTSIZE, TEXTALIGN_ML);

		static std::vector<std::string> s_vFiles;
		static bool s_FilesPopulated = false;
		static int s_SelectedIndex = -1;
		static CLineInputBuffered<128> s_NameInput;

		if(!s_FilesPopulated)
		{
			s_vFiles.clear();
			auto Callback = [](const char *pName, int IsDir, int DirType, void *pUser) -> int {
				if(!IsDir)
				{
					auto *pFiles = static_cast<std::vector<std::string> *>(pUser);
					std::string Name(pName);
					if(Name.size() > 4 && Name.substr(Name.size() - 4) == ".tas")
						Name = Name.substr(0, Name.size() - 4);
					pFiles->push_back(Name);
				}
				return 0;
			};
			char aMapFolder[IO_MAX_PATH_LENGTH];
			str_format(aMapFolder, sizeof(aMapFolder), "ddnet/tas/%s", GameClient()->Map()->BaseName());
			Storage()->ListDirectory(IStorage::TYPE_SAVE, aMapFolder, Callback, &s_vFiles);
			std::sort(s_vFiles.begin(), s_vFiles.end());
			s_FilesPopulated = true;
		}

		// File list
		if(s_vFiles.empty())
		{
			CUIRect EmptyLabel;
			LeftCol.HSplitTop(TAS_LINESIZE, &EmptyLabel, &LeftCol);
			Ui()->DoLabel(&EmptyLabel, "No TAS files", 11.0f, TEXTALIGN_ML);
		}
		else
		{
			const float RowH = 18.0f;
			CScrollRegionParams FileScrollParams;
			FileScrollParams.m_ScrollUnit = RowH * 2.0f;
			FileScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
			static CScrollRegion s_FileScroll;
			vec2 FileScrollOffset(0.0f, 0.0f);
			s_FileScroll.Begin(&LeftCol, &FileScrollOffset, &FileScrollParams);
			LeftCol.y += FileScrollOffset.y;

			for(int i = 0; i < (int)s_vFiles.size(); i++)
			{
				CUIRect ItemRect;
				LeftCol.HSplitTop(RowH, &ItemRect, &LeftCol);
				if(!s_FileScroll.AddRect(ItemRect))
					continue;

				if(i == s_SelectedIndex)
					ItemRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.1f), IGraphics::CORNER_ALL, 3.0f);

				CUIRect NameRect;
				ItemRect.VSplitLeft(TAS_MARGIN * 2, nullptr, &NameRect);
				Ui()->DoLabel(&NameRect, s_vFiles[i].c_str(), 11.0f, TEXTALIGN_ML);

				if(Ui()->DoButtonLogic(&s_vFiles[i], i == s_SelectedIndex, &ItemRect, 0))
				{
					s_SelectedIndex = i;
					s_NameInput.Set(s_vFiles[i].c_str());
				}
			}
			s_FileScroll.End();
		}

		// Load / Delete buttons
		CUIRect BtnRow;
		LeftCol.HSplitTop(TAS_BTNH, &BtnRow, &LeftCol);
		LeftCol.HSplitTop(TAS_MARGIN, nullptr, &LeftCol);
		{
			bool HasSel = s_SelectedIndex >= 0 && s_SelectedIndex < (int)s_vFiles.size();
			const float LoadW = (BtnRow.w - TAS_MARGIN) / 2.0f;
			CUIRect LoadBtn, DelBtn;
			BtnRow.VSplitLeft(LoadW, &LoadBtn, &BtnRow);
			BtnRow.VSplitLeft(TAS_MARGIN, nullptr, &BtnRow);
			DelBtn = BtnRow;

			static CButtonContainer s_LoadBtn, s_DelBtn;
			if(DoButton_Menu(&s_LoadBtn, "Load", HasSel ? 0 : -1, &LoadBtn) && HasSel)
			{
				char aCmd[256];
				str_format(aCmd, sizeof(aCmd), "bc_tas_load \"%s\"", s_vFiles[s_SelectedIndex].c_str());
				Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
			}
			if(DoButton_Menu(&s_DelBtn, "Delete", HasSel ? 0 : -1, &DelBtn) && HasSel)
			{
				char aMapFolder[IO_MAX_PATH_LENGTH];
				str_format(aMapFolder, sizeof(aMapFolder), "ddnet/tas/%s/%s.tas", GameClient()->Map()->BaseName(), s_vFiles[s_SelectedIndex].c_str());
				Storage()->RemoveFile(aMapFolder, IStorage::TYPE_SAVE);
				s_vFiles.erase(s_vFiles.begin() + s_SelectedIndex);
				s_SelectedIndex = -1;
			}
		}
	}

	// ═══ RIGHT COLUMN ═════════════════════════════════════════

	// ── Запись ────────────────────────────────────────────────
	{
		CUIRect Header;
		RightCol.HSplitTop(TAS_LINESIZE, &Header, &RightCol);
		RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
		Ui()->DoLabel(&Header, "Запись", TAS_FONTSIZE, TEXTALIGN_ML);

		// File name row
		CUIRect NameRow;
		RightCol.HSplitTop(TAS_BTNH, &NameRow, &RightCol);
		RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);

		static CLineInputBuffered<128> s_SaveNameInput;
		s_SaveNameInput.SetEmptyText("my_tas");

		CUIRect FolderBtn, NameBox, Btn1, Btn2;
		NameRow.VSplitLeft(24.0f, &FolderBtn, &NameRow);
		NameRow.VSplitRight(24.0f * 2 + TAS_MARGIN * 2, &NameRow, &Btn1);
		Btn1.VSplitLeft(TAS_MARGIN, nullptr, &Btn1);
		NameRow.VSplitRight(24.0f, &NameRow, &Btn2);
		Btn2.VSplitLeft(TAS_MARGIN, nullptr, &Btn2);

		static CButtonContainer s_FolderBtn, s_RefreshBtn, s_ExportBtn;
		DoButton_Menu(&s_FolderBtn, FontIcon::FOLDER, 0, &FolderBtn);
		Ui()->DoEditBox(&s_SaveNameInput, &NameBox, 11.0f);
		DoButton_Menu(&s_RefreshBtn, FontIcon::ARROWS_ROTATE, 0, &Btn1);
		DoButton_Menu(&s_ExportBtn, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Btn2);

		// Buttons
		{
			CUIRect Btn;
			RightCol.HSplitTop(TAS_BTNH, &Btn, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			static CButtonContainer s_ValidateBtn;
			DoButton_Menu(&s_ValidateBtn, "Validate replay", 0, &Btn);
		}
		{
			CUIRect Btn;
			RightCol.HSplitTop(TAS_BTNH, &Btn, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			static CButtonContainer s_GetTimeBtn;
			DoButton_Menu(&s_GetTimeBtn, "Get replay time", 0, &Btn);
		}
		{
			CUIRect Btn;
			RightCol.HSplitTop(TAS_BTNH, &Btn, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			static CButtonContainer s_SaveReplayBtn;
			if(DoButton_Menu(&s_SaveReplayBtn, "Save replay", 0, &Btn))
			{
				if(!s_SaveNameInput.IsEmpty())
				{
					char aCmd[256];
					str_format(aCmd, sizeof(aCmd), "bc_tas_save \"%s\"", s_SaveNameInput.GetString());
					Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
				}
			}
		}
	}

	// ── Replay Vault ──────────────────────────────────────────
	{
		CUIRect Header;
		RightCol.HSplitTop(TAS_MARGIN * 2, nullptr, &RightCol);
		RightCol.HSplitTop(TAS_LINESIZE, &Header, &RightCol);
		RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
		Ui()->DoLabel(&Header, "Replay Vault", TAS_FONTSIZE, TEXTALIGN_ML);

		CUIRect BtnRow;
		RightCol.HSplitTop(TAS_BTNH, &BtnRow, &RightCol);
		RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);

		const float BtnW = (BtnRow.w - TAS_MARGIN) / 2.0f;
		CUIRect AutoSyncBtn, SyncNowBtn;
		BtnRow.VSplitLeft(BtnW, &AutoSyncBtn, &BtnRow);
		BtnRow.VSplitLeft(TAS_MARGIN, nullptr, &BtnRow);
		SyncNowBtn = BtnRow;

		static CButtonContainer s_AutoSyncBtn, s_SyncNowBtn;
		DoButton_Menu(&s_AutoSyncBtn, "Auto Sync", 0, &AutoSyncBtn);
		DoButton_Menu(&s_SyncNowBtn, "Sync now", 0, &SyncNowBtn);

		CUIRect InfoRect;
		RightCol.HSplitTop(TAS_LINESIZE, &InfoRect, &RightCol);
		Ui()->DoLabel(&InfoRect, "Automatically sync with the replay vault from github.com/krxclient/krx-replays", 8.0f, TEXTALIGN_ML);
	}

	// ── Tools ─────────────────────────────────────────────────
	{
		CUIRect Header;
		RightCol.HSplitTop(TAS_MARGIN * 2, nullptr, &RightCol);
		RightCol.HSplitTop(TAS_LINESIZE, &Header, &RightCol);
		RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
		Ui()->DoLabel(&Header, "Tools", TAS_FONTSIZE, TEXTALIGN_ML);

		// Ticks slider
		{
			CUIRect TicksRow;
			RightCol.HSplitTop(TAS_BTNH, &TicksRow, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			static int s_TicksValue = 1;
			Ui()->DoScrollbarOption(&s_TicksValue, &s_TicksValue, &TicksRow, "Ticks", 1, 100);
			char aCmd[64];
			str_format(aCmd, sizeof(aCmd), "bc_tas_set_step_ticks %d", s_TicksValue);
			Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
		}

		// Auto checkboxes
		{
			static int s_AutoRewind = 0, s_AutoForward = 0, s_AutoPause = 0;

			CUIRect Row;
			RightCol.HSplitTop(TAS_LINESIZE, &Row, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			DoButton_CheckBoxAutoVMarginAndSet(&s_AutoRewind, "Auto rewind", &s_AutoRewind, &Row, TAS_LINESIZE);

			CUIRect Row2;
			RightCol.HSplitTop(TAS_LINESIZE, &Row2, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			DoButton_CheckBoxAutoVMarginAndSet(&s_AutoForward, "Auto forward", &s_AutoForward, &Row2, TAS_LINESIZE);

			CUIRect Row3;
			RightCol.HSplitTop(TAS_LINESIZE, &Row3, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			DoButton_CheckBoxAutoVMarginAndSet(&s_AutoPause, "Auto pause", &s_AutoPause, &Row3, TAS_LINESIZE);
		}

		// Step button
		{
			CUIRect Btn;
			RightCol.HSplitTop(TAS_BTNH, &Btn, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			static CButtonContainer s_StepBtn;
			if(DoButton_Menu(&s_StepBtn, "Step", 0, &Btn))
			{
				Console()->ExecuteLine("bc_tas_step", IConsole::CLIENT_ID_UNSPECIFIED);
			}
		}
	}

	// ── Fake Aim ──────────────────────────────────────────────
	{
		CUIRect Header;
		RightCol.HSplitTop(TAS_MARGIN * 2, nullptr, &RightCol);
		RightCol.HSplitTop(TAS_LINESIZE, &Header, &RightCol);
		RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
		Ui()->DoLabel(&Header, "Fake Aim", TAS_FONTSIZE, TEXTALIGN_ML);

		// Robot aim dropdown
		{
			CUIRect Dropdown;
			RightCol.HSplitTop(TAS_BTNH, &Dropdown, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			static CButtonContainer s_Dropdown;
			DoButton_Menu(&s_Dropdown, "Robot aim", 0, &Dropdown);
		}

		// Add fake aim
		{
			CUIRect Btn;
			RightCol.HSplitTop(TAS_BTNH, &Btn, &RightCol);
			RightCol.HSplitTop(TAS_MARGIN, nullptr, &RightCol);
			static CButtonContainer s_AddFakeAim;
			DoButton_Menu(&s_AddFakeAim, "Add fake aim", 0, &Btn);
		}
	}
}
