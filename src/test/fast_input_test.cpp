#include <game/client/fast_input.h>

#include <gtest/gtest.h>

#include <algorithm>

TEST(FastInput, Aggressive130Keeps26MillisecondOffsetWithTwoTicks)
{
	const float Offset = 130 / 100.0f;
	EXPECT_EQ(FastInput::ExtraTicks(Offset), 2);
	int LargestTick = 0;
	for(int Phase = 0; Phase < 1000; ++Phase)
	{
		int Tick = 1000;
		float Intra = Phase / 1000.0f;
		const float Expected = Intra + Offset;
		FastInput::ApplyOffset(Offset, Tick, Intra);
		EXPECT_GE(Intra, 0.0f);
		EXPECT_LT(Intra, 1.0f);
		EXPECT_NEAR(Tick - 1000 + Intra, Expected, 0.000001f);
		LargestTick = std::max(LargestTick, Tick - 1000);
	}
	EXPECT_EQ(LargestTick, FastInput::ExtraTicks(Offset));
}

TEST(FastInput, BothModesFitTheirPredictionHorizon)
{
	for(int Amount = 0; Amount <= 200; ++Amount)
	{
		for(float Offset : {Amount / 100.0f, Amount / 20.0f})
		{
			for(float Phase : {0.0f, 0.25f, 0.7f, std::nextafter(1.0f, 0.0f)})
			{
				int Tick = 1000;
				float Intra = Phase;
				FastInput::ApplyOffset(Offset, Tick, Intra);
				EXPECT_LE(Tick, 1000 + FastInput::ExtraTicks(Offset));
				EXPECT_GE(Intra, 0.0f);
				EXPECT_LT(Intra, 1.0f);
			}
		}
	}
}

TEST(FastInput, HookAimRefreshesBeforeSendWithoutSubTickAiming)
{
	CNetObj_PlayerInput Sent{}, Previous{}, Input{};
	Input.m_Hook = 1;
	Previous = FastInput::PrepareInput(Input, Previous, Sent, vec2(100, 0), vec2(0, 0), true, false);
	const auto Turned = FastInput::PrepareInput(Input, Previous, Sent, vec2(0, 100), vec2(100, 0), true, false);
	EXPECT_EQ(Turned.m_TargetX, 0);
	EXPECT_EQ(Turned.m_TargetY, 100);
	EXPECT_TRUE(FastInput::HasChanged(Previous, Turned, Sent));
	EXPECT_FALSE(FastInput::HasChanged(Turned, Turned, Sent));
}

TEST(FastInput, SubTickHookUsesTheAimThatWillBeSent)
{
	CNetObj_PlayerInput Sent{}, Previous{}, Input{};
	Input.m_Hook = 1;
	Previous = FastInput::PrepareInput(Input, Previous, Sent, vec2(100, 0), vec2(100, 0), true, true);
	const auto Turned = FastInput::PrepareInput(Input, Previous, Sent, vec2(0, 100), vec2(100, 0), true, true);
	EXPECT_EQ(Turned.m_TargetX, 100);
	EXPECT_EQ(Turned.m_TargetY, 0);
	EXPECT_FALSE(FastInput::HasChanged(Previous, Turned, Sent));
	// Fallback for actions without a sub-tick mouse sample also holds the first aim.
	const auto NoSample = FastInput::PrepareInput(Input, Previous, Sent, vec2(0, 100), vec2(0, 0), true, true);
	EXPECT_EQ(NoSample.m_TargetX, 100);
	EXPECT_EQ(NoSample.m_TargetY, 0);
}

TEST(FastInput, HeldFireAimUpdatesAfterThePressWasSent)
{
	CNetObj_PlayerInput Sent{}, Previous{}, Input{};
	Sent.m_Fire = Input.m_Fire = 1;
	Sent.m_TargetX = 100;
	Previous = Sent;
	const auto Turned = FastInput::PrepareInput(Input, Previous, Sent, vec2(0, 100), vec2(0, 0), true, true);
	EXPECT_EQ(Turned.m_TargetY, 100);
	EXPECT_TRUE(FastInput::HasChanged(Previous, Turned, Sent));
}

