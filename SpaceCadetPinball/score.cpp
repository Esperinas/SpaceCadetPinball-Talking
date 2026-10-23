#include "pch.h"
#include "score.h"

#include "EmbeddedData.h"
#include "fullscrn.h"
#include "loader.h"
#include "GroupData.h"
#include "pb.h"
#include "render.h"
#include "UnicodeGlyphData.h"


score_msg_font_type* score::msg_fontp;

score_msg_font_type::~score_msg_font_type()
{
	for (auto bmp : GeneratedChars)
		delete bmp;
}

gdrv_bitmap8* score_msg_font_type::GetGlyph(uint32_t codepoint) const
{
	if (codepoint < 256 && Chars[codepoint])
		return Chars[codepoint];
	const auto it = UnicodeChars.find(codepoint);
	if (it != UnicodeChars.end())
		return it->second;
	switch (codepoint)
	{
	case 0x00A0: return Chars[static_cast<unsigned char>(' ')];
	case 0x00AB: case 0x00BB: return Chars[static_cast<unsigned char>('"')];
	case 0x00B0: return Chars[static_cast<unsigned char>('o')];
	case 0x0152: case 0x0153: return Chars[static_cast<unsigned char>('O')];
	case 0x2018: case 0x2019: return Chars[static_cast<unsigned char>('\'')];
	case 0x2013: case 0x2014: return Chars[static_cast<unsigned char>('-')];
	case 0x2026: return Chars[static_cast<unsigned char>('.')];
	default: return Chars[static_cast<unsigned char>('?')];
	}
}

namespace
{
	enum class Accent { Grave, Acute, Circumflex, Diaeresis, Cedilla, Tilde };

	void AddAccentedGlyph(score_msg_font_type& font, unsigned char target, unsigned char base, Accent accent)
	{
		auto source = font.Chars[base];
		if (!source || !source->BmpBufPtr1 || source->Width < 3 || source->Height < 3)
			return;

		auto glyph = new gdrv_bitmap8(source->Width, source->Height, false);
		std::copy(source->BmpBufPtr1, source->BmpBufPtr1 + source->Width * source->Height,
		          glyph->BmpBufPtr1);

		// Pick an actual foreground pixel. Palette entries used for the
		// transparent/background part of the DAT bitmap can still be non-zero,
		// so taking the first non-zero pixel produces invisible accent marks.
		ColorRgba ink{};
		auto bestCount = 0;
		const auto pixelCount = source->Width * source->Height;
		for (auto i = 0; i < pixelCount; ++i)
		{
			const auto candidate = source->BmpBufPtr1[i];
			if (!candidate.Color)
				continue;
			auto count = 0;
			for (auto j = 0; j < pixelCount; ++j)
				if (source->BmpBufPtr1[j].Color == candidate.Color)
					++count;
			if (count > bestCount)
			{
				bestCount = count;
				ink = candidate;
			}
		}
		if (!ink.Color)
		{
			delete glyph;
			return;
		}

		auto dot = [&](int x, int y)
		{
			if (x >= 0 && x < glyph->Width && y >= 0 && y < glyph->Height)
				glyph->BmpBufPtr1[y * glyph->Stride + x] = ink;
		};
		const auto middle = glyph->Width / 2;
		switch (accent)
		{
		case Accent::Grave:
			dot(middle - 3, 0); dot(middle - 2, 0); dot(middle - 1, 0);
			dot(middle - 2, 1); dot(middle - 1, 1); dot(middle, 1);
			dot(middle - 1, 2); dot(middle, 2); dot(middle + 1, 2);
			dot(middle, 3); dot(middle + 1, 3); dot(middle + 2, 3);
			break;
		case Accent::Acute:
			dot(middle + 1, 0); dot(middle + 2, 0); dot(middle + 3, 0);
			dot(middle, 1); dot(middle + 1, 1); dot(middle + 2, 1);
			dot(middle - 1, 2); dot(middle, 2); dot(middle + 1, 2);
			dot(middle - 2, 3); dot(middle - 1, 3); dot(middle, 3);
			break;
		case Accent::Circumflex:
			dot(middle - 1, 0); dot(middle, 0); dot(middle + 1, 0);
			dot(middle - 2, 1); dot(middle - 1, 1); dot(middle + 1, 1); dot(middle + 2, 1);
			dot(middle - 3, 2); dot(middle - 2, 2); dot(middle + 2, 2); dot(middle + 3, 2);
			dot(middle - 3, 3); dot(middle - 2, 3); dot(middle + 2, 3); dot(middle + 3, 3);
			break;
		case Accent::Diaeresis:
			dot(middle - 3, 0); dot(middle - 2, 0); dot(middle - 1, 0);
			dot(middle - 3, 1); dot(middle - 2, 1); dot(middle - 1, 1);
			dot(middle - 3, 2); dot(middle - 2, 2); dot(middle - 1, 2);
			dot(middle + 1, 0); dot(middle + 2, 0); dot(middle + 3, 0);
			dot(middle + 1, 1); dot(middle + 2, 1); dot(middle + 3, 1);
			dot(middle + 1, 2); dot(middle + 2, 2); dot(middle + 3, 2);
			break;
		case Accent::Cedilla:
			dot(middle, glyph->Height - 3); dot(middle + 1, glyph->Height - 3);
			dot(middle - 1, glyph->Height - 2); dot(middle, glyph->Height - 2);
			dot(middle - 1, glyph->Height - 1); dot(middle - 2, glyph->Height - 1);
			break;
		case Accent::Tilde:
			dot(middle - 3, 1); dot(middle - 2, 0); dot(middle - 1, 0);
			dot(middle, 1); dot(middle + 1, 2); dot(middle + 2, 2);
			dot(middle + 3, 1);
			break;
		}

		font.Chars[target] = glyph;
		font.GeneratedChars.push_back(glyph);
	}

