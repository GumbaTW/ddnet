#include <base/net.h>
#include <base/secure.h>

#include <engine/shared/config.h>
#include <engine/shared/network.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>

static int UnpackUncompressedPacket(int Size, CNetPacketConstruct *pPacket, bool AllowDecompression = true)
{
	unsigned char aBuffer[NET_MAX_PACKETSIZE] = {};
	aBuffer[0] = 0; // no flags, ack 0
	aBuffer[1] = 0; // ack 0
	aBuffer[2] = 1; // one chunk
	bool Sixup = false;
	return CNetBase::UnpackPacket(aBuffer, Size, pPacket, Sixup, AllowDecompression);
}

static int UnpackCompressedPacket(CNetPacketConstruct *pPacket, bool AllowDecompression, bool *pDecompressed = nullptr, int PayloadSize = 64)
{
	CNetBase::Init();

	const unsigned char aPayload[4 * NET_MAX_PACKETSIZE] = {};
	unsigned char aBuffer[NET_MAX_PACKETSIZE] = {};
	aBuffer[0] = (NET_PACKETFLAG_COMPRESSION << 2) & 0xfc; // compression, ack 0
	aBuffer[1] = 0; // ack 0
	aBuffer[2] = 1; // one chunk
	const int CompressedSize = CNetBase::Compress(aPayload, PayloadSize,
		&aBuffer[NET_PACKETHEADERSIZE], NET_MAX_PACKETSIZE - NET_PACKETHEADERSIZE);
	bool Sixup = false;
	return CNetBase::UnpackPacket(aBuffer, NET_PACKETHEADERSIZE + CompressedSize, pPacket, Sixup, AllowDecompression, nullptr, nullptr, pDecompressed);
}

TEST(Network, UnpackMaximumUncompressedPacket)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_PACKETHEADERSIZE + (int)sizeof(Packet.m_aChunkData), &Packet), 0);
	EXPECT_EQ(Packet.m_DataSize, (int)sizeof(Packet.m_aChunkData));
	EXPECT_EQ(UnpackUncompressedPacket(NET_MAX_PACKETSIZE, &Packet), 0);
}

TEST(Network, UnpackOversizedUncompressedPacket)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_PACKETHEADERSIZE + (int)sizeof(Packet.m_aChunkData) + 1, &Packet), -1);
}

TEST(Network, UnpackCompressedPacket)
{
	CNetPacketConstruct Packet;
	bool Decompressed = false;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, true, &Decompressed), 0);
	EXPECT_EQ(Packet.m_DataSize, 64);
	EXPECT_TRUE(Decompressed);
}

TEST(Network, UnpackCompressedPacketWithoutDecompression)
{
	CNetPacketConstruct Packet;
	bool Decompressed = true;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, false, &Decompressed), -1);
	EXPECT_FALSE(Decompressed);
}

TEST(Network, UnpackOversizedCompressedPacket)
{
	// Decoding data that does not fit the packet buffer costs the same, so it counts
	// as a decompression all the same.
	CNetPacketConstruct Packet;
	bool Decompressed = false;
	EXPECT_EQ(UnpackCompressedPacket(&Packet, true, &Decompressed, (int)sizeof(Packet.m_aChunkData) + 1), -1);
	EXPECT_TRUE(Decompressed);
}

TEST(Network, UnpackUncompressedPacketWithoutDecompression)
{
	CNetPacketConstruct Packet;
	EXPECT_EQ(UnpackUncompressedPacket(NET_MAX_PACKETSIZE, &Packet, false), 0);
}

