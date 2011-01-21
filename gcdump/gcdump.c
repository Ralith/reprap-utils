#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <reprap/comms.h>
#include <reprap/util.h>

#define STR(x) #x

#define DEFAULT_SPEED 19200
#define READBUF_SIZE 256
#define INPUT_BLOCK_TERMINATOR "\n"

#define HELP \
	"\t-?\n" \
	"\t-h\t\tDisplay this help message.\n" \
	"\t-q\t\tQuiet mode; no output unless an error occurs.\n" \
	"\t-v\t\tVerbose: Prints debug info and all serial I/O, instead of just received data.\n" \
	"\t-s speed\tSerial line speed.  Defaults to " STR(DEFAULT_SPEED) ".\n" \
  "\t-5\t\tUse 5D protocol (default is 3D)\n" \
	"\t-c\t\tFilter out non-meaningful chars. May stress noncompliant gcode interpreters.\n" \
	"\t-u number\tMaximum number of messages to send without receipt confirmation.  Unsafe, but necessary for certain broken firmware.\n" \
  "\t-f file\t\tFile to dump.  If no gcode file is specified, or the file specified is -, gcode is read from the standard input.\n"


void usage(char* name) {
	fprintf(stderr, "Usage: %s [-s <speed>] [-5] [-q] [-v] [-c] [-u <number>] [-f <gcode file>] [port]\n", name);
}

/* Allows atexit to be used for guaranteed cleanup */
rr_dev device = NULL;
int input = STDIN_FILENO;
void cleanup() {
	if(device) {
		rr_close(device);
    rr_free(device);
	}
	if(input != STDIN_FILENO) {
		close(input);
	}
}

void onsend(rr_dev dev, void *data, void *blockdata, const char *line, size_t len) {
  write(STDOUT_FILENO, line, len);
}

void onrecv(rr_dev dev, void *data, const char *reply, size_t len) {
  write(STDOUT_FILENO, reply, len);
}

void onreply(rr_dev dev, void *unconfirmed, rr_reply reply, float f) {
  if(reply == RR_OK) {
    if(*(unsigned*)unconfirmed == 0) {
      fprintf(stderr, "WARNING: Ignoring extra receipt confirmation!\n");
    } else {
      --*(unsigned*)unconfirmed;
    }
  }
}

void onerr(rr_dev dev, void *data, rr_error err, const char *source, size_t len) {
  switch(err) {
  case RR_E_UNCACHED_RESEND:
    fprintf(stderr, "Device requested we resend a line older than we cache!\n"
            "Aborting.\n");
    /* TODO: Halt extruder/heater on abort */
    exit(EXIT_FAILURE);
    break;

  case RR_E_HARDWARE_FAULT:
    fprintf(stderr, "HARDWARE FAULT!\n"
            "Aborting.\n");
    /* TODO: Halt extruder/heater on abort */
    exit(EXIT_FAILURE);
    break;

  case RR_E_UNKNOWN_REPLY:
    fprintf(stderr, "Warning:\t Recieved an unknown reply from the device.\n"
            "\t Your firmware is not supported or there is an error in libreprap.\n"
            "\t Please report this!\n");
    break;

  default:
    fprintf(stderr, "libreprap and gcdump are out of sync.  Please report this!\n");
    break;
  }
}

void update_buffered(rr_dev device, void *state, char value) {
  *(int*)state = value;
}

