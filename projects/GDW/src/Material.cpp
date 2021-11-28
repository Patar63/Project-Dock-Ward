#include "Material.h"

SMI_Material::SMI_Material()
{
}

void SMI_Material::BindAllUniform()
{
	std::unordered_map<std::string, Uniform::Sptr>::iterator it = m_UniformMap.begin();

	while (it != m_UniformMap.end())
	{
		it->second->SetUniform(m_Shader);
		it++;
	}
}

void SMI_Material::setUniform(const Uniform::Sptr& _uniform)
{
	std::string Name = _uniform->getName();
	m_UniformMap[Name] = _uniform;
}

SMI_Material::~SMI_Material()
{
}
