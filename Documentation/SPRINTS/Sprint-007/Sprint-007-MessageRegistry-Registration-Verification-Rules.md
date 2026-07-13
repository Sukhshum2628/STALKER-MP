# MessageRegistry Registration Verification Rules

> **Governance scope.** This section freezes the verification rules for registering Sprint-007 player-request message ids in the existing `net::MessageRegistry`. It is verification-governance documentation only. It modifies no sprint scope, architecture, ADR, implementation order, ownership boundary, networking architecture, message-id allocation, wire format, packet layout, or serialization. It introduces no new `MessageId`, packet type, implementation, or test code. It governs how registration is *verified*, so implementation and independent verification interpret it identically. All ids referenced are those already frozen in `Sprint-007-Player-Request-Message-ID-Allocation.md`; the authoritative source of the constants is `include/stalkermp/player/PlayerMessageIds.h` (single-source-of-truth).

## 1. Frozen inputs (restated, not redefined)

- **Used (must be registered):** `kMsgPlayerJoinRequest = 0x0100`, `kMsgPlayerRespawnRequest = 0x0101`.
- **Reserved for future Sprint-007 work (must NOT be registered):** `0x0102–0x0107`.
- **Reserved — Do Not Allocate (must NOT be registered):** `0x0108–0x010F`.
- **Out of scope for this block:** control range `0x0000–0x00FF` (Sprint-006) and data ids `>= 0x0110` (future sprints) — not registered by Sprint-007.

## 2. Registration completeness (rule R1)

- **R1.1** Every id marked **Used** (`0x0100`, `0x0101`) MUST be registered in the `MessageRegistry` exactly once during Sprint-007 service initialization.
- **R1.2** After initialization, a completeness check MUST confirm the registry contains a resolvable entry for each Used id. Absence of any Used id is a **verification failure** (missing-registration failure, see R6).
- **R1.3** The set of Sprint-007-registered player ids MUST equal the Used set exactly — no more, no fewer. A registered id outside `{0x0100, 0x0101}` is a verification failure (see R3).

## 3. Registration uniqueness (rule R2)

- **R2.1** Duplicate registration of the same `MessageId` is prohibited. Each Used id is registered once and only once.
- **R2.2** A duplicate-registration attempt MUST be surfaced as a **deterministic initialization error** (a value outcome, ADR-007 — never a silent overwrite, never an exception). The initializing service MUST treat this outcome as a hard failure that fails `Initialize` deterministically, not a warning.
- **R2.3** Determinism of the error: given the same registration inputs, the duplicate outcome MUST be identical on every run and on both toolchains (GCC + MSVC).

## 4. Reserved ids (rule R3)

- **R3.1** Every id marked **Reserved for future Sprint-007 work** (`0x0102–0x0107`) MUST remain **unregistered** throughout Sprint-007. A lookup of any of these ids MUST resolve to "no handler".
- **R3.2** Every id marked **Reserved — Do Not Allocate** (`0x0108–0x010F`) MUST remain **unregistered**. A lookup MUST resolve to "no handler".
- **R3.3** Presence of any Reserved id in the registry is a **verification failure** (reserved-id-present failure). This is the negative counterpart to R1.

## 5. Handler correctness (rule R4)

- **R4.1** Each **Used** id MUST resolve to **exactly one** handler after registration (one-to-one id→handler).
- **R4.2** No handler may be bound to more than one player-request id unless a future architecture revision explicitly documents and authorizes the sharing. Under the current frozen architecture, `0x0100` and `0x0101` bind to **distinct** handlers.
- **R4.3** A Used id resolving to zero handlers (R1 gap) or to more than one handler, or two Used ids sharing a handler without authorization, is a **verification failure** (incorrect-handler-binding failure).
- **R4.4** Handler identity is verified structurally (each Used id maps to its own distinct, intended handler), not by invoking wire behavior — this section governs registration, not message semantics.

## 6. Determinism (rule R5)

- **R5.1** Runtime behavior MUST NOT depend on the order in which the Used ids are registered. Registering `0x0100` then `0x0101`, or `0x0101` then `0x0100`, MUST yield an identical resolvable registry state.
- **R5.2** Lookup results MUST be deterministic: the same id resolves to the same handler on every call and every run (consistent with the registry's sorted-key + accelerator discipline, BC-005).
- **R5.3** Replay behavior MUST remain unchanged regardless of initialization order: because id→handler resolution is order-independent and side-effect-free at lookup time, the per-tick control flow (and therefore the Sprint-007 replay property) is invariant to registration order.
- **R5.4** No registration step may introduce wall-clock, address-ordering, or hash-iteration-order dependence into control flow; resolution is by `MessageId` value only.

## 7. Independent verification requirements (rule R6)

Independent verification (Antigravity) MUST confirm all of the following before the corresponding step is accepted:

- **R6.1 Every required id exists.** Both Used ids (`0x0100`, `0x0101`) are registered and resolvable exactly once. *(Guards R1.)*
- **R6.2 Every reserved id is absent.** No id in `0x0102–0x010F` is registered; each resolves to "no handler". *(Guards R3.)*
- **R6.3 Duplicate registration is rejected.** Attempting to register an already-registered Used id produces the deterministic initialization-error outcome and fails `Initialize`; it does not overwrite or silently succeed. *(Guards R2.)*
- **R6.4 Missing registration fails verification.** If any Used id is not registered, the completeness check fails and the step is rejected. *(Guards R1.2.)*
- **R6.5 Incorrect handler bindings fail verification.** A Used id resolving to zero or multiple handlers, or two Used ids sharing a handler without authorization, fails verification. *(Guards R4.)*
- **R6.6 Order-independence confirmed.** Verification demonstrates identical resolvable state and identical lookups under at least two distinct registration orders of the Used ids. *(Guards R5.)*

## 8. Failure classification

Every rule violation is a deterministic, value-outcome failure (ADR-007 — no exceptions, no RTTI): missing-registration (R1/R6.4), duplicate-registration (R2/R6.3), reserved-id-present (R3/R6.2), incorrect-handler-binding (R4/R6.5), and order-dependence (R5/R6.6). None is a warning; each blocks acceptance of the registration step until resolved. Registration is treated as all-or-nothing at service `Initialize`: a failed registration leaves the service uninitialized rather than partially registered, so no partial-ownership state ever exists.

## 9. Scope boundary

These rules govern **registration and its verification** only. They define no message payload, no handler behavior, no wire structure, and no new id — those remain governed by the frozen architecture, the frozen implementation plan, the frozen message-id allocation, and ADR-010. Any future change (e.g. naming a Reserved id or authorizing a shared handler) is additive and must amend the relevant frozen document before these rules apply to the new id.

*These verification rules are frozen. Step Specifications and independent verification MUST interpret registration exactly as defined here.*
