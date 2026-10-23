#pragma once
class TTextBoxMessage
{
public:
	TTextBoxMessage* NextMessage;
	char* Text;
	float Time;
	int EndTicks;
	bool LowPriority;
	bool PreserveLines;

	TTextBoxMessage(const char* text, float time, bool lowPriority, bool preserveLines = false);
	~TTextBoxMessage();
	float TimeLeft() const;
	void Refresh(float time);
};
