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
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return;
    }

    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, NULL);

    (void)pInput;
}