int main(int argc, char** argv)
{
	atexit(cleanup);

	// Get arguments
	long speed = DEFAULT_SPEED;
	char *devpath = NULL;
	char *filepath = NULL;
  rr_proto protocol = RR_PROTO_SIMPLE;
	int quiet = 1;
	int verbose = 0;
	int strip = 0;
	int interactive = isatty(STDIN_FILENO);
  int buffered = 0;
  unsigned unconfirmed = 0;
	unsigned max_unconfirmed = 1;
	{
		int opt;
		while ((opt = getopt(argc, argv, "h?5qvcs:u:f:")) >= 0) {
			switch(opt) {
			case 's':			/* Speed */
				speed = strtol(optarg, NULL, 10);
				break;

			case 'f':
				filepath = optarg;
				break;

			case 'u':
				max_unconfirmed = strtol(optarg, NULL, 10);
				break;

			case 'q':			/* Quiet */
				quiet = 1;
				break;

			case 'v':			/* Verbose */
				verbose = 1;
				break;

			case 'c':
				strip = 1;
				break;

			case '?':			/* Help */
			case 'h':
				usage(argv[0]);
				fprintf(stderr, HELP);
				exit(EXIT_SUCCESS);
				break;

      case '5':
        protocol = RR_PROTO_FIVED;
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
			devpath = rr_guess_port();
			if(devpath == NULL) {
				fprintf(stderr, "Unable to autodetect a serial port.  Please specify one explicitly.\n");
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;

		default:
			fprintf(stderr, "Too many arguments!\n");
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
		
		if(filepath == NULL) {
			filepath = "-";
		}
	}
	if(verbose) {
		printf("Serial port:\t%s\n", devpath);
		printf("Line speed:\t%ld\n", speed);
		printf("Gcode file:\t%s\n", filepath);
	}

  device = rr_create(protocol,
                     (verbose ? &onsend : NULL), NULL,
                     (quiet ? NULL : &onrecv), NULL,
                     &onreply, &unconfirmed,
                     &onerr, NULL,
                     &update_buffered, &buffered,
                     128);

	/* Connect to machine */
	if(rr_open(device, devpath, speed) < 0) {
		fprintf(stderr, "Error opening connection to machine on port %s: %s\n",
            devpath, strerror(errno));
		exit(EXIT_FAILURE);
	}

  /* Open input */
	if(strncmp("-", filepath, 1) == 0) {
		if(verbose) {
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
			fprintf(stderr, "Unable to open gcode file \"%s\": %s\n",
              filepath, strerror(errno));
			exit(EXIT_FAILURE);
		}
		interactive = 0;
	}

  /* Mainloop */
  fd_set readable, writable;
  int result;
  int devfd = rr_dev_fd(device);
  int highfd = devfd > input ? devfd : input;
  char readbuf[READBUF_SIZE];
  size_t bytesread = 0;
  while(1) {
    FD_ZERO(&readable);
    FD_ZERO(&writable);
    if(unconfirmed < max_unconfirmed) {
      /* Only look for input when we're confident the machine's
       * keeping up */
      FD_SET(input, &readable);
    }
    FD_SET(devfd, &readable);
    /* Only look for writability if there's data to be written */
    if(buffered) {
      FD_SET(devfd, &writable);
    }
    
    result = select(highfd + 1, &readable, &writable, NULL, NULL);
    if(result < 0) {
      /* Handle error */
      if(errno == EINTR) {
        /* select was interrupted before any of the selected events
         * occurred and before the timeout interval expired. */
        continue;
      } else {
        perror("Waiting on I/O failed");
        rr_flush(device);
        fprintf(stderr, "Buffers flushed\n");
        fprintf(stderr, "Aborting.\n");
        /* TODO: Halt extruder/heater on abort */
        exit(EXIT_FAILURE);
      }
    } else if(result > 0) {
      /* Perform I/O */
      if(FD_ISSET(devfd, &readable)) {
        result = rr_handle_readable(device);
        if(result < 0) {
          perror("Reading from device failed");
          fprintf(stderr, "Aborting.\n");
          /* TODO: Halt extruder/heater on abort */
          exit(EXIT_FAILURE);
        }
      }
      if(FD_ISSET(devfd, &writable)) {
        result = rr_handle_writable(device);
        if(result < 0) {
          perror("Writing to device failed");
          fprintf(stderr, "Aborting.\n");
          exit(EXIT_FAILURE);
        }
      }
      if(FD_ISSET(input, &readable)) {
        /* Read input */
        do {
          result = read(input, readbuf + bytesread, READBUF_SIZE - bytesread);
        } while(result < 0 && errno == EINTR);
        if(result < 0) {
          perror("Reading from input failed");
          result = rr_flush(device);
          if(result < 0) {
            perror("Flushing output buffers failed");
          } else {
            fprintf(stderr, "Output buffers flushed.\n");
          }
          fprintf(stderr, "Aborting.\n");
          /* TODO: Halt extruder/heater on abort */
          exit(EXIT_FAILURE);
        } else if(result == 0) {
          /* Got EOF */
          if(verbose) {
            printf("Got EOF!\n");
          }
          result = rr_flush(device);
          if(result < 0) {
            perror("Flushing output buffers failed");
          } else if(verbose) {
            printf("Output buffers flushed.\n");
          }
          break;
        }

        /* Scan for terminator */
        /* TODO: Don't over-enqueue when multiple blocks are read at once  */
        const size_t termlen = strlen(INPUT_BLOCK_TERMINATOR);
        size_t start = 0;
        bytesread += result;
        size_t scan = (bytesread > termlen) ? bytesread - termlen : 0;
        for(; scan < (bytesread - termlen); ++scan) {
          if(!strncmp(readbuf + scan, INPUT_BLOCK_TERMINATOR, termlen)) {
            /* Send off complete input block and continue scanning */
            rr_enqueue(device, RR_PRIO_NORMAL, NULL, readbuf + start, scan);
            ++unconfirmed;
            scan += termlen;
            start = scan;
          }
        }
        /* Move incomplete input block to beginning of buffer */
        memmove(readbuf, readbuf+start, bytesread - start);
      }
    }
  }

	exit(EXIT_SUCCESS);
}
