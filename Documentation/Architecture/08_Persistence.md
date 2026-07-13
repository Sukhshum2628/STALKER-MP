# STALKER-MP Persistence Architecture

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, Gameplay Developers, Infrastructure Developers

---

# 1. Purpose

The Persistence subsystem is responsible for preserving the authoritative state of the Zone across simulation sessions.

Unlike the Replication subsystem, which synchronizes live state between the host and clients, Persistence ensures that simulation survives:

- server shutdowns
- crashes
- restarts
- player reconnects
- future dedicated server deployments

Persistence is therefore concerned with durability rather than communication.

---

# 2. Philosophy

The Zone is persistent.

Players are temporary.

The World continues to exist independently of player participation.

Persistence preserves that continuity.

This philosophy aligns directly with the core design principle established throughout STALKER-MP:

> **The World exists before players join and continues after they leave.**

---

# 3. Design Goals

The Persistence subsystem has several primary goals.

### Preserve the World

Every persistent component of the simulation should survive across sessions.

---

### Maintain Consistency

Saved state must represent a valid, internally consistent simulation.

---

### Support Recovery

The system should recover cleanly from interruptions whenever possible.

---

### Preserve Identity

Persistent entities retain their identity across saves and loads.

---

### Enable Future Growth

The architecture should support dedicated servers, migrations, and long-lived persistent worlds.

---

# 4. Architectural Position

Within STALKER-MP, Persistence occupies the following position.

```
                World
                  │
                  ▼
                ALife
                  │
                  ▼
             Gameplay
                  │
                  ▼
            Persistence
        ┌─────────┼─────────┐
        ▼         ▼         ▼
   Save Manager  Versioning  Recovery
        │         │         │
        └─────────┼─────────┘
                  ▼
              Storage
```

Persistence consumes authoritative simulation state.

It never creates gameplay state independently.

---

# 5. Responsibilities

The Persistence subsystem is responsible for:

- world saves
- world loading
- player persistence
- entity persistence
- save validation
- version management
- recovery
- migration
- storage coordination

Persistence is **not** responsible for:

- gameplay
- AI
- networking
- replication
- rendering
- transport
- session management

---

# 6. Internal Architecture

The subsystem consists of several cooperating components.

```
              Persistence

                     │

     ┌───────────────┼───────────────┐

     ▼               ▼               ▼

Save Manager    Version Manager   Recovery

     ▼               ▼               ▼

Serializer      Migration      Validation

                     ▼

                 Storage
```

Each component performs one clearly defined responsibility.

---

# 7. Save Manager

The Save Manager coordinates the creation of durable world snapshots.

Responsibilities include:

- save scheduling
- save coordination
- save lifecycle
- save metadata
- save completion

The Save Manager never performs gameplay simulation.

---

# 8. Version Manager

Persistent worlds evolve over time.

The Version Manager tracks:

- save format version
- schema evolution
- migration requirements
- compatibility rules

Versioning exists independently of gameplay.

---

# 9. Recovery Manager

The Recovery Manager coordinates restoration after interruptions.

Responsibilities include:

- interrupted saves
- validation failures
- crash recovery
- recovery diagnostics
- integrity verification

Recovery should always preserve the most recent valid world state.

---

# 10. Core Principles

The Persistence subsystem follows several fundamental principles.

---

## World First

The World is always saved before gameplay resumes after loading.

---

## Durability

Committed saves must survive process termination.

---

## Consistency

Partial world states are never considered valid saves.

---

## Explicit Ownership

Persistence never invents state.

It stores only authoritative information.

---

## Simulation Independence

Saving the world never changes simulation behaviour.

---

## Future Compatibility

Save formats are expected to evolve throughout the lifetime of the project.

---

# 11. Save Architecture

## 11.1 Purpose

The Save Architecture defines how the authoritative simulation is transformed into durable persistent data.

Rather than serializing arbitrary engine memory, the Persistence subsystem constructs a consistent representation of the World that can later be reconstructed regardless of process lifetime.

The Save Architecture therefore serves as the bridge between runtime simulation and long-term storage.

---

## 11.2 Design Philosophy

Saving the World should never depend upon implementation details such as object memory layouts or pointer addresses.

Instead, persistence operates on logical simulation state.

Conceptually:

```
Authoritative World

↓

Persistence Snapshot

↓

Serialization

↓

Storage

↓

Deserializer

↓

World Reconstruction
```

The saved world represents *what exists*, not *how it exists in memory*.

---

## 11.3 Save Boundaries

A save captures a complete and internally consistent simulation state.

A save must never contain:

- partially updated entities
- incomplete scheduler execution
- mixed simulation ticks
- inconsistent ownership

