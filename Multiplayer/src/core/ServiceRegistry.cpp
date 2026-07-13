#include "stalkermp/core/ServiceRegistry.h"

#include <algorithm>
#include <unordered_set>

#include "stalkermp/core/Log.h"
#include "stalkermp/common/StringUtil.h"

namespace stalkermp::core
{
    ServiceRegistry::~ServiceRegistry()
    {
        ShutdownAll();
    }

    Expected<void> ServiceRegistry::RegisterImpl(const std::type_index type, std::unique_ptr<IService> service)
    {
        if (m_byType.find(type) != m_byType.end())
        {
            return MakeError(ErrorCode::AlreadyExists,
                             common::Format("ServiceRegistry: type '{}' already registered", type.name()));
        }
        if (FindByName(service->Name()) != nullptr)
        {
            return MakeError(ErrorCode::AlreadyExists,
                             common::Format("ServiceRegistry: service name '{}' already registered",
                                            service->Name()));
        }

        m_byType.emplace(type, m_entries.size());
        m_entries.push_back(Entry{type, std::move(service), false});
        return Success();
    }

    IService* ServiceRegistry::FindImpl(const std::type_index type) const noexcept
    {
        const auto it = m_byType.find(type);
        return it != m_byType.end() ? m_entries[it->second].service.get() : nullptr;
    }

    IService* ServiceRegistry::FindByName(const std::string_view name) const noexcept
    {
        for (const Entry& entry : m_entries)
        {
            if (entry.service->Name() == name)
            {
                return entry.service.get();
            }
        }
        return nullptr;
    }

    Expected<void> ServiceRegistry::ValidateDependencies() const
    {
        std::unordered_set<std::string> seen;
        for (const Entry& entry : m_entries)
        {
            for (const std::string& dependency : entry.service->Dependencies())
            {
                if (seen.find(dependency) != seen.end())
                {
                    continue;
                }
                const bool registeredLater = FindByName(dependency) != nullptr;
                return MakeError(
                    ErrorCode::DependencyMissing,
                    registeredLater
                        ? common::Format("Service '{}' depends on '{}', which is registered after it "
                                         "(registration order is initialization order)",
                                         entry.service->Name(), dependency)
                        : common::Format("Service '{}' depends on '{}', which is not registered",
                                         entry.service->Name(), dependency));
            }
            seen.insert(std::string(entry.service->Name()));
        }
        return Success();
    }

    Expected<void> ServiceRegistry::InitializeAll()
    {
        if (auto validation = ValidateDependencies(); !validation)
        {
            return validation;
        }

        for (Entry& entry : m_entries)
        {
            if (entry.initialized)
            {
                continue;
            }

            if (auto result = entry.service->Initialize(); !result)
            {
                const Error failure = MakeError(
                    result.GetError().Code(),
                    common::Format("Service '{}' failed to initialize: {}",
                                   entry.service->Name(), result.GetError().Message()));

                // Never remain half-initialized: unwind what succeeded.
                ShutdownAll();
                return failure;
            }
            entry.initialized = true;
        }
        return Success();
    }

    void ServiceRegistry::ShutdownAll() noexcept
    {
        for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
        {
            if (!it->initialized)
            {
                continue;
            }
            it->service->Shutdown();
            it->initialized = false;

            if (IsLogAvailable())
            {
                Log().Debug("ServiceRegistry",
                            common::Format("Service '{}' shut down", it->service->Name()));
            }
        }
    }

    std::size_t ServiceRegistry::Count() const noexcept
    {
        return m_entries.size();
    }
} // namespace stalkermp::core
