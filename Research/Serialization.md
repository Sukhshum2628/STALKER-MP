# Engine Research: Serialization & Networking State Reference

This document maps out the verified serialization flows, packet generation, and state synchronisation logic of X-Ray entities.

---

## 1. Serialization Functions

In X-Ray Engine, state serialization is divided into **Server Entity States** (persistent databases and savegames) and **Client Entity Sync** (active multiplayer updates).

### Server State Serialization (`xrServerEntities`)
- **`STATE_Write(NET_Packet& packet)`**
  - Writes permanent properties (spawn configuration, health, custom data, level name) to be saved to disk or sent to a client joining the game.
  - **Actor:** `CSE_ALifeCreatureActor::STATE_Write` in [xrServer_Objects_ALife_Monsters.cpp:1504](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrServerEntities/xrServer_Objects_ALife_Monsters.cpp#L1504)
  - **Stalker:** `CSE_ALifeHumanStalker::STATE_Write` in [xrServer_Objects_ALife_Monsters.cpp:2172](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrServerEntities/xrServer_Objects_ALife_Monsters.cpp#L2172)
- **`STATE_Read(NET_Packet& packet, u16 size)`**
  - Reads permanent properties during load.
  - **Actor:** `CSE_ALifeCreatureActor::STATE_Read` in [xrServer_Objects_ALife_Monsters.cpp:1474](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrServerEntities/xrServer_Objects_ALife_Monsters.cpp#L1474)
  - **Stalker:** `CSE_ALifeHumanStalker::STATE_Read` in [xrServer_Objects_ALife_Monsters.cpp:2178](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrServerEntities/xrServer_Objects_ALife_Monsters.cpp#L2178)

### Client Real-time Synchronisation (`xrGame`)
- **`net_Export(NET_Packet& P)`**
  - Serializes transient properties (current position, look yaw/pitch, active weapon state, movement speeds) to be sent over the wire.
  - **Actor:** `CActor::net_Export` in [Actor_Network.cpp:88](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Network.cpp#L88)
  - **Stalker:** `CAI_Stalker::net_Export` in [ai_stalker.cpp:863](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L863)
- **`net_Import(NET_Packet& P)`**
  - Deserializes packets received from the network.
  - **Actor:** `CActor::net_Import` in [Actor_Network.cpp:290](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Network.cpp#L290)
  - **Stalker:** `CAI_Stalker::net_Import` in [ai_stalker.cpp:912](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L912)

---

## 2. Event Lifecycle: Hit & Death Events

Multiplayer gameplay relies on event packets (`M_EVENT` / `OnEvent`).

- **`OnEvent(NET_Packet& P, u16 type)`**
  - **Actor:** [Actor_Events.cpp:27](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Events.cpp#L27)
  - **Stalker:** [ai_stalker_events.cpp:26](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker_events.cpp#L26)
  - Dispatches inventory trades, weapon fire triggerings, item usage, and active status updates.

- **`Hit(SHit* pHDS)`**
  - **Actor:** [Actor.cpp:554](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L554)
  - **Stalker:** [ai_stalker_fire.cpp:266](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker_fire.cpp#L266)
  - Resolves local bone damages, armor shielding reductions, and health drops.

- **`Die(CObject* who)`**
  - **Actor:** [Actor.cpp:867](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L867)
  - **Stalker:** [ai_stalker.cpp:596](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L596)
  - Disables gameplay scripts, detaches active cameras, triggers ragdoll physics support, and updates score databases.

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** `OnEvent()` packet handlers, `STATE_Write` for save game enhancements.
- **Unsafe To Hook:** Raw memory copies of network stream buffers.
- **Engine Rewrite Required?** No. Monolith preserves standard X-Ray MP serialisation routines.
- **Lua Accessible?** Low. Script binders can intercept `net_spawn` and `net_destroy`, but actual packet layouts are hardcoded in C++.
- **DLL Modification Required?** Yes, to change or expand network packet headers in `xrGame.dll`.
- **Risk Level:** Medium (highly exposed to buffer overflows if packet sizes aren't strictly validated).
