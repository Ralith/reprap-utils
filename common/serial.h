#ifndef _SERIAL_H_
#define _SERIAL_H_

#define SERIAL_INVALID_SPEED -1
#define SERIAL_INVALID_FILEDESC -2
#define SERIAL_SETTING_FAILED -3

/* Returns a prepared FD for the serial device specified, or some
 * value < 0 if an error occurred. */
int serial_open(char *path, long speed);

/* Thin wrapper of standard close for consistency. */
int serial_close(int fd);

/* Returns a human-readable interpretation of a failing serial_open
 * return value */
const char* serial_strerror(int errno);

#endif