Every save corresponds to one completed simulation state.

---

## 11.4 Save Pipeline

```
Simulation Tick Complete

↓

Freeze Save State

↓

Collect World

↓

Collect Entities

↓

Collect Players

↓

Collect ALife

↓

Serialize

↓

Integrity Verification

↓

Commit Storage
```

Only after successful verification does the save become authoritative.

---

## 11.5 Save Snapshot

The Persistence subsystem constructs an immutable persistence snapshot.

Unlike replication snapshots, persistence snapshots are optimized for durability rather than transmission.

Typical contents include:

### World

- global time
- weather
- emission state
- environmental variables
- active levels

---

### Players

- identity
- inventory
- position
- health
- quests
- reputation

---

### Entities

- persistent identifiers
- transforms
- ownership
- simulation state
- inventories

---

### ALife

- scheduler metadata
- Smart Terrain occupancy
- faction planner state
- ecology state
- artifact lifecycle

The snapshot represents the complete persistent state of the Zone.

---

## 11.6 Save Lifecycle

Every save follows the same lifecycle.

```
Save Requested

↓

Snapshot Created

↓

Serialization

↓

Validation

↓

Storage Commit

↓

Metadata Update

↓

Save Complete
```

The lifecycle is intentionally linear and deterministic.

---

## 11.7 Atomic Saves

Persistence should be treated as an atomic operation.

Either:

```
Entire Save Valid
```

or

```
Entire Save Discarded
```

Partial saves are never considered recoverable world states.

---

## 11.8 Save Metadata

Every save should contain metadata describing:

- save format version
- world version
- creation timestamp
- simulation tick
- active level information
- save identifier

Metadata allows validation before full deserialization begins.

---

## 11.9 Save Invariants

Every save guarantees:

- one authoritative world state
- deterministic ordering
- explicit ownership
- complete entity identity
- valid references
- internally consistent data

These guarantees simplify recovery and future migrations.

---

# 12. World Persistence

## 12.1 Purpose

The World is the primary persistence boundary.

Gameplay systems persist through the World rather than writing independently to storage.

This preserves architectural ownership.

---

## 12.2 Persistent World State

The World persists:

- global clock
- weather
- emission status
- environmental systems
- level topology
- world identifiers
- active simulation regions

The World remains the root of every save.

---

## 12.3 World Reconstruction

Loading follows the reverse pipeline.

```
Storage

↓

Deserialize

↓

Create World

↓

Initialize Levels

↓

Register Entities

↓

Initialize ALife

↓

Resume Simulation
```

The World must exist before any gameplay systems initialize.

---

## 12.4 Persistence Contract

Gameplay systems never load themselves independently.

Instead:

```
World

↓

Gameplay System

↓

Restore State

↓

Simulation Ready
```

The World orchestrates initialization.

---

# 13. Entity Persistence

## 13.1 Purpose

Every persistent entity within the Zone must survive save and load operations.

Entity persistence preserves identity rather than memory.

Objects are reconstructed rather than restored by pointer.

---

## 13.2 Persistent Identity

Each entity possesses a globally unique persistent identifier.

Identifiers remain unchanged throughout the lifetime of the entity.

Pointers are considered runtime implementation details and are never persisted.

---

## 13.3 Persistent State

Typical entity data includes:

- identifier
- class type
- position
- orientation
- inventory
- ownership
- health
- simulation state
- relationships

Only authoritative data is stored.

---

## 13.4 Entity Reconstruction

```
Entity Record

↓

Allocate Runtime Object

↓

Assign Identifier

↓

Restore Persistent State

↓

Register World

↓

Register ALife

↓

Simulation Ready
```

Reconstruction occurs deterministically.

---

## 13.5 Reference Integrity

Entities frequently reference one another.

Examples include:

- squad membership
- inventory ownership
- Smart Terrain assignment
- quest targets

References should resolve through persistent identifiers rather than runtime memory addresses.

---

## 13.6 Persistence Guarantees

Entity persistence guarantees:

- identity preservation
- reference integrity
- deterministic reconstruction
- explicit ownership
- compatibility with future migrations

---

# 14. Player Persistence

## 14.1 Purpose

Players participate within the persistent World.

Their characters therefore remain persistent even though their network sessions are temporary.

---

## 14.2 Persistent Components

Player persistence includes:

- identity
- inventory
- equipment
- reputation
- quests
- faction
- statistics
- location

Connection information is intentionally excluded.

---

## 14.3 Player Lifecycle

```
Character Exists

↓

Player Connects

↓

Gameplay

↓

Player Disconnects

↓

Character Saved

↓

Reconnect

↓

Character Restored
```

The character survives independently of the session.

---

