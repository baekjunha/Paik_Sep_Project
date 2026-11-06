# VPSync Architecture Refactor Design

## Intent

Refactor the current VPSync Blueprint prototype into a maintainable structure without changing its visible behavior. The work will split playback, camera/stage control, and review drawing into focused Actor Components while keeping `BP_VPSessionController` as the stable public facade used by the operator UI, level, and Remote Control preset.

## Why

The current implementation works, but its two central assets have become difficult to reason about:

- `BP_VPSessionController` contains 30 declared variables, 37 graphs, and 457 nodes. It currently owns shot selection, playback, review drawing, lens and camera control, lighting, VCam activation, and operator UI creation.
- `WBP_VP_Operator` contains 19 graphs and 273 nodes, including a 139-node Event Graph and review rendering logic.
- External Review Pad traffic depends on the public function `ReceiveReviewDrawing` exposed through `RC_VPSyncOperator`; breaking this name or signature would break the iPad workflow.

The refactor therefore needs clear responsibility boundaries and an incremental migration path, not a rewrite.

## Goals

- Give each runtime unit one recognizable responsibility.
- Preserve all current operator behavior, Remote Control routes, shot playback, VCam controls, and review drawing behavior.
- Keep one authoritative owner for session state and remove duplicated UI state.
- Make the main controller readable as an orchestration layer rather than a collection of implementation graphs.
- Reduce direct coupling between the operator widget and low-level playback, camera, and review implementation.
- Make every migration step independently testable and reversible through source control.

## Non-goals

- Redesigning the operator UI appearance.
- Adding new VPSync features.
- Replacing Sequencer, Remote Control, or the Review Pad transport.
- Converting all Blueprint logic to C++ in this refactor.
- Refactoring unrelated animation, Foley, SoulCity, or marketplace Blueprints reachable from `LV_SC01`.
- Changing the public Review Pad payload format (`DrawingData : FString`).

## Anti-goals

- A big-bang replacement of `BP_VPSessionController`.
- Multiple assets owning their own conflicting copies of playback or session state.
- Allowing the widget to search the world for manager Actors or to call component implementation directly.
- Duplicating production Blueprint assets as permanent `Backup`, `New`, or `V2` variants.
- Moving graphs without first recording and checking the current behavior.

## Constraints

- Unreal Engine version remains 5.7.
- `BP_VPSessionController` remains the level-facing Actor and the public facade.
- `RC_VPSyncOperator` continues to expose `Receive Review Drawing` with input `DrawingData : FString` on `BP_VPSessionController`.
- The endpoint `/vpsync/review-drawing` and ports `30000`/`30010` remain unchanged.
- Existing assets under `/Game/Junha_Made` remain at their current paths unless a migration task explicitly creates a new focused asset.
- Blueprint changes must compile without warnings introduced by the refactor.
- The current user modification to `WBP_VP_Operator.uasset` must be preserved.

## Approaches Considered

### 1. Blueprint Actor Components with a stable controller facade — selected

Create focused Actor Components owned by `BP_VPSessionController`. Existing public functions remain on the controller and delegate to the appropriate component. This supports gradual migration, avoids breaking the level and Remote Control preset, and matches the team's current Blueprint workflow.

### 2. Move the core directly to C++

This would provide stronger typing and easier automated testing, but it would create a much larger migration and make iteration less accessible in the current Blueprint-centered project. C++ remains an option later for isolated parsing or transport code after behavior is stable.

### 3. Separate manager Actors placed in the level

Manager Actors make responsibilities visually obvious in the Outliner, but introduce lifecycle, lookup, and reference-order problems. Actor Components avoid those problems because they share the existing controller's lifetime.

## Target Architecture

```text
LV_SC01 / RC_VPSyncOperator / WBP_VP_Operator
                       │
                       ▼
             BP_VPSessionController
             public facade + session state
               │        │        │
               ▼        ▼        ▼
      BP_VPPlayback  BP_VPCamera  BP_VPReview
        Component      Component    Component
               │
               ▼
      Sequencer / shot sections

WBP_VP_Operator
  └─ WBP_VPReviewCanvas (drawing only)
```

Only `BP_VPSessionController` is a public runtime API. The level, Remote Control preset, and operator widget do not hold direct component references.

