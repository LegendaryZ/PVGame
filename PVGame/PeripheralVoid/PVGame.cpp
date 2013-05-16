#include "PVGame.h"

map<string, MeshData>MeshMaps::MESH_MAPS = MeshMaps::create_map();

PVGame::PVGame(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
	VOLUME = 10;
	MOUSESENSITIVITY = 32;
	FULLSCREEN = false;
	OCULUS = false;
	VSYNC = true;
	LOOKINVERSION = false;

	input = new Input();
	gameState = MENU;
}

PVGame::~PVGame(void)
{
	delete player;
	
	for (unsigned int i = 0; i < proceduralGameObjects.size(); ++i)
	{
		delete proceduralGameObjects[i];
	}

	gameObjects.clear();
	proceduralGameObjects.clear();
	
	ClearRooms();	

	alcDestroyContext(audioContext);
    alcCloseDevice(audioDevice);
	delete physicsMan;
	delete audioSource;
	delete audioWin;
	//OCULUS RIFT
	delete riftMan;
}

bool PVGame::Init(char * args)
{
	//OCULUS RIFT
	riftMan = new RiftManager(args);
	renderMan->SetRiftMan(riftMan);

	if (!D3DApp::Init())
		return false;

	renderMan->BuildBuffers();
	renderMan->SetRiftMan(riftMan);

	BuildFX();
	BuildVertexLayout();
	
	renderMan->LoadTexture("Loading Screen", "Textures/LoadingScreen.dds", "Diffuse");
	renderMan->DrawLoadingScreen();
	renderMan->EndDrawMenu();

	audioDevice=alcOpenDevice(NULL);
    if(audioDevice==NULL)
	{
		DBOUT("cannot open sound card");
		return 0;
	}
	audioContext=alcCreateContext(audioDevice,NULL);
	if(audioContext==NULL)
	{
		DBOUT("cannot open context");
		return 0;
	}
	alcMakeContextCurrent(audioContext);
    
	selector = 0;

	SELECTOR_MAP[MENU]			=  5;
	SELECTOR_MAP[OPTION]		=  7;
	SELECTOR_MAP[INSTRUCTIONS]	=  3;
	SELECTOR_MAP[END]			=  2;

	const enum GAME_STATE { MENU, OPTION, PLAYING, END, INSTRUCTIONS };

	physicsMan = new PhysicsManager();
	player = new Player(physicsMan, renderMan, riftMan);
	
	//Test load a cube.obj
	//renderMan->LoadFile(L"crest.obj", "crest");
	renderMan->LoadFile(L"medusacrest.obj", "medusacrest");
	renderMan->LoadFile(L"unlockcrest.obj", "unlockcrest");
	//renderMan->LoadFile(L"caelhammer.obj", "caelhammer");
	renderMan->LoadFile(L"boat.obj", "boat");

	renderMan->BuildBuffers();
	renderMan->SetRiftMan(riftMan);

	//Cook Rigid Bodies from the meshes
	map<string, MeshData>::const_iterator itr;
	for(itr = MeshMaps::MESH_MAPS.begin(); itr != MeshMaps::MESH_MAPS.end(); itr++)
		physicsMan->addTriangleMesh((*itr).first, (*itr).second);
	
	LoadContent();

	#if DEV_MODE
	devMode = true;
	#endif
	#if !DEV_MODE
	devMode = false;
	#endif
	//OnResize();

	#if DRAW_FRUSTUM
	gameObjects.push_back(player->GetCamera()->frustumBody);
	proceduralGameObjects.push_back(player->GetCamera()->frustumBody);
	#endif

	mPhi = 1.5f*MathHelper::Pi;
	mTheta = 0.25f*MathHelper::Pi;
	mLastMousePos.x = 0;
	mLastMousePos.y = 0;

	SortGameObjects();

	renderMan->BuildInstancedBuffer(gameObjects);

	audioSource = new AudioSource();
	audioSource->initialize("Audio\\HomeSweetHome.wav", AudioSource::WAV);
	audioWin = new AudioSource();
	audioWin->initialize("Audio\\test_mono_8000Hz_8bit_PCM.wav", AudioSource::WAV);

	ReadOptions();
	ApplyOptions();

	return true;
}

bool PVGame::LoadContent()
{	
	renderMan->LoadContent();
	LoadXML();
	return true;
}

