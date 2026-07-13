// STALKER-MP — Expected<T> result type (Sprint-001, §7.12)
//
// Why this exists:
//   The coding standard calls for std::expected, which requires C++23.
//   The module builds as C++17 (approved toolchain decision, see
//   Multiplayer/docs/Decisions.md), so this header provides the equivalent:
//   a value-or-Error result type used by every fallible operation.
//
// Responsibilities:
//   - Carry either a value of type T or a core::Error.
//   - Make failure handling explicit at every call site.
//
// Ownership / lifetime:
//   Value type built on std::variant. No external dependencies.
//
// Interactions:
//   Returned by ConfigLoader, ServiceRegistry, Bootstrap and utilities.

#pragma once

#include <utility>
#include <variant>

#include "stalkermp/core/Assert.h"
#include "stalkermp/core/Error.h"

namespace stalkermp::core
{
    template <typename T>
    class Expected
    {
    public:
        // Implicit construction from a value or an Error keeps call sites
        // readable: `return value;` / `return MakeError(...);`
        Expected(T value)
            : m_storage(std::in_place_index<0>, std::move(value))
        {
        }

        Expected(Error error)
            : m_storage(std::in_place_index<1>, std::move(error))
        {
        }

        [[nodiscard]] bool HasValue() const noexcept { return m_storage.index() == 0; }
        [[nodiscard]] explicit operator bool() const noexcept { return HasValue(); }

        // Accessing the wrong alternative is a programmer error.
        [[nodiscard]] T& Value() &
        {
            SMP_ASSERT_MSG(HasValue(), "Expected::Value() called on an error result");
            return std::get<0>(m_storage);
        }

        [[nodiscard]] const T& Value() const&
        {
            SMP_ASSERT_MSG(HasValue(), "Expected::Value() called on an error result");
            return std::get<0>(m_storage);
        }

        [[nodiscard]] T&& Value() &&
        {
            SMP_ASSERT_MSG(HasValue(), "Expected::Value() called on an error result");
            return std::get<0>(std::move(m_storage));
        }

        [[nodiscard]] const Error& GetError() const
        {
            SMP_ASSERT_MSG(!HasValue(), "Expected::GetError() called on a value result");
            return std::get<1>(m_storage);
        }

        [[nodiscard]] T ValueOr(T fallback) const&
        {
            return HasValue() ? std::get<0>(m_storage) : std::move(fallback);
        }

    private:
        std::variant<T, Error> m_storage;
    };

    // Specialization for operations that produce no value.
    template <>
    class Expected<void>
    {
    public:
        Expected() = default;

        Expected(Error error)
            : m_error(std::move(error))
            , m_hasError(true)
        {
        }

        [[nodiscard]] bool HasValue() const noexcept { return !m_hasError; }
        [[nodiscard]] explicit operator bool() const noexcept { return HasValue(); }

        [[nodiscard]] const Error& GetError() const
        {
            SMP_ASSERT_MSG(m_hasError, "Expected<void>::GetError() called on a success result");
            return m_error;
        }

    private:
        Error m_error;
        bool m_hasError = false;
    };

    // Canonical success value for Expected<void> call sites.
    [[nodiscard]] inline Expected<void> Success()
    {
        return Expected<void>();
    }
} // namespace stalkermp::core
