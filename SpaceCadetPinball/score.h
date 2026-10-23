#pragma once
#include "gdrv.h"

struct scoreStruct
{
	int Score;
	bool DirtyFlag;
	gdrv_bitmap8* BackgroundBmp;
	int OffsetX;
	int OffsetY;
	int Width;
	int Height;
	gdrv_bitmap8* CharBmp[10];
	bool UseTint{};
	ColorRgba TintColor{};
	uint32_t BlinkStartTicks{};
	bool BlinkActive{};
};

struct score_msg_font_type
{
	int GapWidth;
	int Height;
	gdrv_bitmap8* Chars[256]{};
	std::unordered_map<uint32_t, gdrv_bitmap8*> UnicodeChars{};
	std::vector<gdrv_bitmap8*> GeneratedChars{};

	~score_msg_font_type();
	gdrv_bitmap8* GetGlyph(uint32_t codepoint) const;
};


class score
{
public:
	static score_msg_font_type* msg_fontp;
	static int init();
	static scoreStruct* create(LPCSTR fieldName, gdrv_bitmap8* renderBgBmp);
	static scoreStruct* dup(scoreStruct* score, int scoreIndex);
	static void load_msg_font(LPCSTR lpName);
	static void unload_msg_font();
	static void erase(scoreStruct* score, int blitFlag);
	static void set(scoreStruct* score, int value);
	static void update(scoreStruct* score);
	static void string_format(int score, char* str);
};
