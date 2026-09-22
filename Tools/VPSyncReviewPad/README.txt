VPSync Review Pad - 2026-09-22

[목적]
iPad Apple Pencil Canvas
→ Remote Control Web Server :30000
→ VPSync Proxy
→ localhost:30010 Remote Control API
→ BP_VPSessionController.ReceiveReviewDrawing
→ Unreal DrawingData 전달

[검증 상태]
PASS - iPad Canvas 접속
PASS - Apple Pencil Stroke 좌표 생성
PASS - 30000 → 30010 Proxy
PASS - DrawingData → ReceiveReviewDrawing → Unreal Print String

[UE 설치 위치]
C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\VirtualProduction\RemoteControlWebInterface\WebApp

[집 PC 재적용]
1. vpsync_review_pad.html
   → WebApp\Server\public\ 에 복사

2. App_with_VPSyncProxy.ts의 VPSync Proxy 코드를
   → WebApp\Server\src\App.ts 에 반영

3. Server 폴더 PowerShell:

$nodeDir = (Resolve-Path ..\node-v16.17.0).Path
$env:Path = "$nodeDir;$env:Path"
node -v
npm.cmd run build

4. Unreal Editor 재시작

5. PC 확인:
http://127.0.0.1:30000/vpsync_review_pad.html

6. iPad 확인:
http://<PC_IP>:30000/vpsync_review_pad.html

[Unreal]
Preset:
RC_VPSyncOperator

Function:
Receive Review Drawing

Underlying BP Function:
ReceiveReviewDrawing

Argument:
DrawingData : FString

[다음 작업]
ReceiveReviewDrawing
→ ReviewDrawingData 저장
→ | 기준 Stroke 분리
→ ; 기준 Point 분리
→ , 기준 X/Y 분리
→ Vector2D 변환
→ WBP Decision Monitor
→ Draw Lines