# Presentation regression tests

## Play options panel

`RunMusicSelectViewTests.ps1` includes `PlayOptionsPanelTests.cpp`: full option/uint64
round trips, Done/Esc apply once, complete Reset defaults, invalid/hidden field repair,
conditional controls, the HP gauge choice and its `Clear 60%` caption, the changed-option
badge count, the flat modal size and its scaling on a 4:3 canvas, native-input visibility
and stale action rejection. The actual Win32/engine route verifies footer entry and badge,
numeric editing, Esc isolation, Reset/Done, preserved selection/search, rendering and cleanup.
It replaces the removed native PlayOptionsDialog form test; PlayOptions value/range,
launch snapshots and Retry contracts remain covered by domain/gameplay tests.

## Options drawer and explanation languages

`RunOptionsPanelTests.ps1` covers the 432px drawer and navy rail, rail clicks that
scroll a section to the top, section tracking while scrolling (the last section at
the bottom), Up/Down navigation, delayed rail expansion and folding, popup/input
cleanup, the footer notice outside the body, kept scroll position on reopening,
volume dragging that saves once on release and rejects values outside 0..100, key
slots with hover-only clear buttons and the faint plus, unchanged device/key/
resolution persistence and Japanese round trips. It also drives the shared tooltip:
the half-second rest, a pointed child resolving to its registered ancestor, key
chips for `Ctrl+O` and `Esc / Enter`, the quick switch to a neighbouring control,
dismissal by a press until the pointer moves on, host suppression and opening above
a control at the bottom edge. Tooltip lines inside a control that keeps the pointer
(card titles) show only while pointed inside that control, and clearing forgets every
hint, and a long name wraps inside the 300-wide plate. The shared scrollbar is a 12px strip with a centred 6px bar (8/4 horizontally)
that hides without overflow, keeps its 32px minimum, darkens on hover and turns solid
while held; dragging keeps the grab point, a rail press centres the bar, and an
event-following host (the drop-down list) drags and wheels through the strip's own
pointer events. Drop-downs are the shared UiDropdown: list rows take
clicks ahead of the fields under them, and a press outside an open list only closes it.
The shared alert is checked for its layout, Enter/Esc answers, button clicks, the blocking
scrim and detail scrolling; the result screen retries a failed record save through it, and
song select keeps Enter and Esc away from the screen while it is open. `RunLanguageFileTests.ps1`
checks English captions/value formats, selected EN/KO/JA explanations, complete
Japanese explanation-key coverage and existing immutable pack/fallback contracts.
Japanese adds `ja` to the same INI Language field; stored ko/en remain valid.

## Editor dialogs and song preview

`RunEditorPresentationTests.ps1` also checks the editor's shared alert: Escape with unsaved changes asks
Discard/Cancel/Save, editor shortcuts wait behind it, Escape cancels and Discard leaves, without saving the
bundled chart. A successful save publishes the YME it wrote. Its preview section plays the
bundled song through SongPreviewController by audio path: no reload for the focused song, pause and stop
keeping the voice, seeking in place, the end of a song reported once (title) or restarted (song select),
failed files not reopened every frame, and released audio reopening on return.

## Lane key beam regression test

Build the Client solution first. From an x64 Visual Studio developer command
prompt in the repository root, compile the presentation test against the same
configuration's Client object and engine library (set `FMOD_ROOT` to the local SDK):

```bat
cl /nologo /std:c++20 /EHsc /MDd /D_DEBUG /DNOMINMAX /IClient /IYOUMU.Rhythm /IDependencies\PhantomCore\Engine\SDK Tests\Presentation\LaneKeyBeamTests.cpp build\obj\YOUMU.Client\x64\Debug\LaneKeyBeam.obj build\obj\YOUMU.Client\x64\Debug\Pch.obj bin\x64\Debug\YOUMU.Rhythm.lib Dependencies\PhantomCore\bin\x64\Debug\PhantomCore.lib /Fobuild\obj\LaneKeyBeamTests.obj /Febin\x64\Debug\LaneKeyBeamTests.exe /link /OPT:NOICF /OPT:NOREF /LIBPATH:"%FMOD_ROOT%\api\core\lib\x64" user32.lib gdi32.lib ole32.lib
bin\x64\Debug\LaneKeyBeamTests.exe
```

For Release, replace `Debug` with `Release`, `/MDd /D_DEBUG` with `/MD /DNDEBUG`.
Include the Client `Pch.obj`: the production key-beam object uses its PCH.
Release may additionally pass `/LTCG` to the linker to avoid its automatic restart.
The test verifies free input, judgement colors/interpolation, input rejection,
roll ticks, missed-note rollover, alpha at 0/100/200ms, retrigger, sprite reuse,
inherited Lane rotation/scale, resize clipping, and reset. It does not initialize
an audio device or renderer; normal gameplay smoke covers resource loading.

