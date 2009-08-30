#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <getopt.h>

#ifdef UNIX
#include <poll.h>
#elif WINDOWS
#include <windows.h>
#endif

#include "../common/serial.h"
#include "../common/handlesigs.h"

#define _STR(x) #x
#define STR(x) _STR(x)

#ifdef DEBUG
#define debug(msg) printf("DEBUG: " msg "\n")
#define debugf(fmt, ...) printf("DEBUG: " fmt "\n", __VA_ARGS__)
#else
#define debug(msg)
#define debugf(fmt, ...)
#endif

#define DEFAULT_SPEED 19200
#define SERIAL_BUFSIZE 128
#define GCODE_BUFSIZE 512		/* Standard states 256 chars max */
#define SHORT_TIMEOUT 100
#define CONFIRM_MSG "ok\r\n"
#define DEFAULT_WRITEAHEAD 0	/* 0 is safer, but broken firmware cannot handle it. */

#ifdef UNIX
#define DEVPATH "/dev"
#define DEVPREFIX "ttyUSB"
#elif WINDOWS
#define DEVPREFIX "COM"
#define MAX_SERIAL_GUESSES 10
#endif

#define FD_COUNT 2
/* Must be sequential from 0 to FD_COUNT-1 */
#define FD_INPUT 0
#define FD_SERIAL 1

#define HELP \
	"" \
	"\t-s speed\tSerial line speed.  Defaults to " STR(DEFAULT_SPEED) ".\n"		\
	"\t-?\n" \
	"\t-h\t\tDisplay this help message.\n" \
	"\t-q\t\tQuiet/noninteractive mode; no output unless an error occurs.\n" \
	"\t-v\t\tVerbose: Prints serial I/O.\n" \
	"\t-c\t\tFilter out non-meaningful chars. May stress noncompliant gcode interpreters.\n" \
    "\t-f file\t\tFile to dump.  If no gcode file is specified, or the file specified is -, gcode is read from the standard input.\n"


void checkSignal() 
{
#ifdef UNIX
	if(sigstate != NO_SIGNAL) {
		fprintf(stderr, "Caught a fatal signal, cleaning up.\n");
		exit(EXIT_FAILURE);
	}
#endif
}

void usage(int argc, char** argv) {
	fprintf(stderr, "Usage: %s [-s <speed>] [-q] [-v] [-c] [-f <gcode file>] [serial device]\n", argv[0]);
}

