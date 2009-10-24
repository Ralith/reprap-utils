#include "serial.h"

#include <string.h>

#ifdef UNIX
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#endif

int serial_errno;

#ifdef UNIX
// Convert between the numeric speed and the termios representation
// thereof.  Returns < 0 if argument is an unuspported speed.
speed_t ntocf(long l) {
	switch(l) {
#ifdef B0
	case 0:
		return B0;
#endif
#ifdef B50
	case 50:
		return B50;
#endif
#ifdef B75
	case 75:
		return B75;
#endif
#ifdef B110
	case 110:
		return B110;
#endif
#ifdef B134
	case 134:
		return B134;
#endif
#ifdef B150
	case 150:
		return B150;
#endif
#ifdef B200
	case 200:
		return B200;
#endif
#ifdef B300
	case 300:
		return B300;
#endif
#ifdef B600
	case 600:
		return B600;
#endif
#ifdef B1200
	case 1200:
		return B1200;
#endif
#ifdef B1800
	case 1800:
		return B1800;
#endif
#ifdef B2400
	case 2400:
		return B2400;
#endif
#ifdef B4800
	case 4800:
		return B4800;
#endif
#ifdef B9600
	case 9600:
		return B9600;
#endif
#ifdef B19200
	case 19200:
		return B19200;
#endif
#ifdef B38400
	case 38400:
		return B38400;
#endif
#ifdef B57600
	case 57600:
		return B57600;
#endif
#ifdef B115200
	case 115200:
		return B115200;
#endif
#ifdef B230400
	case 230400:
		return B230400;
#endif
#ifdef B460800
	case 460800:
		return B460800;
#endif
#ifdef B500000
	case 500000:
		return B500000;
#endif
#ifdef B576000
	case 576000:
		return B576000;
#endif
#ifdef B921600
	case 921600:
		return B921600;
#endif
#ifdef B1000000
	case 1000000:
		return B1000000;
#endif
#ifdef B1152000
	case 1152000:
		return B1152000;
#endif
#ifdef B1500000
	case 1500000:
		return B1500000;
#endif
#ifdef B2000000
	case 2000000:
		return B2000000;
#endif
#ifdef B2500000
	case 2500000:
		return B2500000;
#endif
#ifdef B3000000
	case 3000000:
		return B3000000;
#endif
#ifdef B3500000
	case 3500000:
		return B3500000;
#endif
#ifdef B4000000
	case 4000000:
		return B4000000;
#endif
	default:
		return SERIAL_INVALID_SPEED;
	}
}

// Repeated many times to allow errors to be isolated to the specific
// setting that failed to apply.  Returns < 0 on failure.
int serial_set_attrib(int fd, struct termios* attribp) {
	if(tcsetattr(fd, TCSANOW, attribp) < 0) {
		return SERIAL_SETTING_FAILED;
	}
	return SERIAL_NO_ERROR;
}

int serial_init(int fd, long speed) {
	struct termios attribs;
	// Initialize attribs
	if(tcgetattr(fd, &attribs) < 0) {
		close(fd);
		return SERIAL_INVALID_FILEDESC;
	}

	/* Set speed */
	{
		speed_t cfspeed = ntocf(speed);
		if(cfsetispeed(&attribs, cfspeed) < 0) {
			return SERIAL_INVALID_SPEED;
		}
		serial_set_attrib(fd, &attribs);
		if(cfsetospeed(&attribs, cfspeed) < 0) {
			return SERIAL_INVALID_SPEED;
		}
		serial_set_attrib(fd, &attribs);
	}

	/* Set non-canonical mode */
	int status;
	attribs.c_cc[VTIME] = 0;
	if((status = serial_set_attrib(fd, &attribs)) < 0) {
		return status;
	}
	attribs.c_cc[VMIN] = 0;
	if((status = serial_set_attrib(fd, &attribs)) < 0) {
		return status;
	}
	cfmakeraw(&attribs);
	if((status = serial_set_attrib(fd, &attribs)) < 0) {
		return status;
	}

	/* Prevents DTR from being dropped, resetting the MCU when using
	 * an Arduino bootloader */
	attribs.c_cflag &= ~HUPCL;
	if((status = serial_set_attrib(fd, &attribs)) < 0) {
		return status;
	}

	return SERIAL_NO_ERROR;
}

