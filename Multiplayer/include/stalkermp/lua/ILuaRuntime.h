// STALKER-MP — Lua runtime seam (Sprint-013, Step 3)
//
// The engine-free / VM-free boundary through which the scripting subsystem drives a
// script runtime. This is the ADR-009 SCRIPTING-VM BOUNDARY: the `lua::` core talks
// ONLY to this interface with value-only arguments and value outcomes; the concrete
// VM binding (which reuses the X-Ray engine's existing Lua runtime) is added later at
// Step-17 in the sanctioned boundary TU and is the ONLY translation unit that touches
// a real VM. No VM/engine/OS header enters this file or any core consumer of it.
//
// This step introduces the SEAM ONLY, plus two engine-free test doubles:
//   * `FakeLuaRuntime`  — deterministic; records loads/executions/registrations/
//     invocations and returns scriptable outcomes (for unit tests).
//   * `NullLuaRuntime`  — inert; accepts everything and does nothing.
// No concrete VM, no loading orchestration (Step 11), no manager (Steps 12-13),
// no wiring (Step 17).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream;
// value outcomes / Expected<T>; bounded memory.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"        // core::Expected
#include "stalkermp/lua/LuaScriptTypes.h"   // ScriptId / CallbackId / ScriptOutcome (Step 1)

namespace stalkermp::lua
{
    // Value-only identity of a host function registered into the runtime. The concrete
    // binding of an id to a real native function is a later step; the seam handles only
    // the value id. `0 = none`.
    struct HostFunctionId
    {
        std::uint64_t value = 0;

        [[nodiscard]] constexpr bool IsNone() const noexcept { return value == 0; }
        [[nodiscard]] constexpr bool operator==(const HostFunctionId& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const HostFunctionId& o) const noexcept { return value != o.value; }
    };

    // Value-only invocation arguments crossing the seam. Opaque value words only — no
    // VM type, no engine object. Bounded, trivially copyable.
    struct ScriptArgs
    {
        std::uint32_t count = 0;              // number of meaningful words (0..4)
        std::uint64_t words[4] = {0, 0, 0, 0};
    };

    // The engine-free scripting runtime boundary. All operations are value outcomes
    // (ADR-007); nothing here throws or exposes a VM type.
    class ILuaRuntime
    {
    public:
        virtual ~ILuaRuntime() = default;

        // Create / destroy the underlying script state.
        [[nodiscard]] virtual ScriptOutcome CreateState() = 0;
        virtual void DestroyState() noexcept = 0;

        // Load the runtime's standard libraries into the created state.
        [[nodiscard]] virtual ScriptOutcome LoadStandardLibraries() = 0;

        // Load a named chunk of script bytes, yielding a ScriptId on success or a value
        // outcome on failure (e.g. syntax error).
        [[nodiscard]] virtual core::Expected<ScriptId> LoadChunk(std::string_view name,
                                                                 const std::vector<std::byte>& bytes) = 0;

        // Execute a previously-loaded script.
        [[nodiscard]] virtual ScriptOutcome Execute(ScriptId id) = 0;

        // Register a host function under a name (value id only; concrete binding later).
        [[nodiscard]] virtual ScriptOutcome RegisterFunction(std::string_view name, HostFunctionId handle) = 0;

        // Invoke a registered callback with value args.
        [[nodiscard]] virtual ScriptOutcome InvokeCallback(CallbackId id, const ScriptArgs& args) = 0;
    };

    // ------------------------------------------------------------- FakeLuaRuntime
    // Deterministic in-memory double: records every seam interaction and returns
    // scriptable outcomes so tests can drive both success and each failure path. No
    // VM, no engine, no OS. Records are public for inspection (mirrors the project's
    // recording-double convention).
    class FakeLuaRuntime final : public ILuaRuntime
    {
    public:
        struct LoadRecord
        {
            std::string name;
            std::size_t byteCount = 0;
            ScriptId assigned{};
        };
        struct RegisterRecord
        {
            std::string name;
            HostFunctionId handle{};
        };
        struct InvokeRecord
        {
            CallbackId id{};
            ScriptArgs args{};
        };

