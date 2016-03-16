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

//#include <cutils/log.h>
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

int LSensor_CALIDATA[2] = {0};
int PSensor_CALIDATA[2] = {0};

void readProximityCalibration() {
    int fd;

    FILE *calibration = fopen(PSENSOR_CALIBRATION, "r");

    if (calibration == NULL) {
        printf("Unable to open %s (%s)\n", PSENSOR_CALIBRATION, strerror(errno));
        return;
    }

    fscanf(calibration, "%*d %d %d", &PSensor_CALIDATA[0], &PSensor_CALIDATA[1]);

    fd = open(PROXIMITY_DEVICE, O_RDWR);

    if (fd < 0) {
        printf("Unable to open proximity device (%s)\n", strerror(errno));
        return;
    }

    ioctl(fd, ASUS_PSENSOR_SETCALI_DATA, PSensor_CALIDATA);
}

void readLightSensorCalibration() {
    int fd;

    FILE *calibration = fopen(LSENSOR_CALIBRATION, "r");

    if (calibration == NULL) {
        printf("Unable to open %s (%s)\n", LSENSOR_CALIBRATION, strerror(errno));
        return;
    }

    fscanf(calibration, "%d %d", &LSensor_CALIDATA[0], &LSensor_CALIDATA[1]);

    fd = open(LIGHTSENSOR_DEVICE, O_RDWR);

    if (fd < 0) {
        printf("Unable to open lightsensor device (%s)\n", strerror(errno));
        return;
    }

    ioctl(fd, ASUS_LIGHTSENSOR_SETCALI_DATA, LSensor_CALIDATA);
}

int main() {
    readProximityCalibration();
    readLightSensorCalibration();

    return 0;
}



