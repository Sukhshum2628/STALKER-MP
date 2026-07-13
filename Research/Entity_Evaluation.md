# Engine Research — Entity Options Evaluation

This document evaluates three architectural options to implement a network-controlled player entity inside the X-Ray Monolith Engine C++ class hierarchy.

---

## 1. Class Hierarchy Context

```
                    CObject
                       │
                  CGameObject
                       │
              CPhysicsShellHolder
                       │
                 CEntityAlive
                  /         \
       CCustomMonster      CActor (Local Player Singleton)
            │
       CAI_Stalker (Autonomous AI Agent)
```

---

## 2. Option Comparison

### Option 1: Extending `CAI_Stalker`
This approach uses the existing AI stalker class but adds a flag (`controlled_by_network`) to bypass steering and planners.

*   **Code Reuse**: **Maximum**. Inherits all existing visual skeletal rendering, inventory ownership (`CInventoryOwner`), weapon handling, active slot bindings, sound triggers, and dialogue interfaces.
*   **Required Engine Changes**: **Low**. Add checks in `CAI_Stalker::UpdateCL()`, `CSightManager::update()`, and `MovementManager::update()` to skip calculations if the network flag is active. Position is written directly to `XFORM` inside `UpdateCL()`.
*   **Animation Support**: Fully supported out-of-the-box via the model's standard skeleton bindings.
*   **Inventory Compatibility**: Full. Interacts naturally with traders, stash boxes, and looting callbacks.
*   **Networking Integration**: Requires writing network packet updates (`net_Import` / `net_Export`) into the stalker serialization loops.
*   **Maintainability / Risk**: **Medium-High**. The `CAI_Stalker` class is extremely complex (~10,000 lines of code). Bypassing parts of it can trigger edge-case bugs (e.g. AI sensory queries crashing because memory arrays are empty, or logic schemes trying to reference a deleted planner).

---

### Option 2: Extending `CEntityAlive` (or `CCustomMonster` / `CInventoryOwner`)
This approach creates a new class `CNetPlayer` derived from `CEntityAlive` and inheriting `CInventoryOwner` to support items.

*   **Code Reuse**: **Medium**. Inherits basic health stats, skeletal rendering (`IKinematics`), and physical properties, but lacks weapon slot logic and posture animations.
*   **Required Engine Changes**: **High**. We must manually implement weapon unholstering, aim-offsets, active user slots, HUD updates, and skeletal state binders that standard stalkers get for free.
*   **Animation Support**: Medium. Requires writing custom C++ animation controllers to map indices to skeletal bone tracks.
*   **Inventory Compatibility**: High. Since it inherits `CInventoryOwner`, it can hold objects, but trading and slot attachment logic must be manually written.
*   **Networking Integration**: High. We write clean, custom packet serialization without legacy single-player Alife constraints.
*   **Maintainability / Risk**: **Medium**. Keeps the network player decoupled from complex AI files, but introduces significant code duplication to recreate weapon slot mechanics.

---

### Option 3: Creating a New `CRemotePlayer` Class
This approach designs a new class from the base `CGameObject` or `CPhysicsShellHolder` layers.

*   **Code Reuse**: **Minimal**. Almost every gameplay system (health, inventory, weapons, dialogues, HUD) must be implemented from scratch.
*   **Required Engine Changes**: **Very High**. Massive C++ changes to tie this new entity type into level maps, sector managers, trade menus, and HUD layouts.
*   **Animation Support**: Low. Requires writing a custom skeletal animator wrapping `IKinematicsAnimated`.
*   **Inventory Compatibility**: Low. Requires a custom database subclass.
*   **Networking Integration**: Excellent. Completely clean client-server socket synchronization.
*   **Maintainability / Risk**: **Low Risk for desyncs, but High Development Overhead**. The development time to rewrite basic features (weapon aiming, armor items, HUD) is prohibitive for a mod project.

---

## 3. Comparative Matrix

| Criterion | Option 1: `CAI_Stalker` | Option 2: `CEntityAlive` | Option 3: `CRemotePlayer` |
| :--- | :--- | :--- | :--- |
| **Development Cost** | **Lowest** (Days) | High (Weeks) | Prohibitive (Months) |
| **Code Reuse** | **90%** (Saves days of rewriting) | 50% | 10% |
| **Animation Fidelity** | **Perfect** (Uses stalker anims) | Needs manual mapping | Needs custom player mapping |
| **Inventory Support** | **Full** (Native slot logic) | Limited (Needs slot bindings) | None (Needs rewrite) |
| **Desync Risk** | Medium (AI planner interference) | **Low** (No AI code) | **Lowest** (No AI code) |
| **DLL Compatibility** | High (Hooks into existing files) | Low (Requires new exports) | Lowest (Requires new exports) |

---

## 4. Architectural Recommendation

> [!TIP]
> **We recommend Option 1: Extending `CAI_Stalker`**. 

### Justification:
S.T.A.L.K.E.R.'s gameplay is defined by item inventories, dialogue overlays, weapon attachments, and skeletal animation grids. Recreating these systems in a custom `CRemotePlayer` (Option 3) or `CEntityAlive` (Option 2) introduces massive development overhead and code duplication, essentially requiring you to rewrite half of the single-player gameplay logic.

By choosing **Option 1**, we leverage 90% of the existing stalker mechanics. To resolve the planner interference (our previous desync issue), we implement the network override at the C++ level inside `CAI_Stalker::UpdateCL()`. 

If `controlled_by_network` is true, we:
1.  Bypass `movement().update()`, `sight().update()`, and the GOAP planner.
2.  Perform direct cubic spline interpolation (`lerp`) on the entity's `Position` and `XFORM` based on incoming network packet updates.
3.  Directly call `IKinematicsAnimated::PlayCycle()` to force target animations received from the network.
This gives us the complete visual and inventory stability of a stalker while completely silencing its autonomous AI.
