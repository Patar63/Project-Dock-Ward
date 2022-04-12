#include "SMI_Include.h"
#include "Framebuffer.h"

Semi::SMI_DepthTarget::~SMI_DepthTarget()
{
	//unloaad depth target
	Unload();
}

void Semi::SMI_DepthTarget::Unload()
{
	//deletes texture from given handle
	glDeleteTextures(1, &m_texture->GetHandle());
}

Semi::SMI_ColourTarget::~SMI_ColourTarget()
{
	Unload();
}

void Semi::SMI_ColourTarget::Unload()
{
	for (int i = 0; i < m_AttatchmentCount; i++)
	{
		glDeleteTextures(1, &m_textures[i]->GetHandle());
	}
}

Semi::SMI_Framebuffer::SMI_Framebuffer()
{
}

Semi::SMI_Framebuffer::~SMI_Framebuffer()
{
	Unload();
}

void Semi::SMI_Framebuffer::Unload()
{
	glDeleteFramebuffers(1, &m_handle);
	m_isInit = false;
}

void Semi::SMI_Framebuffer::Init(unsigned width, unsigned height)
{
	SetSize(width, height);

	Init();
}

void Semi::SMI_Framebuffer::Init()
{
	//if the fullscreen quad has not been initliazed, initliaze it 
	if (!m_isInitFSQ)
		InitFullscreen();

	//Generates the FBO
	glGenFramebuffers(1, &m_handle);
	//Bind it
	glBindFramebuffer(GL_FRAMEBUFFER, m_handle);

	if (m_depthActive)
	{
		//because we have depth we need to clear our depth bit
		m_clearFlag |= GL_DEPTH_BUFFER_BIT;

		//create the pointer for the texture
		m_depth.m_texture = std::make_shared<Texture2D>();

		//Generate the texture
		glGenTextures(1, &m_depth.m_texture->GetHandle());
		//Binds the texture
		glBindTexture(GL_TEXTURE_2D, m_depth.m_texture->GetHandle());
		//Sets the texture data
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, m_width, m_height);

		//Set texture parameters
		glTextureParameteri(m_depth.m_texture->GetHandle(), GL_TEXTURE_MIN_FILTER, m_filter);
		glTextureParameteri(m_depth.m_texture->GetHandle(), GL_TEXTURE_MAG_FILTER, m_filter);
		glTextureParameteri(m_depth.m_texture->GetHandle(), GL_TEXTURE_WRAP_S, m_wrap);
		glTextureParameteri(m_depth.m_texture->GetHandle(), GL_TEXTURE_WRAP_T, m_wrap);

		//Sets up as a framebuffer texture
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth.m_texture->GetHandle(), 0);

		glBindTexture(GL_TEXTURE_2D, GL_NONE);
	}

	//If there is more than zero color attachments
		//We create them
	if (m_colour.m_AttatchmentCount)
	{
		//Because we have a color target we include a color buffer bit into clear flag
		m_clearFlag |= GL_COLOR_BUFFER_BIT;
		//Creates the GLuints to hold the new texture handles;
		GLuint* textureHandles = new GLuint[m_colour.m_AttatchmentCount];

		glGenTextures(m_colour.m_AttatchmentCount, textureHandles);

		//Loops through them
		for (unsigned i = 0; i < m_colour.m_AttatchmentCount; i++)
		{
			//create the texture pointers
			m_colour.m_textures[i] = std::make_shared<Texture2D>();

			//set the handle
			m_colour.m_textures[i]->GetHandle() = textureHandles[i];

			//Binds the texture
			glBindTexture(GL_TEXTURE_2D, m_colour.m_textures[i]->GetHandle());
			//Sets the texture storage
			glTexStorage2D(GL_TEXTURE_2D, 1, m_colour.m_formats[i], m_width, m_height);

			//Set texture parameters
			glTextureParameteri(m_colour.m_textures[i]->GetHandle(), GL_TEXTURE_MIN_FILTER, m_filter);
			glTextureParameteri(m_colour.m_textures[i]->GetHandle(), GL_TEXTURE_MAG_FILTER, m_filter);
			glTextureParameteri(m_colour.m_textures[i]->GetHandle(), GL_TEXTURE_WRAP_S, m_wrap);
			glTextureParameteri(m_colour.m_textures[i]->GetHandle(), GL_TEXTURE_WRAP_T, m_wrap);

			//Sets up as a framebuffer texture
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_colour.m_textures[i]->GetHandle(), 0);
		}

		delete[] textureHandles;
	}

	//Make sure it's set up right
	CheckFBO();
	//Unbind buffer
	glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
	//Set init to true
	m_isInit = true;
}

void Semi::SMI_Framebuffer::AddDepthTarget()
{
	if (m_depth.m_texture != nullptr && m_depth.m_texture->GetHandle())
	{
		m_depth.Unload();
	}

	m_depthActive = true;
}

