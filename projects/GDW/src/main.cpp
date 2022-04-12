#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#include <Logging.h>

#include "IndexBuffer.h"
#include "VertexBuffer.h"
#include "VertexArrayObject.h"
#include "Shader.h"
#include "Camera.h"
#include "Player.h"
#include "Physics.h"
#include "Scene.h"
#include "Texture2D.h"
#include "TextureCube.h"

#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "VertexTypes.h"

#include <memory>
#include <filesystem>
#include <json.hpp>
#include <fstream>
#include <string>
#include <iostream>
#include "Sound.h"

#define LOG_GL_NOTIFICATIONS

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::string sourceTxt;
	switch (source) {
	case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
	case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
	case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) {
	case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
#ifdef LOG_GL_NOTIFICATIONS
	case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
#endif
	default: break;
	}
}

// Stores our GLFW window in a global variable for now
GLFWwindow* window;
// The current size of our window in pixels
glm::ivec2 windowSize = glm::ivec2(1500, 1000);
// The title of our GLFW window
std::string windowTitle = "Project Dock-Ward";







void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	windowSize = glm::ivec2(width, height);
}





/// <summary>
/// Handles intializing GLFW, should be called before initGLAD, but after Logger::Init()
/// Also handles creating the GLFW window
/// </summary>
/// <returns>True if GLFW was initialized, false if otherwise</returns>
bool initGLFW() {
	// Initialize GLFW
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

	//Create a new GLFW window and make it current
	window = glfwCreateWindow(windowSize.x, windowSize.y, windowTitle.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	return true;
}

/// <summary>
/// Handles initializing GLAD and preparing our GLFW window for OpenGL calls
/// </summary>
/// <returns>True if GLAD is loaded, false if there was an error</returns>
bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}


GLuint shader_program;

//load in vertex and fragment shaders
bool loadShaders()
{
	// Read Shaders from file
	std::string vert_shader_str;
	std::ifstream vs_stream("vertex_shader.glsl", std::ios::in);
	if (vs_stream.is_open())
	{
		std::string Line = "";
		while (getline(vs_stream, Line))
			vert_shader_str += "\n" + Line;
		vs_stream.close();
	}
	else
	{
		printf("Could not open vertex shader!!\n");
		return false;
	}
	const char* vs_str = vert_shader_str.c_str();

	std::string frag_shader_str;
	std::ifstream fs_stream("frag_shader.glsl", std::ios::in);
	if (fs_stream.is_open()) {
		std::string Line = "";
		while (getline(fs_stream, Line))
			frag_shader_str += "\n" + Line;
		fs_stream.close();
	}
	else {
		printf("Could not open fragment shader!!\n");
		return false;
	}
	const char* fs_str = frag_shader_str.c_str();

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vs_str, NULL);
	glCompileShader(vs);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fs_str, NULL);
	glCompileShader(fs);

	shader_program = glCreateProgram();
	glAttachShader(shader_program, fs);
	glAttachShader(shader_program, vs);
	glLinkProgram(shader_program);

	return true;
}

template <typename T>
//templated LERP to be used for bullets
T Lerp(T a, T b, float t)
{
	return(1.0f - t) * a + b * t;
}


class GameScene1 : public SMI_Scene
{
public:
	//just ignore the warning, guys. It's fine.
	GameScene1() : SMI_Scene() {}

