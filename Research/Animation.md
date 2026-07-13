# Engine Research: Animation Subsystem Reference

This document maps out the verified technical structure and execution model of the animation, model mesh visual updates, and bone calculation engines in `xray-monolith`.

---

## 1. Skeletal System & Bone Updates

Animations are processed by the rendering layer (`Layers/xrRender`) on the **Primary Thread**.

- **Bone Matrices Calculation (`CalculateBones()`):**
  - **Class:** `IKinematics` / `CKinematics` (defined in [bone.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrEngine/bone.cpp))
  - Evaluates active animations, blending matrices, and applying constraints (e.g., look-at aim controllers for sight alignment).
- **Legs Controller Sync:**
  - **Class:** `CActor::m_legs_controller`
  - **Location:** [Actor.cpp:1182](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L1182)
  - Synchronizes legs animation cycle to client speed.

---

## 2. Thread Safety & Parallelism

- Animation data (motions, textures) is loaded on startup or prefetched during levels load.
- During `CRenderDevice::FrameMove()`, `Device.seqFrame` evaluates logical states (positions, speeds) of game entities.
- During rendering phase, the primary thread calls `CalculateBones()` to transform vertices prior to rasterisation.
- Bone transform calculations are NOT thread-safe to run in parallel with gameplay scripts modifying entity transforms.

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** Animation state triggers, footstep sound cues.
- **Unsafe To Hook:** Direct bone matrices or mesh vertices.
- **Engine Rewrite Required?** No.
- **Lua Accessible?** Yes, visual paths and play cycles can be triggered in scripts.
- **DLL Modification Required?** No (mostly gameplay state-driven).
- **Risk Level:** Low (mostly visual).
