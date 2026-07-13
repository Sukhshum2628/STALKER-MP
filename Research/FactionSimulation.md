# Faction Warfare & Relations Simulation

This document outlines the ownership and implementation details of the faction simulation and relations systems.

---

## 1. Faction Relations & Community Lookup

* **Engine Interfaces**:
  There are no dedicated C++ class controllers for faction warfare or faction campaigns inside `xray-monolith`. The engine delegates faction classification and relations queries to the Lua script environment.
* **Lua Callback Integrations**:
  - For example, actor community updates retrieve the character's community name from script variables using `_g.get_actor_true_community` ([Actor.cpp:L1425](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/Actor.cpp#L1425)).
  - System relations (Stalker community profiles) are defined in game configurations (`game_relations.ltx`) and parsed into memory by game scripts rather than native C++ classes.

---

## 2. Faction Warfare & Territory Ownership

* **Obsolete Engine Classes**:
  Legacy UI classes for faction warfare visualization (e.g., `CUIFactionWarWnd` in `UIPdaWnd.cpp`) are commented out and unused ([UIPdaWnd.cpp:L101](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/ui/UIPdaWnd.cpp#L101)).
* **Script Ownership**:
  All mechanics related to faction territory control, faction resources, squad reinforcements, raid generators, and territory occupation are entirely implemented in **Lua scripts** (specifically, `sim_board.script`, `sim_faction.script`, `sim_squad_generic.script`).
* **Squad Movement**:
  Squad routing and movement commands are issued by script planners that assign target smart terrains or coordinate graph vertices to the squad commander `CSE_ALifeOnlineOfflineGroup`, which then updates its members.
