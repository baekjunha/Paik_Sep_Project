// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VPShotLibrary.generated.h"

class ULevelSequence;
class UMovieSceneSequence;

/**
 * 
 */
UCLASS()
class PAIK_SEP_PROJECT_API UVPShotLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	static int32 GetMasterShotCount(ULevelSequence* MasterSequence);

	UFUNCTION(BlueprintCallable)
	static UMovieSceneSequence* GetMasterShotSequence(ULevelSequence* MasterSequence, int32 ShotIndex);

};
