/* Client: ESC-menu browser for ddnet-saves.txt */

#include "menus.h"

#include <base/io.h>
#include <base/math.h>
#include <base/str.h>
#include <base/vmath.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/shared/localization.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <vector>

namespace
{
	bool ParseCsvFields(const char *pLine, char *pField0, size_t Field0Size, char *pField1, size_t Field1Size, char *pField2, size_t Field2Size, char *pField3, size_t Field3Size)
	{
		char *const apFields[4] = {pField0, pField1, pField2, pField3};
		const size_t aFieldSizes[4] = {Field0Size, Field1Size, Field2Size, Field3Size};

		int Field = 0;
		size_t Out = 0;
		bool InQuotes = false;
		pField0[0] = pField1[0] = pField2[0] = pField3[0] = '\0';

		for(const char *p = pLine; *p; ++p)
		{
			if(InQuotes)
			{
				if(*p == '"')
				{
					if(p[1] == '"')
					{
						if(Out + 1 < aFieldSizes[Field])
							apFields[Field][Out++] = '"';
						++p;
					}
					else
					{
						InQuotes = false;
					}
				}
				else if(Out + 1 < aFieldSizes[Field])
				{
					apFields[Field][Out++] = *p;
				}
			}
			else if(*p == '"')
			{
				InQuotes = true;
			}
			else if(*p == ',')
			{
				apFields[Field][Out] = '\0';
				++Field;
				Out = 0;
				if(Field >= 4)
					return false;
			}
			else if(Out + 1 < aFieldSizes[Field])
			{
				apFields[Field][Out++] = *p;
			}
		}

		if(Field != 3 || InQuotes)
			return false;
		apFields[Field][Out] = '\0';
		return true;
	}

	bool IsSavesHeader(const char *pTime, const char *pPlayers, const char *pMap, const char *pCode)
	{
		// Legacy chat writer used "Player"; StoreSave uses "Players".
		return str_comp_nocase(pTime, "Time") == 0 &&
		       (str_comp_nocase(pPlayers, "Players") == 0 || str_comp_nocase(pPlayers, "Player") == 0) &&
		       str_comp_nocase(pMap, "Map") == 0 &&
		       str_comp_nocase(pCode, "Code") == 0;
	}

	void FormatPlayersLabel(const char *pPlayers, char *pOut, size_t OutSize, float MaxWidth, ITextRender *pTextRender)
	{
		static constexpr float FontSize = 12.0f;
		pOut[0] = '\0';
		if(!pPlayers || pPlayers[0] == '\0' || MaxWidth <= 0.0f)
			return;

		char aLine[256] = "";
		const char *p = pPlayers;
		while(*p)
		{
			while(*p == ' ')
				++p;
			if(*p == '\0')
				break;

			const char *pNameStart = p;
			const char *pComma = str_find(p, ",");
			const char *pNameEnd = pComma ? pComma : p + str_length(p);
			while(pNameEnd > pNameStart && pNameEnd[-1] == ' ')
				--pNameEnd;

			if(pNameEnd > pNameStart)
			{
				char aName[64];
				str_truncate(aName, sizeof(aName), pNameStart, pNameEnd - pNameStart);

				char aCandidate[512];
				if(aLine[0] != '\0')
					str_format(aCandidate, sizeof(aCandidate), "%s, %s", aLine, aName);
				else
					str_copy(aCandidate, aName);

				if(aLine[0] != '\0' && pTextRender->TextWidth(FontSize, aCandidate) > MaxWidth)
				{
					str_append(pOut, aLine, OutSize);
					str_append(pOut, "\n", OutSize);
					str_copy(aLine, aName, sizeof(aLine));
				}
				else
				{
					str_copy(aLine, aCandidate, sizeof(aLine));
				}
			}

			if(!pComma)
				break;
			p = pComma + 1;
		}
		if(aLine[0] != '\0')
			str_append(pOut, aLine, OutSize);
	}
} // namespace

void CMenus::SavesPopulate()
{
	m_vSaves.clear();
	m_vFilteredSaves.clear();
	m_SavesSelectedIndex = 0;

	IOHANDLE File = Storage()->OpenFile(SAVES_FILE, IOFLAG_READ, IStorage::TYPE_SAVE);
	CLineReader LineReader;
	if(!LineReader.OpenFile(File))
	{
		m_SavesNeedReload = false;
		return;
	}

	std::vector<CSaveItem> vLoaded;
	while(const char *pLine = LineReader.Get())
	{
		if(pLine[0] == '\0')
			continue;

		CSaveItem Item;
		if(!ParseCsvFields(pLine, Item.m_aTime, sizeof(Item.m_aTime), Item.m_aPlayers, sizeof(Item.m_aPlayers), Item.m_aMap, sizeof(Item.m_aMap), Item.m_aCode, sizeof(Item.m_aCode)))
			continue;
		if(IsSavesHeader(Item.m_aTime, Item.m_aPlayers, Item.m_aMap, Item.m_aCode))
			continue;
		if(Item.m_aCode[0] == '\0')
			continue;
		vLoaded.push_back(Item);
	}

	// File is append-only (oldest first); sorting is applied afterwards.
	m_vSaves = std::move(vLoaded);
	m_SavesNeedReload = false;
	SavesSort();
	SavesRefreshFiltered();
}

