# Story Objects & Quest Entity Protections

This document details how quest-critical entities and story NPCs are registered, protected, and excluded from standard dynamic simulation cleanup.

---

## 1. Story Registry Storage

* **Class Identifier**:
  Story objects are assigned a unique `ALife::_STORY_ID` ([xrServer_Objects_ALife.h:L139](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrServerEntities/xrServer_Objects_ALife.h#L139)).
* **Registry Database**:
  Managed by the simulator's registry `CALifeStoryRegistry` ([alife_story_registry.h:L15](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_story_registry.h#L15)), mapping IDs to object references via standard `STORY_P_MAP`.
* **Registration**:
  Story objects register themselves during server entity creation:
  `story_objects().add(object->m_story_id, object);` ([alife_simulator_base2.cpp:L32](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator_base2.cpp#L32)).

---

## 2. Quest Entity Protection & Simulation Exclusion

* **Despawn Immunity**:
  Standard dynamic ALife entities can be removed if they are flagged as redundant (`redundant()`) ([alife_switch_manager.cpp:L231](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_switch_manager.cpp#L231)). Story NPCs have specific configuration rules preventing them from ever returning true on redundancy checks.
* **Controlled Switching**:
  Lua scripts can override the online/offline switching behaviour by setting `can_switch_online` and `can_switch_offline` properties to force story NPCs to remain in the simulation ([xrServer_Objects_ALife_script.cpp:L54](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrServerEntities/xrServer_Objects_ALife_script.cpp#L54)).
* **Script-Driven Reference Preservation**:
  Quest systems and scripts query story entities via `alife():story_object(story_id)` ([alife_simulator_script.cpp:L78](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator_script.cpp#L78)). These lookups bypass normal proximity sweeps, ensuring they are protected from random cleanup routines.
