# LV_SC01 Blueprint Audit Design

## Intent

Identify only the Blueprints that are statically reachable from `/Game/Junha_Made/Maps/Paik_Map/LV_SC01`, then document why each Blueprint is required and how it behaves.

## Goals

- Traverse hard and soft package references recursively from `LV_SC01`.
- Exclude unrelated sample and marketplace Blueprints.
- Record a reproducible reference path from the map to every included Blueprint.
- Inventory each included Blueprint's parent, interfaces, components, declared variables, function/event graphs, dispatchers, called functions, and Blueprint-to-Blueprint relationships.
- Report unresolved references and extraction failures explicitly.

## Non-goals

- Dynamic runtime tracing or PIE coverage.
- Editing Blueprint assets.
- Declaring an asset unused solely because no runtime execution trace exists.

## Approach

Use Unreal Asset Registry dependency data to build the recursive package closure from the default map. Load only reachable `UBlueprint` assets and extend the existing read-only `BlueprintFlowAnalyzer` JSON with reference evidence and detailed Blueprint metadata. Generate a Markdown report from that JSON and validate that every included Blueprint has a path back to `LV_SC01`.

## Constraints

- Unreal Engine 5.7.
- Analysis must be read-only with respect to project assets.
- Generated output lives under `Saved/BlueprintFlowAnalyzer`.
- The default-map package is `/Game/Junha_Made/Maps/Paik_Map/LV_SC01`.

