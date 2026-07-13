#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "stalkermp/core/FrameDispatcher.h"

using namespace stalkermp::core;

namespace
{
    class RecordingTickable final : public ITickable
    {
    public:
        RecordingTickable(std::string name, std::vector<std::string>& log)
            : m_name(std::move(name))
            , m_log(log)
        {
        }

        void Tick(const double deltaSeconds) override
        {
            m_log.push_back(m_name);
            lastDelta = deltaSeconds;
            ++tickCount;
        }

        double lastDelta = -1.0;
        int tickCount = 0;

    private:
        std::string m_name;
        std::vector<std::string>& m_log;
    };
} // namespace

TEST(FrameDispatcher, DispatchesInAscendingOrderKeyOrder)
{
    std::vector<std::string> log;
    RecordingTickable world("world", log);
    RecordingTickable plugins("plugins", log);
    RecordingTickable bubble("bubble", log);

    FrameDispatcher dispatcher;
    // Deliberately registered out of order.
    ASSERT_TRUE(dispatcher.Subscribe(plugins, tick_order::kPlugins, "Plugins").HasValue());
    ASSERT_TRUE(dispatcher.Subscribe(world, tick_order::kWorld, "World").HasValue());
    ASSERT_TRUE(dispatcher.Subscribe(bubble, tick_order::kBubble, "Bubble").HasValue());

    dispatcher.Dispatch(0.016);

    ASSERT_EQ(log.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(log[0], std::string("world"));
    EXPECT_EQ(log[1], std::string("bubble"));
    EXPECT_EQ(log[2], std::string("plugins"));
}

TEST(FrameDispatcher, ForwardsDeltaToEverySubscriber)
{
    std::vector<std::string> log;
    RecordingTickable a("a", log);
    RecordingTickable b("b", log);

    FrameDispatcher dispatcher;
    ASSERT_TRUE(dispatcher.Subscribe(a, 1, "A").HasValue());
    ASSERT_TRUE(dispatcher.Subscribe(b, 2, "B").HasValue());

    dispatcher.Dispatch(0.033);
    EXPECT_EQ(a.lastDelta, 0.033);
    EXPECT_EQ(b.lastDelta, 0.033);

    dispatcher.Dispatch(0.0); // Zero delta is legal (paused engine).
    EXPECT_EQ(a.lastDelta, 0.0);
    EXPECT_EQ(a.tickCount, 2);
}

TEST(FrameDispatcher, RejectsDuplicateOrderKey)
{
    std::vector<std::string> log;
    RecordingTickable a("a", log);
    RecordingTickable b("b", log);

    FrameDispatcher dispatcher;
    ASSERT_TRUE(dispatcher.Subscribe(a, tick_order::kWorld, "A").HasValue());

    const auto duplicate = dispatcher.Subscribe(b, tick_order::kWorld, "B");
    ASSERT_FALSE(duplicate.HasValue());
    EXPECT_TRUE(duplicate.GetError().Code() == ErrorCode::AlreadyExists);
}

TEST(FrameDispatcher, RejectsDuplicateSubscriber)
{
    std::vector<std::string> log;
    RecordingTickable a("a", log);

    FrameDispatcher dispatcher;
    ASSERT_TRUE(dispatcher.Subscribe(a, 1, "A").HasValue());

    const auto duplicate = dispatcher.Subscribe(a, 2, "A-again");
    ASSERT_FALSE(duplicate.HasValue());
    EXPECT_TRUE(duplicate.GetError().Code() == ErrorCode::AlreadyExists);
}

TEST(FrameDispatcher, UnsubscribeRemovesAndAllowsKeyReuse)
{
    std::vector<std::string> log;
    RecordingTickable a("a", log);
    RecordingTickable b("b", log);

    FrameDispatcher dispatcher;
    ASSERT_TRUE(dispatcher.Subscribe(a, 1, "A").HasValue());
    EXPECT_TRUE(dispatcher.IsSubscribed(a));

    ASSERT_TRUE(dispatcher.Unsubscribe(a).HasValue());
    EXPECT_FALSE(dispatcher.IsSubscribed(a));
    EXPECT_EQ(dispatcher.SubscriberCount(), static_cast<std::size_t>(0));

    // The key is free again.
    ASSERT_TRUE(dispatcher.Subscribe(b, 1, "B").HasValue());

    const auto missing = dispatcher.Unsubscribe(a);
    ASSERT_FALSE(missing.HasValue());
    EXPECT_TRUE(missing.GetError().Code() == ErrorCode::NotFound);

    dispatcher.Dispatch(0.016);
    EXPECT_EQ(a.tickCount, 0);
    EXPECT_EQ(b.tickCount, 1);
}
