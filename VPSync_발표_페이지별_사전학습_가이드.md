# VPSync 발표 페이지별 사전학습 가이드

작성 기준: Canva 「VPSync 프로젝트 발표자료」 14페이지 및 현재 `Paik_Sep_Project` 저장소

노드 단위 실행 흐름은 별도 문서 `VPSync_핵심_블루프린트_로직_해설.md`에 실제 Blueprint 복사본 기준으로 정리되어 있다.

## 이 문서를 편하게 읽는 방법

이 문서는 Unreal을 잘 모르는 사람도 발표 흐름을 이해할 수 있도록 다음 순서로 설명한다.

1. **쉽게 말하면**: 이 페이지가 결국 무슨 뜻인지 먼저 설명한다.
2. **발표할 때 이렇게 말하면 된다**: 실제 발표에서 바로 사용할 수 있는 말투로 적었다.
3. **기술적으로 알아둘 것**: 질문을 받았을 때 필요한 BP, 함수, 변수와 데이터 흐름이다.

처음 읽을 때는 함수 이름을 전부 외우려고 하지 않아도 된다. 먼저 아래 역할만 구분하면 이후 내용이 쉬워진다.

- **Shot**: 영화의 장면 단위다. 이 프로젝트에서는 `SH010`, `SH020`, `SH030` 세 장면을 뜻한다.
- **Sequencer**: Unreal 안에서 영상의 시간, 카메라, 애니메이션을 재생하는 타임라인이다.
- **Blueprint 또는 BP**: Unreal에서 노드를 연결해 로직을 만드는 방식이다.
- **Remote Control 또는 RC**: iPad나 웹 화면에서 Unreal의 함수와 값을 조작할 수 있게 해주는 기능이다.
- **WBP**: 화면에 보이는 버튼, 글자, 수치 같은 UI를 만든 Widget Blueprint다.
- **VCam**: iPad 등을 가상 카메라처럼 사용해 Unreal 카메라를 조작하는 기능이다.
- **PIE**: Play In Editor의 약자다. Unreal 편집기 안에서 게임을 실행한 상태다.
- **World Context**: 지금 조작하려는 객체가 편집 화면의 객체인지, 실제 실행 중인 PIE 객체인지 구분하는 실행 환경 정보다.

## 먼저 외워야 할 프로젝트 전체 구조

발표 전체를 한 문장으로 요약하면 다음과 같다.

> VPSync는 Unreal의 Sequencer, Remote Control, VCam, Mocap, UMG를 새 기능처럼 각각 보여주는 프로젝트가 아니라, Shot 선택 → 재생 → 현장 조정 → 실제값 확인 → 리뷰 표시 → 초기화·재검증을 하나의 반복 가능한 VP 리뷰 흐름으로 연결한 프로토타입이다.

핵심 데이터 흐름은 두 갈래다.

```text
[일반 조작]
iPad Remote Control Web Interface
  → RC_VPSyncOperator
  → BP_VPSessionController
  → Shot / Sequencer / Camera / Light / VCam
  → WBP_VP_Operator에서 실제 상태 확인

[Review Pad 드로잉]
iPad vpsync_review_pad.html (:30000)
  → /vpsync/review-drawing Proxy
  → Unreal Remote Control HTTP API (:30010)
  → RC_VPSyncOperator.Receive Review Drawing
  → BP_VPSessionController.ReceiveReviewDrawing
  → ST_ReviewStroke / ReviewStrokes
  → WBP_VP_Operator.DrawPoints / DrawLines
```

발표 전에 위치를 외워둘 핵심 에셋은 다음과 같다.

- 중앙 제어: `Content/Junha_Made/Blueprints/BP_VPSessionController.uasset`
- PC 모니터 UI: `Content/Junha_Made/Blueprints/UI/WBP_VP_Operator.uasset`
- Remote Control Preset: `Content/Junha_Made/Blueprints/RC_VPSyncOperator.uasset`
- Shot enum: `Content/Junha_Made/Enum/E_VPShot.uasset`
- 상태 enum: `Content/Junha_Made/Enum/E_VPSessionState.uasset`
- Shot 구조체: `Content/Junha_Made/Struct/ST_VPShotData.uasset`
- 리뷰 선 구조체: `Content/Junha_Made/Struct/ST_ReviewStroke.uasset`
- Shot 시퀀스: `SEQ_SH010`, `SEQ_SH020`, `SEQ_SH030`
- Master 시퀀스: `SEQ_SC01_MASTER`
- 메인 레벨: `Content/Junha_Made/Maps/Paik_Map/LV_SC01.umap`
- Review Pad 복구 자료: `Tools/VPSyncReviewPad/`

---

## 1페이지 — 표지: VPSync

### 쉽게 말하면

첫 페이지에서는 복잡한 기능 설명을 하지 않고, “이 프로젝트가 무엇을 해결하려는 프로젝트인지”만 분명하게 전달하면 된다. VPSync는 새로운 카메라나 새로운 렌더러를 만든 프로젝트가 아니라, Unreal에 이미 있는 여러 기능을 촬영 현장에서 쓰기 편한 검토 순서로 연결한 프로젝트다.

### 발표할 때 이렇게 말하면 된다

> 안녕하세요. 제가 만든 VPSync는 Unreal Engine을 기반으로 한 버추얼 프로덕션 리뷰 워크플로 프로토타입입니다. iPad에서 장면을 선택하고 조작하면 PC에서 실제 적용 상태를 확인하고, 피드백을 남긴 뒤 같은 장면을 다시 검증할 수 있도록 구성했습니다.

### 이 페이지에서 말해야 하는 핵심

- 프로젝트 성격은 “Unreal 기반 VP Review Workflow Prototype”이다.
- 1인 프로젝트이며 Unreal Engine 5.7 기준이다.
- 목표는 영상 한 편을 만드는 데서 끝나지 않고, 촬영·리뷰 현장에서 반복 검토가 가능한 도구 흐름을 만드는 것이다.

