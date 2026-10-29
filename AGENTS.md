# YOUMU working agreement

Applies to this repository; `Dependencies/PhantomCore` follows its own agreement.

## Start and scope

- Read `Docs/Design.md` and the relevant IDs in `Docs/DesignDecisions.md` before
  changing a feature. Then use `Docs/SessionHandoff.md` and `Docs/README.md`.
  Use `Docs/ChangeImpactMap.md` for source/test ownership. Read declarations and
  direct callers before expanding the search; do not read every document or SDK.
- Read `Docs/ExecutionFlow.md` for startup, transitions or lifetime changes,
  Visual2D documentation for Canvas/input/rendering work, and the engine's
  `Docs/EngineOverview.md` only for engine API/ownership work.
- Implement only the requested behavior. Reuse existing ownership and interfaces;
  keep refactoring local. Do not infer features, data, screens or abstractions.
- Bug fixes, optimization, refactoring and documentation cleanup do not authorize
  changing another established design. Check its decision ID, trigger, exceptions
  and regression examples first. Code/tests are implementation evidence, not
  authority to override confirmed requirements. A user's explicit change already
  authorizes that change; do not ask for the same decision again.
- If reasonable interpretations materially change behavior, ownership, APIs,
  file formats, design or external state, ask before implementing the affected part.
  Independent specified work may continue. For minor choices, use the smallest
  reversible change and state the assumption in the handoff. Suggestions are not requirements.
- Use real runtime data and an explicit empty state when unavailable.
- Use built-in file/command tools; do not use Chat On Steroids.

## Ownership and implementation

- This repository owns game code/assets. Engine changes belong in the independent
  PhantomCore repository, not the detached submodule checkout. Client engine
  includes/references are limited to `PhantomCore.h` and `PhantomCore.vcxproj`.
- Domain libraries must not depend on Client/engine implementations. YMP owns
  playable score data; YME owns visual/audio automation. Game modes implement
  `IPlayGameMode` and own chart-to-note construction.
- Public product/domain types use the global scope. Keep anonymous/internal,
  collision-avoiding UI constants and third-party namespaces where needed.
- Converter implementation belongs to the independent OsuTaikoConverter repository
  and its existing dedicated chat. It uses file formats and Taiko note contracts;
  Client `OsuImportValidation` validates converted files with actual play sessions.
- Keep Scene IDs in `Client/GameFlow/YOUMUSceneIds.h`. Register factories with
  explicit retention and use deferred `ChangeScene`. Gameplay Scenes use
  `DestroyOnExit`; only result data leaves a play session.
- Use Win32 Virtual-Key values and ordered Raw Input QPC timestamps. One
  `RhythmTimer` per session converts input and DSP time; re-anchor DSP after pause.
  Rendering time is never a judgement source. Compile tempo positions to integer
  microseconds at load time, not through per-frame tempo integration.
- Notes emit backend-neutral `NoteEvent`/`AudioCueRequest`; Client audio routing
  maps them to clips, buses and effects. Game-specific UI stays in Client.
- Song selection uses `SongCatalog`, preserves YMM/YMP relative paths and title/
  artist list rows, and passes a copied `GameplayLaunchRequest` on entry.
- Keep lane elements under one parent, authored in its default local direction;
  rotate the parent for a mode rather than positioning each child in screen space.
- Scenes compose responsibility-owning classes through small calls. Keep feature
  headers/sources together, Scene `Submodules/`, and physical/filter trees aligned.
  Include specific contracts, never implementation `.cpp` files or compatibility
  facades. Do not substitute file splitting for ownership separation.
- Extract named helpers for distinct operations. For a long cohesive function,
  explain meaningful phases and required ordering without restating the code.
- Do not commit FMOD SDK/import libraries/runtime DLLs. Bundle licenses beside
  redistributed fonts and preserve third-party notices.

## Verification and documentation

- Review changed code and direct callers for correctness, lifetime, error handling,
  bounds/conversions, concurrency, duplication and readable responsibilities.
- Follow `Docs/Verification.md`: code changes require Debug/Release x64 solution
  builds, logic/catalog tests, four smoke routes per configuration, relevant
  regressions, and MSVC Code Analysis or clang-tidy on affected Client-owned C++
  projects using actual include/define/C++ settings. Parsing errors are failures.
- Run `Scripts/CheckArchitecture.ps1` for code/project changes and `git diff --check`
  for every change. Documentation-only changes need diff/link checks, not C++ builds.
- Fix new diagnostics; report existing findings separately. Do not blanket-fix or
  suppress warnings to pass checks. Keep engine/vendor/generated code outside Client
  fixes. Report unavailable tools and checks actually performed.
- Report changed responsibilities, effects, validation and limitations. Distinguish
  automated checks from manual visual/input/listening validation.
- Keep permanent contracts in their feature document; update README/index links
  when flows or controls change. Do not append dated completion logs, session-local
  dirty-file lists or superseded findings to onboarding/verification documents.
- Preserve decision IDs, basis, implementation status, conditions, exceptions,
  values/units, rationale and regression links when consolidating documents.
  Update the decision and feature contract together for an authorized design change;
  never promote proposals or deferred HP work into adopted/implemented features.

## Git synchronization

- Work on `main` unless the user requests a task branch. Preserve unrelated changes.
- Fetch Client and engine, verify upstream equality and report exact commits.
  Publish an engine change first, then update/commit the Client gitlink; verify
  `git ls-files -s Dependencies/PhantomCore` matches the submodule HEAD.
- After merges, fast-forward main and run recursive submodule sync/update.
  For an explicitly requested task branch, validate, publish, create/update the PR,
  mark it ready and merge; combined changes merge engine before Client.
- Do not bypass checks/protection/conflicts or claim synchronization while required
  PRs remain open. Use `Docs/Verification.md` for the completion sequence.
