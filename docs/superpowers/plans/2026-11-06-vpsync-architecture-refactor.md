# VPSync Architecture Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split VPSync playback, camera/stage control, and review drawing into focused Blueprint Actor Components while preserving the controller, UI, level, and Remote Control behavior.

**Architecture:** Keep `BP_VPSessionController` as the only public runtime facade and the owner of canonical session state. Add three Actor Components for playback, camera/stage, and review responsibilities; move review rendering into a child widget; migrate one behavior group at a time behind unchanged controller functions.

**Tech Stack:** Unreal Engine 5.7, Blueprint Actor Components, UMG, Sequencer, Remote Control, PowerShell architecture checks, existing BlueprintFlowAnalyzer

**Spec:** `docs/superpowers/specs/2026-11-06-vpsync-architecture-refactor-design.md`

## Global Constraints

- Preserve the current user modification to `Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset` before isolation or migration work begins.
- Keep `/Game/Junha_Made/Blueprints/Controller/BP_VPSessionController` as the level-facing Actor and public facade.
- Keep `RC_VPSyncOperator` exposing `Receive Review Drawing` with `DrawingData : FString` on the controller.
- Do not change `/vpsync/review-drawing`, port `30000`, port `30010`, or the serialized stroke format.
- Do not edit unrelated animation, Foley, SoulCity, marketplace, or sample assets.
- Migrate one responsibility at a time; do not delete its old graph until the replacement passes the relevant acceptance checks.
- Compile every changed Blueprint and introduce no new Blueprint warnings.
- Never create permanent `Backup`, `New`, or `V2` copies of production Blueprint assets.

## Review Focus

- Remote Control must still find `ReceiveReviewDrawing(FString DrawingData)` on the controller after review extraction; Task 5 tests the reflection contract and the live Review Pad route.
- Missing optional VCam or Directional Light references must not prevent playback or review use; Task 4 tests both references unset.
- Reinitializing the operator widget must not bind dispatchers twice; Task 6 performs a remove/re-add cycle and checks that each update fires once.
- Empty or malformed drawing input must not replace the last valid review drawing; Task 5 tests empty, malformed, and valid payloads.
- Playback finish and reset must be idempotent; Task 3 invokes each twice and checks the final state, active player, and UI state.

---

### Task 1: Baseline, recovery point, and executable architecture checks

**Files:**
- Create: `docs/vpsync/VPSync-public-contract.md`
- Create: `docs/vpsync/VPSync-acceptance-checklist.md`
- Create: `Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1`
- Verify: `Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset`
- Verify: `Saved/BlueprintFlowAnalyzer/analysis.json`
- Verify: `Saved/BlueprintFlowAnalyzer/report.json`

**Interfaces:**
- Consumes: the current saved controller, UI, level, Remote Control preset, and fresh analyzer JSON.
- Produces: a recovery commit, a public compatibility inventory, a repeatable manual checklist, and `Test-VPSyncArchitecture.ps1 -Phase Baseline|Target`.

- [ ] **Step 1: Preserve the current operator-widget work**

Confirm the Unreal Editor has saved `WBP_VP_Operator`, inspect `git status --short`, and create a checkpoint commit containing only the user's current `WBP_VP_Operator.uasset` change. Stop rather than stashing, reverting, or overwriting it if its intended saved state cannot be confirmed.

- [ ] **Step 2: Regenerate the Blueprint analysis baseline**

