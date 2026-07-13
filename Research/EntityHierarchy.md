# Engine Research: Entity Hierarchy Reference

This document maps out the inheritance hierarchy of core gameplay entities within `xray-monolith`.

---

## 1. Class Inheritance Tree

```
          CObject [xrEngine]
             │
        CGameObject [xrGame]
             │
          CEntity [xrGame]
             │
        CEntityAlive [xrGame]
         /         \
        /           \
  CActor [xrGame]  CCustomMonster [xrGame]
                        │
                  CAI_Stalker [xrGame]
```

---

## 2. Detailed Class Breakdown

### `CObject`
- **Header:** [xr_object.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrEngine/xr_object.h) (Line 28)
- **CPP:** [xr_object.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrEngine/xr_object.cpp)
- **Base Class:** `DLL_Pure`, `ISpatial`, `ISheduled`, `IRenderable`, `ICollidable`
- **Responsibilities:**
  - Base class for all world objects. Handles transform matrices (`XFORM()`), rendering interface registration, spatial database registration, and scheduler updates.

### `CGameObject`
- **Header:** [GameObject.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/GameObject.h) (Line 53)
- **CPP:** [GameObject.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/GameObject.cpp)
- **Base Class:** `CObject`
- **Responsibilities:**
  - Game logic attachment point. Integrates script binding callbacks (`CScriptBinder`), active inventory attachments, and net-ID mappings.

### `CEntity`
- **Header:** [Entity.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Entity.h) (Line 17)
- **CPP:** [Entity.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Entity.cpp)
- **Base Class:** `CGameObject`
- **Responsibilities:**
  - Represents a biological entity. Manages visual meshes, simple hit logic, and death state flags.

### `CEntityAlive`
- **Header:** [entity_alive.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/entity_alive.h) (Line 18)
- **CPP:** [entity_alive.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/entity_alive.cpp)
- **Base Class:** `CEntity`
- **Responsibilities:**
  - Implements condition management (stamina, morale, bleeding, radiation), dynamic hits processing (`Hit()`), and ragdoll triggers on death.

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** Virtual functions like `Hit()` or `Die()` at the base level to capture all entities uniformly.
- **Unsafe To Hook:** Memory layout variables inside `CObject` as it will break offset calculations in the engine DLLs.
- **Engine Rewrite Required?** No.
- **Lua Accessible?** High (most derived classes have scripting wrappers).
- **DLL Modification Required?** Yes, to add multiplayer synchronisation logic to `CEntityAlive` or `CGameObject`.
- **Risk Level:** High (essential foundation layer).
