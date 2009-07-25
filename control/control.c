#include <stdio.h>
#include <stdlib.h>
#include <popt.h>

#include "../common/serial.h"
#include "../common/handlesigs.h"

#define _STR(x) #x
#define STR(x) _STR(x)

#define DEFAULT_SPEED 19200

typedef struct _gcode_cmd
{
	char *command;
	struct _gcode_cmd *next;
} gcode_cmd;

int main(int argc, const char** argv) 
{
	init_sig_handling();

	/* Get options */
	long speed = DEFAULT_SPEED;
	char *devpath = NULL;
	gcode_cmd head, *tail;
	int verbose = 0;
	{
		poptContext ctx;

		struct poptOption options_table[] = {
			{"linespeed", 's', POPT_ARG_LONG, &speed, 0,
			 "Serial linespeed (defaults to " STR(DEFAULT_SPEED) ".", "<speed>"},

			{"rapid", 'p', POPT_ARG_STRING, NULL, 0,
			 "Rapid positioning (G0).", "<[x]:[y]:[z]>"},
			{"linear", 'l', POPT_ARG_STRING, NULL, 0,
			 "Linear move (G1).", "<[x]:[y]:[z]>"},
			{"dwell", 'd', POPT_ARG_INT, NULL, 0,
			 "Dwell for <time> seconds (G4).", "<time>"},
			{"inches", 'i', POPT_ARG_NONE, NULL, 0,
			 "Set units to inches (G20).", NULL},
			{"milimeters", 'm', POPT_ARG_NONE, NULL, 0,
			 "Set units to milimeters (default) (G21).", NULL},
			{"absolute", 'a', POPT_ARG_NONE, NULL, 0,
			 "Use absolute coordinates (default) (G90).", NULL},
			{"relative", 'r', POPT_ARG_NONE, NULL, 0,
			 "Use relative/incremental coordinates (G91).", NULL},
			
			{"extrude", 'e', POPT_ARG_STRING, NULL, 0,
			 "Set extruder motor state (M101/M103).", "<on|off|reverse>"},
			{"temp", 't', POPT_ARG_INT, NULL, 0,
			 "Set extrusion temperature in Celsius (M104).", "<temperature>"},
			{"flowrate", 'f', POPT_ARG_INT, NULL, 0,
			 "Sets extrusion motor speed (M108).", "<speed>"},

			{"zero", 'z', POPT_ARG_STRING, NULL, 0,
			 "Zeroes the axes named in the argument.", "<[x][y][z]>"},
			POPT_AUTOHELP
			{NULL, 0, 0, NULL, 0}
		};

		ctx = poptGetContext(NULL, argc, argv, options_table, 0);
		poptSetOtherOptionHelp(ctx, "[OPTIONS]* <serial device>");

		if (argc < 2) {
			poptPrintUsage(ctx, stderr, 0);
			exit(EXIT_FAILURE);
		}
	}

	int serial = serial_open(devpath, speed);
	if(serial < 0) {
		fprintf(stderr, "Error opening serial device: %s\n", serial_strerror(serial));
		exit(EXIT_FAILURE);
	}

	serial_close(serial);
}
