# Legacy Multiplayer Subsystem Reuse Analysis

This document classifies the legacy multiplayer subsystems inside the `xray-monolith` engine and evaluates their suitability for future reuse or required rewrite.

---

| Subsystem Name | Exists | Complete | Reusable | Needs Modification | Needs Rewrite | Risk / Dependency | Example Implementation References |
|---|---|---|---|---|---|---|---|
| **Networking Transport** | Yes | Yes | No | No | Yes (DirectPlay 8 is obsolete) | **High**: Hardlocked to obsolete Windows DirectPlay 8 APIs. | [NET_Client.h:L52](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrNetServer/NET_Client.h#L52) (`IDirectPlay8Client`), [NET_Server.h:L167](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrNetServer/NET_Server.h#L167) (`IDirectPlay8Server`) |
| **Physics State Sync** | Yes | Yes | Yes | Yes | No | **Medium**: Relies on specific `SPHNetState` serialization and local physics ticks. | [actor_mp_client_export.cpp:L48](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/actor_mp_client_export.cpp#L48) (`PHGetSyncItem`) |
| **Prediction & Interpolation** | Yes | Yes | Yes | Yes | No | **Medium**: Enforces strict step count adjustments on the engine's main thread loop. | [Level.cpp:L1564](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Level.cpp#L1564) (`CLevel::make_NetCorrectionPrediction`) |
| **Weapon Sync** | Yes | Yes | Yes | Yes | No | **Low**: Couples keyboard input triggers to game events. | [ActorInput.cpp:L72](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ActorInput.cpp#L72) (`M_PLAYER_FIRE`) |
| **Inventory Replication** | Yes | Yes | Yes | Yes | No | **Low**: Couples inventory drag/drop actions to ownership events. | [Inventory.cpp:L672](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Inventory.cpp#L672) (`GE_INV_ACTION`), [Inventory.cpp:L922](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Inventory.cpp#L922) (`GE_OWNERSHIP_REJECT`) |
| **Game Modes & Scoring** | Yes | Yes | Yes | Yes | No | **Low**: Legacy MP rule-matching algorithms (DM, TDM, AH). | [game_sv_mp.h:L32](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/game_sv_mp.h#L32) (`class game_sv_mp`) |
| **Compressed State Updates** | Yes | Yes | Yes | Yes | No | **Medium**: Frame payload byte limit restrictions (`256` bytes size checking per object chunk). | [xr_object_list.cpp:L344](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrEngine/xr_object_list.cpp#L344) (`CObjectList::net_Export`) |