	void InitScene()
	{
		SMI_Scene::InitScene();
		SMI_Scene::setGravity(glm::vec3(0.0, 0.0, -9.8));


		// Load our shaders
		Shader::Sptr shader = Shader::Create();

		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", ShaderPartType::Vertex);
		shader->LoadShaderPartFromFile("shaders/frag_shader.glsl", ShaderPartType::Fragment);
		shader->Link();

		// GL states, we'll enable depth testing and backface fulling
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glClearColor(0.0f, 0.0f, 0.1f, 0.0f);

		// Get uniform location for the model view projection
		Camera::Sptr camera = Camera::Create();

		//camera position
		camera->SetPosition(glm::vec3(-0.2, 60.5, 17.9));
		//this defines the point the camera is looking at
		camera->LookAt(glm::vec3(0, 1.5, 7));
		camera->SetFovDegrees(-20);
		setCamera(camera);

		//create the player character
		VertexArrayObject::Sptr chara = ObjLoader::LoadFromFile("Models/character.obj");
		{
			character = CreateEntity();

			//create texture
			Texture2D::Sptr CharacterTex = Texture2D::Create("Textures/character1.png");
			//create material
			SMI_Material::Sptr CharacterMat = SMI_Material::Create();
			CharacterMat->setShader(shader);

			//set texture
			CharacterMat->setTexture(CharacterTex, 0);
			//render
			Renderer CharacterRend = Renderer(CharacterMat, chara);
			AttachCopy(character, CharacterRend);
			//transform & Physics
			SMI_Transform CharaTrans = SMI_Transform();
			CharaTrans.setPos(glm::vec3 (4, 7.0, 2.3));
			CharaTrans.SetDegree(glm::vec3(90, 0, -90));
			CharaTrans.setScale(glm::vec3(0.25, 0.25, 0.25));
			AttachCopy(character, CharaTrans);

			SMI_Physics CharaPhys = SMI_Physics(glm::vec3(4, 7.0, 2.3), glm::vec3(90, 0, -90), glm::vec3(2, 1, 1), character, SMI_PhysicsBodyType::DYNAMIC, 1.0f);
			CharaPhys.setHasGravity(true);
			CharaPhys.setIdentity(1);
			AttachCopy(character, CharaPhys);
		}

		//creates object
		//Bar
		VertexArrayObject::Sptr vao4 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr window1Texture = Texture2D::Create("Textures/brown1.png");
			//material
			SMI_Material::Sptr BarrelMat = SMI_Material::Create();
			BarrelMat->setShader(shader);

			//set textures
			BarrelMat->setTexture(window1Texture, 0);
			//render
			Renderer BarrelRend = Renderer(BarrelMat, vao4);
			AttachCopy(barrel, BarrelRend);
			//transform
			SMI_Transform BarrelTrans = SMI_Transform();

			BarrelTrans.setPos(glm::vec3(-6.0, -3.3, 1));

			BarrelTrans.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans);
		}
		//creates object
		VertexArrayObject::Sptr firstwin = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr firstTexture1 = Texture2D::Create("Textures/brown1.png");
			//material
			SMI_Material::Sptr firstMat = SMI_Material::Create();
			firstMat->setShader(shader);

			//set textures
			firstMat->setTexture(firstTexture1, 0);
			//render
			Renderer winRend = Renderer(firstMat, firstwin);
			AttachCopy(barrel, winRend);
			//transform
			SMI_Transform win22Trans = SMI_Transform();

			win22Trans.setPos(glm::vec3(18, -3.3, 1));

			win22Trans.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, win22Trans);
		}
		//creates object
      /*
		VertexArrayObject::Sptr back = ObjLoader::LoadFromFile("Models/wi1.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr backTexture = Texture2D::Create("Textures/back.png");
			//material
			SMI_Material::Sptr backMat = SMI_Material::Create();
			backMat->setShader(shader);

			//set textures
			backMat->setTexture(backTexture, 0);
			//render
			Renderer backRend = Renderer(backMat, back);
			AttachCopy(barrel, backRend);
			//transform
			SMI_Transform backTrans = SMI_Transform();

			backTrans.setPos(glm::vec3(-50.0, -9.3, 1));

			backTrans.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(barrel, backTrans);
		}
		*/
		//creates object
		VertexArrayObject::Sptr window2 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr window2Texture = Texture2D::Create("Textures/brown1.png");
			//material
			SMI_Material::Sptr window2Mat = SMI_Material::Create();
			window2Mat->setShader(shader);

			//set textures
			window2Mat->setTexture(window2Texture, 0);
			//render
			Renderer window2Rend = Renderer(window2Mat, window2);
			AttachCopy(barrel, window2Rend);
			//transform
			SMI_Transform window2Trans = SMI_Transform();

			window2Trans.setPos(glm::vec3(-31.0, -3.3, 1));

			window2Trans.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, window2Trans);
		}


		VertexArrayObject::Sptr vao5 = ObjLoader::LoadFromFile("Models/barrel1.obj");
		{

			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr BarrelTexture = Texture2D::Create("Textures/Barrel.png");

			//material
			SMI_Material::Sptr BarrelMat1 = SMI_Material::Create();
			BarrelMat1->setShader(shader);

			//set textures
			BarrelMat1->setTexture(BarrelTexture, 0);

			//render
			Renderer BarrelRend1 = Renderer(BarrelMat1, vao5);
			AttachCopy(barrel, BarrelRend1);
			//transform
			SMI_Transform BarrelTrans1 = SMI_Transform();

			BarrelTrans1.SetDegree(glm::vec3(0, 90, 0));
			AttachCopy(barrel, BarrelTrans1);

			SMI_Physics BarrelPhys = SMI_Physics(glm::vec3(-0.6, 6.2, 2.7), glm::vec3(0, 90, 0), glm::vec3(1.67, 2.59, 2.12), barrel, SMI_PhysicsBodyType::DYNAMIC, 1.0f);
			BarrelPhys.setHasGravity(true);
			BarrelPhys.setIdentity(2);
			AttachCopy(barrel, BarrelPhys);
		}
		VertexArrayObject::Sptr vao512 = ObjLoader::LoadFromFile("Models/3barrel.obj");
		{

			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr BarrelTexture12 = Texture2D::Create("Textures/Barrel.png");

			//material
			SMI_Material::Sptr BarrelMat112 = SMI_Material::Create();
			BarrelMat112->setShader(shader);

			//set textures
			BarrelMat112->setTexture(BarrelTexture12, 0);

			//render
			Renderer BarrelRend112 = Renderer(BarrelMat112, vao512);
			AttachCopy(barrel, BarrelRend112);
			//transform
			SMI_Transform BarrelTrans112 = SMI_Transform();

			BarrelTrans112.setPos(glm::vec3(-9.0, 6.0, 3.7));

			BarrelTrans112.SetDegree(glm::vec3(0, 90, 0));
			AttachCopy(barrel, BarrelTrans112);

			SMI_Physics BarrelPhys = SMI_Physics(glm::vec3(-9.0, 6.0, 3.7), glm::vec3(0, 90, 0), glm::vec3(3, 6, 2), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			BarrelPhys.setHasGravity(true);
			BarrelPhys.setIdentity(2);
			AttachCopy(barrel, BarrelPhys);
		}




		VertexArrayObject::Sptr vao7 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr floor1Texture = Texture2D::Create("Textures/Untitled.1001.png");
			//material
			SMI_Material::Sptr BarrelMat3 = SMI_Material::Create();
			BarrelMat3->setShader(shader);

			//set textures
			BarrelMat3->setTexture(floor1Texture, 0);
			//render
			Renderer BarrelRend3 = Renderer(BarrelMat3, vao7);
			AttachCopy(barrel, BarrelRend3);
			//transform
			SMI_Transform BarrelTrans3 = SMI_Transform();

			BarrelTrans3.setPos(glm::vec3(-0.85, 0, 0.8));

			BarrelTrans3.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans3);

			SMI_Physics GroundPhys = SMI_Physics(glm::vec3(-0.85, 0, 0.8), glm::vec3(90, 0, 90), glm::vec3(15.3, 3.32, 11.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			GroundPhys.setIdentity(2);
			AttachCopy(barrel, GroundPhys);
		}
		VertexArrayObject::Sptr onevao7 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr onefloor1Texture = Texture2D::Create("Textures/Untitled.1001.png");
			//material
			SMI_Material::Sptr oneBarrelMat3 = SMI_Material::Create();
			oneBarrelMat3->setShader(shader);

			//set textures
			oneBarrelMat3->setTexture(onefloor1Texture, 0);
			//render
			Renderer oneBarrelRend3 = Renderer(oneBarrelMat3, onevao7);
			AttachCopy(barrel, oneBarrelRend3);
			//transform
			SMI_Transform oneBarrelTrans3 = SMI_Transform();

			oneBarrelTrans3.setPos(glm::vec3(-0.85, 15.3, 0.8));

			oneBarrelTrans3.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, oneBarrelTrans3);

			SMI_Physics GroundPhys = SMI_Physics(glm::vec3(-0.85, 15.3, 0.8), glm::vec3(90, 0, 90), glm::vec3(15.3, 3.32, 11.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			GroundPhys.setIdentity(2);
			AttachCopy(barrel, GroundPhys);
		}
		VertexArrayObject::Sptr vao8 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr window10Texture = Texture2D::Create("Textures/Untitled.1001.png");
			//material
			SMI_Material::Sptr BarrelMat4 = SMI_Material::Create();
			BarrelMat4->setShader(shader);

			//set textures
			BarrelMat4->setTexture(window10Texture, 0);
			//render
			Renderer BarrelRend4 = Renderer(BarrelMat4, vao8);
			AttachCopy(barrel, BarrelRend4);
			//transform
			SMI_Transform BarrelTrans4 = SMI_Transform();

			BarrelTrans4.setPos(glm::vec3(-12.8, 0, 0.8));

			BarrelTrans4.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans4);

			SMI_Physics GroundPhys = SMI_Physics(glm::vec3(-12.8, 0, 0.8), glm::vec3(90, 0, 90), glm::vec3(15.3, 3.32, 11.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			GroundPhys.setIdentity(2);
			AttachCopy(barrel, GroundPhys);
		}
		VertexArrayObject::Sptr twovao8 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr window10Texture = Texture2D::Create("Textures/Untitled.1001.png");
			//material
			SMI_Material::Sptr BarrelMat4 = SMI_Material::Create();
			BarrelMat4->setShader(shader);

			//set textures
			BarrelMat4->setTexture(window10Texture, 0);
			//render
			Renderer BarrelRend4 = Renderer(BarrelMat4, twovao8);
			AttachCopy(barrel, BarrelRend4);
			//transform
			SMI_Transform BarrelTrans4 = SMI_Transform();

			BarrelTrans4.setPos(glm::vec3(-12.8, 15.3, 0.8));

			BarrelTrans4.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans4);

			SMI_Physics GroundPhys = SMI_Physics(glm::vec3(-12.8, 15.3, 0.8), glm::vec3(90, 0, 90), glm::vec3(15.3, 3.32, 11.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			GroundPhys.setIdentity(2);
			AttachCopy(barrel, GroundPhys);
		}
		VertexArrayObject::Sptr wall4 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr window90Texture = Texture2D::Create("Textures/Untitled.1001.png");
			//material
			SMI_Material::Sptr WallMat1 = SMI_Material::Create();
			WallMat1->setShader(shader);

			//set textures
			WallMat1->setTexture(window90Texture, 0);
			//render
			Renderer WallRend1 = Renderer(WallMat1, wall4);
			AttachCopy(barrel, WallRend1);
			//transform
			SMI_Transform WallTrans1 = SMI_Transform();

			WallTrans1.setPos(glm::vec3(-24.7, 0, 0.8));

			WallTrans1.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, WallTrans1);

			SMI_Physics GroundPhys = SMI_Physics(glm::vec3(-24.7, 0, 0.8), glm::vec3(90, 0, 90), glm::vec3(15.3, 3.32, 11.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			GroundPhys.setIdentity(2);
			AttachCopy(barrel, GroundPhys);
		}
		VertexArrayObject::Sptr barground = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr window90Texture = Texture2D::Create("Textures/Untitled.1001.png");
			//material
			SMI_Material::Sptr WallMat1 = SMI_Material::Create();
			WallMat1->setShader(shader);

			//set textures
			WallMat1->setTexture(window90Texture, 0);
			//render
			Renderer WallRend1 = Renderer(WallMat1, barground);
			AttachCopy(barrel, WallRend1);
			//transform
			SMI_Transform WallTrans1 = SMI_Transform();

			WallTrans1.setPos(glm::vec3(-24.7, 15.3, 0.8));

			WallTrans1.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, WallTrans1);

			SMI_Physics GroundPhys = SMI_Physics(glm::vec3(-24.7, 15.3, 0.8), glm::vec3(90, 0, 90), glm::vec3(15.3, 3.32, 11.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			GroundPhys.setIdentity(2);
			AttachCopy(barrel, GroundPhys);
		}
		VertexArrayObject::Sptr barground1 = ObjLoader::LoadFromFile("Models/floor3.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr twindow90Texture = Texture2D::Create("Textures/Untitled.1001.png");
			//material
			SMI_Material::Sptr tWallMat1 = SMI_Material::Create();
			tWallMat1->setShader(shader);

			//set textures
			tWallMat1->setTexture(twindow90Texture, 0);
			//render
			Renderer tWallRend1 = Renderer(tWallMat1, barground1);
			AttachCopy(barrel, tWallRend1);
			//transform
			SMI_Transform tWallTrans1 = SMI_Transform();

			tWallTrans1.setPos(glm::vec3(-38.3, 6.3, 0.8));
			

			tWallTrans1.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, tWallTrans1);

			SMI_Physics tGroundPhys = SMI_Physics(glm::vec3(-38.3, 6.3, 0.8), glm::vec3(90, 0, 90), glm::vec3(15.3, 3.32, 16.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			tGroundPhys.setIdentity(2);
			AttachCopy(barrel, tGroundPhys);
		}

		VertexArrayObject::Sptr crate = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex = Texture2D::Create("Textures/box32.png");
			//material
			SMI_Material::Sptr BarrelMat5 = SMI_Material::Create();
			BarrelMat5->setShader(shader);

			//set textures
			BarrelMat5->setTexture(crateTex, 0);
			//render
			Renderer BarrelRend5 = Renderer(BarrelMat5, crate);
			AttachCopy(barrel, BarrelRend5);
			//transform
			SMI_Transform BarrelTrans5 = SMI_Transform();

			BarrelTrans5.setPos(glm::vec3(-22.0, 7.0, 4.5));

			BarrelTrans5.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans5);

			SMI_Physics cratephys20111 = SMI_Physics(glm::vec3(-22.0, 7.0, 4.5), glm::vec3(90, 0, 90), glm::vec3(2.15, 1.97, 3.2), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			cratephys20111.setIdentity(2);
			AttachCopy(barrel, cratephys20111);
		}
		
		
		VertexArrayObject::Sptr crate3 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex3 = Texture2D::Create("Textures/box32.png");
			//material
			SMI_Material::Sptr BarrelMat53 = SMI_Material::Create();
			BarrelMat53->setShader(shader);

			//set textures
			BarrelMat53->setTexture(crateTex3, 0);
			//render
			Renderer BarrelRend53 = Renderer(BarrelMat53, crate3);
			AttachCopy(barrel, BarrelRend53);
			//transform
			SMI_Transform BarrelTrans53 = SMI_Transform();

			BarrelTrans53.setPos(glm::vec3(-22.0, 7.0, 6.2));

			BarrelTrans53.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans53);

			SMI_Physics cratephys2011 = SMI_Physics(glm::vec3(-22.0, 7.0, 6.2), glm::vec3(90, 0, 90), glm::vec3(2.15, 1.97, 3.2), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			cratephys2011.setIdentity(4);
			AttachCopy(barrel, cratephys2011);
			
		}
		
		VertexArrayObject::Sptr crate31 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex331 = Texture2D::Create("Textures/box32.png");
			//material
			SMI_Material::Sptr BarrelMat5331 = SMI_Material::Create();
			BarrelMat5331->setShader(shader);

			//set textures
			BarrelMat5331->setTexture(crateTex331, 0);
			//render
			Renderer BarrelRend5331 = Renderer(BarrelMat5331, crate31);
			AttachCopy(barrel, BarrelRend5331);
			//transform
			SMI_Transform BarrelTrans5331 = SMI_Transform();

			BarrelTrans5331.setPos(glm::vec3(-22.0, 7.0, 7.8));

			BarrelTrans5331.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans5331);

			SMI_Physics cratephys201131 = SMI_Physics(glm::vec3(-22.0, 7.0, 7.8), glm::vec3(90, 0, 90), glm::vec3(2.15, 1.97, 3.2), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			cratephys201131.setIdentity(4);
			AttachCopy(barrel, cratephys201131);

		}
		VertexArrayObject::Sptr crate4 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex4 = Texture2D::Create("Textures/box32.png");
			//material
			SMI_Material::Sptr BarrelMat534 = SMI_Material::Create();
			BarrelMat534->setShader(shader);

			//set textures
			BarrelMat534->setTexture(crateTex4, 0);
			//render
			Renderer BarrelRend53 = Renderer(BarrelMat534, crate4);
			AttachCopy(barrel, BarrelRend53);
			//transform
			SMI_Transform BarrelTrans534 = SMI_Transform();

			BarrelTrans534.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans534);

			SMI_Physics cratephys552 = SMI_Physics(glm::vec3(-17.0, 7.0, 2.5), glm::vec3(90, 0, 90), glm::vec3(2.15, 1.97, 3.2), barrel, SMI_PhysicsBodyType::DYNAMIC, 1.0f);
			cratephys552.setIdentity(2);
			AttachCopy(barrel, cratephys552);
		}
		VertexArrayObject::Sptr door = ObjLoader::LoadFromFile("Models/warehousedoor.obj");
		{

			door1 = CreateEntity();

			Texture2D::Sptr doorTexture = Texture2D::Create("Textures/doortex.png");
			//material
			SMI_Material::Sptr BarrelMat6 = SMI_Material::Create();
			BarrelMat6->setShader(shader);

			//set textures
			BarrelMat6->setTexture(doorTexture, 0);
			//render
			Renderer BarrelRend6 = Renderer(BarrelMat6, door);
			AttachCopy(door1, BarrelRend6);
			//transform
			SMI_Transform BarrelTrans6 = SMI_Transform();

			BarrelTrans6.setPos(glm::vec3(-46.0, 9.5, 2));

			BarrelTrans6.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(door1, BarrelTrans6);
			
			SMI_Physics dphys201131 = SMI_Physics(glm::vec3(-46.0, 9.5, 2), glm::vec3(90, 0, -90), glm::vec3(4.02, 10.298, 0.13), door1, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			dphys201131.setIdentity(8);
			AttachCopy(door1, dphys201131);
		}
		VertexArrayObject::Sptr dw1 = ObjLoader::LoadFromFile("Models/wdoorway.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr dwTexture80 = Texture2D::Create("Textures/bricktex.png");
			//material
			SMI_Material::Sptr dwMa80 = SMI_Material::Create();


			dwMa80->setShader(shader);

			//set textures
			dwMa80->setTexture(dwTexture80, 0);
			//render
			Renderer dwRen80 = Renderer(dwMa80, dw1);
			AttachCopy(barrel, dwRen80);
			//transform
			SMI_Transform dwTrans1801 = SMI_Transform();

			dwTrans1801.setPos(glm::vec3(-46.0, -8.8, 2.0));

			dwTrans1801.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, dwTrans1801);
		}

		VertexArrayObject::Sptr crate1 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crate1Texture = Texture2D::Create("Textures/box32.png");
			//material
			SMI_Material::Sptr BarrelMat5 = SMI_Material::Create();
			BarrelMat5->setShader(shader);

			//set textures
			BarrelMat5->setTexture(crate1Texture, 0);
			//render
			Renderer BarrelRend5 = Renderer(BarrelMat5, crate1);
			AttachCopy(barrel, BarrelRend5);
			//transform
			SMI_Transform BarrelTrans5 = SMI_Transform();

			BarrelTrans5.setPos(glm::vec3(-22.0, 7.0, 2.7));

			BarrelTrans5.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans5);

			SMI_Physics cratephys20 = SMI_Physics(glm::vec3(-22.0, 7.0, 2.7), glm::vec3(90, 0, 90), glm::vec3(2.15, 1.97, 3.2), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			cratephys20.setIdentity(2);
			AttachCopy(barrel, cratephys20);
		}

		VertexArrayObject::Sptr bar = ObjLoader::LoadFromFile("Models/btab.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr WinTexture80 = Texture2D::Create("Textures/bartabtex.png");
			//material
			SMI_Material::Sptr BarrelMa80 = SMI_Material::Create();


			BarrelMa80->setShader(shader);

			//set textures
			BarrelMa80->setTexture(WinTexture80, 0);
			//render
			Renderer BarrelRen80 = Renderer(BarrelMa80, bar);
			AttachCopy(barrel, BarrelRen80);
			//transform
			SMI_Transform winTrans180 = SMI_Transform();

			winTrans180.setPos(glm::vec3(1.2, 1.5, 2.0));

			winTrans180.SetDegree(glm::vec3(90, 0, -180));
			AttachCopy(barrel, winTrans180);
		}
		VertexArrayObject::Sptr bararea = ObjLoader::LoadFromFile("Models/bar_area.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr bararea80 = Texture2D::Create("Textures/tabletex1.png");
			//material
			SMI_Material::Sptr barareaMa80 = SMI_Material::Create();


			barareaMa80->setShader(shader);

			//set textures
			barareaMa80->setTexture(bararea80, 0);
			//render
			Renderer barareaRen80 = Renderer(barareaMa80, bararea);
			AttachCopy(barrel, barareaRen80);
			//transform
			SMI_Transform barareaTrans180 = SMI_Transform();

			barareaTrans180.setPos(glm::vec3(-5.5, 16.5, 3.0));

			barareaTrans180.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, barareaTrans180);
		}
		VertexArrayObject::Sptr garbage1 = ObjLoader::LoadFromFile("Models/bardoorway.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr gTexture80 = Texture2D::Create("Textures/bricktex.png");
			//material
			SMI_Material::Sptr gMa80 = SMI_Material::Create();


			gMa80->setShader(shader);

			//set textures
			gMa80->setTexture(gTexture80, 0);
			//render
			Renderer gRen80 = Renderer(gMa80, garbage1);
			AttachCopy(barrel, gRen80);
			//transform
			SMI_Transform winTrans1801 = SMI_Transform();

			winTrans1801.setPos(glm::vec3(-12.5, -8.8, 2.0));

			winTrans1801.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, winTrans1801);
		}
		VertexArrayObject::Sptr barway = ObjLoader::LoadFromFile("Models/bardoorway.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr barwayTexture80 = Texture2D::Create("Textures/bricktex.png");
			//material
			SMI_Material::Sptr barwayMa80 = SMI_Material::Create();


			barwayMa80->setShader(shader);

			//set textures
			barwayMa80->setTexture(barwayTexture80, 0);
			//render
			Renderer barwayRen80 = Renderer(barwayMa80, barway);
			AttachCopy(barrel, barwayRen80);
			//transform
			SMI_Transform barwaywinTrans1801 = SMI_Transform();

			barwaywinTrans1801.setPos(glm::vec3(-24.5, -8.8, 2.0));

			barwaywinTrans1801.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, barwaywinTrans1801);
		}
		VertexArrayObject::Sptr garbage11 = ObjLoader::LoadFromFile("Models/bardoor.obj");
		{
			door4 = CreateEntity();
			//create texture
			Texture2D::Sptr gTexture8017 = Texture2D::Create("Textures/bardoor.png");
			//material
			SMI_Material::Sptr gMa8017 = SMI_Material::Create();


			gMa8017->setShader(shader);

			//set textures
			gMa8017->setTexture(gTexture8017, 0);
			//render
			Renderer gRen8017 = Renderer(gMa8017, garbage11);
			AttachCopy(door4, gRen8017);
			//transform
			SMI_Transform winTrans180117 = SMI_Transform();

			winTrans180117.setPos(glm::vec3(-12.5, 9.2, 2.0));

			winTrans180117.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(door4, winTrans180117);

			SMI_Physics bardoorphys = SMI_Physics(glm::vec3 (-12.5, 9.2, 2.0), glm::vec3(90, 0, -90), glm::vec3(14.3917, 10.56, 0.472), door4, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			bardoorphys.setIdentity(2);
			AttachCopy(door4, bardoorphys);
		}
		VertexArrayObject::Sptr button149 = ObjLoader::LoadFromFile("Models/barbutton.obj");
		{
			button6 = CreateEntity();

			//create texture
			Texture2D::Sptr buttonTexture8059 = Texture2D::Create("Textures/buttontex.png");
			//material
			SMI_Material::Sptr buttongMa8059 = SMI_Material::Create();


			buttongMa8059->setShader(shader);

			//set textures
			buttongMa8059->setTexture(buttonTexture8059, 0);
			//render
			Renderer buttongRen8059 = Renderer(buttongMa8059, button149);
			AttachCopy(button6, buttongRen8059);
			//transform
			SMI_Transform buttonTrans180159 = SMI_Transform();

			buttonTrans180159.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(button6, buttonTrans180159);

			SMI_Physics buttonphys59 = SMI_Physics(glm::vec3(-12.5, 7.7, 15.1), glm::vec3(90, 0, -90), glm::vec3(0.75, 3.05, 0.226), button6, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			buttonphys59.setIdentity(6);
			AttachCopy(button6, buttonphys59);
		}
		VertexArrayObject::Sptr button149act = ObjLoader::LoadFromFile("Models/barbutton.obj");
		{
			button7 = CreateEntity();

			//create texture
			Texture2D::Sptr buttonTexture8059act = Texture2D::Create("Textures/buttontexactivate.png");
			//material
			SMI_Material::Sptr buttongMa8059act = SMI_Material::Create();


			buttongMa8059act->setShader(shader);

			//set textures
			buttongMa8059act->setTexture(buttonTexture8059act, 0);
			//render
			Renderer buttongRen8059act = Renderer(buttongMa8059act, button149act);
			AttachCopy(button7, buttongRen8059act);
			//transform
			SMI_Transform buttonTrans180159act = SMI_Transform();

			buttonTrans180159act.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(button7, buttonTrans180159act);

			SMI_Physics buttonphys159 = SMI_Physics(glm::vec3(-12.5, -87.7, 15.1), glm::vec3(90, 0, -90), glm::vec3(0.75, 3.05, 0.226), button7, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			buttonphys159.setIdentity(6);
			AttachCopy(button7, buttonphys159);

		}
		/*VertexArrayObject::Sptr button1234 = ObjLoader::LoadFromFile("Models/button.obj");
		{
			button8 = CreateEntity();

			//create texture
			Texture2D::Sptr buttonTexture805134 = Texture2D::Create("Textures/buttontexactivate.png");
			//material
			SMI_Material::Sptr buttongMa805134 = SMI_Material::Create();


			buttongMa805134->setShader(shader);

			//set textures
			buttongMa805134->setTexture(buttonTexture805134, 0);
			//render
			Renderer buttongRen805134 = Renderer(buttongMa805134, button1234);
			AttachCopy(button8, buttongRen805134);
			//transform
			SMI_Transform buttonTrans18015134 = SMI_Transform();

			buttonTrans18015134.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(button8, buttonTrans18015134);

			SMI_Physics buttonphys5134 = SMI_Physics(glm::vec3(-28, 6.7, 3.1), glm::vec3(90, 0, -90), glm::vec3(0.75, 0.02, 0.226), button8, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			buttonphys5134.setIdentity(7);
			AttachCopy(button8, buttonphys5134);
		}
		*/
		VertexArrayObject::Sptr garbage115 = ObjLoader::LoadFromFile("Models/doortop.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr gTexture80175 = Texture2D::Create("Textures/bricktex.png");
			//material
			SMI_Material::Sptr gMa80175 = SMI_Material::Create();


			gMa80175->setShader(shader);

			//set textures
			gMa80175->setTexture(gTexture80175, 0);
			//render
			Renderer gRen80175 = Renderer(gMa80175, garbage115);
			AttachCopy(barrel, gRen80175);
			//transform
			SMI_Transform winTrans1801175 = SMI_Transform();

			winTrans1801175.setPos(glm::vec3(-12.5, 9.2, 10.0));

			winTrans1801175.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winTrans1801175);


			SMI_Physics doortop159 = SMI_Physics(glm::vec3(-12.5, 9.2, 10.0), glm::vec3(90, 0, -90), glm::vec3(14.6505, 10.0617, 0.112798), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			doortop159.setIdentity(2);
			AttachCopy(barrel, doortop159);
		}
		VertexArrayObject::Sptr door115 = ObjLoader::LoadFromFile("Models/doortop.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr doorgTexture80175 = Texture2D::Create("Textures/bricktex.png");
			//material
			SMI_Material::Sptr doorgMa80175 = SMI_Material::Create();


			doorgMa80175->setShader(shader);

			//set textures
			doorgMa80175->setTexture(doorgTexture80175, 0);
			//render
			Renderer doorgRen80175 = Renderer(doorgMa80175, door115);
			AttachCopy(barrel, doorgRen80175);
			//transform
			SMI_Transform doorwinTrans1801175 = SMI_Transform();

			doorwinTrans1801175.setPos(glm::vec3(-24.5, 9.2, 2.0));

			doorwinTrans1801175.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, doorwinTrans1801175);


			SMI_Physics doortop1591 = SMI_Physics(glm::vec3(-24.5, 9.2, 2.0), glm::vec3(90, 0, -90), glm::vec3(14.6505, 16.0617, 0.112798), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			doortop1591.setIdentity(2);
			AttachCopy(barrel, doortop1591);
		}
		VertexArrayObject::Sptr garbage2 = ObjLoader::LoadFromFile("Models/doorwall.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr gTexture801 = Texture2D::Create("Textures/brown1.png");
			//material
			SMI_Material::Sptr gMa801 = SMI_Material::Create();


			gMa801->setShader(shader);

			//set textures
			gMa801->setTexture(gTexture801, 0);
			//render
			Renderer gRen801 = Renderer(gMa801, garbage2);
			AttachCopy(barrel, gRen801);
			//transform
			SMI_Transform winTrans18011 = SMI_Transform();

			winTrans18011.setPos(glm::vec3(5.0, 8.8, 6.8));

			winTrans18011.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winTrans18011);
		}
		//Warehouse
		VertexArrayObject::Sptr wall41 = ObjLoader::LoadFromFile("Models/floor1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr window90Texture1 = Texture2D::Create("Textures/cement.png");
			//material
			SMI_Material::Sptr WallMat11 = SMI_Material::Create();
			WallMat11->setShader(shader);

			//set textures
			WallMat11->setTexture(window90Texture1, 0);
			//render
			Renderer WallRend11 = Renderer(WallMat11, wall41);
			AttachCopy(barrel, WallRend11);
			//transform
			SMI_Transform WallTrans11 = SMI_Transform();

			WallTrans11.setPos(glm::vec3(-75.6, 7, 0.8));

			WallTrans11.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, WallTrans11);

			SMI_Physics Ground1Phys = SMI_Physics(glm::vec3(-75.6, 7, 0.8), glm::vec3(90, 0, 90), glm::vec3(20.8, 3.32, 57.8), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
			Ground1Phys.setIdentity(2);
			AttachCopy(barrel, Ground1Phys);
		}

		VertexArrayObject::Sptr bag = ObjLoader::LoadFromFile("Models/bag1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr bagTexture1 = Texture2D::Create("Textures/bag.png");
			//material
			SMI_Material::Sptr bagMat11 = SMI_Material::Create();
			bagMat11->setShader(shader);

			//set textures
			bagMat11->setTexture(bagTexture1, 0);
			//render
			Renderer bagRend11 = Renderer(bagMat11, bag);
			AttachCopy(barrel, bagRend11);
			//transform
			SMI_Transform bagTrans11 = SMI_Transform();

			bagTrans11.setPos(glm::vec3(-54.7, 7.0, 2.1));

			bagTrans11.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, bagTrans11);

			SMI_Physics bagphys1 = SMI_Physics(glm::vec3(-54.7, 7.0, 2.1), glm::vec3(90, 0, 90), glm::vec3(5.48, 13.67, 1.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			bagphys1.setIdentity(2);
			AttachCopy(barrel, bagphys1);
		}

		VertexArrayObject::Sptr bag2 = ObjLoader::LoadFromFile("Models/bag2.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr bagTexture12 = Texture2D::Create("Textures/bag.png");
			//material
			SMI_Material::Sptr bagMat112 = SMI_Material::Create();
			bagMat112->setShader(shader);

			//set textures
			bagMat112->setTexture(bagTexture12, 0);
			//render
			Renderer bagRend112 = Renderer(bagMat112, bag2);
			AttachCopy(barrel, bagRend112);
			//transform
			SMI_Transform bagTrans112 = SMI_Transform();

			bagTrans112.setPos(glm::vec3(-50.7, 7.0, 2.1));

			bagTrans112.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, bagTrans112);

			SMI_Physics bagphys12 = SMI_Physics(glm::vec3(-50.7, 7.0, 2.1), glm::vec3(90, 0, 90), glm::vec3(4.52, 4.62, 1.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			bagphys12.setIdentity(2);
			AttachCopy(barrel, bagphys12);
		}
		VertexArrayObject::Sptr bag22 = ObjLoader::LoadFromFile("Models/barrelset.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr bagTexture122 = Texture2D::Create("Textures/Barrel.png");
			//material
			SMI_Material::Sptr bagMat1122 = SMI_Material::Create();
			bagMat1122->setShader(shader);

			//set textures
			bagMat1122->setTexture(bagTexture122, 0);
			//render
			Renderer bagRend1122 = Renderer(bagMat1122, bag22);
			AttachCopy(barrel, bagRend1122);
			//transform
			SMI_Transform bagTrans1122 = SMI_Transform();

			bagTrans1122.setPos(glm::vec3(-50.7, 2.0, 4.5));

			bagTrans1122.SetDegree(glm::vec3(90, 0, 0));
			AttachCopy(barrel, bagTrans1122);

		}
		VertexArrayObject::Sptr bag223 = ObjLoader::LoadFromFile("Models/barrelset.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr bagTexture1223 = Texture2D::Create("Textures/Barrel.png");
			//material
			SMI_Material::Sptr bagMat11223 = SMI_Material::Create();
			bagMat11223->setShader(shader);

			//set textures
			bagMat11223->setTexture(bagTexture1223, 0);
			//render
			Renderer bagRend11223 = Renderer(bagMat11223, bag223);
			AttachCopy(barrel, bagRend11223);
			//transform
			SMI_Transform bagTrans11223 = SMI_Transform();

			bagTrans11223.setPos(glm::vec3(-50.7, 19.0, 4.5));

			bagTrans11223.SetDegree(glm::vec3(90, 0, 0));
			AttachCopy(barrel, bagTrans11223);

		}
		VertexArrayObject::Sptr shelf = ObjLoader::LoadFromFile("Models/shelf12.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr shelfTexture1 = Texture2D::Create("Textures/shelf.png");
			//material
			SMI_Material::Sptr shelfMat11 = SMI_Material::Create();
			shelfMat11->setShader(shader);

			//set textures
			shelfMat11->setTexture(shelfTexture1, 0);
			//render
			Renderer bagRend11 = Renderer(shelfMat11, shelf);
			AttachCopy(barrel, bagRend11);
			//transform
			SMI_Transform shelfTrans11 = SMI_Transform();

			shelfTrans11.setPos(glm::vec3(-70.7, -1, 2.1));

			shelfTrans11.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, shelfTrans11);
		}
		VertexArrayObject::Sptr window231 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr window2Texture31 = Texture2D::Create("Textures/brown1.png");
			//material
			SMI_Material::Sptr window2Mat31 = SMI_Material::Create();
			window2Mat31->setShader(shader);

			//set textures
			window2Mat31->setTexture(window2Texture31, 0);
			//render
			Renderer window2Rend31 = Renderer(window2Mat31, window231);
			AttachCopy(barrel, window2Rend31);
			//transform
			SMI_Transform window2Trans31 = SMI_Transform();

			window2Trans31.setPos(glm::vec3(-55.5, -3.3, 1));

			window2Trans31.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, window2Trans31);
		}
		VertexArrayObject::Sptr doorw2 = ObjLoader::LoadFromFile("Models/bardoorway.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr doorw2801Tex = Texture2D::Create("Textures/bricktex.png");
			//material
			SMI_Material::Sptr doorw280Mat1 = SMI_Material::Create();


			doorw280Mat1->setShader(shader);

			//set textures
			doorw280Mat1->setTexture(doorw2801Tex, 0);
			//render
			Renderer doorw2801ren = Renderer(doorw280Mat1, doorw2);
			AttachCopy(barrel, doorw2801ren);
			//transform
			SMI_Transform doorw2Trans18011 = SMI_Transform();

			doorw2Trans18011.setPos(glm::vec3(-92.8, 29.5, 1.8));

			doorw2Trans18011.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, doorw2Trans18011);
		}
		VertexArrayObject::Sptr door1w2 = ObjLoader::LoadFromFile("Models/blockedbardoor.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr doorw2801Tex1 = Texture2D::Create("Textures/bricktex.png");
			//material
			SMI_Material::Sptr doorw280Mat11 = SMI_Material::Create();


			doorw280Mat11->setShader(shader);

			//set textures
			doorw280Mat11->setTexture(doorw2801Tex1, 0);
			//render
			Renderer doorw2801ren1 = Renderer(doorw280Mat11, door1w2);
			AttachCopy(barrel, doorw2801ren1);
			//transform
			SMI_Transform doorw2Trans180111 = SMI_Transform();

			doorw2Trans180111.setPos(glm::vec3(-92.8, 9.8, 3.0));

			doorw2Trans180111.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, doorw2Trans180111);


			SMI_Physics bbardoorphys1 = SMI_Physics(glm::vec3(-92.8, 9.8, 3.0), glm::vec3(90, 0, -90), glm::vec3(14.3917, 16.56, 0.472), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			bbardoorphys1.setIdentity(2);
			AttachCopy(barrel, bbardoorphys1);
		}
		VertexArrayObject::Sptr building = ObjLoader::LoadFromFile("Models/building1.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr build2801Tex = Texture2D::Create("Textures/build.png");
			//material
			SMI_Material::Sptr build280Mat1 = SMI_Material::Create();


			build280Mat1->setShader(shader);

			//set textures
			build280Mat1->setTexture(build2801Tex, 0);
			//render
			Renderer build2801ren = Renderer(build280Mat1, building);
			AttachCopy(barrel, build2801ren);
			//transform
			SMI_Transform build2Trans18011 = SMI_Transform();

			build2Trans18011.setPos(glm::vec3(-89.5, -2.3, 1.8));

			build2Trans18011.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, build2Trans18011);
		}
		VertexArrayObject::Sptr building2 = ObjLoader::LoadFromFile("Models/build4.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr build2801Tex2 = Texture2D::Create("Textures/2build.png");
			//material
			SMI_Material::Sptr build280Mat12 = SMI_Material::Create();


			build280Mat12->setShader(shader);

			//set textures
			build280Mat12->setTexture(build2801Tex2, 0);
			//render
			Renderer build2801ren2 = Renderer(build280Mat12, building2);
			AttachCopy(barrel, build2801ren2);
			//transform
			SMI_Transform build2Trans180112 = SMI_Transform();

			build2Trans180112.setPos(glm::vec3(-103.5, -2.3, 1.8));

			build2Trans180112.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, build2Trans180112);
		}
		VertexArrayObject::Sptr window2311 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr window2Texture311 = Texture2D::Create("Textures/brown1.png");
			//material
			SMI_Material::Sptr window2Mat311 = SMI_Material::Create();
			window2Mat311->setShader(shader);

			//set textures
			window2Mat311->setTexture(window2Texture311, 0);
			//render
			Renderer window2Rend311 = Renderer(window2Mat311, window2311);
			AttachCopy(barrel, window2Rend311);
			//transform
			SMI_Transform window2Trans311 = SMI_Transform();

			window2Trans311.setPos(glm::vec3(-80.5, -3.3, 1));

			window2Trans311.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, window2Trans311);
		}
		VertexArrayObject::Sptr car = ObjLoader::LoadFromFile("Models/car.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr carTexture80 = Texture2D::Create("Textures/car_Tex.png");
			//material
			SMI_Material::Sptr cargMa80 = SMI_Material::Create();


			cargMa80->setShader(shader);

			//set textures
			cargMa80->setTexture(carTexture80, 0);
			//render
			Renderer cargRen80 = Renderer(cargMa80, car);
			AttachCopy(barrel, cargRen80);
			//transform
			SMI_Transform carTrans1801 = SMI_Transform();

			carTrans1801.setPos(glm::vec3(-98.5, 1.8, 3.5));

			carTrans1801.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, carTrans1801);
		}
		VertexArrayObject::Sptr car1 = ObjLoader::LoadFromFile("Models/car.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr carTexture801 = Texture2D::Create("Textures/car_Tex.png");
			//material
			SMI_Material::Sptr cargMa801 = SMI_Material::Create();


			cargMa801->setShader(shader);

			//set textures
			cargMa801->setTexture(carTexture801, 0);
			//render
			Renderer cargRen801 = Renderer(cargMa801, car1);
			AttachCopy(barrel, cargRen801);
			//transform
			SMI_Transform carTrans18011 = SMI_Transform();

			carTrans18011.setPos(glm::vec3(-118.5, 6.8, 2.7));

			carTrans18011.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, carTrans18011);

			SMI_Physics carphys = SMI_Physics(glm::vec3(-118.5, 6.8, 2.7), glm::vec3(90, 0, 90), glm::vec3(3.17, 5.15, 10.6), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			carphys.setIdentity(2);
			AttachCopy(barrel, carphys);
		}
		VertexArrayObject::Sptr car3 = ObjLoader::LoadFromFile("Models/car.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr carTexture8013 = Texture2D::Create("Textures/car_Tex.png");
			//material
			SMI_Material::Sptr cargMa8013 = SMI_Material::Create();


			cargMa8013->setShader(shader);

			//set textures
			cargMa8013->setTexture(carTexture8013, 0);
			//render
			Renderer cargRen8013 = Renderer(cargMa8013, car3);
			AttachCopy(barrel, cargRen8013);
			//transform
			SMI_Transform carTrans180113 = SMI_Transform();

			carTrans180113.setPos(glm::vec3(-130.5, 6.8, 2.7));

			carTrans180113.SetDegree(glm::vec3(90, 0, -120));
			AttachCopy(barrel, carTrans180113);

			SMI_Physics carphys2 = SMI_Physics(glm::vec3(-130.5, 6.8, 2.7), glm::vec3(90, 0, -120), glm::vec3(3.17, 5.15, 10.6), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			carphys2.setIdentity(2);
			AttachCopy(barrel, carphys2);
		}
		VertexArrayObject::Sptr ashphalt = ObjLoader::LoadFromFile("Models/floor2.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr ashphaltTexture80 = Texture2D::Create("Textures/gravel.png");
			//material
			SMI_Material::Sptr ashphaltgMa80 = SMI_Material::Create();


			ashphaltgMa80->setShader(shader);

			//set textures
			ashphaltgMa80->setTexture(ashphaltTexture80, 0);
			//render
			Renderer ashphaltgRen80 = Renderer(ashphaltgMa80, ashphalt);
			AttachCopy(barrel, ashphaltgRen80);
			//transform
			SMI_Transform ashphaltTrans1801 = SMI_Transform();

			ashphaltTrans1801.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, ashphaltTrans1801);

			SMI_Physics gravelphys = SMI_Physics(glm::vec3(-112.5, 3, -0.8), glm::vec3(90, 0, -90), glm::vec3(20.8154, 4.62269, 77.7581), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
			gravelphys.setIdentity(2);
			AttachCopy(barrel, gravelphys);
		}
		VertexArrayObject::Sptr plank = ObjLoader::LoadFromFile("Models/plank.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr plankTexture80 = Texture2D::Create("Textures/spike.png");
			//material
			SMI_Material::Sptr plankgMa80 = SMI_Material::Create();


			plankgMa80->setShader(shader);

			//set textures
			plankgMa80->setTexture(plankTexture80, 0);
			//render
			Renderer plankgRen80 = Renderer(plankgMa80, plank);
			AttachCopy(barrel, plankgRen80);
			//transform
			SMI_Transform plankTrans1801 = SMI_Transform();

			plankTrans1801.setPos(glm::vec3(-122.5, 6.5, 8.8));

			plankTrans1801.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, plankTrans1801);

			SMI_Physics plankphys = SMI_Physics(glm::vec3(-122.5, 6.5, 8.8), glm::vec3(90, 0, -90), glm::vec3(4.42609, 2.172688, 9.0021), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			plankphys.setIdentity(2);
			AttachCopy(barrel, plankphys);
		}
		VertexArrayObject::Sptr building6 = ObjLoader::LoadFromFile("Models/build4.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr tbuild2801Tex2 = Texture2D::Create("Textures/2build texture.png");
			//material
			SMI_Material::Sptr tbuild280Mat12 = SMI_Material::Create();


			tbuild280Mat12->setShader(shader);

			//set textures
			tbuild280Mat12->setTexture(tbuild2801Tex2, 0);
			//render
			Renderer tbuild2801ren2 = Renderer(tbuild280Mat12, building6);
			AttachCopy(barrel, tbuild2801ren2);
			//transform
			SMI_Transform tbuild2Trans180112 = SMI_Transform();

			tbuild2Trans180112.setPos(glm::vec3(-132.0, -2.3, 1.8));

			tbuild2Trans180112.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, tbuild2Trans180112);
		}
		VertexArrayObject::Sptr fan1 = ObjLoader::LoadFromFile("Models/Cfan1.obj");
		{
			fan = CreateEntity();
			//create texture
			Texture2D::Sptr twbuild2801Tex2 = Texture2D::Create("Textures/fan.png");
			//material
			SMI_Material::Sptr twbuild280Mat12 = SMI_Material::Create();


			twbuild280Mat12->setShader(shader);

			//set textures
			twbuild280Mat12->setTexture(twbuild2801Tex2, 0);
			//render
			Renderer twbuild2801ren2 = Renderer(twbuild280Mat12, fan1);
			AttachCopy(fan, twbuild2801ren2);
			//transform
			SMI_Transform twbuild2Trans180112 = SMI_Transform();

			twbuild2Trans180112.setPos(glm::vec3(-61.0, 4.1, 3.8));

			twbuild2Trans180112.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(fan, twbuild2Trans180112);

			SMI_Physics fanPhys = SMI_Physics(glm::vec3(-61.0, 4.1, 3.8), glm::vec3(90, 0, 90), glm::vec3(5.05, 5.62, 0),fan, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			fanPhys.setIdentity(7);
			AttachCopy(fan, fanPhys);
		}
		VertexArrayObject::Sptr fan22 = ObjLoader::LoadFromFile("Models/Cfan1.obj");
		{
			fan2 = CreateEntity();
			//create texture
			Texture2D::Sptr twbuild2801Tex22 = Texture2D::Create("Textures/fan.png");
			//material
			SMI_Material::Sptr twbuild280Mat122 = SMI_Material::Create();


			twbuild280Mat122->setShader(shader);

			//set textures
			twbuild280Mat122->setTexture(twbuild2801Tex22, 0);
			//render
			Renderer twbuild2801ren22 = Renderer(twbuild280Mat122, fan22);
			AttachCopy(fan2, twbuild2801ren22);
			//transform
			SMI_Transform twbuild2Trans1801122 = SMI_Transform();

			twbuild2Trans1801122.setPos(glm::vec3(-67.0, 4.1, 3.8));

			twbuild2Trans1801122.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(fan2, twbuild2Trans1801122);

			SMI_Physics fanPhys2 = SMI_Physics(glm::vec3(-67.0, 4.1, 3.8), glm::vec3(90, 0, 90), glm::vec3(5.05, 5.62, 0), fan2, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			fanPhys2.setIdentity(7);
			AttachCopy(fan2, fanPhys2);
		}
		VertexArrayObject::Sptr elevator1 = ObjLoader::LoadFromFile("Models/elevator.obj");
		{
			elevator = CreateEntity();
			//create texture
			Texture2D::Sptr twbuild2801Tex21 = Texture2D::Create("Textures/elevator.png");
			//material
			SMI_Material::Sptr twbuild280Mat121 = SMI_Material::Create();


			twbuild280Mat121->setShader(shader);
			
			//set textures
			twbuild280Mat121->setTexture(twbuild2801Tex21, 0);
			//render
			Renderer twbuild2801ren21 = Renderer(twbuild280Mat121, elevator1);
			AttachCopy(elevator, twbuild2801ren21);
			//transform
			SMI_Transform twbuild2Trans1801121 = SMI_Transform();

			twbuild2Trans1801121.setPos(glm::vec3(-75.0, 7.0, 1.8));

			twbuild2Trans1801121.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(elevator, twbuild2Trans1801121);

			SMI_Physics elevator1Phys = SMI_Physics(glm::vec3(-75.0, 7.0, -1.8), glm::vec3(90, 0, -90), glm::vec3(1.14, 4.05, 5.55), elevator, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			elevator1Phys.setIdentity(2);
			AttachCopy(elevator, elevator1Phys);
		}
		VertexArrayObject::Sptr warehouseplank = ObjLoader::LoadFromFile("Models/plank.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr warehousewallTex21 = Texture2D::Create("Textures/platform.png");
			//material
			SMI_Material::Sptr warehousewall121 = SMI_Material::Create();


			warehousewall121->setShader(shader);

			//set textures
			warehousewall121->setTexture(warehousewallTex21, 0);
			//render
			Renderer warehousewallren21 = Renderer(warehousewall121, warehouseplank);
			AttachCopy(barrel, warehousewallren21);
			//transform
			SMI_Transform warehousewallTrans1801121 = SMI_Transform();

			warehousewallTrans1801121.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, warehousewallTrans1801121);

			SMI_Physics plankphys1 = SMI_Physics(glm::vec3(-82.0, 7.0, 10.8), glm::vec3(90, 0, -90), glm::vec3(10.3, 1.82, 8.8), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
			plankphys1.setIdentity(2);
			AttachCopy(barrel, plankphys1);

		}
		VertexArrayObject::Sptr warehouseplank1 = ObjLoader::LoadFromFile("Models/plank.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr warehousewallTex211 = Texture2D::Create("Textures/platform.png");
			//material
			SMI_Material::Sptr warehousewall1211 = SMI_Material::Create();


			warehousewall1211->setShader(shader);

			//set textures
			warehousewall1211->setTexture(warehousewallTex211, 0);
			//render
			Renderer warehousewallren211 = Renderer(warehousewall1211, warehouseplank1);
			AttachCopy(barrel, warehousewallren211);
			//transform
			SMI_Transform warehousewallTrans18011211 = SMI_Transform();

			warehousewallTrans18011211.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, warehousewallTrans18011211);

			SMI_Physics plankphys11 = SMI_Physics(glm::vec3(-89.0, 7.0, 10.8), glm::vec3(90, 0, -90), glm::vec3(10.3, 1.82, 8.8), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
			plankphys11.setIdentity(2);
			AttachCopy(barrel, plankphys11);

		}
		VertexArrayObject::Sptr railing = ObjLoader::LoadFromFile("Models/railing.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr railingTex21 = Texture2D::Create("Textures/railing.png");
			//material
			SMI_Material::Sptr railing121 = SMI_Material::Create();


			railing121->setShader(shader);

			//set textures
			railing121->setTexture(railingTex21, 0);
			//render
			Renderer railingren21 = Renderer(railing121, railing);
			AttachCopy(barrel, railingren21);
			//transform
			SMI_Transform railingTrans1801121 = SMI_Transform();

			railingTrans1801121.setPos(glm::vec3(-81.8, 12.0, 12.8));

			railingTrans1801121.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, railingTrans1801121);

		}
		VertexArrayObject::Sptr pillar = ObjLoader::LoadFromFile("Models/concretepillar.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr pillarTex21 = Texture2D::Create("Textures/shelf.png");
			//material
			SMI_Material::Sptr pillar121 = SMI_Material::Create();


			pillar121->setShader(shader);

			//set textures
			pillar121->setTexture(pillarTex21, 0);
			//render
			Renderer pillarren21 = Renderer(pillar121, pillar);
			AttachCopy(barrel, pillarren21);
			//transform
			SMI_Transform pillarTrans1801121 = SMI_Transform();

			pillarTrans1801121.setPos(glm::vec3(-82.0, 7.0, 3.5));

			pillarTrans1801121.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, pillarTrans1801121);

		}
		VertexArrayObject::Sptr pillar1 = ObjLoader::LoadFromFile("Models/smallerpillar.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr pillarTex211 = Texture2D::Create("Textures/shelf.png");
			//material
			SMI_Material::Sptr pillar1211 = SMI_Material::Create();


			pillar1211->setShader(shader);

			//set textures
			pillar1211->setTexture(pillarTex211, 0);
			//render
			Renderer pillarren211 = Renderer(pillar1211, pillar1);
			AttachCopy(barrel, pillarren211);
			//transform
			SMI_Transform pillarTrans18011211 = SMI_Transform();

			pillarTrans18011211.setPos(glm::vec3(-89.5, 7.0, 3.5));

			pillarTrans18011211.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, pillarTrans18011211);

		}
		VertexArrayObject::Sptr winwall = ObjLoader::LoadFromFile("Models/winwalls.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr winwallTex21 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr winwall121 = SMI_Material::Create();


			winwall121->setShader(shader);

			//set textures
			winwall121->setTexture(winwallTex21, 0);
			//render
			Renderer winwallren21 = Renderer(winwall121, winwall);
			AttachCopy(barrel, winwallren21);
			//transform
			SMI_Transform winwallTrans1801121 = SMI_Transform();

			winwallTrans1801121.setPos(glm::vec3(-151.0, 7.0, 2.5));

			winwallTrans1801121.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winwallTrans1801121);

		}
		VertexArrayObject::Sptr winwall1 = ObjLoader::LoadFromFile("Models/winwalls1.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr winwall1Tex211 = Texture2D::Create("Textures/brick1.png");
			//material
			SMI_Material::Sptr winwall11211 = SMI_Material::Create();


			winwall11211->setShader(shader);

			//set textures
			winwall11211->setTexture(winwall1Tex211, 0);
			//render
			Renderer winwall1ren211 = Renderer(winwall11211, winwall1);
			AttachCopy(barrel, winwall1ren211);
			//transform
			SMI_Transform winwallTrans118011211 = SMI_Transform();


			winwallTrans118011211.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winwallTrans118011211);

			SMI_Physics windowphys1 = SMI_Physics(glm::vec3(-151.0, 7.0, 15.4), glm::vec3(90, 0, -90), glm::vec3(7.11048, 0.19836, 0.601408), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
			windowphys1.setIdentity(2);
			AttachCopy(barrel, windowphys1);

		}
		VertexArrayObject::Sptr winwall2 = ObjLoader::LoadFromFile("Models/winwalls1.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr winwall1Tex2112 = Texture2D::Create("Textures/brick1.png");
			//material
			SMI_Material::Sptr winwall112112 = SMI_Material::Create();


			winwall112112->setShader(shader);

			//set textures
			winwall112112->setTexture(winwall1Tex2112, 0);
			//render
			Renderer winwall1ren2112 = Renderer(winwall112112, winwall2);
			AttachCopy(barrel, winwall1ren2112);
			//transform
			SMI_Transform winwallTrans1180112112 = SMI_Transform();


			winwallTrans1180112112.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winwallTrans1180112112);

			SMI_Physics windowphys12 = SMI_Physics(glm::vec3(-151.0, 7.0, 10.4), glm::vec3(90, 0, -90), glm::vec3(7.11048, 0.189836, 0.601408), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
			windowphys12.setIdentity(2);
			AttachCopy(barrel, windowphys12);

		}
		VertexArrayObject::Sptr ashphalt5 = ObjLoader::LoadFromFile("Models/wood.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr ashphaltTexture805 = Texture2D::Create("Textures/lounge.png");
			//material
			SMI_Material::Sptr ashphaltgMa805 = SMI_Material::Create();


			ashphaltgMa805->setShader(shader);

			//set textures
			ashphaltgMa805->setTexture(ashphaltTexture805, 0);
			//render
			Renderer ashphaltgRen805 = Renderer(ashphaltgMa805, ashphalt5);
			AttachCopy(barrel, ashphaltgRen805);
			//transform
			SMI_Transform ashphaltTrans18015 = SMI_Transform();

			ashphaltTrans18015.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, ashphaltTrans18015);

			SMI_Physics gravelphys5 = SMI_Physics(glm::vec3(-169.5, 6, -0.8), glm::vec3(90, 0, -90), glm::vec3(20.8154, 4.62269, 77.7581), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
			gravelphys5.setIdentity(2);
			AttachCopy(barrel, gravelphys5);
		}
		VertexArrayObject::Sptr door21 = ObjLoader::LoadFromFile("Models/door2.obj");
		{
			door2 = CreateEntity();

			//create texture
			Texture2D::Sptr door2Texture805 = Texture2D::Create("Textures/doortex.png");
			//material
			SMI_Material::Sptr door2gMa805 = SMI_Material::Create();


			door2gMa805->setShader(shader);

			//set textures
			door2gMa805->setTexture(door2Texture805, 0);
			//render
			Renderer door2gRen805 = Renderer(door2gMa805, door21);
			AttachCopy(door2, door2gRen805);
			//transform
			SMI_Transform door2Trans18015 = SMI_Transform();

			door2Trans18015.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(door2, door2Trans18015);

			SMI_Physics door2phys5 = SMI_Physics(glm::vec3(-151.0, 7.0, 2.5), glm::vec3(90, 0, -90), glm::vec3(4.02,16.298,0.13), door2, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			door2phys5.setIdentity(2);
			AttachCopy(door2, door2phys5);
		}
		VertexArrayObject::Sptr button14 = ObjLoader::LoadFromFile("Models/button.obj");
		{
			button = CreateEntity();

			//create texture
			Texture2D::Sptr buttonTexture805 = Texture2D::Create("Textures/buttontex.png");
			//material
			SMI_Material::Sptr buttongMa805 = SMI_Material::Create();


			buttongMa805->setShader(shader);

			//set textures
			buttongMa805->setTexture(buttonTexture805, 0);
			//render
			Renderer buttongRen805 = Renderer(buttongMa805, button14);
			AttachCopy(button, buttongRen805);
			//transform
			SMI_Transform buttonTrans18015 = SMI_Transform();

			buttonTrans18015.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(button, buttonTrans18015);

			SMI_Physics buttonphys5 = SMI_Physics(glm::vec3(-158.0, 6.7, 2.1), glm::vec3(90, 0, -90), glm::vec3(0.75,0.05,0.226), button, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			buttonphys5.setIdentity(3);
			AttachCopy(button, buttonphys5);
		}
		VertexArrayObject::Sptr insidewall = ObjLoader::LoadFromFile("Models/inside.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr insideTexture805 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr insidegMa805 = SMI_Material::Create();


			insidegMa805->setShader(shader);

			//set textures
			insidegMa805->setTexture(insideTexture805, 0);
			//render
			Renderer insidegRen805 = Renderer(insidegMa805, insidewall);
			AttachCopy(barrel, insidegRen805);
			//transform
			SMI_Transform insideTrans18015 = SMI_Transform();

			insideTrans18015.setPos(glm::vec3(-155.8, -2.7, 1.3));

			insideTrans18015.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, insideTrans18015);
		}
		VertexArrayObject::Sptr insidewall1 = ObjLoader::LoadFromFile("Models/inside.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr insideTexture8051 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr insidegMa8051 = SMI_Material::Create();


			insidegMa8051->setShader(shader);

			//set textures
			insidegMa8051->setTexture(insideTexture8051, 0);
			//render
			Renderer insidegRen8051 = Renderer(insidegMa8051, insidewall1);
			AttachCopy(barrel, insidegRen8051);
			//transform
			SMI_Transform insideTrans180151 = SMI_Transform();

			insideTrans180151.setPos(glm::vec3(-171.4, -2.7, 1.3));

			insideTrans180151.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, insideTrans180151);
		}
		VertexArrayObject::Sptr button12 = ObjLoader::LoadFromFile("Models/button.obj");
		{
			button1 = CreateEntity();

			//create texture
			Texture2D::Sptr buttonTexture8051 = Texture2D::Create("Textures/buttontexactivate.png");
			//material
			SMI_Material::Sptr buttongMa8051 = SMI_Material::Create();


			buttongMa8051->setShader(shader);

			//set textures
			buttongMa8051->setTexture(buttonTexture8051, 0);
			//render
			Renderer buttongRen8051 = Renderer(buttongMa8051, button12);
			AttachCopy(button1, buttongRen8051);
			//transform
			SMI_Transform buttonTrans180151 = SMI_Transform();

			buttonTrans180151.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(button1, buttonTrans180151);

			SMI_Physics buttonphys51 = SMI_Physics(glm::vec3(-158.0, -34.7, 2.1), glm::vec3(90, 0, -90), glm::vec3(0.75, -100.05, 0.226), button1, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			buttonphys51.setIdentity(3);
			AttachCopy(button1, buttonphys51);
		}
		VertexArrayObject::Sptr winwall5 = ObjLoader::LoadFromFile("Models/winwalls3.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr winwallTex215 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr winwall1215 = SMI_Material::Create();


			winwall1215->setShader(shader);

			//set textures
			winwall1215->setTexture(winwallTex215, 0);
			//render
			Renderer winwallren215 = Renderer(winwall1215, winwall5);
			AttachCopy(barrel, winwallren215);
			//transform
			SMI_Transform winwallTrans18011215 = SMI_Transform();

			winwallTrans18011215.setPos(glm::vec3(-166.7, 7.0, 2.5));

			winwallTrans18011215.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winwallTrans18011215);

		}
		VertexArrayObject::Sptr door212 = ObjLoader::LoadFromFile("Models/door2.obj");
		{
			door3 = CreateEntity();

			//create texture
			Texture2D::Sptr door2Texture8052 = Texture2D::Create("Textures/doortex.png");
			//material
			SMI_Material::Sptr door2gMa8052 = SMI_Material::Create();


			door2gMa8052->setShader(shader);

			//set textures
			door2gMa8052->setTexture(door2Texture8052, 0);
			//render
			Renderer door2gRen8052 = Renderer(door2gMa8052, door212);
			AttachCopy(door3, door2gRen8052);
			//transform
			SMI_Transform door2Trans180152 = SMI_Transform();

			door2Trans180152.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(door3, door2Trans180152);

			SMI_Physics door2phys52 = SMI_Physics(glm::vec3(-166.7, 7.0, 2.5), glm::vec3(90, 0, -90), glm::vec3(4.02, 40.298, 0.13), door3, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			door2phys52.setIdentity(2);
			AttachCopy(door3, door2phys52);
		}
		VertexArrayObject::Sptr crate19 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex19 = Texture2D::Create("Textures/box32.png");
			//material
			SMI_Material::Sptr BarrelMat519 = SMI_Material::Create();
			BarrelMat519->setShader(shader);

			//set textures
			BarrelMat519->setTexture(crateTex19, 0);
			//render
			Renderer BarrelRend519 = Renderer(BarrelMat519, crate19);
			AttachCopy(barrel, BarrelRend519);
			//transform
			SMI_Transform BarrelTrans519 = SMI_Transform();

			BarrelTrans519.setPos(glm::vec3(-122.0, 7.0, 14.4));

			BarrelTrans519.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans519);

			SMI_Physics cratephys52 = SMI_Physics(glm::vec3(-122.0, 7.0, 14.4), glm::vec3(90, 0, -90), glm::vec3(2.15, 1.97, 3.2), barrel, SMI_PhysicsBodyType::DYNAMIC, 1.0f);
			cratephys52.setHasGravity(true);
			cratephys52.setIdentity(4);
			AttachCopy(barrel, cratephys52);
		}
		VertexArrayObject::Sptr button123 = ObjLoader::LoadFromFile("Models/button.obj");
		{
			button5 = CreateEntity();

			//create texture
			Texture2D::Sptr buttonTexture80513 = Texture2D::Create("Textures/buttontexactivate.png");
			//material
			SMI_Material::Sptr buttongMa80513 = SMI_Material::Create();


			buttongMa80513->setShader(shader);

			//set textures
			buttongMa80513->setTexture(buttonTexture80513, 0);
			//render
			Renderer buttongRen80513 = Renderer(buttongMa80513, button123);
			AttachCopy(button5, buttongRen80513);
			//transform
			SMI_Transform buttonTrans1801513 = SMI_Transform();

			buttonTrans1801513.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(button5, buttonTrans1801513);

			SMI_Physics buttonphys513 = SMI_Physics(glm::vec3(-163.0, 6.7, 2.1), glm::vec3(90, 0, -90), glm::vec3(0.75, 0.02, 0.226), button5, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			buttonphys513.setIdentity(5);
			AttachCopy(button5, buttonphys513);
		}

		VertexArrayObject::Sptr enemy = ObjLoader::LoadFromFile("Models/enemy.obj");
		{
			en = CreateEntity();

			//create texture
			Texture2D::Sptr enemyCharacterTex = Texture2D::Create("Textures/enemy.png");
			//create material
			SMI_Material::Sptr enemyCharacterMat = SMI_Material::Create();
			enemyCharacterMat->setShader(shader);

			//set texture
			enemyCharacterMat->setTexture(enemyCharacterTex, 0);
			//render
			Renderer enemyCharacterRend = Renderer(enemyCharacterMat, enemy);
			AttachCopy(en, enemyCharacterRend);
			//transform & Physics
			SMI_Transform enemyCharaTrans = SMI_Transform();
			enemyCharaTrans.setPos(glm::vec3(-181.0, 7.8, 2.2));

			enemyCharaTrans.SetDegree(glm::vec3(90, 0, 90));
			enemyCharaTrans.setScale(glm::vec3(0.25, 0.25, 0.25));
			AttachCopy(en, enemyCharaTrans);

			SMI_Physics enemyPhys = SMI_Physics(glm::vec3(-181.0, 7.8, 2.2), glm::vec3(90, 0, -90), glm::vec3(2, 3, 1), en, SMI_PhysicsBodyType::KINEMATIC, 1.0f);

			enemyPhys.setIdentity(10);
			AttachCopy(en, enemyPhys);
		}

		VertexArrayObject::Sptr enemy1 = ObjLoader::LoadFromFile("Models/denemy.obj");
		{
			en1 = CreateEntity();

			//create texture
			Texture2D::Sptr enemyCharacterTex1 = Texture2D::Create("Textures/enemy.png");
			//create material
			SMI_Material::Sptr enemyCharacterMat1 = SMI_Material::Create();
			enemyCharacterMat1->setShader(shader);

			//set texture
			enemyCharacterMat1->setTexture(enemyCharacterTex1, 0);
			//render
			Renderer enemyCharacterRend1 = Renderer(enemyCharacterMat1, enemy1);
			AttachCopy(en1, enemyCharacterRend1);
			//transform & Physics
			SMI_Transform enemyCharaTrans1 = SMI_Transform();
			enemyCharaTrans1.setPos(glm::vec3(-181.0, 37.8, 2.3));

			enemyCharaTrans1.SetDegree(glm::vec3(90, 0, 90));
			enemyCharaTrans1.setScale(glm::vec3(0.25, 0.25, 0.25));
			AttachCopy(en1, enemyCharaTrans1);

			SMI_Physics enemyPhys1 = SMI_Physics(glm::vec3(-181.0, 37.8, 2.3), glm::vec3(90, 0, 90), glm::vec3(2, 0.0, 1), en1, SMI_PhysicsBodyType::KINEMATIC, 1.0f);

			enemyPhys1.setIdentity(10);
			AttachCopy(en1, enemyPhys1);
		}
		VertexArrayObject::Sptr bullet1 = ObjLoader::LoadFromFile("Models/bullet.obj");
		{
			bullet = CreateEntity();

			//create texture
			Texture2D::Sptr bulletTexture80513 = Texture2D::Create("Textures/brown1.png");
			//material
			SMI_Material::Sptr bulletgMa80513 = SMI_Material::Create();


			bulletgMa80513->setShader(shader);

			//set textures
			bulletgMa80513->setTexture(bulletTexture80513, 0);
			//render
			Renderer bulletgRen80513 = Renderer(bulletgMa80513, bullet1);
			AttachCopy(bullet, bulletgRen80513);
			//transform
			SMI_Transform bulletTrans1801513 = SMI_Transform();

			bulletTrans1801513.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(bullet, bulletTrans1801513);

			SMI_Physics bulletphys513 = SMI_Physics(glm::vec3(-179.0, 7.2, -87.9), glm::vec3(90, 0, -90), glm::vec3(0.23, 2.819, 0.23), bullet, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			bulletphys513.setIdentity(9);
			AttachCopy(bullet, bulletphys513);
		}
		VertexArrayObject::Sptr end = ObjLoader::LoadFromFile("Models/wi1.obj");
		{
			ed = CreateEntity();

			//create texture
			Texture2D::Sptr bulletTexture805133 = Texture2D::Create("Textures/rough.png");
			//material
			SMI_Material::Sptr bulletgMa805133 = SMI_Material::Create();


			bulletgMa805133->setShader(shader);

			//set textures
			bulletgMa805133->setTexture(bulletTexture805133, 0);
			//render
			Renderer bulletgRen805133 = Renderer(bulletgMa805133, end);
			AttachCopy(ed, bulletgRen805133);
			//transform
			SMI_Transform bulletTrans18015133 = SMI_Transform();
			
			bulletTrans18015133.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(ed, bulletTrans18015133);

			SMI_Physics bulletphys5133 = SMI_Physics(glm::vec3(-879.0, -7.2, -8.9), glm::vec3(90, 0, -90), glm::vec3(0.23, 2.819, 0.23), ed, SMI_PhysicsBodyType::STATIC, 1.0f);
			bulletphys5133.setIdentity(9);
			AttachCopy(ed, bulletphys5133);
		}
		VertexArrayObject::Sptr crate119 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex19 = Texture2D::Create("Textures/box32.png");
			//material
			SMI_Material::Sptr BarrelMat519 = SMI_Material::Create();
			BarrelMat519->setShader(shader);

			//set textures
			BarrelMat519->setTexture(crateTex19, 0);
			//render
			Renderer BarrelRend519 = Renderer(BarrelMat519, crate119);
			AttachCopy(barrel, BarrelRend519);
			//transform
			SMI_Transform BarrelTrans519 = SMI_Transform();

			BarrelTrans519.setPos(glm::vec3(-182.0, 8.2, 14.4));

			BarrelTrans519.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans519);

			SMI_Physics crate1phys52 = SMI_Physics(glm::vec3(-182.0, 8.2, 14.4), glm::vec3(90, 0, -90), glm::vec3(2.15, 1.97, 3.2), barrel, SMI_PhysicsBodyType::DYNAMIC, 1.0f);
			crate1phys52.setHasGravity(true);
			crate1phys52.setIdentity(11);
			AttachCopy(barrel, crate1phys52);
		}
		VertexArrayObject::Sptr plank5 = ObjLoader::LoadFromFile("Models/splank.obj");
		{
			planks = CreateEntity();

			//create texture
			Texture2D::Sptr plankTexture805 = Texture2D::Create("Textures/platform.png");
			//material
			SMI_Material::Sptr plankgMa805 = SMI_Material::Create();


			plankgMa805->setShader(shader);

			//set textures
			plankgMa805->setTexture(plankTexture805, 0);
			//render
			Renderer plankgRen805 = Renderer(plankgMa805, plank5);
			AttachCopy(planks, plankgRen805);
			//transform
			SMI_Transform plankTrans18015 = SMI_Transform();

			plankTrans18015.setPos(glm::vec3(-182.5, 6.5, 8.8));

			plankTrans18015.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(planks, plankTrans18015);

			SMI_Physics plankphys5 = SMI_Physics(glm::vec3(-182.5, 6.5, 8.8), glm::vec3(90, 0, -90), glm::vec3(4.42609, 2.173, 4.8), planks, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			plankphys5.setIdentity(2);
			AttachCopy(planks, plankphys5);
		}
		VertexArrayObject::Sptr button1239 = ObjLoader::LoadFromFile("Models/button.obj");
		{
			button9 = CreateEntity();

			//create texture
			Texture2D::Sptr buttonTexture805139 = Texture2D::Create("Textures/buttontexactivate.png");
			//material
			SMI_Material::Sptr buttongMa805139 = SMI_Material::Create();


			buttongMa805139->setShader(shader);

			//set textures
			buttongMa805139->setTexture(buttonTexture805139, 0);
			//render
			Renderer buttongRen805139 = Renderer(buttongMa805139, button1239);
			AttachCopy(button9, buttongRen805139);
			//transform
			SMI_Transform buttonTrans18015139 = SMI_Transform();

			buttonTrans18015139.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(button9, buttonTrans18015139);

			SMI_Physics buttonphys5139 = SMI_Physics(glm::vec3(-175.0, 6.7, 2.1), glm::vec3(90, 0, -90), glm::vec3(0.75, 0.02, 0.226), button9, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			buttonphys5139.setIdentity(12);
			AttachCopy(button9, buttonphys5139);
		}
		VertexArrayObject::Sptr insidewall2 = ObjLoader::LoadFromFile("Models/inside.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr insideTexture8052 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr insidegMa8052 = SMI_Material::Create();


			insidegMa8052->setShader(shader);

			//set textures
			insidegMa8052->setTexture(insideTexture8052, 0);
			//render
			Renderer insidegRen8052 = Renderer(insidegMa8052, insidewall2);
			AttachCopy(barrel, insidegRen8052);
			//transform
			SMI_Transform insideTrans180152 = SMI_Transform();

			insideTrans180152.setPos(glm::vec3(-187.2, -2.7, 1.3));

			insideTrans180152.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, insideTrans180152);
		}
		VertexArrayObject::Sptr winwall7 = ObjLoader::LoadFromFile("Models/winwalls3.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr winwallTex21511 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr winwall121511 = SMI_Material::Create();


			winwall121511->setShader(shader);

			//set textures
			winwall121511->setTexture(winwallTex21511, 0);
			//render
			Renderer winwallren21511 = Renderer(winwall121511, winwall7);
			AttachCopy(barrel, winwallren21511);
			//transform
			SMI_Transform winwallTrans1801121511 = SMI_Transform();

			winwallTrans1801121511.setPos(glm::vec3(-185.7, 7.0, 2.5));

			winwallTrans1801121511.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winwallTrans1801121511);

		}

		VertexArrayObject::Sptr plank5hold = ObjLoader::LoadFromFile("Models/plankhold.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr plankTexture805hold = Texture2D::Create("Textures/spike.png");
			//material
			SMI_Material::Sptr plankgMa80hold5 = SMI_Material::Create();


			plankgMa80hold5->setShader(shader);

			//set textures
			plankgMa80hold5->setTexture(plankTexture805hold, 0);
			//render
			Renderer plankgRen805hold = Renderer(plankgMa80hold5, plank5hold);
			AttachCopy(barrel, plankgRen805hold);
			//transform
			SMI_Transform plankTrans18015hold = SMI_Transform();

			plankTrans18015hold.setPos(glm::vec3(-182.5, 7.5, 2.8));

			plankTrans18015hold.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, plankTrans18015hold);
		}
	
		VertexArrayObject::Sptr button12391 = ObjLoader::LoadFromFile("Models/button.obj");
		{
			button10 = CreateEntity();

			//create texture
			Texture2D::Sptr buttonTexture8051391 = Texture2D::Create("Textures/buttontexactivate.png");
			//material
			SMI_Material::Sptr buttongMa8051391 = SMI_Material::Create();


			buttongMa8051391->setShader(shader);

			//set textures
			buttongMa8051391->setTexture(buttonTexture8051391, 0);
			//render
			Renderer buttongRen8051391 = Renderer(buttongMa8051391, button12391);
			AttachCopy(button10, buttongRen8051391);
			//transform
			SMI_Transform buttonTrans180151391 = SMI_Transform();

			buttonTrans180151391.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(button10, buttonTrans180151391);

			SMI_Physics buttonphys51391 = SMI_Physics(glm::vec3(-214.0, 6.7, 2.1), glm::vec3(90, 0, -90), glm::vec3(0.75, 0.02, 0.226), button10, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			buttonphys51391.setIdentity(13);
			AttachCopy(button10, buttonphys51391);
		}
		
		VertexArrayObject::Sptr spike = ObjLoader::LoadFromFile("Models/spike.obj");
		{
			glide = CreateEntity();
			//create texture
			Texture2D::Sptr spiketex = Texture2D::Create("Textures/spike.png");
			//material
			SMI_Material::Sptr spikemat = SMI_Material::Create();


			spikemat->setShader(shader);

			//set textures
			spikemat->setTexture(spiketex, 0);
			//render
			Renderer spikeren21511 = Renderer(spikemat, spike);
			AttachCopy(glide, spikeren21511);
			//transform
			SMI_Transform spikeTrans1801121511 = SMI_Transform();

			spikeTrans1801121511.setPos(glm::vec3(-194.7, 7.0, 2.3));

			spikeTrans1801121511.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(glide, spikeTrans1801121511);

			SMI_Physics glidephys51391 = SMI_Physics(glm::vec3(-198.7, 7.0, 2.3), glm::vec3(90, 0, -90), glm::vec3(4.1, 3.62, 14.000), glide, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			glidephys51391.setIdentity(14);
			AttachCopy(glide, glidephys51391);

		}
	
		VertexArrayObject::Sptr winwall78 = ObjLoader::LoadFromFile("Models/winwalls.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr winwallTex2151178 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr winwall12151178 = SMI_Material::Create();


			winwall12151178->setShader(shader);

			//set textures
			winwall12151178->setTexture(winwallTex2151178, 0);
			//render
			Renderer winwallren2151178 = Renderer(winwall12151178, winwall78);
			AttachCopy(barrel, winwallren2151178);
			//transform
			SMI_Transform winwallTrans180112151178 = SMI_Transform();

			winwallTrans180112151178.setPos(glm::vec3(-209, 7.3, 2.5));

			winwallTrans180112151178.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winwallTrans180112151178);

		}

		VertexArrayObject::Sptr insidewall234 = ObjLoader::LoadFromFile("Models/inside.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr insideTexture805234 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr insidegMa805234 = SMI_Material::Create();


			insidegMa805234->setShader(shader);

			//set textures
			insidegMa805234->setTexture(insideTexture805234, 0);
			//render
			Renderer insidegRen805234 = Renderer(insidegMa805234, insidewall234);
			AttachCopy(barrel, insidegRen805234);
			//transform
			SMI_Transform insideTrans18015234 = SMI_Transform();

			insideTrans18015234.setPos(glm::vec3(-203.0, -2.7, 1.3));

			insideTrans18015234.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, insideTrans18015234);
		}
		VertexArrayObject::Sptr insidewall2342 = ObjLoader::LoadFromFile("Models/inside.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr insideTexture8052342 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr insidegMa8052342 = SMI_Material::Create();


			insidegMa8052342->setShader(shader);

			//set textures
			insidegMa8052342->setTexture(insideTexture8052342, 0);
			//render
			Renderer insidegRen8052342 = Renderer(insidegMa8052342, insidewall2342);
			AttachCopy(barrel, insidegRen8052342);
			//transform
			SMI_Transform insideTrans180152342 = SMI_Transform();

			insideTrans180152342.setPos(glm::vec3(-218.8, -2.7, 1.3));

			insideTrans180152342.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, insideTrans180152342);
		}

		VertexArrayObject::Sptr plank5hold1 = ObjLoader::LoadFromFile("Models/plankhold.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr plankTexture805hold1 = Texture2D::Create("Textures/spike.png");
			//material
			SMI_Material::Sptr plankgMa80hold51 = SMI_Material::Create();


			plankgMa80hold51->setShader(shader);

			//set textures
			plankgMa80hold51->setTexture(plankTexture805hold1, 0);
			//render
			Renderer plankgRen805hold1 = Renderer(plankgMa80hold51, plank5hold1);
			AttachCopy(barrel, plankgRen805hold1);
			//transform
			SMI_Transform plankTrans18015hold1 = SMI_Transform();

			plankTrans18015hold1.setPos(glm::vec3(-198.5, 7.5, 0.4));

			plankTrans18015hold1.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, plankTrans18015hold1);
		}

		VertexArrayObject::Sptr plank51 = ObjLoader::LoadFromFile("Models/splank.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr plankTexture8051 = Texture2D::Create("Textures/platform.png");
			//material
			SMI_Material::Sptr plankgMa8051 = SMI_Material::Create();


			plankgMa8051->setShader(shader);

			//set textures
			plankgMa8051->setTexture(plankTexture8051, 0);
			//render
			Renderer plankgRen8051 = Renderer(plankgMa8051, plank51);
			AttachCopy(barrel, plankgRen8051);
			//transform
			SMI_Transform plankTrans180151 = SMI_Transform();

			plankTrans180151.setPos(glm::vec3(-198.5, 7.5, 4.8));

			plankTrans180151.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, plankTrans180151);

			SMI_Physics plankphys51 = SMI_Physics(glm::vec3(-198.5, 7.5, 4.8), glm::vec3(90, 0, -90), glm::vec3(4.42609, 2.173, 4.8), barrel, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			plankphys51.setIdentity(2);
			AttachCopy(barrel, plankphys51);
		}

		VertexArrayObject::Sptr crate1191 = ObjLoader::LoadFromFile("Models/SCrate.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex199 = Texture2D::Create("Textures/box32.png");
			//material
			SMI_Material::Sptr BarrelMat5199 = SMI_Material::Create();
			BarrelMat5199->setShader(shader);

			//set textures
			BarrelMat5199->setTexture(crateTex199, 0);
			//render
			Renderer BarrelRend5199 = Renderer(BarrelMat5199, crate1191);
			AttachCopy(barrel, BarrelRend5199);
			//transform
			SMI_Transform BarrelTrans5199 = SMI_Transform();

			BarrelTrans5199.setPos(glm::vec3(-198.5, 7.5, 7.8));

			BarrelTrans5199.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans5199);

			SMI_Physics crate1phys5299 = SMI_Physics(glm::vec3(-198.5, 7.5, 7.8), glm::vec3(90, 0, -90), glm::vec3(2.15, 1.85, 1.67), barrel, SMI_PhysicsBodyType::DYNAMIC, 1.0f);
			crate1phys5299.setHasGravity(true);
			crate1phys5299.setIdentity(15);
			AttachCopy(barrel, crate1phys5299);
		}

		VertexArrayObject::Sptr door2123 = ObjLoader::LoadFromFile("Models/Gdoor.obj");
		{
			door8 = CreateEntity();

			//create texture
			Texture2D::Sptr door215Texture8052 = Texture2D::Create("Textures/doortex.png");
			//material
			SMI_Material::Sptr door215gMa8052 = SMI_Material::Create();


			door215gMa8052->setShader(shader);

			//set textures
			door215gMa8052->setTexture(door215Texture8052, 0);
			//render
			Renderer door215gRen8052 = Renderer(door215gMa8052, door2123);
			AttachCopy(door8, door215gRen8052);
			//transform
			SMI_Transform door215Trans180152 = SMI_Transform();

			door215Trans180152.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(door8, door215Trans180152);

			SMI_Physics door215door2phys5 = SMI_Physics(glm::vec3(-209.0, 7.0, 2.5), glm::vec3(90, 0, -90), glm::vec3(4.02, 16.298, 0.13), door8, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			door215door2phys5.setIdentity(2);
			AttachCopy(door8, door215door2phys5);
		}

		VertexArrayObject::Sptr ashphalt51 = ObjLoader::LoadFromFile("Models/wood.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr ashphaltTexture80511 = Texture2D::Create("Textures/lounge.png");
			//material
			SMI_Material::Sptr ashphaltgMa80511 = SMI_Material::Create();


			ashphaltgMa80511->setShader(shader);

			//set textures
			ashphaltgMa80511->setTexture(ashphaltTexture80511, 0);
			//render
			Renderer ashphaltgRen80511 = Renderer(ashphaltgMa80511, ashphalt5);
			AttachCopy(barrel, ashphaltgRen80511);
			//transform
			SMI_Transform ashphaltTrans1801511 = SMI_Transform();

			ashphaltTrans1801511.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, ashphaltTrans1801511);

			SMI_Physics gravelphys511 = SMI_Physics(glm::vec3(-227.5, 6, -0.8), glm::vec3(90, 0, -90), glm::vec3(20.8154, 4.62269, 77.7581), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
			gravelphys511.setIdentity(2);
			AttachCopy(barrel, gravelphys511);
		}

		VertexArrayObject::Sptr Laser1 = ObjLoader::LoadFromFile("Models/lasercircle.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr  Laser1Texture80511 = Texture2D::Create("Textures/laserred.png");
			//material
			SMI_Material::Sptr  Laser1gMa80511 = SMI_Material::Create();


			Laser1gMa80511->setShader(shader);

			//set textures
			Laser1gMa80511->setTexture(Laser1Texture80511, 0);
			//render
			Renderer  Laser1gRen80511 = Renderer(Laser1gMa80511, Laser1);
			AttachCopy(barrel, Laser1gRen80511);
			//transform
			SMI_Transform  Laser1Trans1801511 = SMI_Transform();

			Laser1Trans1801511.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, Laser1Trans1801511);

			SMI_Physics Laser1phys511 = SMI_Physics(glm::vec3(-228.5, 6, -1.3), glm::vec3(90, 0, -90), glm::vec3(0.56,80.0106,0.56), barrel, SMI_PhysicsBodyType::KINEMATIC, 0.0f);
			Laser1phys511.setIdentity(16);
			AttachCopy(barrel, Laser1phys511);
		}

		VertexArrayObject::Sptr plank5t = ObjLoader::LoadFromFile("Models/Cfan12.obj");
		{
			fan3 = CreateEntity();

			//create texture
			Texture2D::Sptr plankTexture805t = Texture2D::Create("Textures/fan.png");
			//material
			SMI_Material::Sptr plankgMa805t = SMI_Material::Create();


			plankgMa805t->setShader(shader);

			//set textures
			plankgMa805t->setTexture(plankTexture805t, 0);
			//render
			Renderer plankgRen805t = Renderer(plankgMa805t, plank5t);
			AttachCopy(fan3, plankgRen805t);
			//transform
			SMI_Transform plankTrans18015t = SMI_Transform();

			plankTrans18015t.setPos(glm::vec3(-223.5, 6.5, 5.8));

			plankTrans18015t.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(fan3, plankTrans18015t);

			SMI_Physics plankphys5t = SMI_Physics(glm::vec3(-223.5, 7.2, 8.8), glm::vec3(90, 0, -90), glm::vec3(14.5157, 103.9552, 0.687242), fan3, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			plankphys5t.setIdentity(2);
			AttachCopy(fan3, plankphys5t);
		}

		VertexArrayObject::Sptr winwall783 = ObjLoader::LoadFromFile("Models/winwalls.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr winwallTex21511783 = Texture2D::Create("Textures/inside.png");
			//material
			SMI_Material::Sptr winwall121511783 = SMI_Material::Create();


			winwall121511783->setShader(shader);

			//set textures
			winwall121511783->setTexture(winwallTex21511783, 0);
			//render
			Renderer winwallren21511783 = Renderer(winwall121511783, winwall783);
			AttachCopy(barrel, winwallren21511783);
			//transform
			SMI_Transform winwallTrans1801121511783 = SMI_Transform();

			winwallTrans1801121511783.setPos(glm::vec3(-230, 7.3, 2.5));

			winwallTrans1801121511783.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winwallTrans1801121511783);

		}

		VertexArrayObject::Sptr edoor = ObjLoader::LoadFromFile("Models/Door2.obj");
		{
			door7 = CreateEntity();

			//create texture
			Texture2D::Sptr edoor215Texture8052 = Texture2D::Create("Textures/doortex.png");
			//material
			SMI_Material::Sptr edoor215gMa8052 = SMI_Material::Create();


			edoor215gMa8052->setShader(shader);

			//set textures
			edoor215gMa8052->setTexture(edoor215Texture8052, 0);
			//render
			Renderer edoor215gRen8052 = Renderer(edoor215gMa8052, edoor);
			AttachCopy(door7, edoor215gRen8052);
			//transform
			SMI_Transform edoor215Trans180152 = SMI_Transform();

			edoor215Trans180152.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(door7, edoor215Trans180152);

			SMI_Physics edoor215door2phys5 = SMI_Physics(glm::vec3(-185.7, 6.7, 2.5), glm::vec3(90, 0, -90), glm::vec3(4.02, 16.298, 0.13), door7, SMI_PhysicsBodyType::KINEMATIC, 1.0f);
			edoor215door2phys5.setIdentity(2);
			AttachCopy(door7, edoor215door2phys5);
		}

		VertexArrayObject::Sptr clear = ObjLoader::LoadFromFile("Models/wi11.obj");
		{
			ed1 = CreateEntity();

			//create texture
			Texture2D::Sptr clearTexture805133 = Texture2D::Create("Textures/levcleared.png");
			//material
			SMI_Material::Sptr cleargMa805133 = SMI_Material::Create();


			cleargMa805133->setShader(shader);

			//set textures
			cleargMa805133->setTexture(clearTexture805133, 0);
			//render
			Renderer cleargRen805133 = Renderer(cleargMa805133, clear);
			AttachCopy(ed1, cleargRen805133);
			//transform
			SMI_Transform clearTrans18015133 = SMI_Transform();

			clearTrans18015133.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(ed1, clearTrans18015133);

			SMI_Physics clearphys5133 = SMI_Physics(glm::vec3(432.0, -4.2, -7.0), glm::vec3(90, 0, -90), glm::vec3(0.23, 2.819, 0.23), ed1, SMI_PhysicsBodyType::STATIC, 1.0f);
			clearphys5133.setIdentity(9);
			AttachCopy(ed1, clearphys5133);
		}
	}
	
	void Update(float deltaTime)
	{

		//increment times
		current += deltaTime;
		c += deltaTime;
		if (current > max)
		{
			current = max;
		}
		if (c > max)
		{
			c = 0;

		}
		float t = current / max;
		float time = c / max;
		
		//reference to player physics body
		SMI_Physics& PlayerPhys = GetComponent<SMI_Physics>(character);
		glm::vec3 NewCamPos = glm::vec3(PlayerPhys.GetPosition().x, camera->GetPosition().y, camera->GetPosition().z);
		camera->SetPosition(NewCamPos);


		//keyboard input
		//move left
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		{
			PlayerPhys.AddForce(glm::vec3(5.0, 0, 0));
		}
		//move right
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		{
			PlayerPhys.AddForce(glm::vec3(-5, 0, 0));
		}
		

		//jump
		if ((glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) && JumpState == GLFW_RELEASE && (grounded || CurrentMidAirJump < MidAirJump))
		{
			PlayerPhys.AddImpulse(glm::vec3(0, 0, 6));

			//allows for one mid air jump
			if (!grounded)
			{
				CurrentMidAirJump++;
			}
		}
		JumpState = glfwGetKey(window, GLFW_KEY_SPACE);



		// LERP example


		GetComponent<SMI_Transform>(fan).FixedRotate(glm::vec3(30, 0, 0) * deltaTime * 50.0f);

		GetComponent<SMI_Transform>(fan2).FixedRotate(glm::vec3(30, 0, 0) * deltaTime * 50.0f);

		GetComponent<SMI_Transform>(fan3).FixedRotate(glm::vec3(30, 0, 0) * deltaTime * 50.0f);

		


		SMI_Physics& elevator1Phys = GetComponent<SMI_Physics>(elevator);
		elevator1Phys.SetPosition(Lerp(glm::vec3(-75.0, 7.0, 1.8), glm::vec3(-75.0, 7.0, 8.8), time));

		SMI_Physics& dphys201131 = GetComponent<SMI_Physics>(door1);
		dphys201131.SetPosition(Lerp(glm::vec3(-46.0, 9.5, 15), glm::vec3(-46.0, 9.5, 2), time));

		SMI_Physics bulletphys513 = GetComponent<SMI_Physics>(bullet);
		bulletphys513.SetPosition(Lerp(glm::vec3(-179.0, 7.2, 3.9), glm::vec3(-168.0, 7.2, 3.9), time));

		SMI_Physics glidephys51391 = GetComponent<SMI_Physics>(glide);
		glidephys51391.SetPosition(Lerp(glm::vec3(-198.7, 7.0, 2.3), glm::vec3(-198.7, 7.0, -6.3), time));
		
		

		
		
		


		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 3))))
				{

					
					SMI_Physics& door2phys5 = GetComponent<SMI_Physics>(door2);
					door2phys5.SetPosition(Lerp(glm::vec3(-151.0, 7.0, 2.5), glm::vec3(151.0, 7.0, 6.5), t));

					SMI_Physics& buttonphys5 = GetComponent<SMI_Physics>(button);
					buttonphys5.SetPosition(Lerp(glm::vec3(-158.0, 6.7, 2.1), glm::vec3(-158.0, -34.7, 2.1), t));

					SMI_Physics& buttonphys51 = GetComponent<SMI_Physics>(button1);
					buttonphys51.SetPosition(Lerp(glm::vec3(-158.0, -34.7, 2.1), glm::vec3(-158.0, 6.7, 2.1), t));
				}
			}
		}
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 4 && Phys2.getIdentity() == 5|| Phys1.getIdentity() == 1 && Phys2.getIdentity() == 5))))
				{
					
					SMI_Physics& door2phys52 = GetComponent<SMI_Physics>(door3);
					door2phys52.SetPosition(Lerp(glm::vec3(-166.7, 7.0, 2.5), glm::vec3(-166.7, -34.0, 2.5), t));
				}
				
				else if((cont && ((Phys1.getIdentity() == 2 && Phys2.getIdentity() == 4))))
				{

						SMI_Physics& door2phys52 = GetComponent<SMI_Physics>(door3);
						door2phys52.SetPosition(Lerp(glm::vec3(-166.7, 7.0, 2.5), glm::vec3(-166.7, 7.0, 2.5), t));
				}

				
			}
		}
		for (int i = 0; i < Collisions.size(); i++)
		{
			
			
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 6))))
				{

					
					
					GetComponent<SMI_Transform>(door4).setPos(Lerp(glm::vec3(-12.5, 9.2, 2.0), glm::vec3(-12.5, -9.2, 2.0), t));
					
					SMI_Physics& bardoorphys = GetComponent<SMI_Physics>(door4);
					bardoorphys.SetPosition(Lerp(glm::vec3(-12.5, 9.2, 2.0), glm::vec3(-12.5, -9.2, 2.0), t));

					SMI_Physics& buttonphys59 = GetComponent<SMI_Physics>(button6);
					buttonphys59.SetPosition(Lerp(glm::vec3(-12.5, 7.7, 15.1), glm::vec3(-12.5, -87.7, 15.1), t));

					SMI_Physics& buttonphys159 = GetComponent<SMI_Physics>(button7);
					buttonphys159.SetPosition(Lerp(glm::vec3(-12.5, -87.7, 15.1), glm::vec3(-12.5, 7.7, 15.1), t));
					

				}
			}
		}
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 7 || Phys1.getIdentity() == 7 && Phys2.getIdentity() == 1))))
				{	
					
					SMI_Physics& bulletphys5133 = GetComponent<SMI_Physics>(ed);
					glm::vec3 NewCamPos = glm::vec3(bulletphys5133.GetPosition().x, camera->GetPosition().y, camera->GetPosition().z);
					camera->SetPosition(NewCamPos);

					deltaTime = 0.0;

					if (glfwGetKey(window, GLFW_KEY_E))
					{
						exit(1);
					}
					
				}
			}
		}
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 8 || Phys1.getIdentity() == 8 && Phys2.getIdentity() == 1))))
				{
					SMI_Physics& bulletphys5133 = GetComponent<SMI_Physics>(ed);
					glm::vec3 NewCamPos = glm::vec3(bulletphys5133.GetPosition().x, camera->GetPosition().y, camera->GetPosition().z);
					camera->SetPosition(NewCamPos);

					deltaTime = 0.0;

					if (glfwGetKey(window, GLFW_KEY_E))
					{
						exit(1);
					}

				}
			}
		}
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 9 || Phys1.getIdentity() == 9 && Phys2.getIdentity() == 1))))
				{

					SMI_Physics& bulletphys5133 = GetComponent<SMI_Physics>(ed);
					glm::vec3 NewCamPos = glm::vec3(bulletphys5133.GetPosition().x, camera->GetPosition().y, camera->GetPosition().z);
					camera->SetPosition(NewCamPos);

					deltaTime = 0.0;

					if (glfwGetKey(window, GLFW_KEY_E))
					{
						exit(1);
					}

				}
			}
		}
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 10 && Phys2.getIdentity() == 11 || Phys1.getIdentity() == 11 && Phys2.getIdentity() == 10))))
				{

					SMI_Physics bulletphys513 = GetComponent<SMI_Physics>(bullet);
					bulletphys513.SetPosition(glm::vec3(-168.0, 7.2, 68.9));


					SMI_Physics& enemyPhys = GetComponent<SMI_Physics>(en);
					enemyPhys.SetPosition(glm::vec3(-181.0, 400.8, 2.2));

					SMI_Physics enemyPhys1 = GetComponent<SMI_Physics>(en1);
					enemyPhys1.SetPosition(glm::vec3(-181.0, 7.2, 2.3));


					SMI_Physics& door215door2phys5 = GetComponent<SMI_Physics>(door7);
					door215door2phys5.SetPosition(glm::vec3(-185.7, -47.0, 2.5));

				}
			
			}
		}
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 12 || Phys1.getIdentity() == 12 && Phys2.getIdentity() == 1))))
				{


					SMI_Physics& plankphys5 = GetComponent<SMI_Physics>(planks);
					plankphys5.SetPosition(glm::vec3(-182.5, 6.5, -434.8));


				}
			
			}
		}

		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 13 || Phys1.getIdentity() == 13 && Phys2.getIdentity() == 1))))
				{

					
					SMI_Physics& door215door2phys5 = GetComponent<SMI_Physics>(door8);
					door215door2phys5.SetPosition(glm::vec3(-209.0, 327.0, 2.5));

				}

			}
		}
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 14 || Phys1.getIdentity() == 14 && Phys2.getIdentity() == 1))))
				{

					SMI_Physics& bulletphys5133 = GetComponent<SMI_Physics>(ed);
					glm::vec3 NewCamPos = glm::vec3(bulletphys5133.GetPosition().x, camera->GetPosition().y, camera->GetPosition().z);
					camera->SetPosition(NewCamPos);

					deltaTime = 0.0;

					if (glfwGetKey(window, GLFW_KEY_E))
					{
						exit(1);
					}

				}

			}
		}

		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 15 && Phys2.getIdentity() == 13 || Phys1.getIdentity() == 13 && Phys2.getIdentity() == 15))))
				{


					SMI_Physics& plankphys5t = GetComponent<SMI_Physics>(fan3);
					plankphys5t.SetPosition(glm::vec3(-223.5, 7.2, 434.8));

					
				}

			}
		}
		
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);
				if ((cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 16 || Phys1.getIdentity() == 16 && Phys2.getIdentity() == 1))))
				{


					SMI_Physics& clearphys5133 = GetComponent<SMI_Physics>(ed1);
					glm::vec3 NewCamPos = glm::vec3(clearphys5133.GetPosition().x, camera->GetPosition().y, camera->GetPosition().z);
					camera->SetPosition(NewCamPos);

					deltaTime = 0.0;

					if (glfwGetKey(window, GLFW_KEY_E))
					{
						exit(1);
					}


				}

			}
		}
		

		
		SMI_Scene::Update(deltaTime);

		grounded = false;

		//checks every collision type
		Collider();


	}

	//manages all collisions in the game
	void Collider()
	{
		for (int i = 0; i < Collisions.size(); i++)
		{
			entt::entity Ent1 = Collisions[i]->getB1();
			entt::entity Ent2 = Collisions[i]->getB2();

			if (GetRegistry().valid(Ent1) && GetRegistry().valid(Ent2))
			{
				bool cont = true;

				SMI_Physics Phys1 = GetComponent<SMI_Physics>(Ent1);
				SMI_Physics Phys2 = GetComponent<SMI_Physics>(Ent2);

				if (cont && ((Phys1.getIdentity() == 1 && Phys2.getIdentity() == 2) || (Phys1.getIdentity() == 2 && Phys2.getIdentity() == 1)))
				{
					grounded = true;
					CurrentMidAirJump = 0;
				}

			}
		}
	}

	~GameScene1() = default;

