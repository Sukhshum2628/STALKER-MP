# Engine Research — CAI_Stalker & UpdateCL Trace

This document maps out the execution lifecycle of `CAI_Stalker::UpdateCL()` (Update Client) inside the X-Ray C++ game logic layer (`xrGame\ai_stalker.cpp`).

---

## 1. CAI_Stalker::UpdateCL() Call Graph Trace

When the engine schedules a frame update for a stalker, the main loop calls `CAI_Stalker::UpdateCL()`. Below is the sequential call path:

```
CAI_Stalker::UpdateCL()  [xrGame\ai_stalker.cpp]
  │
  ├──► CEntityAlive::UpdateCL()  [xrGame\entity_alive.cpp]
  │      │
  │      └──► CEntity::UpdateCL()  [xrGame\entity.cpp]
  │             │
  │             └──► CGameObject::UpdateCL()  [xrGame\GameObject.cpp]
  │
  ├──► CAttachmentOwner::update_attachments()  [xrGame\attachment_owner.cpp]
  │      └─► Sync weapon attachments visual models/transforms
  │
  ├──► sensors().update()  [xrGame\stalker_sensor.cpp]
  │      ├─► VisualMemoryManager::update()  [xrGame\visual_memory_manager.cpp]
  │      └─► SoundMemoryManager::update()   [xrGame\sound_memory_manager.cpp]
  │
  ├──► movement().update()  [xrGame\stalker_movement_manager_obstacles.cpp]
  │      └─► Trace delegated to MovementManager (See Physics.md)
  │
  ├──► sight().update()  [xrGame\stalker_sight_manager.cpp]
  │      └─► CSightManager::update()
  │            └─► Computes head/torso aim matrices towards target look direction
  │
  ├──► ExecuteScriptBinderUpdate()
  │      └─► luabind::call_member<void>("update")  [Triggers motivator_binder:update() in Lua JIT]
  │
  └──► IKinematicsAnimated::PlayCycle() / CalculateBones()  [xrRender\KinematicsAnimated.cpp]
         └─► Blends moving skeletal animations (walk, idle, hit reactions) based on movement speed
```

---

## 2. Line-by-Line Execution Breakdown

### `CAI_Stalker::UpdateCL()` (Inside `xrGame\ai_stalker.cpp`)
1.  **Safety Guard Checks**:
    ```cpp
    if (!g_Alive()) {
        // If the stalker is dead, skip brain/movement loops
        Inherited::UpdateCL(); // Calls CEntityAlive::UpdateCL() to process ragdoll physics decay
        return;
    }
    ```
2.  **Visual Update Sync**:
    ```cpp
    if (g_Alive() && !animation().active()) {
        // Initializes default animation state if animation engine is idle
        animation().update();
    }
    ```
3.  **Sensory Scan**:
    ```cpp
    spatial.type |= STYPE_COLLIDEABLE; // Ensure entity is registered in spatial database for raycasts
    VisualMemoryManager.update(client_update_delta); // Scans the player and enemy lists in the visual frustum
    SoundMemoryManager.update();                     // Evaluates gunshots, steps, or explosion sounds in the radius
    ```
4.  **Steering & Orientation Planning**:
    ```cpp
    movement().update(client_update_delta); // Triggers detail path navigation and resolves obstacles
    sight().update();                       // Applies facing matrices based on current steering path or target locks
    ```
5.  **Script State Engine Callback**:
    ```cpp
    // Calls out to Lua (motivator_binder:update)
    // Lua state manager evaluates states and sets target weapon/motion overrides
    callback(GameObject::eUpdate)(client_update_delta);
    ```
6.  **Bone Coordinate Matrix Alignment**:
    ```cpp
    // Syncs the skeleton visual render model to the actual character controller physics position
    XFORM().set(PhysicsSelfShell()->GetTransform());
    ```