bool PVGame::LoadXML()
{
	Room* startRoom = new Room(MAP_LEVEL_1, physicsMan, 0, 0);
	startRoom->loadRoom();
	currentRoom = startRoom;

	tinyxml2::XMLDocument doc;

	#pragma region Textures
	doc.LoadFile(TEXTURES_FILE);
	for (XMLElement* atlas = doc.FirstChildElement("TextureList")->FirstChildElement("Atlas"); 
				atlas != NULL; atlas = atlas->NextSiblingElement("Atlas"))
	{
		// Tells renderman to load the texture and save it in a map.
		renderMan->LoadTexture(atlas->Attribute("name"), atlas->Attribute("filename"),
			atlas->Attribute("type"));

		for (XMLElement* texture = atlas->FirstChildElement("Texture"); 
			texture != NULL; texture = texture->NextSiblingElement("Texture"))
		{
			XMFLOAT2 aCoord((float)atof(texture->FirstChildElement("OffsetU")->FirstChild()->Value()), 
							(float)atof(texture->FirstChildElement("OffsetV")->FirstChild()->Value()));
			renderMan->LoadTextureAtlasCoord(texture->Attribute("name"), aCoord);
		}
	}

	// Explictitly load menu background texture for now.
	renderMan->LoadTexture("Menu Background", "Textures/MenuBackground.dds", "Diffuse");
	
	#pragma endregion

	#pragma region Materials
	doc.LoadFile(MATERIALS_FILE);
	for (XMLElement* material = doc.FirstChildElement("MaterialsList")->FirstChildElement("Material"); 
				material != NULL; material = material->NextSiblingElement("Material"))
	{
		// Loop through all the materials, setting appropriate attributes.
		GameMaterial aMaterial;
		aMaterial.Name = material->Attribute("name");
		aMaterial.SurfaceKey = material->FirstChildElement("SurfaceMaterial")->FirstChild()->Value();
		aMaterial.DiffuseKey = material->FirstChildElement("DiffuseMap")->FirstChild()->Value();

		XMLElement* glow = material->FirstChildElement("Glow");
		if (glow)
			aMaterial.GlowColor = XMFLOAT4((float)atof(glow->Attribute("r")), (float)atof(glow->Attribute("g")), 
									 (float)atof(glow->Attribute("b")), (float)atof(glow->Attribute("a")));
		GAME_MATERIALS[aMaterial.Name] = aMaterial;
	}
	#pragma endregion

	#pragma region Surface Materials
	doc.LoadFile(SURFACE_MATERIALS_FILE);
	for (XMLElement* surfaceMaterial = doc.FirstChildElement("SurfaceMaterialsList")->FirstChildElement("SurfaceMaterial"); 
				surfaceMaterial != NULL; surfaceMaterial = surfaceMaterial->NextSiblingElement("SurfaceMaterial"))
	{
		// Gets values for surface material from xml.
		XMLElement* ambient = surfaceMaterial->FirstChildElement("Ambient");
		XMLElement* diffuse = surfaceMaterial->FirstChildElement("Diffuse");
		XMLElement* specular = surfaceMaterial->FirstChildElement("Specular");
		XMLElement* reflect = surfaceMaterial->FirstChildElement("Reflect");
		
		SurfaceMaterial aMaterial;
		aMaterial.Ambient = XMFLOAT4((float)atof(ambient->Attribute("r")), (float)atof(ambient->Attribute("g")), 
									 (float)atof(ambient->Attribute("b")), (float)atof(ambient->Attribute("a")));
		aMaterial.Diffuse = XMFLOAT4((float)atof(diffuse->Attribute("r")), (float)atof(diffuse->Attribute("g")), 
									 (float)atof(diffuse->Attribute("b")), (float)atof(diffuse->Attribute("a")));
		aMaterial.Specular = XMFLOAT4((float)atof(specular->Attribute("r")), (float)atof(specular->Attribute("g")), 
									 (float)atof(specular->Attribute("b")), (float)atof(specular->Attribute("a")));
		aMaterial.Reflect = XMFLOAT4((float)atof(reflect->Attribute("r")), (float)atof(reflect->Attribute("g")), 
									 (float)atof(reflect->Attribute("b")), (float)atof(reflect->Attribute("a")));
		SURFACE_MATERIALS[surfaceMaterial->Attribute("name")] = aMaterial;
	}
	#pragma endregion

	#pragma region Map Loading
	//Get the filename from constants, hand it into tinyxml
	BuildRooms(currentRoom);

	player->setPosition(currentRoom->getSpawn()->col, 2.0f, currentRoom->getSpawn()->row);
	#pragma endregion

	#pragma region Make Turrets
	/*
	GameObject* turretGOJ = new Turret("caelhammer", "Snow", physicsMan->createRigidBody("Cube", 29.0f, 0.5f, 13.0f, 0.0f), physicsMan, ALPHA);
	if(Turret* turretOJ = dynamic_cast<Turret*>(turretGOJ))
	{
		//turretOJ->CreateProjectiles(gameObjects);
	}
	
	turretGOJ->scale(1.5, 0.6, 0.6);
	turretGOJ->rotate(1.0f, 0.0f, 0.0f);
	gameObjects.push_back(turretGOJ);
	*/
	/*
	GameObject* turretGOJ2 = new Turret("Cube", "Sand", physicsMan->createRigidBody("Cube", 40.0f, 0.5f, 13.0f, 0.0f), physicsMan, BETA);
	turretGOJ2->scale(1.5, 0.6, 0.6);
	turretGOJ2->rotate(2.2f, 0.0f, 0.0f);
	gameObjects.push_back(turretGOJ2);

	GameObject* turretGOJ3 = new Turret("Cube", "Rock", physicsMan->createRigidBody("Cube", 48.0f, 0.5f, 13.0f, 0.0f), physicsMan, GAMMA);
	turretGOJ3->scale(1.5, 0.6, 0.6);
	turretGOJ3->rotate(1.77f, 0.0f, 0.0f);
	gameObjects.push_back(turretGOJ3);*/
	#pragma endregion

	SortGameObjects();

	return true;
}

void PVGame::OnResize()
{
	D3DApp::OnResize();
	player->OnResize(AspectRatio());

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	//XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	//XMStoreFloat4x4(&mProj, P);
}

