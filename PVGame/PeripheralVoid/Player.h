#pragma once

#include "PhysicsManager.h"
#include "RenderManager.h"
#include "Common\Camera.h"
#include "Common\xnacollision.h"
#include "Input.h"
#include "Audio\AudioListener.h"
#include <string>
#include <memory>
#include "RiftManager.h"

using namespace std;
using namespace XNA;

class Player
{
	public:
		Player(PhysicsManager* pm, RenderManager* rm, RiftManager* riftM);
		virtual ~Player(void);
		void OnResize(float aspectRatio);
		void Update(float dt, Input* aInput);
		XMMATRIX ViewProj() const;
		
		void resetStatuses(bool resetHeph);
		void increaseMedusaPercent();
		void increaseWinPercent();
		void resetWinPercent();

		void setMobilityStatus(bool newStatus);
		void setMedusaStatus(bool newStatus);
		void setLeapStatus(bool newStatus);
		void setWinStatus(bool newStatus);
		void setHephStatus(bool newStatus);
		void setPosition(float setX, float setY, float setZ);

		Camera* GetCamera();
		btKinematicCharacterController* getController();
		bool getMobilityStatus();
		bool getMedusaStatus();
		bool getLeapStatus();
		bool getWinStatus();
		bool getHephStatus();

		float getWinPercent() {return winPercent;}

		XMFLOAT4 getPosition();

		AudioListener* getListener();
		btVector3 getCameraPosition();

		void setMouseSensitivity(float ms);
		void setInverted(bool i);

		void setRotation(float rotRad);

	private:
		bool leapStatus;
		bool mobilityStatus;
		bool medusaStatus;
		bool winStatus;
		bool hephStatus;
		float medusaPercent;
		float winPercent;
		float playerSpeed;
		float camLookSpeed;
		const float PIXELS_PER_SEC;
		const float LOOK_SPEED;
		Camera* playerCamera;
		XMFLOAT4 position;
		XMFLOAT3 fwd;
		XMFLOAT3 right;
		XMFLOAT3 up;
		void HandleInput(Input* input);

		PhysicsManager* physicsMan;
		RenderManager* renderMan;
		btKinematicCharacterController* controller;

		AudioListener* listener;
		AudioSource* audioSource;

		//Camera movement variables
		float mTheta;
		float mPhi;
		POINT mLastMousePos;

		//OCULUS RIFT
		RiftManager* riftMan;

		float EyeYaw, EyePitch, EyeRoll, yaw;
		float MOUSESENSITIVITY;
		bool INVERTED;
};

