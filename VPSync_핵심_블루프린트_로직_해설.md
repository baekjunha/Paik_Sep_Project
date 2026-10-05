# VPSync 핵심 블루프린트 로직 해설

이 문서는 복사해 준 블루프린트 노드 텍스트 12개를 실제 연결선 기준으로 읽어서 정리한 것이다. 발표 자료의 개념 설명보다 한 단계 더 들어가서, **버튼을 눌렀을 때 어떤 함수가 실행되고, 어떤 변수가 바뀌며, 왜 다음 동작이 가능한지**를 이해하는 데 목적이 있다.

---

---

## 1. 프로젝트 전체 구조

`BP_VPSessionController`가 여러 Shot의 Level Sequence Player를 관리하고, `WBP_VP_Operator`가 그 상태를 화면에 보여 주며, iPad/Remote Control 입력이 Controller의 함수를 호출하는 구조다.

```text
iPad / Remote Control / Operator UI
                │
                ▼
       BP_VPSessionController
     ┌──────────┼───────────┐
     ▼          ▼           ▼
Shot 선택    Sequence 재생   카메라·조명 조절
     │          │           │
     └──────────┼───────────┘
                ▼
        WBP_VP_Operator 갱신
```

쉽게 말하면 `BP_VPSessionController`는 이 프로젝트의 **진행 요원 겸 교통정리 담당자**다. 어떤 Shot을 볼지, 지금 재생 중인지, 슬라이더를 어디로 옮길지, 카메라·조명 트랙을 잠시 끌지를 한곳에서 결정한다.

---

## 2. 핵심 블루프린트와 데이터

### `BP_VPSessionController`

프로젝트의 핵심 제어 BP다. 다음 책임을 가진다.

- 시작할 때 UI와 Sequence Player를 준비한다.
- 현재 선택된 Shot을 기억한다.
- Play, Pause, Resume, Reset을 처리한다.
- 재생 중 Shot이 바뀌지 않도록 잠근다.
- 타임라인 슬라이더 값을 실제 프레임 위치로 바꾼다.
- Sequencer의 카메라/조명 트랙을 Mute 또는 Unmute한다.
- VCam 출력과 Operator UI 표시를 토글한다.
- Review Pad에서 받은 드로잉 문자열을 점 배열로 해석한다.

### `WBP_VP_Operator`

운영자 화면이다. 버튼을 보여 주는 데서 끝나지 않고, 현재 Shot의 카메라 수치와 조명 값을 읽어 Decision Monitor에 표시한다.

### `E_VPShot`

현재 Shot이 무엇인지 표현하는 Enum이다. 복사본의 실제 값 배치를 보면 다음처럼 사용된다.

| 내부 Enum 값 | 의미 |
|---|---|
| `NewEnumerator0` | 선택 없음 또는 초기 상태 |
| `NewEnumerator1` | SH010 |
| `NewEnumerator2` | SH020 |
| `NewEnumerator3` | SH030 |

에디터에서 보이는 이름은 SH010·SH020·SH030이지만, 복사 텍스트에는 내부 이름인 `NewEnumerator1` 형식으로 나타난다.

### `E_VPSessionState`

세션 상태를 UI에 전달하는 Enum이다.

| 내부 Enum 값 | 의미 |
|---|---|
| `NewEnumerator0` | READY |
| `NewEnumerator1` | PLAYING |
| `NewEnumerator2` | PAUSED |
| 추가 값 | ERROR |

정상 흐름의 중심은 READY → PLAYING → PAUSED이며, Enum 자체에는 ERROR도 있다.

### `ST_VPShotData`

Shot 하나에 필요한 정보를 한 묶음으로 저장하는 구조체다. 핵심적으로 Shot Enum, Level Sequence, Sequence Player, Camera 참조를 한 항목으로 묶어 `ShotList`에서 관리한다.

### 핵심 변수

| 변수 | 역할 | 이해 포인트 |
|---|---|---|
| `ShotList` | Shot 데이터 배열 | 시작할 때 각 Shot의 Player를 만들어 채운다. |
| `CurrentShot` | 현재 선택 Shot | 선택 버튼과 재생 대상의 기준이다. |
| `ActiveSequencePlayer` | 현재 조작할 Player | Play, Pause, Scrub이 모두 이 참조를 사용한다. |
| `bShotLocked` | Shot 변경 잠금 | 재생 중 다른 Shot으로 바뀌는 사고를 막는다. |
| `bIsPlaying` | 재생 상태 기록 | UI나 외부 로직이 상태를 알 수 있게 한다. |
| `bISPaused` | 일시정지 상태 기록 | READY와 PAUSED를 구분할 때 중요하다. |
| `OperatorWidgetRef` | 운영자 UI 참조 | 상태 텍스트와 Shot 버튼 표시를 갱신한다. |
| `RC_SeekValue` | 외부에서 들어온 슬라이더 값 | 0~1의 정규화 값이다. |
| `LastRCSeekValue` | 직전 슬라이더 값 | 작은 변화에도 매 Tick Seek하는 것을 줄인다. |
| `ReviewDrawingData` | Review Pad 원문 문자열 | 파싱 전의 전송 데이터를 보관한다. |
| `ReviewStrokes` | 해석된 선 목록 | 각 선은 여러 개의 2D 점으로 구성된다. |

---

## 3. 전체 실행 사이클 미리 보기

```text
WBP Construct
  → Controller 참조 확보
  → OnShotFinished 이벤트 연결
  → Decision Monitor 0.1초 Timer 시작

Shot 버튼 클릭
  → Controller에서 CurrentShot 변경
  → WBP에서 버튼 색과 Shot 문구 변경

Play 클릭
  → Shot 선택 여부 검사
  → Shot 버튼 비활성화
  → Controller.PlayShots
  → ActiveSequencePlayer 재생
  → bShotLocked = true

Tick
  → Player 시간을 Slider에 표시
  → 실제 재생 상태 문구 표시

사용자 Slider 조작
  → Slider_PlayTime
  → 정규화 값을 프레임으로 변환
  → 해당 위치로 Jump하고 Pause

Shot 재생 종료
  → Controller.ShotFinished
  → OnShotFinished Broadcast
  → WBP.UI_ShotFinished
  → Shot 버튼 재활성화

Reset 클릭
  → Controller 상태와 Player 초기화
  → WBP.ResetUI
  → 선택 강조 제거, Slider 0
```

