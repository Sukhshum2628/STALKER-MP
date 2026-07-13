# Level Streaming & Transition Lifecycle

This document traces the complete lifecycle of ALife dynamic objects transitioning between online, offline, and level-changing states.

---

## 1. Step-by-Step State Transition Lifecycle

### Step 1: Going Offline
1. The dynamic object moves outside `offline_distance` relative to the player actor ([alife_dynamic_object.cpp:L180](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_dynamic_object.cpp#L180)).
2. `try_switch_offline` triggers `switch_offline()` ([alife_switch_manager.cpp:L117](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_switch_manager.cpp#L117)).
3. `remove_online()` is called ([alife_switch_manager.cpp:L74](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_switch_manager.cpp#L74)):
   - Calls `server().Perform_destroy(object)` to completely delete the online client-side game entity representation (e.g., `CAI_Stalker`).
   - Exposes `add_offline` on the server entity (`CSE_ALifeDynamicObject`) to register it to the offline tick scheduler `scheduled().add(this)` and graph registry `graph().add()`.

### Step 2: Going Online
1. The player actor approaches within `online_distance` of the offline entity.
2. `try_switch_online()` triggers `switch_online()` ([alife_switch_manager.cpp:L106](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_switch_manager.cpp#L106)).
3. `add_online()` is called ([alife_switch_manager.cpp:L47](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_switch_manager.cpp#L47)):
   - Removes the server object from the offline update registry: `scheduled().remove(this)`.
   - Spawns the client-side entity representation via `server().Process_spawn()` (Line 60), instantiating the actual active game object class.

### Step 3: Changing Levels
1. The player actor interacts with a level transition zone, writing coordinate details to a transition network packet.
2. `CALifeUpdateManager::change_level(net_packet)` is invoked ([alife_update_manager.cpp:L159](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L159)):
   - Flushes input buffers via `Level().ClientSend()`.
   - Serializes actor target level coordinate offsets.
   - Commands client-side serialization via `Level().ClientSave()`.
   - Saves the entire ALife state to an autosave file via `save(autosave_name)` (Line 225).
3. The engine initiates a level reload using the generated autosave.
4. Upon load:
   - Entities on the old map are kept offline because their game graph level ID no longer matches the current level ID: `ai().game_graph().vertex(I->m_tGraphID)->level_id() != current_level_id`.
   - The player actor spawns on the new map, bringing entities on the new level online.

### Step 4: Returning Online
1. Proximity matching and level ID checks evaluate true, triggering `try_switch_online()` and spawning the entity back into the active game world.
