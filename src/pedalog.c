/*
 * pedalog.c
 *
 * Copyright (c) 2011 Dan Haughey (http://www.powwow-pedal-power.org.uk)
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

#ifdef WIN32
#include <lusb0_usb.h>
#else
#include <usb.h>
#endif

#include "pedalog.h"

#define PEDALOG_VENDOR_ID   0x04d8
#define PEDALOG_PRODUCT_ID  0x000c

#define V1_RESPONSE_LENGTH  48
#define V2_RESPONSE_LENGTH  52

#define USB_TIMEOUT         1000

#define GET_SERIAL_COMMAND  0x01
#define MAX_SERIAL_LENGTH   4

#define READ_DATA_COMMAND   0x43

#define VOLTAGE_INDEX       1
#define VOLTAGE_LENGTH      4

#define CURRENT_INDEX       5
#define CURRENT_LENGTH      5

#define POWER_INDEX         10
#define POWER_LENGTH        5

#define ENERGY_INDEX        15
#define ENERGY_LENGTH       7

#define MAX_POWER_INDEX     22
#define MAX_POWER_LENGTH    5

#define AVG_POWER_INDEX     27
#define AVG_POWER_LENGTH    5

#define TIME_INDEX          32
#define TIME_LENGTH         8

#ifdef WIN32
#define WIN32_WORKAROUND_ATTEMPTS   8
#define WIN32_WORKAROUND_SLEEP      250
#endif

/* Structure to match a device's serial number to its usb_device structure to access it directly */
typedef struct {
    int                 serial;
    struct usb_device   *device;
} _pedalog_device_internal;

/* A lookup table to find usb_device structures from Pedalog serial numbers after calling pedalog_find_devices */
static _pedalog_device_internal     device_lookup[PEDALOG_MAX_DEVICES];

/* The number of Pedalog devices found in the last call to pedalog_find_devices */
static int                          device_count;

static int send_command(usb_dev_handle *handle, struct usb_device *pedalog, char cmd, char *response, int max_response)
{
#ifdef DEBUG
    printf("Entering send_command...\n");
#endif

    int r = usb_set_configuration(handle, pedalog->config[0].bConfigurationValue);
    if (r != 0) {
#ifdef DEBUG
        printf("usb_set_configuration returned %d, exiting send_command, returning PEDALOG_ERROR_FAILED_TO_OPEN\n", r);
#endif
        return PEDALOG_ERROR_FAILED_TO_OPEN;
    }
    
    int interface = pedalog->config[0].interface[0].altsetting->bInterfaceNumber;
    
    r = usb_claim_interface(handle, interface);
    if (r != 0) {
        switch (r) {
            case EBUSY:
#ifdef DEBUG
                printf("usb_claim_interface returned %d, exiting send_command, returning PEDALOG_ERROR_DEVICE_BUSY\n", r);
#endif
                return PEDALOG_ERROR_DEVICE_BUSY;
            case ENOMEM:
#ifdef DEBUG
                printf("usb_claim_interface returned %d, exiting send_command, returning PEDALOG_ERROR_OUT_OF_MEMORY\n", r);
#endif
                return PEDALOG_ERROR_OUT_OF_MEMORY;
            default:
#ifdef DEBUG
                printf("usb_claim_interface returned %d, exiting send_command, returning PEDALOG_ERROR_UNKNOWN\n", r);
#endif
                return PEDALOG_ERROR_UNKNOWN;
        }
    }

#ifdef DEBUG
    printf("  Calling usb_bulk_write with command '%x'...\n", cmd);
#endif

    r = usb_bulk_write(handle, 1, &cmd, 1, USB_TIMEOUT);
    if (r != 1) {
#ifdef DEBUG
        printf("  usb_bulk_write returned %d, expected 1, exiting send_command, returning PEDALOG_ERROR_UNKNOWN\n", r);
#endif
        return PEDALOG_ERROR_UNKNOWN;
    }

#ifdef DEBUG
    printf("  usb_bulk_write returned %d\n", r);
#endif

#ifdef DEBUG
    printf("  Calling usb_bulk_read, expecting at most %d bytes...\n", max_response);
#endif

    r = usb_bulk_read(handle, 0x81, response, max_response, USB_TIMEOUT);
    
#ifdef DEBUG
    printf("  usb_bulk_read returned %d\n", r);
    
    printf("  Calling usb_release_interface\n");
#endif

    usb_release_interface(handle, interface);
    
#ifdef DEBUG
    printf("Exiting send_command, returning %d\n", r);
#endif

    return r;
}