private:
	entt::entity barrel;
	entt::entity door1;
	entt::entity door2;
	entt::entity door3;
	entt::entity door4;
	entt::entity door7;
	entt::entity door8;
	entt::entity button;
	entt::entity button1;
	entt::entity button5;
	entt::entity button6;
	entt::entity button7;
	entt::entity button8;
	entt::entity button9;
	entt::entity button10;
	entt::entity button11;
	entt::entity character;
	entt::entity fan;
	entt::entity fan2;
	entt::entity fan3;
	entt::entity elevator;
	entt::entity bullet;
	entt::entity ed;
	entt::entity ed1;
	entt::entity en;
	entt::entity en1;
	entt::entity planks;
	entt::entity glide;
	float max = 5;
	float current = 0;
	float c = 0;

	//variables for jump checks
	int JumpState = GLFW_RELEASE;
	int MidAirJump = 1;
	int CurrentMidAirJump = 0;
	bool grounded = false;
};


class GameScene2 : public SMI_Scene
{
public:
	//just ignore the warning, guys. It's fine.
	GameScene2() : SMI_Scene() {}

	void InitScene()
	{
		SMI_Scene::InitScene();


		// Load our shaders
		Shader::Sptr shader1 = Shader::Create();
		shader1->LoadShaderPartFromFile("shaders/vertex_shader.glsl", ShaderPartType::Vertex);
		shader1->LoadShaderPartFromFile("shaders/frag_shader.glsl", ShaderPartType::Fragment);
		shader1->Link();

		// Get uniform location for the model view projection
		Camera::Sptr camera1 = Camera::Create();

		//camera position
		camera1->SetPosition(glm::vec3(-0.2, 10.5, 9.9));
		//this defines the point the camera is looking at
		camera1->LookAt(glm::vec3(0, 0.4, -0.2));

		camera1->SetOrthoEnabled(true);
		camera1->SetOrthoVerticalScale(20);
		setCamera(camera1);

		VertexArrayObject::Sptr menuback = ObjLoader::LoadFromFile("Models/menu2.obj");
		{
			menu = CreateEntity();

			//create texture
			Texture2D::Sptr owindow1Texturet = Texture2D::Create("Textures/Main_Menu.png");
			//material
			SMI_Material::Sptr oBarrelMatt = SMI_Material::Create();
			oBarrelMatt->setShader(shader1);

			//set textures
			oBarrelMatt->setTexture(owindow1Texturet, 0);
			//render
			Renderer oBarrelRendt = Renderer(oBarrelMatt, menuback);
			AttachCopy(menu, oBarrelRendt);
			//transform
			SMI_Transform oBarrelTranst = SMI_Transform();

			oBarrelTranst.setPos(glm::vec3(4.2, 2, 0));

			oBarrelTranst.SetDegree(glm::vec3(130, -8, -189));
			AttachCopy(menu, oBarrelTranst);
		}






	}


