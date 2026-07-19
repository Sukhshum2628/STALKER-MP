// STALKER-MP — In-memory save source (Sprint-012, Step 8)
//
// See InMemorySaveSource.h. Engine-free / OS-free; no exceptions, no RTTI, no
// iostream; value outcomes / Expected<T>.

#include "stalkermp/saveload/InMemorySaveSource.h"

#include "stalkermp/core/Error.h" // core::MakeError / ErrorCode

namespace stalkermp::saveload
{
    void InMemorySaveSource::Store(const SaveDescriptor& descriptor, std::vector<std::uint8_t> bytes)
    {
        // Replace an existing entry for the same saveId; otherwise insert in ascending
        // saveId order (deterministic enumeration).
        for (Entry& e : m_entries)
        {
            if (e.descriptor.saveId == descriptor.saveId)
            {
                e.descriptor = descriptor;
                e.bytes = std::move(bytes);
                return;
            }
        }
        std::size_t insertAt = m_entries.size();
        for (std::size_t i = 0; i < m_entries.size(); ++i)
        {
            if (descriptor.saveId < m_entries[i].descriptor.saveId)
            {
                insertAt = i;
                break;
            }
        }
        m_entries.insert(m_entries.begin() + static_cast<std::ptrdiff_t>(insertAt),
                         Entry{descriptor, std::move(bytes)});
    }

    void InMemorySaveSource::Remove(const std::uint64_t saveId) noexcept
    {
        for (std::size_t i = 0; i < m_entries.size(); ++i)
        {
            if (m_entries[i].descriptor.saveId == saveId)
            {
                m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    std::vector<SaveDescriptor> InMemorySaveSource::Enumerate() const
    {
        std::vector<SaveDescriptor> out;
        out.reserve(m_entries.size());
        for (const Entry& e : m_entries) // already ascending by saveId
        {
            out.push_back(e.descriptor);
        }
        return out;
    }

    core::Expected<std::vector<std::uint8_t>> InMemorySaveSource::Read(const std::uint64_t saveId) const
    {
        for (const Entry& e : m_entries)
        {
            if (e.descriptor.saveId == saveId)
            {
                return e.bytes; // value copy
            }
        }
        return core::MakeError(core::ErrorCode::NotFound, "save not found");
    }

    bool InMemorySaveSource::Exists(const std::uint64_t saveId) const
    {
        for (const Entry& e : m_entries)
        {
            if (e.descriptor.saveId == saveId)
            {
                return true;
            }
        }
        return false;
    }
} // namespace stalkermp::saveload
