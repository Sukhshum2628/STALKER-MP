#include "stalkermp/core/ModuleRegistry.h"

#include "stalkermp/common/StringUtil.h"

namespace stalkermp::core
{
    Expected<void> ModuleRegistry::Declare(ModuleDescriptor descriptor)
    {
        if (descriptor.name.empty())
        {
            return MakeError(ErrorCode::InvalidArgument, "ModuleRegistry::Declare: empty module name");
        }
        if (IsDeclared(descriptor.name))
        {
            return MakeError(ErrorCode::AlreadyExists,
                             common::Format("ModuleRegistry: module '{}' already declared", descriptor.name));
        }

        m_descriptors.push_back(std::move(descriptor));
        m_slots.emplace_back();
        return Success();
    }

    Expected<void> ModuleRegistry::AttachImplementation(const std::string_view moduleName,
                                                        std::unique_ptr<IModule> implementation)
    {
        if (!implementation)
        {
            return MakeError(ErrorCode::InvalidArgument, "ModuleRegistry::AttachImplementation: null module");
        }

        const std::ptrdiff_t index = IndexOf(moduleName);
        if (index < 0)
        {
            return MakeError(ErrorCode::NotFound,
                             common::Format("ModuleRegistry: module '{}' was never declared", moduleName));
        }
        if (m_slots[static_cast<std::size_t>(index)].implementation)
        {
            return MakeError(ErrorCode::AlreadyExists,
                             common::Format("ModuleRegistry: module '{}' already has an implementation",
                                            moduleName));
        }

        m_slots[static_cast<std::size_t>(index)].implementation = std::move(implementation);
        return Success();
    }

    bool ModuleRegistry::IsDeclared(const std::string_view moduleName) const noexcept
    {
        return IndexOf(moduleName) >= 0;
    }

    bool ModuleRegistry::IsImplemented(const std::string_view moduleName) const noexcept
    {
        const std::ptrdiff_t index = IndexOf(moduleName);
        return index >= 0 && m_slots[static_cast<std::size_t>(index)].implementation != nullptr;
    }

    IModule* ModuleRegistry::FindImplementation(const std::string_view moduleName) const noexcept
    {
        const std::ptrdiff_t index = IndexOf(moduleName);
        return index >= 0 ? m_slots[static_cast<std::size_t>(index)].implementation.get() : nullptr;
    }

    std::ptrdiff_t ModuleRegistry::IndexOf(const std::string_view moduleName) const noexcept
    {
        for (std::size_t i = 0; i < m_descriptors.size(); ++i)
        {
            if (m_descriptors[i].name == moduleName)
            {
                return static_cast<std::ptrdiff_t>(i);
            }
        }
        return -1;
    }
} // namespace stalkermp::core