## Music selection viewport and catalog worker

Run `./Tests/Presentation/RunMusicSelectViewTests.ps1 -Configuration Debug`
and the same command with Release from the developer PowerShell.
The test links production view/state objects, checks that 5,000 in-memory songs
create only viewport cards across focus/search/resize, and polls real catalog
loads/cache reuse/error results. It writes no song or option files.
Both gameplay and editor selection check that clicking another difficulty only selects it and
that clicking the selected difficulty launches exactly once.
Both also select Base BPM through the actual dropdown, verify numeric order and
stable ties with retained song focus, then check language switching and search.
Both drag the song list's shared scrollbar away from and back to the first card, show
the now-playing mark 21px before the focused title only while its song plays, and
hint every name of a card title with several names but nothing for a single fitting
artist. The F7 band test checks its dash before measurement, `FPS 144 | UPS 1000`,
draw calls only in Debug builds and its bottom-right corner.
Song filter regressions verify visible pattern projection with original indices,
modal navigation/launch isolation and responsive filter/sort controls in both purposes.
The hidden native-window run edits real range fields, tests IME versus Enter, validates
atomic Apply/Reset/Close and invalid bounds, hides fields under the mode popup and checks
scene-hide cleanup. Difficulty remains empty/disabled while all runtime values are unknown.
Favorite-folder coverage runs both selection purposes through real folder tabs,
horizontal wheel scrolling, projected keyboard navigation, transactional membership,
native Unicode create/rename/cancel dialogs, active-folder deletion, empty selection,
independent folder-list scrolling and language changes. The hidden D3D12 run also
checks search hiding and modal Escape/options-key isolation. Persistence, malformed
JSON/Unicode, failed atomic replacement and chart preservation are tested in the logic suite.
Selected BG coverage checks both song-select purposes for full-screen image reuse,
aspect/offset framing across resize, same-path offset changes and clearing/fallback.
Song-list regressions verify no thumbnail nodes or BG requests for collapsed cards,
one-line title/artist captions and unchanged selected-chart jacket rendering.
Frame tests resize to narrow/normal/wide: the three columns are plain nodes without panel sprites,
one sheet starts at x 362 with its shadow band and reaches the right screen edge (game and editor alike,
the extension only on a wider screen), a thin rule separates details and list, no veils or ALL bar exist,
record rows, the notice and the page controls sit on plates of one width from the left screen edge,
the board keeps the 16:9-or-wider
arrangement and scales as a whole below 16:9 with letterbox bands, and the song list stays on the right edge.
Skin tests check that a selected skin replaces the sheet/button/card images and that returning to
the default skin restores the built-in parts.
ResultPresentationTests checks the shared tooltips of Back (keys only), the score and an
option chip, none on the rank, and hiding with the view. It also checks the BG value snapshot, reference-height offset conversion,
resize/language rebuild, image failure fallback and actual D3D12 retention after gameplay destruction.
GameplayJudgementTests retains its existing full-screen BG/offset regression.
Record history remains in its assigned panel with real SQLite paging and a separate
record-filter modal. Tests edit the legacy direct condition through that modal and
verify deferred Apply, actual matching rows, retained panel bounds and chart changes.
The hidden native-window run checks record ranges, invalid bounds, IME/Enter isolation,
committed-value restoration, Close/Escape/scene-hide cancellation, combo popup occlusion
and preservation of song search/selection. RecordFilterTests verifies inclusive AND
conditions against an isolated database, uint64 seeds, direct OR/NOT grouping and invalid input.
HP information checks both selection purposes for exact hundredths, the 9.00
default, pattern/language changes, empty-state clearing and the figures sitting side by side
inside the detail column at narrow, standard and wide viewport sizes.
Representative-name checks cover both selection purposes: only the first name, artist and maker
appear (one line each) in the details and on cards, wheeling over a card caption scrolls the list,
the global options block that scroll, and the hidden D3D12 integration run renders the one-line title.
It also checks marquee Unicode preservation, shared repeated text payloads,
pixel displacement and stopping after the label becomes wide enough.
`SongSearchInputTests` runs the production view in a hidden D3D12/NoSound window.
It inserts English/Korean/Japanese text into the actual Unicode edit control,
checks single-word and AND results, prevents editing/IME keys from invoking Scene
commands, and verifies options visibility and native-control cleanup. It does not
operate the user's clipboard or perform manual IME candidate selection.
Options-button input closes an already open panel. Catalog refreshes queue only
the latest Songs root, discard obsolete results and handle switching back mid-load.