char* guessSerial() 
{
#ifdef UNIX
	DIR *d = opendir(DEVPATH);
	char *dev = NULL;
	{
		struct dirent foo;
		dev = malloc(sizeof(foo.d_name));
	}

	char found = 0;
	if(d) {
		struct dirent *entry;
		while((entry = readdir(d))) {
			if(strncmp(entry->d_name, DEVPREFIX, strlen(DEVPREFIX)) == 0) {
				found = 1;
				strcpy(dev, entry->d_name);
			}
		}
	}
	
	closedir(d);

	if(found) {
		char *ret = malloc(sizeof(char)*(strlen(dev) + strlen(DEVPATH "/") + 1));
		strcpy(ret, DEVPATH);
		strcat(ret, "/");
		strcat(ret, dev);
		free(dev);

		return ret;
	}
	return NULL;
	
#elif WINDOWS
	char *devname = calloc(strlen(DEVPREFIX) + (MAX_SERIAL_GUESSES / 10), sizeof(char));
	char *num = calloc(MAX_SERIAL_GUESSES / 10, sizeof(char));
	int i;
	char exists = 0;
	for (i = 0; i < MAX_SERIAL_GUESSES; i++)
    {
		strcpy(devname, DEVPREFIX);
		itoa(i, num, 10);
		strcat(devname, num);
  
		HANDLE port = CreateFile(devname, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
		if (port == INVALID_HANDLE_VALUE) {
			/* DWORD error = GetLastError(); */

			/* Check to see if the error was because some other app
			 * had the port open */
			/* Ignore in-use ports. */
			/* if (error == ERROR_ACCESS_DENIED) {
				exists = 1;
				}*/
		} else {
			/* The port was opened successfully */
			exists = 1;

			/* Don't forget to close the port, since we are going to
			 * do nothing with it anyway */
			CloseHandle(port);
		}

		/* Add the port number to the array which will be returned */
		if(exists) {
			break;
		}
	}
	free(num);

	if(exists) {
		return devname;
	}
	free(devname);
	return NULL;
#endif
}


/* Allows atexit to be used for guaranteed cleanup */
serial_port *serial = NULL;
int input = STDIN_FILENO;
void cleanup() 
{
	if(serial) {
		serial_close(serial);
	}
	if(input != STDIN_FILENO) {
		close(input);
	}
}
int main(int argc, char** argv)
{
	init_sig_handling();
	atexit(cleanup);

	// Get arguments
	long speed = DEFAULT_SPEED;
	char *devpath = NULL;
	char *filepath = NULL;
	int noisy = 1;
	int verbose = 0;
	int compress = 0;
	int interactive = isatty(STDIN_FILENO);
	{
		int opt;
		while ((opt = getopt(argc, argv, "h?qvcs:f:")) >= 0) {
			switch(opt) {
			case 's':			/* Speed */
				speed = strtol(optarg, NULL, 10);
				break;

			case 'f':
				filepath = optarg;
				break;

			case 'q':			/* Quiet */
				noisy = 0;
				break;

			case 'v':			/* Verbose */
				verbose = 1;
				break;

			case 'c':
				compress = 1;
				break;

			case '?':			/* Help */
			case 'h':
				usage(argc, argv);
				fprintf(stderr, HELP);
				exit(EXIT_SUCCESS);
				break;

				

			default:
				break;
			}
		}
		switch(argc - optind) {

		case 1:
			devpath = argv[optind];
			break;

		case 0:
			if(noisy) {
				printf("Guessing a likely USB serial device...\n");
			}
			devpath = guessSerial();
			if(devpath == NULL) {
				fprintf(stderr, "Unable to autodetect any USB serial devices; if you are certain the device is available, please manually specify the path.\n");
				usage(argc, argv);
				exit(EXIT_FAILURE);
			}
			break;

		default:
			fprintf(stderr, "Too many arguments!\n");
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
		
		if(filepath == NULL) {
			filepath = "-";
		}
	}
	if(noisy) {
		printf("Serial device:\t%s\n", devpath);
		printf("Line speed:\t%ld\n", speed);
		printf("Gcode file:\t%s\n", filepath);
	}

	/* Open FDs */
	serial = serial_open(devpath, speed);
	if(serial == NULL) {
		fprintf(stderr, "Error opening serial device %s: %s\n", devpath, serial_strerror(serial_errno));
		exit(EXIT_FAILURE);
	}

	if(strncmp("-", filepath, 1) == 0) {
		if(noisy) {
			printf("Will read gcode from standard input");
			if(interactive) {
				printf("; enter Ctrl-D (EOF) to finish.");
			}
			printf("\n");
		}
		/* input defaults to stdin */
	} else {
		input = open(filepath, O_RDONLY);
		if(input < 0) {
			fprintf(stderr, "Unable to open gcode file \"%s\": %s\n", filepath, strerror(errno));
			exit(EXIT_FAILURE);
		}
		interactive = 0;
	}

#ifdef UNIX
	struct pollfd fds[FD_COUNT];
	fds[FD_INPUT].fd = input;
	fds[FD_INPUT].events = POLLIN;
	fds[FD_SERIAL].fd = serial->handle;
	fds[FD_SERIAL].events = POLLIN;
#endif

	char serialbuf[SERIAL_BUFSIZE];
	char gcodebuf[GCODE_BUFSIZE];
	int ret = 0;
	int charsfound = 0;				/* N chars of CONFIRM_MSG found. */
	size_t len;
	size_t gcpoint = 0;
	int gccomment = 0;
	unsigned unconfirmed = 0;
	while(1) {
		debug("Polling...");
#ifdef UNIX
		ret = poll(fds, FD_COUNT, -1);
#elif WINDOWS
#error TODO: Modify windows code to poll stdin as well as serial.
		switch(WaitForMultipleObjects(1, &(serial->handle), FALSE, -1)) {
		case WAIT_FAILED:
			/* TODO: Get error */
			ret = -1;
			break;

		case WAIT_TIMEOUT:
			ret = 0;
			break;

		case WAIT_OBJECT_0:
			ret = 1;
			break;

		default:
			break;
		}
				
#endif
				
		if(ret < 0) {
			checkSignal();
			fprintf(stderr, "Error during poll: %s\n", strerror(errno));
			fprintf(stderr, "Giving up.\n");
			exit(EXIT_FAILURE);
		}

		/* TODO: Windows */
		/* FIXME: Why are replies shifted forwards by one? */
		if(fds[FD_SERIAL].revents & POLLIN) {
			/* We've got reply data! */
			debug("Got serial.");
			len = serial_read(serial, serialbuf, sizeof(serialbuf)-1);
			
			if(verbose || interactive) {
				fwrite(serialbuf, sizeof(char), len, stdout);
				fflush(stdout);
			}
			
			/* Scan for confirmation message */
			int i;
			for(i = 0; i < len; i++) {
				if(serialbuf[i] == CONFIRM_MSG[charsfound]) {
					charsfound++;
					if(charsfound >= strlen(CONFIRM_MSG)) {
						if(unconfirmed > 0) { /* Sanity check */
							unconfirmed--;
						}
						/* Got confirmation, resume polling for and sending gcode. */
						fds[FD_INPUT].events = POLLIN;
						debug("Message receipt confirmed!");
						charsfound = 0;
					}
				} else {
					charsfound = 0;
				}
			}
		}

		/* TODO: Windows */
		if(fds[FD_INPUT].revents & POLLIN) {
			/* We've got input data! */
			int ret;
			char ch;
			ret = read(fds[FD_INPUT].fd, &ch, 1);
			
			if(ret == 0) {
				/* We're at EOF */
				if(noisy) {
					printf("Got EOF; dump complete.\n");
				}
				exit(EXIT_SUCCESS);
			} else if(ret < 0) {
				/* Something went wrong */
				checkSignal();
				fprintf(stderr, "Error reading gcode: %s\n", strerror(errno));
				fprintf(stderr, "Giving up.\n");
				exit(EXIT_FAILURE);
			}
				
			switch(ch) {
			case '\r':
			case '\n':
				{
					gccomment = 0;
					if(gcpoint == 0) {
						break;
					}

					serial_write(serial, gcodebuf, gcpoint);
					serial_write(serial, "\r\n", 2);

					debug("Sent complete block.");

					if(verbose && !interactive) {
						fwrite(gcodebuf, sizeof(char), gcpoint, stdout);
						printf("\n");
					}
					
					gcpoint = 0;

					/* Stop polling input until we have confirmation */
					fds[FD_INPUT].events = 0;

					break;
				}

			case ';':
				if(compress) {
					gccomment = 1;
				}
			case ' ':
			case '\t':
				if(compress) {
					break;
				}
			default:
				if(!gccomment) {
					gcodebuf[gcpoint++] = ch;
				}
				break;
			}
		}
	}

	if(noisy) {
		printf("Successfully completed!\n");
	}

	exit(EXIT_SUCCESS);
}
