#include "Room.h"
#include "Crest.h"

Room::Room(const char* xmlFile, PhysicsManager* pm, float xPos, float zPos)
{
	mapFile = xmlFile;
	physicsMan = pm;
	x = xPos;
	z = zPos;

	tinyxml2::XMLDocument doc;

	doc.LoadFile(xmlFile);

	// Set initial room dimensions
	width = -100.0f;
	depth = -100.0f;

	mapOffsetX = 0.0f;
	mapOffsetZ = 0.0f;

	bool isFirst = true;

	XMLElement* walls = doc.FirstChildElement( "level" )->FirstChildElement( "walls" );

	// Find room walls
	for (XMLElement* wall = walls->FirstChildElement("wall"); wall != NULL; wall = wall->NextSiblingElement("wall"))
	{
		// Get information from xml
		const char* row = wall->Attribute("row");
		const char* col = wall->Attribute("col");

		const char* xLength = wall->Attribute("xLength");
		const char* zLength = wall->Attribute("zLength");

		if (isFirst)
		{
			mapOffsetX = (float)-atof(col);
			mapOffsetZ = (float)-atof(row);

			isFirst = false;
		}

		else
		{
			if (atof(col) < -mapOffsetX)
				mapOffsetX = (float)-atof(col);
		}

		// increase room dimensions if necessary
		if (atof(col) + atof(xLength) + 1 + mapOffsetX > width)
			width = (float)atof(col) + (float)atof(xLength) + (float)mapOffsetX;
		if (atof(row) + atof(zLength) + 1 + mapOffsetZ > depth)
			depth = (float)atof(row) + (float)atof(zLength) + (float)mapOffsetZ;
	}

	XMLElement* exits = doc.FirstChildElement( "level" )->FirstChildElement( "exits" );

	// Find room exits
	for (XMLElement* exit = exits->FirstChildElement("exit"); exit != NULL; exit = exit->NextSiblingElement("exit"))
	{
		Wall* tempWall = new Wall();

		// Get information from xml
		const char* row = exit->Attribute("row");
		const char* col = exit->Attribute("col");
		const char* file = exit->Attribute("file");
		const char* xLength = exit->Attribute("xLength");
		const char* zLength = exit->Attribute("zLength");
		const char* centerX = exit->Attribute("centerX");
		const char* centerY = exit->Attribute("centerY");
		const char* centerZ = exit->Attribute("centerZ");

		// Get full filename of xml file
		char folder[80] = "Assets/";
		const char* fullFile = strcat(folder, file);

		// Store exit information in Wall object
		tempWall->row = (float)atof(row) + mapOffsetZ;
		tempWall->col = (float)atof(col) + mapOffsetX;
		tempWall->xLength = (float)atof(xLength);
		tempWall->zLength = (float)atof(zLength);
		tempWall->centerX = (float)atof(centerX) + mapOffsetX;
		tempWall->centerY = (float)atof(centerY);
		tempWall->centerZ = (float)atof(centerZ) + mapOffsetZ;
		tempWall->file = fullFile;
		tempWall->direction = "";

		// Add to exit vector
		exitVector.push_back(tempWall);
	}
}


Room::~Room(void)
{
	for (unsigned int i = 0; i < exitVector.size(); ++i)
	{
		Wall* temp = exitVector[exitVector.size() - 1];
		exitVector.pop_back();
		delete temp;
		--i;
	}
	//exitVector.clear();

	for (unsigned int i = 0; i < spawnVector.size(); ++i)
	{
		Wall* temp = spawnVector[spawnVector.size() - 1];
		spawnVector.pop_back();
		delete temp;
		--i;
	}
	//spawnVector.clear();


	for (unsigned int i = 0; i < cubeVector.size(); ++i)
	{
		Cube* temp = cubeVector[cubeVector.size() - 1];
		cubeVector.pop_back();
		delete temp;
		--i;
	}
	//cubeVector.clear();

	for (unsigned int i = 0; i < crestVector.size(); ++i)
	{
		Wall* temp = crestVector[crestVector.size() - 1];
		crestVector.pop_back();
		delete temp;
		--i;
	}
	//crestVector.clear();

	for (unsigned int i = 0; i < gameObjs.size(); ++i)
	{
		GameObject* temp = gameObjs[gameObjs.size() - 1];
		gameObjs.pop_back();
		delete temp;
		--i;
	}
	//gameObjs.clear();
}

