
#include "ReviewSubsystem.h"

#include "HttpModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"


// =====================================================
// 1. JSON 객체를 FReviewRecord 구조체로 변환
// =====================================================

static bool JsonObjectToReview(
    const TSharedPtr<FJsonObject>& JsonObject,
    FReviewRecord& OutResult)
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    double IdValue = 0.0;

    // 서버에서 받은 JSON의 필수 필드 읽기
    bool bValid =
        JsonObject->TryGetNumberField(
            TEXT("id"), IdValue
        ) &&
        JsonObject->TryGetStringField(
            TEXT("shotName"), OutResult.ShotName
        ) &&
        JsonObject->TryGetStringField(
            TEXT("drawingData"), OutResult.DrawingData
        );

    if (!bValid)
    {
        return false;
    }

    OutResult.Id = static_cast<int32>(IdValue);

    // 날짜는 선택적으로 읽는다.
    JsonObject->TryGetStringField(
        TEXT("createdAt"),
        OutResult.CreatedAt
    );

    return true;
}


// =====================================================
// 2. FReviewRecord 구조체를 JSON 문자열로 변환
// =====================================================

static FString MakeJsonFromReview(
    const FReviewRecord& Review)
{
    TSharedPtr<FJsonObject> JsonObject =
        MakeShareable(new FJsonObject());

    JsonObject->SetStringField(TEXT("shotName"), Review.ShotName );

    JsonObject->SetStringField(TEXT("drawingData"),Review.DrawingData);

    FString JsonString;

    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);

    FJsonSerializer::Serialize(JsonObject.ToSharedRef(),Writer);

    return JsonString;
}


// =====================================================
// 3. POST: Review 저장 요청
// =====================================================

void UReviewSubsystem::AddReview(
    FReviewRecord NewReview)
{
    // 구조체를 JSON 문자열로 변환
    FString JsonBody =
        MakeJsonFromReview(NewReview);

    // HTTP 요청 객체 생성
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    // URL 설정
    Request->SetURL(BaseUrl);

    // HTTP Method 설정
    Request->SetVerb(TEXT("POST"));

    // 전송 데이터 형식 설정
    Request->SetHeader(
        TEXT("Content-Type"),
        TEXT("application/json")
    );

    // JSON Body 설정
    Request->SetContentAsString(JsonBody);

    // 응답 완료 시 실행할 Callback 연결
    Request->OnProcessRequestComplete().BindUObject(
        this,
        &UReviewSubsystem::OnAddReviewResponse
    );

    // HTTP 요청 시작
    Request->ProcessRequest();
}


// =====================================================
// 4. POST: 서버 응답 처리
// =====================================================

void UReviewSubsystem::OnAddReviewResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bConnectedSuccessfully)
{
    if (!bConnectedSuccessfully || !Response.IsValid())
    {
        OnActionCompleted.Broadcast(
            false,
            TEXT("POST")
        );

        return;
    }

    FString ResponseString =
        Response->GetContentAsString().TrimStartAndEnd();

    // C# 서버의 bool 응답(true/false) 확인
    bool bSuccess =
        Response->GetResponseCode() == 200 &&
        ResponseString.Equals(
            TEXT("true"),
            ESearchCase::IgnoreCase
        );

    // 저장 결과를 Blueprint에 전달
    OnActionCompleted.Broadcast(
        bSuccess,
        TEXT("POST")
    );
}


// =====================================================
// 5. GET: 특정 Review 조회 요청
// =====================================================

void UReviewSubsystem::GetReview(int32 TargetId)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    // /api/reviews/1 형태의 URL 생성
    FString Url = FString::Printf(
        TEXT("%s/%d"),
        *BaseUrl,
        TargetId
    );

    Request->SetURL(Url);

    Request->SetVerb(TEXT("GET"));

    Request->OnProcessRequestComplete().BindUObject(
        this,
        &UReviewSubsystem::OnGetReviewResponse
    );

    Request->ProcessRequest();
}


// =====================================================
// 6. GET: 서버 응답 처리
// =====================================================

void UReviewSubsystem::OnGetReviewResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bConnectedSuccessfully)
{
    FReviewRecord Result;

    if (!bConnectedSuccessfully ||
        !Response.IsValid() ||
        Response->GetResponseCode() != 200)
    {
        OnReviewLoaded.Broadcast(false, Result);
        return;
    }

    // 서버에서 받은 JSON 문자열
    FString ResponseString =
        Response->GetContentAsString();

    // JSON 객체를 담을 변수
    TSharedPtr<FJsonObject> JsonObject;

    // JSON 문자열을 읽는 Reader 생성
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(ResponseString);

    // JSON 문자열 → JSON 객체
    bool bParseSuccess =
        FJsonSerializer::Deserialize(
            Reader,
            JsonObject
        );

    if (!bParseSuccess || !JsonObject.IsValid())
    {
        OnReviewLoaded.Broadcast(false, Result);
        return;
    }

    // JSON 객체 → FReviewRecord 구조체
    bool bSuccess =
        JsonObjectToReview(JsonObject, Result);

    // Blueprint에 조회 결과 전달
    OnReviewLoaded.Broadcast(
        bSuccess,
        Result
    );
}

