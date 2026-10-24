#include <engine/shared/twitch_chat.h>

#include <gtest/gtest.h>

TEST(TwitchChat, TaggedMessage)
{
	CTwitchMessage Message;
	ASSERT_TRUE(ParseTwitchMessage("@display-name=Viewer;id=abc;user-id=42 :viewer!viewer@viewer.tmi.twitch.tv PRIVMSG #streamer :hello :world", Message));
	EXPECT_EQ(Message.m_Command, "PRIVMSG");
	EXPECT_EQ(Message.m_Name, "Viewer");
	EXPECT_EQ(Message.m_Text, "hello :world");
	EXPECT_EQ(Message.m_Id, "abc");
	EXPECT_EQ(Message.m_UserId, "42");
}

TEST(TwitchChat, UntaggedAndPing)
{
	CTwitchMessage Message;
	ASSERT_TRUE(ParseTwitchMessage(":viewer!viewer@host PRIVMSG #streamer :hello", Message));
	EXPECT_EQ(Message.m_Name, "viewer");
	ASSERT_TRUE(ParseTwitchMessage("PING :tmi.twitch.tv", Message));
	EXPECT_EQ(Message.m_Command, "PING");
	EXPECT_EQ(Message.m_Text, "tmi.twitch.tv");
	EXPECT_TRUE(Message.m_Name.empty());
	EXPECT_TRUE(Message.m_Id.empty());
}

TEST(TwitchChat, EscapedTags)
{
	CTwitchMessage Message;
	ASSERT_TRUE(ParseTwitchMessage("@display-name=One\\sTwo\\:Three\\\\Four\\r\\n :user!user@host PRIVMSG #channel :hello", Message));
	EXPECT_EQ(Message.m_Name, "One Two;Three\\Four  ");
}

TEST(TwitchChat, Moderation)
{
	CTwitchMessage Message;
	ASSERT_TRUE(ParseTwitchMessage("@target-msg-id=deleted :tmi.twitch.tv CLEARMSG #streamer :removed", Message));
	EXPECT_EQ(Message.m_Command, "CLEARMSG");
	EXPECT_EQ(Message.m_TargetId, "deleted");
	ASSERT_TRUE(ParseTwitchMessage("@target-user-id=42 :tmi.twitch.tv CLEARCHAT #streamer :viewer", Message));
	EXPECT_EQ(Message.m_UserId, "42");
	ASSERT_TRUE(ParseTwitchMessage(":tmi.twitch.tv CLEARCHAT #streamer", Message));
	EXPECT_TRUE(Message.m_UserId.empty());
}

TEST(TwitchChat, RejectMalformedLines)
{
	CTwitchMessage Message;
	EXPECT_FALSE(ParseTwitchMessage("", Message));
	EXPECT_FALSE(ParseTwitchMessage("@tag=value", Message));
	EXPECT_FALSE(ParseTwitchMessage(":prefix", Message));
	EXPECT_FALSE(ParseTwitchMessage("PING :host\r\nPRIVMSG #channel :injected", Message));
	EXPECT_FALSE(ParseTwitchMessage(std::string("PING :a\0b", 9), Message));
	EXPECT_FALSE(ParseTwitchMessage(std::string(8193, 'a'), Message));
}

TEST(TwitchChat, RejectInvalidConnectionAndDisconnectedSend)
{
	EXPECT_TRUE(IsTwitchLogin("streamer_123"));
	EXPECT_FALSE(IsTwitchLogin(""));
	EXPECT_FALSE(IsTwitchLogin("#channel"));
	EXPECT_FALSE(IsTwitchLogin("UPPERCASE"));
	EXPECT_FALSE(IsTwitchLogin("channel\r\nJOIN #other"));
	EXPECT_FALSE(IsTwitchLogin(std::string(26, 'a')));
	CTwitchChatConnection Connection;
	Connection.Connect("streamer", "channel", "token\r\nJOIN #other");
	EXPECT_FALSE(Connection.IsConnected());
	const auto Messages = Connection.Poll();
	ASSERT_EQ(Messages.size(), 1u);
	EXPECT_EQ(Messages[0].m_Command, "STATUS");
	EXPECT_FALSE(Connection.Send("hello"));
	Connection.Disconnect();
	Connection.Disconnect();
	EXPECT_TRUE(Connection.Poll().empty());
}
