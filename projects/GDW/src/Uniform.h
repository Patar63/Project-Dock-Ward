#pragma once
#include "Shader.h"
#include "GLM/glm.hpp"
#include <memory>
#include <string>

class Uniform
{
public:
	typedef std::shared_ptr<Uniform> Sptr;

public:
	//pure virtual function. Acts as a parent for UniformObject
	virtual void SetUniform(Shader::Sptr) = 0;

	//setters
	void setName(const std::string& _name) { UniformName = _name; }
	
	//getters
	std::string getName() const { return UniformName; }

protected:
	std::string UniformName;
};


template <typename T>
class UniformObject : public Uniform
{
public:
	typedef std::shared_ptr<UniformObject> Sptr;

	inline static Sptr Create() {
		return std::make_shared<UniformObject>();
	}

public:
	//declare function
	void SetUniform(Shader::Sptr);

	//setters
	void setData(const T& _data) { UniformData = _data; }

	//getters
	T getData() const { return UniformData; }

protected:
	//T variable to represent the Uniform. Passed to shader
	T UniformData;
};


//function to set the Uniform
template<typename T>
inline void UniformObject<T>::SetUniform(Shader::Sptr shader)
{
	//sets if Uniform is a matrix
	if (std::is_same_v<T, glm::mat3> || std::is_same_v<T, glm::mat4>)
	{
		shader->SetUniformMatrix(UniformName, UniformData);
	}
	//sets if Uniform is anything but a matrix
	else
	{
		shader->SetUniform(UniformName, UniformData);
	}
}
