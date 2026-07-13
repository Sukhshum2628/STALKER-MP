# Engine Research: Actor Subsystem Reference

This document maps out the verified technical structure and execution model of the `CActor` class and related player-controlled entity subsystems within the `xray-monolith` engine.

---

## 1. Class Layout & Definition

### `CActor`
- **Header File:** [Actor.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.h) (Line 73)
- **CPP File:** [Actor.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp) (Line 89)
- **Base Class:** `CEntityAlive` (Line 74), `IInputReceiver` (Line 75), `Feel::Touch` (Line 76), `CInventoryOwner` (Line 77), `CPhraseDialogManager` (Line 78), `CStepManager` (Line 79), `Feel::Sound` (Line 80)
- **Derived Classes:** `CActorMP` (Defined in [actor_mp_client.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/actor_mp_client.h) at Line 8)
- **Major Responsibilities:**
  - Standard player entity behavior, including input parsing, HUD synchronization, camera manager updates, condition management (health, stamina, radiation), and direct movement control.
  - Manages active camera instances (`cam_Update` in [Actor.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L1181)).
  - Direct first-person and third-person death transitions (`Die` in [Actor.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L867)).

---

## 2. Memory Ownership

| Component / Subsystem | Allocation / Ownership Pattern | Allocator Location |
| :--- | :--- | :--- |
| **GameObject / CActor** | Heap-allocated, instantiated through `object_factory()`. Lifetime managed by `CObjectList` via client spawn/destroy packets. | `src/xrEngine/xr_object_list.cpp` |
| **Entity / CEntity** | Base class allocation (part of `CActor` memory block on the Heap). | `src/xrGame/Entity.cpp` |
| **Inventory / CInventory** | Dynamic member allocation (`CInventory* m_inventory` inside `CInventoryOwner`). | `src/xrGame/InventoryOwner.cpp` |
| **Weapon / CWeapon** | Instantiated as standalone Game Objects on the heap; registered and held by `CInventory` as pointers. | `src/xrGame/Weapon.cpp` |
| **PhysicsShell / IPhysicsShell** | Reference-counted or raw pointer owned by `character_physics_support()`. Destructed on entity release. | `src/xrGame/CharacterPhysicsSupport.cpp` |
| **Animation / CStepManager** | Skeletal animations managed by rendering backend (`IKinematicsAnimated`), logic-level footsteps managed via base class. | `src/xrGame/step_manager.cpp` |

---

## 3. Thread Ownership & Synchronization

### Thread Architecture
- **Rendering:** Owned by **Primary Thread** (`"X-RAY Primary thread"`). Coordinates with D3D device and issues draw calls.
- **AI Planning:** Primary Thread (Scheduled) / Parallelised Tasks (`seqParallel`) delegated to the **Secondary Thread** (`"X-RAY Secondary thread"`).
- **Physics:** Stepped synchronously within the Primary Thread (`Device.seqFrame`) inside `xrPhysics` DLL.
- **Networking:** Client reception runs on the Primary Thread, but synchronous network IO is processed on the main loop. Time sync is run in the `"network-time-sync"` thread.
- **Animation:** Bone matrices calculated on the Primary Thread during `CalculateBones()`.
- **Input:** Captured on the Primary Thread via DirectInput wrapper `InitInput()`.
- **Audio:** Handled asynchronously via OpenAL on dedicated audio mixer threads spawned by `xrSound`.
- **Lua VM:** Single-threaded execution bound to the Primary Thread. Parallel garbage collection helpers (`Device.LuaGC`) are ran concurrently on the Secondary Thread during rendering.

### Synchronization Primitives
- **`xrCriticalSection`:** Win32 `CRITICAL_SECTION` wrapper. Used heavily in resource loaders and network message queues (`NET_Client`, `NET_Server`).
- **`xrSRWLock`:** Slim Reader-Writer locks used for low-overhead read/write locking of shared states.
- **`Lock`:** Simple atomic-backed spinning lock (`std::atomic_int lockCounter` in [Lock.hpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrCore/Lock.hpp#L36)).

---

## 4. Lifecycle Functions

- **`net_Spawn(CSE_Abstract* DC)`**: [Actor_Network.cpp:511](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Network.cpp#L511)
- **`net_Destroy()`**: [Actor_Network.cpp:721](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Network.cpp#L721)
- **`Load(LPCSTR section)`**: [Actor.cpp:356](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L356)
- **`Save(NET_Packet& packet)`**: [Actor.cpp] (NOT FOUND IN SOURCE - Client states are serialized via `net_Export`).
- **`OnEvent(NET_Packet& P, u16 type)`**: [Actor_Events.cpp:27](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor_Events.cpp#L27)
- **`Hit(SHit* pHDS)`**: [Actor.cpp:554](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L554)
- **`Die(CObject* who)`**: [Actor.cpp:867](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L867)

---

## 5. Open Questions
- How is network prediction for local actor transforms kept in sync with the physics thread?
- Is there a fallback mechanism to re-initialize actor inputs if the primary thread hangs?

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** `OnEvent()` for event replication, `Hit()` to intercept damage before resolving locally.
- **Unsafe To Hook:** Direct physics step variables or raw DirectInput processing.
- **Engine Rewrite Required?** No. Most MP replication hooks are already stubbed out or implemented in `CActorMP`.
- **Lua Accessible?** Partially, through the `CActor` export bindings in Lua space.
- **DLL Modification Required?** Yes, `xrGame.dll` must be rebuilt to implement server-side verification.
- **Risk Level:** High (due to prediction & lag compensation challenges).
