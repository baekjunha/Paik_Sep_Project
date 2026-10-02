# VPSync Review Pad 재적용 실습 가이드

작성 기준: 2026-09-22

---

## 0. 이 문서는 무엇을 위한 것인가?

이 문서는 다른 PC에서 VPSync 프로젝트를 실행할 때 **iPad Review Pad 기능을 다시 사용할 수 있도록 설정하는 방법**을 정리한 문서다.

Review Pad의 데이터 흐름은 다음과 같다.

```text
iPad Apple Pencil Canvas
↓
Remote Control Web Server :30000
↓
VPSync Proxy
↓
localhost:30010 Remote Control API
↓
RC_VPSyncOperator
↓
Receive Review Drawing
↓
BP_VPSessionController.ReceiveReviewDrawing
↓
DrawingData 전달
```

### WHY — 왜 별도 설정이 필요한가?

Review Pad에 사용되는 HTML과 Proxy 코드는 Unreal 프로젝트 Content 안에만 있는 것이 아니라,

```text
UE_5.7
└─ Engine
   └─ Plugins
      └─ VirtualProduction
         └─ RemoteControlWebInterface
```

내부에도 수정 사항이 있기 때문이다.

따라서 프로젝트를 Git으로 Pull하는 것만으로는 Review Pad가 완전히 복구되지 않는다.

---

# 1. 준비 파일 확인

다음 3개 파일이 준비되어 있는지 확인한다.

```text
vpsync_review_pad.html
App_with_VPSyncProxy.ts
README.txt
```

### WHY — 왜 이 파일들이 필요한가?

- `vpsync_review_pad.html`
  - iPad에서 실제로 그림을 그리는 Review Pad 화면이다.

- `App_with_VPSyncProxy.ts`
  - iPad에서 들어온 요청을 Unreal Remote Control API로 전달하는 Proxy 코드가 들어 있다.

- `README.txt`
  - 현재 보고 있는 복구 절차 문서다.

---

# 2. Unreal Engine WebApp 위치로 이동

다음 경로로 이동한다.

```text
C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\VirtualProduction\RemoteControlWebInterface\WebApp
```

여기에서 이후 작업을 진행한다.

### WHY — 왜 프로젝트 폴더가 아니라 여기인가?

Review Pad는 Unreal의 `RemoteControlWebInterface`가 제공하는 Web Server를 사용한다.

따라서 Review Pad HTML과 Proxy 코드는 프로젝트 Content가 아니라 이 Plugin의 WebApp에 연결해야 한다.

---

# 3. Review Pad HTML 복사

`vpsync_review_pad.html` 파일을 다음 폴더에 복사한다.

```text
WebApp
└─ Server
   └─ public
      └─ vpsync_review_pad.html
```

즉 최종 위치는 다음과 같다.

```text
...\RemoteControlWebInterface\WebApp\Server\public\vpsync_review_pad.html
```

### WHY — 왜 public 폴더인가?

Remote Control Web Server가 브라우저에서 직접 보여주는 정적 웹 파일이 `Server/public`에 있기 때문이다.

이곳에 HTML을 넣어야 PC와 iPad에서

```text
http://PC주소:30000/vpsync_review_pad.html
```

형태로 접속할 수 있다.

---

# 4. VPSync Proxy 코드를 App.ts에 반영

다음 파일을 연다.

```text
WebApp\Server\src\App.ts
```

백업해둔

```text
App_with_VPSyncProxy.ts
```

를 참고해서 VPSync Proxy 코드를 `App.ts`에 반영한다.

핵심 Proxy 주소는 다음과 같다.

```text
/vpsync/review-drawing
```

이 Proxy는 받은 `DrawingData`를 다음 Remote Control API로 전달한다.

```text
127.0.0.1:30010
```

대상은 다음과 같다.

```text
Preset:
RC_VPSyncOperator

Function:
Receive Review Drawing

Argument:
DrawingData
```

### WHY — 왜 Proxy가 필요한가?

iPad는 Remote Control Web Interface의

```text
:30000
```

에는 접속할 수 있었지만,

Unreal Remote Control HTTP API의

```text
:30010
```

에는 직접 접근할 수 없었다.

그래서 흐름을 다음처럼 만들었다.

```text
iPad
↓
PC :30000
↓
VPSync Proxy
↓
PC 내부 localhost:30010
↓
Unreal
```

즉 `:30000`이 iPad와 Unreal 사이의 중간 통로 역할을 한다.

---

# 5. Server 폴더에서 PowerShell 실행

다음 폴더로 이동한다.

```text
WebApp\Server
```

이 폴더에서 PowerShell을 연다.

### WHY — 왜 여기서 실행하는가?

우리가 수정한 `App.ts`는 TypeScript 소스 파일이므로, 수정했다고 바로 Web Server에 반영되는 것이 아니다.

WebApp을 다시 Build해야 수정 내용이 실제 실행 코드에 반영된다.

---

# 6. Unreal에 포함된 Node.js 사용 준비

PowerShell에 다음 명령을 순서대로 입력한다.

```powershell
$nodeDir = (Resolve-Path ..\node-v16.17.0).Path
```

그다음:

```powershell
$env:Path = "$nodeDir;$env:Path"
```

정상적으로 잡혔는지 확인한다.

```powershell
node -v
```

정상이라면 다음과 같이 Node 버전이 출력된다.

```text
v16.17.0
```

### WHY — 왜 Node.js가 필요한가?

`RemoteControlWebInterface`의 Web Server 코드가 Node.js 기반으로 동작하기 때문이다.

또한 별도의 Node를 설치해서 사용하는 대신 Unreal WebApp에 포함된

