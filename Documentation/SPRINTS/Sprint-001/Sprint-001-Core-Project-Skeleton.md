# Sprint-001: Core Project Skeleton
# Sprint-001: Core Project Skeleton

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 001 |
| Title | Core Project Skeleton |
| Status | Planned |
| Priority | Critical |
| Estimated Duration | 1–2 Weeks |
| Prerequisites | Architecture v1, RFC-0001 through RFC-0007 |
| Next Sprint | Sprint-002 – World Abstraction Layer |

---

# 1. Sprint Overview

Sprint-001 establishes the foundation upon which the remainder of STALKER-MP will be developed.

No gameplay functionality is introduced during this sprint.

Instead, the objective is to prepare a clean, modular, maintainable codebase capable of supporting all future multiplayer systems without requiring structural refactoring.

By the conclusion of this sprint, the project should compile successfully, integrate into the X-Ray Engine, initialize correctly, expose the project's core module boundaries, and provide the infrastructure required by subsequent implementation sprints.

---

# 2. Objectives

## Primary Objectives

• Establish project directory structure.

• Integrate multiplayer module into X-Ray.

• Create foundational namespaces.

• Define subsystem boundaries.

• Introduce project configuration.

• Create logging framework.

• Create assertion framework.

• Establish initialization pipeline.

• Create basic testing infrastructure.

• Ensure successful compilation.

---

## Secondary Objectives

• Documentation templates.

• Coding conventions.

• Build configurations.

• Debug utilities.

• Version information.

---

# 3. Scope

Included:

- Repository organization
- Module registration
- Build system
- Configuration system
- Logging
- Assertions
- Engine bootstrap
- Project settings
- Base interfaces
- Common utilities

---

# 4. Out of Scope

The following systems are NOT implemented.

- Multiplayer
- Networking
- Replication
- ALife modifications
- Bubble Manager
- Persistence
- Threading
- Player synchronization
- Prediction
- Lua
- Plugins

Only infrastructure required to support these systems.

---

# 5. Architecture References

02_Engine.md

06_Multiplayer.md

09_Threading.md

10_Extensibility.md

---

# 6. RFC References

RFC-0001

RFC-0006

RFC-0007

---

# 7. Technical Tasks

## 7.1 Repository Structure

Create project folders.

Example:

src/

include/

tests/

third_party/

docs/

tools/

assets/

scripts/

---

## 7.2 Namespace Layout

Introduce root namespace.

Example

stalkermp::

Sub-namespaces

core

world

network

player

alife

replication

persistence

threading

plugins

common

---

## 7.3 Build System

Configure CMake.

Support

Debug

Release

RelWithDebInfo

Static Analysis

Warnings as errors.

---

## 7.4 Project Configuration

Introduce configuration loader.

Support

server.cfg

client.cfg

development.cfg

Future runtime reload support.

---

## 7.5 Logging System

Create centralized logger.

Support

Info

Warning

Error

Debug

Verbose

Subsystem categories.

Timestamp support.

Log file output.

Console output.

---

## 7.6 Assertion Framework

Create internal assertion macros.

Development assertions.

Fatal assertions.

Validation helpers.

Compile-time assertions.

---

## 7.7 Engine Bootstrap

Create multiplayer initialization entry point.

Responsibilities

Initialize modules.

Initialize logging.

Load configuration.

Register services.

Verify engine compatibility.

Shutdown pipeline.

---

## 7.8 Base Interfaces

Create abstract interfaces.

Examples

ISubsystem

IModule

IService

IManager

ISerializable

ITickable

IInitializable

Interfaces only.

No implementation.

---

## 7.9 Service Registry

Create service locator.

Responsibilities

Register service.

Find service.

Shutdown order.

Dependency validation.

---

## 7.10 Module Registration

Register future modules.

World

Player

Replication

Persistence

Networking

Plugins

Diagnostics

Modules may remain empty.

---

## 7.11 Utility Library

Common helpers.

Examples

Math

Hashing

UUID

Timing

Filesystem

String utilities

Containers

Memory helpers

---

## 7.12 Error Handling

Unified error reporting.

Fatal errors.

Recoverable errors.

Error codes.

Crash diagnostics.

---

## 7.13 Version System

Project version.

Engine version.

Build version.

Git revision.

Compatibility version.

---

## 7.14 Testing Infrastructure

Setup

GoogleTest

(or preferred framework)

Test runner.

Test utilities.

Mock interfaces.

Example unit test.

---

## 7.15 Documentation

Document

Folder layout.

Naming conventions.

Coding standards.

Branch strategy.

Contribution guide.

---

# 8. Deliverables

By the end of Sprint-001 the repository contains

✓ Modular project structure

✓ Successful compilation

✓ Engine bootstrap

✓ Configuration system

✓ Logging framework

✓ Assertion framework

✓ Base interfaces

✓ Service registry

✓ Testing infrastructure

✓ Documentation skeleton

---

# 9. Risks

Potential risks

Incorrect engine integration.

Poor module boundaries.

Over-engineering.

Dependency cycles.

Build portability.

---

Mitigation

Keep systems lightweight.

Use interfaces.

Avoid gameplay implementation.

Maintain strict ownership.

---

# 10. Validation Strategy

Compilation

✓ Debug build succeeds.

✓ Release build succeeds.

Initialization

✓ Engine loads successfully.

✓ Modules initialize in order.

Configuration

✓ Config files load correctly.

Logging

✓ Log file generated.

✓ Console output verified.

Testing

✓ Example unit test passes.

Shutdown

✓ Engine shuts down cleanly.

---

# 11. Acceptance Criteria

The sprint is complete when

□ Project builds successfully.

□ Engine starts.

□ Engine shuts down cleanly.

□ All modules register.

□ Logging operational.

□ Assertions operational.

□ Config loader operational.

□ Base interfaces complete.

□ Service registry functional.

□ Documentation updated.

---

# 12. Definition of Done

Sprint-001 is considered complete only when

• Every deliverable exists.

• Code compiles without warnings.

• Documentation matches implementation.

• Unit tests pass.

• Engine integration verified.

• No gameplay functionality has been introduced.

---

# 13. Next Sprint

Completion of Sprint-001 unlocks

Sprint-002

World Abstraction Layer

This sprint will introduce the first gameplay-facing architecture by creating the World subsystem responsible for entity ownership, world queries, spatial organization, and the foundation upon which ALife and multiplayer systems will later operate.