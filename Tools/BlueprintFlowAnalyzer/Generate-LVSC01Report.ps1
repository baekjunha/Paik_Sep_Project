param(
    [string]$AnalysisPath = "Saved/BlueprintFlowAnalyzer/analysis.json",
    [string]$ExtractionReportPath = "Saved/BlueprintFlowAnalyzer/report.json",
    [string]$ClosurePath = "Saved/BlueprintFlowAnalyzer/lv_sc01_dependency_closure.json",
    [string]$ReportPath = "Saved/BlueprintFlowAnalyzer/lv_sc01_blueprint_audit.md"
)

$ErrorActionPreference = "Stop"
$root = "/Game/Junha_Made/Maps/Paik_Map/LV_SC01"

function Escape-Markdown([object]$Value) {
    if ($null -eq $Value) { return "" }
    return ([string]$Value).Replace("|", "\|").Replace("`r", " ").Replace("`n", " ")
}

function Format-Type([object]$Variable) {
    $parts = [System.Collections.Generic.List[string]]::new()
    if ($Variable.cpp_type) { $parts.Add([string]$Variable.cpp_type) }
    elseif ($Variable.type) { $parts.Add([string]$Variable.type) }
    if ($Variable.type_details.subcategory_object) { $parts.Add("subtype=$($Variable.type_details.subcategory_object)") }
    if ($Variable.type_details.container -and $Variable.type_details.container -ne 'None') { $parts.Add("container=$($Variable.type_details.container)") }
    if ($Variable.type_details.is_reference) { $parts.Add('ref') }
    if ($Variable.type_details.is_const) { $parts.Add('const') }
    return $parts -join '; '
}

$closure = Get-Content -Raw -LiteralPath $ClosurePath | ConvertFrom-Json
if ($closure.entry_map -ne $root) { throw "Unexpected entry map in closure: $($closure.entry_map)" }
$visited = [System.Collections.Generic.HashSet[string]]::new([string[]]$closure.dependency_packages)
$dependencies = @{}
foreach ($property in $closure.edges.PSObject.Properties) {
    $dependencies[$property.Name] = @($property.Value)
}

$analysis = Get-Content -Raw -LiteralPath $AnalysisPath | ConvertFrom-Json
if (@($analysis.errors).Count -gt 0) { throw "Blueprint analysis contains $(@($analysis.errors).Count) extraction errors" }
$extractionReport = Get-Content -Raw -LiteralPath $ExtractionReportPath | ConvertFrom-Json
if ($extractionReport.status -ne "completed" -or $extractionReport.blueprints_failed -ne 0) {
    throw "Blueprint extraction is incomplete: status=$($extractionReport.status), failed=$($extractionReport.blueprints_failed)"
}

$blueprintByPackage = @{}
foreach ($blueprint in $analysis.blueprints) {
    $package = ($blueprint.asset -split '\.')[0]
    $blueprintByPackage[$package] = $blueprint
}

