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
#define debug(msg) fprintf(stderr, "DEBUG: " msg "\n")
#define debugf(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", __VA_ARGS__)
#else
#define debug(msg)
#define debugf(fmt, ...)
#endif

#define DEFAULT_SPEED 19200
#define BUFFER_SIZE 128
#define TIMEOUT_MSECS (30 * 1000)
#define SHORT_TIMEOUT 10
#define CONFIRM_MSG "ok\r\n"

#define PROMPT "> "

#ifdef UNIX
#define DEVPATH "/dev"
#define DEVPREFIX "ttyUSB"
#define DEVPREFIX_LEN 6
#elif WINDOWS
#define DEVPREFIX "COM"
#define MAX_SERIAL_GUESSES 10
#endif

#define HELP \
	"" \
	"\t-s\tSerial line speed.  Defaults to " STR(DEFAULT_SPEED) ".\n"		\
	"\t-?\n" \
	"\t-h\tDisplay this help message.\n" \
	"\t-q\tQuiet/noninteractive mode; no output unless an error occurs.\n" \
	"\t-v\tVerbose: Prints serial I/O.\n" \
    "\t-f\tFile to dump.  If no gcode file is specified, or the file specified is -, gcode is read from the standard input.\n"


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
	fprintf(stderr, "Usage: %s [-s <speed>] [-q] [-v] [-f <gcode file>] [serial device]\n", argv[0]);
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
			if(strncmp(entry->d_name, DEVPREFIX, DEVPREFIX_LEN) == 0) {
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
FILE *input = NULL;
void cleanup() 
{
	if(serial) {
		serial_close(serial);
	}
	if(input != NULL && input != stdin) {
		fclose(input);
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
	int interactive = isatty(STDIN_FILENO);
	{
		int opt;
		while ((opt = getopt(argc, argv, "h?qvs:f:")) >= 0) {
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
				fprintf(stderr, "Unable to find any USB serial devices; please try again with the correct device manually specified.\n");
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
		input = stdin;
	} else {
		input = fopen(filepath, "r");
		if(input == NULL) {
			fprintf(stderr, "Unable to open gcode file \"%s\": %s\n", filepath, strerror(errno));
			exit(EXIT_FAILURE);
		}
		interactive = 0;
	}

	int timeout;
#ifdef UNIX
	struct pollfd fds[1];
	fds[0].fd = serial->handle;
	fds[0].events = POLLIN;
#endif

	char readbuf[BUFFER_SIZE];
	int ret = 0, msg_confirmed;
	int charsfound;				/* N chars of CONFIRM_MSG found. */
	ssize_t len;

	/* Be sure that the machine's ready */
	{
		if(noisy) {
			printf("Waiting for machine to come up...\n");
		}
		char ready = 0;
		char almost = 0;
		char charsfound = 0;
		/* Loop until we receive some data and no more is waiting. */
		do {
			/* Harmless (get current temp) */
			if(ret == 0) {
				serial_write(serial, "M105\n", 5);
			}
#ifdef UNIX
			ret = poll(fds, 1, SHORT_TIMEOUT);
#elif WINDOWS
			switch(WaitForMultipleObjects(1, &(serial->handle), FALSE, SHORT_TIMEOUT)) {
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
			checkSignal();
			if(ret < 0) {
				checkSignal();

				fprintf(stderr, "Error reading from serial: %s\n", strerror(errno));
				fprintf(stderr, "Giving up.\n");
				exit(EXIT_FAILURE);
			} else if(ret > 0) {
				len = serial_read(serial, readbuf, sizeof(readbuf)-1);
				/* Scan for confirmation message */
				int i;
				for(i = 0; i < len; i++) {
					if(readbuf[i] == CONFIRM_MSG[charsfound]) {
						charsfound++;
						if(charsfound >= strlen(CONFIRM_MSG)) {
							almost = 1;
							debug("Message receipt confirmed!");
						}
					} else {
						charsfound = 0;
					}
				}
			} else if(ret == 0) {
				if(almost) {
					ready = 1;
				}
			}
		} while(!ready);
		if(noisy) {
			printf("Ready!\n");
		}
	}


	if(verbose && interactive) {
		printf(PROMPT);
	}

	/* TODO: Use only poll so we can get input any time */
	while(fgets(readbuf, sizeof(readbuf), input)) {
		len = strlen(readbuf);
		if(verbose && !interactive) {
			printf("> %s", readbuf);
		}

		checkSignal();

		debug("Writing to serial...");
		serial_write(serial, readbuf, len);
		if(readbuf[len-1] == '\n') {
			debug("Wrote a complete line.");
			/* Wait for 'ok' reply */
			msg_confirmed = 0;
			charsfound = 0;
			timeout = TIMEOUT_MSECS;
			while(1) {
				debug("Polling serial...");
#ifdef UNIX
				ret = poll(fds, 1, timeout);
#elif WINDOWS
				switch(WaitForMultipleObjects(1, &(serial->handle), FALSE, timeout)) {
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
				debugf("done (returned %d).", ret);
				
				if(ret < 0) {
					checkSignal();

					fprintf(stderr, "Error reading from serial: %s\n", strerror(errno));
					fprintf(stderr, "Giving up.\n");
					exit(EXIT_FAILURE);
				} else if(ret == 0) {
					if(msg_confirmed) {
						/* We got what we wanted and no data remains. */
						break;
					} else {
						fprintf(stderr, "Timed out waiting for recept confirmation, giving up.\n");
						exit(EXIT_FAILURE);
					}
				}
				
				/* We've got data! */
				debug("Reading data from serial...");
				len = serial_read(serial, readbuf, sizeof(readbuf)-1);
				readbuf[len] = '\0';
				if(verbose) {
					printf("%s", readbuf);
				}
				/* Scan for confirmation message */
				int i;
				for(i = 0; i < len; i++) {
					if(readbuf[i] == CONFIRM_MSG[charsfound]) {
						charsfound++;
						if(charsfound >= strlen(CONFIRM_MSG)) {
							msg_confirmed = 1;
							debug("Message receipt confirmed!");
						}
					} else {
						charsfound = 0;
					}
				}
				if(msg_confirmed) {
					/* Disable timeout so we can quickly work through
					 * remaining data. */
					timeout = 0;
				}
			}
		}

		if(verbose && interactive) {
			printf(PROMPT);
		}
	}
	if(verbose && interactive) {
		printf("\n");
	}

	if(noisy) {
		printf("Successfully completed!\n");
	}

	exit(EXIT_SUCCESS);
}
