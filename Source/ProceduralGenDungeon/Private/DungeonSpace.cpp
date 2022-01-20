// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonSpace.h"

// Sets default values
ADungeonSpace::ADungeonSpace()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	int rows = DungeonSize / TileSize;
	TileArray.Init(FTile(), rows * rows);
	RootSpace = nullptr;

	CubeISMC = CreateDefaultSubobject<class UInstancedStaticMeshComponent>(TEXT("Cube InstancedStaticMesh"));
	CubeISMC->SetMobility(EComponentMobility::Static);
	CubeISMC->SetCollisionProfileName("NoCollision");
	CubeISMC->NumCustomDataFloats = rows * rows;

	FloorTileISMC = CreateDefaultSubobject<class UInstancedStaticMeshComponent>(TEXT("Floor InstancedStaticMesh"));
	FloorTileISMC->SetMobility(EComponentMobility::Static);
	FloorTileISMC->SetCollisionProfileName("BlockAll");

	WallTileISMC = CreateDefaultSubobject<class UInstancedStaticMeshComponent>(TEXT("Wall InstancedStaticMesh"));
	WallTileISMC->SetMobility(EComponentMobility::Static);
	WallTileISMC->SetCollisionProfileName("BlockAll");
	

	

	
}

// Called when the game starts or when spawned
void ADungeonSpace::BeginPlay()
{
	Super::BeginPlay();

	GenerateDungeon();
	FString text;
	PrintTree(text, RootSpace);
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, text);
}

void ADungeonSpace::CreateCellGrid()
{
	CellCols = DungeonSize / MinimapTileSize;
	CellRows = CellCols;
	float x = 0;
	float y = 0;
	float baseMeshSize = 100;


	FTransform newTransform = GetTransform();
	newTransform.SetScale3D(FVector(float(MinimapTileSize) / baseMeshSize, float(MinimapTileSize) / baseMeshSize, float(MinimapTileSize) / baseMeshSize));

	int index = 0;

	for (int row = 0; row < CellRows; row++)
	{
		for (int col = 0; col < CellCols; col++) {
			CellArray.Add(FCell(MinimapTileSize, MinimapTileSize, x, y));
			newTransform.SetLocation(FVector(x, y, float(MinimapTileSize) / 2.f));
			CubeISMC->AddInstance(newTransform);
			CubeISMC->SetCustomDataValue(row + col * DungeonSize, 0, row * 50.f, true);
			x += MinimapTileSize;
		}
		x = 0;
		y += MinimapTileSize;

	}

	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, FString::FromInt(CubeISMC->GetInstanceCount()));
}

void ADungeonSpace::GenerateDungeon()
{
	const int maxElements = pow(2, RoomSplitIterations + 1) - 1;
	FData parentData = FData();
	parentData.width = DungeonSize;
	parentData.height = DungeonSize;
	parentData.left = 0;
	parentData.bottom = 0;
	parentData.seperation = ESeperation(FMath::RandRange(0, 1));
	parentData.tilesSeperated = FMath::RandRange(MinTilesPerRoom, DungeonSize / TileSize - MinTilesPerRoom);
	RootSpace = SplitSpace(nullptr, 0, maxElements, parentData);
	SelectDungeonRooms(RootSpace, 0);
	FillTileGrid();
	ConstructDungeonGrid();
}

