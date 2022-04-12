//Framebuffer code based on code provided by Ame Gilham

#pragma once
//large include stored aside
#include "SMI_Include.h"

#include "Texture2D.h"
#include "Shader.h"

namespace Semi
{
	//setup Depth target for later
	struct SMI_DepthTarget
	{
		//destructor
		~SMI_DepthTarget();
		//delete texture
		void Unload();
		//hold texture
		Texture2D::Sptr m_texture;
	};

	struct SMI_ColourTarget
	{
		//destructor
		~SMI_ColourTarget();
		//deletes texture
		void Unload();
		//holds colour texture
		std::vector<Texture2D::Sptr> m_textures;
		std::vector<GLenum> m_formats;
		std::vector<GLenum> m_buffers;

		//stores the amount of colour attachments for the target
		unsigned int m_AttatchmentCount = 0;
	};


	class SMI_Framebuffer
	{
	public:
		//defines a better name for shared pointers
		typedef std::shared_ptr<SMI_Framebuffer> ssptr;

		static inline ssptr Create()
		{
			return std::make_shared<SMI_Framebuffer>();
		}


	public:
		SMI_Framebuffer();
		~SMI_Framebuffer();

		void Unload();

		virtual void Init(unsigned width, unsigned height);

		void Init();

		void AddDepthTarget();

		void AddColourTarget(GLenum format);


		void BindDepthAsTexture(int textureSlot) const;

		void BindColorAsTexture(unsigned colourBuffer, int textureSlot) const;
		
		void UnbindTexture(int textureSlot) const;


		void Reshape(unsigned width, unsigned height);
		void SetSize(unsigned width, unsigned height);

		void SetViewport() const;

		void Bind() const;
		void UnBind() const;

		void RenderFS() const;

		void DrawtoBackbuffer() ;

		void Clear();
		bool CheckFBO();

		//initialize fullscreen quad
		static void InitFullscreen();
		static void DrawFullscreen();


		//get framebuffer handle
		GLuint GetHandle() { return m_handle; }

		unsigned int m_width = 0;
		unsigned int m_height = 0;

		GLenum m_filter = GL_NEAREST;
		GLenum m_wrap = GL_CLAMP_TO_EDGE;

	protected:
		GLuint m_handle;
		SMI_DepthTarget m_depth;
		SMI_ColourTarget m_colour;

		GLbitfield m_clearFlag = 0;

		bool m_isInit = false;
		bool m_depthActive = false;

		inline static GLuint m_fullscreenQuadVBO = 0;
		inline static GLuint m_fullscreenQuadVAO = 0;

		static int m_maxColourAttachment;
		inline static bool m_isInitFSQ = false;
	};
}