#pragma region Awful use of variables courtesy of Jason
bool is1Up = true;
bool is2Up = true;
bool is8Up = true;
bool isEUp = true;
#pragma endregion
void PVGame::UpdateScene(float dt)
{
	#pragma region General Controls
	if(input->wasKeyPressed('P') || input->wasKeyPressed('p'))
	{
		ToggleDevMode();
	}

	if (input->isQuitPressed())
	{
		if(devMode)
			PostMessage(this->mhMainWnd, WM_CLOSE, 0, 0);
		else
		{
			player->resetWinPercent();
			currentRoom = loadedRooms[0];
			player->setPosition((currentRoom->getX() + currentRoom->getSpawn()->centerX), 2.0f, (currentRoom->getZ() + currentRoom->getSpawn()->centerZ));
			gameState = END;
		}
	}
	if(input->wasKeyPressed('M'))
	{
		if(player->getListener()->isMuted())
			player->getListener()->unmute();
		else
			player->getListener()->mute();
	}
	#pragma endregion
	switch(gameState)
	{
	#pragma region Menu
	case MENU:
		if(!audioSource->isPlaying())
		{
			audioSource->restartAndPlay();
		}
		ListenSelectorChange();
		break;
	#pragma endregion
	#pragma region Option
	case OPTION:
		ListenSelectorChange();
		HandleOptions();
		break;
	#pragma endregion
	#pragma region Playing
	case PLAYING:
		if(audioSource->isPlaying())
		{
			audioSource->stop();
		}
		if (input->isQuitPressed())
			PostMessage(this->mhMainWnd, WM_CLOSE, 0, 0);

		if (player->getWinPercent() >= 0.99f)
		{
			player->resetWinPercent();
			
			//currentRoom->loadNeighbors(loadedRooms);
		
			if(currentRoom->getExits().size() == 1)
			{
				currentRoom = loadedRooms[0];
				player->setPosition((currentRoom->getX() + currentRoom->getSpawn()->centerX), 2.0f, (currentRoom->getZ() + currentRoom->getSpawn()->centerZ));
				gameState = END;
			}
			else if(currentRoom->getExits().size() == 2) //Go to Next Area
			{
				//Load Last room possible
				char* map    = (char*)malloc(sizeof(char) * (currentRoom->getExits()[0]->file.length()) + 1);
				strcpy(map, currentRoom->getExits()[0]->file.c_str());
				for(int i = 0; i < currentRoom->getExits().size(); i++)
				{
					if(strcmp(map, currentRoom->getExits()[i]->file.c_str()) < 0)
						strcpy(map, currentRoom->getExits()[i]->file.c_str());
				}

				ClearRooms();	
				loadedRooms.clear();

				for (unsigned int i = 0; i < proceduralGameObjects.size(); ++i)
				{
					delete proceduralGameObjects[i];
				}

				gameObjects.clear();
				proceduralGameObjects.clear();
	
				

//				delete currentRoom;
				Room* startRoom = new Room(map, physicsMan, 0, 0);
				startRoom->loadRoom();
				currentRoom = startRoom;
				BuildRooms(currentRoom);
				
				player->setPosition((currentRoom->getX() + currentRoom->getSpawn()->centerX), 2.0f, (currentRoom->getZ() + currentRoom->getSpawn()->centerZ));
				delete[] map;
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
			}
			return;
		}

		if(input->wasKeyPressed('K'))
		{
			//Load Last room possible
				char* map    = (char*)malloc(sizeof(char) * (strlen(currentRoom->getNeighbors()[0]->getMapFile())) + 1);
				strcpy(map, currentRoom->getNeighbors()[0]->getMapFile());
				for(int i = 0; i < currentRoom->getNumNeighbors(); i++)
				{
					if(strcmp(map, currentRoom->getNeighbors()[i]->getMapFile()) < 0)
						strcpy(map, currentRoom->getNeighbors()[i]->getMapFile());
				}

				ClearRooms();	
				loadedRooms.clear();

				for (unsigned int i = 0; i < proceduralGameObjects.size(); ++i)
				{
					delete proceduralGameObjects[i];
				}

				gameObjects.clear();
				proceduralGameObjects.clear();
	
				

//				delete currentRoom;
				Room* startRoom = new Room(map, physicsMan, 0, 0);
				startRoom->loadRoom();
				currentRoom = startRoom;
				BuildRooms(currentRoom);
				
				player->setPosition((currentRoom->getX() + currentRoom->getSpawn()->centerX), 2.0f, (currentRoom->getZ() + currentRoom->getSpawn()->centerZ));
				delete[] map;
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
		}

		player->Update(dt, input);
		#pragma region Player Wireframe and blur controls
		if(devMode)
		{
			if (input->wasKeyPressed('R'))
				renderMan->AddPostProcessingEffect(WireframeEffect);
			if (input->wasKeyPressed('N'))
				renderMan->RemovePostProcessingEffect(WireframeEffect);

			if (input->wasKeyPressed('B'))
				renderMan->AddPostProcessingEffect(BlurEffect);
			if (input->wasKeyPressed('V'))
				renderMan->RemovePostProcessingEffect(BlurEffect);

			// Brackets.
			if (input->wasKeyPressed(VK_OEM_6))
				renderMan->ChangeBlurCount(1);
			if (input->wasKeyPressed(VK_OEM_4))
				renderMan->ChangeBlurCount(-1);
		}

		if ( /*riftMan->isRiftConnected() &&*/  input->isOculusButtonPressed())
		{
			renderMan->ToggleOculusEffect();
			riftMan->setUsingRift(!riftMan->isUsingRift());
			player->OnResize(renderMan->AspectRatio());
		}

		#pragma endregion
		#pragma region Physics for Worlds Game Objects
		// If physics updated, tell the game objects to update their world matrix.
		if (physicsMan->update(dt))
		{
			for (unsigned int i = 0; i < gameObjects.size(); ++i)
			{
				gameObjects[i]->Update();
				/* //Should delete objects below -20. Doesn't work 'well' or 'at all'
				if(gameObjects[i]->getRigidBody()->getWorldTransform().getOrigin().getY() < -20)
				{
				gameObjects.erase(gameObjects.begin() += i);

				SortGameObjects();
				}*/
			}
		}
		#pragma endregion

		#if USE_FRUSTUM_CULLING
			player->GetCamera()->frustumCull();
		#endif

		#pragma region Player Room Tracking and Resetting to Checkpoints
		for (unsigned int i = 0; i < loadedRooms.size(); i++)
		{
			if ((player->getPosition().x > loadedRooms[i]->getX()) && (player->getPosition().x < (loadedRooms[i]->getX() + loadedRooms[i]->getWidth())) &&
				(player->getPosition().z > loadedRooms[i]->getZ()) && (player->getPosition().z < (loadedRooms[i]->getZ() + loadedRooms[i]->getDepth())))
			{
					currentRoom = loadedRooms[i];
					break;
			}
		}
		//If the player falls of the edge of the world, respawn in current room
		if(devMode)
		{
			if (player->getPosition().y < -100)
			{
				player->setPosition((currentRoom->getX() + currentRoom->getSpawn()->centerX), 2.0f, (currentRoom->getZ() + currentRoom->getSpawn()->centerZ));
				if(currentRoom->getSpawn()->direction.compare("up") == 0)
					player->setRotation(3.14f/2.0f);
				else if(currentRoom->getSpawn()->direction.compare("left") == 0)
					player->setRotation(3.14f);
				else if(currentRoom->getSpawn()->direction.compare("down") == 0)
					player->setRotation((3.0f*3.14f)/2.0f);
				else if(currentRoom->getSpawn()->direction.compare("right") == 0)
					player->setRotation(3.14f *2.0f);
			}
		}
		else
		{
			if (player->getPosition().y < -5)
			{
				player->setPosition((currentRoom->getX() + currentRoom->getSpawn()->centerX), 2.0f, (currentRoom->getZ() + currentRoom->getSpawn()->centerZ));
				if(currentRoom->getSpawn()->direction.compare("up") == 0)
					player->setRotation(3.14f/2.0f);
				else if(currentRoom->getSpawn()->direction.compare("left") == 0)
					player->setRotation(3.14f);
				else if(currentRoom->getSpawn()->direction.compare("down") == 0)
					player->setRotation((3*3.14f)/2.0f);
				else if(currentRoom->getSpawn()->direction.compare("right") == 0)
					player->setRotation(3.14f *2.0f);
			}
		}
			
		#pragma endregion
		#pragma region Player Statuses and Vision Affected Object Updating
		if(!player->getWinStatus())
		{
			if(audioWin->isPlaying())
				audioWin->stop();
		}
		player->resetStatuses();

		// Reset blur, we only do it if a single Medusa is in sight.
		renderMan->RemovePostProcessingEffect(BlurEffect);

		for(unsigned int i = 0; i < gameObjects.size(); i++)
		{
			if(gameObjects[i]->GetVisionAffected())
			{	
				//If it is a crest
					//And if it is colliding
				if(MovingObject* currentMovObj = dynamic_cast<MovingObject*>(gameObjects[i]))
				{
					currentMovObj->Update();
				}

				if(Crest* currentCrest = dynamic_cast<Crest*>(gameObjects[i]))
				{
					//btVector3 crestPos = gameObjects[i]->getRigidBody()->getCenterOfMassPosition();
					
					if(physicsMan->broadPhase(player->GetCamera(), gameObjects[i]) && physicsMan->narrowPhase(player->GetCamera(), gameObjects[i]))
					{
						currentCrest->ChangeView(true);

						// For now, only Medusa causes blur effect.
						if (currentCrest->GetCrestType() == MEDUSA && player->getController()->onGround())
						{
							renderMan->SetBlurColor(XMFLOAT4(0.0f, 0.25f, 0.0f, 1.0f));
							renderMan->AddPostProcessingEffect(BlurEffect);
						}

						if (currentCrest->GetCrestType() == WIN)
						{
							audioWin->setPosition(player->getPosition().x, player->getPosition().y, player->getPosition().z);
							if(!audioWin->isPlaying())
								audioWin->play();
							renderMan->SetBlurColor(XMFLOAT4(0.99f * player->getWinPercent(), 0.99f * player->getWinPercent(), 0.0f, 1.0f));
							renderMan->AddPostProcessingEffect(BlurEffect);
						}

					}
					else
					{
						currentCrest->ChangeView(false);

						// If Medusa is out of sight, remove blur. Overrides manual blur add - comment out to require manual toggle on/off.
						if (currentCrest->GetCrestType() == MEDUSA)
						{
							//renderMan->RemovePostProcessingEffect(BlurEffect);
						}

						if (currentCrest->GetCrestType() == WIN)
						{
							//renderMan->RemovePostProcessingEffect(BlurEffect);
						}
					}
					currentCrest->Update(player);
				}

				/*if(Turret* currentTurret = dynamic_cast<Turret*>(gameObjects[i]))
				{
					//btVector3 turretPos = gameObjects[i]->getRigidBody()->getCenterOfMassPosition();

					if(physicsMan->broadPhase(player->GetCamera(), gameObjects[i]) && physicsMan->narrowPhase(player->GetCamera(), gameObjects[i]))
					{
						currentTurret->ChangeView(true);
					}
					else
					{
						currentTurret->ChangeView(false);
					}
					currentTurret->Update(player);
				}*/
			}
		}
		#pragma endregion

		if(devMode)
		{
			#pragma region Throwing Crests
			if(input->wasKeyPressed('3'))
			{
				XMFLOAT4 p = player->getPosition();
				XMFLOAT3 look = player->GetCamera()->GetLook();
				XMFLOAT3 pos(p.x + (look.x * 2),p.y + (look.y * 2),p.z + (look.z * 2));
				float speed = 15;
				GameObject* crestObj = new Crest("Cube", Crest::GetCrestTypeString(MOBILITY), physicsMan->createRigidBody("Cube", pos.x, pos.y, pos.z, 1.0f), physicsMan, MOBILITY, 1.0f);
				crestObj->setLinearVelocity(look.x * speed, look.y * speed, look.z * speed);
				crestObj->SetTexScale(0.0f, 0.0f, 0.0f, 0.0f);
				gameObjects.push_back(crestObj);
				proceduralGameObjects.push_back(crestObj);
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
			}
			if(input->wasKeyPressed('4'))
			{
				XMFLOAT4 p = player->getPosition();
				XMFLOAT3 look = player->GetCamera()->GetLook();
				XMFLOAT3 pos(p.x + (look.x * 2),p.y + (look.y * 2),p.z + (look.z * 2));
				float speed = 15;
				GameObject* crestObj = new Crest("Cube", Crest::GetCrestTypeString(LEAP), physicsMan->createRigidBody("Cube", pos.x, pos.y, pos.z, 1.0f), physicsMan, LEAP, 1.0f);
				crestObj->setLinearVelocity(look.x * speed, look.y * speed, look.z * speed);
				gameObjects.push_back(crestObj);
				proceduralGameObjects.push_back(crestObj);
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
			}
			if(input->wasKeyPressed('5'))
			{
				XMFLOAT4 p = player->getPosition();
				XMFLOAT3 look = player->GetCamera()->GetLook();
				XMFLOAT3 pos(p.x + (look.x * 2),p.y + (look.y * 2),p.z + (look.z * 2));
				float speed = 15;
				GameObject* crestObj = new Crest("Cube", "Brick", physicsMan->createRigidBody("Cube", pos.x, pos.y, pos.z, 1.0f), physicsMan, HADES, 0.0f);
				crestObj->scale(2.0f, .1f, 2.0f);
				crestObj->SetTexScale(2.0f, 2.0f, 0.0f, 1.0f);
				crestObj->setLinearVelocity(look.x * speed, look.y * speed, look.z * speed);
				gameObjects.push_back(crestObj);
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
			}
			if(input->wasKeyPressed('9'))
			{
				XMFLOAT4 p = player->getPosition();
				XMFLOAT3 look = player->GetCamera()->GetLook();
				XMFLOAT3 pos(p.x + (look.x * 2),p.y + (look.y * 2),p.z + (look.z * 2));
				float speed = 15;
				GameObject* crestObj = new Crest("Cube", Crest::GetCrestTypeString(MEDUSA), physicsMan->createRigidBody("Cube", pos.x, pos.y, pos.z, 1.0f), physicsMan, MEDUSA, 1.0f);
				crestObj->setLinearVelocity(look.x * speed, look.y * speed, look.z * speed);
				crestObj->SetTexScale(0.0f, 0.0f, 0.0f, 0.0f);
				gameObjects.push_back(crestObj);
				proceduralGameObjects.push_back(crestObj);
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
			}
			#pragma endregion

			#pragma region 1: Slow Sphere
			if((input->wasKeyPressed('1') || input->getGamepadLeftTrigger(0)) && is1Up)
			{
				XMFLOAT4 p = player->getPosition();
				XMFLOAT3 look = player->GetCamera()->GetLook();
				XMFLOAT3 pos(p.x + (look.x * 2),p.y + (look.y * 2),p.z + (look.z * 2));
				float speed = 10;

				GameObject* testSphere = new GameObject("Sphere", "Wood", physicsMan->createRigidBody("Sphere", pos.x, pos.y, pos.z, 0.3f, 0.3f, 0.3f, 1.0f), physicsMan, WORLD, 1.0f);
				testSphere->setLinearVelocity(look.x * speed, look.y * speed, look.z * speed);
				testSphere->SetTexScale(0.0f, 0.0f, 0.0f, 0.0f);
				testSphere->initAudio("Audio\\shot.wav");
			
				testSphere->playAudio();
				gameObjects.push_back(testSphere);
				proceduralGameObjects.push_back(testSphere);
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
				is1Up = false;
			}
			else if(!input->isKeyDown('1') && !input->getGamepadLeftTrigger(0))
				is1Up = true;
			#pragma endregion
			#pragma region 2: Slow Cube
			if((input->wasKeyPressed('2') || input->getGamepadRightTrigger(0))  && is2Up)
			{
				XMFLOAT4 p = player->getPosition();
				XMFLOAT3 look = player->GetCamera()->GetLook();
				XMFLOAT3 pos(p.x + (look.x * 2),p.y + (look.y * 2),p.z + (look.z * 2));
				float speed = 20;

				GameObject* testSphere = new GameObject("Cube", "Wood", physicsMan->createRigidBody("Cube", pos.x, pos.y, pos.z, 1.0), physicsMan, WORLD, 1.0);
				testSphere->SetTexScale(0.0f, 0.0f, 0.0f, 0.0f);
				testSphere->setLinearVelocity(look.x * speed, look.y * speed, look.z * speed);
				testSphere->initAudio("Audio\\test_mono_8000Hz_8bit_PCM.wav");
				//testSphere->playAudio();
				gameObjects.push_back(testSphere);
				proceduralGameObjects.push_back(testSphere);
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
				is2Up = false;
			}
			else if(!input->isKeyDown('2') && !input->getGamepadRightTrigger(0))
				is2Up = true;
			#pragma endregion
			#pragma region 8: Speedless Cube
			if((input->wasKeyPressed('8'))  && is8Up)
			{
				XMFLOAT4 p = player->getPosition();
				XMFLOAT3 look = player->GetCamera()->GetLook();
				XMFLOAT3 pos(p.x + (look.x * 2),p.y + (look.y * 2),p.z + (look.z * 2));
				float speed = 0;

				GameObject* testSphere = new GameObject("Cube", "Wood", physicsMan->createRigidBody("Cube", pos.x, pos.y, pos.z, 1.0), physicsMan, WORLD, 1.0);
				testSphere->SetTexScale(0.0f, 0.0f, 0.0f, 0.0f);
				testSphere->setLinearVelocity(look.x * speed, look.y * speed, look.z * speed);
				testSphere->initAudio("Audio\\test_mono_8000Hz_8bit_PCM.wav");
				//testSphere->playAudio();
				gameObjects.push_back(testSphere);
				proceduralGameObjects.push_back(testSphere);
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
				is8Up = false;
			}
			else if(!input->isKeyDown('8'))
				is8Up = true;
			#pragma endregion
			#pragma region E: Fast Sphere
			if((input->wasKeyPressed('E'))  && isEUp)
			{
				XMFLOAT4 p = player->getPosition();
				XMFLOAT3 look = player->GetCamera()->GetLook();
				XMFLOAT3 pos(p.x + (look.x * 2),p.y + (look.y * 2),p.z + (look.z * 2));
				float speed = 90;

				GameObject* testSphere = new GameObject("Sphere", "Wood", physicsMan->createRigidBody("Sphere", pos.x, pos.y, pos.z, 0.3f, 0.3f, 0.3f, 90.0f), physicsMan, WORLD, 90.0f);
				testSphere->SetTexScale(0.0f, 0.0f, 0.0f, 0.0f);
				testSphere->setLinearVelocity(look.x * speed, look.y * speed, look.z * speed);
				testSphere->initAudio("Audio\\shot.wav");
				testSphere->playAudio();
				gameObjects.push_back(testSphere);
				proceduralGameObjects.push_back(testSphere);
				SortGameObjects();
				renderMan->BuildInstancedBuffer(gameObjects);
				isEUp = false;
			}
			else if(!input->isKeyDown('E'))
				isEUp = true;
			#pragma endregion

			#pragma region Level Controls U and I
			if (input->wasKeyPressed('U'))
			{
				for (unsigned int i = 0; i < loadedRooms.size(); i++)
				{
					if (strcmp(currentRoom->getFile(), loadedRooms[i]->getFile()) == 0)
					{
						if (i > 0)
							currentRoom = loadedRooms[i - 1];
						else
							currentRoom = loadedRooms[loadedRooms.size() - 1];

						player->setPosition((currentRoom->getX() + currentRoom->getSpawn()->centerX), 2.0f, (currentRoom->getZ() + currentRoom->getSpawn()->centerZ));
						break;
					}
				}
			}
			if (input->wasKeyPressed('I'))
			{
				for (unsigned int i = 0; i < loadedRooms.size(); i++)
				{
					if (strcmp(currentRoom->getFile(), loadedRooms[i]->getFile()) == 0)
					{
						if (i < (loadedRooms.size() - 1))
							currentRoom = loadedRooms[i + 1];
						else
							currentRoom = loadedRooms[0];

						player->setPosition((currentRoom->getX() + currentRoom->getSpawn()->centerX), 2.0f, (currentRoom->getZ() + currentRoom->getSpawn()->centerZ));
						break;
					}
				}
			}
			#pragma endregion
		}
		#if USE_FRUSTUM_CULLING
		player->GetCamera()->frustumCull();
		#endif
		break;
	#pragma endregion
	#pragma region End
	case END:
		ListenSelectorChange();
		break;
	#pragma endregion
	#pragma region Instructions
	case INSTRUCTIONS:
		ListenSelectorChange();
		break;
	#pragma endregion
	default:
		break;
	}
}

