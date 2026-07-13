# ALife Simulator Core Architecture

This document details the core ALife simulation architecture verified inside the `xray-monolith` engine codebase.

---

## 1. CALifeSimulator Definition

* **Header**: [alife_simulator.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator.h#L18)
* **Source**: `alife_simulator.cpp`
* **Class Declaration**:
  ```cpp
  class CALifeSimulator :
      public CALifeUpdateManager,
      public CALifeInteractionManager
  ```
* **Constructor**: `CALifeSimulator::CALifeSimulator(xrServer* server, shared_str* command_line)` ([alife_simulator.cpp:L43](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator.cpp#L43))
* **Ownership**: Owned by the single-player server game state `game_sv_Single` via `m_alife_simulator` ([game_sv_single.h:L14](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/game_sv_single.h#L14)).
* **Initialization**: Instantiated during server game phase configuration inside `game_sv_Single::Create` if the `/alife` startup option is passed ([game_sv_single.cpp:L32](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/game_sv_single.cpp#L32)). Registers itself to the AI namespace space registry via `ai().set_alife(this)` ([alife_simulator.cpp:L50](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator.cpp#L50)).
* **Shutdown**: Deallocated in `game_sv_Single::~game_sv_Single()` using `delete_data(m_alife_simulator)` ([game_sv_single.cpp:L23](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/game_sv_single.cpp#L23)). Unregisters itself via `ai().set_alife(0)` ([alife_simulator.cpp:L95](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_simulator.cpp#L95)).

---

## 2. ALife Scheduler

* **Update Loop Hook**: 
  Updates are driven by the engine's scheduler. `CALifeUpdateManager` inherits from `ISheduled` ([alife_update_manager.h:L24](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.h#L24)).
* **Update Frequency**:
  Ticked via `CALifeUpdateManager::shedule_Update(u32 dt)` ([alife_update_manager.cpp:L119](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L119)). Its update priority scale is set to `.5f` ([alife_update_manager.cpp:L89](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L89)).
* **Threading Execution**:
  - If the multithreading flag `mtALife` is active (`g_mt_config.test(mtALife)`), the update tick is pushed to a parallel queue via `Device.seqParallel.push_back(...)` ([alife_update_manager.cpp:L128](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L128)) to run on the secondary worker thread.
  - If disabled, it executes synchronously on the main thread during `shedule_Update` via a direct call to `update()` ([alife_update_manager.cpp:L140](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L140)).
* **Tick Actions**:
  The `update()` method executes:
  1. `update_switch()`: Manages online/offline transition predicates ([alife_update_manager.cpp:L92](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L92)).
  2. `update_scheduled(false)`: Invokes the scheduled registry simulation tick via `scheduled().update()` ([alife_update_manager.cpp:L109](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/alife_update_manager.cpp#L109)).