	~GameScene2() = default;
private:

	entt::entity rt;
	entt::entity rrt;
	entt::entity menu;


};
class GameScene3 : public SMI_Scene
{
public:
	//just ignore the warning, guys. It's fine.
	GameScene3() : SMI_Scene() {}

	void InitScene()
	{
		SMI_Scene::InitScene();


		// Load our shaders
		Shader::Sptr shader1 = Shader::Create();
		shader1->LoadShaderPartFromFile("shaders/vertex_shader.glsl", ShaderPartType::Vertex);
		shader1->LoadShaderPartFromFile("shaders/frag_shader.glsl", ShaderPartType::Fragment);
		shader1->Link();


		// Get uniform location for the model view projection
		Camera::Sptr camera2 = Camera::Create();

		//camera position
		camera2->SetPosition(glm::vec3(-0.2, 10.5, 9.9));
		//this defines the point the camera is looking at
		camera2->LookAt(glm::vec3(0, 0.4, -0.2));

		camera2->SetOrthoEnabled(true);
		camera2->SetOrthoVerticalScale(20);
		setCamera(camera2);

		VertexArrayObject::Sptr yt1 = ObjLoader::LoadFromFile("Models/menu2.obj");
		{
			rt1 = CreateEntity();

			//create texture
			Texture2D::Sptr window1Texturet1 = Texture2D::Create("Textures/Pause_Screen.png");
			//material
			SMI_Material::Sptr BarrelMatt1 = SMI_Material::Create();
			BarrelMatt1->setShader(shader1);

			//set textures
			BarrelMatt1->setTexture(window1Texturet1, 0);
			//render
			Renderer BarrelRendt1 = Renderer(BarrelMatt1, yt1);
			AttachCopy(rt1, BarrelRendt1);
			//transform
			SMI_Transform BarrelTranst1 = SMI_Transform();

			BarrelTranst1.setPos(glm::vec3(4.2, 2, 0));

			BarrelTranst1.SetDegree(glm::vec3(130, -8, -189));
			AttachCopy(rt1, BarrelTranst1);
		}



	}


