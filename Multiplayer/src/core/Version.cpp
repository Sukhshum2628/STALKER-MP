#include "stalkermp/core/Version.h"

#include "stalkermp/common/StringUtil.h"

namespace stalkermp::core::version
{
    std::string_view BuildTimestamp() noexcept
    {
        return __DATE__ " " __TIME__;
    }

    std::string_view GitRevision() noexcept
    {
#if defined(SMP_GIT_REVISION)
        return SMP_GIT_REVISION;
#else
        return "unstamped";
#endif
    }

    std::string Banner()
    {
        return common::Format("STALKER-MP {} (compat {}, built {}, rev {})",
                              kProject.ToString(), kCompatibility, BuildTimestamp(), GitRevision());
    }

    Expected<void> VerifyEngineCompatibility(const std::string_view engineVersion)
    {
        if (engineVersion.empty())
        {
            return MakeError(ErrorCode::InvalidArgument,
                             "Engine compatibility check: empty engine version string");
        }
        if (!common::StartsWith(engineVersion, kCompatibleEnginePrefix))
        {
            return MakeError(ErrorCode::VersionMismatch,
                             common::Format("Engine '{}' is not compatible; expected prefix '{}'",
                                            engineVersion, kCompatibleEnginePrefix));
        }
        return Success();
    }
} // namespace stalkermp::core::version