Run:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\Paik_Sep_Project.uproject" -unattended -nop4 -nosplash -BPAnalyzerRun -DDC-ForceMemoryCache -log
```

Expected: exit code `0`; `report.json` has `status = completed` and `blueprints_failed = 0`.

- [ ] **Step 3: Write the public compatibility inventory**

Document every caller and exact pin signature for the controller functions observed in the analyzer, including `ReceiveReviewDrawing`, `ClearReviewDrawing`, `SetPlaybackNormalized`, `Slider_PlayTime`, `SelectShot`, `SelectMasterShot`, `PlaySelectedMasterShot`, `PlayShots`, `PauseAndResume`, `ResetSession`, `GetMasterShotSections`, `GetSessionStateText`, `ToggleOperatorUIVisibility`, `ToggleVCamActive`, and all camera/lens/light/track-mute functions exposed to UI or Remote Control.

- [ ] **Step 4: Write the manual acceptance checklist**

Give each check a stable ID and expected result: `START-01`, `SHOT-01`, `PLAY-01`, `PAUSE-01`, `SEEK-01`, `FINISH-01`, `RESET-01`, `VCAM-01`, `LENS-01`, `LIGHT-01`, `TRACK-01`, `REVIEW-VALID-01`, `REVIEW-CLEAR-01`, `REVIEW-BAD-01`, and `UI-REBIND-01`.

- [ ] **Step 5: Write the baseline architecture test**

Implement `Test-VPSyncArchitecture.ps1` with parameters `-AnalysisPath` and `-Phase`. `Baseline` must assert that the controller and widget load, the compatibility functions and signatures exist, and analyzer failures equal zero. `Target` must additionally assert that the three component assets and review-canvas widget exist, the controller owns the components, the main widget no longer calls `GetActorOfClass`, and review drawing is absent from its `OnPaint` graph.

- [ ] **Step 6: Run the baseline and target checks**

Run:

```powershell
pwsh -File Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1 -AnalysisPath Saved/BlueprintFlowAnalyzer/analysis.json -Phase Baseline
pwsh -File Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1 -AnalysisPath Saved/BlueprintFlowAnalyzer/analysis.json -Phase Target
```

Expected: `Baseline` passes; `Target` fails because the focused assets do not exist yet.

- [ ] **Step 7: Execute and record the complete baseline checklist**

Run every checklist item once in `LV_SC01` and record observed results before changing behavior. Any currently failing item becomes a recorded pre-existing limitation, not a refactor regression.

- [ ] **Step 8: Commit the baseline artifacts**

```powershell
git add docs/vpsync/VPSync-public-contract.md docs/vpsync/VPSync-acceptance-checklist.md Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1
git commit -m "test: capture VPSync refactor baseline"
```

### Task 2: Canonical session state and stable controller facade

**Files:**
- Modify: `Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset`
- Modify: `Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset`
- Modify: `Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1`
- Modify: `docs/vpsync/VPSync-public-contract.md`

**Interfaces:**
- Consumes: existing `E_VPSessionState` values (`READY`, `PLAYING Scene`, `PAUSED`, `ERROR`) and the Task 1 public contract.
- Produces: controller variable `CurrentSessionState`, function `TrySetSessionState(NewState, bChanged)`, dispatcher `OnSessionStateChanged(NewState)`, and a UI that treats controller state as authoritative.

- [ ] **Step 1: Add a failing state-ownership assertion**

Extend the PowerShell check to require `CurrentSessionState`, `TrySetSessionState`, and `OnSessionStateChanged` on the controller and to reject an authoritative `VPSessionState` variable on `WBP_VP_Operator` in `Target` phase.

- [ ] **Step 2: Run the target check to verify failure**

Expected: failure names the missing controller state API and the duplicate widget state.

- [ ] **Step 3: Add the canonical controller state API**

Create `CurrentSessionState : E_VPSessionState`, `OnSessionStateChanged(E_VPSessionState NewState)`, and `TrySetSessionState(E_VPSessionState NewState, bool bChanged)`. Allow `READY → PLAYING/ERROR`, `PLAYING → PAUSED/READY/ERROR`, `PAUSED → PLAYING/READY/ERROR`, `ERROR → READY`; setting the current value is a successful no-op with `bChanged = false`.

- [ ] **Step 4: Route existing state changes through `TrySetSessionState`**

Replace writes represented by `bIsPlaying` and `bISPaused` with canonical state transitions while temporarily keeping compatibility reads where removal would alter behavior. Bind existing session-state UI refresh to `OnSessionStateChanged`.

- [ ] **Step 5: Remove the widget as a state authority**

Change `WBP_VP_Operator` to read state from its supplied controller reference and remove its `VPSessionState` variable after all reads are replaced.

- [ ] **Step 6: Verify state behavior**

Compile both Blueprints. Run `START-01`, `PLAY-01`, `PAUSE-01`, `FINISH-01`, and `RESET-01`; verify repeated assignment of the same state emits no duplicate state-change event.

- [ ] **Step 7: Regenerate analysis and run the baseline assertions**

Expected: analyzer completes with zero failures and every public facade signature remains unchanged.

- [ ] **Step 8: Commit the state boundary**

```powershell
git add Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1 docs/vpsync/VPSync-public-contract.md
git commit -m "refactor: centralize VPSync session state"
```

### Task 3: Extract shot selection and playback

**Files:**
- Create: `Content/Junha_Made/Blueprints/Components/BP_VPPlaybackComponent.uasset`
- Modify: `Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset`
- Modify: `Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1`

**Interfaces:**
- Consumes: `E_VPShot`, `ST_VPShotData`, `Master_SEQ`, shot sequences, existing controller facade signatures, and controller state transitions from Task 2.
- Produces: `BP_VPPlaybackComponent` with selection/playback state and dispatchers; controller facade functions delegate to it.

- [ ] **Step 1: Add failing playback-component assertions**

Require the component asset, a controller SCS component named `PlaybackComponent`, and component functions `InitializePlayback`, `SelectShot`, `SelectMasterShot`, `GetMasterShotSections`, `PlayShots`, `PlaySelectedMasterShot`, `PauseAndResume`, `SetPlaybackNormalized`, `ResetPlayback`, and `GetCurrentShotSequence`.

- [ ] **Step 2: Run the target check to verify failure**

Expected: failure identifies the absent playback component and functions.

- [ ] **Step 3: Create `BP_VPPlaybackComponent` and move shot discovery**

Move `MasterSequence`, shot list, current/loaded shot, selected master section, and player references into the component. Implement `InitializePlayback(ULevelSequence MasterSequence)`, `SelectShot(E_VPShot TargetShot)`, `SelectMasterShot(UMovieSceneCinematicShotSection TargetSection)`, `GetMasterShotSections(out Array<UMovieSceneCinematicShotSection>)`, and `GetCurrentShotSequence(out ULevelSequence)`.

- [ ] **Step 4: Verify selection before moving playback**

Compile both assets and run `SHOT-01`. Confirm all existing shot buttons select the same sections and the controller facade outputs remain identical.

- [ ] **Step 5: Move playback, seek, completion, and reset**

Implement component functions `PlayShots`, `PlaySelectedMasterShot`, `PauseAndResume`, `SetPlaybackNormalized(double SeekValue)`, and `ResetPlayback`. Add dispatchers `OnSelectedShotChanged`, `OnPlaybackStarted`, `OnPlaybackPaused(bool bPaused)`, `OnPlaybackPositionChanged(double NormalizedValue)`, and `OnPlaybackFinished`.

- [ ] **Step 6: Convert controller playback functions to delegation-only wrappers**

Keep every Task 1 facade name and pin type. Each wrapper validates `PlaybackComponent`, calls one component function, and coordinates `TrySetSessionState`; it contains no Sequencer graph implementation.

- [ ] **Step 7: Test playback failure modes and idempotency**

Run `PLAY-01`, `PAUSE-01`, `SEEK-01`, `FINISH-01`, and `RESET-01`. Trigger finish twice and reset twice; expect `READY`, no active player, unlocked shot selection, and one final UI refresh. Run once with no selected shot; expect a categorized warning and no invalid state transition.

- [ ] **Step 8: Delete migrated playback graphs and variables**

Remove controller-owned variables and implementation graphs only after Step 7 passes. Retain compatibility wrappers and controller-only operator-widget ownership.

- [ ] **Step 9: Regenerate analysis, run checks, and commit**

Expected: baseline contract passes; playback assertions pass; analyzer load failures remain zero.

```powershell
git add Content/Junha_Made/Blueprints/Components/BP_VPPlaybackComponent.uasset Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1
git commit -m "refactor: extract VPSync playback component"
```

### Task 4: Extract camera, lens, VCam, and stage controls

**Files:**
- Create: `Content/Junha_Made/Blueprints/Components/BP_VPCameraControlComponent.uasset`
- Modify: `Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset`
- Modify: `Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1`

**Interfaces:**
- Consumes: `VCamActorRef : VCamActor`, `DirectionalLightRef : DirectionalLight`, active-sequence access through the playback component, and existing camera facade signatures.
- Produces: `BP_VPCameraControlComponent`; controller camera/stage facade functions delegate to it.

- [ ] **Step 1: Add failing camera-component assertions**

Require the component asset, a controller SCS component named `CameraControlComponent`, and functions `SetVCamActor`, `SetDirectionalLight`, `ToggleVCamActive`, `SetCameraLensMute`, `SetFocalMute`, `FocalMuteOn`, `FocalMuteOff`, `ApplyFocalOverride`, `SetApertureMute`, `SetFocusMute`, `SetDirectionalLightMute`, and `SetTrackMute`.

- [ ] **Step 2: Run the target check to verify failure**

Expected: failure identifies the missing camera-control boundary.

- [ ] **Step 3: Create the component and move target ownership**

Move `VCamActorRef`, `DirectionalLightRef`, and focal-override state into the component. Add setters so the controller initializes explicit references without world searches inside the component.

- [ ] **Step 4: Move VCam and lens behavior**

Move `ToggleVCamActive`, lens mute, focal mute/override, aperture mute, and focus mute graphs. Preserve facade inputs exactly and add `OnCameraControlChanged` for UI refresh.

- [ ] **Step 5: Move light and sequence-track mute behavior**

Move `SetDirectionalLightMute` and `SetTrackMute`. Obtain the active sequence/player through an explicit controller-supplied reference or typed playback-component query, never `GetActorOfClass`.

- [ ] **Step 6: Convert controller camera/stage functions to delegation-only wrappers**

Keep the public names and pin signatures from Task 1 and remove their implementation only after functional verification.

- [ ] **Step 7: Test optional-reference isolation**

Run `VCAM-01`, `LENS-01`, `LIGHT-01`, and `TRACK-01`. Then clear `VCamActorRef` and `DirectionalLightRef` independently and together; expect categorized warnings while `PLAY-01` and `REVIEW-VALID-01` remain usable.

- [ ] **Step 8: Regenerate analysis, run checks, and commit**

```powershell
git add Content/Junha_Made/Blueprints/Components/BP_VPCameraControlComponent.uasset Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1
git commit -m "refactor: extract VPSync camera controls"
```

### Task 5: Extract review parsing and drawing storage

**Files:**
- Create: `Content/Junha_Made/Blueprints/Components/BP_VPReviewComponent.uasset`
- Create: `Content/Junha_Made/Blueprints/UI/WBP_VPReviewCanvas.uasset`
- Modify: `Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset`
- Modify: `Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset`
- Verify: `Content/Junha_Made/Blueprints/RemoteControl/RC_VPSyncOperator.uasset`
- Modify: `Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1`

**Interfaces:**
- Consumes: `ST_ReviewStroke`, normalized `X,Y;X,Y|X,Y` payloads, controller `ReceiveReviewDrawing(FString DrawingData)`, and controller `ClearReviewDrawing()`.
- Produces: `BP_VPReviewComponent`, `WBP_VPReviewCanvas`, unchanged Remote Control entry point, and controller dispatcher `OnReviewChanged`.

- [ ] **Step 1: Add failing review-boundary assertions**

Require both new assets, a controller SCS component named `ReviewComponent`, component functions `TryParseReviewDrawing`, `ReceiveReviewDrawing`, `ClearReviewDrawing`, and `GetReviewStrokes`, and controller function `ReceiveReviewDrawing` with exactly one `FString` input.

- [ ] **Step 2: Run the target check to verify failure**

Expected: failure identifies the missing review component/canvas while confirming the old public entry point still exists.

- [ ] **Step 3: Create the review component and pure parser boundary**

Move `CurrentReviewDrawingData`, `CurrentStrokePoints`, and `ReviewStrokes` into the component. Implement `TryParseReviewDrawing(FString DrawingData, out Array<ST_ReviewStroke> ParsedStrokes, out bool bSuccess)` without mutating stored state.

- [ ] **Step 4: Implement commit-on-success review storage**

Implement `ReceiveReviewDrawing(FString DrawingData, out bool bAccepted)` so only successful, non-empty parse results replace stored state; implement idempotent `ClearReviewDrawing`; emit `OnReviewChanged` only when stored review data changes.

- [ ] **Step 5: Preserve the Remote Control facade**

Keep `BP_VPSessionController.ReceiveReviewDrawing(DrawingData)` and `ClearReviewDrawing()` at their current names and signatures. Delegate immediately to `ReviewComponent`, relay `OnReviewChanged`, and verify `RC_VPSyncOperator` still resolves `Receive Review Drawing` without re-exposing a component function.

- [ ] **Step 6: Create `WBP_VPReviewCanvas`**

Add `SetReviewStrokes(Array<ST_ReviewStroke> InStrokes)` and keep all line rendering in this widget's `OnPaint`. It must not parse strings, find the controller, or call runtime functions.

- [ ] **Step 7: Embed the review canvas in the operator widget**

Replace the main widget's drawing `OnPaint` logic with the child canvas. On controller `OnReviewChanged`, query the facade for a copy of the current strokes, pass them to the canvas, and invalidate only the canvas.

- [ ] **Step 8: Test valid, empty, and malformed payloads**

Send a valid multi-stroke payload and record the stored stroke/point counts. Then send `""`, `"bad"`, `"0.1,0.2;broken"`, and out-of-range values; expect rejection, a warning, no crash, and the prior valid drawing unchanged. Clear twice; expect empty data and one effective change.

- [ ] **Step 9: Test the complete live Review Pad path**

Run `REVIEW-VALID-01`, `REVIEW-BAD-01`, and `REVIEW-CLEAR-01` through `iPad/HTML → :30000 proxy → :30010 → RC_VPSyncOperator → controller → review component → canvas`.

- [ ] **Step 10: Regenerate analysis, run checks, and commit**

```powershell
git add Content/Junha_Made/Blueprints/Components/BP_VPReviewComponent.uasset Content/Junha_Made/Blueprints/UI/WBP_VPReviewCanvas.uasset Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1
git commit -m "refactor: extract VPSync review pipeline"
```

### Task 6: Simplify operator UI wiring and naming

**Files:**
- Modify: `Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset`
- Modify: `Content/Junha_Made/Blueprints/UI/ShotButton.uasset`
- Modify: `Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset`
- Modify: `Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1`
- Modify: `docs/vpsync/VPSync-public-contract.md`

**Interfaces:**
- Consumes: controller facade and controller-level dispatchers from Tasks 2–5.
- Produces: one-time widget initialization, short intent handlers, event-driven refresh functions, descriptive names, and no repeated controller search.

- [ ] **Step 1: Add failing UI-boundary assertions**

Require no `GetActorOfClass` call in `WBP_VP_Operator`, no review parsing/drawing call in its graphs, and no generic `Test` variable in the controller. Require named refresh functions `RefreshSessionState`, `RefreshShotSelection`, `RefreshPlaybackPosition`, `RefreshCameraControls`, and `RefreshReview`.

- [ ] **Step 2: Run the target check to verify failure**

Expected: failure lists remaining world search, oversized mixed handlers, or temporary names.

- [ ] **Step 3: Add explicit widget initialization**

Implement `InitializeOperator(BP_VPSessionController InController)` as the only controller-reference assignment and binding point. Guard against the same controller being supplied twice; unbind the previous controller before binding a different one.

- [ ] **Step 4: Replace mixed UI update paths with focused refresh functions**

Bind controller events once and route them to `RefreshSessionState`, `RefreshShotSelection`, `RefreshPlaybackPosition`, `RefreshCameraControls`, and `RefreshReview`. Keep bound button/slider events as short calls expressing user intent.

- [ ] **Step 5: Normalize names after reference search**

Rename numbered buttons and ambiguous handlers to their purpose, change inconsistent Boolean casing such as `bISPaused` where it still exists, and remove `Test` after verifying it has no callers. Preserve names that are part of the documented public facade.

- [ ] **Step 6: Test rebinding and event duplication**

Initialize the widget with the same controller twice, remove/re-add it to the viewport, and initialize again. Trigger one state, shot, playback, camera, and review change; expect exactly one UI refresh per event and no stale-controller callback.

- [ ] **Step 7: Run the complete UI and feature checklist**

Execute all Task 1 checklist IDs. Compare results to the baseline and record any intentional difference; unapproved differences are regressions.

- [ ] **Step 8: Regenerate analysis, run checks, and commit**

```powershell
git add Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset Content/Junha_Made/Blueprints/UI/ShotButton.uasset Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1 docs/vpsync/VPSync-public-contract.md
git commit -m "refactor: simplify VPSync operator UI"
```

### Task 7: Remove migration scaffolding and document the final architecture

**Files:**
- Modify: `Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset`
- Modify: `Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset`
- Modify: `Tools/VPSyncReviewPad/README.txt`
- Create: `docs/vpsync/VPSync-architecture.md`
- Modify: `docs/vpsync/VPSync-acceptance-checklist.md`
- Verify: `Saved/BlueprintFlowAnalyzer/analysis.json`
- Verify: `Saved/BlueprintFlowAnalyzer/report.json`
- Verify: `Saved/BlueprintFlowAnalyzer/lv_sc01_blueprint_audit.md`

**Interfaces:**
- Consumes: the migrated assets and all baseline/target assertions.
- Produces: production-ready assets with no dead migration logic, current documentation, and a recorded final verification result.

- [ ] **Step 1: Identify removable compatibility scaffolding**

Use Find References and `VPSync-public-contract.md` to classify every remaining controller wrapper as intentional public facade or unused migration helper. Delete only helpers with zero Blueprint, level, UI, and Remote Control callers.

- [ ] **Step 2: Remove dead graphs, variables, and duplicate bindings**

Delete disconnected nodes, obsolete state flags, old review arrays, old camera/player references, and old widget update paths only when their designated component owns the replacement.

- [ ] **Step 3: Compile all project Blueprints**

Run:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\Paik_Sep_Project.uproject" -run=CompileAllBlueprints -ProjectOnly -unattended -nop4 -NullRHI -log
```

