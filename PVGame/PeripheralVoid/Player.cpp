#include "Player.h"

Player::Player(PhysicsManager* pm) 
	: PIXELS_PER_SEC(100.0f), LOOK_SPEED(3.5f)
{
	// Build the view matrix. Now done in init because we only need to set it once.
	XMVECTOR aPos = XMVectorSet(0.0f, 1.727f, 0.0f, 1.0f);
	XMVECTOR aUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR aFwd = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR aRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

	XMStoreFloat4(&position, aPos);
	XMStoreFloat3(&up, aUp);
	XMStoreFloat3(&fwd, aFwd);
	XMStoreFloat3(&right, aRight);

	XMVECTOR target = XMVectorSet(0.0f, 0.0f, 10.0f, 1.0f);

	playerCamera = new Camera();
	playerCamera->LookAt(aPos, target, aUp);
	playerCamera->UpdateViewMatrix();

	physicsMan = pm;
	controller = physicsMan->createCharacterController( 1, .5, .1);
	controller->setGravity(9.81);
	controller->setJumpSpeed(10);
	controller->setMaxJumpHeight(10);
	controller->setMaxSlope(3 * 3.1415);

	listener = new AudioListener();
}

void Player::Update(float dt, Input* input)
{
	playerSpeed = dt * PIXELS_PER_SEC;
	camLookSpeed = dt * LOOK_SPEED;
	this->HandleInput(input);
}

void Player::HandleInput(Input* input)
{
	// Now check for camera input.
	if (input->isCameraUpKeyDown())
	{
		playerCamera->Pitch(-camLookSpeed / 2);
	}
	if (input->isCameraDownKeyDown())
	{
		playerCamera->Pitch(camLookSpeed/ 2);
	}

	if (input->isCameraLeftKeyDown())
	{
		float angle = -camLookSpeed / 2;
		playerCamera->RotateY(angle);
		XMMATRIX R = XMMatrixRotationY(angle);

		XMStoreFloat3(&right, XMVector3TransformNormal(XMLoadFloat3(&right), R));
		XMStoreFloat3(&up, XMVector3TransformNormal(XMLoadFloat3(&up), R));
		XMStoreFloat3(&fwd, XMVector3TransformNormal(XMLoadFloat3(&fwd), R));
		//TransformOrientedBox(boundingBox.get(), boundingBox.get(), 1.0f, XMQuaternionRotationMatrix(R), XMVECTOR());
	}
	if (input->isCameraRightKeyDown())
	{
		float angle = camLookSpeed / 2;
		playerCamera->RotateY(angle);
		XMMATRIX R = XMMatrixRotationY(angle);

		XMStoreFloat3(&right, XMVector3TransformNormal(XMLoadFloat3(&right), R));
		XMStoreFloat3(&up, XMVector3TransformNormal(XMLoadFloat3(&up), R));
		XMStoreFloat3(&fwd, XMVector3TransformNormal(XMLoadFloat3(&fwd), R));
		//TransformOrientedBox(boundingBox.get(), boundingBox.get(), 1.0f, XMQuaternionRotationMatrix(R), XMVECTOR());
	}

	//XMVECTOR tempPosition = XMLoadFloat4(&position);

	//// Checking for position movement input, will want to move to separate method later.
	//if (input->isPlayerUpKeyDown())
	//	tempPosition = XMVectorAdd(tempPosition, XMLoadFloat3(&fwd) * playerSpeed);
	//if (input->isPlayerDownKeyDown())
	//	tempPosition = XMVectorAdd(tempPosition, -XMLoadFloat3(&fwd) * playerSpeed);
	//if (input->isPlayerLeftKeyDown())
	//	tempPosition = XMVectorAdd(tempPosition, -XMLoadFloat3(&right) * playerSpeed);
	//if (input->isPlayerRightKeyDown())
	//	tempPosition = XMVectorAdd(tempPosition, XMLoadFloat3(&right) * playerSpeed);

	btVector3 direction(0,0,0);
	btVector3 forward(fwd.x, fwd.y, fwd.z);
	btVector3 r(right.x, right.y, right.z);

	if(input->isPlayerUpKeyDown())
		direction += forward;
	if(input->isPlayerDownKeyDown())
		direction -= forward;
	if(input->isPlayerRightKeyDown())
		direction += r;
	if(input->isPlayerLeftKeyDown())
		direction -= r;
	if(input->isJumpKeyPressed())
		controller->jump();

	controller->setWalkDirection(direction * playerSpeed);
	
	btVector3 pos = controller->getGhostObject()->getWorldTransform().getOrigin();
	XMFLOAT3 cPos(pos.getX(), pos.getY() + 1, pos.getZ());
	playerCamera->SetPosition(cPos);

	listener->setPosition(cPos.x, cPos.y, cPos.z);
	listener->setOrientation(-playerCamera->GetLook().x, -playerCamera->GetLook().y, -playerCamera->GetLook().z, playerCamera->GetUp().x, playerCamera->GetUp().y, playerCamera->GetUp().z);

	//XMFLOAT4 unit(0.0f, 0.0f, 0.0f, 1.0f);
	
	/*XMFLOAT3 pos;
	XMStoreFloat3(&pos, tempPosition);
	playerCamera->SetPosition(pos);
	XMStoreFloat4(&position, tempPosition);*/

	/*
	wstring temp = L"";
	stringstream ss;
	ss << "Player position - X: " << position.x << ", Y: " << position.y << ", Z: " << position.z << endl;
	char * cstr = new char [ss.str().length()+1];
	std::strcpy (cstr, ss.str().c_str());

	OutputDebugStringA(cstr);

	delete cstr;
	*/
	playerCamera->UpdateViewMatrix();
}

void Player::OnResize(float aspectRatio)
{
	playerCamera->SetLens(0.25f*MathHelper::Pi, aspectRatio, 1.0f, 1000.0f);
	playerCamera->UpdateViewMatrix();
}

XMMATRIX Player::ViewProj() const
{
	return playerCamera->ViewProj();
}

Camera* Player::GetCamera()
{
	return playerCamera;
}

XMFLOAT4 Player::getPosition()
{
	btVector3 pos = controller->getGhostObject()->getWorldTransform().getOrigin();
	return  XMFLOAT4(pos.getX(), pos.getY(), pos.getZ(), 1);
}

btVector3 Player::getCameraPosition()
{
	XMFLOAT3 pos = playerCamera->GetPosition();
	return btVector3(pos.x, pos.y, pos.z);
}

Player::~Player(void)
{
	physicsMan->removeCharacterController(controller);
	delete listener;
	delete playerCamera;
}
