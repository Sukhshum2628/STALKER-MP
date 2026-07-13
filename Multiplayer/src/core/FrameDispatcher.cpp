#include "stalkermp/core/FrameDispatcher.h"

#include <algorithm>

#include "stalkermp/core/Assert.h"
#include "stalkermp/common/StringUtil.h"

namespace stalkermp::core
{
    Expected<void> FrameDispatcher::Subscribe(ITickable& subscriber, const std::uint32_t order,
                                              const std::string_view name)
    {
        SMP_ASSERT_MSG(!m_dispatching, "Subscribe called during Dispatch");
        if (m_dispatching)
        {
            return MakeError(ErrorCode::Internal,
                             "FrameDispatcher::Subscribe: subscription changes during Dispatch are forbidden");
        }

        for (const Entry& entry : m_subscribers)
        {
            if (entry.subscriber == &subscriber)
            {
                return MakeError(ErrorCode::AlreadyExists,
                                 common::Format("FrameDispatcher: subscriber '{}' already registered",
                                                entry.name));
            }
            if (entry.order == order)
            {
                return MakeError(ErrorCode::AlreadyExists,
                                 common::Format("FrameDispatcher: order key {} already taken by '{}' "
                                                "(ordering is total; allocate a new key in tick_order)",
                                                order, entry.name));
            }
        }

        Entry entry;
        entry.subscriber = &subscriber;
        entry.order = order;
        entry.name = std::string(name);

        const auto position = std::lower_bound(
            m_subscribers.begin(), m_subscribers.end(), order,
            [](const Entry& e, const std::uint32_t key) { return e.order < key; });
        m_subscribers.insert(position, std::move(entry));
        return Success();
    }

    Expected<void> FrameDispatcher::Unsubscribe(ITickable& subscriber)
    {
        SMP_ASSERT_MSG(!m_dispatching, "Unsubscribe called during Dispatch");
        if (m_dispatching)
        {
            return MakeError(ErrorCode::Internal,
                             "FrameDispatcher::Unsubscribe: subscription changes during Dispatch are forbidden");
        }

        const auto it = std::find_if(m_subscribers.begin(), m_subscribers.end(),
                                     [&](const Entry& entry) { return entry.subscriber == &subscriber; });
        if (it == m_subscribers.end())
        {
            return MakeError(ErrorCode::NotFound, "FrameDispatcher: subscriber not registered");
        }

        m_subscribers.erase(it);
        return Success();
    }

    void FrameDispatcher::Dispatch(const double deltaSeconds)
    {
        SMP_ASSERT_MSG(deltaSeconds >= 0.0, "FrameDispatcher::Dispatch: negative delta");
        SMP_ASSERT_MSG(!m_dispatching, "FrameDispatcher::Dispatch: re-entrant dispatch");

        m_dispatching = true;
        for (const Entry& entry : m_subscribers)
        {
            entry.subscriber->Tick(deltaSeconds);
        }
        m_dispatching = false;
    }

    bool FrameDispatcher::IsSubscribed(const ITickable& subscriber) const noexcept
    {
        for (const Entry& entry : m_subscribers)
        {
            if (entry.subscriber == &subscriber)
            {
                return true;
            }
        }
        return false;
    }
} // namespace stalkermp::core
