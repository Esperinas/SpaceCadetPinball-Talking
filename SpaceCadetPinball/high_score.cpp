#include "pch.h"
#include "high_score.h"

#include "options.h"
#include "pb.h"
#include "fullscrn.h"
#include "render.h"
#include "score.h"
#include "Sound.h"
#include "TPinballTable.h"
#include "translations.h"

bool high_score::dlg_enter_name;
bool high_score::ShowDialog = false, high_score::CloseDialog = false;
high_score_entry high_score::DlgData;
std::vector<high_score_entry> high_score::ScoreQueue;
high_score_struct high_score::highscore_table[high_score::HighScoreCount];
Mix_Chunk* high_score::ApplauseSound = nullptr;
Mix_Chunk* high_score::DrumSound = nullptr;
Uint32 high_score::ApplauseStartTick = 0;

namespace
{
	Uint32 ChunkDurationMs(const Mix_Chunk* chunk)
	{
		int frequency = 0, channels = 0;
		Uint16 format = 0;
		if (!chunk || !Mix_QuerySpec(&frequency, &format, &channels) || frequency <= 0 || channels <= 0)
			return 1900;
		const auto bytesPerSample = SDL_AUDIO_BITSIZE(format) / 8;
		const auto bytesPerSecond = frequency * channels * bytesPerSample;
		return bytesPerSecond > 0 ? static_cast<Uint32>(chunk->alen * 1000ULL / bytesPerSecond) : 1900;
	}
}

int high_score::read()
{
	char Buffer[20];

	int checkSum = 0, legacyCheckSum = 0;
	clear_table();
	for (auto position = 0; position < HighScoreCount; ++position)
	{
		auto& tablePtr = highscore_table[position];

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Name");
		auto name = options::GetSetting(Buffer, "");
		strncpy(tablePtr.Name, name.c_str(), sizeof tablePtr.Name);
		tablePtr.Name[sizeof tablePtr.Name - 1] = 0;

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Score");
		tablePtr.Score = options::get_int(Buffer, tablePtr.Score);

		int entryCheckSum = 0;
		for (int i = static_cast<int>(strlen(tablePtr.Name)); --i >= 0; entryCheckSum += tablePtr.Name[i])
		{
		}
		entryCheckSum += tablePtr.Score;
		checkSum += entryCheckSum;
		if (position < LegacyHighScoreCount)
			legacyCheckSum += entryCheckSum;
	}

	auto verification = options::get_int("Verification", 7);
	if (checkSum == verification)
		return 0;

	if (legacyCheckSum == verification)
	{
		// Preserve the five scores from previous versions and persist the new eight-entry checksum.
		write();
	}
	else
		clear_table();
	return 0;
}

int high_score::write()
{
	char Buffer[20];

	int checkSum = 0;
	for (auto position = 0; position < HighScoreCount; ++position)
	{
		auto& tablePtr = highscore_table[position];

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Name");
		options::SetSetting(Buffer, tablePtr.Name);

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Score");
		options::set_int(Buffer, tablePtr.Score);

		for (int i = static_cast<int>(strlen(tablePtr.Name)); --i >= 0; checkSum += tablePtr.Name[i])
		{
		}
		checkSum += tablePtr.Score;
	}

	options::set_int("Verification", checkSum);
	return 0;
}

void high_score::clear_table()
{
	for (auto& table : highscore_table)
	{
		table.Score = -999;
		table.Name[0] = 0;
	}
}

int high_score::get_score_position(int score)
{
	if (score <= 0)
		return -1;

	for (int position = 0; position < HighScoreCount; position++)
	{
		if (highscore_table[position].Score < score)
			return position;
	}
	return -1;
}

void high_score::place_new_score_into(high_score_entry data)
{
	if (data.Position >= 0 && data.Position < HighScoreCount)
	{
		for (int i = HighScoreCount - 1; i > data.Position; i--)
		{
			highscore_table[i] = highscore_table[i - 1];
		}

		data.Entry.Name[31] = 0;
		highscore_table[data.Position] = data.Entry;
	}
}

void high_score::show_high_score_dialog()
{
	CloseDialog = false;
	ShowDialog = true;
}

void high_score::close_high_score_dialog()
{
	CloseDialog = true;
}