FSpace* ADungeonSpace::SplitSpace(FSpace* currentSpace, int index, int maxElements, FData parentData)
{
	if (index < maxElements)
	{
		int minRoomSize = TileSize * MinTilesPerRoom + TileSize * 2;

		FSpace* temp = new FSpace();
		temp->data.key = index;

		//check if the width and height are still big enough to split
		if (parentData.width <= minRoomSize && parentData.height <= minRoomSize)
			return currentSpace;

		//Change data depending on left or right of parent space
		if (index > 0)
		{
			if (index % 2 == 1)//odd = left or top of the space split
			{
				if (parentData.seperation == ESeperation::VERTICAL)
				{
					parentData.width = TileSize * parentData.tilesSeperated;
				}
				else
				{
					parentData.height = parentData.height - (TileSize * parentData.tilesSeperated);
					parentData.bottom = parentData.bottom + TileSize * parentData.tilesSeperated;
				}

				//create startpoint of corridor
				FCorridor* corridor = new FCorridor();
				corridor->start.X = parentData.left + (parentData.width / TileSize / 2 - 1) * TileSize;
				corridor->start.Y = parentData.bottom + (parentData.height / TileSize / 2 + 1) * TileSize;
				corridor->seperation = parentData.seperation;
				corridor->fromSpaceID = index;
				corridor->id = DungeonCorridors.Num() + 1;

				DungeonCorridors.Add(index, corridor);

			}
			else//even = right or bottom of the space split
			{
				if (parentData.seperation == ESeperation::VERTICAL)
				{
					parentData.width = parentData.width - (TileSize * parentData.tilesSeperated);
					parentData.left = parentData.left + TileSize * parentData.tilesSeperated;
				}
				else
				{
					parentData.height = TileSize * parentData.tilesSeperated;
				}

				//create endpoint of corridor if corridor exists
				if (DungeonCorridors.Contains(index - 1))
				{
					FCorridor* corridorOfSister = DungeonCorridors[index - 1];
					corridorOfSister->end.X = parentData.left + (parentData.width / TileSize / 2 + 1) * TileSize;
					corridorOfSister->end.Y = parentData.bottom + (parentData.height / TileSize / 2 - 1) * TileSize;
					corridorOfSister->toSpaceID = index;

				}
			}
		}

		//change data of current space
		temp->data.width = parentData.width;
		temp->data.height = parentData.height;
		temp->data.left = parentData.left;
		temp->data.bottom = parentData.bottom;

		currentSpace = temp;



		//calculate next split
		int minXTiles = int((float(parentData.height) * MinRoomRatio)) / TileSize;
		int maxXTiles = (parentData.width / TileSize) - minXTiles;
		bool isVerticalSplitValid = minXTiles < maxXTiles;

		int minYTiles = int((float(parentData.width) * MinRoomRatio)) / TileSize;
		int maxYTiles = (parentData.height / TileSize) - minYTiles;
		bool isHorizontalSplitValid = minYTiles < maxYTiles;

		if (isVerticalSplitValid && isHorizontalSplitValid)
		{
			//randomize split
			parentData.seperation = ESeperation(FMath::RandRange(0, 1));
			if (parentData.seperation == ESeperation::VERTICAL)
				parentData.tilesSeperated = FMath::RandRange(minXTiles, maxXTiles);
			else
				parentData.tilesSeperated = FMath::RandRange(maxYTiles, maxYTiles);
		}
		else if (isVerticalSplitValid && !isHorizontalSplitValid)
		{
			//vertical split
			parentData.tilesSeperated = FMath::RandRange(minXTiles, maxXTiles);
			parentData.seperation = ESeperation::VERTICAL;
		}
		else if (!isVerticalSplitValid && isHorizontalSplitValid)
		{
			//horizontal split
			parentData.tilesSeperated = FMath::RandRange(minYTiles, maxYTiles);
			parentData.seperation = ESeperation::HORIZONTAL;
		}
		else // no split possible
			return currentSpace;


		currentSpace->left = SplitSpace(currentSpace->left, 2 * index + 1, maxElements, parentData);

		currentSpace->right = SplitSpace(currentSpace->right, 2 * index + 2, maxElements, parentData);
	}
	return currentSpace;
}

void ADungeonSpace::PrintTree(FString& string, FSpace* root)
{
	if (root != nullptr)
	{
		PrintTree(string, root->left);
		string.Append(FString::FromInt(root->data.key));
		string.Append(TEXT(" "));
		PrintTree(string, root->right);
	}
}

void ADungeonSpace::SelectDungeonRooms(FSpace* currentSpace, int currentDepth)
{
	if (currentSpace == nullptr)
		return;

	if (currentDepth == RoomSplitIterations || (currentSpace->left == nullptr || currentSpace->right == nullptr))
	{
		DungeonRooms.Add(currentSpace);
	}

	SelectDungeonRooms(currentSpace->left, currentDepth + 1);
	SelectDungeonRooms(currentSpace->right, currentDepth + 1);
}