void Semi::SMI_Framebuffer::AddColourTarget(GLenum format)
{
	//Resizes the textures to number of attachments
	m_colour.m_textures.resize(m_colour.m_AttatchmentCount + 1);
	//Add the format
	m_colour.m_formats.push_back(format);
	//Add the color attachment buffer number
	m_colour.m_buffers.push_back(GL_COLOR_ATTACHMENT0 + m_colour.m_AttatchmentCount);
	//Incremenets number of attachments
	m_colour.m_AttatchmentCount++;
}

void Semi::SMI_Framebuffer::BindDepthAsTexture(int textureSlot) const
{
	m_depth.m_texture->Bind(textureSlot);
}

void Semi::SMI_Framebuffer::BindColorAsTexture(unsigned colourBuffer, int textureSlot) const
{
	m_colour.m_textures[colourBuffer]->Bind(textureSlot);
}

void Semi::SMI_Framebuffer::UnbindTexture(int textureSlot) const
{
	//bind to gl_none
	glActiveTexture(GL_TEXTURE0 + textureSlot);
	glBindTexture(GL_TEXTURE_2D, GL_NONE);
}

void Semi::SMI_Framebuffer::Reshape(unsigned width, unsigned height)
{
	//Set size
	SetSize(width, height);
	//Unloads the framebuffer
	Unload();
	//Unload the depth target
	m_depth.Unload();
	//Unloads the color target
	m_colour.Unload();
	//Inits the framebuffer
	Init();
}

void Semi::SMI_Framebuffer::SetSize(unsigned width, unsigned height)
{
	m_width = width;
	m_height = height;
}

void Semi::SMI_Framebuffer::SetViewport() const
{
	glViewport(0, 0, m_width, m_height);
}

void Semi::SMI_Framebuffer::Bind() const
{
	//bind the buffer
	glBindFramebuffer(GL_FRAMEBUFFER, m_handle);

	//add all the color buffers
	if (m_colour.m_AttatchmentCount)
	{
		glDrawBuffers(m_colour.m_AttatchmentCount, &m_colour.m_buffers[0]);
	}
}

void Semi::SMI_Framebuffer::UnBind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

void Semi::SMI_Framebuffer::RenderFS() const
{
	//Sets viewport
	SetViewport();
	//Bind the framebuffer
	Bind();
	//Draw full screen quad
	DrawFullscreen();
	//Unbind the framebuffer
	UnBind();
}

void Semi::SMI_Framebuffer::DrawtoBackbuffer() 
{
	//bind the read and draw framebuffer
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GL_NONE);

	//Blits the framebuffer to the back buffer
	glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, GL_NONE);
}

void Semi::SMI_Framebuffer::Clear()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
	glClear(m_clearFlag);
	glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

bool Semi::SMI_Framebuffer::CheckFBO()
{
	Bind();
	//check status of framebuffer
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		LOG_ERROR("Framebuffer is not correctly set up");
		return false;
	}

	return true;
}

void Semi::SMI_Framebuffer::InitFullscreen()
{
	//A vbo with Uvs and verts from
	//-1 to 1 for verts
	//0 to 1 for UVs
	float VBO_DATA[]
	{
		-1.f, -1.f, 0.f,
		1.f, -1.f, 0.f,
		-1.f, 1.f, 0.f,

		1.f, 1.f, 0.f,
		-1.f, 1.f, 0.f,
		1.f, -1.f, 0.f,

		0.f, 0.f,
		1.f, 0.f,
		0.f, 1.f,

		1.f, 1.f,
		0.f, 1.f,
		1.f, 0.f
	};
	//Vertex size is 6pts * 3 data points * sizeof (float)
	int vertexSize = 6 * 3 * sizeof(float);
	//texcoord size = 6pts * 2 data points * sizeof(float)
	int texCoordSize = 6 * 2 * sizeof(float);

	//Generates vertex array
	glGenVertexArrays(1, &m_fullscreenQuadVAO);
	//Binds VAO
	glBindVertexArray(m_fullscreenQuadVAO);

	//Enables 2 vertex attrib array slots
	glEnableVertexAttribArray(0); //Vertices
	glEnableVertexAttribArray(1); //UVS

	//Generates VBO
	glGenBuffers(1, &m_fullscreenQuadVBO);

	//Binds the VBO
	glBindBuffer(GL_ARRAY_BUFFER, m_fullscreenQuadVBO);
	//Buffers the vbo data
	glBufferData(GL_ARRAY_BUFFER, vertexSize + texCoordSize, VBO_DATA, GL_STATIC_DRAW);

#pragma warning(push)
#pragma warning(disable : 4312)
	//Sets first attrib array to point to the beginning of the data
	glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
	//Sets the second attrib array to point to an offset in the data
	glVertexAttribPointer((GLuint)1, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(vertexSize));
#pragma warning(pop)

	glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
	glBindVertexArray(GL_NONE);

	m_isInitFSQ = true;
}

void Semi::SMI_Framebuffer::DrawFullscreen()
{
	glBindVertexArray(m_fullscreenQuadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(GL_NONE);
}