	void AddRotatedGlyph(score_msg_font_type& font, unsigned char target, unsigned char base)
	{
		auto source = font.Chars[base];
		if (!source || !source->BmpBufPtr1)
			return;
		auto glyph = new gdrv_bitmap8(source->Width, source->Height, false);
		for (auto y = 0; y < source->Height; ++y)
			for (auto x = 0; x < source->Width; ++x)
				glyph->BmpBufPtr1[y * glyph->Stride + x] =
					source->BmpBufPtr1[(source->Height - 1 - y) * source->Stride + source->Width - 1 - x];
		font.Chars[target] = glyph;
		font.GeneratedChars.push_back(glyph);
	}

	void AddCompositeGlyph(score_msg_font_type& font, unsigned char target, unsigned char left, unsigned char right)
	{
		auto leftGlyph = font.Chars[left];
		auto rightGlyph = font.Chars[right];
		if (!leftGlyph || !rightGlyph || !leftGlyph->BmpBufPtr1 || !rightGlyph->BmpBufPtr1)
			return;

		const auto overlap = std::max(1, std::min(leftGlyph->Width, rightGlyph->Width) / 3);
		const auto width = leftGlyph->Width + rightGlyph->Width - overlap;
		const auto height = std::max(leftGlyph->Height, rightGlyph->Height);
		auto glyph = new gdrv_bitmap8(width, height, false);
		std::fill(glyph->BmpBufPtr1, glyph->BmpBufPtr1 + glyph->Stride * glyph->Height, ColorRgba{0});

		auto blit = [&](gdrv_bitmap8* source, int xOffset)
		{
			for (auto y = 0; y < source->Height; ++y)
				for (auto x = 0; x < source->Width; ++x)
				{
					const auto pixel = source->BmpBufPtr1[y * source->Stride + x];
					if (pixel.Color)
						glyph->BmpBufPtr1[y * glyph->Stride + xOffset + x] = pixel;
				}
		};
		blit(leftGlyph, 0);
		blit(rightGlyph, leftGlyph->Width - overlap);

		font.Chars[target] = glyph;
		font.GeneratedChars.push_back(glyph);
	}