## Gameplay judgement guide and overlays

GameplayJudgementTests checks numeric HUD text, combo inside the key layout without
overlapping keys, exact accuracy-to-100-million score, eight live judgement counts,
rewind/reset restoration, narrow/wide wrapping and reuse of numeric nodes/images.
PlayResultTests covers logical-note combo rules, all seven long-note kinds, BAD/POOR,
terminal/rejection event order, retirement and maximum combo transfer. ResultPresentationTests
decodes numeric sprite UVs to verify final score and maximum combo against actual result data.

ResultPresentationTests also checks the unchanged HP plot with real curve geometry, used-only
Pause/Rewind summaries inside the plot, language rebuilds, unavailable versus valid empty history,
deactivation-time cutoff and clipped finite geometry. HpGaugeTests verifies that real HP/history
stop at menu pause/Rewind, survive Resume/rewind/retirement, transfer by value and reset on Retry.

Failure-overlay cases cover the HP pass boundary, eligible failure, high/low-HP retirement,
pause/Rewind-disabled completion and unavailable HP. Only failure receives an annotation inside
the lower HP plot; result HP/history are never rewritten. GameplayPauseTests retains the real
Retire regression that all remaining notes become MISS without pending logical notes.

HP 회귀는 GameplayJudgementTests의 틀 위치·채움 잘라 보이기·클리어 기준에서 그림 전환·클리어 표시·자물쇠·크기 변경과
GameplayPauseTests의 유효 노트가 없는 채보 진입 차단을 포함합니다.
수치·롱노트·POOR·되감기·종료 판정과 포맷/기본값 검사는 Rhythm의 HpGaugeTests가 담당합니다.

플레이 화면 회귀(UI-02)는 GameplayJudgementTests의 2px·50% 마디선, Kiai 판정원(곡 시간 250ms 페이드·박 맥동·뒤로 이동 시
바로 바뀜·레인 크기로 잘린 빛), 공용 화면 오류 줄(종류·경로와 행·원인, 위 20 가운데, 최대 폭에서 한 줄로 잘림, 좁은 화면 여백, 비우면 숨김)과
남은 타격 배지 회귀를 포함합니다. 로딩 준비 실패 때의 메뉴 모양(진행 띠 없음·값 유지·Retry 선택)은 GameplayPauseTests의 TestMenu,
컨트롤러 상태(`PreparationFailed`·`AttemptStarted`·`ErrorSource`)는 같은 러너의 실패 자원 회귀가 확인합니다.

After a solution build, run in an x64 Visual Studio developer PowerShell with
`FMOD_ROOT` set to the installed SDK:

```powershell
./Tests/Presentation/RunGameplayJudgementTests.ps1 -Configuration Debug
./Tests/Presentation/RunGameplayJudgementTests.ps1 -Configuration Release
```

The test links the production Presenter/JudgementView/key-beam objects and reads
real fallback PNG metadata. It checks independent band colors, JudgeLevel-based
lengths, fixed base windows during syncopation, +/-10ms margins, early/late/zero
and clamped markers, all seven overlays, MISS without invented input timing,
POOR only on red-beam rejection, out-of-range/release/unjudged-tick suppression,
200ms fade/retrigger/reset, inherited circle centre, resize, node/image reuse,
and accepted-hit priority over a passive MISS.
Hit lines are 48 tall on the 12px bar; at most 50 stay live (64 pooled nodes), each fading over 5s,
and a displaced line fades in 100ms. The test checks overflow, lifetime, resizing and node reuse.
The translucent `Hud.TimingPanel` is drawn untinted as five abutting pieces (fixed wings and centre,
stretching columns) that follow the JudgeLevel width with a 300 minimum, with EARLY/LATE in the wings.
The mean-error tab shows the explicit empty value, a signed tenth-of-a-millisecond mean from the live
statistics (`+` late, U+2212 early, unsigned zero) and is unaffected by hit-line resets.
Key-overlay flashes share the Don/Kat red/blue palette. Background tests use real
PNG metadata to verify aspect-preserving cover and 2D offsets at four viewports.
It moves the shared ScrollGear parent and checks that every descendant moves together while
the screen HUD stays fixed, and that the outer gear bottom remains on the centre line after resizing.
It checks chronological note stacking across Separate speed changes and backward seeks, with notes
in front of the judgement circle and its overlays. Counted rolls, big rolls, balloons and DengDeng also
verify remaining-hit visibility/decrement, extra-hit clamping, completion hiding and reset.
It checks coincident note/measure centres under Whole and Separate at several times and viewport sizes,
and all four count-only roll heads for suppressed timing feedback, white beams and OK/MISS completion.
Both DengDeng starting colors cover repeated-action rejection with a stable count/next action,
no POOR/timing marker, white beams, Reset and successful OK completion.
It also verifies displaying an automation diagnostic in the shared error HUD and hiding it when cleared.
The same executable tests the production gameplay progress view: last long-tail
duration, pre/post-roll clamping, fill UV and marker position, current/total time,
the shared RhythmTimer's pause/resume, Reset, four viewport sizes, compiled
BPM/delay timing, empty/zero/negative duration and constant node/image reuse.
The all-notes actual D3D12/FMOD replay additionally checks that progress advances
during real playback and reaches 100% by the final note/tail.
The judgement/progress geometry executable uses MSVC `/analyze`; it starts no GPU/audio device.
Automated geometry checks do not replace actual screen/input/listening inspection.

