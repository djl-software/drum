# Drum repository rules

These instructions apply to every platform, branch, chat and machine working in this repository. User instructions take precedence.

## Read and maintain the handover

- Before changing code, read `Common/handover/LIVE.md` and the relevant platform handover in `Common/handover/`.
- All authoritative handover notes must live in `Common/handover/`. `LIVE.md` is the current cross-platform status, decisions and source-migration register. Platform READMEs may link there but must not become competing handovers. Chats, temporary logs and machine-local files are not the handover of record.
- Update the live handover in the same change as implementation, build selection, source ownership or validation changes. Before committing or handing work to another chat/machine, record what changed, affected platforms, actual validation, unresolved problems and the next action. State when a platform has not been compiled or tested.
- Keep build commands, prerequisites, feature-parity gaps and machine handover instructions current in the relevant platform note. Do not describe Mac test results as Windows validation.

## Shared source safety: split when there is doubt

- `Common/` source is shared only while it is demonstrably safe for all its consuming platforms. Before editing a shared source file, assess Windows, Mac, Android and iPad impact, including host APIs, UI/window behavior, audio/routing, state compatibility, toolchains and dependencies.
- If the proposed edit may cause problems on another platform, or its cross-platform safety is uncertain, that implementation must no longer remain common. Split it before applying the risky change. Do not rely on another platform eventually fixing a regression.
- Preserve the pre-change implementation in unchanged copies under the other affected platform source folders (`Windows/src/`, `Mac/src/`, `Android/src/`, `iPad/src/`, preserving useful subdirectories). Move the active platform's implementation to its own source folder and make the intended change only there. For a planned platform without a working target, retain the applicable baseline copy and explicitly mark it unvalidated; do not invent a successful build.
- Update CMake/project files and include paths so each platform compiles exactly its own implementation and never both the old Common file and its platform replacement. Move paired private headers as needed; only a stable, genuinely shared interface may remain in Common.
- A new `#if` inside the risky Common implementation is not a substitute for the required split. Existing gated/partially extracted code is legacy debt, not an exemption: apply this rule when next making a potentially unsafe change to it. Do not mass-move unrelated files merely to satisfy a checklist.
- Every move/copy must be recorded in `Common/handover/LIVE.md`: original Common path, new paths for every affected platform, baseline/source revision, which copy changed, build-selection changes, validation and remaining porting work. Distinguish complete moves from partial extractions.
- Validate the changed platform and check unaffected platforms where their toolchains are available. Otherwise record the exact missing validation and the preserved baseline. Never silently claim platform parity.

## Branch and artifact discipline

- `development/mac` is Mac development; `development/windows` is Windows development. The user requested `main` to match the Mac branch. Do not merge Windows-specific changes into main without a new user instruction.
- Keep generated builds, installers, VM disks and ISO downloads out of Git. Follow `.gitignore`; do not add the cancelled Windows media download.
- Do not change another platform's implementation as an incidental part of a platform fix. Make explicit ownership and handover decisions first.
