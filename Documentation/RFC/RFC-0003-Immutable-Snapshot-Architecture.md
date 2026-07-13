# RFC-0003: Immutable Snapshot Architecture
# RFC-0003: Immutable Snapshot Architecture

| Field | Value |
|--------|-------|
| RFC | 0003 |
| Title | Immutable Snapshot Architecture |
| Status | Accepted |
| Author | STALKER-MP Project |
| Created | July 2026 |
| Depends On | RFC-0001, RFC-0002, 04_World.md, 07_Replication.md, 08_Persistence.md, 09_Threading.md |
| Supersedes | None |
| Superseded By | None |

---

# 1. Abstract

This RFC defines the immutable snapshot architecture used throughout STALKER-MP.

Rather than allowing multiple subsystems to directly access or modify the authoritative simulation, the Simulation Thread periodically produces immutable snapshots of world state. These snapshots are consumed asynchronously by worker systems such as replication, persistence, diagnostics, replay recording, and future tooling.

Snapshots are read-only representations of authoritative state. Once created, they are never modified.

This design eliminates shared mutable state between threads, preserves deterministic simulation, minimizes synchronization overhead, and establishes a consistent data exchange mechanism across the entire engine.

This RFC serves as the foundation for the Replication Pipeline (RFC-0004), Persistence Architecture (RFC-0005), and Threading Execution Model (RFC-0006).

---

# 2. Motivation

The original X-Ray Engine assumes a single-threaded simulation.

Modern multiplayer functionality introduces several asynchronous consumers:

- Network replication
- Save serialization
- Replay recording
- Telemetry
- Diagnostics
- Future plugins

Allowing these systems to directly inspect or modify the live simulation introduces several problems:

- Data races
- Lock contention
- Inconsistent reads
- Partial serialization
- Deadlocks
- Reduced simulation performance

A mechanism is required to safely expose world state without compromising the authority of the Simulation Thread.

---

# 3. Problem Statement

Multiple subsystems require access to simulation state.

Examples include:

- Replication workers building outbound packets.
- Persistence workers writing save files.
- Debug tools inspecting entity state.
- Replay systems recording world history.

If these systems access live simulation memory directly, they may observe partially updated state or block the Simulation Thread while reading.

Conversely, allowing worker systems to modify simulation data would violate the ownership model established in RFC-0001.

The project therefore requires a safe, deterministic, and scalable mechanism for sharing world state.

---

# 4. Goals

The snapshot architecture must:

- Preserve host authority.
- Preserve deterministic simulation.
- Eliminate shared mutable state.
- Avoid locks during normal simulation.
- Support asynchronous workers.
- Scale to future subsystems.
- Minimize synchronization overhead.

---

# 5. Non-Goals

This RFC does not:

- Replace entity storage.
- Replace replication protocols.
- Define serialization formats.
- Introduce distributed simulation.
- Define network packet layouts.
- Define save file structures.

Those topics are covered by later RFCs.

---

# 6. Decision

The Simulation Thread is the only subsystem permitted to access and modify live world state.

Whenever external systems require simulation data, the Simulation Thread produces immutable snapshots.

Workers consume snapshots independently.

Workers never modify snapshots.

Workers never access live simulation objects.

Snapshots are discarded after consumption.

---

# 7. Snapshot Ownership

Snapshot production belongs exclusively to the Simulation Thread.

Snapshot consumption belongs to worker systems.

Workers include:

- Replication
- Persistence
- Replay
- Debugging
- Analytics
- Future extensions

Ownership of authoritative state never leaves the Simulation Thread.

---

# 8. Snapshot Characteristics

Every snapshot must satisfy the following properties.

## Immutable

After construction, no field may be modified.

---

## Self-Contained

Consumers must not require access to live engine objects.

All required information must exist inside the snapshot.

---

## Consistent

All data represents one completed simulation tick.

Snapshots never represent partially updated simulation.

