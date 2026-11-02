import json
import os
from collections import deque

import unreal


ROOT_PACKAGE = "/Game/Junha_Made/Maps/Paik_Map/LV_SC01"


def project_path(*parts):
    root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    return os.path.join(root, *parts)


registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()

def dependency_options(include_hard, include_soft):
    options = unreal.AssetRegistryDependencyOptions()
    options.set_editor_property("include_soft_package_references", include_soft)
    options.set_editor_property("include_hard_package_references", include_hard)
    options.set_editor_property("include_game_package_references", True)
    options.set_editor_property("include_editor_only_package_references", False)
    options.set_editor_property("include_searchable_names", False)
    options.set_editor_property("include_soft_management_references", False)
    options.set_editor_property("include_hard_management_references", False)
    return options


hard_options = dependency_options(True, False)
soft_options = dependency_options(False, True)

root_assets = registry.get_assets_by_package_name(ROOT_PACKAGE, True)
if not root_assets:
    raise RuntimeError(f"Entry map package not found: {ROOT_PACKAGE}")

visited = {ROOT_PACKAGE}
predecessor = {}
edges = {}
edge_kinds = {}
missing_dependency_packages = set()
pending = deque([ROOT_PACKAGE])

while pending:
    current = pending.popleft()
    hard_dependencies = {
        str(item)
        for item in registry.get_dependencies(current, hard_options)
        if str(item).startswith("/Game/")
    }
    soft_dependencies = {
        str(item)
        for item in registry.get_dependencies(current, soft_options)
        if str(item).startswith("/Game/")
    }
    dependencies = sorted(hard_dependencies | soft_dependencies)
    edges[current] = dependencies
    for dependency in dependencies:
        kinds = []
        if dependency in hard_dependencies:
            kinds.append("hard")
        if dependency in soft_dependencies:
            kinds.append("soft")
        edge_kinds[f"{current} -> {dependency}"] = kinds
        if not registry.get_assets_by_package_name(dependency, True):
            missing_dependency_packages.add(dependency)
        if dependency not in visited:
            visited.add(dependency)
            predecessor[dependency] = current
            pending.append(dependency)

analysis_path = project_path("Saved", "BlueprintFlowAnalyzer", "analysis.json")
with open(analysis_path, "r", encoding="utf-8-sig") as handle:
    analysis = json.load(handle)

blueprint_details = {
    item["asset"].split(".", 1)[0]: item["asset"] for item in analysis["blueprints"]
}
discovered_blueprint_assets = {}
for package in sorted(visited):
    for asset in registry.get_assets_by_package_name(package, True):
        class_name = str(asset.asset_class_path.asset_name)
        if "Blueprint" in class_name:
            discovered_blueprint_assets[package] = {
                "asset": f"{asset.package_name}.{asset.asset_name}",
                "class": class_name,
            }

missing_detail_packages = set(discovered_blueprint_assets).difference(blueprint_details)
embedded_blueprints_missing_details = sorted(
    package
    for package in missing_detail_packages
    if package == ROOT_PACKAGE
    or discovered_blueprint_assets[package]["class"] == "LevelScriptBlueprint"
)
blueprints_missing_details = sorted(
    missing_detail_packages.difference(embedded_blueprints_missing_details)
)
blueprints = []
for package in sorted(set(discovered_blueprint_assets).intersection(blueprint_details)):
    path = [package]
    cursor = package
    while cursor != ROOT_PACKAGE:
        cursor = predecessor.get(cursor)
        if cursor is None:
            raise RuntimeError(f"No predecessor for reachable package: {package}")
        path.append(cursor)
    path.reverse()
    blueprints.append(
        {
            "package": package,
            "asset": discovered_blueprint_assets[package]["asset"],
            "asset_class": discovered_blueprint_assets[package]["class"],
            "reference_path": path,
        }
    )

output = {
    "schema_version": "1.0",
    "source": "live Unreal Asset Registry",
    "dependency_filter": "hard+soft, game-only, /Game packages",
    "entry_map": ROOT_PACKAGE,
    "dependency_packages": sorted(visited),
    "edges": {key: edges[key] for key in sorted(edges)},
    "edge_kinds": {key: edge_kinds[key] for key in sorted(edge_kinds)},
    "missing_dependency_packages": sorted(missing_dependency_packages),
    "discovered_blueprint_assets": [
        {"package": package, **discovered_blueprint_assets[package]}
        for package in sorted(discovered_blueprint_assets)
    ],
    "blueprints_missing_details": blueprints_missing_details,
    "embedded_blueprints_missing_details": embedded_blueprints_missing_details,
    "blueprints": blueprints,
}

output_path = project_path(
    "Saved", "BlueprintFlowAnalyzer", "lv_sc01_dependency_closure.json"
)
with open(output_path, "w", encoding="utf-8") as handle:
    json.dump(output, handle, ensure_ascii=False, indent=2)

unreal.log(
    f"LV_SC01 dependency closure written: packages={len(visited)} "
    f"blueprints={len(blueprints)} path={output_path}"
)
