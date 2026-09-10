/* GClient — settings tab for client-specific options (gc_* config). */

#include "menus.h"

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <algorithm>

bool CMenus::DoSliderWithScaledValue(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, int Scale, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	const bool NoClampValue = Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;

	int Value = *pOption;
	Min /= Scale;
	Max /= Scale;
	int Increment = std::max(1, (Max - Min) / 35);
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && Ui()->MouseInside(pRect))
	{
		Value += Increment;
		Value = std::clamp(Value, Min, Max);
	}
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ui()->MouseInside(pRect))
	{
		Value -= Increment;
		Value = std::clamp(Value, Min, Max);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %i%s", pStr, Value * Scale, pSuffix);

	if(NoClampValue)
		Value = std::clamp(Value, Min, Max);

	CUIRect Label, ScrollBar;
	pRect->VSplitMid(&Label, &ScrollBar, std::min(10.0f, pRect->w * 0.05f));

	const float LabelFontSize = Label.h * CUi::ms_FontmodHeight * 0.8f;
	Ui()->DoLabel(&Label, aBuf, LabelFontSize, TEXTALIGN_ML);

	Value = pScale->ToAbsolute(Ui()->DoScrollbarH(pId, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(NoClampValue && ((Value == Min && *pOption < Min) || (Value == Max && *pOption > Max)))
		Value = *pOption;

	if(*pOption != Value)
	{
		*pOption = Value;
		return true;
	}
	return false;
}

bool CMenus::DoSliderWithDecimalValue(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, int Scale, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	const bool NoClampValue = Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;

	int Value = *pOption;
	int Increment = std::max(1, (Max - Min) / 35);
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && Ui()->MouseInside(pRect))
	{
		Value += Increment;
		Value = std::clamp(Value, Min, Max);
	}
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ui()->MouseInside(pRect))
	{
		Value -= Increment;
		Value = std::clamp(Value, Min, Max);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %.2f%s", pStr, Value / (float)Scale, pSuffix);

	if(NoClampValue)
		Value = std::clamp(Value, Min, Max);

	CUIRect Label, ScrollBar;
	pRect->VSplitMid(&Label, &ScrollBar, std::min(10.0f, pRect->w * 0.05f));

	const float LabelFontSize = Label.h * CUi::ms_FontmodHeight * 0.8f;
	Ui()->DoLabel(&Label, aBuf, LabelFontSize, TEXTALIGN_ML);

	Value = pScale->ToAbsolute(Ui()->DoScrollbarH(pId, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(NoClampValue && ((Value == Min && *pOption < Min) || (Value == Max && *pOption > Max)))
		Value = *pOption;

	if(*pOption != Value)
	{
		*pOption = Value;
		return true;
	}
	return false;
}

void CMenus::RenderSettingsGClient(CUIRect MainView)
{
	const float LineSize = 20.0f;
	const float HeadlineFontSize = 20.0f;
	const float HeadlineHeight = 30.0f;
	const float MarginSmall = 5.0f;
	const float MarginBetweenSections = 20.0f;

	CUIRect Column, Button, Label;

	MainView.VSplitMid(&Column, nullptr, MarginBetweenSections);
	Column.VSplitLeft(MarginSmall, nullptr, &Column);
	Column.VSplitRight(MarginSmall, &Column, nullptr);

	Ui()->DoLabel_AutoLineSize(Localize("Fast input"), HeadlineFontSize, TEXTALIGN_ML, &Column, HeadlineHeight);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_GcFastInput, Localize("Enable fast input (reduced visual delay)"), &g_Config.m_GcFastInput, &Column, LineSize);

	if(!g_Config.m_GcFastInput)
	{
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Label, &Column);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.55f);
		Ui()->DoLabel(&Label, Localize("Turn this on to configure prediction style and amount."), 12.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		return;
	}

	Column.HSplitTop(MarginSmall, nullptr, &Column);
	Column.HSplitTop(LineSize, &Label, &Column);
	Ui()->DoLabel(&Label, Localize("Style"), 14.0f, TEXTALIGN_ML);

	Column.HSplitTop(MarginSmall, nullptr, &Column);
	Column.HSplitTop(LineSize, &Button, &Column);
	{
		CUIRect ClassicButton, AggressiveButton;
		Button.VSplitMid(&ClassicButton, &AggressiveButton, 2.0f);
		static CButtonContainer s_FastInputClassic;
		static CButtonContainer s_FastInputAggressive;
		if(DoButton_Menu(&s_FastInputClassic, Localize("Classic (ms)"), g_Config.m_GcFastInputMode == 0, &ClassicButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
			g_Config.m_GcFastInputMode = 0;
		if(DoButton_Menu(&s_FastInputAggressive, Localize("Aggressive (ticks)"), g_Config.m_GcFastInputMode == 1, &AggressiveButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
			g_Config.m_GcFastInputMode = 1;
	}

	Column.HSplitTop(MarginSmall, nullptr, &Column);
	Column.HSplitTop(LineSize, &Button, &Column);
	if(g_Config.m_GcFastInputMode == 1)
		DoSliderWithDecimalValue(&g_Config.m_GcFastInputTicks, &g_Config.m_GcFastInputTicks, &Button, Localize("Amount"), 0, 200, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, " ticks");
	else
		DoSliderWithScaledValue(&g_Config.m_GcFastInputAmount, &g_Config.m_GcFastInputAmount, &Button, Localize("Amount"), 1, 40, 1, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");

	Column.HSplitTop(MarginSmall, nullptr, &Column);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_GcFastInputOthers, Localize("Also apply to other players"), &g_Config.m_GcFastInputOthers, &Column, LineSize);

	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	Column.HSplitTop(LineSize * 2.0f, &Label, &Column);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.55f);
	Ui()->DoLabel(&Label, Localize("Aggressive predicts in ticks and repredicts every frame. Classic uses milliseconds."), 12.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
