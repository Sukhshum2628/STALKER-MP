// STALKER-MP — Replication queues (Sprint-009, Step 8)
//
// Two independent, fixed-capacity, FIFO outgoing queues — one reliable, one
// unreliable — with capacity bounded by ReplicationConfiguration and overflow
// reported as a value outcome (ReplicationOutcome). Storage is pre-reserved once
// at construction; Enqueue/Dequeue/Clear perform NO dynamic allocation (a ring
// buffer flips indices). Pure value semantics; deterministic FIFO order within
// each queue (the consumer decides reliable-vs-unreliable ordering across queues,
// a later step). Records carry their §7.A classification (reliability/priority)
// for that later ordering.
//
// This step introduces the queues ONLY — no packet assembly, serialization,
// networking, worker/threading, or tick/service. Consumes Step-02
// ReplicationConfiguration, Step-06 ReplicationChangeSet, Step-07 classifier.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "stalkermp/replication/DeltaEncoder.h"             // ReplicationChangeSet
#include "stalkermp/replication/ReplicationClassifier.h"    // ReplicationChangeKind + Classify*
#include "stalkermp/replication/ReplicationConfiguration.h"
#include "stalkermp/replication/ReplicationTypes.h"

namespace stalkermp::replication
{
    // One queued outgoing record. Values only; carries the entity payload plus its
    // §7.A classification. For a removal, `entity.id` is the removed id and the
    // other payload fields are zero.
    struct QueuedRecord
    {
        ReplicationChangeKind kind = ReplicationChangeKind::None;
        EntityReplicationState entity{};
        ReplicationReliability reliability = ReplicationReliability::Unreliable;
        ReplicationPriority priority = ReplicationPriority::Low;
    };

    // A fixed-capacity FIFO ring buffer of QueuedRecord. No allocation after
    // construction; Overflow is a value outcome.
    class FixedRecordQueue
    {
    public:
        explicit FixedRecordQueue(std::size_t capacity) : m_buffer(capacity) {}

        // Append to the back. Returns Overflow (value outcome) when full — the
        // queue is unchanged. No allocation.
        [[nodiscard]] ReplicationOutcome Enqueue(const QueuedRecord& record) noexcept
        {
            if (m_count >= m_buffer.size())
            {
                return ReplicationOutcome::Overflow;
            }
            const std::size_t tail = (m_head + m_count) % m_buffer.size();
            m_buffer[tail] = record;
            ++m_count;
            return ReplicationOutcome::Ok;
        }

        // Remove and return the front (FIFO). nullopt when empty. No allocation.
        [[nodiscard]] std::optional<QueuedRecord> Dequeue() noexcept
        {
            if (m_count == 0)
            {
                return std::nullopt;
            }
            const QueuedRecord front = m_buffer[m_head];
            m_head = (m_head + 1) % m_buffer.size();
            --m_count;
            return front;
        }

        void Clear() noexcept
        {
            m_head = 0;
            m_count = 0;
        }

        [[nodiscard]] bool Empty() const noexcept { return m_count == 0; }
        [[nodiscard]] bool Full() const noexcept { return m_count >= m_buffer.size(); }
        [[nodiscard]] std::size_t Size() const noexcept { return m_count; }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_buffer.size(); }

    private:
        std::vector<QueuedRecord> m_buffer; // fixed after construction (ring storage)
        std::size_t m_head = 0;
        std::size_t m_count = 0;
    };

    // Named independent queues (distinct types for clarity at call sites).
    class ReliableQueue final : public FixedRecordQueue
    {
    public:
        using FixedRecordQueue::FixedRecordQueue;
    };
    class UnreliableQueue final : public FixedRecordQueue
    {
    public:
        using FixedRecordQueue::FixedRecordQueue;
    };

    // Owns the two independent queues; routes records by reliability and enforces
    // per-queue capacity budgets. Deterministic.
    class ReplicationQueues
    {
    public:
        explicit ReplicationQueues(const ReplicationConfiguration& config)
            : m_reliable(config.reliableQueueDepth), m_unreliable(config.unreliableQueueDepth)
        {
        }

        // Route one record to its queue by reliability. Overflow (value outcome)
        // when that queue is full.
        [[nodiscard]] ReplicationOutcome Enqueue(const QueuedRecord& record) noexcept
        {
            return record.reliability == ReplicationReliability::Reliable ? m_reliable.Enqueue(record)
                                                                          : m_unreliable.Enqueue(record);
        }

        // Classify each change in `changes` (§7.A) into a QueuedRecord and enqueue
        // it into the appropriate queue, in deterministic order: removed, added,
        // changed. Returns the first Overflow encountered (records that fit remain
        // queued); Ok when everything was enqueued.
        [[nodiscard]] ReplicationOutcome EnqueueChangeSet(const ReplicationChangeSet& changes) noexcept;

        [[nodiscard]] ReliableQueue& Reliable() noexcept { return m_reliable; }
        [[nodiscard]] UnreliableQueue& Unreliable() noexcept { return m_unreliable; }
        [[nodiscard]] const ReliableQueue& Reliable() const noexcept { return m_reliable; }
        [[nodiscard]] const UnreliableQueue& Unreliable() const noexcept { return m_unreliable; }

        void Clear() noexcept
        {
            m_reliable.Clear();
            m_unreliable.Clear();
        }
        [[nodiscard]] bool Empty() const noexcept { return m_reliable.Empty() && m_unreliable.Empty(); }
        [[nodiscard]] std::size_t Size() const noexcept { return m_reliable.Size() + m_unreliable.Size(); }

    private:
        [[nodiscard]] static QueuedRecord MakeRecord(ReplicationChangeKind kind, const EntityReplicationState& entity);

        ReliableQueue m_reliable;
        UnreliableQueue m_unreliable;
    };
} // namespace stalkermp::replication
