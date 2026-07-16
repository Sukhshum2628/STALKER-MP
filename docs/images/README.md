# Documentation Images

This folder holds diagrams and illustrations referenced by the top-level `README.md`.

Diagrams are shipped as **SVG** (text-based, versionable, and rendered natively by
GitHub). Keep filenames stable so README references do not break.

| File | Status | Purpose |
|---|---|---|
| `architecture-overview.svg` | ✅ present | High-level module/boundary diagram (engine ↔ adapters ↔ subsystems). |
| `frame-tick-order` | inline Mermaid (README) | Deterministic per-frame `tick_order` pipeline (100 → 900). |
| `snapshot-dataflow` | planned | Sprint-008 snapshot build → publish → consume flow. |

The README also embeds equivalent Mermaid diagrams (rendered inline by GitHub) so
the documentation is complete regardless of image assets.