void Room::loadRoom(void)
{
	loadRoom(x, z);
}

void Room::loadRoom(float xPos, float zPos)
{
	tinyxml2::XMLDocument doc;

	doc.LoadFile(mapFile);

	XMLElement* walls = doc.FirstChildElement( "level" )->FirstChildElement( "walls" );
	XMLElement* spawns = doc.FirstChildElement( "level" )->FirstChildElement( "spawns" );
	XMLElement* cubes = doc.FirstChildElement( "level" )->FirstChildElement( "cubes" );
	XMLElement* crests = doc.FirstChildElement( "level" )->FirstChildElement( "crests" );

	vector<vector<Wall*>> wallRowCol;
	vector<Wall*> wallVector;

	wallRowCol.assign(25, wallVector);

	for (XMLElement* wall = walls->FirstChildElement("wall"); wall != NULL; wall = wall->NextSiblingElement("wall"))
	{
		Wall* tempWall = new Wall();

		const char* row = wall->Attribute("row");
		const char* col = wall->Attribute("col");
		const char* xLength = wall->Attribute("xLength");
		const char* yLength = wall->Attribute("yLength");
		const char* zLength = wall->Attribute("zLength");
		const char* centerX = wall->Attribute("centerX");
		const char* centerY = wall->Attribute("centerY");
		const char* centerZ = wall->Attribute("centerZ");

		tempWall->row = (float)atof(row) + mapOffsetZ;
		tempWall->col = (float)atof(col) + mapOffsetX;
		tempWall->xLength = (float)atof(xLength);
		tempWall->yLength = (float)atof(yLength);
		tempWall->zLength = (float)atof(zLength);
		tempWall->centerX = (float)atof(centerX) + mapOffsetX;
		tempWall->centerY = (float)atof(centerY);
		tempWall->centerZ = (float)atof(centerZ) + mapOffsetZ;
		tempWall->direction = "";
		tempWall->file = "";
	
		wallRowCol[(unsigned int)atof(row) + (unsigned int)mapOffsetZ].push_back(tempWall);
	}

	for (XMLElement* spawn = spawns->FirstChildElement("spawn"); spawn != NULL; spawn = spawn->NextSiblingElement("spawn"))
	{
		Wall* tempWall = new Wall();

		const char* row = spawn->Attribute("row");
		const char* col = spawn->Attribute("col");
		const char* dir = spawn->Attribute("dir");
		const char* xLength = spawn->Attribute("xLength");
		const char* zLength = spawn->Attribute("zLength");
		const char* centerX = spawn->Attribute("centerX");
		const char* centerY = spawn->Attribute("centerY");
		const char* centerZ = spawn->Attribute("centerZ");

		tempWall->row = (float)atof(row) + mapOffsetZ;
		tempWall->col = (float)atof(col) + mapOffsetX;
		tempWall->xLength = (float)atof(xLength);
		tempWall->zLength = (float)atof(zLength);
		tempWall->centerX = (float)atof(centerX) + mapOffsetX;
		tempWall->centerY = (float)atof(centerY);
		tempWall->centerZ = (float)atof(centerZ) + mapOffsetZ;
		tempWall->direction = dir;
		tempWall->file = "";
	
		spawnVector.push_back(tempWall);
	}

	for (XMLElement* cube = cubes->FirstChildElement("cube"); cube != NULL; cube = cube->NextSiblingElement("cube"))
	{
		Cube* tempCube = new Cube();

		const char* row = cube->Attribute("row");
		const char* col = cube->Attribute("col");
		const char* xLength = cube->Attribute("xLength");
		const char* zLength = cube->Attribute("zLength");
		const char* centerX = cube->Attribute("centerX");
		const char* centerY = cube->Attribute("centerY");
		const char* centerZ = cube->Attribute("centerZ");
		const char* translateX = cube->Attribute("translateX");
		const char* translateY = cube->Attribute("translateY");
		const char* translateZ = cube->Attribute("translateZ");
		const char* scaleX = cube->Attribute("scaleX");
		const char* scaleY = cube->Attribute("scaleY");
		const char* scaleZ = cube->Attribute("scaleZ");

		tempCube->row = (float)atof(row) + mapOffsetZ;
		tempCube->col = (float)atof(col) + mapOffsetX;
		tempCube->xLength = (float)atof(xLength);
		tempCube->zLength = (float)atof(zLength);
		tempCube->centerX = (float)atof(centerX) + mapOffsetX;
		tempCube->centerY = (float)atof(centerY);
		tempCube->centerZ = (float)atof(centerZ) + mapOffsetZ;
		tempCube->translateX = (float)atof(translateX);
		tempCube->translateY = (float)atof(translateY);
		tempCube->translateZ = (float)atof(translateZ);
		tempCube->scaleX = (float)atof(scaleX);
		tempCube->scaleY = (float)atof(scaleY);
		tempCube->scaleZ = (float)atof(scaleZ);
		tempCube->direction = "";
		tempCube->file = "";
	
		cubeVector.push_back(tempCube);
	}

	for (XMLElement* crest = crests->FirstChildElement("crest"); crest != NULL; crest = crest->NextSiblingElement("crest"))
	{
		Wall* tempWall = new Wall();

		const char* row = crest->Attribute("row");
		const char* col = crest->Attribute("col");
		const char* effect = crest->Attribute("effect");
		const char* xLength = crest->Attribute("xLength");
		const char* zLength = crest->Attribute("zLength");
		const char* centerX = crest->Attribute("centerX");
		const char* centerY = crest->Attribute("centerY");
		const char* centerZ = crest->Attribute("centerZ");
		const char* target = crest->Attribute("target");

		tempWall->row = (float)atof(row) + mapOffsetZ;
		tempWall->col = (float)atof(col) + mapOffsetX;
		tempWall->xLength = (float)atof(xLength);
		tempWall->zLength = (float)atof(zLength);
		tempWall->centerX = (float)atof(centerX) + mapOffsetX;
		tempWall->centerY = (float)atof(centerY);
		tempWall->centerZ = (float)atof(centerZ) + mapOffsetZ;
		tempWall->direction = "";
		tempWall->file = "";
		tempWall->effect = static_cast<CREST_TYPE>(atoi(effect));
		tempWall->target = target;
	
		crestVector.push_back(tempWall);
	}
	
	// Create walls and add to GameObject vector
	for (unsigned int i = 0; i < wallRowCol.size(); i++)
	{
		for (unsigned int j = 0; j < wallRowCol[i].size(); j++)
		{
			GameObject* wallObj = new GameObject("Cube", "Test Wall", physicsMan->createRigidBody("Cube", wallRowCol[i][j]->centerX + xPos, wallRowCol[i][j]->yLength / 2, wallRowCol[i][j]->centerZ + zPos), physicsMan, WORLD);
			wallObj->scale(wallRowCol[i][j]->xLength, wallRowCol[i][j]->yLength, wallRowCol[i][j]->zLength);
			gameObjs.push_back(wallObj);
		}
	}

	for (unsigned int i = 0; i < cubeVector.size(); i++)
	{
		MovingObject* cubeObj = new MovingObject("Cube", "Test Wood", physicsMan->createRigidBody("Cube", cubeVector[i]->centerX + xPos, 1.5f, cubeVector[i]->centerZ + zPos), physicsMan);
		cubeObj->scale(cubeVector[i]->xLength,3.0,cubeVector[i]->zLength);
		//cubeObj->addCollisionFlags(btCollisionObject::CollisionFlags::CF_KINEMATIC_OBJECT|btCollisionObject::CollisionFlags::CF_STATIC_OBJECT);
		cubeObj->AddPosition(XMFLOAT3(cubeVector[i]->centerX + xPos, 1.5f, cubeVector[i]->centerZ + zPos));
		cubeObj->AddPosition(XMFLOAT3(cubeVector[i]->centerX + xPos + cubeVector[i]->translateX, 1.5f + cubeVector[i]->translateY, cubeVector[i]->centerZ + zPos + cubeVector[i]->translateZ));

		float rowId = cubeVector[i]->row - mapOffsetZ;
		float colId = cubeVector[i]->col - mapOffsetX;
		
		char rowChar[30];
		char colChar[30];

		itoa((int)rowId, rowChar, 10);
		itoa((int)colId, colChar, 10);

		strcat(rowChar, "|");
		strcat(rowChar, colChar);

		string mapString = rowChar;

		if (strcmp(mapString.c_str(), "") != 0)
			cubeMap[mapString] = cubeObj;

		gameObjs.push_back(cubeObj);
	}


	for (unsigned int i = 0; i < crestVector.size(); i++)
	{
		GameObject* crestObj = new Crest("medusacrest", "medusacrest", physicsMan->createRigidBody("Cube", crestVector[i]->centerX + xPos, 1.5f, crestVector[i]->centerZ + zPos, 0.0f), physicsMan, crestVector[i]->effect, 0.0f);
		//GameObject* crestObj = new Crest("medusacrest", Crest::GetCrestTypeString(crestVector[i]->effect), physicsMan->createRigidBody("Cube", crestVector[i]->centerX + xPos, 1.5f, crestVector[i]->centerZ + zPos, 0.0f), physicsMan, crestVector[i]->effect, 0.0f);
		//crestObj->scale(crestVector[i]->xLength,1.0,crestVector[i]->zLength);
		crestObj->rotate(0.0f, 3.14f, 0.0f);
		crestObj->translate(0.0f, 0.6f, 0.0f);

		dynamic_cast<Crest*>(crestObj)->SetTargetObject(cubeMap[crestVector[i]->target]);

		gameObjs.push_back(crestObj);
	}

	GameObject* floorObj = new GameObject("Cube", "Test Wood", physicsMan->createRigidBody("Cube", xPos + (width / 2), -0.5f, zPos + (depth / 2)), physicsMan, WORLD);
	floorObj->scale(width, 1.0f, depth);
	gameObjs.push_back(floorObj);

	for (unsigned int i = 0; i < wallRowCol.size(); ++i)
		for (unsigned int j = 0; j < wallRowCol[i].size(); ++j)
			delete wallRowCol[i][j];

	#pragma endregion
}

