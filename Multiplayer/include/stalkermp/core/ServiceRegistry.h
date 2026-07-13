// STALKER-MP — Service registry / locator (Sprint-001, §7.9)
//
// Why this exists:
//   Subsystems obtain their collaborators through one explicit registry
//   instead of global singletons. The registry also owns deterministic
//   shutdown ordering (reverse of registration) and validates declared
//   dependencies before initialization.
//
// Responsibilities:
//   - Register services (ownership transfers to the registry).
//   - Find services by type.
//   - Validate dependencies (every declared dependency is registered, and
//     is registered *before* its dependent — registration order is
//     initialization order).
//   - Initialize in registration order; shut down in reverse order.
//
// Ownership / lifetime:
//   The registry owns every registered service (unique_ptr). The registry
//   itself is owned by the Bootstrap runtime. It is not a global.
//
// Threading:
//   Registration and initialization happen during Bootstrap on one thread.
//   Find() is read-only afterwards and safe from any thread.

#pragma once

#include <memory>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"

namespace stalkermp::core
{
    class ServiceRegistry
    {
    public:
        ServiceRegistry() = default;

        ServiceRegistry(const ServiceRegistry&) = delete;
        ServiceRegistry& operator=(const ServiceRegistry&) = delete;

        ~ServiceRegistry();

        // Registers a service under its concrete type T. Fails with
        // AlreadyExists if T (or a service with the same Name) is present.
        template <typename T>
        Expected<void> Register(std::unique_ptr<T> service)
        {
            static_assert(std::is_base_of_v<IService, T>, "Registered services must implement IService");
            if (!service)
            {
                return MakeError(ErrorCode::InvalidArgument, "ServiceRegistry::Register: null service");
            }
            return RegisterImpl(std::type_index(typeid(T)), std::move(service));
        }

        // Finds a service by its registered concrete type. Returns nullptr
        // when absent — callers decide whether absence is an error.
        template <typename T>
        [[nodiscard]] T* Find() const noexcept
        {
            static_assert(std::is_base_of_v<IService, T>, "Registered services must implement IService");
            return static_cast<T*>(FindImpl(std::type_index(typeid(T))));
        }

        [[nodiscard]] IService* FindByName(std::string_view name) const noexcept;

        // Verifies that every declared dependency (IService::Dependencies)
        // names a service registered earlier in the registration order.
        [[nodiscard]] Expected<void> ValidateDependencies() const;

        // Initializes all services in registration order. Stops at the
        // first failure; already-initialized services are shut down again
        // (in reverse) so the registry never remains half-initialized.
        [[nodiscard]] Expected<void> InitializeAll();

        // Shuts down all initialized services in reverse registration order.
        // Safe to call multiple times.
        void ShutdownAll() noexcept;

        [[nodiscard]] std::size_t Count() const noexcept;

    private:
        struct Entry
        {
            std::type_index type;
            std::unique_ptr<IService> service;
            bool initialized = false;
        };

        Expected<void> RegisterImpl(std::type_index type, std::unique_ptr<IService> service);
        [[nodiscard]] IService* FindImpl(std::type_index type) const noexcept;

        std::vector<Entry> m_entries;                          // Registration order.
        std::unordered_map<std::type_index, std::size_t> m_byType; // type -> index into m_entries.
    };
} // namespace stalkermp::core