```text
node-v16.17.0
```

을 사용한다.

---

# 7. WebApp Build

같은 PowerShell에서 다음 명령을 실행한다.

```powershell
npm.cmd run build
```

Build가 끝날 때까지 확인한다.

### WHY — 왜 Build해야 하는가?

우리가 수정한 것은

```text
Server\src\App.ts
```

라는 TypeScript 원본이다.

Web Server가 실제로 사용하는 실행용 파일에 Proxy 수정 사항을 적용하려면 Build 과정이 필요하다.

즉,

```text
App.ts 수정
↓
npm.cmd run build
↓
실행 가능한 Web Server 코드에 반영
```

이라는 흐름이다.

---

# 8. Unreal Editor 재시작

Build가 완료되면 Unreal Editor를 재시작한다.

### WHY — 왜 재시작하는가?

Remote Control Web Interface가 수정 전 Web Server 상태로 실행되고 있을 수 있기 때문이다.

재시작해서 수정된 WebApp을 다시 시작하게 한다.

---

# 9. PC에서 Review Pad 접속 테스트

PC 브라우저에서 다음 주소를 연다.

```text
http://127.0.0.1:30000/vpsync_review_pad.html
```

Review Pad 페이지가 열리는지 확인한다.

### 성공 기준

다음 화면이 나타나야 한다.

```text
VPSync Review Pad

[Canvas]

CLEAR
SEND
```

### WHY — 왜 PC에서 먼저 확인하는가?

iPad까지 바로 테스트하면 문제가

- HTML 문제인지
- Web Server 문제인지
- 네트워크 문제인지

한 번에 섞인다.

먼저 PC 자기 자신에서 `127.0.0.1`로 확인하면 **WebApp 자체가 정상인지 먼저 분리해서 검증할 수 있다.**

---

# 10. iPad에서 Review Pad 접속

PC의 IP 주소를 확인한다.

예:

```text
192.168.0.90
```

iPad Safari에서 다음과 같이 접속한다.

```text
http://<PC_IP>:30000/vpsync_review_pad.html
```

예:

```text
http://192.168.0.90:30000/vpsync_review_pad.html
```

### WHY — 왜 127.0.0.1이 아닌 PC IP를 쓰는가?

`127.0.0.1`은 현재 장치 자기 자신을 의미한다.

iPad에서 `127.0.0.1`을 입력하면 PC가 아니라 iPad 자신을 가리킨다.

따라서 같은 네트워크에 있는 PC의 실제 IP 주소를 사용해야 한다.

---

# 11. Unreal Remote Control 설정 확인

Unreal에서 다음 Remote Control Preset을 확인한다.

```text
RC_VPSyncOperator
```

노출된 Function은 다음과 같다.

```text
Receive Review Drawing
```

실제 Blueprint 함수는:

```text
ReceiveReviewDrawing
```

입력값은:

```text
DrawingData : FString
```

### WHY — 왜 이 연결이 필요한가?

Proxy가 데이터를 Unreal까지 전달해도 Remote Control Preset에 받을 함수가 연결되어 있지 않으면 Blueprint까지 도달하지 않는다.

전체 흐름의 마지막 연결은 다음과 같다.

```text
Proxy
↓
RC_VPSyncOperator
↓
Receive Review Drawing
↓
BP_VPSessionController.ReceiveReviewDrawing
```

---

# 12. 최종 테스트

iPad Review Pad에서 Apple Pencil 또는 손가락으로 선을 그린다.

그다음:

```text
SEND
```

버튼을 누른다.

### 확인할 것

Review Pad에서 전송이 성공해야 한다.

기존 검증 기준:

```text
PASS - iPad Canvas 접속
PASS - Apple Pencil Stroke 좌표 생성
PASS - 30000 → 30010 Proxy
PASS - DrawingData → ReceiveReviewDrawing → Unreal
```

### WHY — 왜 이 테스트까지 해야 하는가?

웹페이지가 열리는 것만으로는 Review Pad 전체가 정상이라고 볼 수 없다.

실제 기능은 다음 전체 경로가 모두 이어져야 한다.

```text
Apple Pencil
↓
Canvas
↓
DrawingData
↓
:30000
↓
Proxy
↓
:30010
↓
Remote Control
↓
Blueprint
↓
Unreal
```

따라서 SEND 후 Unreal까지 데이터가 도착하는 것을 최종 성공 기준으로 잡는다.

---

# 최종 체크리스트

아래가 전부 완료되면 Review Pad 복구 완료다.

```text
[ ] vpsync_review_pad.html → Server/public 복사

[ ] App.ts에 VPSync Proxy 코드 반영

[ ] bundled Node v16.17.0 PATH 설정

[ ] node -v 정상 확인

[ ] npm.cmd run build 성공

[ ] Unreal Editor 재시작

[ ] PC :30000/vpsync_review_pad.html 접속 성공

[ ] iPad PC_IP:30000/vpsync_review_pad.html 접속 성공

[ ] RC_VPSyncOperator에 Receive Review Drawing 존재

[ ] iPad에서 Stroke 작성

[ ] SEND

[ ] DrawingData가 Unreal까지 전달됨
```

---

# 현재 구현 상태

2026-09-22 기준 검증 결과:

```text
PASS - iPad Canvas 접속
PASS - Apple Pencil Stroke 좌표 생성
PASS - 30000 → 30010 Proxy
PASS - DrawingData → ReceiveReviewDrawing → Unreal Print String
```

이후 VPSync 개발에서는 전달된 `DrawingData`를 Stroke/Point 구조로 파싱하고 PC Decision Monitor에서 다시 그리는 단계까지 확장했다.