이제 UI 입력에서 Controller, Sequence Player, 상태 표시, Decision Monitor까지의 핵심 경로는 모두 연결해서 설명할 수 있다.

---

## 4. 프로그램 시작 1 — Controller `BeginPlay`

### 쉽게 말하면

게임이 시작되자마자 **운영자 화면을 만들고, 각 Shot을 실제로 재생할 플레이어를 미리 준비하는 단계**다.

### 실제 실행 흐름

```text
BeginPlay
  → WBP_VP_Operator 생성
  → OperatorWidgetRef에 저장
  → Add to Viewport
  → 입력 모드를 Game and UI로 설정
  → 마우스 커서 표시
  → ShotList를 ForEach
       → Shot의 Level Sequence로 Player 생성
       → Player의 Finished 이벤트에 ShotFinished 연결
       → 생성된 Player를 ShotList 항목에 다시 저장
  → VCam 출력 및 Operator UI 초기 토글
```

### 왜 Player를 미리 만드는가

Play 버튼을 누를 때마다 Player를 새로 만들면 생성·이벤트 연결·참조 저장을 반복해야 한다. 이 프로젝트는 시작 시 미리 만들어 두고, 이후에는 `ActiveSequencePlayer`가 필요한 Player를 가리키게 한다.

### `Array Set`이 필요한 이유

`ForEach`에서 꺼낸 Shot 구조체는 그대로 수정했다고 해서 원래 배열 항목이 자동으로 바뀐다고 생각하면 안 된다. 생성한 Sequence Player를 구조체에 넣은 뒤 `Array Set`으로 `ShotList`의 해당 인덱스에 다시 써야 실제 데이터가 보존된다.

### Finished 이벤트 바인딩의 의미

각 Player의 재생이 끝났을 때 공통 `ShotFinished`가 불린다. 덕분에 SH010, SH020, SH030마다 별도의 종료 이벤트를 만들지 않아도 된다.

---

## 5. 프로그램 시작 2 — WBP 생성과 이벤트 연결

Operator UI에서는 Operator UI가 Controller에 어떻게 연결되는지 확인되었다.

### 위젯 생성 시 초기 연결 — `Construct`

```text
Construct
  → GetActorOfClass(BP_VPSessionController)
  → VP_Session_ControllerRef에 저장
  → Controller의 OnShotFinished에 UI_ShotFinished 바인딩
  → 0.1초 반복 Timer 생성
       → RefreshDecisionMonitor
       → UpdateDecisionMonitor
```

#### `GetActorOfClass`를 쓰는 이유

WBP는 생성될 때 Controller를 직접 넘겨받지 않고, 현재 World에서 `BP_VPSessionController` Actor를 찾아 참조로 저장한다. 이후 모든 버튼은 이 `VP_Session_ControllerRef`를 통해 Controller 함수를 호출한다.

#### Event Dispatcher 연결

Controller의 `OnShotFinished`에 UI의 `UI_ShotFinished`를 등록한다.

```text
Controller: Shot 종료 감지
  → OnShotFinished Broadcast
  → WBP의 UI_ShotFinished 실행
  → Shot 버튼들을 다시 활성화
```

이 구조의 장점은 Controller가 SH010 버튼, SH020 버튼 같은 UI 세부 요소를 알 필요가 없다는 것이다. Controller는 “Shot이 끝났다”는 사실만 알리고, UI가 자기 버튼을 처리한다.

#### Decision Monitor 갱신 주기

`K2_SetTimerDelegate`의 값은 다음과 같다.

- Time: `0.1초`
- Looping: `true`
- 실행 이벤트: `RefreshDecisionMonitor`

따라서 Decision Monitor는 매 프레임이 아니라 초당 약 10번 `UpdateDecisionMonitor`를 실행한다. 카메라·조명 수치 표시에는 충분히 자연스럽고, Tick마다 무조건 읽는 것보다 호출 횟수가 적다.

### `OnInitialized`에서 Shot 버튼 배열 준비

WBP의 `OnInitialized`에서는 `BTN_SH010`, `BTN_SH020`, `BTN_SH030`을 배열로 만들어 `ShotButtons`에 저장한다. 덕분에 `UpdateShotButtonVisual`이 세 버튼을 개별적으로 초기화하지 않고 ForEach 하나로 처리할 수 있다.

---

## 6. Shot 선택과 버튼 표시

### 쉽게 말하면

버튼을 누르면 `CurrentShot`을 바꾸고 UI의 선택 표시를 갱신한다. 단, 재생 중에는 `bShotLocked` 때문에 선택을 바꾸지 못하게 한다.

```text
Shot 버튼 클릭
  → bShotLocked 확인
  → 잠겨 있지 않으면 CurrentShot 변경
  → WBP_VP_Operator.UpdateShotButtonVisual
```

### 함수별 결과

| 함수 | `CurrentShot`에 저장되는 값 |
|---|---|
| `SelectShots010` | SH010 (`NewEnumerator1`) |
| `SelectShot020` | SH020 (`NewEnumerator2`) |
| `SelectShot030` | SH030 (`NewEnumerator3`) |

### 왜 Shot Lock이 필요한가

SH010이 재생 중인데 사용자가 SH020을 누르면, 화면상 선택 Shot과 실제 재생 Player가 달라질 수 있다. 그러면 Pause나 Scrub이 어느 Shot에 적용되어야 하는지 모호해진다. `bShotLocked`는 이 불일치를 막는 안전장치다.

### UI Shot 버튼에서 Controller까지

| UI 이벤트 | 호출 함수 | 다음 처리 |
|---|---|---|
| `BTN_SH010.OnClicked` | `SelectShots010` | `UpdateShotButtonVisual` |
| `BTN_SH020.OnClicked` | `SelectShot020` | `UpdateShotButtonVisual` |
| `BTN_SH030.OnClicked` | `SelectShot030` | `UpdateShotButtonVisual` |

