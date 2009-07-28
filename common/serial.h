#ifndef _SERIAL_H_
#define _SERIAL_H_

#ifdef WINDOWS
#include <windows.h>
#endif

typedef enum serial_error {
	SERIAL_NO_ERROR = 0,
	SERIAL_INVALID_SPEED = 1,
	SERIAL_INVALID_FILEDESC = 2,
	SERIAL_SETTING_FAILED = 3,
	SERIAL_UNKNOWN_ERROR = 4
} serial_error;

typedef struct _serial_port 
{
#ifdef UNIX
	int handle;
#elif WINDOWS
	HANDLE handle;
#endif
} serial_port;

extern int serial_errno;

/* Opens serial port at path and speed if possible, or NULL on error. */
serial_port* serial_open(const char *path, long speed);

/* Thin wrapper of standard close for consistency. */
int serial_close(serial_port *port);

int serial_write(serial_port *port, const void *buf, size_t nbytes);
int serial_read(serial_port *port, void *buf, size_t nbytes);

/* Returns a human-readable interpretation of a failing serial_open
 * return value */
const char* serial_strerror(int errno);

#endif
