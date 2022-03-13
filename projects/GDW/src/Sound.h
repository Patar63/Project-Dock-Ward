#pragma once

#include "fmod.hpp"
#include <string>
#include <unordered_map>
class Sound
{
public:
	
	void init();
	void update();
	void shutdown();

	void loadsounds(const std::string& soundName, const std::string& filename,bool b3d,bool bLooping=false,bool bStream=false);
	void unloadsound(const std::string& soundName);
	void playsoundbyName(const std::string& soundName);

private:
	FMOD::System* pSystem;
	std::unordered_map<std::string, FMOD::Sound*>sounds;

};

