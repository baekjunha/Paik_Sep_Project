# VPSync Review DB 백업

이 폴더의 `VPSyncReviewDb_2026-11-19.bak`는 VPSync Review 메타데이터와 필기 데이터를 보존한 SQL Server LocalDB 전체 백업이다.

## 백업 내용

- 원본 DB: `(localdb)\MSSQLLocalDB` / `VPSyncReviewDb`
- 백업일: 2026-11-19 (Asia/Seoul)
- Review 행: 11건
- Review ID 범위: 1~11
- 포함 데이터: `ShotName`, `Frame`, `DrawingData`, `CreatedAt`, `ScreenshotFileName`
- 대응 이미지: `../ReviewImages/review_1.png` ~ `review_11.png`
- SHA-256: `4B3641DFB29BCEB63B15E69CB8583306D79C967667F7A676FFCEE8E55F7DBA1D`

PNG는 DB 안에 들어 있지 않다. DB에는 PNG 파일명만 저장되므로 `bak/ReviewImages`도 함께 복원해야 한다.

## 다른 Windows PC에서 복원

1. SQL Server Express LocalDB와 SQL Server Management Studio(SSMS)를 설치한다.
2. SSMS에서 `(localdb)\MSSQLLocalDB`에 Windows 인증으로 연결한다.
3. `Databases`를 우클릭하고 `Restore Database...`를 선택한다.
4. Source에서 `Device`를 선택하고 이 `.bak` 파일을 지정한다.
5. Files 페이지에서 새 PC에서 사용할 수 있는 데이터 폴더로 MDF/LDF 경로를 바꾼다.
6. 기존 `VPSyncReviewDb`가 있다면 먼저 필요한 데이터를 백업한 뒤 교체 여부를 결정한다.
7. 복원 후 아래 쿼리로 11개 Review가 있는지 확인한다.

```sql
USE VPSyncReviewDb;
SELECT COUNT(*) AS ReviewCount, MAX(Id) AS MaxReviewId
FROM dbo.Reviews;
```

정상 결과는 `ReviewCount = 11`, `MaxReviewId = 11`이다.

## 이미지 배치

`bak/ReviewImages`의 `review_*.png`를 새 PC의 아래 폴더에 복사한다.

```text
C:\VPSyncData\ReviewImages
```

그다음 Review API를 실행하고 `GET http://localhost:5000/api/reviews`에서 ID 1~11이 반환되는지 확인한다.
