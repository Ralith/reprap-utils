#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <popt.h>

#include "../common/serial.h"
#include "../common/handlesigs.h"
#include "../common/asprintfx.h"

#define _STR(x) #x
#define STR(x) _STR(x)

#define DEFAULT_SPEED 19200

typedef struct gcode_cmd
{
	char *command;
	struct gcode_cmd *next;
} gcode_cmd;

gcode_cmd* gcode(char *cmd) 
{
	gcode_cmd *ret = malloc(sizeof(gcode_cmd));

	ret->command = cmd;
	ret->next = NULL;
	
	return ret;
}

void gcode_append(gcode_cmd **tail, char *new) 
{
	assert((*tail)->next == NULL);
	
	gcode_cmd *gnew = gcode(new);
	
	(*tail)->next = gnew;
	*tail = gnew;
}

char* decodeCoords(char *coord) 
{
	size_t clen = strlen(coord);
	char *explicit = calloc(clen+3, sizeof(char));
	size_t write, i;
	char last = '\0';
	int blanks = 0;
	for(i = 0, write = 0; i < strlen(coord); i++) {
		if((i == 0 && coord[i] == ':') ||
		   (last == ':' && coord[i] == ':')) {
			blanks++;
			if(blanks > 2) {
				/* Invalid arg or no coords set */
				return NULL;
			}
			explicit[write++] = '_';
		}
			
		explicit[write++] = coord[i];
		last = coord[i];
	}
	if(explicit[write-1] == ':') {
		explicit[write++] = '_';
	}
	explicit[write++] = '\0';

	char *ret = calloc(strlen(coord)+3, sizeof(char));
	char *tok = strtok(explicit, ":");
	i = 0;
	do {
		if(strcmp("_", tok) != 0) {
			strcat(ret, &("X\0Y\0Z\0"[i*2]));
			strcat(ret, tok);
			strcat(ret, " ");
		}
		i++;
	} while((tok = strtok(NULL, ":")) && i < 3);
	free(explicit);
	
	return ret;
}

int isnum(char *str) 
{
	size_t i;
	int points = 0;
	for(i = 0; i < strlen(str); i++) {
		if((str[i] > '9' || str[i] < '0')) {
			if(str[i] == '.') {
				/* Only one '.' allowed */
				if(points++ > 1) {
					return 0;
				}
			} else {
				return 0;
			}
		}	
	}
	return 1;
}

int main(int argc, const char **argv) 
{
	init_sig_handling();

	gcode_cmd *head, *tail;
	head = gcode("G21");
	tail = head;
	gcode_append(&tail, "G90");

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
			 "Linear move (G1).", "[x]:[y]:[z]"},
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
			 "Set extruder motor state (M101/M102/M103).", "on|reverse|off"},
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
		char *arg;
		while((rc = poptGetNextOpt(ctx)) > 0) {
			arg = poptGetOptArg(ctx);
			switch(rc) {
			case 's':
				if(!isnum(arg)) {
					fprintf(stderr, "Speed requires a numeric argument!\n");
					exit(EXIT_FAILURE);
				}
				gcode_append(&tail, asprintfx("G1 F%s", arg));
				break;

			case 'p':
			{
				static char *coords;
				coords = decodeCoords(arg);
				
				if(coords == NULL) {
					fprintf(stderr, "Invalid coordinate formatting.\n");
					exit(EXIT_FAILURE);
				}
				if(strcmp(coords, "") == 0) {
					fprintf(stderr, "Rapid movement requires at least one movement.\n");
					exit(EXIT_FAILURE);
				}

				gcode_append(&tail, asprintfx("G0 %s", coords));
				free(coords);
				break;
			}

			case 'l':
			{
				static char *coords;
				coords = decodeCoords(arg);
				
				if(coords == NULL) {
					fprintf(stderr, "Invalid coordinate formatting.\n");
					exit(EXIT_FAILURE);
				}
				if(strcmp(coords, "") == 0) {
					fprintf(stderr, "Linear movement requires at least one movement.\n");
					exit(EXIT_FAILURE);
				}
				
				gcode_append(&tail, asprintfx("G1 %s", coords));
				free(coords);
				break;
			}

			case 'd':
				if(!isnum(arg)) {
					fprintf(stderr, "Dwell requires a numeric argument!\n");
					exit(EXIT_FAILURE);
				}
				gcode_append(&tail, asprintfx("G4 P%s", arg));
				break;

			case 'i':
				gcode_append(&tail, "G20");
				break;

			case 'm':
				gcode_append(&tail, "G21");
				break;

			case 'a':
				gcode_append(&tail, "G90");
				break;

			case 'r':
				gcode_append(&tail, "G91");
				break;

			case 'e':
				if(strcasecmp(arg, "on") == 0) {
					gcode_append(&tail, "M101");
				} else if(strcasecmp(arg, "reverse")) {
					gcode_append(&tail, "M102");
				} else if(strcasecmp(arg, "off")) {
					gcode_append(&tail, "M103");
				} else {
					fprintf(stderr, "Argument to extrude must be one of on, reverse, or off.\n");
					exit(EXIT_FAILURE);
				}
				break;

			case 't':
				if(!isnum(arg)) {
					fprintf(stderr, "Extruder temperature requires a numeric argument!\n");
					exit(EXIT_FAILURE);
				}
				gcode_append(&tail, asprintfx("M104 S%s", arg));
				break;

			case 'f':
			{
				if(!isnum(arg)) {
					fprintf(stderr, "Extruder flowrate requires a numeric argument!\n");
					exit(EXIT_FAILURE);
				}
				gcode_append(&tail, asprintfx("M108 S%s", arg));
				break;
			}

			case 'z':
			{
				static char dox, doy, doz;
				dox = 0;
				doy = 0;
				doz = 0;

				static size_t i;
				for(i = 0; i < strlen(arg); i++) {
					if(arg[i] == 'x' || arg[i] == 'X') {
						dox = 1;
					}
					if(arg[i] == 'y' || arg[i] == 'Y') {
						doy = 1;
					}
					if(arg[i] == 'z' || arg[i] == 'Z') {
						doz = 1;
					}
				}
				
				if(!(dox || doy || doz)) {
					fprintf(stderr, "Must specify at least one of the x, y, or z axes to be zeroed!\n");
					exit(EXIT_FAILURE);
				}

				/* Move to minimum */
				gcode_append(&tail, asprintfx("G1 %s%s%s",
											  (dox ? "X-999 " : ""),
											  (doy ? "Y-999 " : ""),
											  (doz ? "Z-999" : "")));
				/* Set as zero */
				gcode_append(&tail, asprintfx("G92 %s%s%s",
											  (dox ? "X0 " : ""),
											  (doy ? "Y0 " : ""),
											  (doz ? "Z0" : "")));
				break;
			}
				
				
			default:
				break;
			}

			if(arg) {
				free(arg);
			}
		}
		/* Anything remaining is the dev path. */
		devpath = poptGetArg(ctx);

		if((devpath == NULL) || (poptPeekArg(ctx) != NULL)) {
			poptPrintUsage(ctx, stderr, 0);
			exit(EXIT_FAILURE);
		}
	}

	{
		gcode_cmd *i;
		for(i = head; i != NULL; i = i->next) {
			printf("GCODE: %s\n", i->command);
		}
	}

	int serial = serial_open(devpath, speed);
	if(serial < 0) {
		fprintf(stderr, "Error opening serial device: %s\n", serial_strerror(serial));
		exit(EXIT_FAILURE);
	}

	serial_close(serial);
}
