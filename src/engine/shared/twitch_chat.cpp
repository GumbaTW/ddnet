#include "twitch_chat.h"

#include <algorithm>
#include <chrono>
#include <utility>

#if !defined(CONF_PLATFORM_EMSCRIPTEN)
#include <curl/curl.h>
#endif

namespace
{
std::string UnescapeTag(const std::string &Value)
{
	std::string Result;
	for(size_t i = 0; i < Value.size(); ++i)
	{
		if(Value[i] == '\\')
		{
			if(++i == Value.size())
				break;
			switch(Value[i])
			{
			case 's':
				Result += ' ';
				break;
			case ':':
				Result += ';';
				break;
			case 'r':
			case 'n':
				Result += ' ';
				break;
			default:
				Result += Value[i];
				break;
			}
		}
		else
			Result += Value[i];
	}
	return Result;
}
}

bool IsTwitchLogin(const std::string &Login)
{
	return !Login.empty() && Login.size() <= 25 && std::all_of(Login.begin(), Login.end(), [](unsigned char Character) {
		return (Character >= 'a' && Character <= 'z') || (Character >= '0' && Character <= '9') || Character == '_';
	});
}

bool ParseTwitchMessage(const std::string &Line, CTwitchMessage &Message)
{
	Message = {};
	if(Line.empty() || Line.size() > 8192 || Line.find_first_of("\r\n") != std::string::npos || Line.find('\0') != std::string::npos)
		return false;
	size_t Pos = 0;
	if(Line[0] == '@')
	{
		const size_t End = Line.find(' ');
		if(End == std::string::npos)
			return false;
		for(size_t Start = 1; Start < End;)
		{
			size_t Next = Line.find(';', Start);
			if(Next == std::string::npos || Next > End)
				Next = End;
			const size_t Equal = Line.find('=', Start);
			if(Equal < Next)
			{
				const std::string Key = Line.substr(Start, Equal - Start);
				const std::string Value = UnescapeTag(Line.substr(Equal + 1, Next - Equal - 1));
				if(Key == "display-name")
					Message.m_Name = Value;
				else if(Key == "id")
					Message.m_Id = Value;
				else if(Key == "user-id" || Key == "target-user-id")
					Message.m_UserId = Value;
				else if(Key == "target-msg-id")
					Message.m_TargetId = Value;
			}
			Start = Next + 1;
		}
		Pos = End + 1;
	}
	if(Pos < Line.size() && Line[Pos] == ':')
	{
		const size_t End = Line.find(' ', Pos);
		if(End == std::string::npos)
			return false;
		if(Message.m_Name.empty())
		{
			const size_t Bang = Line.find('!', Pos);
			Message.m_Name = Line.substr(Pos + 1, std::min(Bang, End) - Pos - 1);
		}
		Pos = End + 1;
	}
	const size_t End = Line.find(' ', Pos);
	Message.m_Command = Line.substr(Pos, End == std::string::npos ? End : End - Pos);
	const size_t Text = Line.find(" :", Pos);
	if(Text != std::string::npos)
		Message.m_Text = Line.substr(Text + 2);
	return !Message.m_Command.empty();
}

CTwitchChatConnection::~CTwitchChatConnection()
{
	Disconnect();
}

void CTwitchChatConnection::Push(CTwitchMessage Message)
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	if(m_Incoming.size() == 200)
		m_Incoming.pop_front();
	m_Incoming.push_back(std::move(Message));
}

void CTwitchChatConnection::Connect(const std::string &Login, const std::string &Channel, const std::string &Token)
{
	Disconnect();
	if(!IsTwitchLogin(Login) || !IsTwitchLogin(Channel) || Token.empty() || Token.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos)
	{
		Push({"STATUS", {}, "Enter a lowercase login, channel, and valid access token."});
		return;
	}
	m_Stop = false;
	m_Thread = std::thread(&CTwitchChatConnection::Run, this, Login, Channel, Token);
}

void CTwitchChatConnection::Disconnect()
{
	m_Stop = true;
	if(m_Thread.joinable())
		m_Thread.join();
	m_Connected = false;
	std::lock_guard<std::mutex> Lock(m_Mutex);
	m_Outgoing.clear();
	m_Incoming.clear();
}

bool CTwitchChatConnection::Send(const std::string &Text)
{
	// A complete PRIVMSG must fit the IRC 512-byte limit, including its channel.
	if(!m_Connected || Text.empty() || Text.size() > 450 || Text.find_first_of("\r\n") != std::string::npos || Text.find('\0') != std::string::npos)
		return false;
	std::lock_guard<std::mutex> Lock(m_Mutex);
	if(m_Outgoing.size() >= 5)
		return false;
	m_Outgoing.push_back(Text);
	return true;
}

std::vector<CTwitchMessage> CTwitchChatConnection::Poll()
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	std::vector<CTwitchMessage> Result(m_Incoming.begin(), m_Incoming.end());
	m_Incoming.clear();
	return Result;
}

