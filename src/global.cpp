#include "global.hpp"

GlobalState state = {
    .win = NULL,
    .deltaTime = 0.0f,
    .lastTime = 0.0f,
    .currentSoundFile = NULL,
    .currentPlaylist = -1, 
    .playingPlaylist = -1,

    .soundPosUpdateTimer = 1.0f,
    .soundPosUpdateTime = 0.0f,

    .showVolumeSliderTrackDisplay = false, 
    .showVolumeSliderOverride = false,

    .volumeBeforeMute = VOLUME_INIT,

    .playlistDownloadRunning = false, 
    .playlistDownloadFinished = false
};

void miniaudioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder_read_pcm_frames(&state.soundHandler.decoder, pOutput, frameCount, NULL);

    float* outputBuffer = (float*)pOutput;
    for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {
        outputBuffer[i] *= state.soundHandler.volume / (float)VOLUME_MAX;
    }
}