void Room::loadNeighbors(vector<Room*> loadedRooms)
{
	// Clear neighbors
	neighbors.clear();

	Room* loadedRoom;

	// Check exits
	for (unsigned int i = 0; i < exitVector.size(); i++)
	{
		bool isLoaded = false;

		for (unsigned int j = 0; j < loadedRooms.size(); j++)
		{
			if (strcmp(loadedRooms[j]->getFile(), exitVector[i]->file.c_str()) == 0)
			{
				isLoaded = true;
				loadedRoom = loadedRooms[j];
			}
		}

		if (!isLoaded)
		{
			float offsetX = x, offsetZ = z;

			Room* tmpRoom = new Room(exitVector[i]->file.c_str(), physicsMan, 0, 0); 
			Wall* roomEntrance;

			for (unsigned int j = 0; j < tmpRoom->getExits().size(); j++)
			{
				if (tmpRoom->getExits()[j]->file == mapFile)
					roomEntrance = tmpRoom->getExits()[j];
			}

			if (exitVector[i]->row == 0)
			{
				offsetX -= (roomEntrance->centerX - exitVector[i]->centerX);
				offsetZ -= tmpRoom->depth;
			}

			if (exitVector[i]->row == depth - 1)
			{
				offsetX -= (roomEntrance->centerX - exitVector[i]->centerX);
				offsetZ += depth;
			}

			if (exitVector[i]->col == 0)
			{
				offsetX -= tmpRoom->width;
				offsetZ -= (roomEntrance->centerZ - exitVector[i]->centerZ);
			}

			if (exitVector[i]->col == width - 1)
			{
				offsetX += width;
				offsetZ -= (roomEntrance->centerZ - exitVector[i]->centerZ);
			}

			tmpRoom->setX(offsetX);
			tmpRoom->setZ(offsetZ);

			tmpRoom->loadRoom(offsetX, offsetZ);

			neighbors.push_back(tmpRoom);
		}

		else
			neighbors.push_back(loadedRoom);
	}
}