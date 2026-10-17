// Copyright Epic Games, Inc. All Rights Reserved.

#include "BodycamUEGameMode.h"
#include "BodycamUECharacter.h"
#include "UObject/ConstructorHelpers.h"

ABodycamUEGameMode::ABodycamUEGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