## Gameplay pause menu and transport

After a solution build, run in an x64 developer PowerShell with `FMOD_ROOT` set:

```powershell
./Tests/Presentation/RunGameplayPauseTests.ps1 -Configuration Debug
./Tests/Presentation/RunGameplayPauseTests.ps1 -Configuration Release
```

The test links production pause/menu/controller/audio objects and uses MSVC `/analyze`.
It verifies the redesigned row dimensions/gaps, the PAUSED title and preparing band, keyboard focus, bounded adjustments, pointer Resume/Retry/Retire,
selector clicks that only change values, language changes,
portrait/wide resizing, preparation gating and owned Canvas cleanup. Hidden D3D12
execution checks clock freezing, Resume preserving grades, four-measure Rewind with
meter/BPM changes, score restoration, Retry eligibility, Retire MISS counts and control logs.
A clip backend probe verifies source seek positions and DSP scheduling before chart zero;
a 40ms async-seek probe rejects early unpause and verifies frozen time, one-shot commands,
QPC/DSP lead alignment, no hit-sound replay, failure handling and Retry resource reuse.
The controller probe also checks Debug-only Space freeze, note inputs in the same
Raw Input batch and during frozen time, held 1/2 seeking at 1 chart second per real
second at 1x/1.5x Accelerate, one-shot 3/4 millisecond steps, music resume alignment
and the ESC menu boundary. Release checks that Space and 1–4 do not freeze or seek.
Checks that playback keeps running compare chart-time progress with the real time measured
around the two clock captures, so a busy machine does not fail them; a 250ms seek still leaves
that range. Do not run this suite while a build or another device test is using the machine:
the hidden cold-start and pause-transport runs keep fixed readiness timeouts.
Backward seeking also checks that past MISS history is restored without repeating
MISS feedback. GameplayJudgementTests checks the signed Debug timer in us/ms,
frozen time and 1ms changes, and the absence of that label in Release.
The native controller also runs with no key events until a late-Bad note is
automatically missed in favour of the next Good note, in both configurations.
A test-only second mode uses the same GameplaySessionController lifecycle and polymorphic result;
Ending transfers its result exactly once. A separate real FMOD NoSound MP3 test repeats
source-zero/positive/near-end seeks and checks the actual source end.
The restart key is the backquote key held for half a second: the native controller must ignore a
tap, the former R key and a key held on through a restart, and restart on a fresh half-second hold.
PlayPauseTests also covers rewind inside Buzz, repeated cancellation, preserved input traces,
and forced zero-score MISS for all seven long-note types. These are automated intents and
rendering/audio checks, not physical ESC/arrow-key testing, audible listening or manual screen review.

## Allocation failure boundary regression

After a solution build, run in an x64 Visual Studio developer PowerShell:

```powershell
./Tests/Presentation/RunFailureBoundaryTests.ps1 -Configuration Debug
./Tests/Presentation/RunFailureBoundaryTests.ps1 -Configuration Release
```

The standalone test replaces C++ allocation only inside its own single-threaded
process. It checks that the numeric parser returns false when its temporary
string cannot be allocated and that asset initialization returns false with
empty error text when storing its error also fails. It deliberately omits the
RCDATA pack, so initialization fails before installing or reading the user's cache.
The test source is compiled with MSVC `/analyze`; this is not a game startup route.

## External UTF-8 language files

After building the Client, run `RunLanguageFileTests.ps1 -Configuration Debug` and `Release`.
The runner checks that literal UI keys have authoritative English defaults, then compiles
with `/W4 /analyze` and exercises BOM/Unicode/escapes/invalid UTF-8, entry isolation,
English fallback without either external file, partial English/Korean packs, malformed
format arguments, valid argument reordering, immutable string-view ownership and replacement
on the next catalog construction. Fixtures stay in ignored build folders; user languages
and Option.ini are not edited. Existing screen regressions cover the injected catalog
through the actual menu/option/editor/result/gameplay views.

## Editor audio view regression