void ADungeonSpace::FillTileGrid()
{
	int tilesDungeon = DungeonSize / TileSize;
	int tileIndex;
	//Fill rooms in grid with floor tiles
	int left, right, top, bottom;
	for (int i = 0; i < DungeonRooms.Num(); i++)
	{
		ShrinkSpaceToRoom(DungeonRooms[i]); //todo fix corridor connections

		left = DungeonRooms[i]->data.left;
		right = DungeonRooms[i]->data.left + DungeonRooms[i]->data.width;
		bottom = DungeonRooms[i]->data.bottom;
		top = DungeonRooms[i]->data.bottom + DungeonRooms[i]->data.height;

		for (int row = bottom; row < top; row += TileSize)
		{
			for (int col = left; col < right; col += TileSize) {

				tileIndex = (col / TileSize) + tilesDungeon * (row / TileSize);
				if (TileArray.IsValidIndex(tileIndex))
				{
					TileArray[tileIndex].tileType = ETileType::ROOM;
					TileArray[tileIndex].objectsToSpawn.Add(FDungeonObject()); //default object is a floor
					TileArray[tileIndex].left = col;
					TileArray[tileIndex].bottom = row;
				}

			}

		}
	}

	//fill corridors in grid with floor tiles
	for (auto& elem : DungeonCorridors)
	{
		FCorridor* currentCorridor = elem.Value;
		int x, y;
		if (currentCorridor->seperation == ESeperation::VERTICAL) //vertical seperation = horizontal corridor
		{
			int startTile = -1, endTile = -1;
			y = currentCorridor->start.Y;
			for (x = currentCorridor->start.X; x <= currentCorridor->end.X; x += TileSize)
			{
				tileIndex = (x / TileSize) + tilesDungeon * (y / TileSize);
				if (TileArray.IsValidIndex(tileIndex) && TileArray[tileIndex].objectsToSpawn.Num() == 0)
				{
					TileArray[tileIndex].tileType = ETileType::CORRIDOR;
					TileArray[tileIndex].objectsToSpawn.Add(FDungeonObject()); //floor
					TileArray[tileIndex].left = x;
					TileArray[tileIndex].bottom = y;
					TileArray[tileIndex].corridorID = currentCorridor->id;

					//add walls
					FDungeonObject wall1 = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::TOP, FVector(0, -1, 0));
					FDungeonObject wall2 = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::BOTTOM, FVector(0, -1, 0));
					TileArray[tileIndex].objectsToSpawn.Add(wall1);
					TileArray[tileIndex].objectsToSpawn.Add(wall2);

					//startTile is first valid tile index and endTile is last valid tileIndex
					if(startTile == -1)
					{
						startTile = tileIndex;
					}
					endTile = tileIndex;
				}
			}
			//spawn wall on start and endtile
			/*if(TileArray.IsValidIndex(startTile))
			{
				FDungeonObject wallLeft = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::LEFT, FVector(1, 0, 0));
				TileArray[startTile].objectsToSpawn.Add(wallLeft);
			}
			if (TileArray.IsValidIndex(endTile))
			{
				FDungeonObject wallRight = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::RIGHT, FVector(1, 0, 0));
				TileArray[endTile].objectsToSpawn.Add(wallRight);
			}*/
			
		}
		else if (currentCorridor->seperation == ESeperation::HORIZONTAL)//horizontal seperation = vertical corridor
		{
			int startTile = -1, endTile = -1;
			x = currentCorridor->start.X;
			for (y = currentCorridor->start.Y; y >= currentCorridor->end.Y; y -= TileSize)
			{
				tileIndex = (x / TileSize) + tilesDungeon * (y / TileSize);
				if (TileArray.IsValidIndex(tileIndex) && TileArray[tileIndex].objectsToSpawn.Num() == 0)
				{
					TileArray[tileIndex].tileType = ETileType::CORRIDOR;
					TileArray[tileIndex].objectsToSpawn.Add(FDungeonObject()); //floor
					TileArray[tileIndex].left = x;
					TileArray[tileIndex].bottom = y;
					TileArray[tileIndex].corridorID = currentCorridor->id;

					//add walls
					FDungeonObject wall1 = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::LEFT, FVector(1, 0, 0));
					FDungeonObject wall2 = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::RIGHT, FVector(1, 0, 0));
					TileArray[tileIndex].objectsToSpawn.Add(wall1);
					TileArray[tileIndex].objectsToSpawn.Add(wall2);

					//startTile is first valid tile index and endTile is last valid tileIndex
					if (startTile == -1)
					{
						startTile = tileIndex;
					}
					endTile = tileIndex;
				}
			}
			//spawn wall on start and endtile
			/*if (TileArray.IsValidIndex(startTile))
			{
				FDungeonObject wallTop = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::TOP, FVector(0, -1, 0));
				TileArray[startTile].objectsToSpawn.Add(wallTop);
			}
			if (TileArray.IsValidIndex(endTile))
			{
				FDungeonObject wallBot = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::BOTTOM, FVector(0, -1, 0));
				TileArray[endTile].objectsToSpawn.Add(wallBot);
			}*/
		}
	}

	//add other objectsToSpawn to rooms
	for (int i = 0; i < DungeonRooms.Num(); i++)
	{
		left = DungeonRooms[i]->data.left;
		right = DungeonRooms[i]->data.left + DungeonRooms[i]->data.width;
		bottom = DungeonRooms[i]->data.bottom;
		top = DungeonRooms[i]->data.bottom + DungeonRooms[i]->data.height;
		
		//hold Tmap with corridors id and true or false if they are already connected
		TArray<int> roomCorridorConnections{};
		//change to 4 for loops (left, right, ... side)
		for (int row = bottom; row < top; row += TileSize)
		{
			for (int col = left; col < right; col += TileSize) {

				tileIndex = (col / TileSize) + tilesDungeon * (row / TileSize);
				if (TileArray.IsValidIndex(tileIndex))
				{
					if(row == bottom)
					{
						FDungeonObject dObject = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::BOTTOM, FVector(0, -1, 0));
						TileArray[tileIndex].objectsToSpawn.Add(dObject);
					}else if(row == top - TileSize)
					{
						FDungeonObject dObject = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::TOP, FVector(0, -1, 0));
						TileArray[tileIndex].objectsToSpawn.Add(dObject);
					}
					if(col == left && CheckIfWallShouldBePlaced(tileIndex - 1, roomCorridorConnections))
					{
						FDungeonObject dObject = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::LEFT, FVector(1, 0, 0));
						TileArray[tileIndex].objectsToSpawn.Add(dObject);
						
					}else if(col == right - TileSize && CheckIfWallShouldBePlaced(tileIndex + 1, roomCorridorConnections))
					{
						FDungeonObject dObject = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::RIGHT, FVector(1, 0, 0));
						TileArray[tileIndex].objectsToSpawn.Add(dObject);
					}
					
				}

			}

		}
	
	}
	
}

