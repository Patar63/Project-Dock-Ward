#pragma once
#include "entt.hpp"
#include "GLM/glm.hpp"
#include "GLM/common.hpp"

//class to create a scene 
class SMI_Scene
{
public:
	//constructor calls
	SMI_Scene();
	SMI_Scene(std::string name = std::string());

	//copy, move, and assignment operators
	SMI_Scene(const SMI_Scene& oldScene) = default;
	SMI_Scene(SMI_Scene&&) = default;
	SMI_Scene& operator=(SMI_Scene&) = default;

	//destructor call
	~SMI_Scene();

	entt::handle CreateHandle();
	entt::registry& GetRegistry() { return Store; }

	//function declarations for a scene 
	virtual void InitScene();
	virtual void Update(float deltaTime);
	virtual void Render();
	virtual void PostRender();

	//Physics for scenes
	//gravity setter and getter
	void setGravity(const glm::vec3& _gravity) { gravity = _gravity; }
	glm::vec3 getGravity() const { return gravity; }

	//setter and getter for active scene 
	void setActive(const bool& _isActive) { isActive = _isActive; }
	bool getActive() const { return isActive; }

	//setter and getter for the pause scene
	void setPause(const bool& _isPaused) { isPaused = _isPaused; }
	bool getPause() const { return isPaused; }

private:
	//create registry
	entt::registry Store;
	//bool to tell if a scene is active
	bool isActive;

	//pause screen boolean
	bool isPaused;

	//physics variables
	glm::vec3 gravity;

protected:
	//handle used to reference camera object
	entt::handle camera;

};