After building both configurations, run in an x64 Visual Studio developer PowerShell.
The script obtains the editor object list from the Client project and compiles the editor
test translation units with MSVC Code Analysis. Set `FMOD_ROOT` for a custom SDK install.

```powershell
./Tests/Presentation/RunEditorPresentationTests.ps1 -configuration Debug
./Tests/Presentation/RunEditorPresentationTests.ps1 -configuration Release
```
This checks the production view's draw packets for real song/hit spectra, negative
note/music time ranges, slider input actions, tab visibility, localization, resize,
and Canvas cleanup. `EditorModeTests.cpp` also checks Taiko tool placement, snapping,
deletion/cancellation and expected cues, the Compose panels' Delete/Add/Apply with delay,
Exclude and Audio FX buses, the Score layout and Preview, and a new difficulty's draft, Create
and refused Create, then injects a test-only alternative mode
and document adapter to verify different tools/layout/coordinates/metadata/sound
targets, Save dispatch and marker revision caching through the common editor.
The committed Ogg Vorbis tone fixture also runs through the real analysis worker
and audio tab. The test checks visible waveform/music FFT/hit FFT rectangles;
no installed Vorbis codec or FFmpeg is required. EditorAudioAnalysisIntegrationTests
also checks real WAV/Ogg/MP3 duration, frequency/level, opposite stereo phase,
Unicode/renamed paths, bounded cancellation, truncated-stream errors and bundled
hit sounds. The engine-free domain audio test injects PCM to check the analysis
algorithm; actual decoder coverage belongs to this Client integration runner.
Audio range tests drag through actual Canvas receivers (including a narrow resized
viewport), check ordered source times/localized ms labels and all three overlays,
and verify FFT tile reuse and unchanged chart revision. Rate tests cover the slider's
0.25/2.00 endpoints, deterministic DSP/cue timing, negative pre-roll, live changes,
pending versus already audible hits, pause/resume and invalid values. Actual FMOD
NoSound playback also changes rates across repeated MP3 pause/seek/resume cycles.
It also covers realtime lane boundaries, spaced/case-insensitive Buzz options,
Purple and timed/explicit sound agreement with play, invalid seek preservation,
effect/timing commands, failed edit rollback and drawing failure recovery.
Realtime geometry is compared with and without Whole/custom Separate curves across five zoom levels and five
times, including BPM changes and long-note bodies. Deletion and placement use the same uniform time grid.
`EditorMenuTests.cpp` checks the Save-only Canvas menu, Korean/English and dirty
labels, pointer mapping at four viewport sizes, one-shot mouse/keyboard Save,
Escape/outside/right-click dismissal, input consumption and observer cleanup.
The composed view checks menu/footer coexistence and menu resizing. These are
synthetic menu intents and draw geometry, not physical Raw Input or manual UI tests.
The object list comes from the Client project so removed/stale objects are not linked.
Immutable rectangle batches are expanded only in the recording renderer for visual assertions.
It also checks actual asynchronous MP3 opening/playback with the FMOD NoSound output,
accurate music duration/near-end seeking, editor DSP transport, pause/resume/negative pre-roll,
dense cue deadlines, alias/voice reuse, expired-cue skipping, the player's music/effects
volume on song and hit-sound voices and cleanup through EditorPlaybackTests.
Twelve actual MP3 pause/resume cycles cover reused stream initialization and target seeks separately,
including negative pre-roll and seeks made while paused. The timeline stays frozen until both preparations finish.
EditorModeTests checks time zoom at five scales, stable lane/head dimensions and zoomed snapping,
unchanged chart timing/revision and consecutive Z/X, last-tail/music-end destinations,
missing/stale duration rejection, retry and manual-seek/direction reset behavior.
Q cancels an unfinished placement and selects notes; inspector cycles stay within each of the six
tool groups, retain paired endpoint IDs and reject cross-group drafts without modifying the chart.
Selection tests cover real Taiko drag ranges, empty margins, note overwrite with/without
blanks, grouped Undo/Redo restoring overwritten originals, Ctrl/Shift timing/effect
row selection and metadata history. Realtime wheel navigation follows the beat grid
without changing chart revision. Domain tests additionally cover the 32-action cap,
saved-state dirty tracking, failed edits retaining redo and paired long-note restoration.
Additional checks cover preview failure retry suppression, selection recovery, owned voice cleanup,
and eight consecutive cancelled analysis jobs followed by successful real audio analysis.
Drawing assertions do not initialize a GPU;
this does not replace visual inspection, listening or mouse testing in the app.

## Options panel and Option.ini regression