/* Reads the unique serial number from a Pedalog. */
static int read_device_serial(struct usb_device *device)
{
#ifdef DEBUG
    printf("Entering read_device_serial...\n");
#endif

#ifdef DEBUG
    printf("  Calling usb_open...\n");
#endif

    usb_dev_handle *handle = usb_open(device);

#ifdef DEBUG
    printf("  usb_open returned handle '%x'\n", (unsigned int)handle);
#endif

    if (handle == 0)
    {
#ifdef DEBUG
        printf("Exiting read_device_serial, returning PEDALOG_ERROR_NO_DEVICE_FOUND\n");
#endif
        return PEDALOG_ERROR_NO_DEVICE_FOUND;
    }

    // Add 1 to MAX_SERIAL_LENGTH to deal with the extra leading byte that is returned
    char response[MAX_SERIAL_LENGTH + 1];

#ifdef DEBUG
    printf("  Calling send_command with command '%d', expecting %d bytes response...\n", GET_SERIAL_COMMAND, MAX_SERIAL_LENGTH + 1);
#endif

    int r = send_command(handle, device, GET_SERIAL_COMMAND, response, MAX_SERIAL_LENGTH + 1);

#ifdef DEBUG
    printf("  send_command returned %d\n", r);

    printf("  Calling usb_close\n");
#endif
    
    usb_close(handle);

    if (r <= 0)
    {
        // No response was returned, or an error - assume this is a V1 Pedalog without a serial and
        // return 0
#ifdef DEBUG
    printf("Bad response given, assuming old firmware, exiting read_device_serial, returning 0\n");
#endif
        return 0;
    }

    char serial_string[MAX_SERIAL_LENGTH];
    strncpy(serial_string, response + 1, r - 1);

    int serial = atof(serial_string);

#ifdef DEBUG
    printf("Exiting read_device_serial, returning %d\n", serial);
#endif

    return serial;
}

/* Given a pedalog_device structure, finds the corresponding usb_device structure in the lookup table. */
static struct usb_device *lookup_usb_device(pedalog_device *device)
{
    int i;
    for (i = 0; i < device_count; ++i) {
        if (device_lookup[i].serial == device->serial) {
            return device_lookup[i].device;
        }
    }

    return NULL;
}

/* Initialises the Pedalog library. This must be called first, before any other functions. Returns PEDALOG_OK. */
int pedalog_init()
{
#ifdef DEBUG
    printf("Entering pedalog_init...\n");
#endif

    usb_init();

#ifdef DEBUG
    printf("Exiting pedalog_init, returning PEDALOG_OK\n");
#endif

    return PEDALOG_OK;
}

/* Returns the value of the PEDALOG_MAX_DEVICES constant. */
int pedalog_get_max_devices()
{
#ifdef DEBUG
    printf("Calling pedalog_get_max_devices, returning %d\n", PEDALOG_MAX_DEVICES);
#endif

    return PEDALOG_MAX_DEVICES;
}

/* Returns the value of the PEDALOG_MAX_ERROR_MESSAGE constant. */
int pedalog_get_max_error_message()
{
#ifdef DEBUG
    printf("Calling pedalog_get_max_error_message, returning %d\n", PEDALOG_MAX_ERROR_MESSAGE);
#endif

    return PEDALOG_MAX_ERROR_MESSAGE;
}

/* Finds all the Pedalog devices connected to the system. The devices array must be at least PEDALOG_MAX_DEVICES
 * in length. Returns the number of devices currently connected, or 0 if none were found. */