void ADungeonSpace::ConstructDungeonGrid()
{
	int rows = DungeonSize / TileSize;
	int tileIndex;
	FTransform minimapTileTransform = GetTransform();
	FTransform dungeonTileTranform = GetTransform();
	FVector FromActorToMinimapPos = MinimapPos - GetActorLocation();
	UInstancedStaticMeshComponent* meshISMCToAddInstance = nullptr;
	int objectWidth;
	uint32 newInstanceIndex;
	
	//scale cube mesh to minimap tile size
	minimapTileTransform.SetScale3D(FVector(float(MinimapTileSize) / CubeMeshSize, float(MinimapTileSize) / CubeMeshSize, float(MinimapTileSize) / CubeMeshSize));

	for (int row = 0; row < rows; row++)
	{
		for (int col = 0; col < rows; col++)
		{
			tileIndex = col + rows * row;
			//Check if index is valid and tile is not empty
			if (TileArray.IsValidIndex(tileIndex) && TileArray[tileIndex].tileType != ETileType::EMPTY)
			{
				//create minimap
				if (IsShowingMinimap)
				{
					minimapTileTransform.SetLocation(FVector(col * MinimapTileSize + FromActorToMinimapPos.X, row * MinimapTileSize + FromActorToMinimapPos.Y, FromActorToMinimapPos.Z));
					newInstanceIndex = CubeISMC->AddInstance(minimapTileTransform);
					switch (TileArray[tileIndex].tileType)
					{
					case ETileType::ROOM:
						CubeISMC->SetCustomDataValue(newInstanceIndex, 0, 0.7f, true);
						break;
					case ETileType::CORRIDOR:
						CubeISMC->SetCustomDataValue(newInstanceIndex, 0, 0.2f, true);
						break;
					}
				}
				//create instances for all objectsToSpawn on the tile
				//loop over dungeon objectsToSpawn to create instances of meshes
				if (TileArray[tileIndex].objectsToSpawn.Num() > 0)
				{
					int left, bottom;
					for (int i = 0; i < TileArray[tileIndex].objectsToSpawn.Num(); i++)
					{
						left = TileArray[tileIndex].left;
						bottom = TileArray[tileIndex].bottom;
						//change ISMC depending on object type and the object width (helps with alighning object)
						switch (TileArray[tileIndex].objectsToSpawn[i].objectType)
						{
						case EDungeonObjectType::FLOOR:
							meshISMCToAddInstance = FloorTileISMC;
							objectWidth = 0;
							break;
						case EDungeonObjectType::WALL:
							meshISMCToAddInstance = WallTileISMC;
							objectWidth = WallTileWidth;
						case EDungeonObjectType::CEILING:
							break;
						case EDungeonObjectType::PILLAR:
							break;
						case EDungeonObjectType::TORCH:
							break;
						}

						//change transform to alignment of object
						FVector rotationVector = TileArray[tileIndex].objectsToSpawn[i].rotation;
						float customDataValue = 0.7f;
						switch (TileArray[tileIndex].objectsToSpawn[i].objectAlignement)
						{
						case EDungeonObjectAlign::LEFT:
							dungeonTileTranform.SetLocation(FVector(left, bottom + TileSize / 2, 0));
							dungeonTileTranform.SetRotation(rotationVector.Rotation().Quaternion());
							customDataValue = 0.2f;
							break;
						case EDungeonObjectAlign::RIGHT:
							dungeonTileTranform.SetLocation(FVector(left+TileSize, bottom + TileSize / 2, 0));
							dungeonTileTranform.SetRotation(rotationVector.Rotation().Quaternion());
							customDataValue = 0.2f;
							break;
						case EDungeonObjectAlign::TOP:
							dungeonTileTranform.SetLocation(FVector(left + TileSize / 2, bottom + TileSize, 0));
							dungeonTileTranform.SetRotation(rotationVector.Rotation().Quaternion());
							customDataValue = 0.7f;
							break;
						case EDungeonObjectAlign::BOTTOM:
							dungeonTileTranform.SetLocation(FVector(left + TileSize / 2, bottom, 0));
							dungeonTileTranform.SetRotation(rotationVector.Rotation().Quaternion());
							customDataValue = 0.7f;
							break;
						case EDungeonObjectAlign::CENTER:
							dungeonTileTranform.SetLocation(FVector(left+TileSize/2, bottom+TileSize/2, 0));
							dungeonTileTranform.SetRotation(rotationVector.Rotation().Quaternion());
							break;
						}

						if (meshISMCToAddInstance != nullptr)
						{
							newInstanceIndex = meshISMCToAddInstance->AddInstance(dungeonTileTranform);
							meshISMCToAddInstance->SetCustomDataValue(newInstanceIndex, 0, customDataValue, true);
						}
							
						meshISMCToAddInstance = nullptr;
					}
				}
			}
		}
	}
}

