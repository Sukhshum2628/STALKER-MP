# Engine Research: Physics Subsystem Reference

This document maps out the verified technical structure and execution model of the rigid-body physics and collision systems within `xray-monolith`.

---

## 1. Subsystem Cross-Reference

### `CharacterPhysicsSupport` (`CCharacterPhysicsSupport`)
- **Defined In:**
  - Header: [CharacterPhysicsSupport.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/CharacterPhysicsSupport.h) (Line 23)
  - CPP: [CharacterPhysicsSupport.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/CharacterPhysicsSupport.cpp)
- **Used By:** `CEntityAlive` (Line 13), `CAI_Stalker` (Line 68), `CActor` (Line 36)
- **Calls:** `CPHCharacter::SetVelocity()`, `IPhysicsShell::Update()`, `CPHCharacter::SetPosition()`
- **Called By:** `CEntityAlive::Hit()`, `CAI_Stalker::UpdateCL()`, `CActor::UpdateCL()`
- **Writes:** Transforms to bone structures (`CalculateBones()`), collision boundary boxes, rigid body impulses on hits.
- **Reads:** Entity state (alive/dead), desired gameplay positions, level geometry boundaries.
- **Dependencies:** `xrPhysics.dll` (ODE physics solver, rigid shells)
- **Thread:** Primary Thread (synchronous tick inside `Device.seqFrame`)
- **Lua Exports:** Yes, can access physics shells and apply impulses.
- **Virtual Functions:** None (helper implementation wrapper)

---

## 2. Transform Write Locations

Entity transforms are written to the game engine in specific locations:

1. **Active Rigid Physics Shells:**
   - When physics shells are active (e.g. dead bodies, ragdolls, physical items), transforms are retrieved from the ODE solver.
   - **Write Location:** [CharacterPhysicsSupport.cpp]
     ```cpp
     // Inside in_UpdateCL()
     m_EntityAlife.XFORM().set(m_pPhysicsShell->m_xform);
     ```
2. **Kinematic Character Controllers:**
   - When an entity is alive, it runs under a kinematic capsule (`CPHCharacter`). Gameplay coordinate requests (e.g. from script or movement manager) update the capsule's position.
   - **Write Location:** [CharacterPhysicsSupport.cpp]
     ```cpp
     // Syncs visual transform to kinematic character controller position
     m_EntityAlife.Position().set(movement()->GetPosition());
     ```
3. **Explicit API Updates:**
   - Calls to `SetPosition()` or `SetTransform()` write directly to the spatial database and the physics manager.
   - **Write Location:** [GameObject.cpp:339] (Initial Spawning coords) & `xr_object.cpp:371` (Spatial DB updates via `spatial_update`).

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** Hit impulse applications, ragdoll spawns, and displacement bounds.
- **Unsafe To Hook:** Frame-by-frame capsule collisions or ODE integration matrices.
- **Engine Rewrite Required?** No, but verification of kinematic capsule limits against client-reported transforms is required.
- **Lua Accessible?** Yes, impulses and physics shell toggling can be triggered in scripts.
- **DLL Modification Required?** Yes, modifying `xrPhysics.dll` or checking outputs in `xrGame.dll` is mandatory.
- **Risk Level:** Extreme (client-side physics authority makes teleportation and wallhacking trivial).
