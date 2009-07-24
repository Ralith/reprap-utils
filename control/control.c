#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "../common/serial.h"
#include "../common/handlesigs.h"

#define _STR(x) #x
#define STR(x) _STR(x)

#define DEFAULT_SPEED 19200

#define HELP \
	"\t-s <speed>\tSerial line speed.  Defaults to " STR(DEFAULT_SPEED) ".\n"		\
	"\t-?\n" \
	"\t-h\t\tDisplay this help message.\n" \
	"\t-v\t\tVerbose: Prints serial I/O.\n" \
	"\t-l <[x]:[y]:[z]>\tLinear move: Executes a linear move to the coords specified.\n"
	

void usage(int argc, char** argv) {
	fprintf(stderr, "Usage: %s [-s <speed>] [-q] [-v] [-l] <serial device> [gcode file]\n", argv[0]);
}

int main(int argc, char** argv) 
{
	init_sig_handling();

	/* Get options */
	long speed = DEFAULT_SPEED;
	char *devpath;
	char **commands;
	unsigned int cmdcount = 0;
	int verbose = 0;
	{
		int opt;
		while ((opt = getopt(argc, argv, "h?vsl:")) >= 0) {
			switch(opt) {
			case 's':			/* Speed */
				speed = strtol(optarg, NULL, 10);
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

			case 'l':
				

			default:
				break;
			}
		}
		switch(argc - optind) {
		case 1:
			devpath = argv[optind];
			break;

		case 0:
			fprintf(stderr, "Too few arguments!\n");
			usage(argc, argv);
			exit(EXIT_FAILURE);

		default:
			fprintf(stderr, "You must supply a serial device path!\n");
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}

	int serial = serial_open(devpath, speed);
	if(serial < 0) {
		fprintf(stderr, "FATAL: %s\n", serial_strerror(serial));
		exit(EXIT_FAILURE);
	}

	serial_close(serial);
}
