#include "pch.h"
#include "TTextBox.h"

#include "control.h"
#include "fullscrn.h"
#include "loader.h"
#include "pb.h"
#include "render.h"
#include "score.h"
#include "timer.h"

#include <cfloat>

namespace
{
	uint32_t DecodeBitmapChar(const char*& text)
	{
		unsigned int codepoint = 0;
		const auto length = ImTextCharFromUtf8(&codepoint, text, nullptr);
		if (length <= 0)
		{
			codepoint = static_cast<unsigned char>(*text);
			++text;
		}
		else
			text += length;
		return codepoint;
	}

	bool IsCjkCodepoint(uint32_t codepoint)
	{
		return (codepoint >= 0x3040 && codepoint <= 0x30ff) ||
			(codepoint >= 0x3400 && codepoint <= 0x9fff) ||
			(codepoint >= 0xac00 && codepoint <= 0xd7af);
	}

}


TTextBox::TTextBox(TPinballTable* table, int groupIndex) : TPinballComponent(table, groupIndex, true)
{
	OffsetX = 0;
	OffsetY = 0;
	Width = 0;
	Height = 0;
	BgBmp = render::background_bitmap;
	Font = score::msg_fontp;
	CurrentMessage = nullptr;
	PreviousMessage = nullptr;
	UseSystemFont = false;
	Timer = 0;

	if (groupIndex > 0)
	{
		/*Full tilt: text box dimensions index is offset by resolution*/
		int arrLength;
		auto dimensions = loader::query_iattribute(groupIndex + fullscrn::GetResolution(), 1500, &arrLength);
		OffsetX = dimensions[0];
		OffsetY = dimensions[1];
		Width = dimensions[2];
		Height = dimensions[3];
	}
}

TTextBox::~TTextBox()
{
	if (Timer)
	{
		if (Timer != -1)
			timer::kill(Timer);
		Timer = 0;
	}
	while (CurrentMessage)
	{
		TTextBoxMessage* message = CurrentMessage;
		TTextBoxMessage* nextMessage = message->NextMessage;
		delete message;
		CurrentMessage = nextMessage;
	}
}

int TTextBox::Message(MessageCode code, float value)
{
	return 0;
}

void TTextBox::TimerExpired(int timerId, void* caller)
{
	auto tb = static_cast<TTextBox*>(caller);
	TTextBoxMessage* message = tb->CurrentMessage;
	tb->Timer = 0;
	if (message)
	{
		TTextBoxMessage* nextMessage = message->NextMessage;
		delete message;
		tb->CurrentMessage = nextMessage;
		tb->Draw();
		control::handler(MessageCode::ControlTimerExpired, tb);
	}
}

void TTextBox::Clear(bool lowPriorityOnly)
{
	gdrv_bitmap8* bmp = BgBmp;
	if (bmp)
		gdrv::copy_bitmap(
			render::vscreen,
			Width,
			Height,
			OffsetX,
			OffsetY,
			bmp,
			OffsetX,
			OffsetY);
	else
		gdrv::fill_bitmap(render::vscreen, Width, Height, OffsetX, OffsetY, 0);
	if (Timer)
	{
		if (Timer != -1)
			timer::kill(Timer);
		Timer = 0;
	}
	while (CurrentMessage && (!lowPriorityOnly || CurrentMessage->LowPriority))
	{
		auto message = CurrentMessage;
		CurrentMessage = message->NextMessage;
		delete message;
	}
	if (CurrentMessage)
		Draw();
}