Resolution changes use the window service immediately, with rollback when INI saving fails.
The suite checks primary/auxiliary key capture, default-key conflicts, reserved game controls,
ESC cancellation, integer global offsets, invalid drafts and persistent mode profiles.
It first checks the Client mode presentation registry against the registered play modes:
one entry per mode, binding captions that name layout bindings and existing text keys,
hit-sound files with unique sound ids, and one key-overlay face per binding.
The rhythm suite's OptionTimingTests checks resolved input/held mappings and us timing translation
across notes, ticks, BPM/delay, bars and YME, without changing saved chart data.
Engine SDK `--window-control` verifies actual hidden native resizing and renderer notifications.

After building both configurations, run in an x64 Visual Studio developer PowerShell
with `FMOD_ROOT` set to the installed SDK:

```powershell
./Tests/Presentation/RunOptionsPanelTests.ps1 -Configuration Debug
./Tests/Presentation/RunOptionsPanelTests.ps1 -Configuration Release
```

This links the production panel/settings objects. It checks UTF-8 settings round trips,
panel/field contrast, closing expanded popups on ESC, repeated ESC during animation,
immediate close before the first animation step, stale selection rejection and reopening,
invalid values, all three audio ComboBoxes, sparse driver indices, output-scoped lists,
failed switches and rollback after a read-only INI blocks saving. Test files are isolated
in a unique temporary directory; the user's Option.ini and skins are not changed.
Songs settings cover UTF-8 paths, persistence, the default root and EXE-relative resolution.
The native folder-selection dialog still requires manual input validation.
The ASIO transition/failure cases use a test backend. A separate actual FMOD check
switches Automatic to WASAPI and selects the current driver while retaining a loaded
sample. It does not play sound or replace manual mouse/listening/real ASIO-device checks.

## All-notes actual gameplay replay

After a full solution build, run from the repository root in an x64 Visual Studio
developer PowerShell with `FMOD_ROOT` set to the installed SDK:

```powershell
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Debug
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Release
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Debug -Effects
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Release -Effects
```

This standalone test loads the deployed AngelDream `All Notes Verification` YMP
and its actual MP3. It runs the engine for the entire chart using a QPC rhythm
timer, DSP-scheduled music, the production GameplayPresenter and audio router,
D3D12 rendering and a real audio backend (no NoSound fallback). Normal notes
receive perfect scripted inputs; dense Don/Kat Buzz heads receive +50ms GOOD
inputs. All 19 logical notes, exact-once ordered ticks and final accuracy must
match the logic replay. It also verifies deferred note-node creation, removing expired
subtrees and recreating them after reset at the first note time. Skin aliases share samples as in production. This test
has no game autoplay route and does not change options or user songs/skins.

The window is hidden and input timestamps are supplied directly to PlaySession.
This verifies real rendering/audio execution, not pixel appearance, hearing,
physical keyboard Raw Input or mouse interaction. The test uses the production
asset-root resolver (Client/Assets during development, Asset beside deployed copies);
it fails when the all-notes chart or music is absent.

`-Effects` compiles this test with `TEST_YME_EFFECTS` and loads AngelDream
`YME Effects Verification` instead. It is a test-runner option, not a game CLI route.
The explicit YMP reference resolves its paired YME in the same difficulty directory, including its relative
sound table. The same real engine/presenter/router run plays all 15 logical notes
with Whole/Separate curves, hit-sound changes/volume and syncopation ranges.
The dense Buzz head is deliberately late: 2 of 37 body ticks must fail and the
remaining 35 succeed once. Final scripted accuracy is 97.8897%.
YmeEffectsTests separately checks every expanded judgement boundary, exclusions,
all interpolation types, position continuity, sound precedence and Kiai ON/OFF state.
This remains hidden-window automation; it does not verify audible volume differences
or the visual appearance of motion.

## Development and deployment asset paths

After a solution build, run from an x64 Visual Studio developer PowerShell:

```powershell
./Tests/Presentation/RunAssetPathTests.ps1 -Configuration Debug
./Tests/Presentation/RunAssetPathTests.ps1 -Configuration Release
```

This links the actual asset library and OptionSettings object. The same binary is
copied to isolated development/deployment directory trees and launched with CWD
pointing to the opposite tree. It checks external Songs/default-skin paths and
that Option.ini stays beside the EXE. A saved Songs override remains EXE-relative
even when CWD points to the other tree, and an empty override restores default discovery.
Fixtures stay under ignored build output;
no user settings, songs, skins or embedded cache are changed. The test source
uses MSVC Code Analysis. Catalog tests separately check the AngelDream YMP/YME pairs.

## Result graphs and actual Scene transition

After a solution build, run in an x64 Visual Studio developer PowerShell with `FMOD_ROOT`:

