# ADR Index — STALKER-MP Architecture Decision Registry

This document is the **canonical registry** of every Architecture Decision Record (ADR) in the STALKER-MP project. It is the single authoritative source that answers: which ADRs exist, which are active, which have been superseded, and which ADR governs a given architectural area.

If any other document disagrees with this registry about the status or authority of an ADR, this registry and the referenced ADR prevail.

---

## 1. Purpose

**Why ADRs exist.** Architectural decisions are long-lived, cross-cutting, and expensive to reverse. An ADR captures a single decision together with its context, the alternatives considered, the reasoning, and the consequences, so that the *why* survives long after the people and conversations that produced it. In a project designed to be continued by different contributors — human or AI — with no shared chat history, the repository is the only memory; an ADR is how an architectural decision becomes part of that memory.

**Why architectural decisions must be versioned.** Decisions evolve. A later decision may refine or reverse an earlier one. Versioning (numbered ADRs, explicit status, explicit supersession) means the current rule is always unambiguous, while the historical reasoning remains available. Without versioning, a codebase accumulates conflicting, undated conventions that no one can safely change because no one knows which still applies.

**Why implementation documents never override ADRs.** Implementation documents (sprint specs, plans, coding standards, developer notes, generated docs) describe *how* something is built at a point in time; ADRs define the *architectural constraints* that any such implementation must respect. An implementation document that contradicts an Accepted ADR is wrong on that point by definition — otherwise the constraint would be meaningless and could be silently eroded one document at a time.

