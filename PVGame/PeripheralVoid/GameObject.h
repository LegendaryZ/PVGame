#pragma once
#include "Constants.h"
#include "Audio\AudioSource.h"
#include "PhysicsManager.h"

class PhysicsManager;

//Only GameObjects that need to be affected by physics, aka the player not walking through them need a rigid body
class GameObject
{
	public:
		GameObject(void);
		GameObject(string aMeshKey, string aMaterialKey, XMMATRIX* aWorldMatrix, PhysicsManager* physicsMan, bool visionAff = false);
		GameObject(string aMeshKey, string aMaterialKey, btRigidBody* rB, PhysicsManager* physicsMan, short collisionLayer, float mass = 0.0, bool visionAff = false);

		void setSeen(bool);
		bool isSeen();

		void translate(float x, float y, float z);
		void setPosition(float x, float y, float z);
		void scale(float x, float y, float z);
		void rotate(float x, float y, float z, float w);
		void rotate(float yaw, float pitch, float roll);
		void setLinearVelocity(float x, float y, float z);

		void SetMeshKey(string aKey);
		void SetMaterialKey(string aKey);
		void SetWorldMatrix(XMMATRIX* aMatrix);
		
		void Update();

		bool GetVisionAffected();
		string GetMeshKey() const;
		string GetMaterialKey() const;

		void CalculateWorldMatrix();
		XMFLOAT4X4 GetWorldMatrix() const;

		void SetRigidBody(btRigidBody* rBody, short layer);
		btRigidBody* getRigidBody() const;
		void changeCollisionLayer(short layer);


		virtual ~GameObject(void);

		void initAudio(string file);
		void playAudio();
		void playAudioAt(float x, float y, float z);
		void playAudioAt(float* position);
		void playAudioAtAndMoveTo(float x, float y, float z);
		void playAudioAtAndMoveTo(float* position);
		void setAudioLooping(bool);
		void stopAudio();
		void pauseAudio();
		void resumeAudio();
		void restartAudio();
		void restartAndPlayAudio();

	protected:
		bool visionAffected;
		bool seen;
		string meshKey;
		string materialKey;
		btRigidBody* rigidBody;
		XMFLOAT4X4 worldMatrix;
		XMFLOAT3 localScale;
		PhysicsManager* physicsMan; //Don't use for anything but adding to and deleting the rigid body from the world.
		float mass;
		short collisionLayer;

		AudioSource* audioSource;
		// Figure out how to make this work: //static RenderManager *renderMan = RenderManager::getInstance();
};

struct GameObjectComparer
{
    inline bool operator() (const GameObject* objectA, const GameObject* objectB)
    {
		// Returns true if A's key is 'less than' B's key.
        return (objectA->GetMeshKey().compare(objectB->GetMeshKey()) < 0);
    }
};
