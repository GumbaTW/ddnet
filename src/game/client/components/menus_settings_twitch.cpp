#include "menus.h"

#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderSettingsTwitch(CUIRect MainView)
{
	auto &Chat = GameClient()->m_TwitchChat;
	CUIRect Row, Label;
	MainView.HSplitTop(30.0f, &Row, &MainView);
	Ui()->DoLabel(&Row, Localize("Twitch chat"), 20.0f, TEXTALIGN_ML);
	auto EditRow = [&](const char *pLabel, CLineInput *pInput) {
		MainView.HSplitTop(25.0f, &Row, &MainView);
		Row.VSplitLeft(160.0f, &Label, &Row);
		Ui()->DoLabel(&Label, pLabel, 14.0f, TEXTALIGN_ML);
		Ui()->DoEditBox(pInput, &Row, 14.0f);
		MainView.HSplitTop(5.0f, nullptr, &MainView);
	};
	static CLineInput s_ChannelInput;
	static CLineInput s_LoginInput;
	s_ChannelInput.SetBuffer(g_Config.m_ClTwitchChannel, sizeof(g_Config.m_ClTwitchChannel));
	s_LoginInput.SetBuffer(g_Config.m_ClTwitchLogin, sizeof(g_Config.m_ClTwitchLogin));
	EditRow(Localize("Channel (lowercase)"), &s_ChannelInput);
	EditRow(Localize("Account login"), &s_LoginInput);
	Chat.m_TokenInput.SetBuffer(g_Config.m_ClTwitchAccessToken, sizeof(g_Config.m_ClTwitchAccessToken));
	Chat.m_TokenInput.SetHidden(true);
	EditRow(Localize("Access token"), &Chat.m_TokenInput);
	MainView.HSplitTop(20.0f, &Row, &MainView);
	Ui()->DoLabel(&Row, Localize("Token needs chat:read and chat:edit permissions. Saved locally with client settings."), 12.0f, TEXTALIGN_ML);
	MainView.HSplitTop(25.0f, &Row, &MainView);
	CUIRect ConnectButton, DisconnectButton;
	Row.VSplitMid(&ConnectButton, &DisconnectButton, 10.0f);
	static CButtonContainer s_ConnectButton, s_DisconnectButton;
	if(DoButton_Menu(&s_ConnectButton, Localize("Connect / reconnect"), 0, &ConnectButton))
		Chat.Connect();
	if(DoButton_Menu(&s_DisconnectButton, Localize("Disconnect"), 0, &DisconnectButton))
		Chat.Disconnect();
	MainView.HSplitTop(25.0f, &Row, &MainView);
	Ui()->DoLabel(&Row, Chat.Status(), 14.0f, TEXTALIGN_ML);
	MainView.HSplitTop(25.0f, &Row, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClTwitchShow, Localize("Show Twitch chat overlay"), g_Config.m_ClTwitchShow, &Row))
		g_Config.m_ClTwitchShow ^= 1;
	MainView.HSplitTop(25.0f, &Row, &MainView);
	Ui()->DoScrollbarOption(&g_Config.m_ClTwitchHeightOffset, &g_Config.m_ClTwitchHeightOffset, &Row, Localize("Height above game chat"), 0, 200);
	MainView.HSplitTop(25.0f, &Row, &MainView);
	Ui()->DoLabel(&Row, Localize("Assign a Twitch chat key in Settings > Controls > Chat."), 14.0f, TEXTALIGN_ML);
	MainView.HSplitTop(25.0f, &Row, &MainView);
	Ui()->DoLabel(&Row, Localize("Enter sends; Escape closes; mouse wheel scrolls while typing."), 14.0f, TEXTALIGN_ML);
}
