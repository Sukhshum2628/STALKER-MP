// STALKER-MP — Null script source (Sprint-013, Step 7)
//
// An inert IScriptSource that holds no scripts: Enumerate is empty, Read fails with
// NotFound, Exists is false. The "no script source configured" read backend for builds
// without a source. Engine-free / VM-free / OS-free. ADR-007.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/core/Error.h"          // core::MakeError / ErrorCode
#include "stalkermp/lua/IScriptSource.h"

namespace stalkermp::lua
{
    class NullScriptSource final : public IScriptSource
    {
    public:
        [[nodiscard]] std::vector<ScriptDescriptor> Enumerate() const override { return {}; }

        [[nodiscard]] core::Expected<std::vector<std::byte>> Read(ScriptId) const override
        {
            return core::MakeError(core::ErrorCode::NotFound, "no script source configured");
        }

        [[nodiscard]] bool Exists(ScriptId) const override { return false; }
    };
} // namespace stalkermp::lua