즉 버튼 자신이 Shot 데이터를 바꾸는 것이 아니라 Controller에 선택을 요청하고, 완료 후 UI 표시를 갱신한다.

### 선택 표시 함수 — `UpdateShotButtonVisual`

### 실제 실행 흐름

```text
UpdateShotButtonVisual(Selected Shot)
  → ShotButtons 배열을 ForEach
       → 모든 버튼 BackgroundColor를 기본 어두운 색으로 설정
  → Selected Shot을 Switch
       None  → "VISUAL NONE" 디버그 출력
       SH010 → BTN_SH010을 선택 색으로 변경
               Text_Seq_State = "STATE : SHOT010 선택"
       SH020 → BTN_SH020을 선택 색으로 변경
               Text_Seq_State = "STATE : SHOT020 선택"
       SH030 → BTN_SH030을 선택 색으로 변경
               Text_Seq_State = "STATE : SHOT030 선택"
```

### 색상

- 선택 색: 대략 `R 0.184, G 0.659, B 1.0`의 밝은 파란색
- 기본 색: 대략 `R 0.165, G 0.180, B 0.200`의 어두운 회색

### 왜 먼저 전부 기본 색으로 바꾸는가

이전 선택 버튼에 파란색이 남지 않게 하기 위해서다. 먼저 모든 버튼을 초기화하고, 새로 선택된 버튼 하나만 강조하는 전형적인 단일 선택 UI 방식이다.

---

## 7. 현재 Shot을 Sequence로 변환 — `GetCurrentShotSequence`

이 함수는 `CurrentShot`을 보고 대응되는 Level Sequence를 반환한다.

```text
CurrentShot == SH010 → Shot010Sequence
CurrentShot == SH020 → Shot020Sequence
CurrentShot == SH030 → Shot030Sequence
```

이 함수가 중요한 이유는 카메라 렌즈 트랙이나 조명 트랙을 조절할 때, **어느 Sequence를 수정해야 하는지**를 한곳에서 결정하기 때문이다.

발표에서는 “현재 Shot Enum을 실제 Level Sequence 에셋으로 변환하는 매핑 함수”라고 설명하면 된다.

---

## 8. 재생 시작 사이클 — `PlayShots`

### 쉽게 말하면

`CurrentShot`과 일치하는 Player를 찾아 `ActiveSequencePlayer`로 지정한 다음, 처음부터 재생하고 Shot 선택을 잠근다.

### 실제 실행 흐름

```text
PlayShots
  → ShotList 순회
  → CurrentShot과 같은 항목 찾기
  → 해당 Sequence Player를 ActiveSequencePlayer로 저장
  → 필요하면 기존 위치/재생 상태 정리
  → Playback Position을 0으로 설정
  → bShotLocked = true
  → Play
  → bIsPlaying = true
  → bISPaused = false
  → WBP 상태 = PLAYING
  → 상태 텍스트 갱신
```

### 변수 변화만 외우면

| 시점 | `bShotLocked` | `bIsPlaying` | `bISPaused` | UI 상태 |
|---|---:|---:|---:|---|
| Play 직전 | false | false | false | READY |
| Play 직후 | true | true | false | PLAYING |

### 중요한 설계 포인트

`ActiveSequencePlayer`는 “현재 선택된 Shot”과 “현재 실제로 조작 중인 재생기”를 분리해 준다. Pause, Resume, Scrub은 Shot 배열을 다시 찾지 않고 이 참조 하나만 사용한다.

### UI Play 버튼의 사전 검사

```text
Button_Play.OnClicked
  → CurrentShot이 None인지 검사
       None이면 실행하지 않음
       Shot이 선택되어 있으면
         → SH010·SH020·SH030 버튼 비활성화
         → Controller.PlayShots
```

Controller의 `bShotLocked`가 내부 데이터 변경을 막는 안전장치라면, UI 버튼 비활성화는 사용자가 잘못 누르지 않도록 하는 시각적·조작적 안전장치다. 같은 문제를 Controller와 UI 두 층에서 막는다.

---

## 9. 일시정지와 재개 — `PauseAndResume`

### 실제 흐름

```text
PauseAndResume
  → ActiveSequencePlayer가 유효한지 확인
  → bISPaused가 true인가?
       true  → Play → bIsPlaying=true,  bISPaused=false
       false → Pause → bIsPlaying=false, bISPaused=true
```

같은 버튼이 Pause와 Resume 두 역할을 한다. 핵심 기준은 `bISPaused`다.

### 주의할 점

`bIsPlaying`과 `bISPaused`는 서로 반대처럼 보이지만 완전히 같은 정보는 아니다. Player가 없거나 재생이 끝난 READY 상태도 `bIsPlaying=false`이기 때문이다. 그래서 PAUSED를 정확히 표현하려면 별도 `bISPaused`가 필요하다.

### UI Pause/Resume 버튼 갱신

```text
BTN_PauseResume.OnClicked
  → Controller.PauseAndResume
  → Controller의 bISPaused 확인
  → 버튼 문구와 상태 문구 갱신
```

복사본에서는 Pause/Resume 실행 후 `bISPaused`에 따라 `TXT_PauseResume`과 상태 Text를 나누어 갱신한다. Controller가 실제 상태를 바꾸고, WBP가 그 결과에 맞춰 버튼 문구를 바꾸는 구조다.

---

## 10. Slider와 Scrub 사이클 — `SetPlaybackNormalized`

### 쉽게 말하면

iPad 슬라이더의 `0~1` 값을 Sequence 전체 길이의 실제 프레임 번호로 바꾸어 재생 위치를 옮긴다.

### 실제 계산

```text
SeekValue
  → Clamp(0, 1)
  → Sequence 전체 Frame Duration과 곱하기
  → 가장 가까운 정수 Frame으로 반올림
  → FrameNumber → FrameTime → PlaybackParams
  → SetPlaybackPosition(Jump)
```

예를 들어 Sequence가 1,000프레임이고 슬라이더 값이 0.25라면 약 250프레임으로 이동한다.