void PVGame::ListenSelectorChange()
{
	#pragma region Move Selector Up//Down
	if(input->wasMenuDownPressed())
	{
		if(selector >= SELECTOR_MAP[gameState] - 1)
		{
			selector = 0;
		}
		else
		{
			selector++;
		}
	}
	if(input->wasMenuUpPressed())
	{
		if(selector <= 0)
		{
			selector = SELECTOR_MAP[gameState] - 1;
		}
		else
		{
			selector--;
		}
	}
	#pragma endregion
	#pragma region Select Current Option
	if(input->wasMenuSelectPressed())
	{
		#pragma region MENU
		if(gameState == MENU)
		{
			//Play
			if(selector == 0)
			{
				ShowCursor(false);
				gameState = PLAYING;
				return;
			}
			//Instructions
			if(selector == 1)
			{
				selector = 0;
				gameState = INSTRUCTIONS;
				return;
			}
			//Options
			if(selector == 2)
			{
				selector = 0;
				gameState = OPTION;
				return;
			}
			//Credits
			if(selector == 3)
			{
				selector = 0;
				gameState = END;
				return;
			}
			//End
			if(selector == 4)
			{
				PostMessage(this->mhMainWnd, WM_CLOSE, 0, 0);
				return;
			}
		}
		#pragma endregion
		#pragma region OPTION
		if(gameState == OPTION)
		{
			//Option 1
			if(selector == 0)
			{

				return;
			}
			//Option 2
			if(selector == 1)
			{

				return;
			}
			//Option 3
			if(selector == 2)
			{

				return;
			}
			//Option 3
			if(selector == 3)
			{

				return;
			}
			//Option 4
			if(selector == 4)
			{

				return;
			}
			//Option 5
			if(selector == 5)
			{

				return;
			}
			//Menu
			if(selector == 6)
			{
				selector = 2;
				WriteOptions();
				ReadOptions();
				ApplyOptions();
				gameState = MENU;
				return;
			}
		}
		#pragma endregion
		#pragma region END
		if(gameState == END)
		{
			//if(selector == 0 || selector == 1)
			//{
				if(audioWin->isPlaying())
				{
					audioWin->stop();
				}
				selector = 3;
				gameState = MENU;
				return;
			//}
		}
		#pragma endregion
		#pragma region INSTRUCTIONS
		if(gameState == INSTRUCTIONS)
		{
			//NEXT
			if(selector == 0)
			{
				
				return;
			}
			//PLAY
			if(selector == 1)
			{
				ShowCursor(false);
				gameState = PLAYING;
				return;
			}
			//MENU
			if(selector == 2)
			{
				selector = 1;
				gameState = MENU;
				return;
			}
		}
		#pragma endregion
	}
	#pragma endregion
}

