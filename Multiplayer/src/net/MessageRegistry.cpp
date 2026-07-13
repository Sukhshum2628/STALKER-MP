// STALKER-MP — Message registry (Sprint-006, Step 4)
//
// See MessageRegistry.h. Engine-free / OS-free; no exceptions, core::Expected.
// Sorted-vector + hash accelerator (BC-005), ascending MessageId; id-ordered
// dispatch; unknown ids tolerated (counted). ADR-010 id partitioning.

#include "stalkermp/net/MessageRegistry.h"

#include <algorithm>

#include "stalkermp/common/StringUtil.h"     // common::Format
#include "stalkermp/net/ProtocolConstants.h" // IsControlId / IsDataId

namespace stalkermp::net
{
    core::Expected<void> MessageRegistry::Register(const MessageId id, IMessageHandler* const handler)
    {
        if (!IsControlId(id))
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("message id {} is not in the control range", id.value));
        }
        return Insert(id, handler);
    }

    core::Expected<void> MessageRegistry::RegisterData(const MessageId id, IMessageHandler* const handler)
    {
        if (!IsDataId(id))
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("message id {} is not in the data range", id.value));
        }
        return Insert(id, handler);
    }

    core::Expected<void> MessageRegistry::Insert(const MessageId id, IMessageHandler* const handler)
    {
        const auto pos = std::lower_bound(
            m_entries.begin(), m_entries.end(), id.value,
            [](const Entry& e, const std::uint16_t v) { return e.id.value < v; });
        if (pos != m_entries.end() && pos->id.value == id.value)
        {
            return core::MakeError(core::ErrorCode::AlreadyExists,
                                   common::Format("message id {} already registered", id.value));
        }
        m_entries.insert(pos, Entry{id, handler});
        RebuildAccelerator();
        return core::Success();
    }

    DispatchResult MessageRegistry::Dispatch(const MessageId id, ByteReader& payload, DispatchContext& context)
    {
        const auto it = m_index.find(id.value);
        if (it == m_index.end())
        {
            ++m_unknownCount;
            return DispatchResult::Unknown;
        }
        IMessageHandler* const handler = m_entries[it->second].handler;
        if (handler == nullptr)
        {
            return DispatchResult::Failed;
        }
        if (auto r = handler->Handle(payload, context); !r)
        {
            return DispatchResult::Failed;
        }
        return DispatchResult::Handled;
    }

    bool MessageRegistry::Contains(const MessageId id) const
    {
        return m_index.find(id.value) != m_index.end();
    }

    std::vector<MessageId> MessageRegistry::Ids() const
    {
        std::vector<MessageId> ids;
        ids.reserve(m_entries.size());
        for (const Entry& e : m_entries)
        {
            ids.push_back(e.id);
        }
        return ids;
    }

    void MessageRegistry::Clear()
    {
        m_entries.clear();
        m_index.clear();
        m_unknownCount = 0;
    }

    MessageRegistryReport MessageRegistry::ValidateConsistency() const
    {
        MessageRegistryReport report;
        for (std::size_t i = 1; i < m_entries.size(); ++i)
        {
            if (!(m_entries[i - 1].id.value < m_entries[i].id.value))
            {
                report.sortedUnique = false;
                break;
            }
        }
        if (m_index.size() != m_entries.size())
        {
            report.acceleratorConsistent = false;
        }
        else
        {
            for (std::size_t i = 0; i < m_entries.size(); ++i)
            {
                const auto it = m_index.find(m_entries[i].id.value);
                if (it == m_index.end() || it->second != i)
                {
                    report.acceleratorConsistent = false;
                    break;
                }
            }
        }
        return report;
    }

    void MessageRegistry::RebuildAccelerator()
    {
        m_index.clear();
        m_index.reserve(m_entries.size());
        for (std::size_t i = 0; i < m_entries.size(); ++i)
        {
            m_index.emplace(m_entries[i].id.value, i);
        }
    }
} // namespace stalkermp::net
