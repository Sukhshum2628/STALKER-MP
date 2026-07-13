# Engine Research: Call Graphs & Event Lifecycle Reference

This document maps out the verified call graphs, exact source locations, and lifecycles of key engine functions in `xray-monolith`.

---

## 1. UpdateCL() Call Graph Trace

```
CAI_Stalker::UpdateCL() [ai_stalker.cpp:1017]
  │
  ├──► CGameObject::UpdateCL() [GameObject.cpp:1280] (via inherited::UpdateCL() calling CustomMonster, then EntityAlive, and finally GameObject)
  │      │
  │      └──► CObject::UpdateCL() [xr_object.cpp:357]
  │
  ├──► CCharacterPhysicsSupport::in_UpdateCL() [CharacterPhysicsSupport.cpp]
  │
  └──► CSightManager::update() [sight_manager.cpp]
```

```
CActor::UpdateCL() [Actor.cpp:1128]
  │
  ├──► CGameObject::UpdateCL() [GameObject.cpp:1280] (via inherited::UpdateCL() resolving past EntityAlive/Entity)
  │      │
  │      └──► CObject::UpdateCL() [xr_object.cpp:357]
  │
  ├──► CCharacterPhysicsSupport::in_UpdateCL() [CharacterPhysicsSupport.cpp]
  │
  └──► CCameraManager::Update() [CameraManager.cpp]
```

---

## 2. Event Lifecycle File Locations

### CAI_Stalker
- **`UpdateCL()`**: [ai_stalker.cpp:1017](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L1017)
- **`Load(LPCSTR)`**: [ai_stalker.cpp:654](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L654)
- **`Save()`**: NOT FOUND IN SOURCE
- **`net_Spawn()`**: [ai_stalker.cpp:666](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L666)
- **`net_Destroy()`**: [ai_stalker.cpp:820](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L820)
- **`OnEvent()`**: [ai_stalker_events.cpp:26](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker_events.cpp#L26)
- **`Hit()`**: [ai_stalker_fire.cpp:266](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker_fire.cpp#L266)
- **`Die()`**: [ai_stalker.cpp:596](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L596)

### CActor
- **`UpdateCL()`**: [Actor.cpp:1128](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L1128)
- **`Load(LPCSTR)`**: [Actor.cpp:356](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L356)
- **`Save()`**: NOT FOUND IN SOURCE
- **`net_Spawn()`**: [Actor_Network.cpp:511](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Network.cpp#L511)
- **`net_Destroy()`**: [Actor_Network.cpp:721](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Network.cpp#L721)
- **`OnEvent()`**: [Actor_Events.cpp:27](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Events.cpp#L27)
- **`Hit()`**: [Actor.cpp:554](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L554)
- **`Die()`**: [Actor.cpp:867](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L867)

### CEntityAlive
- **`UpdateCL()`**: NOT FOUND IN SOURCE (Inherits from `CGameObject::UpdateCL`)
- **`Load(LPCSTR)`**: [entity_alive.cpp] (line not cached, inherits base settings)
- **`net_Spawn()`**: [entity_alive.cpp:241](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/entity_alive.cpp#L241)
- **`net_Destroy()`**: [entity_alive.cpp:264](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/entity_alive.cpp#L264)
- **`OnEvent()`**: [entity_alive.cpp:316](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/entity_alive.cpp#L316)
- **`Hit()`**: [entity_alive.cpp:275](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/entity_alive.cpp#L275)
- **`Die()`**: [entity_alive.cpp:321](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/entity_alive.cpp#L321)

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** Event wrappers (`OnEvent`), Hit handlers (`Hit`), Death updates (`Die`).
- **Unsafe To Hook:** UpdateCL loops (extremely performance-critical).
- **Engine Rewrite Required?** No.
- **Lua Accessible?** Partially (callbacks are mapped to binder wrapper classes).
- **DLL Modification Required?** Yes, hooks must be compiled into `xrGame.dll`.
- **Risk Level:** High (governs gameplay integrity).
