#pragma once
#include <memory>

class Material
{
public:
	typedef std::shared_ptr<Material> Sptr;

	  inline static Sptr Create() {
		  return std::make_shared<Material>();
	  }

public:
	//constructor
	Material();

	//destructor
	~Material();

private:

};