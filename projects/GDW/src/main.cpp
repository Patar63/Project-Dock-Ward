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

class GameScene : public SMI_Scene
{
public:
	GameScene() { SMI_Scene(); }

	void InitScene()
	{
		SMI_Scene::InitScene();

		// Load our shaders
		Shader::Sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", ShaderPartType::Vertex);
		shader->LoadShaderPartFromFile("shaders/frag_shader.glsl", ShaderPartType::Fragment);
		shader->Link();

		// GL states, we'll enable depth testing and backface fulling
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glClearColor(0.2f, 0.2f, 0.5f, 1.0f);

		// Get uniform location for the model view projection
		Camera::Sptr camera = Camera::Create();

		//camera position
		camera->SetPosition(glm::vec3(-86, 20.5, 9.9));
		//this defines the point the camera is looking at
		camera->LookAt(glm::vec3(-86.0f));

		//camera->SetOrthoVerticalScale(5);
		setCamera(camera);

		//creates object
		VertexArrayObject::Sptr vao4 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			barrel = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMat = SMI_Material::Create();
			BarrelMat->setShader(shader);
			//render
			Renderer BarrelRend = Renderer(BarrelMat, vao4);
			AttachCopy(barrel, BarrelRend);
			//transform
			SMI_Transform BarrelTrans = SMI_Transform();

			BarrelTrans.setPos(glm::vec3(-0.2, 0, 2));
			
			BarrelTrans.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(barrel, BarrelTrans);
		}
		VertexArrayObject::Sptr win = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			wa1 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMa = SMI_Material::Create();
			BarrelMa->setShader(shader);
			//render
			Renderer BarrelRen = Renderer(BarrelMa, win);
			AttachCopy(wa1, BarrelRen);
			//transform
			SMI_Transform winTrans1 = SMI_Transform();

			winTrans1.setPos(glm::vec3(-10.2, 0, 2));