#endif /* UNIX */



/* Returns a prepared FD for the serial device specified, or some
 * value < 0 if an error occurred. */
serial_port *serial_open(const char *path, long speed) 
{
	serial_errno = SERIAL_NO_ERROR;
	serial_port *port = malloc(sizeof(serial_port));
#ifdef UNIX
	port->handle = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
	if(port->handle < 0) {
		/* An error ocurred */
		serial_errno = SERIAL_INVALID_FILEDESC;
		return NULL;
	}
	int status;
	if((status = serial_init(port->handle, speed)) !=
	   SERIAL_NO_ERROR) {
		/* An error occurred */
		serial_errno = status;
		return NULL;
	}
#elif WINDOWS
	port->handle = CreateFile(path,
							 GENERIC_READ | GENERIC_WRITE,
							 0, NULL, OPEN_EXISTING, 0,
							 NULL);
	if(port->handle == INVALID_HANDLE_VALUE) {
		/* DWORD err = GetLastError(); */
		serial_errno = SERIAL_UNKNOWN_ERROR;
		return NULL;
	}

	/* Configure */
	DCB config;
	FillMemory(&config, sizeof(config), 0);
	if(!GetCommState(port->handle, &config)) {
		serial_errno = SERIAL_UNKNOWN_ERROR;
		return NULL;
	}

	/* This is mostly guessed. */
	config.BaudRate = speed;
	config.fBinary = TRUE;
	config.fParity = FALSE;
	config.Parity = NOPARITY;
	config.fInX = FALSE;
	config.fOutX = FALSE;
/*	config.fDtrControl = DTR_CONTROL_ENABLE;
	config.fDsrSensitivity = FALSE;
	config.fTXContinueOnXoff = TRUE;
	config.fErrorChar = FALSE;
	config.fNull = TRUE;
	config.fRtsControl = RTS_CONTROL_ENABLE;
	config.fAbortOnError = FALSE;*/

	if(!SetCommState(port->handle, &config)) {
		serial_errno = SERIAL_SETTING_FAILED;
		return NULL;
	}
#endif
	return port;
}


/* Thin wrapper of standard close for consistency. */
int serial_close(serial_port *port) 
{
#ifdef UNIX
	return close(port->handle);
#elif WINDOWS
	CloseHandle(port->handle);
	return 0;
#endif
}

int serial_write(serial_port *port, const void *buf, size_t nbytes)
{
#ifdef UNIX
	return write(port->handle, buf, nbytes);
#elif WINDOWS
	DWORD written = 0;
	WriteFile(port->handle, buf, nbytes, &written, NULL);
	return written;
#endif
}

int serial_read(serial_port *port, void *buf, size_t nbytes) 
{
#ifdef UNIX
	return read(port->handle, buf, nbytes);
#elif WINDOWS
	DWORD read = 0;
	ReadFile(port->handle, buf, nbytes, &read, NULL);
	return read;
#endif
}

/* Returns a human-readable interpretation of a failing serial_open
 * return value */
const char* serial_strerror(int errno)
{
	switch(errno) {
	case SERIAL_NO_ERROR:
		return "No error.";
		
	case SERIAL_INVALID_SPEED:
		return "Unsupported serial linespeed.";

	case SERIAL_INVALID_FILEDESC:
		return "Invalid serial device.";

	case SERIAL_SETTING_FAILED:
		return "Unable to apply a necessary setting to the serial device.";

	case SERIAL_UNKNOWN_ERROR:
		return "An unknown error occurred.";

	default:
		return "Unexpected error.  This is an internal bug.";
	}
}
