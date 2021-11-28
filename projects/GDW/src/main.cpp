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
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

		// Get uniform location for the model view projection
		Camera::Sptr camera = Camera::Create();
		camera->SetPosition(glm::vec3(0, 3, 5));
		camera->LookAt(glm::vec3(0.0f));
		camera->SetOrthoVerticalScale(5);
		setCamera(camera);

		//creates object
		VertexArrayObject::Sptr vao4 = ObjLoader::LoadFromFile("Models/barrel.obj");
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
			BarrelTrans.setPos(glm::vec3(0, 0, 0));
			BarrelTrans.setScale(glm::vec3(1, 1, 1));
			BarrelTrans.SetDegree(glm::vec3(0, 0, 0));
			AttachCopy(barrel, BarrelTrans);
		}
	}

	void Update(float deltaTime)
	{
		//incriment time
		current += deltaTime;
		if (current > max)
		{
			current = 0;
		}
		float t = current / max;

		//rotation example
		//GetComponent<SMI_Transform>(barrel).FixedRotate(glm::vec3(0, 0, 30) * deltaTime);
		
		// LERP example
		//GetComponent<SMI_Transform>(barrel).setPos(Lerp(glm::vec3(0, 0, 0), glm::vec3(7, 0, 0), t));
		SMI_Scene::Update(deltaTime);
	}

	~GameScene() = default;

private:
	entt::entity barrel;
	float max = 5;
	float current = 0;
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


		// Calculate the time since our last frame (dt)
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