## 14.4 Persistent Identity

A player's persistent identity remains stable across:

- reconnects
- server restarts
- save/load cycles

This identity serves as the authoritative reference for character reconstruction.

---

## 14.5 Join Integration

When a player reconnects:

```
Load World

↓

Locate Character

↓

Restore Inventory

↓

Restore Position

↓

Restore Quests

↓

Create Session

↓

Gameplay
```

World restoration always precedes session creation.

---

# 15. ALife Persistence

## 15.1 Purpose

The defining feature of STALKER-MP is that the Zone continues evolving independently of player activity.

Preserving this behaviour across save and load operations requires more than storing entity positions.

The Persistence subsystem must preserve the complete simulation context of ALife.

When the world is restored, the simulation should continue naturally rather than restarting from an artificial initial state.

---

## 15.2 Design Philosophy

Persistence should restore **simulation continuity**, not merely object placement.

For example, restoring an NPC should also preserve:

- current objective
- Smart Terrain assignment
- faction relationships
- squad membership
- ongoing tasks
- simulation state

The objective is to make loading indistinguishable from uninterrupted execution.

---

## 15.3 Persistent ALife Components

The following components participate in persistence.

### Scheduler

- simulation tick
- scheduler metadata
- pending execution state
- scheduling statistics (optional)

---

### Smart Terrain

- occupied locations
- assigned NPCs
- population state
- active jobs

---

### Faction Planner

- strategic objectives
- territory ownership
- reinforcement plans
- hostility relationships

---

### Ecology

- mutant migration
- ecosystem state
- population metadata

---

### Artifact System

- artifact locations
- generation timers
- anomaly associations

---

### Story Integration

- active objectives
- completed objectives
- story variables
- narrative state

Collectively these systems define the living behaviour of the Zone.

---

## 15.4 Restoration Pipeline

```
World Loaded

↓

Entities Registered

↓

Restore Smart Terrain

↓

Restore Scheduler

↓

Restore Faction Planner

↓

Restore Story

↓

Restore Ecology

↓

Resume Simulation
```

Ordering is critical.

Higher-level planners initialize before simulation resumes.

---

## 15.5 Scheduler Restoration

The Scheduler should resume from a valid simulation boundary.

It must never resume in the middle of a partially executed tick.

Restoration therefore begins immediately before the next scheduled simulation cycle.

---

## 15.6 Continuity Guarantees

After loading:

- NPCs continue assigned tasks.
- Squads retain objectives.
- Smart Terrains remain populated.
- Faction campaigns continue.
- Story progression remains valid.
- Artifact generation resumes naturally.

The player should perceive the Zone as uninterrupted.

---

# 16. Save Versioning

## 16.1 Purpose

Persistence formats evolve throughout development.

The architecture therefore treats versioning as a first-class concern rather than an afterthought.

Every save explicitly declares the format required for successful restoration.

---

## 16.2 Version Metadata

Each save records:

- persistence version
- engine version
- world schema version
- ALife schema version
- creation timestamp

Version metadata is evaluated before deserialization begins.

---

## 16.3 Compatibility

The Version Manager determines whether a save is:

- fully compatible
- migratable
- unsupported

This decision occurs before allocating gameplay objects.

---

## 16.4 Migration

When supported:

```
Old Save

↓

Migration Pipeline

↓

Current Schema

↓

Validation

↓

Load
```

Migration occurs only during loading.

Runtime simulation never performs schema conversion.

---

## 16.5 Versioning Principles

The architecture favors:

- forward evolution
- explicit schema changes
- deterministic migration
- recoverable failures

Breaking changes should remain exceptional.

---

# 17. Autosave Strategy

## 17.1 Philosophy

Autosaves improve durability without compromising simulation performance.

Saving should become a routine background operation rather than a disruptive gameplay event.

---

## 17.2 Autosave Triggers

Typical triggers include:

- configurable time intervals
- level transitions
- administrative commands
- major story milestones
- controlled server shutdown

The precise policy remains configurable.

---

## 17.3 Autosave Pipeline

```
Trigger

↓

Verify Safe Save Point

↓

Create Snapshot

↓

Serialize

↓

Validate

↓

Commit

↓

Rotate Previous Saves
```

Autosaves follow the same integrity guarantees as manual saves.

---

## 17.4 Safe Save Points

Persistence should occur only after a completed simulation tick.

Unsafe examples include:

- partially processed scheduler queues
- incomplete entity transitions
- active serialization
- unfinished world initialization

Saving during these phases may produce inconsistent state.

---

## 17.5 Save Rotation

The architecture recommends maintaining multiple generations of saves.

Example:

```
Current

↓

Previous

↓

Archive
```

