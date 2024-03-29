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

    float gain = state.soundHandler.volume / VOLUME_MAX; // Convert percent to fraction

    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, NULL);

    if (pDevice->playback.format == ma_format_f32) {
        float* pOutputF32 = (float*)pOutput;
        for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {
            pOutputF32[i] *= gain;
        }
    } else if (pDevice->playback.format == ma_format_s16) {
        ma_int16* pOutputS16 = (ma_int16*)pOutput;
        for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {
            pOutputS16[i] = (ma_int16)(pOutputS16[i] * gain);
        }
    }

    (void)pInput;
}

void changeTabTo(GuiTab tab)  {
  if(state.currentTab == tab) return;
  state.currentTab = tab;
}
