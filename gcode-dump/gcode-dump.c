#include "../common/serial.h"
#include "../common/handlesigs.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <poll.h>

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
#define CONFIRM_MSG "ok\r\n"
#define PROMPT "> "

#define HELP \
	"If no gcode file is specified, or the file specified is -, gcode is read from the standard input.\n" \
	"\t-s\tSerial line speed.  Defaults to " STR(DEFAULT_SPEED) ".\n"		\
	"\t-?\n" \
	"\t-h\tThis help message.\n" \
	"\t-q\tQuiet/noninteractive mode; no output unless an error occurs.\n" \
	"\t-v\tVerbose: Prints serial I/O.\n"
	

void usage(int argc, char** argv) {
	fprintf(stderr, "Usage: %s [-s <speed>] [-q] [-v] <serial device> [gcode file]\n", argv[0]);
}

/* Allows atexit to be used for guaranteed cleanup */
int serial = -1;
FILE* input = NULL;
void cleanup() 
{
	if(serial > 0) {
		close(serial);
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
		while ((opt = getopt(argc, argv, "h?qvs:")) >= 0) {
			switch(opt) {
			case 's':			/* Speed */
				speed = strtol(optarg, NULL, 10);
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
		case 2:
			devpath = argv[optind];
			filepath = argv[optind + 1];
			break;

		case 1:
			devpath = argv[optind];
			filepath = "-";
			break;

		case 0:
			fprintf(stderr, "Too few arguments!\n");
			usage(argc, argv);
			exit(EXIT_FAILURE);

		default:
			fprintf(stderr, "Too many arguments!\n");
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	if(noisy) {
		printf("Serial device:\t%s\n", devpath);
		printf("Line speed:\t%ld\n", speed);
		printf("Gcode file:\t%s\n", filepath);
	}

	/* Open FDs */
	serial = serial_open(devpath, speed);
	if(serial < 0) {
		fprintf(stderr, "FATAL: %s\n", serial_strerror(serial));
		exit(EXIT_FAILURE);
	}

	if(strncmp("-", filepath, 1) == 0) {
		if(noisy) {
			printf("Reading gcode from standard input");
			if(interactive) {
				printf(", enter Ctrl-D (EOF) to finish.");
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
	struct pollfd fds[1];
	fds[0].fd = serial;
	fds[0].events = POLLIN;

	char readbuf[BUFFER_SIZE];
	int ret, msg_confirmed;
	int charsfound;				/* N chars of CONFIRM_MSG found. */
	ssize_t len;

	if(verbose || interactive) {
		printf(PROMPT);
	}
	
	while(fgets(readbuf, sizeof(readbuf), input)) {
		len = strlen(readbuf);
		if(verbose && !interactive) {
			printf("%s", readbuf);
		}
		
		if(sigstate != NO_SIGNAL) {
			fprintf(stderr, "Caught a fatal signal, cleaning up.\n");
			exit(EXIT_FAILURE);
		}

		debug("Writing to serial...");
		write(serial, readbuf, len);
		if(readbuf[len-1] == '\n') {
			debug("Wrote a complete line.");
			/* Wait for 'ok' reply */
			ret = 1;
			msg_confirmed = 0;
			charsfound = 0;
			timeout = TIMEOUT_MSECS;
			while(1) {
				/* Add the serial FD to the set */
				debug("Polling serial...");
				ret = poll(fds, 1, timeout);
				debugf("done (returned %d).", ret);
				
				if(ret < 0) {
					if(sigstate != NO_SIGNAL) {
						fprintf(stderr, "Caught a fatal signal, cleaning up.\n");
						exit(EXIT_FAILURE);
					}

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
				len = read(serial, readbuf, sizeof(readbuf)-1);
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

		if(verbose || interactive) {
			printf(PROMPT);
		}
	}
	if(verbose || interactive) {
		printf("\n");
	}

	if(noisy) {
		printf("Successfully completed!\n");
	}

	exit(EXIT_SUCCESS);
}