This provides recovery options following corruption or unexpected interruption.

---

# 18. Recovery Model

## 18.1 Philosophy

Recovery is based upon the most recent valid persistent world.

The architecture prioritizes integrity over recency.

A slightly older valid world is preferable to a corrupted newer save.

---

## 18.2 Recovery Pipeline

```
Startup

↓

Locate Latest Save

↓

Validate

↓

Integrity Passes?

     │
 ┌───┴────┐
 │        │
 ▼        ▼
Yes       No
 │        │
 ▼        ▼
Load    Previous Save
```

The World should never load unverified persistence data.

---

## 18.3 Integrity Verification

Typical validation includes:

- schema compatibility
- entity references
- identifier uniqueness
- world metadata
- checksum verification
- required subsystem data

Validation occurs before gameplay initialization.

---

## 18.4 Crash Recovery

If a crash occurs during saving:

```
Interrupted Save

↓

Discard Partial Save

↓

Retain Previous Valid Save

↓

Restart Recovery
```

Incomplete saves never replace valid persistence.

---

# 19. Dedicated Server Persistence

## 19.1 Purpose

Version 1 uses a host-authoritative architecture.

However, the Persistence subsystem is intentionally designed to operate unchanged within future dedicated servers.

---

## 19.2 Architectural Model

```
Dedicated Server

↓

Simulation

↓

Persistence

↓

Storage
```

Clients never participate in persistence.

---

## 19.3 Server Responsibilities

Dedicated servers own:

- world saves
- autosaves
- migrations
- integrity verification
- recovery

Clients remain temporary observers.

---

## 19.4 Long-Term Persistence

Persistent public servers may operate for weeks or months.

The Persistence architecture therefore assumes:

- repeated saves
- evolving ALife
- growing entity counts
- long-running story progression

Durability remains the primary objective.

---

# 20. Performance Strategy

## 20.1 Philosophy

Persistence should minimize disruption to simulation.

Save operations should consume predictable resources and avoid introducing unnecessary pauses.

---

## 20.2 Performance Goals

The subsystem aims to provide:

- bounded save times
- deterministic serialization
- efficient storage layout
- incremental optimization opportunities

Correctness remains the highest priority.

---

## 20.3 Optimization Principles

Preferred techniques include:

- immutable persistence snapshots
- sequential serialization
- efficient identifier lookup
- buffered storage
- memory reuse

Optimizations should never compromise save integrity.

---

# 21. Failure Recovery

## 21.1 Philosophy

Persistence failures should not corrupt the authoritative World.

Simulation should continue whenever possible, even if a save operation fails.

---

## 21.2 Save Failure

```
Save Requested

↓

Serialization Error

↓

Abort Save

↓

Retain Existing Save

↓

Continue Simulation
```

No partial save becomes authoritative.

---

## 21.3 Load Failure

If loading fails:

- terminate initialization
- preserve diagnostic information
- avoid partial world construction
- allow operator intervention

Half-loaded worlds are prohibited.

---

## 21.4 Corruption

If corruption is detected:

```
Corrupted Save

↓

Reject

↓

Attempt Recovery

↓

Fallback Save

↓

Initialize
```

Recovery favors correctness over convenience.

---

# 22. Future Extensions

Potential future capabilities include:

- incremental saves
- background save workers
- cloud-backed persistence
- database storage
- world snapshots
- replay integration
- persistence analytics
- distributed server persistence
- region-based saves

Future enhancements should preserve the existing World ownership model.

---

# 23. Design Rationale

The Persistence subsystem deliberately separates durability from gameplay.

Rather than allowing gameplay systems to write directly to storage, the World coordinates persistence through a consistent snapshot architecture.

This provides several architectural advantages.

### Consistency

The saved world always represents a valid simulation state.

---

### Explicit Ownership

Only authoritative systems contribute persistent data.

---

### Recoverability

Validation and versioning simplify long-term maintenance.

---

### Future Compatibility

Dedicated servers, migrations, and cloud persistence become evolutionary improvements rather than architectural redesigns.

---

# 24. Summary

The Persistence subsystem ensures that the authoritative Zone survives beyond individual gameplay sessions.

It coordinates durable world snapshots, entity reconstruction, player persistence, ALife continuity, version management, autosave scheduling, and recovery while remaining independent from networking and gameplay execution.

Its core principles are:

- The World is persistent.
- Players are temporary.
- ALife survives save and load.
- Persistence is authoritative.
- Save integrity precedes performance.
- Recovery favors correctness.

By preserving the complete state of the World rather than isolated gameplay objects, the Persistence subsystem enables STALKER-MP to maintain a living, evolving Zone across crashes, restarts, reconnects, and future dedicated server deployments.