void TTextBox::Display(const char* text, float time, bool lowPriority, bool preserveLines)
{
	if (!text)
		return;
	if (CurrentMessage && !strcmp(text, PreviousMessage->Text))
	{
		PreviousMessage->Refresh(time);
		if (PreviousMessage == CurrentMessage)
		{
			if (Timer && Timer != -1)
				timer::kill(Timer);
			if (time == -1.0f)
				Timer = -1;
			else
				Timer = timer::set(time, this, TimerExpired);
		}
	}
	else
	{
		if (Timer == -1)
			Clear();

		auto message = new TTextBoxMessage(text, time, lowPriority, preserveLines);
		if (message->Text)
		{
			if (CurrentMessage)
				PreviousMessage->NextMessage = message;
			else
				CurrentMessage = message;
			PreviousMessage = message;
			if (Timer == 0)
				Draw();
		}
		else
		{
			delete message;
		}
	}
}

void TTextBox::DrawImGui()
{
	// Bitmap text is rendered directly to VScreen. Long bitmap messages use the
	// current Unicode UI font instead, regardless of the selected language.
	if ((Font && !UseSystemFont) || !CurrentMessage)
		return;

	char windowName[64];
	SDL_Rect rect;
	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoInputs;

	rect.x = OffsetX;
	rect.y = OffsetY;
	rect.w = Width;
	rect.h = Height;

	rect = fullscrn::GetScreenRectFromPinballRect(rect);

	ImGui::SetNextWindowPos(ImVec2(static_cast<float>(rect.x), static_cast<float>(rect.y)));
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(rect.w), static_cast<float>(rect.h)));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	// Use the pointer to generate a window unique name per text box
	snprintf(windowName, sizeof(windowName), "TTextBox_%p", static_cast<void*>(this));
	if (ImGui::Begin(windowName, nullptr, window_flags))
	{
		auto fontScale = fullscrn::GetScreenToPinballRatio() * (UseSystemFont ? 1.5f : 1.0f);
		ImGui::SetWindowFontScale(fontScale);
		const auto wrapWidth = static_cast<float>(rect.w - 12);
		const ImU32 textColor = this == pb::InfoTextBox ? IM_COL32(255, 220, 32, 255) : pb::TextBoxColor;
		if (UseSystemFont)
		{
			auto buildLines = [wrapWidth, this](const char* text, ImFont* font, float fontSize)
			{
				std::vector<std::pair<const char*, const char*>> result;
				const auto textEnd = text + strlen(text);
				const auto fontRatio = fontSize / font->FontSize;
				while (text < textEnd)
				{
					const auto paragraphEndFound = strchr(text, '\n');
					const auto paragraphEnd = paragraphEndFound ? paragraphEndFound : textEnd;
					if (text == paragraphEnd)
						result.push_back({text, text});
					while (text < paragraphEnd)
					{
						auto lineEnd = CurrentMessage->PreserveLines ? paragraphEnd :
							font->CalcWordWrapPositionA(fontRatio, text, paragraphEnd, wrapWidth);
						if (lineEnd <= text)
							lineEnd = paragraphEnd;
						auto visibleEnd = lineEnd;
						while (visibleEnd > text && (*(visibleEnd - 1) == ' ' || *(visibleEnd - 1) == '\t'))
							--visibleEnd;
						result.push_back({text, visibleEnd});
						text = lineEnd;
						while (text < paragraphEnd && (*text == ' ' || *text == '\t'))
							++text;
					}
					if (text < textEnd && *text == '\n')
						++text;
				}
				return result;
			};

			auto font = ImGui::GetFont();
			auto fontSize = ImGui::GetFontSize();
			if (CurrentMessage->PreserveLines)
			{
				const auto size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, CurrentMessage->Text);
				const auto fit = std::min(1.0f, std::min(wrapWidth / std::max(1.0f, size.x),
					static_cast<float>(rect.h - 8) / std::max(1.0f, size.y)));
				fontScale *= fit;
				ImGui::SetWindowFontScale(fontScale);
				fontSize = ImGui::GetFontSize();
			}
			auto lines = buildLines(CurrentMessage->Text, font, fontSize);
			while (lines.size() * fontSize > rect.h - 8 &&
				fontScale > fullscrn::GetScreenToPinballRatio() * 0.75f)
			{
				fontScale -= fullscrn::GetScreenToPinballRatio() * 0.05f;
				ImGui::SetWindowFontScale(fontScale);
				fontSize = ImGui::GetFontSize();
				lines = buildLines(CurrentMessage->Text, font, fontSize);
			}

			const auto blockHeight = lines.size() * fontSize;
			auto lineY = std::max(2.0f, (rect.h - blockHeight) * 0.5f);
			const auto windowPos = ImGui::GetWindowPos();
			auto drawList = ImGui::GetWindowDrawList();
			for (const auto& line : lines)
			{
				const auto lineSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, line.first, line.second);
				const auto lineX = std::max(6.0f, (rect.w - lineSize.x) * 0.5f);
				const ImVec2 position(windowPos.x + lineX, windowPos.y + lineY);
				drawList->AddText(font, fontSize, position, textColor, line.first, line.second);
				drawList->AddText(font, fontSize, ImVec2(position.x + 1.0f, position.y), textColor,
				                  line.first, line.second);
				drawList->AddText(font, fontSize, ImVec2(position.x, position.y + 1.0f), textColor,
				                  line.first, line.second);
				lineY += fontSize;
			}
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, textColor);
			ImGui::TextWrapped("%s", CurrentMessage->Text);
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void TTextBox::Draw()
{
	UseSystemFont = false;
	auto bmp = BgBmp;
	if (bmp)
		gdrv::copy_bitmap(
			render::vscreen,
			Width,
			Height,
			OffsetX,
			OffsetY,
			bmp,
			OffsetX,
			OffsetY);
	else
		gdrv::fill_bitmap(render::vscreen, Width, Height, OffsetX, OffsetY, 0);

	bool display = false;
	while (CurrentMessage)
	{
		if (CurrentMessage->Time == -1.0f)
		{
			if (!CurrentMessage->NextMessage)
			{
				Timer = -1;
				display = true;
				break;
			}
		}
		else if (CurrentMessage->TimeLeft() >= -2.0f)
		{
			Timer = timer::set(std::max(CurrentMessage->TimeLeft(), 0.25f), this, TimerExpired);
			display = true;
			break;
		}

		auto tmp = CurrentMessage;
		CurrentMessage = CurrentMessage->NextMessage;
		delete tmp;
	}

	if (display)
	{
		if (CurrentMessage->PreserveLines)
		{
			UseSystemFont = true;
			return;
		}
		if (!Font)
		{
			// Immediate mode drawing using system font is handled by TTextBox::DrawImGui
			return;
		}

		// Universal decision for every language: keep the bitmap font at its exact
		// native size only when the complete message fits. Longer messages are
		// rendered later with the current Unicode UI font.
		const auto evaluateBitmapFit = this == pb::InfoTextBox && pb::FullTiltMode;
		const auto layoutWidth = evaluateBitmapFit ? std::max(1, Width - 64) : Width;
		std::vector<LayoutResult> lines{};

		const char* messageStart = CurrentMessage->Text;
		const char* messageEnd = messageStart + strlen(messageStart);
		if (evaluateBitmapFit)
		{
			// Reject bitmap mode when a complete word cannot fit. CJK scripts use
			// character boundaries because they do not separate words with spaces.
			auto widestWord = 0;
			auto currentWordWidth = 0;
			for (auto text = messageStart; text < messageEnd;)
			{
				const auto codepoint = DecodeBitmapChar(text);
				if (codepoint == ' ' || codepoint == '\n')
				{
					widestWord = std::max(widestWord, currentWordWidth);
					currentWordWidth = 0;
					continue;
				}
				auto glyph = Font->GetGlyph(codepoint);
				if (!glyph)
					continue;
				const auto glyphWidth = glyph->Width + Font->GapWidth;
				if (IsCjkCodepoint(codepoint))
				{
					widestWord = std::max(widestWord, std::max(currentWordWidth, glyphWidth));
					currentWordWidth = 0;
				}
				else
					currentWordWidth += glyphWidth;
			}
			widestWord = std::max(widestWord, currentWordWidth);
			if (widestWord > layoutWidth)
				UseSystemFont = true;

			if (!UseSystemFont)
			{
				for (auto text = messageStart; *text;)
				{
					auto line = LayoutTextLine(text, 1.0f, layoutWidth);
					if (line.Start == line.End)
						break;
					lines.push_back(line);
					text = line.End;
				}
				if (static_cast<int>(lines.size()) * Font->Height > Height)
					UseSystemFont = true;
			}

			if (UseSystemFont)
				return;
		}
		else
		{
			for (auto text = messageStart; *text;)
			{
				auto line = LayoutTextLine(text);
				if (line.Start == line.End || (static_cast<int>(lines.size()) + 1) * Font->Height > Height)
					break;
				lines.push_back(line);
				text = line.End;
			}
		}
		const auto textHeight = static_cast<int>(lines.size()) * Font->Height;

		// Textboxes in FT display texts centered
		auto offY = OffsetY;
		if (pb::FullTiltMode)
			offY += (Height - textHeight) / 2;
		for (auto line : lines)
		{
			auto offX = OffsetX;
			if (pb::FullTiltMode)
				offX += (Width - line.Width) / 2;
			for (auto text = line.Start; text < line.End;)
			{
				const auto codepoint = DecodeBitmapChar(text);
				// LayoutTextLine includes the newline in End; it is not a drawable glyph.
				if (codepoint == '\n' || codepoint == '\r')
					continue;
				auto charBmp = Font->GetGlyph(codepoint);
				if (charBmp)
				{
					auto height = charBmp->Height;
					auto width = charBmp->Width;
					if (render::background_bitmap)
					{
						if (this == pb::InfoTextBox)
							gdrv::copy_bitmap_w_transparency_tinted(render::vscreen, width, height, offX, offY,
							                                        charBmp, 0, 0, ColorRgba(255, 220, 32, 255));
						else
							gdrv::copy_bitmap_w_transparency(render::vscreen, width, height, offX, offY, charBmp, 0, 0);
					}
					else
						gdrv::copy_bitmap(render::vscreen, width, height, offX, offY, charBmp, 0, 0);
					offX += width + Font->GapWidth;
				}
			}
			offY += Font->Height;
		}
	}
}