        // Public inspection surface.
        bool stateCreated = false;
        bool librariesLoaded = false;
        std::vector<LoadRecord> loads;
        std::vector<ScriptId> executions;
        std::vector<RegisterRecord> registrations;
        std::vector<InvokeRecord> invocations;

        // Scriptable outcomes (default = success). LoadChunk fails with a value outcome
        // when loadOutcome != Ok.
        ScriptOutcome createOutcome = ScriptOutcome::Ok;
        ScriptOutcome librariesOutcome = ScriptOutcome::Ok;
        ScriptOutcome loadOutcome = ScriptOutcome::Ok;
        ScriptOutcome executeOutcome = ScriptOutcome::Ok;
        ScriptOutcome registerOutcome = ScriptOutcome::Ok;
        ScriptOutcome invokeOutcome = ScriptOutcome::Ok;

        [[nodiscard]] ScriptOutcome CreateState() override
        {
            if (createOutcome == ScriptOutcome::Ok)
            {
                stateCreated = true;
            }
            return createOutcome;
        }

        void DestroyState() noexcept override
        {
            stateCreated = false;
            librariesLoaded = false;
        }

        [[nodiscard]] ScriptOutcome LoadStandardLibraries() override
        {
            if (librariesOutcome == ScriptOutcome::Ok)
            {
                librariesLoaded = true;
            }
            return librariesOutcome;
        }

        [[nodiscard]] core::Expected<ScriptId> LoadChunk(std::string_view name,
                                                         const std::vector<std::byte>& bytes) override
        {
            if (loadOutcome != ScriptOutcome::Ok)
            {
                return core::MakeError(core::ErrorCode::InvalidArgument, ScriptOutcomeName(loadOutcome));
            }
            const ScriptId assigned{++m_nextScriptId}; // deterministic ascending, never 0
            loads.push_back(LoadRecord{std::string(name), bytes.size(), assigned});
            return assigned;
        }

        [[nodiscard]] ScriptOutcome Execute(ScriptId id) override
        {
            executions.push_back(id);
            return executeOutcome;
        }

        [[nodiscard]] ScriptOutcome RegisterFunction(std::string_view name, HostFunctionId handle) override
        {
            registrations.push_back(RegisterRecord{std::string(name), handle});
            return registerOutcome;
        }

        [[nodiscard]] ScriptOutcome InvokeCallback(CallbackId id, const ScriptArgs& args) override
        {
            invocations.push_back(InvokeRecord{id, args});
            return invokeOutcome;
        }

    private:
        std::uint64_t m_nextScriptId = 0;
    };

    // ------------------------------------------------------------- NullLuaRuntime
    // Inert double: accepts everything, records nothing, drives no VM. LoadChunk yields
    // a deterministic ascending id so composed callers remain well-formed.
    class NullLuaRuntime final : public ILuaRuntime
    {
    public:
        [[nodiscard]] ScriptOutcome CreateState() override { return ScriptOutcome::Ok; }
        void DestroyState() noexcept override {}
        [[nodiscard]] ScriptOutcome LoadStandardLibraries() override { return ScriptOutcome::Ok; }

        [[nodiscard]] core::Expected<ScriptId> LoadChunk(std::string_view, const std::vector<std::byte>&) override
        {
            return ScriptId{++m_nextScriptId};
        }

        [[nodiscard]] ScriptOutcome Execute(ScriptId) override { return ScriptOutcome::Ok; }
        [[nodiscard]] ScriptOutcome RegisterFunction(std::string_view, HostFunctionId) override
        {
            return ScriptOutcome::Ok;
        }
        [[nodiscard]] ScriptOutcome InvokeCallback(CallbackId, const ScriptArgs&) override
        {
            return ScriptOutcome::Ok;
        }

    private:
        std::uint64_t m_nextScriptId = 0;
    };
} // namespace stalkermp::lua