### 미리 숙지할 것

- 엔진 버전은 `.uproject` 기준 UE 5.7이다.
- 사용 플러그인 중 발표와 직접 관련된 것은 `RemoteControl`, `RemoteControlWebInterface`, `VirtualCamera`이다.
- `nDisplay`, `Switchboard`도 활성화되어 있지만 현재 발표 범위의 완성 기능은 아니다.
- 기본 실행 맵과 에디터 시작 맵은 모두 `LV_SC01`이다.

### 예상 질문

**“VPSync가 새로운 렌더링 기술인가?”**  
아니다. 기존 Unreal 기능을 현장 리뷰 순서에 맞게 묶은 워크플로 프로토타입이다.

**“왜 Sync라는 이름인가?”**  
iPad의 조작 의도, Unreal의 실제 런타임 상태, PC 모니터의 검증 정보를 한 흐름으로 맞춘다는 뜻으로 설명하면 된다.

---

## 2페이지 — 목차

### 쉽게 말하면

목차는 기능 목록을 읽는 페이지가 아니다. 청중에게 “왜 만들었는지부터 설명한 뒤, 실제 구조와 문제 해결 과정까지 보여주겠다”는 길 안내를 해주는 페이지다.

### 발표할 때 이렇게 말하면 된다

> 먼저 왜 이런 리뷰 도구가 필요한지 설명하고, 그다음 프로젝트의 구조와 실제 작업 흐름을 보여드리겠습니다. 이후 Mocap과 VCam을 Shot에 통합한 과정, 구현 중 발생한 두 가지 대표 문제, 실제 시연, 그리고 현재 한계와 개선 방향 순서로 말씀드리겠습니다.

### 이 페이지에서 말해야 하는 핵심

발표 흐름은 “필요성 → 구조 → 통합 → 실제 사용 → 문제 해결 → 결과와 한계” 순서다. 기능 나열이 아니라 문제와 해결 과정의 이야기라는 점을 먼저 안내한다.

### 미리 숙지할 것

- 3~4페이지: 왜 만들었는지
- 5~7페이지: 무엇으로 구성되고 어떻게 흐르는지
- 8페이지: Mocap·VCam을 Shot에 어떻게 넣었는지
- 9~10페이지: 구현 중 가장 큰 기술 문제 두 가지
- 11~12페이지: 실제 작동 증거
- 13페이지: 결과·한계·확장 방향

이 페이지에서는 함수 이름을 설명할 필요는 없지만, 이후 모든 내용을 위 여섯 덩어리로 기억해야 발표 중 길을 잃지 않는다.

---

## 3페이지 — 기존 제작과 Virtual Production 비교

### 쉽게 말하면

기존 영화 제작은 촬영과 후반작업이 많이 진행된 뒤에야 완성된 모습을 볼 수 있다. 그때 문제가 발견되면 다시 촬영하거나 많은 작업을 되돌려야 한다. VP는 촬영 초기에 Unreal 화면으로 결과를 미리 보면서 바로 수정할 수 있다. VPSync는 이 “미리 보고 바로 다시 시험하는 과정”을 도구로 만든 것이다.

### 발표할 때 이렇게 말하면 된다

> 기존 제작 방식은 여러 작업이 끝난 뒤 통합 결과를 확인하는 경우가 많아서, 뒤늦게 수정이 생기면 시간과 비용이 커집니다. 반면 버추얼 프로덕션은 제작 초기에 결과를 확인하고 바로 반복할 수 있습니다. 저는 이 장점 중에서도 특히 선택, 재생, 조정, 확인, 재검증이 반복되는 리뷰 과정에 집중했습니다.

### 이 페이지에서 말해야 하는 핵심

- 기존 제작은 후반에 통합 결과를 보기 때문에 수정 비용이 커진다.
- VP는 제작 초기에 결과를 보고 반복 수정할 수 있다는 장점이 있다.
- VPSync는 그중에서도 “빠르게 보고, 조정하고, 다시 확인하는 반복 루프”에 집중한다.

### 미리 숙지할 개념

- Previsualization과 실시간 렌더링이 왜 의사결정을 앞당기는지
- 감독·촬영감독·프로듀서가 같은 상태를 확인하는 것이 왜 중요한지
- “실시간” 자체보다 “반복 가능한 검토”가 프로젝트 핵심이라는 점

### 프로젝트와 연결할 것

이 페이지에는 직접 연결되는 BP 함수가 없다. 대신 뒤에서 다음 흐름이 이 장점을 실제 구현한다는 예고를 한다.

`Select Shot → Play → Adjust → Verify Actual Value → Reset → Re-test`

---

## 4페이지 — Overview: 배경과 목적

### 쉽게 말하면

처음에는 《헤어질 결심》의 옥상 추격 장면을 Unreal로 재현하는 것이 목표였다. 하지만 장면을 한 번 재생하는 것만으로는 “현장에서 사용하는 VP 도구”라고 보기 어렵다고 판단했다. 그래서 장면을 계속 선택하고, 값을 바꾸고, 실제 결과를 확인하는 리뷰 도구로 목표를 확장했다.

### 발표할 때 이렇게 말하면 된다

> 프로젝트는 영화의 옥상 추격 장면을 UE5로 재구성하는 것에서 시작했습니다. 작업을 진행하면서 완성된 장면을 한 번 보여주는 것보다, 현장에서 같은 Shot을 여러 번 선택하고 조정하고 확인하는 과정이 더 중요하다고 판단했습니다. 그래서 iPad는 조작을 담당하고 PC는 실제 적용 결과를 확인하는 리뷰 도구로 발전시켰습니다.

### 이 페이지에서 말해야 하는 핵심