	void AddLatinGlyphs(score_msg_font_type& font)
	{
		for (const auto& item : std::initializer_list<std::tuple<unsigned char, unsigned char, Accent>>{
			{0xC0, 'A', Accent::Grave}, {0xC1, 'A', Accent::Acute}, {0xC2, 'A', Accent::Circumflex},
			{0xC3, 'A', Accent::Tilde}, {0xC4, 'A', Accent::Diaeresis},
			{0xC7, 'C', Accent::Cedilla}, {0xC8, 'E', Accent::Grave}, {0xC9, 'E', Accent::Acute},
			{0xCA, 'E', Accent::Circumflex}, {0xCB, 'E', Accent::Diaeresis}, {0xCC, 'I', Accent::Grave},
			{0xCD, 'I', Accent::Acute}, {0xCE, 'I', Accent::Circumflex}, {0xCF, 'I', Accent::Diaeresis},
			{0xD1, 'N', Accent::Tilde}, {0xD2, 'O', Accent::Grave}, {0xD3, 'O', Accent::Acute},
			{0xD4, 'O', Accent::Circumflex}, {0xD5, 'O', Accent::Tilde}, {0xD6, 'O', Accent::Diaeresis},
			{0xD9, 'U', Accent::Grave}, {0xDA, 'U', Accent::Acute}, {0xDB, 'U', Accent::Circumflex},
			{0xDC, 'U', Accent::Diaeresis},
			{0xE0, 'a', Accent::Grave}, {0xE1, 'a', Accent::Acute}, {0xE2, 'a', Accent::Circumflex},
			{0xE3, 'a', Accent::Tilde}, {0xE4, 'a', Accent::Diaeresis},
			{0xE7, 'c', Accent::Cedilla}, {0xE8, 'e', Accent::Grave}, {0xE9, 'e', Accent::Acute},
			{0xEA, 'e', Accent::Circumflex}, {0xEB, 'e', Accent::Diaeresis}, {0xEC, 'i', Accent::Grave},
			{0xED, 'i', Accent::Acute}, {0xEE, 'i', Accent::Circumflex}, {0xEF, 'i', Accent::Diaeresis},
			{0xF1, 'n', Accent::Tilde}, {0xF2, 'o', Accent::Grave}, {0xF3, 'o', Accent::Acute},
			{0xF4, 'o', Accent::Circumflex}, {0xF5, 'o', Accent::Tilde}, {0xF6, 'o', Accent::Diaeresis},
			{0xF9, 'u', Accent::Grave}, {0xFA, 'u', Accent::Acute}, {0xFB, 'u', Accent::Circumflex},
			{0xFC, 'u', Accent::Diaeresis},
			{0xFF, 'y', Accent::Diaeresis}})
			AddAccentedGlyph(font, std::get<0>(item), std::get<1>(item), std::get<2>(item));

		AddRotatedGlyph(font, 0xA1, '!');
		AddRotatedGlyph(font, 0xBF, '?');
		AddCompositeGlyph(font, 0xDF, 'f', 's');
	}

	void AddUnicodeGlyphs(score_msg_font_type& font)
	{
		auto reference = font.Chars[static_cast<unsigned char>('A')];
		if (!reference || !reference->BmpBufPtr1 || font.Height <= 0)
			return;

		ColorRgba ink{};
		auto bestCount = 0;
		const auto pixelCount = reference->Width * reference->Height;
		for (auto index = 0; index < pixelCount; ++index)
		{
			const auto candidate = reference->BmpBufPtr1[index];
			if (!candidate.Color)
				continue;
			auto count = 0;
			for (auto compare = 0; compare < pixelCount; ++compare)
				if (reference->BmpBufPtr1[compare].Color == candidate.Color)
					++count;
			if (count > bestCount)
			{
				bestCount = count;
				ink = candidate;
			}
		}
		if (!ink.Color)
			return;

		for (const auto& source : UnicodeBitmapGlyphs)
		{
			const auto width = std::max(1, (source.Width * font.Height + 15) / 16);
			auto glyph = new gdrv_bitmap8(width, font.Height, false);
			std::fill(glyph->BmpBufPtr1, glyph->BmpBufPtr1 + glyph->Stride * glyph->Height, ColorRgba{0});
			for (auto y = 0; y < glyph->Height; ++y)
			{
				const auto sourceY = std::min(15, y * 16 / glyph->Height);
				for (auto x = 0; x < glyph->Width; ++x)
				{
					const auto sourceX = std::min<int>(source.Width - 1, x * source.Width / glyph->Width);
					if (source.Rows[sourceY] & (1u << (source.Width - 1 - sourceX)))
						glyph->BmpBufPtr1[y * glyph->Stride + x] = ink;
				}
			}
			font.UnicodeChars[source.Codepoint] = glyph;
			font.GeneratedChars.push_back(glyph);
		}
	}
}

int score::init()
{
	return 1;
}

scoreStruct* score::create(LPCSTR fieldName, gdrv_bitmap8* renderBgBmp)
{
	auto score = new scoreStruct();
	if (!score)
		return nullptr;
	score->Score = -9999;
	score->BackgroundBmp = renderBgBmp;

	/*Full tilt: score box dimensions index is offset by resolution*/
	auto dimensionsId = pb::record_table->record_labeled(fieldName) + fullscrn::GetResolution();
	auto dimensions = reinterpret_cast<int16_t*>(loader::loader_table->field(dimensionsId,
	                                                                         FieldTypes::ShortArray));
	if (!dimensions)
	{
		delete score;
		return nullptr;
	}
	int groupIndex = *dimensions++;
	score->OffsetX = *dimensions++;
	score->OffsetY = *dimensions++;
	score->Width = *dimensions++;
	score->Height = *dimensions;

	for (int index = 0; index < 10; index++)
	{
		score->CharBmp[index] = loader::loader_table->GetBitmap(groupIndex);
		++groupIndex;
	}
	return score;
}