**Why ADRs exist independently from sprint documents.** Sprints are units of scheduling and delivery; they start, close, and freeze. Architectural decisions outlive individual sprints and frequently span many (ADR-007's Engine ABI Contract governs every sprint through Sprint-015). Binding an architectural decision to a sprint document would cause it to be lost or frozen when that sprint closes. ADRs are therefore a separate, durable track that sprints reference but do not own.

---

## 2. ADR Lifecycle

```
Proposed → Under Review → Accepted → Superseded → Deprecated
```

- **Proposed** — The ADR has been written and submitted. It records a decision the author recommends but that carries no authority yet. Implementation must not rely on it.
- **Under Review** — The ADR is in active architectural review. Its content may still change in response to challenge. It is not yet binding.
- **Accepted** — The decision is approved and **normative**. It binds all lower-tier documents and all implementation within its scope. It remains in force until explicitly superseded.
- **Superseded** — A later ADR has replaced this decision (in whole or in part). The superseded ADR is retained for its historical reasoning; the superseding ADR's number is recorded on both. A superseded ADR is no longer binding but is never deleted.
- **Deprecated** — The decision is no longer relevant (the subsystem was removed, or the constraint no longer applies) and has not been directly replaced by a successor. Retained for history; not binding.

A single ADR moves forward through these states; it never silently reverts. Moving an Accepted ADR to Superseded or Deprecated is itself an architectural action requiring a new or revising ADR.

---

## 3. Authority

**Accepted ADRs are normative.** Within its stated scope, an Accepted ADR is binding on all of:

- Sprint specifications
- Implementation plans and design reviews
- Coding standards and contributing guidelines
- Developer notes
- AI-generated documentation

Each of these **must conform** to every Accepted ADR affecting its area. A statement in any of them that weakens, narrows, or contradicts an Accepted ADR is void on that point and defers to the ADR.

**Only another ADR may supersede an Accepted ADR.** No sprint document, plan, guideline, note, or generated document can override, relax, or retire an Accepted ADR. The sole mechanism for changing an accepted architectural decision is a new (or revising) ADR that explicitly supersedes it and is itself Accepted.

---

## 4. ADR Register

The register lists every ADR currently known to the project. Reserved numbers are shown only where a number has been deliberately set aside for a pending ADR; numbers that do not correspond to a written ADR are not invented.

| ADR | Title | Status | Supersedes | Scope | Notes |
|---|---|---|---|---|---|
| ADR-007 | Engine ABI Contract (Exception and STL Usage Policy) | Accepted | — | Every engine-linked translation unit (all TUs compiled into `xrMP.lib`) through Sprint-015; excludes the standalone `xrMP_Tests` executable | Defines the seven-invariant Engine ABI Contract and the Conformance Rule. File: `Documentation/Architecture/ADR-007-Exception-And-STL-Usage-Policy.md`. Cross-references `Documentation/Engine_Integration/Build_Model.md` and `Build_Compatibility.md`. A pointer entry (D-007) in `Documentation/AI/DECISIONS_LOG.md` is a pending follow-up per ADR-007 §10.1, not yet recorded. |

**Note on the D-series.** Earlier project decisions D-001..D-006 live in `Documentation/AI/DECISIONS_LOG.md` as a chronological decision log and predate this ADR system. They are not ADRs and are not listed here; they retain their own historical status in that log. ADR-007 is the first decision formalized as an ADR. Future architectural decisions are recorded as ADRs and registered here.

---

## 5. How Future ADRs Are Added

```
Problem discovered → Design Review → Alternatives evaluated →
ADR written → Architecture Review → Accepted → Implementation allowed
```

1. **Problem discovered** — an architectural question or conflict is identified.
2. **Design Review** — the problem is investigated with evidence; the affected invariants and existing ADRs are gathered.
3. **Alternatives evaluated** — candidate approaches are compared on their long-term consequences, not on immediate convenience.
4. **ADR written** — a numbered ADR is drafted (Proposed), stating context, options, decision, and consequences.
5. **Architecture Review** — the ADR is challenged (Under Review); the strongest counterargument is stated and must be survived.
6. **Accepted** — on approval the ADR becomes normative and is registered in §4.
7. **Implementation allowed** — only now may implementation proceed.

**Implementation must never begin before an Accepted ADR when the issue affects architecture.** For non-architectural work, ordinary sprint workflow applies; when in doubt whether a change is architectural, treat it as architectural and require an ADR.

The next available ADR number is assigned at drafting time and reserved in §4 if drafting will span multiple sessions.

---

## 6. Relationship to Other Documentation

```
ADR
  ↓
Architecture documents
  ↓
Sprint Specifications
  ↓
Implementation
  ↓
Tests
  ↓
Developer Notes
```

**Higher layers govern lower layers.** Each layer must conform to every layer above it:

- **ADRs** define binding architectural constraints.
- **Architecture documents** describe the system within those constraints.
- **Sprint specifications** schedule and scope work that must respect the architecture.
- **Implementation** realizes the sprints without violating any ADR.
- **Tests** verify the implementation.
- **Developer notes** are the most local and carry the least authority.

A lower-layer document may add detail but may never relax a higher-layer rule. A conflict is always resolved in favor of the higher layer, and ultimately in favor of the governing Accepted ADR.

---

## 7. AI Guidance

Before proposing or making any architectural change, an AI assistant **must**:

1. **Read `ADR_INDEX.md`** (this document) to learn which ADRs exist and their status.
2. **Read every Accepted ADR affecting the area** of the proposed change.
3. **Verify the proposal violates no Accepted ADR** — check it against each relevant invariant and constraint explicitly.
4. **If a contradiction exists:** do **not** modify implementation, build configuration, or project files to force the change through. Instead, **propose a new ADR** that documents the conflict, evaluates alternatives, and — if the decision is to change course — explicitly supersedes the affected ADR. Implementation waits until that ADR is Accepted.

An AI must never treat an Accepted ADR as advisory, must never work around it silently, and must never introduce an unwritten architectural rule. Recalled or summarized context does not override the registered ADRs; when in doubt, re-read the ADR itself.

---

## 8. Maintenance Rules

- **ADRs are append-only historical records.** New decisions are added as new ADRs; the history is not rewritten.
- **Accepted ADRs are never edited except for typo corrections.** Any substantive change to an accepted decision requires a new (or formally revising) ADR, not an in-place edit.
- **Architectural changes require a new ADR.** There is no other sanctioned path to change an architectural decision.
- **Superseded ADRs remain in the repository.** They are marked Superseded (with the superseding ADR recorded) and retained permanently for their reasoning; they are never deleted.

This registry is updated only to add a new ADR, to change an ADR's recorded status (e.g. Proposed → Accepted, Accepted → Superseded), or to correct a typo. Every such update is itself governed by the maintenance rules above.
