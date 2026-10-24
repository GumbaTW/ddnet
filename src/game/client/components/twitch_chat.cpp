#include "twitch_chat.h"

#include <base/str.h>
#include <base/time.h>
#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <algorithm>

CTwitchChat::CTwitchChat()
{
	m_TokenInput.SetHidden(true);
}

void CTwitchChat::Connect()
{
	CloseInput();
	m_Messages.clear();
	m_Scroll = 0;
	std::string Token = g_Config.m_ClTwitchAccessToken;
	if(Token.compare(0, 6, "oauth:") == 0)
		Token.erase(0, 6);
	m_Status = "Connecting...";
	m_Connection.Connect(g_Config.m_ClTwitchLogin, g_Config.m_ClTwitchChannel, Token);
	g_Config.m_ClTwitchShow = 1;
}

void CTwitchChat::Disconnect()
{
	CloseInput();
	m_Connection.Disconnect();
	m_Status = "Disconnected";
	m_Messages.clear();
}

void CTwitchChat::OnShutdown()
{
	Disconnect();
}

void CTwitchChat::CloseInput()
{
	if(m_Active)
	{
		m_Active = false;
		m_Input.Deactivate();
		GameClient()->OnRelease();
	}
}

void CTwitchChat::OnStateChange(int NewState, int OldState)
{
	CloseInput();
}

void CTwitchChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CTwitchChat *>(pUserData);
	if(pSelf->m_Active)
		pSelf->CloseInput();
	else if(pSelf->Client()->State() == IClient::STATE_ONLINE && !pSelf->GameClient()->m_Menus.IsActive() && !pSelf->GameClient()->m_Chat.IsActive())
	{
		pSelf->GameClient()->OnRelease();
		pSelf->m_Active = true;
		g_Config.m_ClTwitchShow = 1;
		pSelf->m_Input.Activate(EInputPriority::CHAT);
	}
}

void CTwitchChat::OnConsoleInit()
{
	Console()->Register("twitch_chat", "", CFGFLAG_CLIENT, ConChat, this, "Open Twitch chat input (Enter sends, Escape closes)");
}

void CTwitchChat::OnUpdate()
{
	if(m_Active && (!g_Config.m_ClTwitchShow || GameClient()->m_Menus.IsActive() || GameClient()->m_Chat.IsActive() || GameClient()->m_GameConsole.IsActive()))
		CloseInput();
	for(auto &Message : m_Connection.Poll())
	{
		if(!str_utf8_check(Message.m_Text.c_str()) || !str_utf8_check(Message.m_Name.c_str()))
			continue;
		if(Message.m_Command == "STATUS")
		{
			m_Status = Message.m_Text;
			Message.m_Name = "Twitch";
			Message.m_Command = "NOTICE";
		}
		if(Message.m_Command == "CLEARMSG")
		{
			if(!Message.m_TargetId.empty())
				m_Messages.erase(std::remove_if(m_Messages.begin(), m_Messages.end(), [&](const CTwitchMessage &Entry) { return Entry.m_Id == Message.m_TargetId; }), m_Messages.end());
		}
		else if(Message.m_Command == "CLEARCHAT")
		{
			if(Message.m_UserId.empty())
				m_Messages.clear();
			else
				m_Messages.erase(std::remove_if(m_Messages.begin(), m_Messages.end(), [&](const CTwitchMessage &Entry) { return Entry.m_UserId == Message.m_UserId || (!Message.m_Text.empty() && str_comp_nocase(Entry.m_Name.c_str(), Message.m_Text.c_str()) == 0); }), m_Messages.end());
		}
		else
		{
			if(Message.m_Command == "NOTICE")
			{
				Message.m_Name = "Twitch";
				m_Status = Message.m_Text;
			}
			if(!Message.m_Id.empty() && std::any_of(m_Messages.begin(), m_Messages.end(), [&](const CTwitchMessage &Entry) { return Entry.m_Id == Message.m_Id; }))
				continue;
			for(char &Character : Message.m_Name)
				if((unsigned char)Character < 32)
					Character = ' ';
			if(m_Scroll > 0)
				++m_Scroll;
			m_Messages.push_back({std::move(Message), time_get()});
			if(m_Messages.size() > 100)
				m_Messages.pop_front();
		}
	}
	m_Scroll = std::clamp(m_Scroll, 0, std::max(0, (int)m_Messages.size() - 1));
}