scoreStruct* score::dup(scoreStruct* score, int scoreIndex)
{
	return new scoreStruct(*score);
}

void score::load_msg_font(LPCSTR lpName)
{
	if (!pb::record_table)
		return;

	int groupIndex = pb::record_table->record_labeled(lpName);
	if (groupIndex < 0)
		return;

	msg_fontp = new score_msg_font_type();

	// FT font has multiple resolutions
	auto gapArray = reinterpret_cast<int16_t*>(pb::record_table->field(groupIndex, FieldTypes::ShortArray));
	if (gapArray)
		msg_fontp->GapWidth = gapArray[fullscrn::GetResolution()];
	else
		msg_fontp->GapWidth = 0;

	for (auto charIndex = 32; charIndex < 128; charIndex++, ++groupIndex)
	{
		auto bmp = pb::record_table->GetBitmap(groupIndex);
		if (!bmp)
			break;
		if (!msg_fontp->Height)
			msg_fontp->Height = bmp->Height;
		msg_fontp->Chars[charIndex] = bmp;
	}
	AddLatinGlyphs(*msg_fontp);
	AddUnicodeGlyphs(*msg_fontp);
}


void score::unload_msg_font()
{
	if (msg_fontp)
	{
		delete msg_fontp;
		msg_fontp = nullptr;
	}
}

void score::erase(scoreStruct* score, int blitFlag)
{
	if (score)
	{
		if (score->BackgroundBmp)
			gdrv::copy_bitmap(
				render::vscreen,
				score->Width,
				score->Height,
				score->OffsetX,
				score->OffsetY,
				score->BackgroundBmp,
				score->OffsetX,
				score->OffsetY);
		else
			gdrv::fill_bitmap(render::vscreen, score->Width, score->Height, score->OffsetX, score->OffsetY, 0);
	}
}

void score::set(scoreStruct* score, int value)
{
	if (score)
	{
		score->Score = value;
		score->DirtyFlag = true;
	}
}


void score::update(scoreStruct* score)
{
	char scoreBuf[12]{};
	if (score && score->DirtyFlag && score->Score <= 1000000000)
	{
		score->DirtyFlag = false;
		int x = score->Width + score->OffsetX;
		int y = score->OffsetY;
		erase(score, 0);
		if (score->Score >= 0)
		{
			snprintf(scoreBuf, sizeof scoreBuf, "%d", score->Score);
			for (ptrdiff_t index = strlen(scoreBuf) - 1; index >= 0; index--)
			{
				unsigned char curChar = scoreBuf[index];
				curChar -= '0';
				gdrv_bitmap8* bmp = score->CharBmp[curChar % 10u];
				x -= bmp->Width;
				int height = bmp->Height;
				int width = bmp->Width;
				// Tint is independent of the background mode (the custom table uses nullptr).
				if (score->UseTint)
					gdrv::copy_bitmap_w_transparency_tinted(render::vscreen, width, height, x, y, bmp, 0, 0,
					                                        score->TintColor);
				else if (render::background_bitmap)
					gdrv::copy_bitmap_w_transparency(render::vscreen, width, height, x, y, bmp, 0, 0);
				else
					gdrv::copy_bitmap(render::vscreen, width, height, x, y, bmp, 0, 0);
			}
		}
	}
}

void score::string_format(int score, char* str)
{
	if (score == -999)
	{
		*str = 0;
	}
	else
	{
		int scoreMillions = score % 1000000000 / 1000000;
		if (score / 1000000000 <= 0)
		{
			if (scoreMillions <= 0)
			{
				if (score % 1000000 / 1000 <= 0)
					snprintf(str, 36, "%d", score);
				else
					snprintf(str, 36, "%d,%03d", score % 1000000 / 1000, score % 1000);
			}
			else
			{
				snprintf(str, 36, "%d,%03d,%03d", scoreMillions, score % 1000000 / 1000, score % 1000);
			}
		}
		else
		{
			snprintf(
				str,
				36,
				"%d,%03d,%03d,%03d",
				score / 1000000000,
				scoreMillions,
				score % 1000000 / 1000,
				score % 1000);
		}
	}
}