- 《헤어질 결심》 옥상 추격 장면을 UE5로 재구성한 것이 콘텐츠 출발점이다.
- 그러나 결과 영상 재생만으로는 VP 프로젝트의 의미가 약하다고 판단했다.
- 그래서 iPad는 조작, PC는 실제 상태 확인을 담당하는 Review Tool로 발전시켰다.

### 미리 숙지할 것

- 메인 레벨은 `LV_SC01`이다.
- Shot은 `SH010`, `SH020`, `SH030` 세 개다.
- iPad UI와 PC UI를 나눈 이유:
  - iPad: 현장에서 빠르게 명령하는 조작면
  - PC: Unreal이 실제로 적용한 상태를 확인하는 검증면
- “명령값을 보냈다”와 “실제 객체 값이 바뀌었다”는 다르기 때문에 PC Decision Monitor가 필요하다.

### 연결되는 핵심 에셋

- `RC_VPSyncOperator`: iPad 조작 진입점
- `BP_VPSessionController`: 실제 제어 로직
- `WBP_VP_Operator`: 실제 상태와 결과 확인

---

## 5페이지 — 현재 프로젝트 구성

### 쉽게 말하면

이 페이지는 프로젝트의 부품 수를 보여주는 페이지다. 장면은 세 개이고, 조작 화면은 iPad와 PC 두 개이며, 재생 상태는 정상 흐름 기준 세 가지다. 이 모든 것을 한 곳에서 관리하는 “관제실”이 `BP_VPSessionController`다.

### 발표할 때 이렇게 말하면 된다

> 현재 프로토타입은 세 개의 Shot과 두 개의 인터페이스로 구성되어 있습니다. 정상 실행 흐름은 READY, PLAYING, PAUSED 세 상태로 관리하며, 모든 Shot과 상태, Sequencer, 카메라 참조는 BP_VPSessionController 한 곳에서 관리합니다.

### 이름이 어려울 때는 이렇게 이해하면 된다

- `E_VPShot`: Shot 이름만 모아둔 선택 목록
- `E_VPSessionState`: 현재 재생 상태를 나타내는 선택 목록
- `ST_VPShotData`: 한 Shot에 필요한 시퀀스, 플레이어, 카메라를 담는 상자
- `BP_VPSessionController`: 여러 상자를 찾아 실제 재생과 상태 변경을 수행하는 관리자

### 슬라이드 숫자의 의미

- 3 Shots: `SH010`, `SH020`, `SH030`
- 2 Interfaces: iPad + PC
- 3 Session States: 발표상 `READY`, `PLAYING`, `PAUSED`
- 1 Main Controller: `BP_VPSessionController`

### 반드시 알고 있어야 할 데이터 타입

#### `E_VPShot`

- `SH010`
- `SH020`
- `SH030`

#### `E_VPSessionState`

- `READY`
- `PLAYING`
- `PAUSED`
- 실제 enum 에셋에는 `ERROR`도 존재한다.

발표에서는 정상 흐름의 상태가 3개이고, 구현에는 예외 상태 `ERROR`까지 마련했다고 답하면 된다.

#### `ST_VPShotData`

Shot 하나를 제어하기 위한 묶음이다.

- `ShotID`: `E_VPShot`
- `Sequence`: `LevelSequence`
- `SequencePlayer`: `LevelSequencePlayer`
- `Camera`: `CineCameraActor`

#### `ST_ReviewStroke`

- `Points`: `Vector2D` 배열

### `BP_VPSessionController`에서 알아둘 주요 변수

- `CurrentShot`
- `VPSessionState`
- `ActiveSequencePlayer`
- `SelectedSequencePlayer`
- `Shot010Sequence`, `Shot020Sequence`, `Shot030Sequence`
- `DirectionalLightRef`
- `VCamActorRef`
- `OperatorWidgetRef`
- `ReviewStrokes`
- `CurrentStrokePoints`
- `LastRCSeekValue`

### 알아둘 주요 함수·이벤트

- 초기화: `RC_InitializeShots` / Remote Control 표시명 `Initialize Shots`
- Shot 선택: `SelectShots010`, `SelectShot020`, `SelectShot030`
  - `SelectShots010`은 현재 에셋에 실제로 존재하는 이름이며 010만 복수형 오타가 남아 있다.
- 재생: `PlayShots`
- 일시정지/재개: `PauseAndResume`
- 초기화: `ResetSession`
- 상태 문자열: `GetSessionStateText`
- 현재 Shot 시퀀스 조회: `GetCurrentShotSequence`
- Shot 종료 처리: `OnShotFinished`
- UI 강조 갱신: `UpdateShotButtonVisual`

---

## 6페이지 — 핵심 시스템 구조

### 쉽게 말하면

이 구조는 식당에 비유하면 쉽다. iPad는 주문을 넣는 손님, Remote Control Preset은 주문을 접수하는 직원, `BP_VPSessionController`는 실제 일을 배분하는 주방장, `WBP_VP_Operator`는 주문 결과가 제대로 나왔는지 보여주는 확인 화면이다.

iPad가 “초점거리를 바꿔 달라”고 명령했다고 해서 실제 카메라 값이 반드시 바뀐 것은 아니다. Sequencer가 값을 다시 덮거나, 잘못된 World의 객체를 조작할 수도 있다. 그래서 PC 화면이 실제 Unreal 객체의 값을 다시 읽어 보여준다.

### 발표할 때 이렇게 말하면 된다

> 구조는 입력, 중앙 제어, 모니터링의 세 단계로 나뉩니다. iPad에서 들어온 명령은 RC_VPSyncOperator를 통해 BP_VPSessionController로 전달됩니다. 컨트롤러가 Shot, Sequencer, Camera, Light, VCam을 실제로 제어하고, PC Decision Monitor는 그 결과를 다시 읽어서 보여줍니다. 따라서 명령을 보냈다는 사실뿐 아니라 실제 적용됐는지도 확인할 수 있습니다.

### 구조를 왼쪽에서 오른쪽으로 설명

