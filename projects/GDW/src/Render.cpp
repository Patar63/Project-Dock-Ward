#include "Render.h"

Renderer::Renderer()
{
	m_Material = nullptr;
	m_VAO = nullptr;
}

Renderer::Renderer(SMI_Material::Sptr _mat, VertexArrayObject::Sptr _vao)
{
	setMaterial(_mat);
	setVAO(_vao);
}

void Renderer::Render()
{
	if (m_Material != nullptr && m_VAO != nullptr)
	{
		//bind shaders and uniforms to materials
		m_Material->getShader()->Bind();
		m_Material->BindAllUniform();
		//draw and unbind
		m_VAO->Draw();
		m_Material->getShader()->Unbind();
	}
}

Renderer::~Renderer()
{
}
