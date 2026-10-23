#pragma once

struct high_score_struct
{
	char Name[32];
	int Score;
};

struct high_score_entry
{	
	high_score_struct Entry;
	int Position;
};


class high_score
{
public:
	static constexpr int HighScoreCount = 8;
	static constexpr int LegacyHighScoreCount = 5;
	static high_score_struct highscore_table[HighScoreCount];

	static int read();
	static int write();
	static int get_score_position(int score);	

	static void show_high_score_dialog();
	static void close_high_score_dialog();
	static void show_and_set_high_score_dialog(high_score_entry score);
	static void RenderHighScoreDialog();
	static void ShutdownSounds();
private:
	static bool dlg_enter_name;
	static high_score_entry DlgData;
	static bool ShowDialog, CloseDialog;
	static std::vector<high_score_entry> ScoreQueue;
	static Mix_Chunk* ApplauseSound;
	static Mix_Chunk* DrumSound;
	static Uint32 ApplauseStartTick;

	static void clear_table();
	static void place_new_score_into(high_score_entry data);
	static void PlayNewScoreSound(int position);
};
