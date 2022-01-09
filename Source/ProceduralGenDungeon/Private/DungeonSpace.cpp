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
	//Fill rooms in grid
	int left, right, top, bottom;
	for (int i = 0; i < DungeonRooms.Num(); i++)
	{
		ShrinkSpaceToRoom(DungeonRooms[i]);

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
					TileArray[tileIndex].objects.Add(FDungeonObject()); //default object is a floor
					TileArray[tileIndex].left = col;
					TileArray[tileIndex].bottom = row;
				}

			}

		}
	}

	//fill corridors in grid
	for (auto& elem : DungeonCorridors)
	{
		FCorridor* currentCorridor = elem.Value;
		int x, y;
		if (currentCorridor->seperation == ESeperation::VERTICAL) //vertical seperation = horizontal corridor
		{
			y = currentCorridor->start.Y;
			for (x = currentCorridor->start.X; x <= currentCorridor->end.X; x += TileSize)
			{
				tileIndex = (x / TileSize) + tilesDungeon * (y / TileSize);
				if (TileArray.IsValidIndex(tileIndex) && TileArray[tileIndex].objects.Num() == 0)
				{
					TileArray[tileIndex].tileType = ETileType::CORRIDOR;
					TileArray[tileIndex].objects.Add(FDungeonObject());
					TileArray[tileIndex].left = x;
					TileArray[tileIndex].bottom = y;
				}
			}
		}
		else if (currentCorridor->seperation == ESeperation::HORIZONTAL)//horizontal seperation = vertical corridor
		{
			x = currentCorridor->start.X;
			for (y = currentCorridor->start.Y; y >= currentCorridor->end.Y; y -= TileSize)
			{
				tileIndex = (x / TileSize) + tilesDungeon * (y / TileSize);
				if (TileArray.IsValidIndex(tileIndex) && TileArray[tileIndex].objects.Num() == 0)
				{
					TileArray[tileIndex].tileType = ETileType::CORRIDOR;
					TileArray[tileIndex].objects.Add(FDungeonObject());
					TileArray[tileIndex].left = x;
					TileArray[tileIndex].bottom = y;
				}
			}
		}
	}

	//add other objects to rooms
	for (int i = 0; i < DungeonRooms.Num(); i++)
	{
		ShrinkSpaceToRoom(DungeonRooms[i]);

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
					if(row == bottom)
					{
						FDungeonObject dObject = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::BOTTOM, FVector());
						TileArray[tileIndex].objects.Add(dObject);
					}else if(row == top - TileSize)
					{
						FDungeonObject dObject = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::TOP, FVector());
						TileArray[tileIndex].objects.Add(dObject);
					}
					if(col == left)
					{
						FDungeonObject dObject = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::LEFT, FVector());
						TileArray[tileIndex].objects.Add(dObject);
					}else if(col == right - TileSize)
					{
						FDungeonObject dObject = FDungeonObject(EDungeonObjectType::WALL, EDungeonObjectAlign::RIGHT, FVector());
						TileArray[tileIndex].objects.Add(dObject);
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
				//create instances for all objects on the tile
				dungeonTileTranform.SetLocation(FVector(TileArray[tileIndex].left, TileArray[tileIndex].bottom, 0));
				newInstanceIndex = FloorTileISMC->AddInstance(dungeonTileTranform);
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

// Called every frame
void ADungeonSpace::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

