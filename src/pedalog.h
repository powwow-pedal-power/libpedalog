/*
   pedalog.h

   Copyright (c) 2011 Dan Haughey (http://www.powwow-pedal-power.org.uk)

   This file is part of libpedalog.

   libpedalog is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   libpedalog is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with libpedalog.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _PEDALOG_H_
#define _PEDALOG_H_

/* return codes */

#define PEDALOG_OK                      0
#define PEDALOG_ERROR_UNKNOWN           1
#define PEDALOG_ERROR_NO_DEVICE_FOUND   2
#define PEDALOG_ERROR_FAILED_TO_OPEN    3
#define PEDALOG_ERROR_BAD_RESPONSE      4
#define PEDALOG_ERROR_DEVICE_BUSY       5
#define PEDALOG_ERROR_OUT_OF_MEMORY     6

/* constants */
#define PEDALOG_MAX_DEVICES             8
#define PEDALOG_MAX_ERROR_MESSAGE       128

typedef struct {
    int serial;
} pedalog_device;

typedef struct {
    double  voltage;
    double  current;
    double  power;
    double  energy;
    double  max_power;
    double  avg_power;
    long    time;
} pedalog_data;

#ifdef __cplusplus
extern "C" {
#endif

int pedalog_init();
int pedalog_get_max_devices();
int pedalog_get_max_error_message();
int pedalog_find_devices(pedalog_device *devices);
int pedalog_read_data(pedalog_device *device, pedalog_data *data);
void pedalog_get_error_message(int error, char *message);

#ifdef __cplusplus
}
#endif

#endif /* _PEDALOG_H_ */
