// STALKER-MP — Client-presentation frame phase hook (Sprint-010, Step 16)
//
// The client-presentation phase runs ONCE per engine frame, synchronously, AFTER
// FrameDispatcher::Dispatch returns — it is NOT a FrameDispatcher subscriber and
// introduces no new tick_order key (networking-last preserved). The engine frame
// bridge (EngineAdapters.cpp) calls AdvanceClientPresentation immediately after
// Dispatch; the composition root (Bootstrap.cpp) owns the driver and defines this
// hook. Tests drive the phase directly through the same entry point.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

namespace stalkermp::prediction
{
    class ClientPresentationDriver; // forward declaration (no heavy include here)
} // namespace stalkermp::prediction

namespace stalkermp::detail
{
    // Runs the client-presentation driver's Advance once for this frame, after
    // FrameDispatcher::Dispatch. No-op when the module is not initialized or no
    // driver is wired. Deterministic (tick-driven); `deltaSeconds` is not used in
    // control flow (Replay Determinism). Never throws across the engine boundary.
    void AdvanceClientPresentation(double deltaSeconds) noexcept;

    // Read-only access to the runtime-owned client-presentation driver (nullptr
    // when uninitialized). For composition-root wiring assertions; subsystems never
    // use it.
    [[nodiscard]] const prediction::ClientPresentationDriver* GetModuleClientPresentationDriver() noexcept;
} // namespace stalkermp::detail
