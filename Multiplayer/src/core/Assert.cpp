#include "stalkermp/core/Assert.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

#include "stalkermp/core/Log.h"
#include "stalkermp/common/StringUtil.h"

namespace stalkermp::core
{
    namespace
    {
        [[nodiscard]] std::string DescribeFailure(const AssertContext& context)
        {
            std::string text = context.fatal ? "FATAL: " : "Assertion failed: ";
            if (context.expression != nullptr)
            {
                text += context.expression;
            }
            if (context.message != nullptr)
            {
                if (context.expression != nullptr)
                {
                    text += " — ";
                }
                text += context.message;
            }
            text += common::Format(" (in {} at {}:{})",
                                   context.function != nullptr ? context.function : "unknown",
                                   context.file != nullptr ? context.file : "unknown",
                                   context.line);
            return text;
        }

        void EmitFailure(const AssertContext& context) noexcept
        {
            // Assertions must work before, during and after logger
            // lifetime, so stderr is the unconditional channel and the
            // logger is used additionally when available.
            const std::string description = DescribeFailure(context);
            std::fputs(description.c_str(), stderr);
            std::fputc('\n', stderr);
            std::fflush(stderr);

            if (IsLogAvailable())
            {
                Log().Error("Assert", description);
                Log().Flush();
            }
        }

        bool DefaultAssertHandler(const AssertContext& context) noexcept
        {
            EmitFailure(context);
#if defined(NDEBUG)
            return false; // Release: report and continue (SMP_VERIFY semantics).
#else
            return true; // Debug: request a trap at the failure site.
#endif
        }

        std::atomic<AssertHandler> g_handler{&DefaultAssertHandler};
    } // namespace

    AssertHandler SetAssertHandler(AssertHandler handler) noexcept
    {
        if (handler == nullptr)
        {
            handler = &DefaultAssertHandler;
        }
        return g_handler.exchange(handler);
    }

    bool ReportAssertFailure(const AssertContext& context) noexcept
    {
        return g_handler.load()(context);
    }

    void ReportFatalFailure(const AssertContext& context) noexcept
    {
        // The installed handler observes the failure first (tests rely on
        // this), then the process terminates unconditionally.
        g_handler.load()(context);
        std::abort();
    }
} // namespace stalkermp::core
