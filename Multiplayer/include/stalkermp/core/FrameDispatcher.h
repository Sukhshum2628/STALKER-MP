// STALKER-MP — Frame dispatcher (Sprint-002, core infrastructure)
//
// Why this exists:
//   Exactly one engine frame observer exists for the whole module
//   (EngineFrameBridge, registered in Device.seqFrame). FrameDispatcher is
//   its single consumer: it fans the engine frame out to STALKER-MP
//   subsystems in a deterministic, centrally owned order.
//
// Contract (authoritative reference: docs/Sprint-002-Design-Review.md §11):
//   - FrameDispatcher owns execution ordering; the ENGINE owns scheduling.
//   - Never creates threads, never controls frame timing; dispatch is
//     synchronous, on the engine main thread, inside the engine frame.
//   - Subscribers register with an explicit key from tick_order below.
//     Duplicate keys are rejected — ordering is total and deterministic.
//   - No subsystem may bypass this dispatcher with its own engine hook.
//   - No subscription changes during Dispatch (asserted).
//
// Ownership / lifetime:
//   Owned by the Bootstrap runtime. Holds non-owning references to
//   subscribers (services owned by the ServiceRegistry); every subscriber
//   must unsubscribe (Bootstrap does this) before it is destroyed.
//
// Threading:
//   Engine main thread only. Not thread-safe by design (RFC-0006 worker
//   threads never enter frame dispatch).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/ITickable.h"

namespace stalkermp::core
{
    // Central ordering table. New slots are allocated only here, together
    // with the design-review table (§11) and the sprint introducing them.
    namespace tick_order
    {
        inline constexpr std::uint32_t kWorld = 100;          // Sprint-002
        inline constexpr std::uint32_t kEntityRegistry = 200; // Sprint-003
        inline constexpr std::uint32_t kPlayerLifecycle = 250; // Sprint-007 (additive; EntityRegistry -> Player -> Bubble)
        inline constexpr std::uint32_t kBubble = 300;         // Sprint-004
        inline constexpr std::uint32_t kAlifeTransition = 350; // Sprint-005 (additive; wired at Step 10)
        inline constexpr std::uint32_t kReplication = 400;    // Sprint-008
        inline constexpr std::uint32_t kPersistence = 500;    // Sprint-011
        inline constexpr std::uint32_t kDiagnostics = 600;    // Sprint-015
        inline constexpr std::uint32_t kPlugins = 700;        // Sprint-014
        inline constexpr std::uint32_t kNetworking = 900;     // Sprint-006 (highest key; networking-last invariant)
    } // namespace tick_order

    class FrameDispatcher
    {
    public:
        FrameDispatcher() = default;

        FrameDispatcher(const FrameDispatcher&) = delete;
        FrameDispatcher& operator=(const FrameDispatcher&) = delete;

        // Registers a subscriber under an ordering key. Fails with
        // AlreadyExists if the key or the subscriber is already registered.
        // The name identifies the subscriber in diagnostics.
        Expected<void> Subscribe(ITickable& subscriber, std::uint32_t order, std::string_view name);

        // Removes a subscriber. Fails with NotFound if absent.
        Expected<void> Unsubscribe(ITickable& subscriber);

        // Invokes every subscriber in ascending key order.
        // deltaSeconds is the engine frame delta (>= 0).
        void Dispatch(double deltaSeconds);

        [[nodiscard]] std::size_t SubscriberCount() const noexcept { return m_subscribers.size(); }
        [[nodiscard]] bool IsSubscribed(const ITickable& subscriber) const noexcept;

    private:
        struct Entry
        {
            ITickable* subscriber = nullptr; // Non-owning.
            std::uint32_t order = 0;
            std::string name;
        };

        std::vector<Entry> m_subscribers; // Kept sorted by ascending order key.
        bool m_dispatching = false;
    };
} // namespace stalkermp::core

namespace stalkermp::detail
{
    // Access to the runtime-owned dispatcher for the engine bridge adapter.
    // Returns nullptr when the module is not initialized. Never used by
    // subsystems (they are wired at the composition root).
    [[nodiscard]] core::FrameDispatcher* GetModuleFrameDispatcher() noexcept;
} // namespace stalkermp::detail
