/*
 * Copyright (C) 2016 The CyanogenMod Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "audio_amplifier"
//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <stdio.h>
#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <stdint.h>
#include <sys/types.h>

#include <cutils/log.h>

#include <hardware/audio_amplifier.h>
#include <hardware/hardware.h>

#include <system/audio.h>
#include <tinyalsa/asoundlib.h>
#include <tinycompress/tinycompress.h>
#include <msm8916/platform.h>
#include <audio_hw.h>

typedef struct tfa9887_amplifier {
    amplifier_device_t amp;
    int num_streams;
    bool calibrating;
    bool writing;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} tfa9887_amplifier_t;

enum tfa9887_Audio_Mode
{
    Audio_Mode_Music_Normal = 0,
    Audio_Mode_Voice_NB,
    Audio_Mode_Voice_NB_EXTRA,
    Audio_Mode_Voice_WB,
    Audio_Mode_Voice_WB_EXTRA,
    Audio_Mode_VT_NB,
    Audio_Mode_VT_WB,
    Audio_Mode_Voice_VOIP,
    Audio_Mode_Voice_VoLTE,
    Audio_Mode_Voice_VoLTE_EXTRA,
    Audio_Mode_VT_VoLTE,
};

extern int tfa9887_dualspeakeron(int mode, int first);
extern int tfa9887_dualspeakeroff();
extern int tfa9887_dualcalibration_startup();

static int quat_mi2s_interface_en(bool enable)
{
    enum mixer_ctl_type type;
    struct mixer_ctl *ctl;
    struct mixer *mixer = mixer_open(0);

    if (mixer == NULL) {
        ALOGE("Error opening mixer 0");
        return -1;
    }

    ctl = mixer_get_ctl_by_name(mixer, "QUAT_MI2S_RX Audio Mixer MultiMedia1");
    if (ctl == NULL) {
        mixer_close(mixer);
        ALOGE("Could not find QUAT_MI2S_RX Audio Mixer MultiMedia1");
        return -1;
    }

    type = mixer_ctl_get_type(ctl);
    if (type != MIXER_CTL_TYPE_BOOL) {
        ALOGE("QUAT_MI2S_RX Audio Mixer MultiMedia1 is not supported");
        mixer_close(mixer);
        return -1;
    }

    mixer_ctl_set_value(ctl, 0, enable);
    mixer_close(mixer);
    return 0;
}

void *write_dummy_data(void *param)
{
    char *buffer;
    int size;
    struct pcm *pcm;
    struct pcm_config config;
    tfa9887_amplifier_t *tfa9887 = (tfa9887_amplifier_t *) param;

    config.channels = 2;
    config.rate = 48000;
    config.period_size = 256;
    config.period_count = 2;
    config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = config.period_size * config.period_count - 1;
    config.stop_threshold = config.period_size * config.period_count;
    config.silence_threshold = 0;
    config.avail_min = 1;

    if (quat_mi2s_interface_en(true)) {
        ALOGE("Failed to enable QUAT_MI2S_RX Audio Mixer MultiMedia1");
        return NULL;
    }

    pcm = pcm_open(0, 0, PCM_OUT | PCM_MONOTONIC, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        ALOGE("pcm_open failed: %s", pcm_get_error(pcm));
        if (pcm) {
            goto err_close_pcm;
        }
        goto err_disable_quat;
    }

    size = DEEP_BUFFER_OUTPUT_PERIOD_SIZE * 8;
    buffer = calloc(size, 1);
    if (!buffer) {
        ALOGE("failed to allocate buffer");
        goto err_close_pcm;
    }

    do {
        if (pcm_write(pcm, buffer, size)) {
            ALOGE("pcm_write failed");
        }
        pthread_mutex_lock(&tfa9887->mutex);
        tfa9887->writing = true;
        pthread_cond_signal(&tfa9887->cond);
        pthread_mutex_unlock(&tfa9887->mutex);
    } while (tfa9887->calibrating);

err_free:
    free(buffer);
err_close_pcm:
    pcm_close(pcm);
err_disable_quat:
    quat_mi2s_interface_en(false);
    ALOGV("--%s:%d", __func__, __LINE__);
    return NULL;
}

bool is_amplifier_device(uint32_t devices)
{
    return devices & AUDIO_DEVICE_OUT_SPEAKER;
}

static int amp_output_stream_start(amplifier_device_t *device,
        struct audio_stream_out *stream, bool offload)
{
    tfa9887_amplifier_t *tfa9887 = (tfa9887_amplifier_t *) device;
    struct stream_out *out = (struct stream_out *)stream;
    char *buffer;
    int size;

    if (!is_amplifier_device(out->devices))
        return 0;

    pthread_mutex_lock(&tfa9887->mutex);
    if (tfa9887->num_streams == 0) {
        size = pcm_frames_to_bytes(out->pcm, pcm_get_buffer_size(out->pcm));
        buffer = calloc(size, 1);
        if (!buffer) {
            pthread_mutex_unlock(&tfa9887->mutex);
            return -ENOMEM;
        }

        if (offload) {
            compress_write(out->compr, buffer, size);
        } else {
            if (out->usecase == USECASE_AUDIO_PLAYBACK_AFE_PROXY)
                pcm_mmap_write(out->pcm, buffer, size);
            else
                pcm_write(out->pcm, buffer, size);
        }

        tfa9887_dualspeakeron(Audio_Mode_Music_Normal, 0);
        free(buffer);
    }

    tfa9887->num_streams++;
    pthread_mutex_unlock(&tfa9887->mutex);
    return 0;
}

static int amp_output_stream_standby(amplifier_device_t *device,
        struct audio_stream_out *stream)
{
    tfa9887_amplifier_t *tfa9887 = (tfa9887_amplifier_t *) device;
    struct stream_out *out = (struct stream_out *)stream;

    if (!is_amplifier_device(out->devices))
        return 0;

    pthread_mutex_lock(&tfa9887->mutex);
    tfa9887->num_streams--;
    if (tfa9887->num_streams == 0) {
        tfa9887_dualspeakeroff();
    }
    pthread_mutex_unlock(&tfa9887->mutex);
    return 0;
}

static int amp_dev_close(hw_device_t *device)
{
    tfa9887_amplifier_t *tfa9887 = (tfa9887_amplifier_t *) device;
    if (tfa9887) {
        pthread_mutex_lock(&tfa9887->mutex);
        if (tfa9887->num_streams) {
            tfa9887_dualspeakeroff();
            tfa9887->num_streams = 0;
        }
        pthread_mutex_unlock(&tfa9887->mutex);
        pthread_cond_destroy(&tfa9887->cond);
        pthread_mutex_destroy(&tfa9887->mutex);
        free(tfa9887);
    }

    return 0;
}

static int amp_calibrate(tfa9887_amplifier_t *tfa9887)
{
    pthread_t write_thread;
    tfa9887->calibrating = true;
    pthread_create(&write_thread, NULL, write_dummy_data, tfa9887);
    pthread_mutex_lock(&tfa9887->mutex);
    while(!tfa9887->writing) {
        pthread_cond_wait(&tfa9887->cond, &tfa9887->mutex);
    }
    pthread_mutex_unlock(&tfa9887->mutex);
    tfa9887_dualcalibration_startup();
    tfa9887->calibrating = false;
    pthread_join(write_thread, NULL);
    return 0;
}

static int amp_module_open(const hw_module_t *module, const char *name,
        hw_device_t **device)
{
    char PRJ_ID[256];

    FILE *fp = fopen("/proc/apid" , "r");
    fgets(PRJ_ID, sizeof(PRJ_ID), fp);
    pclose(fp);

    if (strncmp(PRJ_ID, "1\n", sizeof(PRJ_ID)))
        return 1;

    if (strcmp(name, AMPLIFIER_HARDWARE_INTERFACE)) {
        ALOGE("%s:%d: %s does not match amplifier hardware interface name\n",
                __func__, __LINE__, name);
        return -ENODEV;
    }

    tfa9887_amplifier_t *tfa9887 = calloc(1, sizeof(tfa9887_amplifier_t));
    if (!tfa9887) {
        ALOGE("%s:%d: Unable to allocate memory for amplifier device\n",
                __func__, __LINE__);
        return -ENOMEM;
    }

    tfa9887->amp.common.tag = HARDWARE_DEVICE_TAG;
    tfa9887->amp.common.module = (hw_module_t *) module;
    tfa9887->amp.common.version = HARDWARE_DEVICE_API_VERSION(1, 0);
    tfa9887->amp.common.close = amp_dev_close;

    tfa9887->amp.output_stream_start = amp_output_stream_start;
    tfa9887->amp.output_stream_standby = amp_output_stream_standby;
    pthread_mutex_init(&tfa9887->mutex, NULL);
    pthread_cond_init(&tfa9887->cond, NULL);

    amp_calibrate(tfa9887);

    *device = (hw_device_t *) tfa9887;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = amp_module_open,
};

amplifier_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AMPLIFIER_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AMPLIFIER_HARDWARE_MODULE_ID,
        .name = "MSM8916 audio amplifier HAL",
        .author = "The CyanogenMod Open Source Project",
        .methods = &hal_module_methods,
    },
};