void CMenus::SavesSort()
{
	const bool Desc = g_Config.m_ClSavesSortOrder != 0;
	if(g_Config.m_ClSavesSort == SAVES_SORT_PLAYERS)
	{
		std::stable_sort(m_vSaves.begin(), m_vSaves.end(), [Desc](const CSaveItem &Left, const CSaveItem &Right) {
			const int Cmp = str_comp_nocase(Left.m_aPlayers, Right.m_aPlayers);
			return Desc ? Cmp > 0 : Cmp < 0;
		});
	}
	else if(g_Config.m_ClSavesSort == SAVES_SORT_MAP)
	{
		std::stable_sort(m_vSaves.begin(), m_vSaves.end(), [Desc](const CSaveItem &Left, const CSaveItem &Right) {
			const int Cmp = str_comp_nocase(Left.m_aMap, Right.m_aMap);
			return Desc ? Cmp > 0 : Cmp < 0;
		});
	}
	else // SAVES_SORT_DATE (timestamps are "%Y-%m-%d %H:%M:%S")
	{
		std::stable_sort(m_vSaves.begin(), m_vSaves.end(), [Desc](const CSaveItem &Left, const CSaveItem &Right) {
			const int Cmp = str_comp(Left.m_aTime, Right.m_aTime);
			return Desc ? Cmp > 0 : Cmp < 0;
		});
	}
}

void CMenus::SavesRefreshFiltered()
{
	m_vFilteredSaves.clear();

	const char *pSearch = m_SavesSearchInput.GetString();
	const bool HasSearch = pSearch[0] != '\0';
	const char *pCurrentMap = GameClient()->Map() ? GameClient()->Map()->BaseName() : "";
	const bool FilterMap = m_SavesCurrentMapOnly && pCurrentMap[0] != '\0';

	for(size_t i = 0; i < m_vSaves.size(); ++i)
	{
		const CSaveItem &Item = m_vSaves[i];
		if(FilterMap && str_comp_nocase(Item.m_aMap, pCurrentMap) != 0)
			continue;
		if(HasSearch)
		{
			if(!str_utf8_find_nocase(Item.m_aTime, pSearch) &&
				!str_utf8_find_nocase(Item.m_aPlayers, pSearch) &&
				!str_utf8_find_nocase(Item.m_aMap, pSearch) &&
				!str_utf8_find_nocase(Item.m_aCode, pSearch))
			{
				continue;
			}
		}
		m_vFilteredSaves.push_back((int)i);
	}

	if(m_SavesSelectedIndex >= (int)m_vFilteredSaves.size())
		m_SavesSelectedIndex = m_vFilteredSaves.empty() ? 0 : (int)m_vFilteredSaves.size() - 1;
}

void CMenus::SavesRequestLoadSelected()
{
	if(!SavesCanLoadSelected())
		return;

	const CSaveItem &Item = m_vSaves[m_vFilteredSaves[m_SavesSelectedIndex]];
	char aMessage[256];
	str_format(aMessage, sizeof(aMessage), Localize("Load save on map '%s' from %s?"), Item.m_aMap, Item.m_aTime);
	PopupConfirm(Localize("Load save"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmLoadSave);
}

void CMenus::PopupConfirmLoadSave()
{
	SavesLoadSelected();
}

bool CMenus::SavesCanLoadSelected() const
{
	if(m_SavesSelectedIndex < 0 || m_SavesSelectedIndex >= (int)m_vFilteredSaves.size())
		return false;

	const char *pCurrentMap = GameClient()->Map() ? GameClient()->Map()->BaseName() : "";
	if(pCurrentMap[0] == '\0')
		return false;

	const CSaveItem &Item = m_vSaves[m_vFilteredSaves[m_SavesSelectedIndex]];
	return str_comp_nocase(Item.m_aMap, pCurrentMap) == 0;
}

void CMenus::SavesLoadSelected()
{
	if(!SavesCanLoadSelected())
		return;

	const CSaveItem &Item = m_vSaves[m_vFilteredSaves[m_SavesSelectedIndex]];
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "/load %s", Item.m_aCode);
	GameClient()->m_Chat.SendChat(0, aBuf);
	SetActive(false);
}

