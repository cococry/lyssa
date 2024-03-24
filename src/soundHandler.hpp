#pragma once
#include "config.hpp"
#include "log.hpp"

#include <string>
#include <stdint.h>

#include <miniaudio.h>

class SoundHandler {
    public:
        std::string path;

        bool isPlaying = false, isInit = false;
        double lengthInSeconds = 0;

        uint32_t volume = VOLUME_INIT;

        void init(const std::string& filepath, ma_device_data_proc dataCallback) {
            this->path = filepath;
            if (ma_decoder_init_file(filepath.c_str(), NULL, &this->decoder) != MA_SUCCESS) {
                LOG_ERROR("Failed to load Sound '%s'.\n", filepath.c_str());
                return;
            }

            ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
            deviceConfig.playback.format = this->decoder.outputFormat;
            deviceConfig.playback.channels = this->decoder.outputChannels;
            deviceConfig.sampleRate = this->decoder.outputSampleRate;
            deviceConfig.dataCallback = dataCallback;
            deviceConfig.pUserData         = &this->decoder;

            if (ma_device_init(NULL, &deviceConfig, &this->device) != MA_SUCCESS) {
                return;
            }
            ma_uint64 lengthInFrames;
            ma_decoder_get_length_in_pcm_frames(&this->decoder, &lengthInFrames);
            lengthInSeconds = (double)lengthInFrames / decoder.outputSampleRate;

            isInit = true;
        }
        void uninit() {
            if(!this->isInit) return;
            ma_device_stop(&this->device);
            ma_device_uninit(&this->device);
            ma_decoder_uninit(&this->decoder);
            isInit = false;
        }

        void play() {
            if(this->isPlaying) return;
            ma_device_start(&this->device);
            isPlaying = true;
        }
        void stop() {
            if(!this->isPlaying) return;
            ma_device_stop(&this->device);
            isPlaying = false;
        }

        double getPositionInSeconds() {
            if(!isInit) return 0.0;

            ma_uint64 cursorInFrames;
            ma_decoder_get_cursor_in_pcm_frames(&this->decoder, &cursorInFrames);
            return (double)cursorInFrames / this->decoder.outputSampleRate;
        }
        void setPositionInSeconds(double position) {
            ma_uint64 targetFrame = (ma_uint64)(position * this->decoder.outputSampleRate);

            // Stop the device before seeking
            if(isPlaying)
                ma_device_stop(&this->device);

            if(ma_decoder_seek_to_pcm_frame(&this->decoder, targetFrame) != MA_SUCCESS) {
                LOG_ERROR("Sound position in seconds invalid.\n");
            }

            if(isPlaying)
                ma_device_start(&this->device);
        }

        ma_device device;
        ma_decoder decoder;
        static double getSoundDuration(const std::string& soundPath); 
    private:
};
