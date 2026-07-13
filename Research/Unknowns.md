# ALife Simulation Unknowns & Host Authority Analysis

This document details systems managed by scripts rather than the engine, and evaluates host authority in multiplayer contexts.

---

## 1. Host Authority: Multiple Remote Players vs. Local Actor

### Can ALife run correctly with multiple remote players out of the box?
**No.** The engine's online/offline switching algorithms are hardcoded to calculate distances relative to a single actor reference: `alife().graph().actor()`.

#### Verification Details:
* **Distance Checks**:
  - `CSE_ALifeDynamicObject::try_switch_online()` checks distance specifically against `alife().graph().actor()->o_Position` ([alife_dynamic_object.cpp:L160](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_dynamic_object.cpp#L160)).
  - `CSE_ALifeDynamicObject::try_switch_offline()` also references only `alife().graph().actor()->o_Position` ([alife_dynamic_object.cpp:L180](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_dynamic_object.cpp#L180)).
* **Implication**:
  If multiple players exist, only the player designated as the server's actor (or local client player) will trigger online spawning. Remote clients will wander through offline regions where entities are simulated only as low-fidelity data updates.

---

## 2. Script-Owned / Unknown Systems (NOT FOUND IN SOURCE)

The following systems are not defined in C++ inside the `xray-monolith` engine codebase and are managed entirely by the Lua script environment:

* **Faction Warfare & Squad Raid Generation**:
  - **Status**: **NOT FOUND IN SOURCE**.
  - **Detail**: Commented out UI elements (`CUIFactionWarWnd` in `UIPdaWnd.cpp`) confirm that faction campaigns are not compiled in the engine. All faction warfare mechanics, territory control checks, and squad raid triggers are script-based (e.g., `sim_faction.script`, `sim_board.script`).
* **Camp Ownership & Smart Terrain Jobs**:
  - **Status**: **NOT FOUND IN SOURCE**.
  - **Detail**: The C++ engine code only provides spatial structures (`CALifeSmartTerrainTask`, `CSE_ALifeSmartZone`) and basic registration bindings (`register_npc`, `unregister_npc`). The selection of jobs, matching of NPC profiles to roles, and camp sleep/work states are handled by Lua scripts (e.g., `smart_terrain.script`).