### 실행 순서

```text
SetPlaybackNormalized(SeekValue)
  → ActiveSequencePlayer 유효성 확인
     ├─ 유효하지 않음: "Active Player NONE" 출력
     └─ 유효함
          → Pause
          → 0~1 Clamp
          → 실제 프레임 계산
          → SetPlaybackPosition, UpdateMethod=Jump
          → WBP 상태를 PAUSED로 설정
```

### 왜 먼저 Pause하는가

재생 중 위치를 계속 바꾸면 Play 진행과 사용자의 Seek가 동시에 위치를 갱신한다. 먼저 Pause하면 사용자가 고른 프레임을 안정적으로 보여 줄 수 있다.

### 왜 Clamp가 필요한가

네트워크나 UI에서 -0.1 또는 1.2 같은 값이 들어와도 Sequence 범위 밖으로 나가지 않게 한다.

### `Jump`의 의미

중간 구간을 시간 순서대로 재생해 지나가는 것이 아니라 목표 위치로 바로 이동한다. Review용 Scrub에 맞는 방식이다.

### Tick과의 연결

Event Tick은 `RC_SeekValue`와 `LastRCSeekValue`의 차이를 검사한다. 변화가 의미 있게 클 때만 `SetPlaybackNormalized`를 부르고, 처리 후 현재 값을 `LastRCSeekValue`에 저장한다.

이 비교가 없으면 값이 그대로여도 매 프레임 Seek가 실행되어 불필요한 연산과 떨림이 생길 수 있다.

### WBP Tick과 `bUpdatingSlider`

```text
Slider_Seq_Timer.OnValueChanged(Value)
  → bUpdatingSlider가 false일 때만
  → Controller.Slider_PlayTime(Value)
```

Tick에서는 반대로 Player의 현재 시간을 읽어 Slider 위치를 자동 갱신한다.

```text
Tick
  → ActiveSequencePlayer가 유효한지 확인
  → CurrentTime / Duration 계산
  → bUpdatingSlider = true
  → Slider.SetValue(정규화된 현재 위치)
  → bUpdatingSlider = false
  → GetSessionStateText
  → Text_Seq_State.SetText
```

#### 왜 `bUpdatingSlider`가 필요한가

코드가 `Slider.SetValue`를 호출해도 `OnValueChanged`가 발생할 수 있다. 방지 장치가 없으면 다음 순환이 생긴다.

```text
Tick이 Slider 값 설정
  → OnValueChanged 발생
  → Controller Seek
  → 다음 Tick에서 다시 Slider 값 설정
  → 반복
```

`bUpdatingSlider=true`인 동안에는 사용자가 움직인 것이 아니라 코드가 표시를 맞추는 중이라고 판단해 Seek 호출을 차단한다. 사용자가 직접 움직일 때만 `Slider_PlayTime`이 실행된다.

---

## 11. 재생 종료와 UI 복구 — `ShotFinished`

각 Sequence Player의 Finished 이벤트에 연결된 공통 종료 처리다.

```text
Sequence 재생 완료
  → ShotFinished
  → bIsPlaying = false
  → bShotLocked = false
  → 종료 알림/후속 갱신
```

가장 중요한 역할은 `bShotLocked=false`다. 이 처리가 빠지면 한 번 재생한 뒤 다른 Shot을 선택하지 못하는 문제가 생긴다.

---

## 12. 전체 초기화 — `ResetSession`과 `ResetUI`

### 쉽게 말하면

현재 재생을 멈추는 것뿐 아니라, Player 참조·Shot 선택·재생 플래그·UI를 시작 상태로 돌린다.

```text
ResetSession
  → ActiveSequencePlayer가 유효하면 Stop
  → bIsPlaying = false
  → bISPaused = false
  → ActiveSequencePlayer = None
  → CurrentShot = 선택 없음(NewEnumerator0)
  → UI Reset
```

### Stop과 Reset의 차이

- Stop: 현재 Player만 멈춘다.
- ResetSession: 멈춤에 더해 프로젝트가 기억하던 선택과 상태도 초기화한다.

따라서 시연 중 문제가 생겼을 때 안전하게 처음부터 다시 시작하려면 Stop이 아니라 ResetSession을 사용해야 한다.

### UI Reset 버튼

```text
Button_Reset.OnClicked
  → Controller.ResetSession
  → Controller 내부에서 WBP.ResetUI 호출
```

Reset 버튼의 Event Graph는 단순하다. 실제 초기화 책임은 Controller와 `ResetUI`에 모여 있다.

### 실제 UI 초기화 — `ResetUI`

`ResetSession`에서 호출하는 실제 UI 초기화 함수다.

```text
ResetUI
  → UpdateShotButtonVisual(Selected Shot = None)
  → Slider_Seq_Timer.Value = 0.0
```

### 결과

- 모든 Shot 버튼의 선택 강조가 사라진다.
- 선택된 Shot이 없는 상태로 표시된다.
- 재생 위치 Slider가 맨 앞으로 돌아간다.

Controller의 `ResetSession`이 데이터와 Player를 초기화하고, WBP의 `ResetUI`가 눈에 보이는 상태를 초기화한다. 두 함수의 역할을 구분해서 이해하면 된다.

---

## 13. 상태 표시 — `GetSessionStateText`

이 함수는 단순히 상태 Enum 하나를 읽지 않고, 실제 Player와 `bISPaused`를 함께 확인한다.

```text
ActiveSequencePlayer가 없음
  → "STATE : READY"

ActiveSequencePlayer가 있음
  → bISPaused == true
       → "STATE : PAUSED"
  → 아니고 Player.IsPlaying() == true
       → "STATE : PLAYING"
  → 둘 다 아님
       → "STATE : READY"
```

### 핵심 이해

화면의 상태 문구는 “저장해 둔 Enum만 믿는 표시”가 아니라 **실제 Player가 존재하는지와 재생 중인지까지 확인한 결과**다. 그래서 UI가 실제 재생 상태를 더 잘 반영한다.

---

## 14. Decision Monitor — `UpdateDecisionMonitor`

