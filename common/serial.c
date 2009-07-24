#include "serial.h"

#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>

// Convert between the numeric speed and the termios representation
// thereof.  Returns < 0 if argument is an unuspported speed.
speed_t ntocf(long l) {
	switch(l) {
	case 0:
		return B0;
	case 50:
		return B50;
	case 75:
		return B75;
	case 110:
		return B110;
	case 134:
		return B134;
	case 150:
		return B150;
	case 200:
		return B200;
	case 300:
		return B300;
	case 600:
		return B600;
	case 1200:
		return B1200;
	case 1800:
		return B1800;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	case 500000:
		return B500000;
	case 576000:
		return B576000;
	case 921600:
		return B921600;
	case 1000000:
		return B1000000;
	case 1152000:
		return B1152000;
	case 1500000:
		return B1500000;
	case 2000000:
		return B2000000;
	case 2500000:
		return B2500000;
	case 3000000:
		return B3000000;
	case 3500000:
		return B3500000;
	case 4000000:
		return B4000000;
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
	return 0;
}

int serial_init(int fd, long speed) {
	struct termios attribs;
	// Initialize attribs
	if(tcgetattr(fd, &attribs) < 0) {
		close(fd);
		return SERIAL_INVALID_FILEDESC;
	}

	// Set speed
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

	// Set non-canonical mode
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
	return 0;
}

/* Returns a prepared FD for the serial device specified, or some
 * value < 0 if an error occurred. */
int serial_open(char* path, long speed) 
{
	int fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
	int status;
	if((status = serial_init(fd, speed)) < 0) {
		/* An error occurred */
		return status;
	}
	return fd;
}


/* Thin wrapper of standard close for consistency. */
int serial_close(int fd) 
{
	return close(fd);
}

/* Returns a human-readable interpretation of a failing serial_open
 * return value */
const char* serial_strerror(int errno) 
{
	switch(errno) {
	case SERIAL_INVALID_SPEED:
		return "Unsupported serial linespeed.";

	case SERIAL_INVALID_FILEDESC:
		return "Invalid serial device.";

	case SERIAL_SETTING_FAILED:
		return "Unable to apply a necessary setting to the serial device.";

	default:
		return "Unknown error.  This is an internal bug.";
	}
}