TEST(Network, UnpackChunks)
{
	// Three non-vital chunks of different sizes, each filled with its own byte.
	constexpr int NumChunks = 3;
	const int aSizes[NumChunks] = {1, 100, 7};

	CNetPacketConstruct Packet = {};
	Packet.m_NumChunks = NumChunks;
	unsigned char *pData = Packet.m_aChunkData;
	for(int i = 0; i < NumChunks; i++)
	{
		CNetChunkHeader Header;
		Header.m_Flags = 0;
		Header.m_Size = aSizes[i];
		Header.m_Sequence = 0;
		pData = Header.Pack(pData);
		memset(pData, i, aSizes[i]);
		pData += aSizes[i];
	}
	Packet.m_DataSize = (int)(pData - Packet.m_aChunkData);

	CNetConnection Connection;
	Connection.m_Sixup = false;

	CPacketChunkUnpacker Unpacker;
	Unpacker.FeedPacket(NETADDR{}, Packet, &Connection, 0);
	for(int i = 0; i < NumChunks; i++)
	{
		CNetChunk Chunk;
		ASSERT_TRUE(Unpacker.UnpackNextChunk(&Chunk));
		EXPECT_EQ(Chunk.m_DataSize, aSizes[i]);
		for(int j = 0; j < Chunk.m_DataSize; j++)
		{
			EXPECT_EQ(((const unsigned char *)Chunk.m_pData)[j], i);
		}
	}
	CNetChunk Chunk;
	EXPECT_FALSE(Unpacker.UnpackNextChunk(&Chunk));
}

// Exercise admission through real UDP packets, including the shared 0.6/0.7
// limit and IPv4/IPv6 address comparisons.
class CNetworkConnectionLimitTest : public ::testing::TestWithParam<std::tuple<bool, bool>>
{
protected:
	struct CClient
	{
		NETADDR m_Addr;
		NETSOCKET m_Socket;
	};

	std::unique_ptr<CNetServer> m_pServer = std::make_unique<CNetServer>();
	std::vector<CClient> m_vClients;
	int m_LastAccepted = -1;
	int m_OldConnlimit;
	int m_OldSixup;
	int m_OldTimeoutRecovery;
	int m_OldTimeout;
	int m_OldProtection;

	static int NewClient(int ClientId, void *pUser, bool Sixup)
	{
		static_cast<CNetworkConnectionLimitTest *>(pUser)->m_LastAccepted = ClientId;
		return 0;
	}

	static int RejoinClient(int ClientId, void *pUser, bool Sixup, bool VanillaAuth)
	{
		static_cast<CNetworkConnectionLimitTest *>(pUser)->m_LastAccepted = ClientId;
		return 0;
	}

	void SetUp() override
	{
		CNetBase::Init();
		m_OldConnlimit = g_Config.m_SvConnlimit;
		m_OldSixup = g_Config.m_SvSixup;
		m_OldTimeoutRecovery = g_Config.m_SvTimeoutRecovery;
		g_Config.m_SvTimeoutRecovery = 60;
		m_OldTimeout = g_Config.m_ConnTimeout;
		m_OldProtection = g_Config.m_ConnTimeoutProtection;
		g_Config.m_ConnTimeout = 10;
		g_Config.m_ConnTimeoutProtection = 1000;
		g_Config.m_SvConnlimit = 100;
		g_Config.m_SvSixup = 1;
		NETADDR Addr;
		ASSERT_EQ(net_addr_from_str(&Addr, std::get<0>(GetParam()) ? "[::1]" : "127.0.0.1"), 0);
		bool Opened = false;
		for(int i = 0; i < 32 && !Opened; i++)
		{
			Addr.port = 1024 + secure_rand_below(65535 - 1024);
			Opened = m_pServer->Open(Addr, nullptr, 8, 1);
		}
		ASSERT_TRUE(Opened);
		m_pServer->SetCallbacks(NewClient, nullptr, RejoinClient, nullptr, this);
	}

	void TearDown() override
	{
		for(const auto &Client : m_vClients)
			net_udp_close(Client.m_Socket);
		m_pServer->Close();
		g_Config.m_SvConnlimit = m_OldConnlimit;
		g_Config.m_SvSixup = m_OldSixup;
		g_Config.m_SvTimeoutRecovery = m_OldTimeoutRecovery;
		g_Config.m_ConnTimeout = m_OldTimeout;
		g_Config.m_ConnTimeoutProtection = m_OldProtection;
	}

