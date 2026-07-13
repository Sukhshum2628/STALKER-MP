// STALKER-MP — Module registration (Sprint-001, §7.10)
//
// Why this exists:
//   The project's top-level modules (World, Player, Replication,
//   Persistence, Networking, Plugins, Diagnostics) are declared up front so
//   every later sprint attaches its implementation to an already-known
//   module identity. Sprint-001 registers descriptors only — the sprint
//   spec states "Modules may remain empty."
//
// Responsibilities:
//   - Hold the catalogue of declared modules.
//   - Attach an IModule implementation to a declared module (later sprints).
//
// Ownership / lifetime:
//   Owned by the Bootstrap runtime. When a module implementation is
//   attached, the registry owns it.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IModule.h"

namespace stalkermp::core
{
    struct ModuleDescriptor
    {
        std::string name;        // Stable module identity, e.g. "World".
        std::string description; // Purpose and the sprint that implements it.
    };

    class ModuleRegistry
    {
    public:
        ModuleRegistry() = default;

        ModuleRegistry(const ModuleRegistry&) = delete;
        ModuleRegistry& operator=(const ModuleRegistry&) = delete;

        // Declares a module. Fails with AlreadyExists on duplicate names.
        Expected<void> Declare(ModuleDescriptor descriptor);

        // Attaches an implementation to a previously declared module.
        // Fails with NotFound if the module was never declared, or
        // AlreadyExists if an implementation is already attached.
        Expected<void> AttachImplementation(std::string_view moduleName, std::unique_ptr<IModule> implementation);

        [[nodiscard]] bool IsDeclared(std::string_view moduleName) const noexcept;
        [[nodiscard]] bool IsImplemented(std::string_view moduleName) const noexcept;
        [[nodiscard]] IModule* FindImplementation(std::string_view moduleName) const noexcept;

        [[nodiscard]] const std::vector<ModuleDescriptor>& Declared() const noexcept { return m_descriptors; }
        [[nodiscard]] std::size_t Count() const noexcept { return m_descriptors.size(); }

    private:
        struct Slot
        {
            std::unique_ptr<IModule> implementation; // Null until attached.
        };

        [[nodiscard]] std::ptrdiff_t IndexOf(std::string_view moduleName) const noexcept;

        std::vector<ModuleDescriptor> m_descriptors;
        std::vector<Slot> m_slots; // Parallel to m_descriptors.
    };
} // namespace stalkermp::core
