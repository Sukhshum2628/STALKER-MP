# STALKER-MP

> A Persistent Multiplayer ALife Framework for the X-Ray Engine.

---

# Vision

STALKER-MP is an open-source engine framework that transforms the X-Ray Engine from a strictly single-player experience into a persistent multiplayer simulation while preserving the systems that make S.T.A.L.K.E.R. unique.

Unlike traditional co-op modifications, STALKER-MP does not attempt to replace the existing game simulation.

Instead, it extends the engine so that multiple human players can coexist inside the same living Zone.

The objective is to preserve the original ALife simulation, Smart Terrain behavior, faction warfare, story progression, dynamic population, and world persistence while introducing multiplayer as another layer of the simulation.

Players are not the center of the world.

They are simply additional stalkers living inside it.

---

# Core Philosophy

The Zone exists independently of the players.

The world should continue evolving even if every player disconnects.

Human players should be treated exactly like AI stalkers, with the only difference being that their decisions originate from keyboard and mouse input instead of the GOAP planner.

The simulation always remains authoritative.

---

# Design Goals

• Preserve ALife.

• Preserve Smart Terrain.

• Preserve GOAP AI.

• Preserve Faction Warfare.

• Preserve Story Systems.

• Preserve Offline Simulation.

• Preserve Inventory Systems.

• Preserve Weapon Systems.

• Preserve Existing Animation Systems.

• Reuse as much of the legacy multiplayer infrastructure as possible.

• Support long-running persistent worlds.

• Support future dedicated servers.

• Remain compatible with Anomaly and future derivative projects whenever practical.

---

# Non Goals

STALKER-MP is NOT intended to become:

• A deathmatch modification.

• A battle royale.

• A PvP-first shooter.

• A complete engine rewrite.

• A replacement for ALife.

• A replacement for Smart Terrain.

• A replacement for the scripting system.

---

# Project Principles

## Preserve Before Replace

Existing engine systems should be preserved whenever possible.

Replacing mature engine subsystems is considered the last resort.

---

## Host Authority

The host owns the world simulation.

Clients own only:

- input
- prediction
- interpolation
- rendering

The host owns:

- ALife
- AI
- quests
- inventory
- persistence
- world simulation

---

## One World

There is only one authoritative Zone.

Clients observe and interact with that Zone.

No client owns a private simulation.

---

## Multiple Human Stalkers

Players are represented as additional stalkers living inside the same simulation.

The simulation never pauses because a player disconnects.

---

## Compatibility First

Engine modifications should minimize changes to existing gameplay systems.

When possible, new functionality should be introduced through abstraction layers rather than replacing mature code.

---

# Repository Structure

docs/
    Project documentation

engine/
    xray-monolith source

research/
    Reverse engineering notes

rfc/
    Architecture decisions

sprints/
    Implementation milestones

tools/
    Build tools

tests/
    Validation and regression tests

---

# Current Project Status

Phase

✅ Engine Investigation

✅ Legacy Multiplayer Investigation

✅ ALife Investigation

✅ Modification Surface Analysis

⬜ Architecture RFC

⬜ Engine Refactoring

⬜ Prototype Multiplayer

⬜ Persistent ALife

---

# Guiding Principle

The objective is not to make STALKER multiplayer.

The objective is to make the Zone capable of containing multiple human stalkers without losing what makes the Zone feel alive.