TTextBox::LayoutResult TTextBox::LayoutTextLine(const char* textStart, float horizontalScale,
	                                             int layoutWidth) const
{
	if (layoutWidth < 0)
		layoutWidth = Width;
	auto lineWidth = 0, wordWidth = 0;
	const char *wordBoundary = nullptr, *textEnd = textStart;
	for (;;)
	{
		if (!*textEnd || *textEnd == '\n')
			break;

		const auto charStart = textEnd;
		const auto codepoint = DecodeBitmapChar(textEnd);
		auto charBmp = Font->GetGlyph(codepoint);
		if (!charBmp)
			continue;

		const auto glyphWidth = std::max(1, static_cast<int>(charBmp->Width * horizontalScale + 0.5f));
		const auto gapWidth = std::max(0, static_cast<int>(Font->GapWidth * horizontalScale + 0.5f));
		auto width = lineWidth + glyphWidth + gapWidth;
		if (width > layoutWidth)
		{
			if (wordBoundary)
			{
				textEnd = wordBoundary;
				lineWidth = wordWidth;
			}
			break;
		}
		if (codepoint == ' ')
		{
			wordBoundary = charStart;
			wordWidth = width;
		}
		else if (codepoint == '-' || IsCjkCodepoint(codepoint))
		{
			wordBoundary = textEnd;
			wordWidth = width;
		}
		lineWidth = width;
	}

	while (static_cast<unsigned char>(*textEnd) == ' ')
		++textEnd;
	if (static_cast<unsigned char>(*textEnd) == '\n')
		++textEnd;
	return LayoutResult{textStart, textEnd, lineWidth};
}
