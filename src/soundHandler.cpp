#include "soundHandler.hpp"

#include "global.hpp"

double SoundHandler::getSoundDuration(const std::string &soundPath) {
  SoundHandler sound; 
  sound.init(soundPath, miniaudioDataCallback);
  double duration = sound.lengthInSeconds;
  sound.uninit();
  return duration;
}