void CMenus::RenderSaves(CUIRect MainView)
{
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitRight(5.0f, &MainView, nullptr);

	if(m_SavesNeedReload)
		SavesPopulate();

	CUIRect Headers, Status;
	CUIRect View = MainView;

	View.HSplitTop(17.0f, &Headers, &View);
	View.HSplitBottom(28.0f + 28.0f, &View, &Status);

	class CColumn
	{
	public:
		const char *m_pCaption;
		float m_Width;
		CUIRect m_Rect;
		int m_Id;
		int m_Sort;
	};

	enum
	{
		COL_DATE = 0,
		COL_PLAYERS,
		COL_MAP,
		COL_CODE,
	};

	const bool ShowCode = g_Config.m_ClSavesShowCode != 0;

	CColumn aCols[] = {
		{Localizable("Date"), 150.0f, {0}, COL_DATE, SAVES_SORT_DATE},
		{Localizable("Players"), ShowCode ? 220.0f : 300.0f, {0}, COL_PLAYERS, SAVES_SORT_PLAYERS},
		{Localizable("Map"), ShowCode ? 160.0f : 220.0f, {0}, COL_MAP, SAVES_SORT_MAP},
		{Localizable("Code"), 180.0f, {0}, COL_CODE, -1},
	};
	const int NumCols = ShowCode ? 4 : 3;

	Headers.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitRight(20.0f, &Headers, nullptr);

	static int s_aColIds[4] = {};
	for(int i = 0; i < NumCols; ++i)
	{
		Headers.VSplitLeft(aCols[i].m_Width, &aCols[i].m_Rect, &Headers);
		if(i + 1 < NumCols)
			Headers.VSplitLeft(2.0f, nullptr, &Headers);
	}

	for(int i = 0; i < NumCols; ++i)
	{
		if(DoButton_GridHeader(&s_aColIds[i], Localize(aCols[i].m_pCaption), g_Config.m_ClSavesSort == aCols[i].m_Sort, &aCols[i].m_Rect))
		{
			if(aCols[i].m_Sort >= 0)
			{
				if(g_Config.m_ClSavesSort == aCols[i].m_Sort)
					g_Config.m_ClSavesSortOrder ^= 1;
				else
					g_Config.m_ClSavesSortOrder = 0;
				g_Config.m_ClSavesSort = aCols[i].m_Sort;
				SavesSort();
				SavesRefreshFiltered();
			}
		}
	}

	View.Draw(ColorRGBA(0, 0, 0, 0.15f), 0, 0);

	static constexpr float FontSize = 12.0f;
	static constexpr float FirstLineHeight = 17.0f;

	bool ScrollToSelected = false;
	if(!Ui()->IsPopupOpen() && !m_SavesSearchInput.IsActive() && !GameClient()->m_GameConsole.IsActive())
	{
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_DOWN) && !m_vFilteredSaves.empty())
		{
			m_SavesSelectedIndex = std::min(m_SavesSelectedIndex + 1, (int)m_vFilteredSaves.size() - 1);
			ScrollToSelected = true;
		}
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_UP) && !m_vFilteredSaves.empty())
		{
			m_SavesSelectedIndex = std::max(m_SavesSelectedIndex - 1, 0);
			ScrollToSelected = true;
		}
	}

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarThickness = 20.0f;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	ScrollParams.m_ScrollUnit = FirstLineHeight * 3.0f;
	s_ScrollRegion.Begin(&View, &ScrollParams);

	const float PlayersColWidth = std::max(1.0f, aCols[COL_PLAYERS].m_Width - 4.0f);
	bool Activated = false;

	for(size_t i = 0; i < m_vFilteredSaves.size(); ++i)
	{
		const CSaveItem &Item = m_vSaves[m_vFilteredSaves[i]];
		const bool Selected = (int)i == m_SavesSelectedIndex;

		char aPlayers[1024];
		FormatPlayersLabel(Item.m_aPlayers, aPlayers, sizeof(aPlayers), PlayersColWidth, TextRender());

		float TextHeight = FontSize * CUi::ms_FontmodHeight;
		STextSizeProperties SizeProps{};
		SizeProps.m_pHeight = &TextHeight;
		TextRender()->TextWidth(FontSize, aPlayers, -1, -1.0f, 0, SizeProps);
		const float RowHeight = std::max(FirstLineHeight, TextHeight + 4.0f);

		CUIRect Row;
		View.HSplitTop(RowHeight, &Row, &View);
		const bool Visible = s_ScrollRegion.AddRect(Row, ScrollToSelected && Selected);

		const int Clicked = Visible ? Ui()->DoButtonLogic(&m_vFilteredSaves[i], 0, &Row, BUTTONFLAG_LEFT) : 0;
		if(Clicked)
			m_SavesSelectedIndex = (int)i;
		if(Selected && Visible && (Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || (Clicked == 1 && Ui()->DoDoubleClickLogic(&m_vFilteredSaves[i]))))
			Activated = true;

		if(Selected)
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f), IGraphics::CORNER_ALL, 5.0f);
		else if(Visible && Ui()->HotItem() == &m_vFilteredSaves[i] && !s_ScrollRegion.Animating())
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f), IGraphics::CORNER_ALL, 5.0f);

		if(!Visible)
			continue;

		CUIRect FirstLine = Row;
		FirstLine.h = FirstLineHeight;

		for(int c = 0; c < NumCols; ++c)
		{
			CUIRect Cell;
			Cell.x = aCols[c].m_Rect.x;
			Cell.y = Row.y;
			Cell.h = Row.h;
			Cell.w = aCols[c].m_Rect.w;
			Cell.VMargin(2.0f, &Cell);

			CUIRect LabelRect = FirstLine;
			LabelRect.x = Cell.x;
			LabelRect.w = Cell.w;

			if(aCols[c].m_Id == COL_PLAYERS)
			{
				const vec2 CursorPos = Ui()->CalcAlignedCursorPos(&LabelRect, vec2(0.0f, FontSize * CUi::ms_FontmodHeight), TEXTALIGN_ML);
				CTextCursor Cursor;
				Cursor.SetPosition(CursorPos);
				Cursor.m_FontSize = FontSize;
				Cursor.m_LineWidth = -1.0f;
				TextRender()->TextEx(&Cursor, aPlayers, -1);
				continue;
			}

			const char *pText = Item.m_aTime;
			if(aCols[c].m_Id == COL_MAP)
				pText = Item.m_aMap;
			else if(aCols[c].m_Id == COL_CODE)
				pText = g_Config.m_ClStreamerMode ? "*** *** ***" : Item.m_aCode;

			SLabelProperties Props;
			Props.m_MaxWidth = LabelRect.w;
			Props.m_EllipsisAtEnd = true;
			Ui()->DoLabel(&LabelRect, pText, FontSize, TEXTALIGN_ML, Props);
		}
	}

	s_ScrollRegion.End();

	const bool CanLoad = SavesCanLoadSelected();
	if(Activated && CanLoad)
		SavesRequestLoadSelected();

	CUIRect SearchRow;
	Status.HSplitTop(28.0f, &SearchRow, &Status);

	SearchRow.Draw(ColorRGBA(1, 1, 1, 0.25f), 0, 0);
	SearchRow.Margin(5.0f, &SearchRow);

	// Search width matches refresh (25) + gap (5) + open-saves (175) below.
	{
		CUIRect Search;
		SearchRow.VSplitLeft(25.0f + 5.0f + 175.0f, &Search, &SearchRow);
		if(Ui()->DoEditBox_Search(&m_SavesSearchInput, &Search, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive()))
			SavesRefreshFiltered();
	}

	Status.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_B, 5.0f);
	Status.Margin(5.0f, &Status);

	CUIRect Button;
	Status.VSplitLeft(25.0f, &Button, &Status);

	static CButtonContainer s_ReloadButton;
	if(Ui()->DoButton_FontIcon(&s_ReloadButton, FontIcon::ARROW_ROTATE_RIGHT, 0, &Button, BUTTONFLAG_LEFT) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
	{
		m_SavesNeedReload = true;
		SavesPopulate();
	}
	GameClient()->m_Tooltips.DoToolTip(&s_ReloadButton, &Button, Localize("Refresh"));

	Status.VSplitLeft(5.0f, nullptr, &Status);
	Status.VSplitLeft(175.0f, &Button, &Status);
	static CButtonContainer s_DirectoryButton;
	if(DoButton_Menu(&s_DirectoryButton, Localize("Open saves file"), 0, &Button))
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, SAVES_FILE, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	Status.VSplitLeft(5.0f, nullptr, &Status);
	Status.VSplitLeft(160.0f, &Button, &Status);
	if(DoButton_CheckBox(&m_SavesCurrentMapOnly, Localize("Current map only"), m_SavesCurrentMapOnly, &Button))
	{
		m_SavesCurrentMapOnly ^= 1;
		SavesRefreshFiltered();
	}

	Status.VSplitRight(120.0f, &Status, &Button);
	static CButtonContainer s_LoadButton;
	const ColorRGBA LoadColor = CanLoad ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f);
	if(DoButton_Menu(&s_LoadButton, Localize("Load"), 0, &Button, CanLoad ? BUTTONFLAG_LEFT : BUTTONFLAG_NONE, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, LoadColor) ||
		(CanLoad && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) && !m_SavesSearchInput.IsActive()))
	{
		SavesRequestLoadSelected();
	}
	if(!CanLoad)
		GameClient()->m_Tooltips.DoToolTip(&s_LoadButton, &Button, Localize("Save is for a different map"));
}
