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
		camera->SetPosition(glm::vec3(-0.2, 50.5, 12.9));
		//this defines the point the camera is looking at
		camera->LookAt(glm::vec3(0, -10.5, 2.8));
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
			CharaTrans.setPos(glm::vec3(4, 7.0, 2));

			CharaTrans.SetDegree(glm::vec3(90, 0, -90));
			CharaTrans.setScale(glm::vec3(0.25, 0.25, 0.25));
			AttachCopy(character, CharaTrans);

			SMI_Physics CharaPhys = SMI_Physics(glm::vec3(4, 7.0, 2), glm::vec3(90, 0, -90), glm::vec3(1, 1, 1), character, SMI_PhysicsBodyType::DYNAMIC, 1.0f);
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

			BarrelTrans.SetDegree(glm::vec3(90, -10, 90));
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

			win22Trans.setPos(glm::vec3(16, -3.3, 1));

			win22Trans.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(barrel, win22Trans);
		}
		//creates object
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

			window2Trans.SetDegree(glm::vec3(90, -10, 90));
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

			BarrelTrans1.setPos(glm::vec3(-0.6, 7.2, 2.7));

			BarrelTrans1.SetDegree(glm::vec3(0, 90, 0));
			AttachCopy(barrel, BarrelTrans1);

			SMI_Physics BarrelPhys = SMI_Physics(glm::vec3(-0.6, 7.2, 2.7), glm::vec3(90, 0, -90), glm::vec3(2.5, 2.5, 2.5), barrel, SMI_PhysicsBodyType::DYNAMIC, 1.0f);
			BarrelPhys.setHasGravity(true);
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

			SMI_Physics BarrelPhys = SMI_Physics(glm::vec3(-9.0, 6.0, 3.7), glm::vec3(90, 0, -90), glm::vec3(3, 3, 3), barrel, SMI_PhysicsBodyType::STATIC, 1.0f);
			BarrelPhys.setHasGravity(true);
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

		VertexArrayObject::Sptr crate = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex = Texture2D::Create("Textures/box3.png");
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

			BarrelTrans5.setPos(glm::vec3(-25.0, 7.0, 4.0));

			BarrelTrans5.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans5);
		}
		VertexArrayObject::Sptr crate3 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex3 = Texture2D::Create("Textures/box3.png");
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

			BarrelTrans53.setPos(glm::vec3(-25.0, 7.0, 5.4));

			BarrelTrans53.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans53);
		}
		VertexArrayObject::Sptr crate4 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex4 = Texture2D::Create("Textures/box3.png");
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

			BarrelTrans534.setPos(glm::vec3(-18.0, 7.0, 2.5));

			BarrelTrans534.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans534);
		}
		VertexArrayObject::Sptr crate5 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crateTex5 = Texture2D::Create("Textures/box3.png");
			//material
			SMI_Material::Sptr BarrelMat535 = SMI_Material::Create();
			BarrelMat535->setShader(shader);

			//set textures
			BarrelMat535->setTexture(crateTex5, 0);
			//render
			Renderer BarrelRend535 = Renderer(BarrelMat535, crate5);
			AttachCopy(barrel, BarrelRend535);
			//transform
			SMI_Transform BarrelTrans5345 = SMI_Transform();

			BarrelTrans5345.setPos(glm::vec3(-115.0, 7.0, 10.5));

			BarrelTrans5345.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans5345);
		}
		VertexArrayObject::Sptr topground = ObjLoader::LoadFromFile("Models/ground.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr groundTex = Texture2D::Create("Textures/brick1.png");
			//material
			SMI_Material::Sptr groundmat1 = SMI_Material::Create();
			groundmat1->setShader(shader);

			//set textures
			groundmat1->setTexture(groundTex, 0);
			//render
			Renderer groundRend1 = Renderer(groundmat1, topground);
			AttachCopy(barrel, groundRend1);
			//transform
			SMI_Transform groundTrans1 = SMI_Transform();

			groundTrans1.setPos(glm::vec3(-30.8, 8, 9.8));

			groundTrans1.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, groundTrans1);
		}
		VertexArrayObject::Sptr door = ObjLoader::LoadFromFile("Models/Door2.obj");
		{

			door1 = CreateEntity();

			Texture2D::Sptr doorTexture = Texture2D::Create("Textures/DoorOpen.png");
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

			BarrelTrans6.setPos(glm::vec3(-35.2, 0, 2));

			BarrelTrans6.SetDegree(glm::vec3(90, 0, -180));
			AttachCopy(door1, BarrelTrans6);
		}

		VertexArrayObject::Sptr crate1 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crate1Texture = Texture2D::Create("Textures/box3.png");
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

			BarrelTrans5.setPos(glm::vec3(-25.0, 7.0, 2.2));

			BarrelTrans5.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans5);
		}

		VertexArrayObject::Sptr bar = ObjLoader::LoadFromFile("Models/btab.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr WinTexture80 = Texture2D::Create("Textures/brown1.png");
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
		VertexArrayObject::Sptr garbage1 = ObjLoader::LoadFromFile("Models/wall.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr gTexture80 = Texture2D::Create("Textures/brick1.png");
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

			winTrans1801.setPos(glm::vec3(-10.5, 4.8, 3.0));

			winTrans1801.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, winTrans1801);
		}
		VertexArrayObject::Sptr garbage2 = ObjLoader::LoadFromFile("Models/doorwall.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr gTexture801 = Texture2D::Create("Textures/brick1.png");
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
		VertexArrayObject::Sptr topground1 = ObjLoader::LoadFromFile("Models/ground.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr groundTex1 = Texture2D::Create("Textures/brick1.png");
			//material
			SMI_Material::Sptr groundmat11 = SMI_Material::Create();
			groundmat11->setShader(shader);

			//set textures
			groundmat11->setTexture(groundTex1, 0);
			//render
			Renderer groundRend11 = Renderer(groundmat11, topground1);
			AttachCopy(barrel, groundRend11);
			//transform
			SMI_Transform groundTrans11 = SMI_Transform();

			groundTrans11.setPos(glm::vec3(-38.8, 8, 9.8));

			groundTrans11.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, groundTrans11);
		}
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
		VertexArrayObject::Sptr crate12 = ObjLoader::LoadFromFile("Models/2crates.obj");
		{

			barrel = CreateEntity();

			Texture2D::Sptr crate1Texture12 = Texture2D::Create("Textures/box3.png");
			//material
			SMI_Material::Sptr BarrelMat512 = SMI_Material::Create();
			BarrelMat512->setShader(shader);

			//set textures
			BarrelMat512->setTexture(crate1Texture12, 0);
			//render
			Renderer BarrelRend512 = Renderer(BarrelMat512, crate12);
			AttachCopy(barrel, BarrelRend512);
			//transform
			SMI_Transform BarrelTrans512 = SMI_Transform();

			BarrelTrans512.setPos(glm::vec3(-48.0, 7.0, 2.2));

			BarrelTrans512.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel, BarrelTrans512);
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

			window2Trans31.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(barrel, window2Trans31);
		}
		VertexArrayObject::Sptr doorw2 = ObjLoader::LoadFromFile("Models/doorwall.obj");
		{
			barrel = CreateEntity();
			//create texture
			Texture2D::Sptr doorw2801Tex = Texture2D::Create("Textures/brick1.png");
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

			doorw2Trans18011.setPos(glm::vec3(-93.0, 8.8, 6.8));

			doorw2Trans18011.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, doorw2Trans18011);
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

			window2Trans311.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(barrel, window2Trans311);
		}
		VertexArrayObject::Sptr fence12 = ObjLoader::LoadFromFile("Models/wall.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr fencegTexture80 = Texture2D::Create("Textures/brick1.png");
			//material
			SMI_Material::Sptr fencegMa80 = SMI_Material::Create();


			fencegMa80->setShader(shader);

			//set textures
			fencegMa80->setTexture(fencegTexture80, 0);
			//render
			Renderer fencegRen80 = Renderer(fencegMa80, fence12);
			AttachCopy(barrel, fencegRen80);
			//transform
			SMI_Transform fencewinTrans1801 = SMI_Transform();

			fencewinTrans1801.setPos(glm::vec3(-124.5, 4.8, 3.0));

			fencewinTrans1801.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, fencewinTrans1801);
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

			carTrans1801.setPos(glm::vec3(-105.5, -4.8, 2.8));

			carTrans1801.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, carTrans1801);
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

			ashphaltTrans1801.setPos(glm::vec3(-112.5, 0, -0.8));

			ashphaltTrans1801.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, ashphaltTrans1801);
		}
		VertexArrayObject::Sptr plank = ObjLoader::LoadFromFile("Models/plank.obj");
		{
			barrel = CreateEntity();

			//create texture
			Texture2D::Sptr plankTexture80 = Texture2D::Create("Textures/platform.png");
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

			plankTrans1801.setPos(glm::vec3(-112.5, 0, 8.8));

			plankTrans1801.SetDegree(glm::vec3(90, 0, -90));
			AttachCopy(barrel, plankTrans1801);

			SMI_Physics plankphys = SMI_Physics(glm::vec3(-112.5, 0, 8.8), glm::vec3(90, 0, 90), glm::vec3(15.3, 3.32, 11.8), barrel, SMI_PhysicsBodyType::STATIC, 0.0f);
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

			tbuild2Trans180112.SetDegree(glm::vec3(90, 10, -90));
			AttachCopy(barrel, tbuild2Trans180112);
		}
	}

	void Update(float deltaTime)
	{
		//increment time
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
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		{
			PlayerPhys.AddForce(glm::vec3(0, 5, 0));
		}
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		{
			PlayerPhys.AddForce(glm::vec3(0, -5, 0));
		}

		//jump
		if ((glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) && JumpState == GLFW_RELEASE && (grounded || CurrentMidAirJump < MidAirJump))
		{
			PlayerPhys.AddImpulse(glm::vec3(0, 0, 8));

			//allows for one mid air jump
			if (!grounded)
			{
				CurrentMidAirJump++;
			}
		}
		JumpState = glfwGetKey(window, GLFW_KEY_SPACE);



		// LERP example

		GetComponent<SMI_Transform>(door1).setPos(Lerp(glm::vec3(-35.2, 0, 2), glm::vec3(-35.2, 0, 15), t));



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
	entt::entity character;
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
				glUseProgram;
			}
		}

		lastFrame = thisFrame;
		glfwSwapBuffers(window);
	}

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}