---

## Disposable

Snapshots are temporary transport objects.

They are recreated each update cycle.

---

## Read-Only

Workers may inspect snapshot contents but never alter them.

---

# 9. Design Rationale

## Preserve Simulation Authority

The Simulation Thread remains the single owner of gameplay state.

Snapshots expose state without transferring ownership.

---

## Lock-Free Workers

Workers never acquire simulation locks.

Instead, they operate entirely on immutable data.

This minimizes latency and prevents simulation stalls.

---

## Consistent Serialization

Persistence always serializes complete world states.

No partially updated entity can appear inside a save.

---

## Deterministic Networking

Replication always transmits a consistent simulation frame.

Clients never receive mixed state originating from different simulation ticks.

---

## Future Scalability

Additional worker systems require no changes to simulation ownership.

New consumers simply subscribe to snapshot production.

---

# 10. Snapshot Types

Although sharing the same architectural principles, different systems consume specialized snapshots.

## Replication Snapshot

Purpose:

Transmit authoritative state to connected clients.

Consumer:

Replication Worker.

---

## Persistence Snapshot

Purpose:

Serialize world state for save games.

Consumer:

Persistence Worker.

---

## Thread Snapshot

Purpose:

Expose simulation state to asynchronous processing.

Consumers:

Worker Threads.

---

Future snapshot types may be introduced provided they preserve immutability.

---

# 11. Alternatives Considered

## Shared Mutable State

Rejected.

Reason:

Creates race conditions and lock contention.

---

## Global Simulation Lock

Rejected.

Reason:

Workers would stall simulation while reading.

Scalability becomes poor.

---

## Copy-On-Write Simulation

Rejected.

Reason:

High implementation complexity.

Large memory overhead.

Unnecessary given project requirements.

---

## Per-Subsystem Serialization

Rejected.

Reason:

Each subsystem would independently traverse live world state.

Creates duplicated work and inconsistent reads.

---

# 12. Consequences

Positive:

- Lock-free workers.
- Deterministic snapshots.
- Consistent serialization.
- Clean subsystem boundaries.
- Easy debugging.
- Scalable architecture.

Negative:

- Additional memory allocations.
- Snapshot construction cost.
- Duplicate transient data.

These costs are acceptable because snapshots exist only briefly and eliminate significantly more expensive synchronization mechanisms.

---

# 13. Interaction with Other Systems

## World

Produces authoritative state.

Never consumes snapshots.

---

## ALife

Executes normally.

Never observes snapshots.

---

## Replication

Consumes Replication Snapshots.

Defined in RFC-0004.

---

## Persistence

Consumes Persistence Snapshots.

Defined in RFC-0005.

---

## Threading

Defines worker execution using immutable snapshots.

Defined in RFC-0006.

---

## Plugins

Future plugins may consume snapshots but never modify them.

Defined in RFC-0007.

---

# 14. Risks

Potential risks include:

- Increased memory usage.
- Large snapshot construction cost.
- Excessive snapshot frequency.
- Snapshot version incompatibility.

Mitigation strategies include:

- Delta snapshots where appropriate.
- Memory pooling.
- Snapshot versioning.
- Efficient serialization layouts.

---

# 15. Acceptance Criteria

The Immutable Snapshot Architecture is considered correctly implemented when:

- Only the Simulation Thread modifies authoritative state.
- Workers never access live simulation objects.
- Snapshots are immutable after construction.
- Snapshot consumers require no simulation locks.
- Replication and persistence operate exclusively on snapshots.
- Simulation remains deterministic regardless of worker execution timing.

---

# 16. References

Architecture Documents

- 04_World.md
- 07_Replication.md
- 08_Persistence.md
- 09_Threading.md

Related RFCs

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System
- RFC-0004 — Replication Pipeline
- RFC-0005 — Persistence Architecture
- RFC-0006 — Threading Execution Model