### 쉽게 말하면

현재 Shot에 연결된 카메라와 Directional Light에서 실제 값을 읽어 UI Text에 적는 함수다.

### Shot별 처리

`CurrentShot`을 Switch로 나누고, SH010·SH020·SH030 각각의 `ST_VPShotData.Camera`를 가져온다. 그 카메라의 `CineCameraComponent`에서 다음 값을 읽는다.

- `CurrentFocalLength`: 현재 초점거리, mm
- `CurrentAperture`: 현재 조리개 값, f-stop
- `CurrentFocusDistance`: 현재 초점 거리

그리고 공통 `DirectionalLightRef`의 Light Component에서 `Intensity`를 읽는다.

```text
CurrentShot Switch
  → 해당 ShotData의 Camera
  → CineCameraComponent
  → Focal Length / Aperture / Focus Distance 읽기
  → 문자열로 변환
  → 대응 TextBlock에 SetText

DirectionalLightRef
  → LightComponent
  → Intensity 읽기
  → 조명 TextBlock에 SetText
```

### 이 함수가 증명하는 것

Decision Monitor는 단순히 사용자가 입력한 숫자를 다시 보여 주는 화면이 아니다. **현재 Unreal 오브젝트에 적용된 실제 값을 다시 읽어서 표시하는 검증 화면**이다.

### 현재 구조의 개선점

Shot 세 개에 대해 카메라 값을 읽고 Text를 갱신하는 로직이 반복된다. `GetCurrentShotData` 또는 `GetCurrentCamera` 같은 공통 함수를 만들면 같은 노드를 한 번만 유지할 수 있다.

---

## 15. Sequencer Property 제어 — `SetTrackMute`

`SetTrackMute`의 실제 연결이 확인되었다. 이 함수가 Aperture와 Directional Light Mute의 공통 핵심이다.

### 입력값

| 입력 | 의미 | 예시 |
|---|---|---|
| `BindingName` | Sequencer에서 대상 오브젝트/컴포넌트의 Binding 이름 | `CameraComponent`, `LightComponent0` |
| `TrackName` | 대상 Binding 안에서 찾을 Track 표시 이름 | `Current Aperture`, `Intensity` |
| `bMute` | 끌 것인지 여부 | true면 Mute, false면 Unmute |

### 실제 실행 흐름

```text
SetTrackMute(BindingName, TrackName, bMute)
  → GetCurrentShotSequence
  → FindBindingByName(BindingName)
  → GetTracks
  → 모든 Track을 ForEach
       → GetDisplayName
       → 입력 TrackName과 같은지 비교
       → 일치하면 GetSections
       → 모든 Section을 ForEach
            → SetIsActive(NOT bMute)
```

### 단계별로 쉽게 설명하면

1. 현재 SH010·SH020·SH030 중 어느 Sequence를 보고 있는지 찾는다.
2. 그 Sequence에서 `CameraComponent` 또는 `LightComponent0` 같은 대상을 찾는다.
3. 대상 아래의 모든 Track을 꺼낸다.
4. Track 표시 이름이 원하는 속성 이름과 같은지 검사한다.
5. 일치하는 Track의 모든 Section을 가져온다.
6. `bMute`의 반대값을 `SetIsActive`에 넣는다.

### 왜 `NOT bMute`인가

사용자가 `bMute=true`를 보냈다는 것은 “이 Section을 꺼 달라”는 뜻이다. 하지만 `SetIsActive`는 “활성화할 것인가”를 묻는다. 두 함수의 질문 방향이 반대이므로 `NOT`을 거친다.

```text
bMute = true  → NOT = false → Section 비활성
bMute = false → NOT = true  → Section 활성
```

### 이 함수가 프로젝트에서 중요한 이유

카메라 초점거리, 조리개, 초점거리와 조명 Intensity마다 같은 검색 로직을 복사하지 않아도 된다. 속성별 함수는 Binding과 Track 이름만 미리 채워서 `SetTrackMute`를 호출하면 된다.

### 아직 보완할 수 있는 안전장치

현재 구조에서 다음 결과가 유효한지 검사하면 더 안전하다.

- `GetCurrentShotSequence`가 None인지
- `FindBindingByName`이 유효한 Binding을 찾았는지
- 이름이 일치하는 Track이 실제로 존재하는지
- Section 배열이 비어 있지 않은지

### 카메라 렌즈 Track — `SetCameraLensMute`

### 무엇을 하는가

현재 Shot Sequence에서 카메라 Component Binding을 찾고, 전달받은 이름과 같은 Track을 찾은 뒤 그 Track의 모든 Section을 활성/비활성화한다.

```text
GetCurrentShotSequence
  → FindBindingByName
  → Binding의 GetTracks
  → 모든 Track 순회
  → GetDisplayName == 입력 TrackName?
  → 일치하면 GetSections
  → 모든 Section에 SetIsActive(!bMute)
```

### `!bMute`가 중요한 이유

| `bMute` | `!bMute` | Section 결과 |
|---:|---:|---|
| false | true | 활성, Sequencer가 값을 소유 |
| true | false | 비활성, 외부 조절값이 반영될 여지가 생김 |

즉 Mute는 Track이나 Key를 삭제하는 것이 아니다. Section을 잠시 비활성화했다가 다시 켤 수 있는 **가역적 제어**다.

### 입력 `TrackName`

이 함수는 Track 이름을 입력으로 받으므로 Focal Length, Aperture, Focus Distance 같은 여러 렌즈 속성에 재사용할 수 있다.

### 이름 기반 검색의 위험

`FindBindingByName`과 `GetDisplayName` 비교를 사용하므로 Sequencer에서 Binding이나 Track 표시 이름이 바뀌면 검색에 실패할 수 있다. 발표에서 “빠르게 재사용 가능한 대신 이름 규칙에 의존한다”고 말하면 정확하다.

### Aperture 전용 래퍼 — `SetApertureMute`

이 함수는 복잡한 로직을 다시 만들지 않고 공통 `SetTrackMute`를 호출한다.