int pedalog_find_devices(pedalog_device *devices)
{
#ifdef DEBUG
    printf("Entering pedalog_find_devices...\n");
#endif

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
#ifdef DEBUG
                printf("  Found a Pedalog device...\n");
#endif

                // ask the device for its unique serial number
                int serial = read_device_serial(dev);

#ifdef DEBUG
                printf("    Serial is %d\n", serial);
#endif

                // add an entry to the lookup table so we can access the usb_device structure later
                device_lookup[device_count].serial = serial;
                device_lookup[device_count].device = dev;
                
                // populate a  pedalog_device struct to pass back to the caller
                devices[device_count].serial = serial;

                ++device_count;
                if (device_count >= PEDALOG_MAX_DEVICES) {
#ifdef DEBUG
                    printf("  Found PEDALOG_MAX_DEVICES (%d) so returning\n", PEDALOG_MAX_DEVICES);
#endif

                    break;
                }
            }
        }
    }

#ifdef DEBUG
    printf("Exiting pedalog_find_devices, returning %d\n", device_count);
#endif

    return device_count;
}

static int enumerate_devices()
{
    pedalog_device devices[PEDALOG_MAX_DEVICES];
    return pedalog_find_devices(devices);
}

/* Parses a string returned by the Pedalog device into a pedalog_data structure */
static int raw_string_to_pedalog_data(char *input, pedalog_data *data)
{
    char voltage[VOLTAGE_LENGTH];
    char current[CURRENT_LENGTH];
    char power[POWER_LENGTH];
    char energy[ENERGY_LENGTH];
    char max_power[MAX_POWER_LENGTH];
    char avg_power[AVG_POWER_LENGTH];
    char time[TIME_LENGTH];

    strncpy(voltage, input + VOLTAGE_INDEX, VOLTAGE_LENGTH);
    strncpy(current, input + CURRENT_INDEX, CURRENT_LENGTH);
    strncpy(power, input + POWER_INDEX, POWER_LENGTH);
    strncpy(energy, input + ENERGY_INDEX, ENERGY_LENGTH);
    strncpy(max_power, input + MAX_POWER_INDEX, MAX_POWER_LENGTH);
    strncpy(avg_power, input + AVG_POWER_INDEX, AVG_POWER_LENGTH);
    strncpy(time, input + TIME_INDEX, TIME_LENGTH);

    data->voltage = atof(voltage);
    data->current = atof(current);
    data->power = atof(power);
    data->energy = atof(energy);
    data->max_power = atof(max_power);
    data->avg_power = atof(avg_power);
    data->time = atol(time);

    return 1;
}

/* Does the actual work of reading data from the Pedalog device. Returns PEDALOG_OK if successful,
 * or an error value (as defined in pedalg.h) will be returned. */
static int read_data_internal(pedalog_data *data, usb_dev_handle *handle, struct usb_device *pedalog)
{
#ifdef DEBUG
    printf("Entering read_data_internal...\n");
#endif

    char result[V2_RESPONSE_LENGTH];

#ifdef DEBUG
    printf("  Calling send_command, expecting at least %d bytes response, at most %d bytes...\n", V1_RESPONSE_LENGTH, V2_RESPONSE_LENGTH);
#endif

    int r = send_command(handle, pedalog, READ_DATA_COMMAND, result, V2_RESPONSE_LENGTH);

#ifdef DEBUG
    printf("  send_command returned %d\n", r);
#endif
    
    if (r < 0)
    {
#ifdef DEBUG
        printf("Exiting read_data_internal, returning %d\n", r);
#endif
        return r;
    }

    if (r < V1_RESPONSE_LENGTH)
    {
#ifdef DEBUG
        printf("Response length (%d) was less than minimum length (%d), exiting read_data_internal, returning PEDALOG_ERROR_BAD_RESPONSE\n", r, V1_RESPONSE_LENGTH);
#endif
        return PEDALOG_ERROR_BAD_RESPONSE;
    }

    raw_string_to_pedalog_data(result, data);

#ifdef DEBUG
    printf("Exiting read_data_internal, returning PEDALOG_OK\n");
#endif

    return PEDALOG_OK;
}

static struct usb_device *reenumerate_and_lookup_device(pedalog_device *device, int repeat_call)
{
#ifdef DEBUG
    printf("Entering reenumerate_and_lookup_device...\n");

