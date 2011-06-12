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
    int id;
} pedalog_device;

typedef struct {
    double  volt;
    double  iOut;
    double  pO;
    double  eO;
    double  pMax;
    double  pAve;
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
