/*
 * pedalog.c
 *
 * Copyright (c) 2011 Dan Haughey
 *
 * This file is part of libpedalog.
 *
 * libpedalog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libpedalog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libpedalog.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <usb.h>

#include "pedalog.h"

#define PEDALOG_VENDOR_ID   0x04d8
#define PEDALOG_PRODUCT_ID  0x000c

#define RESPONSE_LENGTH     48
#define USB_TIMEOUT         1000

#define READ_DATA_COMMAND   0x43

#define VOLT_INDEX          1
#define VOLT_LENGTH         4

#define I_OUT_INDEX         5
#define I_OUT_LENGTH        5

#define P_O_INDEX           10
#define P_O_LENGTH          5

#define E_O_INDEX           15
#define E_O_LENGTH          7

#define P_MAX_INDEX         22
#define P_MAX_LENGTH        5

#define P_AVE_INDEX         27
#define P_AVE_LENGTH        5

#define TIME_INDEX          32
#define TIME_LENGTH         8

/* Structure to match a device's unique ID to its usb_device structure to access it directly */
typedef struct {
    int         id;
    struct usb_device   *device;
} _pedalog_device_internal;

/* A lookup table to find usb_device structures from unique Pedalog IDs after calling pedalog_find_devices */
static _pedalog_device_internal     device_lookup[PEDALOG_MAX_DEVICES];

/* The number of Pedalog devices found in the last call to pedalog_find_devices */
static int                          device_count;

/* Reads the unique device ID from a Pedalog. */
static int read_device_id(struct usb_device *device)
{
    // TODO: implement this once firmware supports it
    return 0;
}

/* Given a pedalog_device structure, finds the corresponding usb_device structure in the lookup table. */
static struct usb_device *lookup_usb_device(pedalog_device *device)
{
    int i;
    for (i = 0; i < PEDALOG_MAX_DEVICES; ++i) {
        if (device_lookup[i].id == device->id) {
            return device_lookup[i].device;
        }
    }

    return NULL;
}

/* Initialises the Pedalog library. This must be called first, before any other functions. Returns PEDALOG_OK. */
int pedalog_init()
{
    usb_init();

    return PEDALOG_OK;
}

/* Returns the value of the PEDALOG_MAX_DEVICES constant. */
int pedalog_get_max_devices()
{
    return PEDALOG_MAX_DEVICES;
}

/* Returns the value of the PEDALOG_MAX_ERROR_MESSAGE constant. */
int pedalog_get_max_error_message()
{
    return PEDALOG_MAX_ERROR_MESSAGE;
}

/* Finds all the Pedalog devices connected to the system. The devices array must be at least PEDALOG_MAX_DEVICES
 * in length. Returns the number of devices currently connected, or 0 if none were found. */
int pedalog_find_devices(pedalog_device *devices)
{
    struct usb_bus *busses;
    struct usb_bus *bus;

    usb_find_busses();
    usb_find_devices();

    busses = usb_get_busses();

    device_count = 0;
    for (bus = busses; bus; bus = bus->next) {
        struct usb_device *dev;

        for (dev = bus->devices; dev; dev = dev->next) {
            if (dev->descriptor.idVendor == PEDALOG_VENDOR_ID && dev->descriptor.idProduct == PEDALOG_PRODUCT_ID) {
                // ask the device for its unique ID
                int id = read_device_id(dev);

                // add an entry to the lookup table so we can access the usb_device structure later
                device_lookup[device_count].id = id;
                device_lookup[device_count].device = dev;
                
                // populate a  pedalog_device struct to pass back to the caller
                devices[device_count].id = id;

                ++device_count;
                if (device_count >= PEDALOG_MAX_DEVICES) {
                    break;
                }
            }
        }
    }

    return device_count;
}