#### 1. Remote Operation — iPad Web Remote

- `Remote Control Web Interface`
- `RC_VPSyncOperator`
- Review Pad의 리뷰/드로잉 입력

여기서는 조작 의도만 보낸다. 실제 상태의 최종 권한은 Unreal 런타임 객체에 있다.

#### 2. Central Control — `BP_VPSessionController`

관리 대상:

- `CurrentShot`
- `VPSessionState`
- Shot/Sequence Reference
- `ActiveSequencePlayer`
- Camera/Directional Light Reference

하단의 Controlled Systems는 Shot, Sequencer, Camera, Light, VCam, Mocap이다.

#### 3. Monitoring — PC Decision Monitor

`WBP_VP_Operator`가 담당한다.

- Shot/Session 상태
- Focal/Aperture/Focus의 실제값
- Directional Light 실제값/설정
- 리뷰 드로잉 표시

### `WBP_VP_Operator`에서 알아둘 것

주요 버튼·위젯:

- `BTN_SH010`, `BTN_SH020`, `BTN_SH030`
- `BTN_PauseResume`
- `BTN_ClearReview`
- `BTN_TestReview`
- `Slider_PlayTime`
- `Text_Seq_State`, `Text_StateText`
- `GRID_Telemetry`

주요 함수:

- `UpdateDecisionMonitor`
- `RefreshDecisionMonitor`
- `UpdateShotButtonVisual`
- `DrawPoints`
- `DrawLines`

### Remote Control Preset에서 알아둘 함수

- `InitializeShots`
- `SelectShots010`, `SelectShot020`, `SelectShot030`
- `PlayShots`
- `PauseAndResume`
- `ResetSession`
- `SetPlaybackNormalized`
- `ToggleOperatorUIVisibility`
- `ToggleVCamActive`, `SetVCamActive`
- `ReceiveReviewDrawing`

### 발표 시 꼭 강조할 설계 원칙

Remote Control Preset이 모든 로직을 직접 가지는 것이 아니다. 외부 입력을 받는 “노출 계층”이고, 실제 상태 전환과 객체 제어는 `BP_VPSessionController`로 모은다.

추가로 확인된 실제 UI 연결은 다음과 같다.

- `WBP_VP_Operator.Construct`가 World에서 `BP_VPSessionController`를 찾아 참조로 저장한다.
- Controller의 `OnShotFinished`를 WBP의 `UI_ShotFinished`에 바인딩한다.
- Shot이 끝나면 UI가 SH010·SH020·SH030 버튼을 다시 활성화한다.
- 0.1초 반복 Timer가 `RefreshDecisionMonitor → UpdateDecisionMonitor`를 호출한다.
- Tick은 Player의 `CurrentTime / Duration`으로 재생 Slider를 갱신한다.
- `bUpdatingSlider`가 프로그램에 의한 Slider 갱신과 사용자의 직접 조작을 구분한다.

---

## 7페이지 — VP Review Workflow

### 쉽게 말하면

이 페이지가 프로젝트의 실제 사용 순서다. 보고 싶은 장면을 고르고, 재생하면서 검토하고, 카메라나 조명을 바꾼 뒤 실제값을 확인한다. 검토가 끝나면 초기 상태로 되돌려 같은 조건에서 다시 시험한다.

### 발표할 때 이렇게 말하면 된다

> 사용자는 먼저 검토할 Shot을 선택합니다. 이후 Sequencer를 재생해 장면을 확인하고, 필요한 경우 카메라나 조명 값을 조정합니다. PC에서는 명령값이 아니라 실제 적용된 값을 확인합니다. 검토가 끝나면 Reset을 눌러 READY 상태로 돌아가고, 같은 과정을 다시 반복할 수 있습니다.

### 함수 이름을 흐름으로 외우는 방법

```text
고른다       → SelectShot...
재생한다     → PlayShots
멈추거나 잇는다 → PauseAndResume
시간을 옮긴다 → SetPlaybackNormalized
처음으로 간다 → ResetSession
```

### 1. Shot Select

- `SelectShots010`, `SelectShot020`, `SelectShot030`
- `CurrentShot` 변경
- 대응하는 `ST_VPShotData`에서 Sequence, SequencePlayer, Camera를 선택
- `UpdateShotButtonVisual`로 PC UI의 선택 상태 갱신

### 2. Play / Review

- `PlayShots` 실행
- `ActiveSequencePlayer` 또는 `SelectedSequencePlayer`가 재생 주체가 됨
- `VPSessionState`를 `PLAYING`으로 전환
- 종료 시 `OnShotFinished` 처리

### 3. Adjust / Verify

- Remote Control에서 Camera·Light 값을 조정
- PC UI는 명령값이 아니라 실제 `CineCameraComponent`와 `DirectionalLight` 값을 읽어 표시
- 카메라 확인 항목: `CurrentFocalLength`, `CurrentAperture`, Manual Focus Distance
- 조명 확인 항목: Directional Light Intensity

### 4. Reset / Re-test

- `ResetSession`
- 재생 위치와 상태를 초기화하고 `READY`로 복귀
- 같은 Shot을 동일 조건에서 다시 검증

### Scrub을 설명할 때

- 외부 값은 `RC_SeekValue`/`SeekValue`로 들어온다.
- `SetPlaybackNormalized`가 0~1 정규화 값을 시퀀스의 실제 Frame/Duration에 매핑한다.
- PC UI의 `Slider_PlayTime`도 같은 재생 위치 개념을 사용한다.

### 상태 전환을 정확히 말하기

```text
READY --PlayShots--> PLAYING
PLAYING --PauseAndResume--> PAUSED
PAUSED --PauseAndResume--> PLAYING
어느 상태든 --ResetSession--> READY
예외 발생 시 --> ERROR
```

---

## 8페이지 — Shot Integration: Mocap과 VCam

### 쉽게 말하면