```powershell
./Tests/Presentation/RunResultPresentationTests.ps1 -Configuration Debug
./Tests/Presentation/RunResultPresentationTests.ps1 -Configuration Release
```

This links the production Result Scene/view/factory/graphs and gameplay objects.
It checks the judgement-share ring geometry (slice order, area shares, band radii and pointer lookup),
reads real embedded rank PNG metadata and checks all 11 ranks at 21 exact/below-threshold
six-decimal accuracy values, including 100% Y, 99.999999% X and the E/F boundary.
Rank images retain their aspect ratio inside the ring across resize and Canvas teardown.
It checks accuracy text truncated to two decimals (99.999999% reads 99.99%), Korean/English strings, signed scatter axes,
1ms histogram rounding/spacing, per-grade stacked colors matching scatter points,
peak totals, sparse outliers, minimum pixel widths without bin merging, empty samples,
narrow/wide layout, pointer-router return/retry actions,
disabled replay and catalog metadata snapshots,
and one immutable graph batch for 10,003 samples. It then runs D3D12/FMOD with a
hidden window and a one-note regression session: the production gameplay Scene
waits three seconds after automatic judgement, moves its result to the lazy Result
factory, and destroys its own Canvas. Clicking retry constructs a fresh gameplay
session after destroying Result and its Canvas; the return route then cleans gameplay.
The test source uses MSVC Code Analysis and does not write user options/songs/skins.
This is automation, not a manual pixel/physical keyboard/mouse/listening check.
ESC routing is connected in production; its interrupted scoring and tied-us input
order are covered by domain tests, without claiming a physical ESC test.

## Title music player

```powershell
./Tests/Presentation/RunTitlePlayerTests.ps1 -Configuration Debug
./Tests/Presentation/RunTitlePlayerTests.ps1 -Configuration Release
```

Links the production LogoView/TitlePlayerView and drives them with synthesized window input. Checks that
Space, Left and Right request play/pause, previous and next song while Enter alone runs the menu; that the
play/pause, stop and next buttons and seek-bar clicks report their intents; that dragging previews the
position and seeks only on release (also when dragged off the bar); that the bar does not seek before the
length is known; that an empty playlist blanks the captions, shows the empty art box and disables every
control and key; that the album art is square-cropped; and that the options panel keeps the player keys.
Audio transport (pause, seek, stop, end of song) is covered with real NoSound FMOD playback in
`RunEditorPresentationTests.ps1`. Listening and physical input remain manual checks.

## Skin manifest and scene images

```powershell
./Tests/Presentation/RunSkinTests.ps1 -Configuration Debug
./Tests/Presentation/RunSkinTests.ps1 -Configuration Release
```

Links production SkinManifest/selection/binding and Logo/MusicSelect views in an isolated
deployment directory, without writing user skins or Option.ini. Checks UTF-8/BOM/CRLF/Unicode paths,
scene-folder/image relativity, invalid paths/encoding/duplicates, per-component embedded fallback,
partial skins, cache/reload, button states, full-screen logo replacement, song-select sheet/button
replacement and restoration, virtualized song-card bindings using the real development Songs catalog, and that
options rail icons keep their skin image while their colour follows the lit section.
The test source uses MSVC Code Analysis. Native captures/manual input are reported separately.

## New difficulty metadata and file creation

After building the solution, run `RunNewDifficultyTests.ps1 -Configuration Debug` and `Release`.
The test links production creation/dialog objects with MSVC `/W4 /analyze`. It uses an isolated
ignored `build/new-difficulty-tests/<pid>` Songs fixture. It checks Unicode metadata, default
empty Taiko timing, sibling YME and Songs-relative YMM paths, no overwrites, invalid names/BPM,
JudgeLevel 1.00..100.00 in 0.01 steps, cancellation, correcting invalid fields in the same native
dialog, and rediscovery by SongCatalog. Timers drive the owned dialog automatically; this is
not physical mouse/keyboard validation. The real user Songs, Skins and Option.ini are untouched.
MusicSelectViewTests additionally checks Editor-only plus buttons and their creation intent.
EditorMetronomeTests checks cached grid generation, tempo/meter/offset/delay changes and
separation of playback clicks from the original hit-sound analysis markers.

## Canonical PCM timing check for an existing song

The editor regression picks the tracked built-in song from the development Songs
directory by default, keeping its analysis deadline independent of the user's
imported music library. An explicit `-SongFilter` selects another song from the
same directory instead.

After building both configurations, the editor and actual gameplay regression runners
accept `-SongFilter <substring of the catalog metadata path>`. This is a test-only
selection, not a game command-line option. Use a chart whose first note was authored
at the first audible attack; this path checks that timing without saving the document.