    printf("  Calling enumerate_devices\n");
#endif

    int device_count = enumerate_devices();
    if (device_count == 0)
    {
#ifdef DEBUG
        printf("0 devices enumerated, so exiting reenumerate_and_lookup_device, returning NULL");
#endif
        return NULL;
    }

#ifdef DEBUG
    printf("  Calling lookup_usb_device...\n");
#endif

    struct usb_device *pedalog;

    pedalog = lookup_usb_device(device);
    if (pedalog == NULL) {
#ifdef DEBUG
        printf("  lookup_usb_device returned NULL, exiting pedalog_read_data, returning NULL\n");
#endif
        return NULL;
    }
    
#ifdef WIN32
    // On Windows, the device will sometimes be found a while after it's been disconnected, so we do a
    // crappy workaround
    if (repeat_call == WIN32_WORKAROUND_ATTEMPTS)
    {
#ifdef DEBUG
        printf("Exiting reenumerate_and_lookup_device, returning found pedalog_device\n");
#endif
        return pedalog;
    }
    else
    {
#ifdef DEBUG
        printf("  Trying workaround for strange win32 behaviour, sleeping a while and reenumerating again...\n");
#endif
        Sleep(WIN32_WORKAROUND_SLEEP);
        return reenumerate_and_lookup_device(device, repeat_call + 1);
    }
#else
#ifdef DEBUG
        printf("Exiting reenumerate_and_lookup_device, returning found pedalog_device\n");
#endif
    return pedalog;
#endif
}


/* Reads the current data from a Pedalog device. pedalog_find_devices must have been called first, to obtain a
 * pedalog_device structure for the device that is to be read. If successful, the pedalog_data structure is populated with
 * data and PEDALOG_OK is returned. Otherwise, an error value (as defined in pedalog.h) will be returned. */
int pedalog_read_data(pedalog_device *device, pedalog_data *data)
{
#ifdef DEBUG
    printf("Entering pedalog_read_data...\n");

    printf("  Calling lookup_usb_device for serial '%d'...\n", device->serial);
#endif

    struct usb_device *pedalog = lookup_usb_device(device);
    if (pedalog == NULL) {
#ifdef DEBUG
        printf("  lookup_usb_device returned NULL, exiting pedalog_read_data, returning PEDALOG_ERROR_NO_DEVICE_FOUND\n");
#endif
        return PEDALOG_ERROR_NO_DEVICE_FOUND;
    }

    usb_dev_handle *handle;

#ifdef DEBUG
    printf("  Calling usb_open...\n");
#endif

    handle = usb_open(pedalog);
#ifdef DEBUG
    printf("  usb_open returned handle '%x'\n", (unsigned int)handle);
#endif

    if (handle == 0)
    {
#ifdef DEBUG
        printf("Exiting pedalog_read_data, returning PEDALOG_ERROR_NO_DEVICE_FOUND\n");
#endif
        return PEDALOG_ERROR_NO_DEVICE_FOUND;
    }
    
#ifdef DEBUG
    printf("  Calling read_data_internal...\n");
#endif

    int r = read_data_internal(data, handle, pedalog);

#ifdef DEBUG
    printf("  read_data_internal returned %d\n", r);

    printf("  Calling usb_close\n");
#endif
    
    usb_close(handle);

    if (r != PEDALOG_OK) {
#ifdef DEBUG
        printf("  r != PEDALOG_OK (%d) so calling enumerate_devices\n", PEDALOG_OK);
#endif

        // the device may have been disconnected, so re-enumerate devices to find out
        pedalog = reenumerate_and_lookup_device(device, 0);
        if (pedalog == NULL) {
#ifdef DEBUG
            printf("  lookup_usb_device returned NULL, exiting pedalog_read_data, returning PEDALOG_ERROR_NO_DEVICE_FOUND\n");
#endif
            return PEDALOG_ERROR_NO_DEVICE_FOUND;
        }
    }
   
#ifdef DEBUG
    printf("Exiting pedalog_read_data, returning %d\n", r);
#endif

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