void PVGame::HandleOptions()
{
	if(input->wasMenuLeftKeyPressed())
	{
		switch(selector)
		{
		case 0:
			VOLUME--;
			if(VOLUME < 0)
				VOLUME = 0;
			break;
		case 1:
			FULLSCREEN = !FULLSCREEN;
			break;
		case 2:
			OCULUS = !OCULUS;
			break;
		case 3:
			VSYNC = !VSYNC;
			break;
		case 4:
			MOUSESENSITIVITY--;
			if(MOUSESENSITIVITY < 1)
				MOUSESENSITIVITY = 1;
			break;
		case 5:
			LOOKINVERSION = !LOOKINVERSION;
			break;
		}
	}
	
	if(input->wasMenuRightKeyPressed())
	{
		switch(selector)
		{
		case 0:
			VOLUME++;
			if(VOLUME > 10)
				VOLUME = 10;
			break;
		case 1:
			FULLSCREEN = !FULLSCREEN;
			break;
		case 2:
			OCULUS = !OCULUS;
			break;
		case 3:
			VSYNC = !VSYNC;
			break;
		case 4:
			MOUSESENSITIVITY++;
			if(MOUSESENSITIVITY > 100)
				MOUSESENSITIVITY = 100;
			break;
		case 5:
			LOOKINVERSION = !LOOKINVERSION;
			break;
		}
	}
}