Mocap 장비에서 받은 움직임은 Unreal 캐릭터와 뼈 구조가 바로 맞지 않을 수 있다. 그래서 중간에 IK Retarget 과정을 거쳐 캐릭터에 맞게 움직임을 변환하고, 그 결과를 Sequencer 안에 넣었다. VCam은 iPad의 움직임을 가상 카메라 조작에 사용하는 기능이며, 이것도 별도 데모가 아니라 같은 Shot 리뷰 과정 안에 연결했다.

### 발표할 때 이렇게 말하면 된다

> Rokoko에서 얻은 Mocap 데이터를 FBX로 가져온 뒤, 원본 스켈레톤과 Unreal 캐릭터의 구조가 다르기 때문에 IK Retarget을 수행했습니다. 변환된 애니메이션은 각 Shot의 Sequencer에 배치했습니다. VCam도 별도 기능으로 보여주는 것이 아니라, 현재 선택한 Shot과 카메라를 검토하는 흐름 안에서 활성화할 수 있도록 연결했습니다.

### IK Retarget을 한 문장으로 설명하면

> 서로 뼈 구조가 다른 두 캐릭터 사이에서 움직임을 맞춰 옮기는 과정입니다.

### Mocap 파이프라인

슬라이드의 순서는 다음과 같다.

`Rokoko → FBX → IK Retarget → Sequencer`

실제 관련 에셋:

- `IK_Rokoko`
- `IK_SKM_UEFN_Mannequin`
- `RTG_SH020_RokokoToManny`
- `A_SH020_Mocap_Manny`
- 원본/가공 애니메이션 에셋들
- 통합 대상 `SEQ_SH020`

알아둘 설명:

- Rokoko 스켈레톤과 Unreal 캐릭터 스켈레톤이 다르므로 IK Rig와 IK Retargeter가 필요하다.
- Retarget 결과 애니메이션을 Sequencer의 Skeletal Animation Track에 배치한다.
- `SEQ_SH010`, `SEQ_SH020`, `SEQ_SH030`에는 Camera Cut, Transform, Skeletal Animation 및 카메라 속성 Track이 포함되어 있다.

### VCam 통합

관련 요소:

- `VirtualCamera` 플러그인
- 맵의 `VCamActor_C_0`
- `BP_VPSessionController.VCamActorRef`
- `GetVCamComponent`
- `ToggleVCamActive`
- `SetVCamActive`

알아둘 설명:

- VCam은 단순히 켜는 기능이 아니라 현재 Shot/Camera 리뷰 흐름에 들어가야 의미가 있다.
- 활성화 시 어떤 CineCamera/VCam Component를 제어하는지 참조 관계를 설명할 수 있어야 한다.
- VCam 입력 결과도 Sequencer가 같은 카메라 속성을 평가하고 있으면 충돌할 수 있으므로 다음 페이지의 Property Ownership 문제와 이어진다.

---

## 9페이지 — Troubleshooting 1: Sequencer와 Remote Control의 Property Ownership 충돌

### 쉽게 말하면

한 카메라 값을 두 사람이 동시에 잡고 있는 상황이다. Remote Control이 초점거리를 50으로 바꿔도 Sequencer가 매 프레임 “원래 값은 35야”라고 다시 쓰면 화면은 35로 돌아간다. 입력이 전달되지 않은 것이 아니라, 전달된 뒤 다른 시스템이 다시 덮은 것이다.

### 발표할 때 이렇게 말하면 된다

> Remote Control 입력은 정상적으로 들어왔지만 카메라 값이 유지되지 않는 문제가 있었습니다. 원인을 확인해 보니 Sequencer의 카메라 Property Track이 같은 값을 매 프레임 평가하면서 Remote Control 값을 다시 덮고 있었습니다. 그래서 리뷰 중에는 해당 Track을 삭제하지 않고 Mute하여 원래 연출 데이터는 보존하면서 Remote Control에 임시 제어권을 주었습니다.

### `Mute`가 의미하는 것

파일이나 키프레임을 삭제하는 것이 아니다. 해당 Track이 잠시 값을 계산하고 적용하지 못하게 쉬게 만드는 것이다. Mute를 해제하면 기존 Sequencer 연출을 다시 사용할 수 있다.

### 증상

- Remote Control 다이얼을 움직여도 카메라 값이 유지되지 않는다.
- 잠깐 바뀌었다가 Sequencer 값으로 돌아오거나, 입력 자체가 먹지 않는 것처럼 보인다.

### 원인

Sequencer에 다음 카메라 Property Track이 활성화되어 있으면 매 프레임 Sequencer가 값을 평가한다.

- Current Focal Length
- Current Aperture
- Manual Focus Distance

Remote Control과 Sequencer가 같은 Property를 동시에 쓰려고 하고, Sequencer 평가가 Remote Control 값을 다시 덮는다.

### 프로젝트의 해결 방식

- `SetTrackMute`로 관련 Track 평가를 끈다.
- 카메라 계열 묶음 처리: `SetCameraLensMute`
- 개별 제어:
  - `SetFocalMute`
  - `SetApertureMute`
  - `SetFocusMute`
- 조명 계열: `SetDirectionalLightMute`
- Focal override 관련:
  - `ApplyFocalOverride`
  - `FocalMuteOn`
  - `FocalMuteOff`
  - `bFocalOverride`

### 발표에서 반드시 구분할 것

- Track을 지우는 것이 아니라 필요할 때 평가를 Mute한다.
- 따라서 원래 Sequencer 연출 데이터를 보존하면서 현장 조정 권한을 Remote Control 쪽으로 넘길 수 있다.
- 검증은 PC Decision Monitor의 `LIVE` 값이 실제로 변하는지를 보고 한다.

### 예상 질문

**“그냥 키프레임을 삭제하면 되지 않나?”**  
그러면 원래 연출 데이터를 잃는다. 리뷰 모드에서는 Track 평가만 잠시 끄고, 필요하면 다시 원래 연출로 돌아갈 수 있어야 한다.

