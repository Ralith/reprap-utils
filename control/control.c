#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <popt.h>

#include "../common/serial.h"
#include "../common/handlesigs.h"

#define _STR(x) #x
#define STR(x) _STR(x)

#define DEFAULT_SPEED 19200

typedef struct gcode_cmd
{
	char *command;
	struct gcode_cmd *next;
} gcode_cmd;

gcode_cmd* gcode(char *cmd, int argc, char **argv) 
{
	gcode_cmd *ret = malloc(sizeof(gcode_cmd));
	size_t len = 0;
	
	len += strlen(cmd);

	int i;
	for(i = 0; i < argc; i++) {
		len += strlen(argv[i]);
	}

	ret->command = calloc(len+1, sizeof(char));
	strcpy(ret->command, cmd);

	for(i = 0; i < argc; i++) {
		strcat(ret->command, argv[i]);
	}
	ret->next = NULL;
	
	return ret;
}

void gcode_append(gcode_cmd **tail, gcode_cmd *new) 
{
	assert((*tail)->next == NULL);
	(*tail)->next = new;
	*tail = new;
}

int main(int argc, const char **argv) 
{
	init_sig_handling();

	gcode_cmd *head, *tail;
	head = gcode("G21", 0, NULL);
	tail = head;

	/* Get options */
	long speed = DEFAULT_SPEED;
	char *devpath = NULL;
	int verbose = 0;
	{
		poptContext ctx;

		struct poptOption options_table[] = {
			{"linespeed", 'l', POPT_ARG_LONG, &speed, 0,
			 "Serial linespeed (defaults to " STR(DEFAULT_SPEED) ".", "<speed>"},

			{"speed", 's', POPT_ARG_INT, NULL, 's',
			 "Set movement speed.", "speed"},
			{"rapid", 'p', POPT_ARG_STRING, NULL, 'p',
			 "Rapid positioning (G0).", "[x]:[y]:[z]"},
			{"linear", 'l', POPT_ARG_STRING, NULL, 'l',
			 "Linear move (G1).", "<[x]:[y]:[z]>"},
			{"dwell", 'd', POPT_ARG_INT, NULL, 'd',
			 "Dwell for <time> seconds (G4).", "time"},
			{"inches", 'i', POPT_ARG_NONE, NULL, 'i',
			 "Set units to inches (G20).", NULL},
			{"milimeters", 'm', POPT_ARG_NONE, NULL, 'm',
			 "Set units to milimeters (default) (G21).", NULL},
			{"absolute", 'a', POPT_ARG_NONE, NULL, 'a',
			 "Use absolute coordinates (default) (G90).", NULL},
			{"relative", 'r', POPT_ARG_NONE, NULL, 'r',
			 "Use relative/incremental coordinates (G91).", NULL},
			
			{"extrude", 'e', POPT_ARG_STRING, NULL, 'e',
			 "Set extruder motor state (M101/M103).", "on|off|reverse"},
			{"temp", 't', POPT_ARG_INT, NULL, 't',
			 "Set extrusion temperature in Celsius (M104).", "temperature"},
			{"flowrate", 'f', POPT_ARG_INT, NULL, 'f',
			 "Sets extrusion motor speed (M108).", "speed"},

			{"zero", 'z', POPT_ARG_STRING, NULL, 'z',
			 "Zeroes the axes named in the argument.", "[x][y][z]"},
			POPT_AUTOHELP
			{NULL, 0, 0, NULL, 0}
		};

		ctx = poptGetContext(NULL, argc, argv, options_table, 0);
		poptSetOtherOptionHelp(ctx, "<serial device>");

		if (argc < 2) {
			poptPrintUsage(ctx, stderr, 0);
			exit(EXIT_FAILURE);
		}

		/* Process args */
		int rc;
		while((rc = poptGetNextOpt(ctx)) > 0) {
			switch(rc) {
			default:
				break;
			}
		}
		/* Anything remaining is the dev path. */
		devpath = poptGetArg(ctx);

		if((devpath == NULL) || (poptPeekArg(ctx) != NULL)) {
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
