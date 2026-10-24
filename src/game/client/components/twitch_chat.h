#ifndef GAME_CLIENT_COMPONENTS_TWITCH_CHAT_H
#define GAME_CLIENT_COMPONENTS_TWITCH_CHAT_H

#include <engine/console.h>
#include <engine/shared/twitch_chat.h>
#include <game/client/component.h>
#include <game/client/lineinput.h>

class CTwitchChat : public CComponent
{
	CTwitchChatConnection m_Connection;
	CLineInputBuffered<451> m_Input;
	struct CLine : CTwitchMessage
	{
		int64_t m_Time;
	};
	std::deque<CLine> m_Messages;
	std::string m_Status = "Disconnected";
	bool m_Active = false;
	int m_Scroll = 0;
	static void ConChat(IConsole::IResult *pResult, void *pUserData);
	void CloseInput();

public:
	CLineInput m_TokenInput;
	CTwitchChat();
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnShutdown() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnUpdate() override;
	void OnRender() override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override { return m_Active; }
	bool IsActive() const { return m_Active; }
	bool IsConnected() const { return m_Connection.IsConnected(); }
	const char *Status() const { return m_Status.c_str(); }
	void Connect();
	void Disconnect();
};

#endif
