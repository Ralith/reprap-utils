#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>

#include "../common/handlesigs.h"
#include "../common/asprintfx.h"

#define _STR(x) #x
#define STR(x) _STR(x)

#define DEFAULT_SPEED 19200

void usage(char *progname)
{
	fprintf(stderr, "%s [h?imarspldetfz]*\n", progname);
}

void help(char *progname) 
{
	usage(progname);
	printf("Each option represents one or more machine control or configuration messages.  Messages are output in the order that their corresponding options were provided.  Repeated options will result in repeated messages.\n");

	char *commands[][3] = {
		{"s", "speed", "Set movement speed."},
		{"p", "[x]:[y]:[z]", "Rapid positioning (G0)."},
		{"l", "[x]:[y]:[z]", "Linear move (G1)."},
		{"d", "time\t", "Dwell for <time> seconds (G4)."},
		{"i", NULL, "Set units to inches (G20)."},
		{"m", NULL, "Set units to milimeters (default) (G21)."},
		{"a", NULL, "Use absolute coordinates (default) (G90)."},
		{"r", NULL, "Use relative/incremental coordinates (G91)."},
		{"e", "on|reverse|off", "Set extruder motor state (M101/M102/M103)."},
		{"t", "temperature", "Set extrusion temperature in Celsius (M104)."},
		{"f", "speed", "Sets extrusion motor speed (M108)."},
		{"z", "[x][y][z]", "Zeroes the axes named in the argument."},
		{"h", NULL, "Displays this help message."},
		{NULL, NULL, NULL}
	};

	size_t i = 0;
	while(commands[i][0] != NULL) {
		fprintf(stderr, "\t-%s %s\t%s\n", commands[i][0], (commands[i][1] ? commands[i][1] : "\t"), commands[i][2]);
		i++;
	}
}

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

int main(int argc, char **argv) 
{
	init_sig_handling();

	gcode_cmd *head, *tail;
	/* Standard prelude, sets up absolute milimeter coordinates as a
	 * reliable default. */
	head = gcode("G21");
	tail = head;
	gcode_append(&tail, "G90");

	/* Handle options */
	{
		if (argc < 2) {
			help(argv[0]);
			exit(EXIT_FAILURE);
		}

		/* Process args */
		int opt;
		/* TODO: Generate the getopt string automatically */
		while((opt = getopt(argc, argv, "himars:p:l:d:e:t:f:z:")) >= 0) {
			switch(opt) {
			case 's':
				if(!isnum(optarg)) {
					fprintf(stderr, "Speed requires a numeric argument!\n");
					exit(EXIT_FAILURE);
				}
				gcode_append(&tail, asprintfx("G1 F%s", optarg));
				break;

			case 'p':
			{
				static char *coords;
				coords = decodeCoords(optarg);
				
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
				coords = decodeCoords(optarg);
				
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
				if(!isnum(optarg)) {
					fprintf(stderr, "Dwell requires a numeric argument!\n");
					exit(EXIT_FAILURE);
				}
				gcode_append(&tail, asprintfx("G4 P%s", optarg));
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
				if(strcasecmp(optarg, "on") == 0) {
					gcode_append(&tail, "M101");
				} else if(strcasecmp(optarg, "reverse")) {
					gcode_append(&tail, "M102");
				} else if(strcasecmp(optarg, "off")) {
					gcode_append(&tail, "M103");
				} else {
					fprintf(stderr, "Argument to extrude must be one of on, reverse, or off.\n");
					exit(EXIT_FAILURE);
				}
				break;

			case 't':
				if(!isnum(optarg)) {
					fprintf(stderr, "Extruder temperature requires a numeric argument!\n");
					exit(EXIT_FAILURE);
				}
				gcode_append(&tail, asprintfx("M104 S%s", optarg));
				break;

			case 'f':
			{
				if(!isnum(optarg)) {
					fprintf(stderr, "Extruder flowrate requires a numeric argument!\n");
					exit(EXIT_FAILURE);
				}
				gcode_append(&tail, asprintfx("M108 S%s", optarg));
				break;
			}

			case 'z':
			{
				static char dox, doy, doz;
				dox = 0;
				doy = 0;
				doz = 0;

				static size_t i;
				for(i = 0; i < strlen(optarg); i++) {
					if(optarg[i] == 'x' || optarg[i] == 'X') {
						dox = 1;
					}
					if(optarg[i] == 'y' || optarg[i] == 'Y') {
						doy = 1;
					}
					if(optarg[i] == 'z' || optarg[i] == 'Z') {
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

			case 'h':
				help(argv[0]);
				exit(EXIT_SUCCESS);
				
			case '?':
				help(argv[0]);
				exit(EXIT_FAILURE);
			
			default:
				break;
			}
		}
	}

	gcode_cmd *current = head;
	while(current != NULL) {
		printf("%s\r\n", current->command);
		current = current->next;
	}

	exit(EXIT_SUCCESS);
}
