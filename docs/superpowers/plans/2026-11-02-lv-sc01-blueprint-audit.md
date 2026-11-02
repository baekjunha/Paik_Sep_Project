# LV_SC01 Blueprint Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce an evidence-backed inventory of every Blueprint statically reachable from `LV_SC01` and no unrelated Blueprint.

**Architecture:** Extend the existing editor-only analyzer to compute an Asset Registry dependency closure rooted at the map, retain one predecessor edge per reachable package as inclusion evidence, and extract richer metadata only for reachable Blueprints. A report script transforms the machine-readable JSON into a detailed Markdown audit and performs invariant checks.

**Tech Stack:** Unreal Engine 5.7 C++, Asset Registry, Blueprint editor APIs, JSON, PowerShell report generation

**Spec:** `docs/superpowers/specs/2026-11-02-lv-sc01-blueprint-audit-design.md`

## Global Constraints

- Start package: `/Game/Junha_Made/Maps/Paik_Map/LV_SC01`.
- Follow hard and soft package references recursively.
- Do not modify project assets.
- Write generated artifacts under `Saved/BlueprintFlowAnalyzer`.

## Review Focus

- A missing root map must fail visibly rather than produce an empty successful report.
- Cyclic dependencies must terminate without duplicate assets.
- Every included Blueprint must have a reconstructable path from the root map.
- Non-Blueprint packages in the closure must not appear in the Blueprint inventory.
- Unresolved function targets and failed asset loads must be reported, not discarded.

---

### Task 1: Dependency closure and inclusion evidence

**Files:**
- Modify: `Plugins/BlueprintFlowAnalyzer/Source/BlueprintFlowAnalyzer/Private/BlueprintFlowAnalyzerModule.cpp`

**Interfaces:**
- Produces: JSON fields `entry_map`, `dependency_packages`, `blueprint_reference_paths`, and a Blueprint list restricted to the closure.

- [ ] Add a recursive Asset Registry package traversal rooted at `LV_SC01` with visited-set cycle protection.
- [ ] Retain a predecessor map and dependency kind sufficient to reconstruct one reference path per package.
- [ ] Fail the analysis when the root package is absent.
- [ ] Filter discovered Blueprint assets to packages contained in the closure.
- [ ] Build the plugin and verify compilation succeeds.

### Task 2: Detailed Blueprint metadata

**Files:**
- Modify: `Plugins/BlueprintFlowAnalyzer/Source/BlueprintFlowAnalyzer/Private/BlueprintFlowAnalyzerModule.cpp`

**Interfaces:**
- Consumes: reachable Blueprint assets from Task 1.
- Produces: detailed variable types/defaults, interfaces, components, functions/events, dispatchers, and call counts in `analysis.json`.

- [ ] Serialize full pin-type details including object/struct/enum subtype and container kind.
- [ ] Export declared-variable defaults from the generated class default object when available.
- [ ] Extract implemented interfaces and construction-script components.
- [ ] Distinguish function, macro, event, and delegate-signature graphs.
- [ ] Aggregate called functions by target path and invocation count; retain unresolved calls.
- [ ] Build the plugin and verify compilation succeeds.

### Task 3: Focused report generation

**Files:**
- Create: `Tools/BlueprintFlowAnalyzer/Generate-LVSC01Report.ps1`
- Create: `Saved/BlueprintFlowAnalyzer/lv_sc01_blueprint_audit.md` (generated)

**Interfaces:**
- Consumes: `Saved/BlueprintFlowAnalyzer/analysis.json`.
- Produces: a human-readable audit with scope summary, required Blueprint table, reference paths, per-BP details, interaction map, and warnings.

- [ ] Add assertions for completed status, zero failed loads, root-map identity, and a reference path for every Blueprint.
- [ ] Render every reachable Blueprint rather than only `/Game/Junha_Made` assets.
- [ ] Include exact variable types/defaults, graph definitions, components, interfaces, and call counts.
- [ ] Include Blueprint-to-Blueprint dependencies and unresolved targets.
- [ ] Run the generator and verify all invariant checks pass.

### Task 4: End-to-end extraction and verification

**Files:**
- Verify: `Saved/BlueprintFlowAnalyzer/analysis.json`
- Verify: `Saved/BlueprintFlowAnalyzer/report.json`
- Verify: `Saved/BlueprintFlowAnalyzer/lv_sc01_blueprint_audit.md`

**Interfaces:**
- Consumes: analyzer executable behavior and report generator from Tasks 1-3.
- Produces: final verified audit artifacts.

- [ ] Run `UnrealEditor-Cmd.exe` with `-BPAnalyzerRun -DDC-ForceMemoryCache`.
- [ ] Confirm Unreal exits with code 0 and the analyzer reports no failed Blueprint loads.
- [ ] Run the report generator against the fresh JSON.
- [ ] Independently verify every reported Blueprint package belongs to the root dependency closure and has a non-empty path.
- [ ] Record extraction warnings separately from fatal errors.

