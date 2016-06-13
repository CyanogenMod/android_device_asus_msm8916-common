/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2014, The CyanogenMod Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_NIDEBUG 0

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>

#define LOG_TAG "ASUS/QCOM PowerHAL"
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/power.h>

#include "utils.h"
#include "metadata-defs.h"
#include "hint-data.h"
#include "performance.h"
#include "power-common.h"

#define DEFAULT_INTERACTION_BOOST_MS    50
#define LAUNCH_BOOST_DURATION_MS        100
#define MAX_INTERACTION_BOOST_MS        100
#define MAX_CPU_BOOST_MS                100

static int is_8916 = -1;

int get_number_of_profiles() {
    return 3;
}

static int current_power_profile = PROFILE_BALANCED;

static int profile_high_performance_8916[3] = {
    0x1C00, 0x0901, CPU0_MIN_FREQ_TURBO_MAX,
};

static int profile_high_performance_8939[11] = {
    SCHED_BOOST_ON, 0x1C00, 0x0901,
    CPU0_MIN_FREQ_TURBO_MAX, CPU1_MIN_FREQ_TURBO_MAX,
    CPU2_MIN_FREQ_TURBO_MAX, CPU3_MIN_FREQ_TURBO_MAX,
    CPU4_MIN_FREQ_TURBO_MAX, CPU5_MIN_FREQ_TURBO_MAX,
    CPU6_MIN_FREQ_TURBO_MAX, CPU7_MIN_FREQ_TURBO_MAX,
};

static int profile_power_save_8916[1] = {
    CPU0_MAX_FREQ_NONTURBO_MAX,
};

static int profile_power_save_8939[5] = {
    CPUS_ONLINE_MAX_LIMIT_2,
    CPU0_MAX_FREQ_NONTURBO_MAX, CPU1_MAX_FREQ_NONTURBO_MAX,
    CPU2_MAX_FREQ_NONTURBO_MAX, CPU3_MAX_FREQ_NONTURBO_MAX,
};

static int is_target_8916()
{
    int fd;
    char buf[10] = {0};

    if (is_8916 >= 0)
        return is_8916;

    fd = open("/sys/devices/soc0/soc_id", O_RDONLY);
    if (fd >= 0) {
        if (read(fd, buf, sizeof(buf) - 1) == -1) {
            ALOGW("Unable to read soc_id");
            is_8916 = 1;
        } else {
            int soc_id = atoi(buf);
            if (soc_id == 206 || (soc_id >= 247 && soc_id <= 250))  {
                is_8916 = 1;
            } else {
                is_8916 = 0;
            }
        }
    } else {
      is_8916 = 1;
    }

    close(fd);
    return is_8916;
}

static void set_power_profile(int profile) {

    if (profile == current_power_profile)
        return;

    ALOGV("%s: profile=%d", __func__, profile);

    if (current_power_profile != PROFILE_BALANCED) {
        undo_hint_action(DEFAULT_PROFILE_HINT_ID);
        ALOGV("%s: hint undone", __func__);
    }

    if (profile == PROFILE_HIGH_PERFORMANCE) {
        int *resource_values = is_target_8916() ?
            profile_high_performance_8916 : profile_high_performance_8939;

        perform_hint_action(DEFAULT_PROFILE_HINT_ID,
            resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
        ALOGD("%s: set performance mode", __func__);

    } else if (profile == PROFILE_POWER_SAVE) {
        int *resource_values = is_target_8916() ?
            profile_power_save_8916 : profile_power_save_8939;

        perform_hint_action(DEFAULT_PROFILE_HINT_ID,
            resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
        ALOGD("%s: set powersave", __func__);
    }

    current_power_profile = profile;
}

extern void interaction(int duration, int num_args, int opt_list[]);

int power_hint_override(struct power_module *module __unused, power_hint_t hint, void *data)
{
    if (hint == POWER_HINT_SET_PROFILE) {
        set_power_profile(*(int32_t *)data);
    }

    // Skip other hints in custom power modes
    if (current_power_profile != PROFILE_BALANCED) {
        return HINT_HANDLED;
    }

    if (hint == POWER_HINT_INTERACTION) {
        int duration = 0;
        static unsigned long long previous_boost_time = 0;

        if (data) {
            duration = *((int *)data);
        }

        if (duration == 0) {
            duration = DEFAULT_INTERACTION_BOOST_MS;
        }

        struct timeval cur_boost_timeval = {0, 0};
        gettimeofday(&cur_boost_timeval, NULL);
        unsigned long long cur_boost_time = cur_boost_timeval.tv_sec * 1000000 + cur_boost_timeval.tv_usec;
        double elapsed_time = (double)(cur_boost_time - previous_boost_time);
        if (elapsed_time > 750000)
            elapsed_time = 750000;
        // don't hint if it's been less than 250ms since last boost
        // also detect if we're doing anything resembling a fling
        // support additional boosting in case of flings
        else if (elapsed_time < 250000 && duration <= 750)
            return HINT_HANDLED;

        previous_boost_time = cur_boost_time;

        if (duration > MAX_INTERACTION_BOOST_MS) {
            duration = MAX_INTERACTION_BOOST_MS;
        }

        if (duration >= 1500) {
            int resources[] = { SCHED_BOOST_ON, 0x20D, 0x101, 0x3E01 };
            interaction(duration, sizeof(resources)/sizeof(resources[0]), resources);
        } else {
            int resources[] = { 0x20D, 0x101, 0x3E01 };
            interaction(duration, sizeof(resources)/sizeof(resources[0]), resources);
        }
        return HINT_HANDLED;
    }

    if (hint == POWER_HINT_LAUNCH_BOOST) {
        int duration = LAUNCH_BOOST_DURATION_MS;
        int resources[] = { SCHED_BOOST_ON, 0x20F, 0x101, 0x1C00, 0x3E01, 0x4001, 0x4101, 0x4201 };

        interaction(duration, sizeof(resources)/sizeof(resources[0]), resources);

        return HINT_HANDLED;
    }

    if (hint == POWER_HINT_CPU_BOOST) {
        int duration = *(int32_t *)data / 1000;
        int resources[] = { SCHED_BOOST_ON, 0x20D, 0x3E01, 0x101 };

        if (duration > 0) {
            if (duration > MAX_CPU_BOOST_MS) {
                duration = MAX_CPU_BOOST_MS;
            }
            interaction(duration, sizeof(resources)/sizeof(resources[0]), resources);
        }

        return HINT_HANDLED;
    }

    if (hint == POWER_HINT_VIDEO_ENCODE) {
        return HINT_HANDLED;
    }

    if (hint == POWER_HINT_VIDEO_DECODE) {
        return HINT_HANDLED;
    }

    return HINT_NONE;
}
