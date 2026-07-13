// STALKER-MP — Assertion framework (Sprint-001, §7.6)
//
// Why this exists:
//   Assertions detect programmer errors (invariant violations) as early as
//   possible. They are distinct from runtime validation of external input,
//   which uses Error/Expected (02_Engine.md §19.6).
//
// Responsibilities:
//   - SMP_ASSERT / SMP_ASSERT_MSG : development assertions (compiled out in
//     release builds).
//   - SMP_VERIFY                  : always evaluated; reports on failure and
//     continues (release) or traps (debug).
//   - SMP_FATAL                   : unconditional fatal error; logs, flushes
//     and terminates. Used when continuing would corrupt state.
//   - Pluggable failure handler so unit tests can observe failures without
//     terminating the test runner.
//   - Compile-time assertions use static_assert directly; no wrapper needed.
//
// Ownership / lifetime:
//   The failure handler is process-wide state guarded internally. It is
//   installed once by Bootstrap and may be replaced by tests.
//
// Note on macros:
//   Macros are used here because capturing __FILE__/__LINE__ and the
//   expression text is impossible with functions. This is the accepted
//   exception to the "avoid macros" rule (Sprint-001 §7.6).

#pragma once

#include <cstdint>

namespace stalkermp::core
{
    struct AssertContext
    {
        const char* expression = nullptr; // Text of the failed expression (may be null for SMP_FATAL).
        const char* message = nullptr;    // Optional developer message (may be null).
        const char* file = nullptr;
        const char* function = nullptr;   // Enclosing function (__func__ at the failure site).
        std::uint32_t line = 0;
        bool fatal = false;
    };

    // Handler invoked on every assertion failure.
    // Returning true requests a debug trap at the failure site (default
    // handler returns true in debug builds). Fatal failures never return.
    using AssertHandler = bool (*)(const AssertContext& context) noexcept;

    // Installs a custom handler and returns the previous one.
    // Passing nullptr restores the default handler.
    AssertHandler SetAssertHandler(AssertHandler handler) noexcept;

    // Invoked by the macros below. Not called directly by user code.
    bool ReportAssertFailure(const AssertContext& context) noexcept;

    // Logs, flushes the logger and terminates the process. Never returns.
    [[noreturn]] void ReportFatalFailure(const AssertContext& context) noexcept;
} // namespace stalkermp::core

#if defined(_MSC_VER)
    #define SMP_DEBUG_TRAP() __debugbreak()
#else
    #define SMP_DEBUG_TRAP() __builtin_trap()
#endif

#define SMP_INTERNAL_ASSERT_IMPL(expr, msg)                                              \
    do                                                                                   \
    {                                                                                    \
        if (!(expr))                                                                     \
        {                                                                                \
            const ::stalkermp::core::AssertContext smpAssertContext{                     \
                #expr, msg, __FILE__, __func__,                                          \
                static_cast<std::uint32_t>(__LINE__), false};                            \
            if (::stalkermp::core::ReportAssertFailure(smpAssertContext))                \
            {                                                                            \
                SMP_DEBUG_TRAP();                                                        \
            }                                                                            \
        }                                                                                \
    } while (false)

// Development assertions: active unless NDEBUG is defined.
#if defined(NDEBUG)
    #define SMP_ASSERT(expr) ((void)0)
    #define SMP_ASSERT_MSG(expr, msg) ((void)0)
#else
    #define SMP_ASSERT(expr) SMP_INTERNAL_ASSERT_IMPL(expr, nullptr)
    #define SMP_ASSERT_MSG(expr, msg) SMP_INTERNAL_ASSERT_IMPL(expr, msg)
#endif

// Always-on verification. The expression is evaluated in every build.
#define SMP_VERIFY(expr) SMP_INTERNAL_ASSERT_IMPL(expr, nullptr)
#define SMP_VERIFY_MSG(expr, msg) SMP_INTERNAL_ASSERT_IMPL(expr, msg)

// Unconditional fatal error. Logs, flushes and terminates.
#define SMP_FATAL(msg)                                                                   \
    ::stalkermp::core::ReportFatalFailure(::stalkermp::core::AssertContext{              \
        nullptr, msg, __FILE__, __func__, static_cast<std::uint32_t>(__LINE__), true})