TEST(FastInput, UnsentFireTapKeepsSubTickAimAcrossCounterWrap)
{
	CNetObj_PlayerInput Sent{}, Previous{}, Input{};
	Sent.m_Fire = INPUT_STATE_MASK - 1;
	Previous = Sent;
	// Press and release before the next network tick: two counter increments.
	Input.m_Fire = 0;
	const auto Locked = FastInput::PrepareInput(Input, Previous, Sent, vec2(0, 100), vec2(100, 0), true, true);
	EXPECT_EQ(Locked.m_TargetX, 100);
	EXPECT_EQ(Locked.m_TargetY, 0);
	const auto Turned = FastInput::PrepareInput(Input, Locked, Sent, vec2(0, 100), vec2(100, 0), true, false);
	EXPECT_EQ(Turned.m_TargetY, 100);
	EXPECT_TRUE(FastInput::HasChanged(Locked, Turned, Sent));
}

TEST(FastInput, InactiveDummyKeepsItsOwnAim)
{
	CNetObj_PlayerInput Sent{}, Previous{}, Input{};
	Input.m_TargetX = 300;
	Input.m_TargetY = 400;
	Input.m_Hook = 1;
	for(bool SubTickAiming : {false, true})
	{
		const auto Prepared = FastInput::PrepareInput(Input, Previous, Sent, vec2(-100, 0), vec2(-100, 0), false, SubTickAiming);
		EXPECT_EQ(Prepared.m_TargetX, 300);
		EXPECT_EQ(Prepared.m_TargetY, 400);
	}
}

TEST(FastInput, IdleMouseMovementDoesNotRequireReprediction)
{
	CNetObj_PlayerInput Previous{}, Input{}, Sent{};
	Previous.m_TargetX = 100;
	Input.m_TargetY = 100;
	EXPECT_FALSE(FastInput::HasChanged(Previous, Input, Sent));
	Input.m_Jump = 1;
	EXPECT_TRUE(FastInput::HasChanged(Previous, Input, Sent));
}

TEST(FastInput, ZeroAimMatchesActiveInputNormalization)
{
	CNetObj_PlayerInput Empty{};
	const auto Prepared = FastInput::PrepareInput(Empty, Empty, Empty, vec2(0, 0), vec2(0, 0), true, false);
	EXPECT_EQ(Prepared.m_TargetX, 1);
	EXPECT_EQ(Prepared.m_TargetY, 0);
}

TEST(FastInput, LocalDummyUsesTheLocalHorizonWithOthersDisabled)
{
	int aTicks[200]{};
	for(int Tick = 995; Tick <= 1002; ++Tick)
		aTicks[Tick % 200] = Tick;
	for(int ClientId : {3, 10})
	{
		ASSERT_TRUE(FastInput::IsLocalClient(ClientId, 3, 10));
		int Tick = 1000;
		float Intra = 0.8f;
		FastInput::ApplyOffset(1.3f, Tick, Intra);
		EXPECT_TRUE(FastInput::IsValidSample(Tick, 995, 1000 + FastInput::ExtraTicks(1.3f), aTicks));
	}
	EXPECT_FALSE(FastInput::IsLocalClient(20, 3, 10));
	EXPECT_FALSE(FastInput::IsLocalClient(-1, -1, -1));
}

TEST(FastInput, RejectsStaleFutureSampleAfterChangingTheAmount)
{
	int aTicks[200]{};
	aTicks[1001 % 200] = 1001;
	aTicks[1002 % 200] = 802;
	EXPECT_FALSE(FastInput::IsValidSample(1002, 995, 1002, aTicks));
	aTicks[1002 % 200] = 1002;
	EXPECT_TRUE(FastInput::IsValidSample(1002, 995, 1002, aTicks));
	aTicks[1001 % 200] = 801;
	EXPECT_FALSE(FastInput::IsValidSample(1002, 995, 1002, aTicks));
}

TEST(FastInput, ValidatesHistoryWrapAndBounds)
{
	int aTicks[200]{};
	aTicks[199] = 1199;
	aTicks[0] = 1200;
	EXPECT_TRUE(FastInput::IsValidSample(1200, 1195, 1200, aTicks));
	EXPECT_FALSE(FastInput::IsValidSample(1200, 1195, 1199, aTicks));
	EXPECT_FALSE(FastInput::IsValidSample(1200, 1200, 1202, aTicks));
	EXPECT_FALSE(FastInput::IsValidSample(0, -10, 10, aTicks));
	EXPECT_FALSE(FastInput::IsValidSample(-1, -10, 10, aTicks));
}