---

## 10페이지 — Troubleshooting 2: Editor World와 PIE World Context

### 쉽게 말하면

Unreal 편집 화면에 있는 `BP_VPSessionController`와 Play 버튼을 눌러 실행된 화면의 `BP_VPSessionController`는 이름과 설계도는 같지만 실제로는 서로 다른 복사본이다. 편집 화면의 복사본을 조작하면 함수 호출은 성공해도 실행 화면에는 변화가 없다.

집 주소에 비유하면 Blueprint 이름은 사람 이름이고, World와 Object Path는 실제 주소다. 같은 이름의 사람이 있어도 정확한 주소를 찾아야 원하는 사람에게 명령이 전달된다.

### 발표할 때 이렇게 말하면 된다

> 두 번째 문제는 World Context였습니다. 같은 BP_VPSessionController라도 Editor World와 PIE World에는 서로 다른 인스턴스로 존재합니다. 처음에는 Remote Control 호출 자체는 성공했지만 Editor 쪽 객체를 가리켜서 PIE 화면이 바뀌지 않았습니다. Object Path와 Binding Context를 비교해 실제 실행 중인 PIE 인스턴스로 연결하면서 해결했습니다.

### 증상

- Remote Control 요청 성공 또는 함수 호출 로그는 보인다.
- 하지만 PIE 화면의 객체 상태는 바뀌지 않는다.

### 원인

`BP_VPSessionController`라는 같은 Blueprint 클래스라도 Editor World와 PIE World에는 서로 다른 인스턴스가 존재한다. 잘못된 World의 객체에 Remote Control이 바인딩되면 호출은 성공해도 사용자가 보는 PIE 상태는 바뀌지 않는다.

### 확인 방법

- Object Path를 비교한다.
- 맵에는 런타임 컨트롤러 인스턴스 `PersistentLevel.BP_VPSessionController_C_1`이 존재한다.
- RC Preset의 Binding Context가 어느 World/Level 인스턴스를 가리키는지 확인한다.
- `GetActorOfClass`를 사용할 때도 어떤 World Context Object를 넣는지 확인한다.

### 관련 함수·항목

- `EditorRequestBeginPlay`
- `EditorRequestEndPlay`
- `IsInPlayInEditor`
- `TestFindController`
- Remote Control의 `BindingContext`, `LastBoundObjectPath`, `LevelWithLastSuccessfulResolve`

### 핵심 문장

> 같은 Blueprint 클래스라는 사실은 같은 객체라는 뜻이 아니다. Runtime 제어에서는 “어느 World의 어느 인스턴스인가”가 주소의 일부다.

---

## 11페이지 — 시연영상 1: 기본 조작과 실제 상태 확인

### 쉽게 말하면

이 영상에서는 “버튼이 눌렸다”만 보면 안 된다. iPad에서 보낸 명령, Unreal 장면의 실제 변화, PC 모니터에 표시되는 상태가 서로 맞는지를 함께 봐야 한다.

### 영상을 틀기 전에 청중에게 알려줄 것

> 왼쪽 또는 상단 화면은 사용자가 보내는 명령이고, Unreal 화면은 실제 장면 결과입니다. PC Decision Monitor에 표시되는 Shot, 상태, 카메라 수치는 Unreal 객체에서 다시 읽은 값입니다. 세 화면이 같은 상태를 가리키는지를 중심으로 봐주시면 됩니다.

### 영상을 보여주면서 말할 짧은 멘트

- “지금 SH010을 선택해서 현재 Shot이 변경됐습니다.”
- “Play를 누르자 상태가 READY에서 PLAYING으로 바뀝니다.”
- “Pause 후에는 PAUSED가 표시되고, 다시 누르면 이어서 재생됩니다.”
- “카메라 값을 조정하면 PC의 LIVE 값이 실제로 바뀌는 것을 확인할 수 있습니다.”
- “Reset 후에는 재생 위치와 상태가 초기화되어 같은 Shot을 다시 검토할 수 있습니다.”

슬라이드 구성상 iPad/웹 조작 화면과 Unreal 실행 화면을 함께 보여주는 기본 워크플로 시연이다.

### 영상 재생 전에 숙지할 시연 순서

1. `SH010/020/030` 중 하나 선택
2. PC 화면에서 Shot 강조와 `READY` 확인
3. `PlayShots`
4. `PLAYING` 확인
5. `PauseAndResume`으로 `PAUSED`/재개 확인
6. Scrub 조작 시 `SetPlaybackNormalized`로 재생 위치가 이동하는지 확인
7. Focal/Aperture/Focus 또는 Light를 조정
8. PC의 `BASE`와 `LIVE` 값 차이 확인
9. 필요하면 VCam 활성화
10. `ResetSession` 후 `READY`로 복귀

### 각 화면에서 무엇을 증거로 볼지

- iPad: 사용자가 어떤 명령을 보냈는가
- Unreal Viewport: Shot/카메라/장면이 실제로 바뀌었는가
- PC Decision Monitor: `CurrentShot`, 상태, Actual/Live 수치가 실제 객체와 일치하는가

### 영상이 멈췄을 때 설명할 수 있어야 하는 것

- Shot을 먼저 선택했는지
- `ActiveSequencePlayer`가 유효한지
- 현재 상태가 `PAUSED`인지
- 해당 Camera Property Track이 Mute되어 있는지
- RC Preset이 PIE World 인스턴스를 가리키는지

---

## 12페이지 — 시연영상 2: Review Pad 드로잉

### 쉽게 말하면

iPad에서 화면 위에 선을 그리면 그 그림을 이미지 파일로 보내는 것이 아니다. 선을 이루는 점들의 좌표를 작은 문자열로 만들어 Unreal에 보낸다. Unreal은 그 문자열을 다시 점 배열로 바꾸고 PC 화면에 같은 선을 그린다.