bool CTwitchChat::OnInput(const IInput::CEvent &Event)
{
	if(!m_Active)
		return false;
	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		if(Event.m_Key == KEY_ESCAPE)
		{
			CloseInput();
			return true;
		}
		if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
		{
			if(m_Input.IsEmpty() || m_Connection.Send(m_Input.GetString()))
			{
				m_Input.Clear();
				CloseInput();
				m_Scroll = 0;
			}
			else
			{
				m_Status = "Message not queued. Check connection or wait before retrying.";
				m_Messages.push_back({{"NOTICE", "Twitch", m_Status}, time_get()});
				if(m_Messages.size() > 100)
					m_Messages.pop_front();
				m_Scroll = 0;
			}
			return true;
		}
		if(Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
		{
			m_Scroll = std::clamp(m_Scroll + (Event.m_Key == KEY_MOUSE_WHEEL_UP ? 1 : -1), 0, std::max(0, (int)m_Messages.size() - 1));
			return true;
		}
	}
	m_Input.ProcessInput(Event);
	return true;
}

void CTwitchChat::OnRender()
{
	if(!g_Config.m_ClTwitchShow || Client()->State() != IClient::STATE_ONLINE || GameClient()->m_Menus.IsActive())
		return;

	const auto &GameChat = GameClient()->m_Chat;
	const float ScreenHeight = 300.0f;
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(ScreenWidth, ScreenHeight);
	const float FontSize = GameChat.FontSize();
	const float InputFontSize = FontSize * (8.0f / 6.0f);
	const bool ScoreboardOpen = GameClient()->m_Scoreboard.IsActive() && Graphics()->ScreenAspect() > 1.7f;
	const float Width = ScoreboardOpen ? std::max(85.0f, FontSize * 85.0f / 6.0f) : (float)g_Config.m_ClChatWidth;
	const float X = 5.0f;
	const float PaddingX = g_Config.m_ClChatOld ? 0.0f : GameChat.MessagePaddingX();
	const float PaddingY = g_Config.m_ClChatOld ? 0.0f : GameChat.MessagePaddingY();
	const float LineWidth = Width - PaddingX * 1.5f;
	// Use the game chat's anchor and available height, shifted vertically by
	// the only Twitch-specific layout setting. Input uses the same baseline.
	const float GameInputY = ScreenHeight - 20.0f * FontSize / 6.0f;
	const float InputHeight = InputFontSize * 2.25f;
	const float InputY = std::max(5.0f + FontSize + InputFontSize, GameInputY - g_Config.m_ClTwitchHeightOffset);
	float Bottom = InputY - InputFontSize;
	const bool Expanded = m_Active || g_Config.m_ClShowChat == 2;
	const float GameHeightLimit = ScoreboardOpen ? 180.0f : (Expanded ? 50.0f : 200.0f);
	const float HistoryHeight = GameInputY - InputFontSize - GameHeightLimit;
	const float Y = std::max(5.0f, Bottom - HistoryHeight);
	const float Height = std::min(ScreenHeight, InputY + InputHeight) - Y;
	const int MaxLines = std::max(1, (int)((Bottom - Y - PaddingY) / FontSize));
	const bool ShowHistory = Expanded;
	const int64_t Now = time_get();
	const ColorRGBA PreviousColor = TextRender()->GetTextColor();
	const ColorRGBA PreviousOutline = TextRender()->GetTextOutlineColor();
	const ColorRGBA NameColor(145.0f / 255.0f, 70.0f / 255.0f, 1.0f, 1.0f);
	const ColorRGBA MessageColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
	const ColorRGBA BackgroundColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClChatBackgroundColor, true));

	const float ScaleX = Graphics()->ScreenWidth() / ScreenWidth;
	const float ScaleY = Graphics()->ScreenHeight() / ScreenHeight;
	Graphics()->ClipEnable((int)(X * ScaleX), (int)(Y * ScaleY), (int)(Width * ScaleX), (int)(Height * ScaleY));
	int Index = (int)m_Messages.size() - 1 - (ShowHistory ? m_Scroll : 0);
	for(; Index >= 0; --Index)
	{
		const CLine &Message = m_Messages[Index];
		const float Age = (Now - Message.m_Time) / (float)time_freq();
		if(!ShowHistory && Age >= 16.0f)
			break;
		const float Blend = ShowHistory ? 1.0f : std::clamp((16.0f - Age) / 2.0f, 0.0f, 1.0f);
		std::string Text = Message.m_Text;
		const bool Action = Text.compare(0, 8, "\x01" "ACTION ") == 0;
		if(Action)
		{
			Text.erase(0, 8);
			if(!Text.empty() && Text.back() == '\x01')
				Text.pop_back();
		}
		for(char &Character : Text)
			if((unsigned char)Character < 32)
				Character = ' ';
		const std::string Prefix = (Action ? "* " : "") + Message.m_Name + (Action ? " " : ": ");

		// Measure and draw with the same cursors, including the normal chat's
		// hanging indent in modern style and full-width wrapping in old style.
		auto Layout = [&](float TextY, bool Render) {
			CTextCursor NameCursor;
			NameCursor.SetPosition(vec2(X + PaddingX / 2.0f, TextY));
			NameCursor.m_FontSize = FontSize;
			NameCursor.m_LineWidth = LineWidth * 0.6f;
			NameCursor.m_Flags = (Render ? TEXTFLAG_RENDER : 0) | TEXTFLAG_STOP_AT_END;
			TextRender()->TextColor(NameColor.WithMultipliedAlpha(Blend));
			TextRender()->TextEx(&NameCursor, Prefix.c_str());
			const float PrefixWidth = NameCursor.m_X - NameCursor.m_StartX;
			CTextCursor TextCursor = NameCursor;
			TextCursor.m_Flags = Render ? TEXTFLAG_RENDER : 0;
			TextCursor.m_MaxLines = MaxLines;
			TextCursor.m_Truncated = false;
			TextCursor.m_LineWidth = LineWidth;
			TextCursor.m_LongestLineWidth = 0.0f;
			if(!g_Config.m_ClChatOld)
			{
				TextCursor.m_StartX = NameCursor.m_X;
				TextCursor.m_LineWidth -= PrefixWidth;
			}
			TextRender()->TextColor(MessageColor.WithMultipliedAlpha(Blend));
			TextRender()->TextEx(&TextCursor, Text.c_str());
			const float TextWidth = g_Config.m_ClChatOld ? std::max(PrefixWidth, TextCursor.m_LongestLineWidth) : PrefixWidth + TextCursor.m_LongestLineWidth;
			return vec2(std::min(Width, TextWidth + PaddingX * 1.5f), TextCursor.Height() + PaddingY);
		};

		const vec2 Size = Layout(0.0f, false);
		Bottom -= Size.y;
		if(Bottom < Y)
			break;
		if(!g_Config.m_ClChatOld)
		{
			const CUIRect Background = {X, Bottom, Size.x, Size.y};
			Background.Draw(BackgroundColor.WithMultipliedAlpha(Blend), IGraphics::CORNER_ALL, GameChat.MessageRounding());
		}
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Blend));
		Layout(Bottom + PaddingY / 2.0f, true);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	if(m_Active)
	{
		CTextCursor InputCursor;
		InputCursor.SetPosition(vec2(X, InputY));
		InputCursor.m_FontSize = InputFontSize;
		InputCursor.m_LineWidth = ScreenWidth - 190.0f;
		TextRender()->TextEx(&InputCursor, "Twitch: ");
		const float InputWidth = std::max(FontSize, InputCursor.m_LineWidth - (InputCursor.m_X - X));
		const CUIRect ClippingRect = {InputCursor.m_X, InputY, InputWidth, InputHeight};
		Graphics()->ClipEnable((int)(ClippingRect.x * ScaleX), (int)(ClippingRect.y * ScaleY), (int)(ClippingRect.w * ScaleX), (int)(ClippingRect.h * ScaleY));
		float ScrollOffset = m_Input.GetScrollOffset();
		float ScrollChange = m_Input.GetScrollOffsetChange();
		const CUIRect InputRect = {InputCursor.m_X, InputY - ScrollOffset, 0.0f, 0.0f};
		m_Input.Activate(EInputPriority::CHAT);
		const STextBoundingBox Bounds = m_Input.Render(&InputRect, InputFontSize, TEXTALIGN_TL, m_Input.WasChanged() || m_Input.WasCursorChanged(), InputWidth, 0.0f);
		const float CaretY = m_Input.GetCaretPosition().y - ScrollChange;
		if(CaretY < InputY)
			ScrollChange -= InputY - CaretY;
		else if(CaretY + InputFontSize > InputY + InputHeight)
			ScrollChange += CaretY + InputFontSize - (InputY + InputHeight);
		Ui()->DoSmoothScrollLogic(&ScrollOffset, &ScrollChange, InputHeight, Bounds.m_H);
		m_Input.SetScrollOffset(ScrollOffset);
		m_Input.SetScrollOffsetChange(ScrollChange);
	}
	Graphics()->ClipDisable();
	TextRender()->TextColor(PreviousColor);
	TextRender()->TextOutlineColor(PreviousOutline);
}
