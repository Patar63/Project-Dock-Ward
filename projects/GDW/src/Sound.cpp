#include "Sound.h"



void Sound::init()
{
	FMOD::System_Create(&pSystem);
	pSystem->init(32, FMOD_INIT_NORMAL, nullptr);
}

void Sound::update()
{
	pSystem->update();
}

void Sound::shutdown()
{
	
	pSystem->close();
	pSystem->release();
}

void Sound::loadsounds(const std::string& soundName, const std::string& filename, bool b3d, bool bLooping, bool bStream)
{
	FMOD::Sound* loadedSound;
	pSystem->createSound(filename.c_str(), FMOD_3D, nullptr, &loadedSound);
	pSystem->playSound(loadedSound, nullptr, false, nullptr);
	
	
}

void Sound::unloadsound(const std::string& soundName)
{

}

void Sound::playsoundbyName(const std::string& soundName)
{
}