### 발표할 때 이렇게 말하면 된다

> Review Pad에서는 Apple Pencil이나 손가락으로 그린 선을 0부터 1 사이의 좌표로 저장합니다. SEND를 누르면 점 좌표 문자열이 PC의 Web Proxy를 거쳐 Unreal Remote Control 함수로 전달됩니다. Unreal에서는 문자열을 Stroke와 Point로 다시 나누고, PC Decision Monitor에 선을 재구성합니다. 좌표를 정규화했기 때문에 iPad와 PC의 해상도가 달라도 같은 상대 위치에 표시됩니다.

### 왜 굳이 문자열로 보냈는가

프로토타입 단계에서 Remote Control 함수의 `FString` 입력 하나로 쉽게 전달하고 파싱할 수 있기 때문이다. 구조가 단순해 디버깅도 쉽지만, 데이터가 커지거나 여러 사용자가 동시에 그리면 JSON 구조화, 압축, 사용자·시간 정보 추가가 필요하다.

### 웹 쪽 처리

`vpsync_review_pad.html`의 핵심 함수:

- `resizeCanvas`: 화면 크기와 DPR에 맞게 Canvas 재설정
- `redraw`: 저장된 Stroke 다시 그리기
- `pointFromEvent`: 포인터 좌표를 0~1 정규화 좌표로 변환
- `farEnough`: 약 3px 이상 이동한 점만 저장해 데이터 과밀 방지
- `serializeStrokes`: Stroke와 Point를 문자열로 직렬화

데이터 포맷:

```text
한 Point: x,y
Point 구분: ;
Stroke 구분: |

예: 0.1000,0.2000;0.1200,0.2300|0.5000,0.5000;0.5500,0.5200
```

### 네트워크 경로

```text
iPad http://PC_IP:30000/vpsync_review_pad.html
  → POST /vpsync/review-drawing
  → App.ts Proxy
  → PUT http://127.0.0.1:30010/remote/preset/RC_VPSyncOperator/function/Receive Review Drawing
  → Parameters.DrawingData
```

### Unreal 쪽 처리

- RC 함수 표시명: `Receive Review Drawing`
- 실제 Blueprint 함수: `ReceiveReviewDrawing(DrawingData: FString)`
- 문자열을 `|`, `;`, `,` 기준으로 파싱
- `Conv_StringToDouble`, `MakeVector2D`로 좌표 복원
- `ST_ReviewStroke.Points`에 저장
- 기존 `ReviewStrokes`를 비운 뒤 이번 SEND의 Stroke들을 저장
- `WBP_VP_Operator.OnPaint`에서 `ReviewStrokes`를 순회
- 정규화 좌표에 현재 Widget의 Local Width/Height를 곱해 화면 좌표로 변환
- `DrawLines`로 PC 화면에 표시
- `ClearReviewDrawing`과 `BTN_ClearReview`로 초기화

실제 Draw 설정은 두께 `3.0`, Anti Alias 활성, 노랑·연두 계열 색상이다. 각 Stroke마다 별도로 `DrawLines`를 호출하므로 펜을 떼었다가 새로 그린 두 선이 서로 연결되지 않는다.

### 반드시 설명할 설계 이유

- 좌표를 픽셀이 아니라 0~1로 정규화했기 때문에 iPad와 PC의 해상도가 달라도 같은 상대 위치에 그릴 수 있다.
- WBP는 `GetCachedGeometry → GetLocalSize`로 현재 표시 크기를 구해 정규화 좌표를 실제 화면 좌표로 바꾼다.
- iPad는 `:30010`에 직접 접근하지 않고 `:30000` WebApp Proxy를 통한다.
- HTML과 Proxy 수정은 엔진 플러그인 WebApp에도 반영·빌드해야 하므로 프로젝트 Git Pull만으로 완전히 복구되지 않는다.

---

## 13페이지 — 결과 · 한계 · 향후 개선

### 쉽게 말하면

이 페이지에서는 모든 기능이 완성됐다고 말하기보다, “어디까지 실제로 동작하고 무엇이 아직 프로토타입 수준인지”를 정직하게 나누어 말해야 한다. 이 구분이 분명할수록 프로젝트를 제대로 이해하고 있다는 인상을 준다.

### 발표할 때 이렇게 말하면 된다

> 현재는 세 개의 Shot을 선택하고 재생 상태를 제어하며, iPad 조작과 PC 실제값 확인, Mocap·VCam·Review Pad까지 하나의 리뷰 흐름으로 연결했습니다. 다만 Shot이 세 개로 고정되어 있고 Review Pad 설치가 로컬 엔진 환경에 의존하며, 실제 LED와 nDisplay 운영까지는 아직 연결하지 못했습니다. 다음 단계에서는 Shot을 데이터 기반으로 일반화하고, 리뷰 내용을 Shot별로 저장하며, 실제 VP 장비와 연결하고자 합니다.

### 한계를 말할 때 주의할 점

- “실패했다”가 아니라 “현재 프로토타입의 경계가 여기까지다”라고 설명한다.
- 개선 방향은 막연하게 “고도화”라고 하지 말고 데이터화, 자동 설치, 저장 기능, 장비 연동처럼 구체적으로 말한다.
- nDisplay는 프로젝트에 켜져 있지만 VPSync 기능과 완전히 통합된 것은 아니므로 둘을 구분한다.

### 현재 결과

- 3개 Shot 선택·재생·상태 제어
- iPad Remote Control 연동
- PC에서 실제 Camera/Light 값 확인
- Mocap Retarget 및 Sequencer 통합
- VCam 활성화 및 리뷰 흐름 연동
- Review Pad 드로잉 전달 및 PC 재표시

### 현재 한계와 코드 근거

#### Shot 구성이 고정