/* Parses a string returned by the Pedalog device into a pedalog_data structure */
static int raw_string_to_pedalog_data(char *input, pedalog_data *data)
{
    char volt[VOLT_LENGTH];
    char i_out[I_OUT_LENGTH];
    char p_o[P_O_LENGTH];
    char e_o[E_O_LENGTH];
    char p_max[P_MAX_LENGTH];
    char p_ave[P_AVE_LENGTH];
    char time[TIME_LENGTH];

    strncpy(volt, input + VOLT_INDEX, VOLT_LENGTH);
    strncpy(i_out, input + I_OUT_INDEX, I_OUT_LENGTH);
    strncpy(p_o, input + P_O_INDEX, P_O_LENGTH);
    strncpy(e_o, input + E_O_INDEX, E_O_LENGTH);
    strncpy(p_max, input + P_MAX_INDEX, P_MAX_LENGTH);
    strncpy(p_ave, input + P_AVE_INDEX, P_AVE_LENGTH);
    strncpy(time, input + TIME_INDEX, TIME_LENGTH);

    data->volt = atof(volt);
    data->iOut = atof(i_out);
    data->pO = atof(p_o);
    data->eO = atof(e_o);
    data->pMax = atof(p_max);
    data->pAve = atof(p_ave);
    data->time = atol(time);

    return 1;
}

/* Does the actual work of reading data from the Pedalog device. Returns PEDALOG_OK if successful,
 * or an error value (as defined in pedalg.h) will be returned. */
static int read_data_internal(pedalog_data *data, usb_dev_handle *handle, struct usb_device *pedalog)
{
    int r = usb_set_configuration(handle, pedalog->config[0].bConfigurationValue);
    if (r != 0) {
        return PEDALOG_ERROR_FAILED_TO_OPEN;
    }
    
    int interface = pedalog->config[0].interface[0].altsetting->bInterfaceNumber;
    
    r = usb_claim_interface(handle, interface);
    if (r != 0) {
        switch (r) {
            case EBUSY:     return PEDALOG_ERROR_DEVICE_BUSY;
            case ENOMEM:    return PEDALOG_ERROR_OUT_OF_MEMORY;
            default:        return PEDALOG_ERROR_UNKNOWN;
        }
    }

    char cmd = READ_DATA_COMMAND;

    r = usb_bulk_write(handle, 1, &cmd, 1, USB_TIMEOUT);

    char result[RESPONSE_LENGTH];
    r = usb_bulk_read(handle, 0x81, result, RESPONSE_LENGTH, USB_TIMEOUT);
    
    usb_release_interface(handle, interface);
    
    if (r != RESPONSE_LENGTH)
    {
        return PEDALOG_ERROR_BAD_RESPONSE;
    }

    raw_string_to_pedalog_data(result, data);

    return PEDALOG_OK;
}

/* Reads the current data from a Pedalog device. pedalog_find_devices must have been called first, to obtain a
 * pedalog_device structure for the device that is to be read. If successful, the pedalog_data structure is populated with
 * data and PEDALOG_OK is returned. Otherwise, an error value (as defined in pedalog.h) will be returned. */
int pedalog_read_data(pedalog_device *device, pedalog_data *data)
{
    struct usb_device *pedalog = lookup_usb_device(device);
    if (pedalog == NULL) {
        return PEDALOG_ERROR_NO_DEVICE_FOUND;
    }

    usb_dev_handle *handle;
    handle = usb_open(pedalog);
    
    int r = read_data_internal(data, handle, pedalog);
    
    usb_close(handle);
    
    return r;
}

/* Returns a human-readable string describing an error code. The message parameter must be a pointer to a char array
 * with at least PEDALOG_MAX_ERROR_MESSAGE elements. */
void pedalog_get_error_message(int error, char *message)
{
    char *message_string;

    switch (error) {
        case PEDALOG_OK:
            message_string = "";
            break;
        case PEDALOG_ERROR_NO_DEVICE_FOUND:
            message_string = "The Pedalog device was not found. It may have been disconnected.";
            break;
        case PEDALOG_ERROR_FAILED_TO_OPEN:
            message_string = "The device could not be opened for communication. You might not have permission to access it, try running as root.";
            break;
        case PEDALOG_ERROR_BAD_RESPONSE:
            message_string = "A bad response was received from the device. It may have an incompatible firmware version.";
            break;
        case PEDALOG_ERROR_DEVICE_BUSY:
            message_string = "The device is busy. It may be in use by another application.";
            break;
        case PEDALOG_ERROR_OUT_OF_MEMORY:
            message_string = "An out of memory error occurred when trying to communicate with the device.";
            break;
        case PEDALOG_ERROR_UNKNOWN:
        default:
            message_string = "An unknown error occurred.";
            break;
    }

    strncpy(message, message_string, PEDALOG_MAX_ERROR_MESSAGE);
}
