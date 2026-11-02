param(
    [string]$ClosurePath = "Saved/BlueprintFlowAnalyzer/lv_sc01_dependency_closure.json"
)

$ErrorActionPreference = "Stop"
$expectedRoot = "/Game/Junha_Made/Maps/Paik_Map/LV_SC01"
$closure = Get-Content -Raw -LiteralPath $ClosurePath | ConvertFrom-Json

if ($closure.entry_map -ne $expectedRoot) {
    throw "Expected entry_map '$expectedRoot', got '$($closure.entry_map)'"
}

if (-not $closure.dependency_packages -or $closure.dependency_packages.Count -lt 1) {
    throw "dependency_packages is empty"
}
if (-not $closure.blueprints -or $closure.blueprints.Count -lt 1) {
    throw "No reachable Blueprints were reported"
}

$packages = [System.Collections.Generic.HashSet[string]]::new([string[]]$closure.dependency_packages)
$seenBlueprints = [System.Collections.Generic.HashSet[string]]::new()
if ($null -ne $closure.missing_dependency_packages -and @($closure.missing_dependency_packages).Count -gt 0) {
    throw "Unresolved dependency packages: $($closure.missing_dependency_packages -join ', ')"
}
if ($null -ne $closure.blueprints_missing_details -and @($closure.blueprints_missing_details).Count -gt 0) {
    throw "Reachable Blueprints missing details: $($closure.blueprints_missing_details -join ', ')"
}
foreach ($blueprint in $closure.blueprints) {
    if (-not $seenBlueprints.Add($blueprint.package)) {
        throw "Duplicate Blueprint package: $($blueprint.package)"
    }
    if (-not $packages.Contains($blueprint.package)) {
        throw "Blueprint is outside dependency closure: $($blueprint.package)"
    }
    if (-not $blueprint.reference_path -or $blueprint.reference_path.Count -lt 1) {
        throw "Blueprint has no reference path: $($blueprint.package)"
    }
    if ($blueprint.package -ne $expectedRoot -and $blueprint.reference_path.Count -lt 2) {
        throw "Non-root Blueprint reference path is too short: $($blueprint.package)"
    }
    if ($blueprint.reference_path[0] -ne $expectedRoot) {
        throw "Reference path does not start at entry map: $($blueprint.package)"
    }
    if ($blueprint.reference_path[-1] -ne $blueprint.package) {
        throw "Reference path does not end at Blueprint package: $($blueprint.package)"
    }
    for ($index = 0; $index -lt $blueprint.reference_path.Count - 1; $index++) {
        $source = $blueprint.reference_path[$index]
        $target = $blueprint.reference_path[$index + 1]
        if (-not $packages.Contains($source) -or -not $packages.Contains($target)) {
            throw "Reference path contains a package outside the closure: $source -> $target"
        }
        $targets = @($closure.edges.$source)
        if ($target -notin $targets) {
            throw "Reference path contains a nonexistent edge: $source -> $target"
        }
        $kindKey = "$source -> $target"
        if (-not $closure.edge_kinds.$kindKey) {
            throw "Dependency kind is missing for edge: $kindKey"
        }
    }
}

if ($closure.discovered_blueprint_assets.Count -ne ($closure.blueprints.Count + @($closure.embedded_blueprints_missing_details).Count)) {
    throw "Discovered Blueprint count does not match detailed Blueprint count"
}

[pscustomobject]@{
    Status = "PASS"
    EntryMap = $closure.entry_map
    DependencyPackages = $closure.dependency_packages.Count
    Blueprints = $closure.blueprints.Count
} | Format-List
