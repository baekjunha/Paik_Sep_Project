// Fill out your copyright notice in the Description page of Project Settings.
#include "VPShotLibrary.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Runtime/LevelSequence/Public/LevelSequence.h"
#include "Sections/MovieSceneSubSection.h"




int32 UVPShotLibrary::GetMasterShotCount(ULevelSequence* MasterSequence)
{	
	if (!MasterSequence)
		return 0;

	UMovieScene* MovieScene = MasterSequence->GetMovieScene(); //UMovieScene* 를 반환하는 함수가 GetMovieScene()이다.
	if (!MovieScene) 
		return 0;
	UMovieSceneCinematicShotTrack* ShotTrack = MovieScene->FindTrack<UMovieSceneCinematicShotTrack>(); //없으면 findtrack함수는 nullptr반환

	if (!ShotTrack)
		return 0;
	//만약 있다면
	const TArray<UMovieSceneSection*>& Section = ShotTrack->GetAllSections(); //Section 포인터 배열을 const 참조로 반환
	int32 ShotCount = Section.Num();
	if(ShotCount == 0) return 0;

	return ShotCount;
}


UMovieSceneSequence* UVPShotLibrary::GetMasterShotSequence(ULevelSequence* MasterSequence, int32 ShotIndex)
{
	if (!MasterSequence) return nullptr;
	UMovieScene* MovieScene = MasterSequence->GetMovieScene();
	if(!MovieScene) return nullptr;
	
	UMovieSceneCinematicShotTrack* ShotTrack = MovieScene->FindTrack<UMovieSceneCinematicShotTrack>();
	if (!ShotTrack) return nullptr;

	const TArray<UMovieSceneSection*>& Section = ShotTrack->GetAllSections();
	if (Section.IsValidIndex(ShotIndex) == false) return nullptr;

	UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section[ShotIndex]);



}



