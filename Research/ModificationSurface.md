# Engine Modification Surface Analysis

This document details the architectural impact analysis for transforming the single-player ALife simulation into a shared multiplayer ALife simulation.

---

## 1. Subsystem Impact Classification

For each subsystem, we evaluate the modification surface based on verified code locations:

### `CALifeGraphRegistry`
* **Current Responsibility**: Tracks spatial coordinates and level nodes of entities, including the single player actor pointer.
* **Why it assumes one actor**: Stores `m_actor` as a single pointer, returned by `actor() const` ([alife_graph_registry.h:L80](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_graph_registry.h#L80)).
* **Classification**: **MAJOR MODIFICATION**
* **Risk**: High risk of breaking graph routing and node verification logic if actor references return null or a specific client's entity.
* **Dependencies**: `CALifeSwitchManager`, `CALifeUpdateManager`, `CALifeStoryRegistry`.

### `CALifeSwitchManager`
* **Current Responsibility**: Triggers online/offline transitions for dynamic objects.
* **Why it assumes one actor**: Computes distances specifically against a single `actor()->o_Position` ([alife_dynamic_object.cpp:L160](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_dynamic_object.cpp#L160)).
* **Classification**: **MAJOR MODIFICATION**
* **Risk**: High risk of frame stutter and memory leaks if multiple player bubbles overlap or toggle entities rapidly.
* **Dependencies**: `CALifeGraphRegistry`, `CALifeObjectRegistry`, `scheduled()`.

### `CALifeUpdateManager`
* **Current Responsibility**: Coordinates simulation ticks and handles level changes.
* **Why it assumes one actor**: Actor level switching saves state variables to a single network packet and forces an autosave cycle ([alife_update_manager.cpp:L159](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L159)).
* **Classification**: **COMPLETE REDESIGN**
* **Risk**: Extremely high risk. The single-save game loop is structurally incompatible with asynchronous player map transitions.
* **Dependencies**: `CALifeGraphRegistry`, `Level()`.

### `CALifeObjectRegistry`
* **Current Responsibility**: Holds the master map of all simulated entities.
* **Why it assumes one actor**: Keyed by `ALife::_OBJECT_ID` ([alife_object_registry.h:L22](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_object_registry.h#L22)).
* **Classification**: **UNCHANGED**
* **Risk**: Low.
* **Dependencies**: None.

### Story Registry
* **Current Responsibility**: Indexes quest-critical story entities.
* **Why it assumes one actor**: Bypasses proximity sweeps assuming a single-player progression timeline.
* **Classification**: **MINOR MODIFICATION**
* **Risk**: Low.
* **Dependencies**: None.

### Smart Terrain Registry
* **Current Responsibility**: Tracks smart zones and coordinates NPC job allocation.
* **Why it assumes one actor**: Evaluation of paths does not explicitly assume a single player, but player proximity controls active job rendering.
* **Classification**: **MINOR MODIFICATION**
* **Risk**: Medium. NPC jobs may flicker if multiple players approach.
* **Dependencies**: `CALifeGraphRegistry`.

### Level Streaming
* **Current Responsibility**: Loads and unloads map sectors dynamically.
* **Why it assumes one actor**: Proximity loading is actor-driven.
* **Classification**: **MAJOR MODIFICATION**
* **Risk**: High. Sector streaming must account for multiple cameras and listener positions.
* **Dependencies**: `CALifeGraphRegistry`.

### Save / Load
* **Current Responsibility**: Serializes game state database to disk.
* **Why it assumes one actor**: Encodes actor state variables directly into a single player profile block.
* **Classification**: **COMPLETE REDESIGN**
* **Risk**: High. Needs to store multiple player profiles and coordinate asynchronously.
* **Dependencies**: `CALifeUpdateManager`.

### Actor Registry
* **Current Responsibility**: Holds the active actor object.
* **Why it assumes one actor**: `Actor()` macro resolves to a single pointer.
* **Classification**: **MAJOR MODIFICATION**
* **Risk**: Very high. Almost every gameplay file references the global actor object.
* **Dependencies**: Most game systems.

### Legacy Multiplayer
* **Current Responsibility**: Low-level message packets and client-server sync.
* **Why it assumes one actor**: Disables ALife entirely when running multiplayer gamemodes ([game_sv_single.cpp:L32](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/game_sv_single.cpp#L32)).
* **Classification**: **COMPLETE REDESIGN**
* **Risk**: High. DirectPlay 8 transport layer is obsolete.
* **Dependencies**: `xrNetServer`.

### Inventory / Weapons / Animation / Physics / Quest / Faction / Scheduler / Renderer / HUD / Input
* **Inventory / Weapons / Animation / Physics**: **MINOR MODIFICATION** to synchronize parent structures.
* **Quest System / Faction Simulation**: **MINOR MODIFICATION** to evaluate relationships for specific player IDs.
* **Scheduler**: **UNCHANGED** (ticks normally on helper thread).
* **Renderer / HUD / Input**: **MINOR MODIFICATION** to detach local HUD viewport from remote player updates.

---

## 2. Modification Surface Summary Table

| Subsystem | Current Responsibility | Modification Level | Primary Risk | Blocking Issues |
|---|---|---|---|---|
| **CALifeUpdateManager** | Game Tick & Level Transitions | **COMPLETE REDESIGN** | Save game corruption on transitions | Single-player level loading cycle |
| **Actor Registry** | Player entity lookup | **MAJOR MODIFICATION** | Null pointer crashes in visual/weapon systems | Actor macro hardcoded globally |
| **CALifeGraphRegistry**| Tracks player position and nodes | **MAJOR MODIFICATION** | Game graph desynchronization | Single actor pointer (`m_actor`) |
| **CALifeSwitchManager**| Spawns/despawns online objects | **MAJOR MODIFICATION** | Stuttering on bubble overlaps | Proximity checks use one actor |
| **Level Streaming** | Map segment loading | **MAJOR MODIFICATION** | Out of memory / missing geometry | Sector loading assumes single viewport |
| **Save / Load** | Database persistence | **COMPLETE REDESIGN** | Save file corruption | Single-player state profile serialization |
| **Legacy Multiplayer** | Network package transport | **COMPLETE REDESIGN** | Network lag / obsolete APIs | Obsolete DirectPlay 8 APIs |

---

## 3. Top 10 Engine Changes (Impact Ordering)

1. **Re-architect `Actor()` Global Lookups**
   * *Why it exists*: Direct reference to single player pointer is embedded in almost all weapon, UI, and event code.
   * *System Dependencies*: Weapons, Inventory, HUD, Quests.
   * *Research Verification*: [ActorInput.cpp:L43](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ActorInput.cpp#L43) (`CActor::IR_OnKeyboardPress`).
   * *Unknowns*: Impact on visual update paths of distant entities when referencing multiple actor viewports.

2. **Decouple Proximity Checks from Single Actor Pointer**
   * *Why it exists*: Proximity calculations in `try_switch_online` / `try_switch_offline` evaluate distance to a single actor.
   * *System Dependencies*: `CALifeSwitchManager`, `scheduled()`.
   * *Research Verification*: [alife_dynamic_object.cpp:L160](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_dynamic_object.cpp#L160).
   * *Unknowns*: Performance impact on grid sweeps when evaluating $N \cdot M$ player distance arrays.

3. **Redesign Level Switch Save/Reload Phase**
   * *Why it exists*: Single-player mode reloads the server state on map changes.
   * *System Dependencies*: `CALifeUpdateManager`, Save/Load.
   * *Research Verification*: [alife_update_manager.cpp:L159](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L159).
   * *Unknowns*: Handling of offline entities traveling across levels while players are on different maps.

4. **Multi-Player Graph Registry Support**
   * *Why it exists*: `CALifeGraphRegistry` holds only one `m_actor` pointer.
   * *System Dependencies*: `CALifeGraphRegistry`, `CALifeSwitchManager`.
   * *Research Verification*: [alife_graph_registry.h:L54](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_graph_registry.h#L54).
   * *Unknowns*: Synchronization of map coordinates when actors are in separate zone sectors.

5. **Transport Layer Replacement (Obsoleting DirectPlay 8)**
   * *Why it exists*: Low-level network wrapper is hardlocked to legacy Microsoft DirectPlay 8.
   * *System Dependencies*: `IPureClient`, `IPureServer`.
   * *Research Verification*: [NET_Client.cpp:L459](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrNetServer/NET_Client.cpp#L459).
   * *Unknowns*: Bandwidth requirements for full physics state sync with high entity density.

6. **Rewrite Game Save Profile Serialization**
   * *Why it exists*: Serialization writes a single actor state block.
   * *System Dependencies*: Save/Load.
   * *Research Verification*: [alife_update_manager.cpp:L225](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L225).
   * *Unknowns*: Conflict resolution when players disconnect mid-save.

7. **Multi-Viewport Sector Streaming**
   * *Why it exists*: Sector/level geometry loads dynamically relative to the local viewport.
   * *System Dependencies*: Level Streaming, Renderer.
   * *Research Verification*: [alife_graph_registry.cpp:L68](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_graph_registry.cpp#L68).
   * *Unknowns*: Video memory budget constraints with disparate viewports.

8. **Lua Script-Side Community Lookups**
   * *Why it exists*: Faction relations and rankings are script-evaluated using player community hooks.
   * *System Dependencies*: Faction Simulation.
   * *Research Verification*: [Actor.cpp:L1425](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L1425).
   * *Unknowns*: Script state updates for multiple active quest profiles.

9. **Smart Terrain Job Proximity Flickering**
   * *Why it exists*: Proximity evaluates online NPC assignments.
   * *System Dependencies*: Smart Terrain Registry.
   * *Research Verification*: [alife_monster_brain.cpp:L134](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_monster_brain.cpp#L134).
   * *Unknowns*: Job state updates when two players interact with the same camp.

10. **Dynamic ID Generation for Reconnecting Clients**
    * *Why it exists*: Player actor ID is hardlocked to `ID = 0` ([alife_simulator_base.cpp:L207](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator_base.cpp#L207)).
    * *System Dependencies*: `xrServer`, `CALifeSimulatorBase`.
    * *Research Verification*: [alife_simulator_base.cpp:L207](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator_base.cpp#L207).
    * *Unknowns*: Dynamic reassignment of entity ID 0 to maintain compatibility with script expectations.
