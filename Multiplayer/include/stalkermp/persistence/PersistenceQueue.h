// STALKER-MP — Persistence queue (Sprint-011, Step 7)
//
// A fixed-capacity FIFO ring of pending SaveRequests with hysteresis back-pressure.
// `Publish` appends a request (a full queue is a QueueFull value outcome, buffer
// unchanged); `Acquire` returns the front request without removing it; `Release`
// pops the front after it has been processed; `Retry` re-enqueues a failed request
// (bounded; counted). Back-pressure engages at the high watermark and releases at
// the low watermark. Storage is pre-reserved once at construction; steady-state
// operations perform NO dynamic allocation.
//
// This step introduces the queue ONLY — no storage, worker, scheduler, or manager.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PersistenceOutcome); bounded memory.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/persistence/PersistenceConfiguration.h" // PersistenceConfiguration
#include "stalkermp/persistence/PersistenceTypes.h"         // SaveRequest, PersistenceOutcome

namespace stalkermp::persistence
{
    class PersistenceQueue
    {
    public:
        // Capacity and watermarks come from the configuration (queueDepth >= 1;
        // low <= high <= queueDepth, validated by PersistenceConfiguration).
        explicit PersistenceQueue(const PersistenceConfiguration& config)
            : m_buffer(config.queueDepth == 0 ? 1 : config.queueDepth),
              m_highWatermark(config.backpressureHighWatermark),
              m_lowWatermark(config.backpressureLowWatermark)
        {
        }

        // Append a request. Full queue => QueueFull (buffer unchanged, overflow
        // counted). No allocation.
        [[nodiscard]] PersistenceOutcome Publish(const SaveRequest& request) noexcept
        {
            if (m_count >= m_buffer.size())
            {
                ++m_overflows;
                return PersistenceOutcome::QueueFull;
            }
            const std::size_t tail = (m_head + m_count) % m_buffer.size();
            m_buffer[tail] = request;
            ++m_count;
            ++m_published;
            TrackDepth();
            UpdateBackpressure();
            return PersistenceOutcome::Ok;
        }

        // The front request (FIFO), or nullptr when empty. Non-owning; valid until the
        // next Release/Clear.
        [[nodiscard]] const SaveRequest* Acquire() const noexcept
        {
            return m_count == 0 ? nullptr : &m_buffer[m_head];
        }

        // Remove the front request after processing. Benign when empty.
        void Release() noexcept
        {
            if (m_count == 0)
            {
                return;
            }
            m_head = (m_head + 1) % m_buffer.size();
            --m_count;
            UpdateBackpressure();
        }

        // Re-enqueue a failed request for another attempt (bounded; counted). Full
        // queue => QueueFull (unchanged).
        [[nodiscard]] PersistenceOutcome Retry(const SaveRequest& request) noexcept
        {
            if (m_count >= m_buffer.size())
            {
                ++m_overflows;
                return PersistenceOutcome::QueueFull;
            }
            const std::size_t tail = (m_head + m_count) % m_buffer.size();
            m_buffer[tail] = request;
            ++m_count;
            ++m_retries;
            TrackDepth();
            UpdateBackpressure();
            return PersistenceOutcome::Ok;
        }

        void Clear() noexcept
        {
            m_head = 0;
            m_count = 0;
            m_backpressured = false;
        }

        // Back-pressure state (hysteresis). When engaged, producers should slow/stop
        // publishing until it releases at the low watermark.
        [[nodiscard]] bool IsBackpressured() const noexcept { return m_backpressured; }

        [[nodiscard]] bool Empty() const noexcept { return m_count == 0; }
        [[nodiscard]] bool Full() const noexcept { return m_count >= m_buffer.size(); }
        [[nodiscard]] std::size_t Size() const noexcept { return m_count; }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_buffer.size(); }

        // Read-only statistics.
        [[nodiscard]] std::uint64_t PublishedCount() const noexcept { return m_published; }
        [[nodiscard]] std::uint64_t OverflowCount() const noexcept { return m_overflows; }
        [[nodiscard]] std::uint64_t RetryCount() const noexcept { return m_retries; }
        [[nodiscard]] std::uint32_t MaxDepth() const noexcept { return m_maxDepth; }

    private:
        void TrackDepth() noexcept
        {
            const std::uint32_t depth = static_cast<std::uint32_t>(m_count);
            if (depth > m_maxDepth)
            {
                m_maxDepth = depth;
            }
        }

        void UpdateBackpressure() noexcept
        {
            // High watermark 0 disables back-pressure entirely.
            if (m_highWatermark == 0)
            {
                m_backpressured = false;
                return;
            }
            if (!m_backpressured && m_count >= m_highWatermark)
            {
                m_backpressured = true;
            }
            else if (m_backpressured && m_count <= m_lowWatermark)
            {
                m_backpressured = false;
            }
        }

        std::vector<SaveRequest> m_buffer; // fixed after construction (ring storage)
        std::size_t m_head = 0;
        std::size_t m_count = 0;
        std::uint32_t m_highWatermark = 0;
        std::uint32_t m_lowWatermark = 0;
        bool m_backpressured = false;

        std::uint64_t m_published = 0;
        std::uint64_t m_overflows = 0;
        std::uint64_t m_retries = 0;
        std::uint32_t m_maxDepth = 0;
    };
} // namespace stalkermp::persistence
