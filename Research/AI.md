# Engine Research: AI Subsystem Reference

This document maps out the verified logical structure, scheduler hooks, and override methods for the stalker and monster AI systems within `xray-monolith`.

---

## 1. AI Updates & Planners

Stalker decision making is managed by the Goal Oriented Action Planner (GOAP) engine in `xrGame`.

- **Brain Planner Updates:**
  - **Class:** `CStalkerPlanner` (defined in [stalker_planner.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/stalker_planner.h) and `stalker_planner.cpp`)
  - **Location:** `CAI_Stalker::shedule_Update` in [ai_stalker.cpp](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp)
  - Updates the active action, evaluates world state conditions, and sets target movement/weapon states.
- **Sight Manager Updates:**
  - **Class:** `CSightManager` (defined in [sight_manager.h](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/sight_manager.h) at Line 22)
  - **Location:** `CAI_Stalker::UpdateCL` in [ai_stalker.cpp:1081](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ai/stalker/ai_stalker.cpp#L1081)
  - Evaluates aiming direction, head look-at vectors, and updates yaw/pitch.

---

## 2. Override & Pause Vectors

Stalker AI execution can be paused, overridden, or disabled in multiple ways:

1. **Script Control (`script_control()`):**
   - Gameplay scripts can acquire explicit control over movement and weapon states.
   - Checked inside `stalker_movement_manager_base::update` at line 639:
     ```cpp
     if (script_control())
         return; // Skip standard pathfinding
     ```
2. **Deactivation / Optimization:**
   - Setting `setEnabled(FALSE)` or `processing_deactivate()` prevents scheduled AI updates.
3. **Dead States:**
   - Checks to `g_Alive()` skip planner runs entirely.

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** Planner action completions and script overrides.
- **Unsafe To Hook:** Low-level pathfinding loops or raycast sensory updates.
- **Engine Rewrite Required?** No.
- **Lua Accessible?** High (highly integrated with Lua scripts for logic schemes).
- **DLL Modification Required?** No (usually script-driven).
- **Risk Level:** Low (mostly AI-focused).