	void AddClient()
	{
		NETADDR Addr = m_pServer->Address();
		NETSOCKET Socket = nullptr;
		for(int i = 0; i < 32 && !Socket; i++)
		{
			Addr.port = 1024 + secure_rand_below(65535 - 1024);
			Socket = net_udp_create(Addr);
		}
		ASSERT_NE(Socket, nullptr);
		m_vClients.push_back({Addr, Socket});
	}

	int Connect(int Index)
	{
		const CClient &Client = m_vClients[Index];
		NETADDR ServerAddr = m_pServer->Address();
		const SECURITY_TOKEN Token = m_pServer->GetToken(Client.m_Addr);
		if(std::get<1>(GetParam()))
		{
			CNetBase::SendControlMsgWithToken7(Client.m_Socket, &ServerAddr, Token, 0, NET_CTRLMSG_CONNECT, 123, false);
		}
		else
		{
			unsigned char aToken[sizeof(Token)];
			WriteSecurityToken(aToken, Token);
			CNetBase::SendControlMsg(Client.m_Socket, &ServerAddr, 0, NET_CTRLMSG_ACCEPT, aToken, sizeof(aToken), NET_SECURITY_TOKEN_UNSUPPORTED);
		}
		m_LastAccepted = -1;
		EXPECT_EQ(net_socket_read_wait(m_pServer->Socket(), std::chrono::seconds(1)), 1);
		CNetChunk Chunk;
		SECURITY_TOKEN ResponseToken;
		while(m_pServer->Recv(&Chunk, &ResponseToken))
		{
		}
		return m_LastAccepted;
	}

	void TimeoutConnections()
	{
		// Force the regular timeout transition without sleeping. The network
		// configuration parser never allows a negative timeout in production.
		const int OldTimeout = g_Config.m_ConnTimeout;
		const int OldProtection = g_Config.m_ConnTimeoutProtection;
		g_Config.m_ConnTimeout = -1;
		g_Config.m_ConnTimeoutProtection = 1000;
		m_pServer->Update();
		g_Config.m_ConnTimeout = OldTimeout;
		g_Config.m_ConnTimeoutProtection = OldProtection;
	}
};

TEST_P(CNetworkConnectionLimitTest, RejectsSecondActiveConnection)
{
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(0), 0);
	m_pServer->IgnoreTimeouts(0);
	ASSERT_NO_FATAL_FAILURE(AddClient());
	EXPECT_EQ(Connect(1), -1);
	EXPECT_FALSE(m_pServer->HasErrored(0));
	// Rejoining the existing active connection does not require another slot.
	EXPECT_EQ(Connect(0), 0);
}

TEST_P(CNetworkConnectionLimitTest, AllowsTimeoutRecoveryAndKeepsLimit)
{
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(0), 0);
	m_pServer->IgnoreTimeouts(0);
	TimeoutConnections();
	ASSERT_TRUE(m_pServer->HasErrored(0));

	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(1), 1);
	// Admission must preserve the old tee for authentication by /timeout.
	EXPECT_TRUE(m_pServer->HasErrored(0));
	ASSERT_NO_FATAL_FAILURE(AddClient());
	EXPECT_EQ(Connect(2), -1);

	// This is the transport transfer performed after checking the timeout code.
	ASSERT_TRUE(m_pServer->IsTimeoutRecovery(1));
	ASSERT_TRUE(m_pServer->ResumeOldConnection(0, 1));
	EXPECT_FALSE(m_pServer->IsTimeoutRecovery(0));
	EXPECT_FALSE(m_pServer->IsTimeoutRecovery(1));
	EXPECT_FALSE(m_pServer->HasErrored(0));
	EXPECT_EQ(*m_pServer->ClientAddr(0), m_vClients[1].m_Addr);
	EXPECT_EQ(Connect(2), -1);

	// A late connection request from the old address cannot revive a second tee.
	EXPECT_EQ(Connect(0), -1);
}

