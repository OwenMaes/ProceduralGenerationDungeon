// Copyright Epic Games, Inc. All Rights Reserved.


#include "ProceduralGenDungeonGameModeBase.h"
#include "BaseCharacter.h"

AProceduralGenDungeonGameModeBase::AProceduralGenDungeonGameModeBase()
{
	DefaultPawnClass = ABaseCharacter::StaticClass();
}

void AProceduralGenDungeonGameModeBase::BeginPlay()
{
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, TEXT("using AProceduralGenDungeonGameModeBase"));
}