## Component Responsibilities

### `BP_VPPlaybackComponent`

Owns shot and Sequencer mechanics:

- Master Sequence and shot-section discovery.
- Current and selected shot references.
- Sequence player creation and active player references.
- Play, pause/resume, reset, normalized seek, and shot-finished handling.
- Playback-specific locks and internal playback flags.
- Track muting where the target belongs to the active sequence.

It emits focused dispatchers such as playback started, playback paused, playback position changed, selected shot changed, and playback finished. It does not create widgets or operate camera Actors directly.

### `BP_VPCameraControlComponent`

Owns virtual-production camera and stage controls:

- VCam Actor reference and activation.
- Active camera/lens targeting.
- Focal length, aperture, and focus mute/override behavior.
- Directional-light reference and mute behavior.

Camera state is not duplicated in the operator widget. The component exposes query functions needed by the controller facade and emits change events for UI refresh.

### `BP_VPReviewComponent`

Owns review drawing data and parsing:

- Receives the `DrawingData` string from the controller facade.
- Parses strokes and normalized points into the existing review structures.
- Owns `CurrentReviewDrawingData`, current stroke points, and review strokes.
- Clears review data and emits a review-changed dispatcher.

It does not know about HTTP, the Remote Control preset, or widget drawing geometry.

### `BP_VPSessionController`

Becomes the coordinator and compatibility facade:

- Owns the three components.
- Owns the single canonical `E_VPSessionState` value.
- Validates allowed state transitions in one named function.
- Preserves existing externally called function names and signatures, delegating their implementation to components.
- Relays component events as a small UI-facing event surface.
- Creates and owns `WBP_VP_Operator` if that remains part of current startup behavior.

Compatibility wrapper functions may temporarily remain after migration. They should be clearly categorized as `VPSync|Facade` and contain delegation only.

### `WBP_VP_Operator`

Becomes a presentation and input adapter:

- Holds one typed controller reference supplied during initialization.
- Sends user intent to controller facade functions.
- Updates controls from controller events and explicit refresh functions.
- Does not use `GetActorOfClass` after initialization.
- Does not store its own authoritative session state.
- Moves review drawing from the main widget's `OnPaint` implementation into `WBP_VPReviewCanvas`.

### `WBP_VPReviewCanvas`

Owns only review-line rendering. It receives immutable/copy input containing parsed strokes, invalidates itself when that input changes, and renders the lines in `OnPaint`. It does not parse strings or call the session controller.

## State Ownership and Communication

- `BP_VPSessionController.CurrentSessionState` is the only authoritative session state.
- Playback mechanics live in the playback component; the component reports events and the controller performs session-state transitions.
- Each data item has one owner. Widgets cache display values only when required by UMG and never become the source of truth.
- Controller-to-component calls are direct typed calls because the components share the same Actor and lifetime.
- Component-to-controller notifications use Event Dispatchers.
- UI-to-runtime calls go only through the controller facade.
- Runtime-to-UI updates use controller Event Dispatchers, followed by focused widget refresh functions.

## Public Compatibility Surface

The first implementation plan must inventory every external call before moving logic. At minimum, the following currently observed controller functions remain callable with their current signatures until all callers and `RC_VPSyncOperator` are verified:

- `ReceiveReviewDrawing`
- `ClearReviewDrawing`
- `SetPlaybackNormalized`
- `SelectShot`
- `SelectMasterShot`
- `PlaySelectedMasterShot`
- `PlayShots`
- `PauseAndResume`
- `ResetSession`
- `GetMasterShotSections`
- `GetSessionStateText`
- `ToggleOperatorUIVisibility`
- `ToggleVCamActive`
- Camera, lens, focus, aperture, light, and track-mute facade functions currently called by UI or Remote Control.

If analysis finds additional external callers, their functions join this compatibility surface before migration begins.

## Migration Strategy

The migration uses a strangler pattern: add a component, move one responsibility behind the existing controller API, verify behavior, and only then remove the old implementation.

### Stage 0: Establish the baseline

- Save the current modified `WBP_VP_Operator` and create a source-control recovery point.
- Compile all VPSync Blueprints and record existing warnings.
- Capture an updated Blueprint analyzer report.
- Record a manual acceptance checklist covering startup, shot selection, play/pause/reset, seek, shot completion, VCam, lens controls, light/track mute, Review Pad receive, drawing display, and clear.