void CTwitchChatConnection::Run(std::string Login, std::string Channel, std::string Token)
{
#if defined(CONF_PLATFORM_EMSCRIPTEN)
	Push({"STATUS", {}, "Twitch chat is not available in web builds."});
#else
	Push({"STATUS", {}, "Connecting to Twitch..."});
	CURL *pCurl = curl_easy_init();
	if(!pCurl)
	{
		Push({"STATUS", {}, "Could not initialize Twitch connection."});
		return;
	}
	curl_easy_setopt(pCurl, CURLOPT_URL, "https://irc.chat.twitch.tv:6697");
	curl_easy_setopt(pCurl, CURLOPT_CONNECT_ONLY, 1L);
	curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
	curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pCurl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(pCurl, CURLOPT_XFERINFOFUNCTION, +[](void *pData, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
		return static_cast<CTwitchChatConnection *>(pData)->m_Stop ? 1 : 0;
	});
	curl_easy_setopt(pCurl, CURLOPT_XFERINFODATA, this);
	// The multi interface lets Disconnect interrupt connection setup promptly.
	CURLM *pMulti = curl_multi_init();
	if(!pMulti)
	{
		curl_easy_cleanup(pCurl);
		Push({"STATUS", {}, "Could not initialize Twitch connection."});
		return;
	}
	curl_multi_add_handle(pMulti, pCurl);
	int Running = 1;
	CURLMcode MultiResult = CURLM_OK;
	while(Running && !m_Stop && MultiResult == CURLM_OK)
	{
		MultiResult = curl_multi_perform(pMulti, &Running);
		if(Running && MultiResult == CURLM_OK)
			MultiResult = curl_multi_wait(pMulti, nullptr, 0, 100, nullptr);
	}
	int MessagesLeft = 0;
	CURLMsg *pResult = curl_multi_info_read(pMulti, &MessagesLeft);
	const bool Connected = !m_Stop && MultiResult == CURLM_OK && pResult && pResult->msg == CURLMSG_DONE && pResult->data.result == CURLE_OK;
	// Keep the easy handle in the multi handle: removing it closes a
	// CURLOPT_CONNECT_ONLY connection before curl_easy_send/recv can use it.
	if(!Connected)
	{
		curl_multi_remove_handle(pMulti, pCurl);
		curl_multi_cleanup(pMulti);
		curl_easy_cleanup(pCurl);
		Push({"STATUS", {}, "Twitch connection failed. Check your network and reconnect."});
		return;
	}
	auto SendRaw = [&](const std::string &Line) {
		size_t Offset = 0;
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while(Offset < Line.size() && !m_Stop)
		{
			size_t Sent = 0;
			const CURLcode Result = curl_easy_send(pCurl, Line.data() + Offset, Line.size() - Offset, &Sent);
			if(Result != CURLE_OK && Result != CURLE_AGAIN)
				return false;
			Offset += Sent;
			if(std::chrono::steady_clock::now() > Deadline)
				return false;
			if(!Sent)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return Offset == Line.size();
	};
	bool Healthy = SendRaw("PASS oauth:" + Token + "\r\nNICK " + Login + "\r\nCAP REQ :twitch.tv/tags twitch.tv/commands\r\n");
	Token.assign(Token.size(), '\0');
	std::string Buffer;
	std::string LastNotice;
	std::string DisplayName = Login;
	std::string UserId;
	auto LastReceive = std::chrono::steady_clock::now();
	auto LastSend = LastReceive - std::chrono::seconds(2);
	while(Healthy && !m_Stop)
	{
		char aBuffer[4096];
		size_t Received = 0;
		const CURLcode Result = curl_easy_recv(pCurl, aBuffer, sizeof(aBuffer), &Received);
		if((Result != CURLE_OK && Result != CURLE_AGAIN) || (Result == CURLE_OK && Received == 0))
			break;
		if(Received)
		{
			LastReceive = std::chrono::steady_clock::now();
			Buffer.append(aBuffer, Received);
			size_t End;
			while((End = Buffer.find("\r\n")) != std::string::npos)
			{
				CTwitchMessage Message;
				const bool Valid = ParseTwitchMessage(Buffer.substr(0, End), Message);
				Buffer.erase(0, End + 2);
				if(!Valid)
					continue;
				if(Message.m_Command == "PING")
					Healthy = SendRaw("PONG :" + Message.m_Text + "\r\n");
				else if(Message.m_Command == "001")
					Healthy = SendRaw("JOIN #" + Channel + "\r\n");
				else if(Message.m_Command == "GLOBALUSERSTATE" || Message.m_Command == "USERSTATE")
				{
					if(!Message.m_Name.empty() && Message.m_Name != "tmi.twitch.tv")
						DisplayName = Message.m_Name;
					if(!Message.m_UserId.empty())
						UserId = Message.m_UserId;
				}
				else if(Message.m_Command == "ROOMSTATE")
				{
					m_Connected = true;
					Push({"STATUS", {}, "Connected to #" + Channel});
				}
				else if(Message.m_Command == "RECONNECT")
					Healthy = false;
				else if(Message.m_Command == "PRIVMSG" || Message.m_Command == "NOTICE" || Message.m_Command == "CLEARCHAT" || Message.m_Command == "CLEARMSG")
				{
					if(Message.m_Command == "NOTICE")
						LastNotice = Message.m_Text;
					Push(std::move(Message));
				}
			}
			if(Buffer.size() > 8192)
				break;
		}
		const auto Now = std::chrono::steady_clock::now();
		if(Now - LastReceive > std::chrono::minutes(6))
			break;
		// Stay below the ordinary account limit of 20 messages per 30 seconds.
		if(m_Connected && Now - LastSend >= std::chrono::milliseconds(1600))
		{
			std::string Text;
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				if(!m_Outgoing.empty())
				{
					Text = std::move(m_Outgoing.front());
					m_Outgoing.pop_front();
				}
			}
			if(!Text.empty())
			{
				Healthy = SendRaw("PRIVMSG #" + Channel + " :" + Text + "\r\n");
				if(Healthy)
					Push({"PRIVMSG", DisplayName, Text, {}, UserId});
				LastSend = Now;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	m_Connected = false;
	curl_multi_remove_handle(pMulti, pCurl);
	curl_multi_cleanup(pMulti);
	curl_easy_cleanup(pCurl);
	if(!m_Stop)
		Push({"STATUS", {}, LastNotice.empty() ? "Disconnected from Twitch. Reconnect in Twitch settings." : "Disconnected: " + LastNotice});
#endif
}
