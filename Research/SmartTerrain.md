# Smart Terrain Architecture

This document details the smart terrain registry, task interfaces, and NPC assignment mechanisms.

---

## 1. Smart Terrain Registry & Representation

* **Representation**:
  Smart terrains are represented by the server entity class `CSE_ALifeSmartZone` ([alife_smart_terrain_registry.cpp:L19](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_smart_terrain_registry.cpp#L19)).
* **Registry Structure**:
  Managed by `CALifeSmartTerrainRegistry` ([alife_smart_terrain_registry.h:L16](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_smart_terrain_registry.h#L16)), storing smart zones inside a map:
  `typedef xr_map<ALife::_OBJECT_ID, CSE_ALifeSmartZone*> OBJECTS;` (Line 19).
* **Owner & Updates**:
  `CALifeSmartTerrainRegistry` is owned by `CALifeSimulatorBase` ([alife_simulator_base.h:L53](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator_base.h#L53)).

---

## 2. Job Assignment & NPC Allocation

* **NPC Association**:
  Stalkers and monsters keep their active smart terrain ID in the variable `m_smart_terrain_id` ([xrServer_Objects_ALife_Monsters.h:L249](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrServerEntities/xrServer_Objects_ALife_Monsters.h#L249)).
* **Task Target**:
  Jobs/positions are defined by `CALifeSmartTerrainTask` ([alife_smart_terrain_task.h:L16](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_smart_terrain_task.h#L16)), which points to a specific patrol path or coordinate position ([alife_smart_terrain_task.cpp:L46](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_smart_terrain_task.cpp#L46)).
* **Evaluation & Selection Loop**:
  NPCs evaluate and choose smart terrains during their update cycles inside `CALifeMonsterBrain::select_smart_terrain()` ([alife_monster_brain.cpp:L134](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_monster_brain.cpp#L134)). This assigns the ID, registers the NPC (`smart_terrain().register_npc(&object())`), and fetches the target patrol coordinates.

---

## 3. camp Ownership & Lua Control

* **Logic Ownership**:
  While the engine provides the base entity, the job matching algorithm, camp states, and actual role-switching logic are completely implemented in **Lua scripts** (specifically, `smart_terrain.script`, `se_smart_terrain.script`, `xr_camper.script`).
* **Engine Script Bindings**:
  The engine binds methods like `register_npc`, `unregister_npc`, and `task` to Lua ([xrServer_Objects_ALife_Monsters_script4.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrServerEntities/xrServer_Objects_ALife_Monsters_script4.cpp)), giving the script layer authority to assign task coordinates to NPCs.