void high_score::show_and_set_high_score_dialog(high_score_entry score)
{
	ScoreQueue.insert(ScoreQueue.begin(), score);
	CloseDialog = false;
	ShowDialog = true;
}

void high_score::PlayNewScoreSound(int position)
{
	if (!ApplauseSound)
		ApplauseSound = Sound::LoadWaveFile(pb::make_path_name("APPLAU.WAV"));

	if (position == 0)
	{
		if (!DrumSound)
			DrumSound = Sound::LoadWaveFile(pb::make_path_name("DRUM.WAV"));
		if (DrumSound)
		{
			Sound::PlaySound(DrumSound, pb::time_ticks, nullptr, "new_high_score_drum");
			ApplauseStartTick = SDL_GetTicks() + ChunkDurationMs(DrumSound);
			return;
		}
	}

	if (ApplauseSound)
		Sound::PlaySound(ApplauseSound, pb::time_ticks, nullptr, "new_high_score_applause");
}

void high_score::ShutdownSounds()
{
	Sound::FreeSound(ApplauseSound);
	Sound::FreeSound(DrumSound);
	ApplauseSound = nullptr;
	DrumSound = nullptr;
	ApplauseStartTick = 0;
}

void high_score::RenderHighScoreDialog()
{
	if (ApplauseStartTick && SDL_TICKS_PASSED(SDL_GetTicks(), ApplauseStartTick))
	{
		ApplauseStartTick = 0;
		if (ApplauseSound)
			Sound::PlaySound(ApplauseSound, pb::time_ticks, nullptr, "new_record_applause");
	}

	if (ShowDialog == true)
	{
		ShowDialog = false;
		if (!ImGui::IsPopupOpen(pb::get_rc_string(Msg::HIGHSCORES_Caption)))
		{
			dlg_enter_name = false;
			while (!ScoreQueue.empty())
			{
				DlgData = ScoreQueue.back();
				ScoreQueue.pop_back();
				if (DlgData.Position < 0 || DlgData.Position >= HighScoreCount)
				{
					DlgData.Position = get_score_position(DlgData.Entry.Score);
				}

				if (DlgData.Position != -1)
				{
					dlg_enter_name = true;
					PlayNewScoreSound(DlgData.Position);
					break;
				}
			}

			ImGui::OpenPopup(pb::get_rc_string(Msg::HIGHSCORES_Caption));
		}
	}

	bool unused_open = true, textBoxSubmit = false;
	float scoreWindowScale = 1.0f;
	const ImVec4 titleBlue(0.02f, 0.03f, 0.68f, 1.0f);
	const ImVec4 brightBlue(0.02f, 0.10f, 0.95f, 1.0f);
	const ImVec4 scoreBackgroundBlue(107.0f / 255.0f, 173.0f / 255.0f, 247.0f / 255.0f, 1.0f);
	const ImVec4 yellow(1.00f, 1.00f, 0.00f, 1.0f);
	const ImVec4 paleBorder(0.90f, 0.90f, 1.00f, 1.0f);
	const ImVec4 lavender(0.49f, 0.50f, 0.98f, 1.0f);
	const ImVec4 lavenderAlt(0.43f, 0.45f, 0.94f, 1.0f);
	const ImVec4 darkText(0.00f, 0.00f, 0.00f, 1.0f);
	if (render::vscreen && pb::MainTable)
	{
		const auto& gameRect = render::DestinationRect;
		const auto tableWidthRatio = static_cast<float>(pb::MainTable->Width) / render::vscreen->Width;
		const auto sidebarLeft = gameRect.x + gameRect.w * tableWidthRatio;
		const auto sidebarCenter = (sidebarLeft + gameRect.x + gameRect.w) * 0.5f;
		const auto scoreWindowTop = gameRect.y + gameRect.h * 0.25f;
		if (fullscrn::screen_mode)
		{
			const auto scaleX = static_cast<float>(gameRect.w) / 960.0f;
			const auto scaleY = static_cast<float>(gameRect.h) / 666.0f;
			scoreWindowScale = std::max(1.0f, std::min(scaleX, scaleY));
		}

		// Keep the score table centred on the right panel, just below the Space Cadet title.
		// The destination rectangle follows both windowed and fullscreen scaling.
		ImGui::SetNextWindowPos(ImVec2(sidebarCenter, scoreWindowTop), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
	}
	// Colour this popup only; the rest of the game's ImGui theme stays unchanged.
	ImGui::PushStyleColor(ImGuiCol_Text, yellow);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, scoreBackgroundBlue);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, scoreBackgroundBlue);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, titleBlue);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, titleBlue);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, titleBlue);
	ImGui::PushStyleColor(ImGuiCol_Button, titleBlue);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, brightBlue);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.01f, 0.02f, 0.50f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, paleBorder);
	ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, paleBorder);
	ImGui::PushStyleColor(ImGuiCol_TableBorderLight, paleBorder);
	ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, brightBlue);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, lavender);
	const int scorePaletteColorCount = 14;

	if (ImGui::BeginPopupModal(pb::get_rc_string(Msg::HIGHSCORES_Caption), &unused_open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SetWindowFontScale(scoreWindowScale);
		if (CloseDialog)
		{
			CloseDialog = false;
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			ImGui::PopStyleColor(scorePaletteColorCount);
			return;
		}
		ImGui::PushStyleColor(ImGuiCol_Text, darkText);
		// The global UI alpha is lower than 1.0 and turns black text into blue-grey
		// over the lavender rows. Keep full opacity while drawing the score table.
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
		if (ImGui::BeginTable("table1", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		{
			char buf[36];
			ImGui::TableSetupColumn(pb::get_rc_string(Msg::HIGHSCORES_Rank));
			ImGui::TableSetupColumn(pb::get_rc_string(Msg::HIGHSCORES_Name));
			ImGui::TableSetupColumn(pb::get_rc_string(Msg::HIGHSCORES_Score));
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			for (int column = 0; column < 3; ++column)
			{
				ImGui::TableSetColumnIndex(column);
				ImGui::TableHeader(ImGui::TableGetColumnName(column));
			}
			ImGui::PopStyleColor();

			for (int offset = 0, row = 0; row < HighScoreCount; row++)
			{
				ImGui::TableNextRow();
				const auto rowColour = row % 2 == 0 ? lavender : lavenderAlt;
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(rowColour));
				ImGui::TableNextColumn();
				snprintf(buf, sizeof buf, "%d", row + 1);
				ImGui::TextUnformatted(buf);

				auto currentRow = &highscore_table[row + offset];
				auto score = currentRow->Score;
				ImGui::TableNextColumn();
				if (dlg_enter_name && DlgData.Position == row)
				{
					offset = -1;
					score = DlgData.Entry.Score;
					ImGui::PushItemWidth(200 * scoreWindowScale);

					if (ImGui::IsWindowAppearing())
					{
						ImGui::SetKeyboardFocusHere(0);
					}
					if (ImGui::InputText("", DlgData.Entry.Name, IM_ARRAYSIZE(DlgData.Entry.Name),
						ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
					{
						textBoxSubmit = true;
					}
				}
				else
				{
					ImGui::TextUnformatted(currentRow->Name);
				}

				ImGui::TableNextColumn();
				score::string_format(score, buf);
				ImGui::TextUnformatted(buf);
			}
			ImGui::EndTable();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();

		if (ImGui::Button(pb::get_rc_string(Msg::GenericOk)) || textBoxSubmit)
		{
			if (dlg_enter_name)
			{
				place_new_score_into(DlgData);
			}
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button(pb::get_rc_string(Msg::GenericCancel)))
			ImGui::CloseCurrentPopup();

		ImGui::SameLine();
		if (ImGui::Button(pb::get_rc_string(Msg::HIGHSCORES_Clear)))
			ImGui::OpenPopup("Confirm");
		if (ImGui::BeginPopupModal("Confirm", nullptr, ImGuiWindowFlags_MenuBar))
		{
			ImGui::TextUnformatted(pb::get_rc_string(Msg::STRING141));
			if (ImGui::Button(pb::get_rc_string(Msg::GenericOk), ImVec2(120, 0)))
			{
				clear_table();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button(pb::get_rc_string(Msg::GenericCancel), ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::EndPopup();

		// Reenter dialog for the next score in the queue
		if (!ImGui::IsPopupOpen(pb::get_rc_string(Msg::HIGHSCORES_Caption)) && !ScoreQueue.empty())
		{
			ShowDialog = true;
		}
	}
	ImGui::PopStyleColor(scorePaletteColorCount);
}
