# Player Request Message-ID Allocation

> **Governance scope.** This section freezes the *semantic mapping* of the already-frozen Sprint-007 player-request id block **`0x0100–0x010F`** (reserved in the Implementation Plan Clarifications §7.2). It is a protocol-governance freeze only. It modifies no sprint scope, architecture, ADR, implementation order, ownership boundary, networking architecture, wire format, packet layout, or serialization. It introduces **request messages only** — no response, replication, snapshot, or prediction messages, and no new packet format. ADR-010 is preserved: these are additive `MessageId` constants within Sprint-006's existing data-id range (`>= 0x0100`); the 18-byte header and field-by-field little-endian codec are untouched.

## 1. Ownership & invariants

- **Block owner.** The contiguous block `0x0100–0x010F` (16 ids) is owned by **Sprint-007 (Player Lifecycle)**. Constants are declared in **one** single-source-of-truth header, `include/stalkermp/player/PlayerMessageIds.h`. Sprint-006 `ProtocolConstants.h` is **not** modified.
- **Direction.** Every id in this block is a **client → host request**. The host owns every resulting state transition (Host Authority); a request is a petition, never a command.
- **Range discipline.** Every id satisfies `IsDataId` (`>= 0x0100`) and is disjoint from the Sprint-006 control range `0x0000–0x00FF` (handshake/keepalive/session control — never reused here).
- **Additive-only.** Once an id is assigned "Used", its meaning is permanent. A retired message's number is **retired, never reissued** with a different meaning (mirrors Sprint-006's additive-id rule and ADR-010's additive-ids constraint).

## 2. Allocation table (`0x0100–0x010F`)

| Constant name | Value | Purpose (request only) | Ownership | Implementation status |
|---|---|---|---|---|
| `kMsgPlayerJoinRequest` | `0x0100` | Client requests to join the session as a player. Carries the optional Sprint-006 reconnect token: token **absent** → fresh-join candidate; token **present** → reclaim path via `net::Session::TryReclaim` (rebinds to the existing `PlayerId`/`EntityId`, never a duplicate). Host validates and owns the outcome. | Sprint-007 | **Used by Sprint-007** |
| `kMsgPlayerRespawnRequest` | `0x0101` | Client requests respawn while its player is in the `Dead` / `AwaitingRespawn` lifecycle state. Host validates timing (tick-derived respawn delay) and location policy and owns the outcome. | Sprint-007 | **Used by Sprint-007** |
| *(unnamed)* | `0x0102` | — | Sprint-007 | **Reserved for future Sprint-007 work** |
| *(unnamed)* | `0x0103` | — | Sprint-007 | **Reserved for future Sprint-007 work** |
| *(unnamed)* | `0x0104` | — | Sprint-007 | **Reserved for future Sprint-007 work** |
| *(unnamed)* | `0x0105` | — | Sprint-007 | **Reserved for future Sprint-007 work** |
| *(unnamed)* | `0x0106` | — | Sprint-007 | **Reserved for future Sprint-007 work** |
| *(unnamed)* | `0x0107` | — | Sprint-007 | **Reserved for future Sprint-007 work** |
| *(none)* | `0x0108` | — | Sprint-007 block | **Reserved — Do Not Allocate** |
| *(none)* | `0x0109` | — | Sprint-007 block | **Reserved — Do Not Allocate** |
| *(none)* | `0x010A` | — | Sprint-007 block | **Reserved — Do Not Allocate** |
| *(none)* | `0x010B` | — | Sprint-007 block | **Reserved — Do Not Allocate** |
| *(none)* | `0x010C` | — | Sprint-007 block | **Reserved — Do Not Allocate** |
| *(none)* | `0x010D` | — | Sprint-007 block | **Reserved — Do Not Allocate** |
| *(none)* | `0x010E` | — | Sprint-007 block | **Reserved — Do Not Allocate** |
| *(none)* | `0x010F` | — | Sprint-007 block | **Reserved — Do Not Allocate** |

## 3. Classification (as required)

1. **Semantic assignment for every currently required Sprint-007 request.** Exactly two messages are required by the frozen architecture (§16 — "join/respawn requests"): `kMsgPlayerJoinRequest = 0x0100` and `kMsgPlayerRespawnRequest = 0x0101`. Reconnect is **not** a separate id: it is the join request carrying the Sprint-006 reconnect token, resolved through `Session::TryReclaim` — consistent with the frozen architecture (reconnect-without-duplication via the existing token seam, AD-7). Graceful leave is **not** a separate id: connection teardown remains Sprint-006's `kMsgBye (0x0007)` in the control range; duplicating it here is prohibited.
2. **Reserved for future Sprint-007 expansion.** `0x0102–0x0107` (6 contiguous ids) are reserved for later Sprint-007-domain player *requests* only. They are intentionally **unnamed** — no speculative allocation; space is reserved, meanings are not. Naming any of these requires a future additive amendment to this section before use.
3. **Intentionally unused.** `0x0108–0x010F` (8 contiguous ids) are intentionally unused and marked **Reserved — Do Not Allocate**. They exist to keep Sprint-007's footprint at the low end of its block and preserve contiguity; they must not be assigned during Sprint-007 implementation.
4. **Prohibited from reuse.** (a) The control range `0x0000–0x00FF` is prohibited for any player request. (b) The two Used ids (`0x0100`, `0x0101`) are permanently bound; their numbers may never be reissued with a different meaning, even if a message is later deprecated. (c) Ids marked **Reserved — Do Not Allocate** (`0x0108–0x010F`) must not be allocated within Sprint-007. (d) No id in this block may be repurposed as a response, replication, snapshot, or prediction message — this block is request-only.

## 4. Future compatibility (no renumbering required)

- **Within the block.** Sprint-007's own future requests grow upward from `0x0102` into the reserved `0x0102–0x0107` span without disturbing `0x0100`/`0x0101`.
- **Beyond the block.** The entire data-id range **`>= 0x0110`** lies outside this Sprint-007 block and is **not governed here**; it remains fully available for Sprint-008+ (replication, prediction, snapshots) and any future player-interaction domains, each of which claims its own contiguous sub-range additively. Because Used ids are permanent and the high half of this block is left unallocated, no future sprint requires renumbering any id defined here.
- **No speculative cross-sprint allocation.** This section reserves space only; it names no id for any sprint other than the two Sprint-007 requests currently required.

*This mapping is frozen. Any change is additive only (naming a Reserved id) and must amend this section before implementation uses the id.*