	~GameScene3() = default;
private:

	entt::entity rt1;


};



//main game loop inside here as well as call all needed shaders
int main()
{
	Logger::Init(); // We'll borrow the logger from the toolkit, but we need to initialize it

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(GlDebugMessage, nullptr);



	// Our high-precision timer
	double lastFrame = glfwGetTime();

	GameScene2 Ma = GameScene2();
	Ma.InitScene();
	GameScene1 MainScene = GameScene1();
	MainScene.InitScene();
	GameScene3 Pausescreen = GameScene3();
	Pausescreen.InitScene();

	bool isButtonPressed = false;
	bool it = false;
	bool notmenu = true;
	bool notpause = true;

	//Sound audio;
	//audio.init();
	//audio.loadsounds("music", "music.wav", true);
	Sound audio1;
	audio1.init();
	
	Sound audio2;
	Sound audio3;
	

	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {

		glfwPollEvents();

		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);


		// Clear the color and depth buffers
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		

		if (glfwGetKey(window, GLFW_KEY_P))
		{
			if (!isButtonPressed) {

				notmenu = !notmenu;

				//audio.shutdown();
			}
			isButtonPressed = true;
		}
		else { //when its pressed while ortho view it will switch back to perspective

			isButtonPressed = false;

		}
		if (glfwGetKey(window, GLFW_KEY_B))
		{
			if (!it) {
				notpause = !notpause;
			}
			it = true;
		}
		else {
			it = false;
		}

		if (!notmenu && notpause)
		{
			MainScene.Render();
			MainScene.Update(dt);
		}
		if (notmenu)
		{
			Ma.Render();
		}
		if (!notmenu && !notpause)
		{
			Pausescreen.Render();
			if (glfwGetKey(window, GLFW_KEY_E))
			{
				exit(1);
			}

			if (glfwGetKey(window, GLFW_KEY_R))
			{
				
			}
		}

		int one = glfwGetKey(window, GLFW_KEY_SPACE);

			if(one==GLFW_PRESS)
			{

			
			audio1.loadsounds("jumping", "jump.wav", true);
			audio1.update();
			
			
			
			}
		

		if (glfwGetKey(window, GLFW_KEY_D))
		{

			//audio2.init();
			//audio2.loadsounds("walk", "walk.wav", true);
			//audio2.update();
			
		}
		
		/*

		if (glfwGetKey(window, GLFW_KEY_F))
		{

			audio3.init();
			audio3.loadsounds("fan", "FAN.wav", true);
			audio3.update();
		}
		*/
	
		lastFrame = thisFrame;
		glfwSwapBuffers(window);
	}

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}