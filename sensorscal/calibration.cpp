/*
 * Copyright (C) 2016 The CyanogenMod Project
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

#include <cutils/log.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define PROXIMITY_DEVICE                 "/dev/cm3602"
#define LIGHTSENSOR_DEVICE               "/dev/lightsensor"

#define PSENSOR_CALIBRATION              "/factory/PSensor_Calibration.ini"
#define LSENSOR_CALIBRATION              "/factory/LightSensor_Calibration.ini"

#define CAPELLA_CM3602_IOCTL_MAGIC       'c'
#define LIGHTSENSOR_IOCTL_MAGIC          'l'

#define ASUS_PSENSOR_SETCALI_DATA        _IOW(CAPELLA_CM3602_IOCTL_MAGIC, 0x15, int[2])
#define ASUS_LIGHTSENSOR_SETCALI_DATA    _IOW(LIGHTSENSOR_IOCTL_MAGIC, 0x15, int[2])

void readProximityCalibration() {
    int CALIDATA[2] = {0};
    int fd;

    FILE *calibration = fopen(PSENSOR_CALIBRATION, "r");

    if (calibration == NULL) {
        ALOGE("Unable to open %s (%s)\n", PSENSOR_CALIBRATION, strerror(errno));
        return;
    }

    fscanf(calibration, "%*d %d %d", &CALIDATA[0], &CALIDATA[1]);

    fd = open(PROXIMITY_DEVICE, O_RDWR);

    if (fd < 0) {
        ALOGE("Unable to open proximity device (%s)\n", strerror(errno));
        return;
    }

    ioctl(fd, ASUS_PSENSOR_SETCALI_DATA, CALIDATA);
}

void readLightSensorCalibration() {
    int CALIDATA[2] = {0};
    int fd;

    FILE *calibration = fopen(LSENSOR_CALIBRATION, "r");

    if (calibration == NULL) {
        ALOGE("Unable to open %s (%s)\n", LSENSOR_CALIBRATION, strerror(errno));
        return;
    }

    fscanf(calibration, "%d %d", &CALIDATA[0], &CALIDATA[1]);

    fd = open(LIGHTSENSOR_DEVICE, O_RDWR);

    if (fd < 0) {
        ALOGE("Unable to open lightsensor device (%s)\n", strerror(errno));
        return;
    }

    ioctl(fd, ASUS_LIGHTSENSOR_SETCALI_DATA, CALIDATA);
}

int main() {
    readProximityCalibration();
    readLightSensorCalibration();

    return 0;
}