```text
SetApertureMute(bMute)
  → SetTrackMute(
       BindingName = "CameraComponent",
       TrackName   = "Current Aperture",
       bMute       = 입력값
     )
```

쉽게 말하면 `SetTrackMute`가 범용 엔진이고, `SetApertureMute`는 iPad에서 쓰기 편하도록 대상 이름을 미리 채워 둔 바로가기 함수다.

### Directional Light — `SetDirectionalLightMute`

실제로 Function Entry에서 연결된 실행선은 다음과 같다.

```text
SetDirectionalLightMute(bMute)
  → SetTrackMute(
       BindingName = "LightComponent0",
       TrackName   = "Intensity",
       bMute       = 입력값
     )
```

### 복사본에서 발견된 정리 포인트

이 함수 그래프에는 `GetCurrentShotSequence → FindBindingByName → GetTracks → GetSections → SetIsActive`를 직접 수행하는 긴 노드 묶음도 남아 있다. 하지만 Function Entry의 실행선은 그 묶음이 아니라 `SetTrackMute` 호출로 연결되어 있다.

따라서 현재 실제 실행 경로는 **공통 함수 `SetTrackMute`를 호출하는 짧은 경로**이고, 긴 묶음은 이전 구현 또는 참고용으로 남은 미연결 노드로 보인다.

이것은 발표보다 유지보수 때 더 중요한 포인트다. 미연결 노드는 실제 기능으로 오해하기 쉬우므로 정리하거나 “Legacy/Reference” 코멘트로 분리하는 편이 좋다.

---

## 16. VCam과 Operator UI 표시 제어

### VCam Output — `ToggleVCamActive`

### 실제 흐름

```text
VCamActorRef
  → GetVCamComponent
  → GetOutputProviderByIndex(0)
  → IsActive
       true  → SetActive(false)
       false → SetActive(true)
  → ToggleOperatorUIVisibility
```

현재는 Output Provider가 하나라는 전제로 인덱스 0을 사용한다.

### 이해 포인트

이 함수는 VCam 출력 상태만 바꾸는 것이 아니라 Operator UI 표시도 함께 토글한다. 즉 VCam 모드 전환과 화면 정리를 하나의 사용자 동작으로 묶었다.

### 개선할 때 볼 부분

Output Provider가 여러 개로 늘어나면 고정 인덱스 0 대신 Provider 타입이나 이름으로 찾는 방식이 더 안전하다. 또한 VCam과 UI를 항상 함께 바꿔야 하는지 정책을 분리하면 재사용성이 좋아진다.

### Operator UI 표시 — `ToggleOperatorUIVisibility`

```text
OperatorWidgetRef.IsVisible
  → true  : SetVisibility(Hidden)
  → false : SetVisibility(Visible)
```

여기서는 `Collapsed`가 아니라 `Hidden`을 사용한다. Hidden은 화면에 보이지 않지만 레이아웃 공간은 유지한다. 전체 화면 Overlay라면 차이가 눈에 띄지 않을 수 있지만, 다른 위젯과 함께 배치할 때는 의미가 있다.

---

## 17. Review Drawing 수신과 파싱 — `ReceiveReviewDrawing`

### 전송 문자열 형식

복사본의 파싱 구조를 보면 다음 형식이다.

```text
x,y;x,y;x,y|x,y;x,y
```

- `|`: Stroke와 Stroke를 구분
- `;`: 한 Stroke 안에서 Point를 구분
- `,`: 한 Point의 X와 Y를 구분

예시:

```text
0.10,0.20;0.15,0.24;0.21,0.30|0.60,0.40;0.65,0.48
```

이 문자열은 2개의 Stroke를 뜻한다. 첫 번째 Stroke에는 점 3개, 두 번째 Stroke에는 점 2개가 있다.

### 실제 파싱 순서

```text
ReceiveReviewDrawing(Data)
  → ReviewDrawingData에 원문 저장
  → ReviewStrokes Clear
  → Data를 "|"로 나누기
  → 각 Stroke 문자열마다
       → CurrentStrokePoints Clear
       → Stroke를 ";"로 나누기
       → 각 Point 문자열마다
            → "," 기준으로 X/Y 분리
            → String을 Double로 변환
            → Make Vector2D(X, Y)
            → CurrentStrokePoints에 Add
       → ST_ReviewStroke 생성
       → ReviewStrokes에 Add
```

### 중요한 동작

함수 시작에서 `ReviewStrokes`를 먼저 비운다. 따라서 SEND할 때 새 그림을 기존 그림 뒤에 추가하는 구조가 아니라, **현재 그림 전체를 새 데이터로 교체하는 구조**다.

### 데이터 예외 처리에서 개선할 점

현재 구조는 문자열 형식이 정상이라는 가정이 강하다. 다음 검증을 추가하면 더 안전하다.

- 빈 Stroke/Point 무시
- 쉼표로 나눈 결과가 정확히 2개인지 확인
- 문자열이 숫자로 변환 가능한지 확인
- 지나치게 많은 Point가 들어오는 경우 최대 개수 제한

---

## 18. Review Drawing 렌더링 — `OnPaint`

Review Pad의 좌표가 실제 WBP 화면에 그려지는 과정까지 확인되었다.

### 전체 Review 파이프라인

```text
Review Pad에서 그림 작성
  → "x,y;x,y|x,y..." 문자열 전송
  → ReceiveReviewDrawing
  → ReviewStrokes 배열로 변환
  → WBP OnPaint
  → 정규화 좌표를 현재 Widget 픽셀 좌표로 변환
  → DrawLines
  → 화면에 선 표시
```

### `OnPaint`에서 사용하는 지역 변수

| 지역 변수 | 역할 |
|---|---|
| `DrawPoints` | 현재 Stroke를 그리기 위해 만든 화면 좌표 배열 |
| `PreviousPoint` | 현재 Point와 연결할 직전 Point |

### 실제 실행 흐름