### Stage 1: Establish contracts and state ownership

- Inventory public controller functions and all callers.
- Add the canonical `CurrentSessionState` and one transition function to the controller.
- Remove the widget's authority over `VPSessionState`; it becomes display-only during migration and is then removed.
- Introduce controller dispatchers for state, shot, playback position, camera settings, and review changes.

### Stage 2: Extract playback

- Create `BP_VPPlaybackComponent` and attach it to the controller.
- Move shot discovery and selection first, then playback and seeking, then completion and reset.
- Keep controller wrapper functions intact and delegate them to the component.
- Verify the full playback checklist before deleting each corresponding old graph.

### Stage 3: Extract camera and stage controls

- Create `BP_VPCameraControlComponent` and attach it to the controller.
- Move VCam activation and lens/focus/aperture logic.
- Move directional-light mute and other stage-control functions currently coupled to camera operation.
- Retain controller facade wrappers and verify Remote Control and UI callers.

### Stage 4: Extract review handling

- Create `BP_VPReviewComponent` and attach it to the controller.
- Move string parsing and stroke storage without changing the payload format.
- Keep `BP_VPSessionController.ReceiveReviewDrawing(DrawingData)` as the Remote Control entry point and delegate immediately to the component.
- Add `WBP_VPReviewCanvas` and move line rendering out of `WBP_VP_Operator`.
- Verify the complete iPad `:30000 → :30010 → RC preset → controller → component → widget` path.

### Stage 5: Simplify the operator widget

- Split bound button events into short intent handlers.
- Replace repeated visual updates with focused refresh functions driven by controller events.
- Remove repeated world searches and duplicate state.
- Rename generic or inconsistent identifiers, including numbered buttons, `Test`, and inconsistent Boolean casing, after references are verified.

### Stage 6: Remove migration scaffolding

- Delete dead graphs, obsolete variables, duplicate event bindings, and wrappers proven to have no external callers.
- Keep wrappers that form the intentional stable facade.
- Regenerate the analyzer report and compare asset size, graph size, dependencies, and unresolved calls against the baseline.
- Update the Review Pad recovery guide and add a short architecture document describing ownership and data flow.

## Error Handling

- Component functions that require a missing sequence, player, camera, or widget return a clear failure result or log a categorized warning rather than silently doing nothing.
- `ReceiveReviewDrawing` rejects or safely ignores malformed stroke data without replacing the last valid drawing.
- Repeated UI initialization must not create duplicate event bindings.
- Playback completion and manual reset must be idempotent: repeated calls leave the system in the same valid idle state.
- Missing optional VCam or light references must not break playback or review features.

## Verification

Each extraction stage must pass all relevant checks before the old graph is deleted:

- Every changed Blueprint compiles successfully.
- Editor startup in `LV_SC01` produces no new Blueprint runtime errors.
- Existing public controller calls still work from `WBP_VP_Operator`, the level, and `RC_VPSyncOperator`.
- Shot selection, play, pause/resume, seek, finish, and reset match the recorded baseline.
- VCam and camera/stage controls match the recorded baseline.
- Review Pad drawing can be sent, rendered, and cleared.
- Reopening the project and rerunning the checklist gives the same result.
- The Blueprint analyzer completes with zero load failures.

## Completion Criteria

- `BP_VPSessionController` primarily contains component ownership, state transitions, event relay, startup wiring, and facade delegation.
- Playback, camera/stage, and review implementation live in their designated components.
- `WBP_VP_Operator` contains no drawing parser and no large review `OnPaint` graph.
- The operator widget does not own authoritative session state or repeatedly search for the controller.
- The Review Pad endpoint, preset, function name, and payload remain compatible.
- No permanent duplicate controller or widget assets are introduced.
- Baseline behavior passes after an editor restart.
- Architecture and recovery documentation reflect the new structure.

## Builder Decisions

The implementer may choose graph layout, comment colors, local variable names, and exact event dispatcher names where this document does not prescribe them. Any newly discovered external caller or Remote Control exposure must be treated as part of the compatibility surface and documented before its implementation is moved.
