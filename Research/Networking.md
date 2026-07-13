# Engine Research: Networking System Reference

This document maps out the verified network message loop, packet delivery, and server-client updates in `xray-monolith`.

---

## 1. Network Interfaces & Loop

Networking is managed by the `xrNetServer` module.

- **`IPureClient` / `IPureServer`:**
  - Interfaces for starting network connections, binding sockets, and sending/receiving packet packets.
  - **Thread:** Running on the **Primary Thread** (with asynchronous helper thread `"network-time-sync"` in client mode).
- **Network Message Queueing:**
  - Inbound network packets are queued in thread-safe queues.
  - Locks (`xrCriticalSection`) protect queue insertions and extractions from concurrent threads.

---

## 2. Sync Packets Flow

1. **Client Sends Input State:**
   - `CActor::net_Export()` writes movement parameters (keys pressed, yaw, pitch) to a packet.
2. **Server Updates Entity positions:**
   - Server processes packets, updates physical objects in `xrPhysics`, and broadcasts absolute entity updates.
3. **Client Interpolation:**
   - Client imports packets via `net_Import()` and interpolates coordinates (`lerp()` in [CustomMonster.cpp:513](file:///c:/Users/sukhs/Downloads/Games/STALKER-MP/Engine/xray-monolith/src/xrGame/CustomMonster.cpp#L513)).

---

==================================================
## MULTIPLAYER RELEVANCE
==================================================
- **Safe To Hook:** Event dispatch loops, connection approvals, packet filtering.
- **Unsafe To Hook:** Low-level socket read/write calls or OpenSSL/cryptography routines.
- **Engine Rewrite Required?** No.
- **Lua Accessible?** Low. Custom packets must be registered in C++.
- **DLL Modification Required?** Yes, `xrNetServer.dll` and `xrGame.dll` must be updated.
- **Risk Level:** High (essential for connection reliability and DDoS/cheats protection).
