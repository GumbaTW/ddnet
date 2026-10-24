#ifndef ENGINE_SHARED_TWITCH_CHAT_H
#define ENGINE_SHARED_TWITCH_CHAT_H

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct CTwitchMessage
{
	std::string m_Command;
	std::string m_Name;
	std::string m_Text;
	std::string m_Id;
	std::string m_UserId;
	std::string m_TargetId;
};

// Parse one complete IRC line (without CRLF), including IRCv3 tags.
bool ParseTwitchMessage(const std::string &Line, CTwitchMessage &Message);
bool IsTwitchLogin(const std::string &Login);

class CTwitchChatConnection
{
	std::atomic<bool> m_Stop{false};
	std::atomic<bool> m_Connected{false};
	std::thread m_Thread;
	std::mutex m_Mutex;
	std::deque<CTwitchMessage> m_Incoming;
	std::deque<std::string> m_Outgoing;
	void Push(CTwitchMessage Message);
	void Run(std::string Login, std::string Channel, std::string Token);

public:
	~CTwitchChatConnection();
	void Connect(const std::string &Login, const std::string &Channel, const std::string &Token);
	void Disconnect();
	bool IsConnected() const { return m_Connected; }
	bool Send(const std::string &Text);
	std::vector<CTwitchMessage> Poll();
};

#endif