```text
OnPaint(Context)
  → Controller의 ReviewStrokes를 ForEach
       → 현재 ST_ReviewStroke에서 Points 배열 추출
       → DrawPoints Clear
       → Points를 ForEach
            → GetCachedGeometry
            → GetLocalSize
            → 정규화 Point의 X/Y 분리
            → LocalSize의 X/Y 분리
            → ScreenX = PointX × LocalWidth
            → ScreenY = PointY × LocalHeight
            → MakeVector2D(ScreenX, ScreenY)
            → 첫 번째 Point인가?
                 예: PreviousPoint = 현재 Point
                 아니오:
                   → DrawPoints에 PreviousPoint 추가
                   → DrawPoints에 현재 Point 추가
                   → PreviousPoint = 현재 Point
       → 한 Stroke 처리가 끝나면 DrawLines(Context, DrawPoints)
  → 모든 Stroke 처리 후 OnPaint 종료
```

### 왜 좌표에 Widget 크기를 곱하는가

Review Pad가 보내는 좌표는 픽셀값이 아니라 0~1 범위의 정규화 좌표다.

예를 들어 좌표가 `(0.5, 0.25)`이고 현재 Widget 크기가 `1920 × 1080`이면 다음처럼 계산된다.

```text
ScreenX = 0.5  × 1920 = 960
ScreenY = 0.25 × 1080 = 270
```

따라서 iPad와 PC의 화면 해상도가 달라도 그림의 상대적인 위치를 유지할 수 있다.

### `PreviousPoint`를 사용하는 이유

점 하나만으로는 선을 만들 수 없다. 직전 점과 현재 점을 한 쌍으로 만들어야 선분이 된다.

```text
P0, P1, P2, P3가 들어오면

P0 → P1
P1 → P2
P2 → P3
```

현재 BP는 이를 위해 `DrawPoints`에 다음과 같이 이전 점과 현재 점을 차례로 추가한다.

```text
[P0, P1, P1, P2, P2, P3]
```

### 실제 Draw 설정

| 항목 | 값 |
|---|---|
| 그리기 함수 | `WidgetBlueprintLibrary.DrawLines` |
| 두께 | `3.0` |
| Anti Alias | `true` |
| 색상 | 노랑·연두 계열 `(R 0.776, G 0.888, B 0.0, A 1.0)` |
| 좌표 기준 | WBP의 Cached Geometry Local Size |

### Clear 버튼과 연결

```text
BTN_ClearReview
  → ClearReviewDrawing
  → ReviewStrokes / CurrentStrokePoints Clear
  → InvalidateLayoutAndVolatility
  → OnPaint가 다시 실행될 때 그릴 Stroke가 없음
  → 화면에서 선이 사라짐
```

### 이 설계의 좋은 점

- 전송 데이터는 해상도와 무관한 정규화 좌표다.
- Stroke를 분리해 서로 떨어진 선들이 연결되지 않는다.
- UMG의 표준 `OnPaint`와 `DrawLines`를 사용한다.
- 그림을 Actor나 Texture로 별도 생성하지 않아 구조가 단순하다.

### 개선할 수 있는 부분

현재 `DrawPoints`에는 각 선분의 끝점이 중복으로 들어간다. 화면에는 문제없이 이어진 선으로 보이지만, `DrawLines`가 연속 Point 배열을 받는다는 전제에서는 각 변환 Point를 한 번씩만 추가하는 방식으로 단순화할 수 있다.

또한 다음을 추가하면 더 안전하다.

- Point가 2개 미만인 Stroke는 DrawLines 생략
- 0~1 범위를 벗어난 좌표 Clamp
- Stroke/Point 최대 개수 제한
- 색상과 두께를 변수로 빼서 UI 또는 Remote Control에서 조절

### 발표에서 한 문장으로 설명하기

> Review Pad에서 받은 좌표는 해상도와 무관한 0부터 1 사이의 값으로 저장하고, WBP의 OnPaint에서 현재 화면 크기를 곱해 실제 픽셀 좌표로 변환한 뒤 두께 3의 선으로 그립니다.

이로써 Review Pad의 입력, 전송, Unreal 파싱, 데이터 저장, 화면 렌더링, Clear까지 전체 흐름이 모두 확인되었다.

---

## 19. Review Drawing 초기화 — `ClearReviewDrawing`

```text
ClearReviewDrawing
  → ReviewStrokes.Clear
  → CurrentStrokePoints.Clear
```

원본 문자열보다 실제 렌더링에 사용되는 배열 두 개를 비우는 것이 핵심이다. 다음 Draw Tick 또는 Paint에서 읽을 점이 없어져 화면의 그림이 사라진다.

### Review UI 버튼

```text
BTN_ClearReview.OnClicked
  → Controller.ClearReviewDrawing
  → InvalidateLayoutAndVolatility
```

배열을 비운 뒤 `InvalidateLayoutAndVolatility`를 호출해 Widget이 다시 그려지도록 요청한다. 데이터만 지우고 화면 갱신을 기다리는 것이 아니라, UMG에 재계산이 필요하다는 사실도 알린다.

`BTN_TestReview`는 현재 `ReviewStrokes` 개수 등을 Print String으로 확인하는 디버그용 버튼이다. 발표용 핵심 기능이라기보다 Review 데이터가 들어왔는지 검사하기 위한 도구로 보는 것이 맞다.

---

## 20. 처음부터 끝까지 동작 예시

### 예시 A — SH020 선택 후 재생 완료

```text
1. SelectShot020
   CurrentShot = SH020
   Shot 버튼 표시 갱신

2. PlayShots
   ShotList에서 SH020 Player 검색
   ActiveSequencePlayer = SH020 Player
   bShotLocked = true
   Play
   상태 = PLAYING

3. 재생 중 SH030 버튼 클릭
   bShotLocked 때문에 선택 변경 차단

4. SH020 재생 완료
   ShotFinished 호출
   bIsPlaying = false
   bShotLocked = false

5. 이제 SH030 선택 가능
```

### 예시 B — 슬라이더를 70% 위치로 이동

```text
1. RC_SeekValue = 0.70
2. Tick이 이전 값과 차이를 감지
3. SetPlaybackNormalized(0.70)
4. Player Pause
5. 0.70 × 전체 Frame Duration
6. 계산된 프레임으로 Jump
7. UI 상태 = PAUSED
```