- `E_VPShot`이 `SH010/020/030`으로 고정
- 선택 함수도 `SelectShots010`, `SelectShot020`, `SelectShot030`으로 분리
- Sequence 변수도 `Shot010Sequence`, `Shot020Sequence`, `Shot030Sequence`으로 고정

#### Review Pad가 로컬 엔진 환경에 의존

- HTML과 Proxy가 `RemoteControlWebInterface` 엔진 플러그인 WebApp에 수동 반영되어야 함
- `npm build`와 Unreal 재시작이 필요
- 저장소의 `Tools/VPSyncReviewPad`는 복구 자료이지 자동 설치 기능은 아님

#### LED/nDisplay 미연동

- 프로젝트에는 nDisplay 플러그인, DisplayCluster 엔진 설정, 예제 `.ndisplay` 파일이 존재한다.
- 하지만 VPSync의 Shot/Review/Decision Monitor 흐름과 실제 LED Stage 운영까지 연결된 상태는 아니다.
- 따라서 “nDisplay가 없다”가 아니라 “환경은 준비되어 있으나 VPSync 워크플로 통합은 미완료”라고 말하는 것이 정확하다.

### 향후 개선을 구체적으로 말하기

- `DataTable` 또는 Primary Data Asset 기반으로 Shot 정의를 데이터화
- Shot 선택 함수를 하나의 `SelectShot(E_VPShot)`로 통합
- Shot별 ReviewStroke, 작성자, 시간, 상태를 저장
- Review Pad Proxy를 프로젝트 플러그인 또는 별도 서비스로 패키징
- RC Preset의 PIE/Runtime Binding을 자동 재해결
- nDisplay/Switchboard 세션 상태와 VPSync SessionState 연동
- 멀티유저 환경에서 Shot Lock과 리뷰 충돌 처리

---

## 14페이지 — Thank You / 질의응답

### 쉽게 말하면

마지막에는 새로운 내용을 추가하지 않는다. 프로젝트의 핵심을 한 문장으로 다시 말하고 질문을 받으면 된다.

### 마무리 멘트 예시

> VPSync는 각각 따로 존재하던 Unreal 기능들을 촬영 현장에서 반복해서 사용할 수 있는 리뷰 흐름으로 연결한 프로젝트입니다. 현재는 프로토타입이지만, Shot 데이터화와 리뷰 저장, nDisplay 연동을 통해 실제 VP 운영 도구로 확장할 수 있습니다. 감사합니다.

마지막 페이지에서는 기능을 더 설명하기보다 질문에 대비한다.

### 가장 가능성 높은 질문과 답변 요지

**왜 C++이 아니라 Blueprint인가?**  
프로토타입 단계에서 Remote Control, Sequencer, UMG, VCam 연결을 빠르게 검증하기 위해 Blueprint 중심으로 구성했다. 확장성과 배포 안정성이 필요한 부분은 이후 C++/Plugin화 대상이다.

**왜 iPad UI와 PC UI를 나눴나?**  
iPad는 조작 효율, PC는 실제 상태 검증이 목적이다. 명령값과 실제값을 분리해 보는 것이 현장 오류를 줄인다.

**왜 30000과 30010 두 포트가 필요한가?**  
30000은 Remote Control Web Interface와 정적 Review Pad/Proxy 진입점, 30010은 Unreal Remote Control HTTP API다. iPad 요청은 30000을 통해 PC 내부의 30010으로 전달한다.

**Sequencer와 Remote Control이 충돌한 이유는?**  
둘이 같은 카메라 Property를 소유하려 했기 때문이다. 리뷰 중에는 해당 Track 평가를 Mute해 Remote Control이 값을 유지하도록 했다.

**World Context 문제를 어떻게 찾았나?**  
함수 호출 여부만 보지 않고 Object Path를 비교해 Editor World 객체와 PIE World 객체가 다른 인스턴스임을 확인했다.

**Shot을 더 추가하려면?**  
현재는 enum, 변수, 함수가 3개 Shot에 맞춰져 있어 수동 확장이 필요하다. 다음 단계는 Shot 데이터를 배열/DataTable화하고 선택 함수를 일반화하는 것이다.

---

## 발표 직전 점검 체크리스트

- [ ] `LV_SC01`이 열리고 `BP_VPSessionController_C_1`이 존재한다.
- [ ] `SEQ_SH010`, `SEQ_SH020`, `SEQ_SH030` 참조가 모두 유효하다.
- [ ] 각 Shot의 CineCameraActor 참조가 유효하다.
- [ ] `DirectionalLightRef`, `VCamActorRef`, `OperatorWidgetRef`가 유효하다.
- [ ] RC Preset `RC_VPSyncOperator`가 현재 사용할 World 인스턴스에 바인딩되어 있다.
- [ ] `Initialize Shots` 후 `READY`가 표시된다.
- [ ] 세 Shot 선택과 `Play/Pause/Reset`이 모두 된다.
- [ ] Scrub 0~1 값과 실제 재생 위치가 맞는다.
- [ ] Camera Track Mute 후 Focal/Aperture/Focus LIVE 값이 변한다.
- [ ] PC에서 Review Pad가 `127.0.0.1:30000`으로 열린다.
- [ ] iPad에서 `PC_IP:30000`으로 열린다.
- [ ] SEND 후 `ReceiveReviewDrawing`까지 도달하고 PC에 선이 나타난다.
- [ ] `CLEAR DRAWING`이 동작한다.
- [ ] 영상 시연이 실패할 경우를 대비해 11·12페이지의 녹화 영상을 즉시 재생할 수 있다.

## 마지막으로 외울 10개 이름

1. `BP_VPSessionController`
2. `RC_VPSyncOperator`
3. `WBP_VP_Operator`
4. `E_VPShot`
5. `E_VPSessionState`
6. `ST_VPShotData`
7. `ST_ReviewStroke`
8. `SetPlaybackNormalized`
9. `ReceiveReviewDrawing`
10. `SetTrackMute`