Expected: exit code `0`, no new Blueprint compile errors, and no warnings introduced by the refactor.

- [ ] **Step 4: Run final static verification**

Regenerate the analyzer JSON, run `Test-VPSyncArchitecture.ps1 -Phase Baseline` and `-Phase Target`, then regenerate `lv_sc01_blueprint_audit.md`. Expected: both phases pass and `blueprints_failed = 0`.

- [ ] **Step 5: Run the full acceptance checklist after editor restart**

Restart Unreal Editor, open `LV_SC01`, and execute every checklist ID. Expected: all items match the baseline or an explicitly approved improvement.

- [ ] **Step 6: Write final architecture and recovery documentation**

Document ownership, component APIs, event flow, initialization order, Remote Control compatibility, Review Pad data flow, and the rule that UI talks only to the controller facade. Update the Review Pad guide so troubleshooting names the new review component and canvas without changing setup steps.

- [ ] **Step 7: Record before/after metrics**

Record controller and main-widget variable, graph, and node counts from the analyzer. The acceptance criterion is responsibility separation and removal of implementation graphs from the facade/UI, not a forced numeric node target.

- [ ] **Step 8: Commit the completed refactor**

```powershell
git add Content/Junha_Made/Blueprints/Controller/BP_VPSessionController.uasset Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset Tools/VPSyncReviewPad/README.txt docs/vpsync/VPSync-architecture.md docs/vpsync/VPSync-acceptance-checklist.md
git commit -m "docs: finalize VPSync component architecture"
```

## Final Verification Sequence

Run these in order after Task 7:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\Paik_Sep_Project.uproject" -run=CompileAllBlueprints -ProjectOnly -unattended -nop4 -NullRHI -log
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\Paik_Sep_Project.uproject" -unattended -nop4 -nosplash -BPAnalyzerRun -DDC-ForceMemoryCache -log
pwsh -File Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1 -AnalysisPath Saved/BlueprintFlowAnalyzer/analysis.json -Phase Baseline
pwsh -File Tools/BlueprintFlowAnalyzer/Test-VPSyncArchitecture.ps1 -AnalysisPath Saved/BlueprintFlowAnalyzer/analysis.json -Phase Target
pwsh -File Tools/BlueprintFlowAnalyzer/Generate-LVSC01Report.ps1
```

Expected final state: all commands exit `0`, analyzer load failures equal zero, all target architecture assertions pass, and the restarted-editor acceptance checklist matches the recorded baseline.