////////////////////////////////////////////////////////////
// WriteOptions()
//
// Overwrites the current options.xml file with Options in
// their current state or creates a new options.xml if one
// is not found.
////////////////////////////////////////////////////////////
void PVGame::WriteOptions()
{
	tinyxml2::XMLDocument doc;
	if(doc.LoadFile(OPTIONS_FILE) != XML_NO_ERROR)
	{
		XMLElement* options = doc.NewElement("options");
		options->SetAttribute("volume",(double)VOLUME);
		options->SetAttribute("mousesensitivity", (double)MOUSESENSITIVITY);
		options->SetAttribute("fullscreen", FULLSCREEN);
		options->SetAttribute("oculus", OCULUS);
		options->SetAttribute("vsync", VSYNC);
		options->SetAttribute("lookinversion", LOOKINVERSION);
		doc.InsertFirstChild(options);
	}
	else
	{
		XMLElement* options = doc.FirstChildElement("options");

		options->SetAttribute("volume",(double)VOLUME);
		options->SetAttribute("mousesensitivity", (double)MOUSESENSITIVITY);
		options->SetAttribute("fullscreen", FULLSCREEN);
		options->SetAttribute("oculus", OCULUS);
		options->SetAttribute("vsync", VSYNC);
		options->SetAttribute("lookinversion", LOOKINVERSION);
	}

	doc.SaveFile(OPTIONS_FILE);
}

////////////////////////////////////////////////////////
// ReadOptions()
//
// Sets the Options to the value stored in options.xml
// Will use default values and rewrite options.xml with default
// values if it is corrupted.
////////////////////////////////////////////////////////////
void PVGame::ReadOptions()
{
	tinyxml2::XMLDocument doc;
	while(doc.LoadFile(OPTIONS_FILE) != XML_NO_ERROR)
	{
		WriteOptions();
	}
	XMLElement* options = doc.FirstChildElement("options");

	if(!options)
	{
		WriteOptions();
		options = doc.FirstChildElement("options");
	}

	if(options->Attribute("volume") == NULL)
	{
		VOLUME = 10;
		WriteOptions();
	}
	else
		VOLUME = (float)atof(options->Attribute("volume"));

	if(options->Attribute("mousesensitivity") == NULL)
	{
		MOUSESENSITIVITY = 32;
		WriteOptions();
	}
	else
		MOUSESENSITIVITY = (float)atof(options->Attribute("mousesensitivity"));

	if(options->Attribute("fullscreen") == NULL)
	{
		FULLSCREEN = false;
		WriteOptions();
	}
	else
		FULLSCREEN = atoi(options->Attribute("fullscreen")) != 0; // Fix warning C4800 by converting to bool this way.

	if(options->Attribute("oculus") == NULL)
	{
		OCULUS = false;
		WriteOptions();
	}
	else
		OCULUS = atoi(options->Attribute("oculus")) != 0; // Fix warning C4800 by converting to bool this way.
		
	if(options->Attribute("vsync") == NULL)
	{
		VSYNC = true;
		WriteOptions();
	}
	else
		VSYNC = atoi(options->Attribute("vsync")) != 0; // Fix warning C4800 by converting to bool this way.

	if(options->Attribute("lookinversion") == NULL)
	{
		LOOKINVERSION = false;
		WriteOptions();
	}
	else
		LOOKINVERSION = atoi(options->Attribute("lookinversion")) != 0; // Fix warning C4800 by converting to bool this way.
}

///////////////////////////////////////////////////
// ApplyOptions()
//
// Applies the Options in their current state to
// the game where applicable
///////////////////////////////////////////////////
void PVGame::ApplyOptions()
{
	player->getListener()->setGain(((float)VOLUME/10.0f));
	player->setMouseSensitivity((float)MOUSESENSITIVITY);
	player->setInverted(LOOKINVERSION);
	renderMan->setVSYNC(VSYNC);
	renderMan->setFullScreen(FULLSCREEN);

	if((renderMan->hasOculusEffect() && !OCULUS) || (!renderMan->hasOculusEffect() && OCULUS))
	{
		renderMan->ToggleOculusEffect();
		riftMan->setUsingRift(OCULUS);
		player->OnResize(renderMan->AspectRatio());
	}
}