$closureBlueprints = @($closure.blueprints | Sort-Object package)
$reachable = @($closureBlueprints.package)
foreach ($entry in $closureBlueprints) {
    if (-not $blueprintByPackage.ContainsKey($entry.package)) {
        throw "Blueprint detail record is missing: $($entry.package)"
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
function Add-Line([string]$Text = "") { $lines.Add($Text) }

Add-Line "# LV_SC01 필수 Blueprint 전수조사"
Add-Line
Add-Line "- 시작 맵: ``$root``"
Add-Line "- 판정 기준: 최신 Unreal Asset Registry에서 Hard+Soft, Game 전용 ``/Game`` 패키지 참조를 재귀 추적"
Add-Line "- 도달 가능한 ``/Game`` 패키지: $($visited.Count)개"
Add-Line "- 도달 가능한 Blueprint: $($reachable.Count)개 (독립 에셋 $($reachable.Count - 1)개 + 맵 내장 Level Script Blueprint 1개)"
Add-Line "- Blueprint 원본 분석 오류: $(@($analysis.errors).Count)개"
Add-Line "- Blueprint 추출 상태: $($extractionReport.status), 실패 $($extractionReport.blueprints_failed)개"
Add-Line
Add-Line "> 정적 패키지 의존성 기준입니다. 런타임 문자열 로드나 서버 응답으로만 생성되는 클래스는 별도 런타임 추적이 필요합니다."
Add-Line
Add-Line "## 필수 Blueprint 목록"
Add-Line
Add-Line "| # | Blueprint | 부모 클래스 | 참조 거리 | 변수 | 그래프 | 노드 | 포함 경로 |"
Add-Line "|---:|---|---|---:|---:|---:|---:|---|"

$index = 0
foreach ($entry in $closureBlueprints) {
    $index++
    $bp = $blueprintByPackage[$entry.package]
    $nodeCount = @($bp.graphs | ForEach-Object { $_.nodes } | Where-Object { $null -ne $_ }).Count
    $shortName = ($entry.package -split '/')[-1]
    $pathText = ($entry.reference_path | ForEach-Object { ($_ -split '/')[-1] }) -join ' → '
    Add-Line "| $index | ``$shortName`` | ``$(Escape-Markdown $bp.parent_class)`` | $($entry.reference_path.Count - 1) | $(@($bp.declared_variables).Count) | $(@($bp.graphs).Count) | $nodeCount | $(Escape-Markdown $pathText) |"
}

Add-Line
Add-Line "### 맵 내장 Level Script Blueprint"
Add-Line
$levelBp = $blueprintByPackage[$root]
Add-Line "- ``$($levelBp.asset)``"
Add-Line "- 그래프 $(@($levelBp.graphs).Count)개, 변수 $(@($levelBp.declared_variables).Count)개"
Add-Line "- 아래 Blueprint별 상세 절에 그래프·노드·호출 정보까지 포함했습니다."

Add-Line
Add-Line "## Blueprint 간 직접 의존성"
Add-Line
$directDependencyLines = 0
foreach ($entry in $closureBlueprints) {
    $bpDeps = @($dependencies[$entry.package] | Where-Object { $blueprintByPackage.ContainsKey($_) -and $visited.Contains($_) } | Sort-Object -Unique)
    if ($bpDeps.Count -gt 0) {
        $targets = $bpDeps | ForEach-Object { '`' + (($_ -split '/')[-1]) + '`' }
        Add-Line "- ``$(($entry.package -split '/')[-1])`` → $($targets -join ', ')"
        $directDependencyLines++
    }
}
if ($directDependencyLines -eq 0) { Add-Line "직접 BP 패키지 의존성 없음." }

Add-Line
Add-Line "## Blueprint별 상세"

foreach ($entry in $closureBlueprints) {
    $bp = $blueprintByPackage[$entry.package]
    $shortName = ($entry.package -split '/')[-1]
    $allNodes = @($bp.graphs | ForEach-Object { $_.nodes } | Where-Object { $null -ne $_ })
    $callNodes = @($allNodes | Where-Object { $null -ne $_.call })
    $customEvents = @($allNodes | Where-Object { $_.custom_event_name } | ForEach-Object { $_.custom_event_name } | Sort-Object -Unique)
    $unresolved = @($callNodes | Where-Object { $_.call.resolution -eq 'unresolved' })

    Add-Line
    Add-Line "### $shortName"
    Add-Line
    Add-Line "- 에셋: ``$($bp.asset)``"
    Add-Line "- 부모: ``$($bp.parent_class)``"
    Add-Line "- 생성 클래스: ``$($bp.generated_class)``"
    Add-Line "- 포함 근거: ``$($entry.reference_path -join ' → ')``"
    Add-Line "- 규모: 변수 $(@($bp.declared_variables).Count), 그래프 $(@($bp.graphs).Count), 노드 $($allNodes.Count), 호출 노드 $($callNodes.Count), 미해결 호출 $($unresolved.Count)"
    $interfaceText = if (@($bp.implemented_interfaces).Count) { (@($bp.implemented_interfaces) | ForEach-Object { '``' + (Escape-Markdown $_) + '``' }) -join ', ' } else { '없음' }
    Add-Line "- 구현 인터페이스: $interfaceText"

    Add-Line
    Add-Line "#### 컴포넌트"
    Add-Line
    if (@($bp.components).Count -eq 0) {
        Add-Line "선언 컴포넌트 없음."
    } else {
        Add-Line "| 이름 | 클래스 | 부모 노드 |"
        Add-Line "|---|---|---|"
        foreach ($component in $bp.components) {
            Add-Line "| ``$(Escape-Markdown $component.name)`` | ``$(Escape-Markdown $component.class)`` | ``$(Escape-Markdown $component.parent)`` |"
        }
    }

    Add-Line
    Add-Line "#### 선언 변수"
    Add-Line
    if (@($bp.declared_variables).Count -eq 0) {
        Add-Line "선언 변수 없음."
    } else {
        Add-Line "| 이름 | 정확한 타입 | 기본값 | Dispatcher | Replicated | RepNotify | 조건 |"
        Add-Line "|---|---|---|---:|---:|---|---:|"
        foreach ($variable in $bp.declared_variables) {
            Add-Line "| ``$(Escape-Markdown $variable.name)`` | ``$(Escape-Markdown (Format-Type $variable))`` | ``$(Escape-Markdown $variable.default_value)`` | $($variable.event_dispatcher) | $($variable.replicated) | ``$(Escape-Markdown $variable.rep_notify)`` | $($variable.replication_condition) |"
        }
    }

    Add-Line
    Add-Line "#### 선언 함수"
    Add-Line
    if (@($bp.declared_functions).Count -eq 0) {
        Add-Line "선언 함수 없음."
    } else {
        Add-Line "| 함수 | 입력/출력 파라미터 | Flags |"
        Add-Line "|---|---|---:|"
        foreach ($function in $bp.declared_functions) {
            $parameters = @($function.parameters | ForEach-Object { "$($_.direction) $($_.name): $($_.cpp_type)" }) -join '; '
            Add-Line "| ``$(Escape-Markdown $function.name)`` | ``$(Escape-Markdown $parameters)`` | $($function.flags) |"
        }
    }

    Add-Line
    Add-Line "#### 그래프·이벤트"
    Add-Line
    Add-Line "| 그래프 | 종류 | 클래스 | 노드 수 |"
    Add-Line "|---|---|---|---:|"
    foreach ($graph in @($bp.graphs | Sort-Object name)) {
        Add-Line "| ``$(Escape-Markdown $graph.name)`` | ``$(Escape-Markdown $graph.kind)`` | ``$(Escape-Markdown $graph.class)`` | $(@($graph.nodes).Count) |"
    }
    if ($customEvents.Count -gt 0) {
        Add-Line
        $eventNames = $customEvents | ForEach-Object { '`' + (Escape-Markdown $_) + '`' }
        Add-Line "Custom Event: $($eventNames -join ', ')"
    }

    Add-Line
    Add-Line "#### 호출 함수"
    Add-Line
    if ($callNodes.Count -eq 0) {
        Add-Line "함수 호출 노드 없음."
    } else {
        Add-Line "| 함수 | 호출 횟수 | 대상 | 네트워크 특성 |"
        Add-Line "|---|---:|---|---|"
        $groups = $callNodes | Group-Object { $_.call.function_name + "`u{001f}" + $_.call.target + "`u{001f}" + $_.call.resolution } | Sort-Object { ($_.Name -split "`u{001f}")[0] }
        foreach ($group in $groups) {
            $call = $group.Group[0].call
            $network = @()
            if ($call.server_rpc) { $network += 'Server RPC' }
            if ($call.client_rpc) { $network += 'Client RPC' }
            if ($call.multicast_rpc) { $network += 'Multicast RPC' }
            if ($call.reliable) { $network += 'Reliable' }
            $target = if ($call.target) { $call.target } else { '[미해결]' }
            Add-Line "| ``$(Escape-Markdown $call.function_name)`` | $($group.Count) | ``$(Escape-Markdown $target)`` | $(Escape-Markdown ($network -join ', ')) |"
        }
    }
}

Add-Line
Add-Line "## 제외 확인"
Add-Line
$excluded = @($blueprintByPackage.Keys | Where-Object { -not $visited.Contains($_) } | Sort-Object)
Add-Line "전체 분석된 Blueprint $($analysis.blueprints.Count)개 중 $($excluded.Count)개는 ``LV_SC01`` 참조 폐쇄 밖이므로 필수 목록에서 제외했습니다."
Add-Line
foreach ($package in $excluded) { Add-Line "- ``$package``" }

$lines | Set-Content -LiteralPath $ReportPath -Encoding utf8

[pscustomobject]@{
    EntryMap = $root
    DependencyPackages = $visited.Count
    ReachableBlueprints = $reachable.Count
    ExcludedBlueprints = $excluded.Count
    ClosurePath = (Resolve-Path $ClosurePath).Path
    ReportPath = (Resolve-Path $ReportPath).Path
} | Format-List
