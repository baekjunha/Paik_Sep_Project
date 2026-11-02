// Fill out your copyright notice in the Description page of Project Settings.


#include "VPShotLibrary.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"

int32 UVPShotLibrary::GetMasterShotCount(ULevelSequence* MasterSequence)
{	
	if (!MasterSequence)
		return 0;

	UMovieScene* MovieScene = MasterSequence->GetMovieScene();
	if (!MovieScene) 
		return 0;
	UMovieSceneCinematicShotTrack* ShotTrack = MovieScene->FindTrack<UMovieSceneCinematicShotTrack>();

	if (!ShotTrack)
		return 0;

	return 0;
}