TEST_P(CNetworkConnectionLimitTest, MultipleProtectedTeesAllowOnlyOneActiveConnection)
{
	m_pServer->SetMaxClientsPerIp(4);
	for(int i = 0; i < 4; i++)
	{
		ASSERT_NO_FATAL_FAILURE(AddClient());
		ASSERT_EQ(Connect(i), i);
		m_pServer->IgnoreTimeouts(i);
	}
	ASSERT_NO_FATAL_FAILURE(AddClient());
	EXPECT_EQ(Connect(4), -1);
	TimeoutConnections();
	for(int i = 0; i < 4; i++)
		ASSERT_TRUE(m_pServer->HasErrored(i));

	m_pServer->SetMaxClientsPerIp(1);
	EXPECT_EQ(Connect(4), 4);
	ASSERT_NO_FATAL_FAILURE(AddClient());
	EXPECT_EQ(Connect(5), -1);
}

TEST_P(CNetworkConnectionLimitTest, UnprotectedTimeoutReleasesSlot)
{
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(0), 0);
	TimeoutConnections();
	ASSERT_NO_FATAL_FAILURE(AddClient());
	EXPECT_EQ(Connect(1), 0);
}

TEST_P(CNetworkConnectionLimitTest, RecoveryTimeoutsCannotAccumulateProtectedTees)
{
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(0), 0);
	m_pServer->IgnoreTimeouts(0);
	TimeoutConnections();
	ASSERT_TRUE(m_pServer->HasErrored(0));

	for(int i = 1; i <= 10; i++)
	{
		ASSERT_NO_FATAL_FAILURE(AddClient());
		ASSERT_EQ(Connect(i), 1);
		ASSERT_TRUE(m_pServer->IsTimeoutRecovery(1));
		// Even direct attempts to protect the temporary connection are ignored.
		m_pServer->IgnoreTimeouts(1);
		TimeoutConnections();
		EXPECT_TRUE(m_pServer->HasErrored(0));
		EXPECT_FALSE(m_pServer->IsTimeoutRecovery(1));
	}

	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(11), 1);
	EXPECT_TRUE(m_pServer->ResumeOldConnection(0, 1));
}

TEST_P(CNetworkConnectionLimitTest, OnlyOneRecoveryConnectionEvenWithHigherLimit)
{
	m_pServer->SetMaxClientsPerIp(4);
	for(int i = 0; i < 4; i++)
	{
		ASSERT_NO_FATAL_FAILURE(AddClient());
		ASSERT_EQ(Connect(i), i);
		m_pServer->IgnoreTimeouts(i);
	}
	TimeoutConnections();
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(4), 4);
	EXPECT_TRUE(m_pServer->IsTimeoutRecovery(4));
	ASSERT_NO_FATAL_FAILURE(AddClient());
	EXPECT_EQ(Connect(5), -1);
	ASSERT_TRUE(m_pServer->ResumeOldConnection(0, 4));
	EXPECT_EQ(Connect(5), 4);
	EXPECT_TRUE(m_pServer->IsTimeoutRecovery(4));
}

TEST_P(CNetworkConnectionLimitTest, RecoveryCannotClaimAnotherRecoveryConnection)
{
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(0), 0);
	m_pServer->IgnoreTimeouts(0);
	TimeoutConnections();
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(1), 1);
	EXPECT_FALSE(m_pServer->ResumeOldConnection(1, 0));
	EXPECT_TRUE(m_pServer->IsTimeoutRecovery(1));
	EXPECT_TRUE(m_pServer->HasErrored(0));
}

TEST_P(CNetworkConnectionLimitTest, RejoiningCannotExtendRecoveryDeadline)
{
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(0), 0);
	m_pServer->IgnoreTimeouts(0);
	TimeoutConnections();
	g_Config.m_SvTimeoutRecovery = 1;
	ASSERT_NO_FATAL_FAILURE(AddClient());
	ASSERT_EQ(Connect(1), 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(600));
	ASSERT_EQ(Connect(1), 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(600));
	m_pServer->Update();
	EXPECT_FALSE(m_pServer->IsTimeoutRecovery(1));
	EXPECT_TRUE(m_pServer->HasErrored(0));
	ASSERT_NO_FATAL_FAILURE(AddClient());
	EXPECT_EQ(Connect(2), 1);
}

INSTANTIATE_TEST_SUITE_P(Network, CNetworkConnectionLimitTest,
	::testing::Combine(::testing::Bool(), ::testing::Bool()));
