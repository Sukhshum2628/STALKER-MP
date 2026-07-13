# Sprint-001 — Windows / MSVC Release Verification Checklist

Run on the target Windows machine with Visual Studio 2022 (v143 toolset).
Sprint-001 is frozen: during verification, fix only compiler errors, linker
errors, runtime crashes, initialization failures, and broken tests.

## A. Solution and project loading

- [ ] 1. Open `Engine/xray-monolith/src/engine-vs2022.sln`.
- [ ] 2. Every project loads (no "load failed" / "incompatible" in Solution
      Explorer). Expected new entry: **xrMP** at the solution root.
- [ ] 3. xrMP reference check: right-click xrEngine → Build Dependencies →
      Project Dependencies → xrMP is checked. xrEngine → Properties →
      Linker → Input contains `xrMP.lib` (all configurations).
- [ ] 4. Dependencies: none to restore for xrMP (no NuGet, no submodules).
      GoogleTest is vendored at `Multiplayer/third_party/googletest` —
      verify `include/gtest/gtest.h` and `src/gtest-all.cc` exist on disk.
- [ ] 5. GoogleTest discovery: open `Multiplayer/xrMP.sln` → xrMP_Tests →
      Properties → C/C++ → Additional Include Directories shows the two
      googletest paths; Solution Explorer shows `gtest-all.cc`.

## B. Build

- [ ] 6. Module Debug build: `Multiplayer/xrMP.sln` → Debug|x64 → Build.
      Expect: xrMP.lib + xrMP_Tests.exe, zero warnings from xrMP code
      (gtest-all.cc may warn — third-party, permitted).
- [ ] 7. Engine Release build: `engine-vs2022.sln` → your usual config
      (e.g. DX11|x64) → Build. xrMP builds first, xrEngine links last.
- [ ] 8. Compile errors: expected none. If MSVC flags anything GCC
      accepted, report before fixing (Sprint-001 hotfix protocol).
- [ ] 9. Linker errors — two known tripwires:
      - `LNK2038 RuntimeLibrary mismatch`: xrMP assumes the engine's
        default `/MD`-family runtime. If the engine config overrides it,
        align `<RuntimeLibrary>` in `Multiplayer/xrMP.vcxproj` — report
        first.
      - `LNK1104 cannot open xrMP.lib`: config-mapping issue — check the
        solution config maps xrMP to Release/Release-AVX/ProfiledDX11/
        Verified as appropriate, and that xrMP built into
        `_build/_game/bin_dbg/x64/<Config>/`.

## C. Runtime

- [ ] 10. Launch the game via the built engine (normal Anomaly launch).
- [ ] 11. Engine reaches the main menu; play a few minutes of any level —
      behavior identical to a pre-STALKER-MP build.
- [ ] 12. Engine log (console / xray log) contains, in order:
      `* Initializing STALKER-MP...` → config directory line → log
      directory line → `* Checking engine compatibility...` →
      `* STALKER-MP 0.1.0 (compat 1, ...) initialized successfully`.
- [ ] 13. `%APPDATA%` app-data root (as configured by fsgame.ltx, typically
      the `appdata` folder of the install) contains
      `stalkermp/config/` and `stalkermp/logs/stalkermp.log`; the module
      log shows the full Bootstrap sequence ending in
      "STALKER-MP initialized".
- [ ] 14. Each of `development.cfg`, `server.cfg`, `client.cfg` contains
      `[meta]` / `config_version = 1`.

## D. Tests

- [ ] 15. Run `Multiplayer/_build/tests/Debug/xrMP_Tests.exe` (and Release).
- [ ] 16. Expect: `[==========] 46 tests from 12 test suites ran.` /
      `[  PASSED  ] 46 tests.` in both.

## E. Debug vs Release comparison

- [ ] 17. Both test builds pass identically; engine startup log sequence is
      identical in both engine configs you build. Known intended
      difference: SMP_ASSERT is compiled out under NDEBUG, SMP_VERIFY
      reports-and-continues in Release vs traps in Debug — not observable
      unless an assertion fires.

## F. Warning triage

- [ ] 18. Record any MSVC warnings emitted from `Multiplayer/` sources
      (they are `/W4 /WX`, so warnings there are build errors by
      definition — anything that appears is a hotfix candidate). Warnings
      from engine or gtest code are out of Sprint-001 scope.

## Negative test (optional but recommended)

- [ ] Temporarily rename the `stalkermp` app-data folder's
      `development.cfg` to contain `config_version = 99`; relaunch; expect
      init failure lines + "Continuing as single-player" and an unaffected
      game. Restore the file afterwards.

Verification sign-off freezes Sprint-001 as milestone M1.