void PVGame::OnMouseMove(WPARAM btnState, int x, int y)
{
	// Make each pixel correspond to a quarter of a degree.
	float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
	float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

	// Update angles based on input to orbit camera around box.
	mTheta += dx;
	mPhi   += dy;

	mPhi = MathHelper::Clamp(dy, -0.10f * MathHelper::Pi, 0.05f * MathHelper::Pi);
	
	//player->GetCamera()->Pitch(mPhi); // Rotate the camera  up/down.
	//player->GetCamera()->RotateY(dx / 2); // Rotate ABOUT the y-axis. So really turning left/right.
	//player->GetCamera()->UpdateViewMatrix();

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void PVGame::DrawScene()
{
	float cWidth = (float)renderMan->GetClientHeight();
	float cHeight = (float)renderMan->GetClientWidth();
	UINT32 color1 = 0xff0000ff;
	UINT32 color2 = 0xffff0000;
	UINT32 color3 = 0xff00ff00;
	UINT32 color4 = 0xff000000;
	float lgSize = .10f * cHeight;
	float xmdSize = .05f * cHeight;
	float medSize = .04f * cHeight;
	float smlSize = .03f * cHeight;
	float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float colortwo[4] = {0.2f, 0.7f, 0.7f, 1.0f};
	float colorthree[4] = {0.4f, 0.3f, 0.1f, 1.0f};
	std::string cStats = "cHeight: " + (int)cHeight;
	
	switch(gameState)
	{
	#pragma region MENU
	case MENU:
		//renderMan->ClearTargetToColor(); //Colors::Silver reinterpret_cast<const float*>(&Colors::Silver)
		renderMan->DrawMenuBackground();
		renderMan->DrawString("P", lgSize, cWidth * .20f, cHeight / 10, color1);
		renderMan->DrawString("   eripheral Voi", lgSize, cWidth * .175f, cHeight / 10, color4);
		renderMan->DrawString("                       d", lgSize, cWidth * .185f, cHeight / 10, color3);
		renderMan->DrawString("By Entire Team is Babies", medSize, cWidth * .20f, cHeight * .25f, color1);
		if(selector == 0)
		{
			renderMan->DrawString(">Play", smlSize, cWidth * .20f, cHeight * .30f, color2);
		}
		else
			renderMan->DrawString("  Play", smlSize, cWidth * .20f, cHeight * .30f, color4);
		if(selector == 1)
		{
			renderMan->DrawString(">Instructions", smlSize, cWidth * .20f, cHeight * .35f, color2);
		}
		else
			renderMan->DrawString("  Instructions", smlSize, cWidth * .20f, cHeight * .35f, color4);
		if(selector == 2)
		{
			renderMan->DrawString(">Options", smlSize, cWidth * .20f, cHeight * .40f, color2);
		}
		else
			renderMan->DrawString("  Options", smlSize, cWidth * .20f, cHeight * .40f, color4);
		if(selector == 3)
		{
			renderMan->DrawString(">Credits", smlSize, cWidth * .20f, cHeight * .45f, color2);
		}
		else
			renderMan->DrawString("  Credits", smlSize, cWidth * .20f, cHeight * .45f, color4);
		if(selector == 4)
		{
			renderMan->DrawString(">Exit", smlSize, cWidth * .20f, cHeight * .50f, color2);
		}
		else
			renderMan->DrawString("  Exit", smlSize, cWidth * .20f, cHeight * .50f, color4);
		
		//renderMan->DrawString(L"Peripheral Void", 128.0f, 100.0f, 50.0f, 0xff0099ff);
		//renderMan->DrawString(L"By Entire Team is Babies", 65.0f, 110.0f, 200.0f, 0xff0099ff);
		renderMan->EndDrawMenu();
		break;
	#pragma endregion
	#pragma region PLAYING
	case PLAYING:
		renderMan->DrawScene(player->GetCamera(), gameObjects);
		//renderMan->DrawString("Health: 100", 24.0f, 50.0f, 50.0f, 0xff0099ff);
		//renderMan->DrawString(L"Babies:   0", 24.0f, 50.0f, 70.0f, 0xff0099ff);
		//renderMan->EndDrawMenu();
		break;
	#pragma endregion
	#pragma region OPTION
	case OPTION:
		renderMan->ClearTargetToColor(colortwo); //Colors::Silver reinterpret_cast<const float*>(&Colors::Silver)
		renderMan->DrawMenuBackground();
		renderMan->DrawString("P", lgSize, cWidth * .20f, cHeight / 10, color1);
		renderMan->DrawString("   eripheral Voi", lgSize, cWidth * .175f, cHeight / 10, color4);
		renderMan->DrawString("                       d", lgSize, cWidth * .185f, cHeight / 10, color3);
		renderMan->DrawString("Options!", medSize, cWidth * .20f, cHeight * .25f, color1);
		if(selector == 0)
		{
			renderMan->DrawString(">Volume: ", smlSize, cWidth * .20f, cHeight * .30f, color2);
			renderMan->DrawString(std::to_wstring(VOLUME).c_str(), smlSize, cWidth * .50f, cHeight * .30f, color2);
		}
		else
		{
			renderMan->DrawString("  Volume", smlSize, cWidth * .20f, cHeight * .30f, color4);
			renderMan->DrawString(std::to_wstring(VOLUME).c_str(), smlSize, cWidth * .50f, cHeight * .30f, color4);
		}
		if(selector == 1)
		{
			renderMan->DrawString(">Fullscreen: ", smlSize, cWidth * .20f, cHeight * .35f, color2);
			if(FULLSCREEN)
				renderMan->DrawString("true", smlSize, cWidth * .50f, cHeight * .35f, color2);
			else
				renderMan->DrawString("false", smlSize, cWidth * .50f, cHeight * .35f, color2);
		}
		else
		{
			renderMan->DrawString("  Fullscreen: ", smlSize, cWidth * .20f, cHeight * .35f, color4);
			if(FULLSCREEN)
				renderMan->DrawString("true", smlSize, cWidth * .50f, cHeight * .35f, color4);
			else
				renderMan->DrawString("false", smlSize, cWidth * .50f, cHeight * .35f, color4);
		}
		if(selector == 2)
		{
			renderMan->DrawString(">Oculus: ", smlSize, cWidth * .20f, cHeight * .40f, color2);
			if(OCULUS)
				renderMan->DrawString("true", smlSize, cWidth * .50f, cHeight * .40f, color2);
			else
				renderMan->DrawString("false", smlSize, cWidth * .50f, cHeight * .40f, color2);
		}
		else
		{
			renderMan->DrawString("  Oculus: ", smlSize, cWidth * .20f, cHeight * .40f, color4);
			if(OCULUS)
				renderMan->DrawString("true", smlSize, cWidth * .50f, cHeight * .40f, color4);
			else
				renderMan->DrawString("false", smlSize, cWidth * .50f, cHeight * .40f, color4);
		}
		if(selector == 3)
		{
			renderMan->DrawString(">VSync: ", smlSize, cWidth * .20f, cHeight * .45f, color2);
			if(VSYNC)
				renderMan->DrawString("true", smlSize, cWidth * .50f, cHeight * .45f, color2);
			else
				renderMan->DrawString("false", smlSize, cWidth * .50f, cHeight * .45f, color2);
		}
		else
		{
			renderMan->DrawString("  VSync: ", smlSize, cWidth * .20f, cHeight * .45f, color4);
			if(VSYNC)
				renderMan->DrawString("true", smlSize, cWidth * .50f, cHeight * .45f, color4);
			else
				renderMan->DrawString("false", smlSize, cWidth * .50f, cHeight * .45f, color4);
		}
		if(selector == 4)
		{
			renderMan->DrawString(">Mouse Sensitivity: ", smlSize, cWidth * .20f, cHeight * .50f, color2);
			renderMan->DrawString(std::to_wstring(MOUSESENSITIVITY).c_str(), smlSize, cWidth * .80f, cHeight * .50f, color2);
		}
		else
		{
			renderMan->DrawString("  Mouse Sensitivity: ", smlSize, cWidth * .20f, cHeight * .50f, color4);
			renderMan->DrawString(std::to_wstring(MOUSESENSITIVITY).c_str(), smlSize, cWidth * .80f, cHeight * .50f, color4);
		}

		if(selector == 5)
		{
			renderMan->DrawString(">Look Inversion: ", smlSize, cWidth * .20f, cHeight * .55f, color2);
			if(LOOKINVERSION)
				renderMan->DrawString("true", smlSize, cWidth * .80f, cHeight * .55f, color2);
			else
				renderMan->DrawString("false", smlSize, cWidth * .80f, cHeight * .55f, color2);
		}
		else
		{
			renderMan->DrawString("  Look Inversion: ", smlSize, cWidth * .20f, cHeight * .55f, color4);
			if(LOOKINVERSION)
				renderMan->DrawString("true", smlSize, cWidth * .80f, cHeight * .55f, color4);
			else
				renderMan->DrawString("false", smlSize, cWidth * .80f, cHeight * .55f, color4);		
		}

		if(selector == 6)
		{
			renderMan->DrawString(">Menu", smlSize, cWidth * .20f, cHeight * .60f, color2);
		}
		else
			renderMan->DrawString("  Menu", smlSize, cWidth * .20f, cHeight * .60f, color4);
		renderMan->EndDrawMenu();
		break;
	#pragma endregion
	#pragma region INSTRUCTIONS
	case INSTRUCTIONS:
		renderMan->ClearTargetToColor(color); //Colors::Silver reinterpret_cast<const float*>(&Colors::Silver)
		renderMan->DrawMenuBackground();
/*		renderMan->DrawString("P", cHeight * .10f, cWidth * .20f, cHeight / 20, color1);
		renderMan->DrawString("   eripheral Voi", cHeight * .10f, cWidth * .175f, cHeight / 20, color4);
		renderMan->DrawString("                       d", cHeight * .10f, cWidth * .185f, cHeight / 20, color3);*/
		renderMan->DrawString("P", xmdSize, cWidth * .20f, 0.0f, color1);
		renderMan->DrawString("    eripheral Voi", xmdSize, cWidth * .175f, 0.0f, color4);
		renderMan->DrawString("                        d", xmdSize, cWidth * .185f, 0.0f, color3);
		renderMan->DrawString("Instructions", medSize, cWidth * .20f, cHeight * .07f, color1);
		//If controller is connected
		if(input->gamepadConnected(0))
		{
			renderMan->DrawString("Right Analog to look!", medSize, cWidth * .20f, cHeight * .15f, color4);
			renderMan->DrawString("Left Analog to move your character around.", medSize, cWidth * .20f, cHeight * .20f, color4);
			renderMan->DrawString("Press A to jump.", medSize, cWidth * .20f, cHeight * .25f, color4);
		}
		else
		{
			renderMan->DrawString("Use the mouse to look!", medSize, cWidth * .20f, cHeight * .15f, color4);
			renderMan->DrawString("Use WASD to move your character around.", medSize, cWidth * .20f, cHeight * .20f, color4);
			renderMan->DrawString("Press Spacebar to jump.", medSize, cWidth * .20f, cHeight * .25f, color4);
		}
		
		//        Imagine living in a world
		//Where things existed whether or not
		//    they were being directly observed

		renderMan->DrawString("          Imagine living in a world", medSize, cWidth * .20f, cHeight * .35f, color4);
		renderMan->DrawString("    Where things existed whether or not", medSize, cWidth * .20f, cHeight * .40f, color4);
		renderMan->DrawString("      they were being directly observed", medSize, cWidth * .20f, cHeight * .45f, color4);


		//renderMan->DrawString("Object Permanence is a psychology concept", cHeight * .04f, cWidth * .20f, cHeight * .30f, color4);
		//renderMan->DrawString("Babies do not know that objects continue to exist when not directly observed", cHeight * .04f, cWidth * .20f, cHeight * .35f, color4);
		//renderMan->DrawString("Imagine what it would be like if parts of the world", medSize, cWidth * .20f, cHeight * .40f, color4);
		//renderMan->DrawString("stopped existing when you weren't looking at them", medSize, cWidth * .20f, cHeight * .45f, color4);

		if(selector == 0)
		{
			renderMan->DrawString(">Next", smlSize, cWidth * .20f, cHeight * .50f, color2);
		}
		else
			renderMan->DrawString("  Next", smlSize, cWidth * .20f, cHeight * .50f, color4);
		if(selector == 1)
		{
			renderMan->DrawString(">Play", smlSize, cWidth * .20f, cHeight * .54f, color2);
		}
		else
			renderMan->DrawString("  Play", smlSize, cWidth * .20f, cHeight * .54f, color4);
		if(selector == 2)
		{
			renderMan->DrawString(">Menu", smlSize, cWidth * .20f, cHeight * .58f, color2);
		}
		else
			renderMan->DrawString("  Menu", smlSize, cWidth * .20f, cHeight * .58f, color4);
		
		renderMan->EndDrawMenu();

		break;
	#pragma endregion
	#pragma region END
	case END:
		renderMan->ClearTargetToColor(colorthree); //Colors::Silver reinterpret_cast<const float*>(&Colors::Silver)
		renderMan->DrawMenuBackground();
		renderMan->DrawString("P", lgSize, cWidth * .20f, cHeight / 10, color1);
		renderMan->DrawString("   eripheral Voi", lgSize, cWidth * .175f, cHeight / 10, color4);
		renderMan->DrawString("                       d", lgSize, cWidth * .185f, cHeight / 10, color3);
		renderMan->DrawString("Credits!", medSize, cWidth * .20f, cHeight * .25f, color1);
		renderMan->DrawString("  Thanks Chris Cascioli, Jen Stanton, Frank Luna,", smlSize, cWidth * .20f, cHeight * .32f, color4);
		renderMan->DrawString("        Oculus VR, SFXR, FW1FontWrapper", smlSize, cWidth * .20f, cHeight * .36f, color4);
		
		renderMan->DrawString("  Made by Jon Palmer, Jason Mandelbaum,", smlSize, cWidth * .20f, cHeight * .44f, color4);
		renderMan->DrawString("        Mike St. Pierre, and Drew Diamantoukos", smlSize, cWidth * .20f, cHeight * .48f, color4);
		/*if(selector == 5)
		{
			renderMan->DrawString(">-Chris Cascioli", smlSize, cWidth * .20f, cHeight * .36f, color2);
		}
		else
			renderMan->DrawString("  Thanks Chris Cascioli", smlSize, cWidth * .20f, cHeight * .36f, color4);
		if(selector == 6)
		{
			renderMan->DrawString(">Thanks Jen Stanton", smlSize, cWidth * .20f, cHeight * .40f, color2);
		}
		else
			renderMan->DrawString("  Thanks Jen Stanton", smlSize, cWidth * .20f, cHeight * .40f, color4);
		if(selector == 2)
		{
			renderMan->DrawString(">Thanks Oculus", smlSize, cWidth * .20f, cHeight * .44f, color2);
		}
		else
			renderMan->DrawString("  Thanks Oculus", smlSize, cWidth * .20f, cHeight * .44f, color4);
		if(selector == 3)
		{
			renderMan->DrawString(">Thanks SFXR", smlSize, cWidth * .20f, cHeight * .48f, color2);
		}
		else
			renderMan->DrawString("  Thanks SFXR", smlSize, cWidth * .20f, cHeight * .48f, color4);*/
		//if(selector == 0 || selector == 1)
		//{
			renderMan->DrawString(">Menu", smlSize, cWidth * .20f, cHeight * .52f, color2);
		//}
		//else
		//	renderMan->DrawString("  Menu", smlSize, cWidth * .20f, cHeight * .52f, color4);
		renderMan->EndDrawMenu();
		break;
	#pragma endregion
	default:
		break;
	}
}
 
void PVGame::ToggleDevMode()
{
	devMode = !devMode;
}

bool PVGame::getDevMode()
{
	return devMode;
}

void PVGame::BuildFX()
{
	renderMan->BuildFX();
}

void PVGame::BuildVertexLayout()
{
	renderMan->BuildVertexLayout();
}

void PVGame::BuildRooms(Room* startRoom)
{
	bool isLoaded = false;

	for (unsigned int i = 0; i < loadedRooms.size(); i++)
	{
		if (strcmp(loadedRooms[i]->getFile(), startRoom->getFile()) == 0)
			isLoaded = true;
	}

	if (!isLoaded)
	{
		for (unsigned int i = 0; i < startRoom->getGameObjs().size(); i++)
		{
			gameObjects.push_back(startRoom->getGameObjs()[i]);
		}
		
		if(!startRoom->hasWinCrest())
			startRoom->loadNeighbors(loadedRooms);
		
		loadedRooms.push_back(startRoom);

		for (unsigned int i = 0; i < startRoom->getNeighbors().size(); i++)
		{
			BuildRooms(startRoom->getNeighbors()[i]);
		}
	}
}

void PVGame::ClearRooms()
{
	for (unsigned int i = 0; i < loadedRooms.size(); i++)
	{
		for (unsigned int j = 0; j < loadedRooms[i]->getNeighbors().size(); j++)
		{
			loadedRooms[i]->getNeighbors()[j] = NULL;
		}

		delete loadedRooms[i];
	}
}

// Sorts game objects based on mesh key. Should only be called after a batch of GameObjects are added.
void PVGame::SortGameObjects()
{
	// GameObjectCompaper() is defined in GameObject.h for now. It's a boolean operator.
	sort(gameObjects.begin(), gameObjects.end(), GameObjectComparer());
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
	#if defined(DEBUG) | defined(_DEBUG)
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	#endif

	PVGame theApp(hInstance);

	if (!theApp.Init(cmdLine))
		return 0;

	return theApp.Run();
}