			winTrans1.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(wa1, winTrans1);
		}
		VertexArrayObject::Sptr win1 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			wa2 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelM = SMI_Material::Create();
			BarrelM->setShader(shader);
			//render
			Renderer BarrelRe = Renderer(BarrelM, win1);
			AttachCopy(wa2, BarrelRe);
			//transform
			SMI_Transform winTrans = SMI_Transform();

			winTrans.setPos(glm::vec3(-20.2, 0, 2));

			winTrans.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(wa2, winTrans);
		}
		VertexArrayObject::Sptr win2 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			wa3 = CreateEntity();
			//material
			SMI_Material::Sptr windowmat = SMI_Material::Create();
			windowmat->setShader(shader);
			//render
			Renderer windowren = Renderer(windowmat, win2);
			AttachCopy(wa3, windowren);
			//transform
			SMI_Transform windowTrans = SMI_Transform();

			windowTrans.setPos(glm::vec3(-39.8, 0, -0.9));

			windowTrans.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(wa3, windowTrans);
		}
		VertexArrayObject::Sptr win3 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			wa3 = CreateEntity();
			//material
			SMI_Material::Sptr windowmat1 = SMI_Material::Create();
			windowmat1->setShader(shader);
			//render
			Renderer windowren1 = Renderer(windowmat1, win3);
			AttachCopy(wa3, windowren1);
			//transform
			SMI_Transform windowTrans1 = SMI_Transform();

			windowTrans1.setPos(glm::vec3(-49.8, 0, -0.9));

			windowTrans1.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(wa3, windowTrans1);
		}
		VertexArrayObject::Sptr win4 = ObjLoader::LoadFromFile("Models/window1.obj");
		{
			wa4 = CreateEntity();
			//material
			SMI_Material::Sptr windowmat2 = SMI_Material::Create();
			windowmat2->setShader(shader);
			//render
			Renderer windowren2 = Renderer(windowmat2, win4);
			AttachCopy(wa4, windowren2);
			//transform
			SMI_Transform windowTrans2 = SMI_Transform();

			windowTrans2.setPos(glm::vec3(-58.8, 0, -0.9));

			windowTrans2.SetDegree(glm::vec3(90, -10, 90));
			AttachCopy(wa4, windowTrans2);
		}
		VertexArrayObject::Sptr vao5 = ObjLoader::LoadFromFile("Models/barrel.obj");
		{

			barrel1 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMat1 = SMI_Material::Create();
			BarrelMat1->setShader(shader);
			//render
			Renderer BarrelRend1 = Renderer(BarrelMat1, vao5);
			AttachCopy(barrel1, BarrelRend1);
			//transform
			SMI_Transform BarrelTrans1 = SMI_Transform();

			BarrelTrans1.setPos(glm::vec3(-0.6, 0, 2.4));
		
			BarrelTrans1.SetDegree(glm::vec3(0, 90, 0));
			AttachCopy(barrel1, BarrelTrans1);
		}


		VertexArrayObject::Sptr vao6 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel2 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMat2 = SMI_Material::Create();
			BarrelMat2->setShader(shader);
			//render
			Renderer BarrelRend2 = Renderer(BarrelMat2, vao6);
			AttachCopy(barrel2, BarrelRend2);
			//transform
			SMI_Transform BarrelTrans2 = SMI_Transform();

			BarrelTrans2.setPos(glm::vec3(0.03, 0, -0.8));
			
			BarrelTrans2.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel2, BarrelTrans2);
		}

		VertexArrayObject::Sptr vao7 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel3 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMat3 = SMI_Material::Create();
			BarrelMat3->setShader(shader);
			//render
			Renderer BarrelRend3 = Renderer(BarrelMat3, vao7);
			AttachCopy(barrel3, BarrelRend3);
			//transform
			SMI_Transform BarrelTrans3 = SMI_Transform();

			BarrelTrans3.setPos(glm::vec3(-1.8, 0, -0.8));
			
			BarrelTrans3.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel3, BarrelTrans3);
		}
		VertexArrayObject::Sptr vao8 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			barrel4 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMat4 = SMI_Material::Create();
			BarrelMat4->setShader(shader);
			//render
			Renderer BarrelRend4 = Renderer(BarrelMat4, vao8);
			AttachCopy(barrel4, BarrelRend4);
			//transform
			SMI_Transform BarrelTrans4 = SMI_Transform();

			BarrelTrans4.setPos(glm::vec3(-12.8, 0, -0.8));
			
			BarrelTrans4.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(barrel4, BarrelTrans4);
		}
		VertexArrayObject::Sptr wall3 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			walls3 = CreateEntity();
			//material
			SMI_Material::Sptr WallMat = SMI_Material::Create();
			WallMat->setShader(shader);
			//render
			Renderer WallRend = Renderer(WallMat, wall3);
			AttachCopy(walls3, WallRend);
			//transform
			SMI_Transform WallTrans = SMI_Transform();

			WallTrans.setPos(glm::vec3(-23.8, 0, -0.8));

			WallTrans.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(walls3, WallTrans);
		}
		VertexArrayObject::Sptr wall4 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			walls4 = CreateEntity();
			//material
			SMI_Material::Sptr WallMat1 = SMI_Material::Create();
			WallMat1->setShader(shader);
			//render
			Renderer WallRend1 = Renderer(WallMat1, wall4);
			AttachCopy(walls4, WallRend1);
			//transform
			SMI_Transform WallTrans1 = SMI_Transform();

			WallTrans1.setPos(glm::vec3(-30.8, 0, -0.8));

			WallTrans1.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(walls4, WallTrans1);
		}
		VertexArrayObject::Sptr elevator = ObjLoader::LoadFromFile("Models/elevator.obj");
		{

			elevator1 = CreateEntity();
			//material
			SMI_Material::Sptr WallMat2 = SMI_Material::Create();
			WallMat2->setShader(shader);
			//render
			Renderer WallRend2 = Renderer(WallMat2, elevator);
			AttachCopy(elevator1, WallRend2);
			//transform
			SMI_Transform WallTrans2 = SMI_Transform();

			WallTrans2.setPos(glm::vec3(-31.8, -1.6, -0.6));

			WallTrans2.SetDegree(glm::vec3(90, 0, 0));
			AttachCopy(elevator1, WallTrans2);
		}
		VertexArrayObject::Sptr crate = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			crate1 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMat5 = SMI_Material::Create();
			BarrelMat5->setShader(shader);
			//render
			Renderer BarrelRend5 = Renderer(BarrelMat5, crate);
			AttachCopy(crate1, BarrelRend5);
			//transform
			SMI_Transform BarrelTrans5 = SMI_Transform();

			BarrelTrans5.setPos(glm::vec3(-13.8, 0, 2));
			
			BarrelTrans5.SetDegree(glm::vec3(0, 90, 0));
			AttachCopy(crate1, BarrelTrans5);
		}
		VertexArrayObject::Sptr door = ObjLoader::LoadFromFile("Models/Door2.obj");
		{

			door1 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMat6 = SMI_Material::Create();
			BarrelMat6->setShader(shader);
			//render
			Renderer BarrelRend6 = Renderer(BarrelMat6, door);
			AttachCopy(door1, BarrelRend6);
			//transform
			SMI_Transform BarrelTrans6 = SMI_Transform();

			BarrelTrans6.setPos(glm::vec3(-27.2, 0, 2));
			
			BarrelTrans6.SetDegree(glm::vec3(90, 0, -180));
			AttachCopy(door1, BarrelTrans6);
		}

		VertexArrayObject::Sptr crate1 = ObjLoader::LoadFromFile("Models/Crates1.obj");
		{

			crate2 = CreateEntity();
			//material
			SMI_Material::Sptr BarrelMat5 = SMI_Material::Create();
			BarrelMat5->setShader(shader);
			//render
			Renderer BarrelRend5 = Renderer(BarrelMat5, crate1);
			AttachCopy(crate2, BarrelRend5);
			//transform
			SMI_Transform BarrelTrans5 = SMI_Transform();

			BarrelTrans5.setPos(glm::vec3(-13.8, 0, 4));
			
			BarrelTrans5.SetDegree(glm::vec3(0, 90, 0));
			AttachCopy(crate2, BarrelTrans5);
		}
		VertexArrayObject::Sptr w = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			w1 = CreateEntity();
			//material
			SMI_Material::Sptr WallMat6 = SMI_Material::Create();
			WallMat6->setShader(shader);
			//render
			Renderer WallRend6 = Renderer(WallMat6, w);
			AttachCopy(w1, WallRend6);
			//transform
			SMI_Transform WallTrans6 = SMI_Transform();

			WallTrans6.setPos(glm::vec3(-45.8, 0, -5.6));

			WallTrans6.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(w1, WallTrans6);
		}
		VertexArrayObject::Sptr shelf = ObjLoader::LoadFromFile("Models/shelf1.obj");
		{

			shelf1 = CreateEntity();
			//material
			SMI_Material::Sptr WallMat7 = SMI_Material::Create();
			WallMat7->setShader(shader);
			//render
			Renderer WallRend7 = Renderer(WallMat7, shelf);
			AttachCopy(shelf1, WallRend7);
			//transform
			SMI_Transform WallTrans7 = SMI_Transform();

			WallTrans7.setPos(glm::vec3(-45.8, 0, -0.6));

			WallTrans7.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(shelf1 , WallTrans7);
		}
		VertexArrayObject::Sptr fan = ObjLoader::LoadFromFile("Models/cfan1.obj");
		{

			fan1 = CreateEntity();
			//material
			SMI_Material::Sptr fanmat = SMI_Material::Create();
			fanmat->setShader(shader);
			//render
			Renderer fanrend = Renderer(fanmat, fan);
			AttachCopy(fan1, fanrend);
			//transform
			SMI_Transform fanTrans = SMI_Transform();

			fanTrans.setPos(glm::vec3(-50.8, 0, 1.8));

			fanTrans.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(fan1, fanTrans);
		}
		VertexArrayObject::Sptr fanHolder = ObjLoader::LoadFromFile("Models/cholder.obj");
		{

			fanH = CreateEntity();
			//material
			SMI_Material::Sptr fanmat1 = SMI_Material::Create();
			fanmat1->setShader(shader);
			//render
			Renderer fanrend1 = Renderer(fanmat1, fanHolder);
			AttachCopy(fanH, fanrend1);
			//transform
			SMI_Transform fanTrans1 = SMI_Transform();

			fanTrans1.setPos(glm::vec3(-50.8, 0, 1.8));

			fanTrans1.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(fanH, fanTrans1);
		}
		VertexArrayObject::Sptr wall5 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			walls5 = CreateEntity();
			//material
			SMI_Material::Sptr WallMat10 = SMI_Material::Create();
			WallMat10->setShader(shader);
			//render
			Renderer WallRend10 = Renderer(WallMat10, wall5);
			AttachCopy(walls5, WallRend10);
			//transform
			SMI_Transform WallTrans10 = SMI_Transform();

			WallTrans10.setPos(glm::vec3(-57.2, 0, -5.6));

			WallTrans10.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(walls5, WallTrans10);
		}
		VertexArrayObject::Sptr shelf1 = ObjLoader::LoadFromFile("Models/shelf1.obj");
		{

			shel = CreateEntity();
			//material
			SMI_Material::Sptr ShelfM = SMI_Material::Create();
			ShelfM->setShader(shader);
			//render
			Renderer ShelfRen7 = Renderer(ShelfM, shelf1);
			AttachCopy(shel, ShelfRen7);
			//transform
			SMI_Transform ShelfTrans7 = SMI_Transform();

			ShelfTrans7.setPos(glm::vec3(-56.8, 0, -0.6));

			ShelfTrans7.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(shel, ShelfTrans7);
		}

		VertexArrayObject::Sptr whouse = ObjLoader::LoadFromFile("Models/warehouse.obj");
		{

			ware = CreateEntity();
			//material
			SMI_Material::Sptr wareh = SMI_Material::Create();
			wareh->setShader(shader);
			//render
			Renderer ShelfRen8 = Renderer(wareh, whouse);
			AttachCopy(ware, ShelfRen8);
			//transform
			SMI_Transform ShelfTrans8 = SMI_Transform();

			ShelfTrans8.setPos(glm::vec3(-56.8, 0, -0.6));

			ShelfTrans8.SetDegree(glm::vec3(90, 0, -180));
			AttachCopy(ware, ShelfTrans8);
		}
		VertexArrayObject::Sptr w3 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			f = CreateEntity();
			//material
			SMI_Material::Sptr floormat2 = SMI_Material::Create();
			floormat2->setShader(shader);
			//render
			Renderer WallRend6 = Renderer(floormat2, w3);
			AttachCopy(f, WallRend6);
			//transform
			SMI_Transform floorTrans1 = SMI_Transform();

			floorTrans1.setPos(glm::vec3(-79.8, 0, 0.6));

			floorTrans1.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(f, floorTrans1);
		}
		VertexArrayObject::Sptr w4 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			f1 = CreateEntity();
			//material
			SMI_Material::Sptr floormat3 = SMI_Material::Create();
			floormat3->setShader(shader);
			//render
			Renderer WallRend7 = Renderer(floormat3, w4);
			AttachCopy(f1, WallRend7);
			//transform
			SMI_Transform floorTrans2 = SMI_Transform();

			floorTrans2.setPos(glm::vec3(-88.8, 0, 0.6));

			floorTrans2.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(f1, floorTrans2);
		}
		VertexArrayObject::Sptr w5 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			f2 = CreateEntity();
			//material
			SMI_Material::Sptr floormat4 = SMI_Material::Create();
			floormat4->setShader(shader);
			//render
			Renderer WallRend13 = Renderer(floormat4, w5);
			AttachCopy(f2, WallRend13);
			//transform
			SMI_Transform floorTrans4 = SMI_Transform();

			floorTrans4.setPos(glm::vec3(-99.8, 0, 0.6));

			floorTrans4.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(f2, floorTrans4);
		}
		VertexArrayObject::Sptr w6 = ObjLoader::LoadFromFile("Models/nba1.obj");
		{

			f3 = CreateEntity();
			//material
			SMI_Material::Sptr floormat5 = SMI_Material::Create();
			floormat5->setShader(shader);
			//render
			Renderer WallRend15 = Renderer(floormat5, w6);
			AttachCopy(f3, WallRend15);
			//transform
			SMI_Transform floorTrans5 = SMI_Transform();

			floorTrans5.setPos(glm::vec3(-119.8, 0, 0.6));

			floorTrans5.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(f3, floorTrans5);
		}
		VertexArrayObject::Sptr cars = ObjLoader::LoadFromFile("Models/car.obj");
		{

			car = CreateEntity();
			//material
			SMI_Material::Sptr carM = SMI_Material::Create();
			carM->setShader(shader);
			//render
			Renderer carRen7 = Renderer(carM, cars);
			AttachCopy(car, carRen7);
			//transform
			SMI_Transform carTrans = SMI_Transform();

			carTrans.setPos(glm::vec3(-88.8, 0, 2.8));

			carTrans.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(car, carTrans);
		}
		VertexArrayObject::Sptr building1 = ObjLoader::LoadFromFile("Models/building1.obj");
		{

			build = CreateEntity();
			//material
			SMI_Material::Sptr buildM = SMI_Material::Create();
			buildM->setShader(shader);
			//render
			Renderer buildRen7 = Renderer(buildM, building1);
			AttachCopy(build, buildRen7);
			//transform
			SMI_Transform buildTrans = SMI_Transform();

			buildTrans.setPos(glm::vec3(-79.8, 0, 3.2));

			buildTrans.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(build, buildTrans);
		}
		VertexArrayObject::Sptr roadblock = ObjLoader::LoadFromFile("Models/build2.obj");
		{

			road1 = CreateEntity();
			//material
			SMI_Material::Sptr roadM = SMI_Material::Create();
			roadM->setShader(shader);
			//render
			Renderer roadren1 = Renderer(roadM, roadblock);
			AttachCopy(road1, roadren1);
			//transform
			SMI_Transform roadbTrans = SMI_Transform();

			roadbTrans.setPos(glm::vec3(-86.8, 0, 3.8));

			roadbTrans.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(road1, roadbTrans);
		}
		VertexArrayObject::Sptr building2 = ObjLoader::LoadFromFile("Models/building1.obj");
		{

			build3 = CreateEntity();
			//material
			SMI_Material::Sptr buildM1 = SMI_Material::Create();
			buildM1->setShader(shader);
			//render
			Renderer build1Ren7 = Renderer(buildM1, building2);
			AttachCopy(build3, build1Ren7);
			//transform
			SMI_Transform buildTrans1 = SMI_Transform();

			buildTrans1.setPos(glm::vec3(-94.8, 0, 3.2));

			buildTrans1.SetDegree(glm::vec3(90, 0, 90));
			AttachCopy(build3, buildTrans1);
		}
		VertexArrayObject::Sptr elevator1 = ObjLoader::LoadFromFile("Models/elevator.obj");
		{

			elevator12 = CreateEntity();
			//material
			SMI_Material::Sptr WallMat4 = SMI_Material::Create();
			WallMat4->setShader(shader);
			//render
			Renderer WallRend12 = Renderer(WallMat4, elevator1);
			AttachCopy(elevator12, WallRend12);
			//transform
			SMI_Transform WallTrans3 = SMI_Transform();

			WallTrans3.setPos(glm::vec3(-68.8, -1.6, -0.6));

			WallTrans3.SetDegree(glm::vec3(90, 0, 0));
			AttachCopy(elevator12, WallTrans3);
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

		//rotation example
		GetComponent<SMI_Transform>(fan1).FixedRotate(glm::vec3(0, 0, 30) * deltaTime*8.0f);
		
		// LERP example
		
		GetComponent<SMI_Transform>(door1).setPos(Lerp(glm::vec3(-27.2, 0, 2), glm::vec3(-27.2, 0, 10), t));

		GetComponent<SMI_Transform>(ware).setPos(Lerp(glm::vec3(-56.8, 0, -0.6), glm::vec3(-56.8, 0, 4.6), time));
			
		GetComponent<SMI_Transform>(car).setPos(Lerp(glm::vec3(-99.8, 0, 2.8), glm::vec3(-74.8, 0, 2.8), time));
		
		GetComponent<SMI_Transform>(elevator1).setPos(Lerp(glm::vec3(-38.8, -1.6, -0.6), glm::vec3(-38.8, -1.6, -8.6), time));

		GetComponent<SMI_Transform>(elevator12).setPos(Lerp(glm::vec3(-68.8, -1.6, -7.6), glm::vec3(-68.8, -1.6, 0.6), time));
		
		SMI_Scene::Update(deltaTime);
	}

	~GameScene() = default;

private:
	entt::entity barrel;
	entt::entity barrel1;
	entt::entity barrel2;
	entt::entity barrel3;
	entt::entity barrel4;
	entt::entity crate1;
	entt::entity door1;
	entt::entity crate2;
	entt::entity walls3;
	entt::entity walls4;
	entt::entity elevator1;
	entt::entity elevator12;
	entt::entity w1;
	entt::entity shelf1;
	entt::entity fan1;
	entt::entity walls5;
	entt::entity shel;
	entt::entity fanH;
	entt::entity ware;
	entt::entity f;
	entt::entity f1;
	entt::entity f2;
	entt::entity f3;
	entt::entity car;
	entt::entity build;
	entt::entity build3;
	entt::entity road1;
	entt::entity wa1;
	entt::entity wa2;
	entt::entity wa3;
	entt::entity wa4;
	entt::entity wa5;
	entt::entity wa6;
	float max = 5;
	float current = 0;
	float c = 0;
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

	GameScene MainScene = GameScene();
	MainScene.InitScene();

	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {

		glfwPollEvents();

		
		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);

		// Clear the color and depth buffers
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		

		MainScene.Update(dt);
		MainScene.Render();

		lastFrame = thisFrame;
		glfwSwapBuffers(window);
	}

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
} 