```powershell
./Tests/Presentation/RunEditorPresentationTests.ps1 -configuration Debug -SongFilter Omoiyoshino
./Tests/Presentation/RunAllNotesGameplayTests.ps1 -Configuration Debug -FmodRoot <SDK> -SongFilter Omoiyoshino
```

The editor checks that the first note belongs to the waveform attack's 10 ms bin and
runs its normal spectrum geometry/transport tests. Gameplay compares the source PCM
attack and compiled first-note time within 1 ms, then renders/judges/plays the opening
four chart seconds through D3D12 and a real audio output. This mode does not replace
the default full-note replay, screenshot review, listening, or physical input calibration.

Pass `-FullResult -SongFilter Omoiyoshino` to run that song to its end in real time with
timestamped test inputs, a visible D3D12 window and real FMOD output. The production
result view then displays the actual collected result for 60 seconds. This mode does
not write a personal play record or add autoplay to the game executable.

## Client-owned osu! import validation and placement

Run `RunOsuImportValidationTests.ps1 -Configuration Debug` and Release from an x64
VS developer shell. The test links the Client-owned OsuImportValidation and
OsuSongPlacement objects and shared domain libraries. It checks JSON/file references
and actual session note counts in the converter's output folder, then the placement
into Songs: names, the three rewritten reference lines, reuse of byte-identical audio
and backgrounds, conflicts that must preserve existing files, the import lock and the
rollback of a publication that fails half way. For an actual converter output, run
`OsuImportValidationTests.exe <result.json> <output folder>`; playability verification
and placement belong to the Client rather than the converter project.
`RunMusicSelectViewTests.ps1` runs the real converter process on an osu! song folder
(scan, choice, placement) and drives the difficulty list.

## Gameplay audio retirement

`RunGameplayAudioRetirementTests.ps1 -Configuration Debug` (and Release) links the
production router object with the public SDK. FMOD uses NoSound; injected test
clips/voices exercise successful, failed and throwing Stop calls. After the last
router alias and voice release a Loading clip, it must remain retired until Ready
and then release exactly once. A bus probe backend checks that the player's music
volume scales the Music bus, effects volume every other bus, YME volume automation
multiplies on top and survives a volume change, and reset or re-created buses keep
the player's volume. No production audio backend is added.

## New difficulty dialog

`RunNewDifficultyTests.ps1` locates its own dialog through current-thread window
enumeration and checks correction of invalid metadata while the dialog is hidden
and inactive. It does not depend on the desktop active window. Timers are matched
to their probe, and a missing/timed-out dialog terminates the test with failure.

## Gameplay timing probe

`RunGameplayTimingProbe.ps1` is a measurement, not a pass/fail test, and it is not part of
`VerifyAll`. It plays one chart through the real `RhythmTestScene`, FMOD output and a visible
1600×900 window, so run it alone: concurrent builds or engine checks change what it measures.
The game never sets the probe; the harness passes `GameplayLaunchRequest::timingProbe`
(`Client/GameFlow/GameplayTimingProbe.h`), which only observes the clock anchors, every running
gameplay clock read and the immediate cues around `PlayNow`.

```powershell
./Tests/Presentation/RunGameplayTimingProbe.ps1 -Configuration Release --pattern "all notes test" --replay --label buzz
./Tests/Presentation/RunGameplayTimingProbe.ps1 -Configuration Release --songs-root <Songs> --pattern "<chart path part>" --pause-every 4 --pause-count 30 --max-seconds 150 --label pause
```

Other options: `--accelerate <0.5..2>`, `--keep-pitch 0|1`, `--minimize-at <s> --minimize-for <s>`,
`--max-seconds <s>`, `--out <folder>` (default `build/timing-probe`). `--replay` presses every note on
time and holds each Buzz from its head to its end.

The summary (`<out>/<label>/summary.txt`, with CSV files beside it) reports:

- Clock offset d: chart time from QPC minus chart time of the raw mixer clock
  (`AudioSystem::DspClock()`) read with it, against the controller's anchor snapshot, in wall
  milliseconds, using the first read of each mixer block. The anchor's DSP value is the engine's
  smoothed estimate, so the raw reads observe the mixer independently of it; the summary also
  counts anchors taken before a smoothed estimate was available. It is given per
  anchor (start, Resume, Rewind, Retry), with the start/middle/end medians and slope for
  segments of 20 s or longer.
- Per Buzz and per window: tick-sound request lateness against the tick's chart time, audio
  routing time, Update rate and duration, main-loop iteration time, FPS, Render duration and
  the longest gap between rendered frames.
- Main-loop stalls of 5 ms or more, split into Update, the engine phase before Render
  (FMOD service, BeginFrame wait), Render, and the phase after it (Present, messages).