### 예시 C — 조리개 값을 iPad에서 직접 조절

```text
1. SetApertureMute(true)
2. CameraComponent의 "Current Aperture" Track Section 비활성
3. Sequencer가 Aperture 값을 덮어쓰지 않음
4. Remote Control에서 값 조절
5. UpdateDecisionMonitor가 실제 CurrentAperture를 다시 읽어 표시
6. SetApertureMute(false)로 Track Section 재활성 가능
```

---

## 21. 설계의 장점과 개선점

### 좋은 점

- 중앙 Controller가 Shot·재생·상태를 한곳에서 관리한다.
- Shot별 Player를 미리 만들고 종료 이벤트를 공통 처리한다.
- `ActiveSequencePlayer` 덕분에 Pause와 Scrub 로직이 단순하다.
- `bShotLocked`로 UI 선택과 실제 재생 대상의 불일치를 막는다.
- 슬라이더 입력을 0~1로 통일해 Sequence 길이가 달라도 같은 UI를 쓸 수 있다.
- Mute를 Section 활성 상태로 처리해 키를 삭제하지 않고 되돌릴 수 있다.
- Decision Monitor가 실제 오브젝트 값을 다시 읽어 입력 성공 여부를 검증한다.
- `SetTrackMute` 같은 공통 함수와 속성별 래퍼를 나누어 Remote Control에서 호출하기 쉽다.

### 개선점

#### 1. 상태의 단일 기준 만들기

현재는 `bIsPlaying`, `bISPaused`, Player의 `IsPlaying`, `E_VPSessionState`가 함께 쓰인다. 어느 하나가 갱신되지 않으면 상태가 어긋날 수 있다. 상태 전환을 `SetSessionState` 같은 함수 하나로 모으면 안전하다.

#### 2. 문자열 이름 의존 줄이기

`CameraComponent`, `LightComponent0`, `Current Aperture`, `Intensity` 같은 이름이 바뀌면 Mute 검색이 실패할 수 있다. 최소한 상수 변수로 모으고, 검색 실패 시 명확한 오류를 출력하는 편이 좋다.

#### 3. 미연결 Legacy 노드 정리

`SetDirectionalLightMute`처럼 실행선에 연결되지 않은 이전 구현이 남아 있으면 실제 경로를 읽기 어렵다. 삭제가 부담되면 별도 함수나 주석 박스로 옮긴다.

#### 4. Decision Monitor 중복 제거

Shot마다 같은 카메라 값 읽기 노드가 반복된다. 현재 Shot의 Camera를 반환하는 공통 함수로 단순화할 수 있다.

#### 5. 참조 유효성 검사 강화

`OperatorWidgetRef`, `VCamActorRef`, Output Provider, Camera, Directional Light, Binding 검색 결과가 비어 있을 때를 모두 처리하면 시연 안정성이 올라간다.

#### 6. Review 문자열 검증

잘못된 좌표 문자열, 빈 값, 과도한 Point 수를 검사해야 외부 입력 때문에 파싱 오류나 성능 문제가 생기는 것을 막을 수 있다.

---

## 22. 발표용 요약, 예상 질문, 공부 순서

### 1분 설명

> 이 프로젝트의 중심은 BP_VPSessionController입니다. 시작할 때 각 Shot의 Level Sequence Player를 미리 만들고, 사용자가 Shot을 고르면 CurrentShot에 저장합니다. Play를 누르면 해당 Player를 ActiveSequencePlayer로 지정해 재생하고, 재생 중에는 bShotLocked로 다른 Shot 선택을 막습니다. Pause와 Scrub은 ActiveSequencePlayer 하나만 조작하며, 슬라이더의 0부터 1 값을 전체 프레임 길이에 곱해 원하는 위치로 이동합니다. 카메라나 조명을 외부에서 조절할 때는 Sequencer Section을 잠시 비활성화해 값 충돌을 막고, Decision Monitor는 실제 카메라와 조명 값을 다시 읽어 적용 여부를 보여 줍니다.

### 예상 질문

**왜 Shot을 재생 중에 바꾸지 못하게 했나요?**  
현재 재생 중인 Player와 UI가 가리키는 Shot이 달라지는 것을 막기 위해서다.

**왜 슬라이더 값이 0~1인가요?**  
Shot마다 길이가 달라도 같은 UI와 Remote Control 입력을 재사용하기 위해서다.

**왜 Scrub하면 PAUSED가 되나요?**  
재생 진행과 Seek가 동시에 위치를 바꾸지 않게 하고, 사용자가 고른 프레임을 안정적으로 검토하기 위해서다.

**Mute하면 키가 삭제되나요?**  
아니다. 해당 Track의 Section을 `SetIsActive(false)`로 잠시 비활성화한다.

**Decision Monitor는 입력값을 그대로 보여 주나요?**  
아니다. CineCameraComponent와 LightComponent에서 현재 실제 값을 다시 읽어 표시한다.

**Review Pad 데이터는 누적되나요?**  
현재 함수는 수신할 때 기존 `ReviewStrokes`를 비우므로 새 그림 전체로 교체된다.

**현재 코드에서 가장 주의할 부분은 무엇인가요?**  
Sequencer Binding/Track을 표시 이름으로 찾는 부분과 여러 상태 변수의 동기화다.

### 공부 순서

시간이 부족하면 다음 순서로 이해하면 된다.

1. `CurrentShot`, `ActiveSequencePlayer`, `bShotLocked`의 관계
2. `BeginPlay → Select Shot → PlayShots → ShotFinished → ResetSession`
3. `SetPlaybackNormalized`의 0~1 값을 프레임으로 바꾸는 계산
4. `SetTrackMute`와 `SetIsActive(!bMute)`의 의미
5. `UpdateDecisionMonitor`가 실제 값을 다시 읽는 이유
6. `ReceiveReviewDrawing`의 `|`, `;`, `,` 3단계 파싱
7. VCam Output Provider와 Operator UI 토글

이 일곱 가지만 이해하면 발표 자료의 핵심 Blueprint 질문 대부분에 대응할 수 있다.


