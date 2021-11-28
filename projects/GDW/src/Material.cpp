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
		it->second->SetUniformMatrix(m_Shader);
		it++;
	}
}

void SMI_Material::setUniform(const Uniform::Sptr& _uniform)
{
	std::string Name = _uniform->getName();
	m_UniformMap[Name] = _uniform;
}

Uniform::Sptr SMI_Material::getUniform(const std::string& UniformName)
{
	if (m_UniformMap.find(UniformName) != m_UniformMap.end())
	{
		return m_UniformMap.at(UniformName);
	}
	
	return nullptr;
}

SMI_Material::~SMI_Material()
{
}
