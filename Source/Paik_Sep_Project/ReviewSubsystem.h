
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Http.h"
#include "ReviewSubsystem.generated.h"

// C#의 Review 모델과 대응하는 Unreal 구조체
USTRUCT(BlueprintType)
struct FReviewRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Review")
    int32 Id = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Review")
    FString ShotName;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Review")
    FString DrawingData;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Review")
    FString CreatedAt;
};

// POST 저장 완료 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnReviewActionSignature,
    bool, bSuccess,
    FString, Verb
);

// GET 조회 완료 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnReviewLoadedSignature,
    bool, bSuccess,
    const FReviewRecord&, Review
);

UCLASS(BlueprintType)
class PAIK_SEP_PROJECT_API UReviewSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    UPROPERTY(BlueprintAssignable, Category = "Review/Event")
    FOnReviewActionSignature OnActionCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Review/Event")
    FOnReviewLoadedSignature OnReviewLoaded;

    UFUNCTION(BlueprintCallable, Category = "Review")
    void AddReview(FReviewRecord NewReview);

    UFUNCTION(BlueprintCallable, Category = "Review")
    void GetReview(int32 TargetId);

private:

    FString BaseUrl =
        TEXT("http://localhost:5074/api/reviews");

    void OnAddReviewResponse(
        FHttpRequestPtr Request,
        FHttpResponsePtr Response,
        bool bConnectedSuccessfully
    );

    void OnGetReviewResponse(
        FHttpRequestPtr Request,
        FHttpResponsePtr Response,
        bool bConnectedSuccessfully
    );
};