void ADungeonSpace::ShrinkSpaceToRoom(FSpace* currentSpace)
{
	if(currentSpace != nullptr)
	{
		//check if there are spare tiles
		int extraTilesInWidth = (currentSpace->data.width / TileSize) - MinTilesPerRoom;
		extraTilesInWidth = std::min(extraTilesInWidth, (currentSpace->data.width / TileSize / 2));
		if (extraTilesInWidth > 1)
		{
			extraTilesInWidth = FMath::RandRange(1, extraTilesInWidth);
			currentSpace->data.width -= extraTilesInWidth * TileSize; 
			if (extraTilesInWidth % 2 == 1)
				extraTilesInWidth = -1;
			currentSpace->data.left += (extraTilesInWidth/2) * TileSize;
		}


		int extraTilesInHeight = (currentSpace->data.height / TileSize) - MinTilesPerRoom;
		extraTilesInHeight = std::min(extraTilesInHeight, (currentSpace->data.height / TileSize) / 2);
		if (extraTilesInHeight > 1)
		{
			extraTilesInHeight = FMath::RandRange(1, extraTilesInHeight);
			currentSpace->data.height -= extraTilesInHeight * TileSize;
			if (extraTilesInHeight % 2 == 1)
				extraTilesInHeight = -1;
			currentSpace->data.bottom += (extraTilesInHeight/2) * TileSize;
		}
	}
}

bool ADungeonSpace::CheckIfWallShouldBePlaced(int adjacentTileIndex, TArray<int>& roomCorridorConnections)
{
	//check if the adjacent tile is not in grid -> place wall
	if (!TileArray.IsValidIndex(adjacentTileIndex))
		return true;

	//put wall if adjacent tile is empty
	if (TileArray[adjacentTileIndex].tileType == ETileType::EMPTY)
		return true;

	//check if adjacent tile is corridor
	if (TileArray[adjacentTileIndex].tileType == ETileType::CORRIDOR)
	{
		//check if corridor id of adjacent corridor is already connected
		if (roomCorridorConnections.Find(TileArray[adjacentTileIndex].corridorID) != INDEX_NONE)
		{
			//corridor is already connected to room -> place wall
			return true;
		}
		else
		{
			roomCorridorConnections.Add(TileArray[adjacentTileIndex].corridorID);
			return false;
		}
	}

	//do not place a wall
	return false;
}

// Called every frame
void ADungeonSpace::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

