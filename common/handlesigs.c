#include "handlesigs.h"

#ifndef WINDOWS
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

int sigstate;
struct sigaction default_sigint_handler;
struct sigaction default_sigterm_handler;
void sig_handler(int sig) {
	switch(sig) {
	case SIGINT:
		sigaction(SIGINT, &default_sigint_handler, NULL);
		sigstate = sig;
		break;
		
	case SIGTERM:
		sigaction(SIGINT, &default_sigterm_handler, NULL);
		sigstate = sig;
		break;

	default:
		break;
	}
}

// Exhibit standard behavior when exiting after a fatal signal
void sig_die() {
	switch(sigstate) {
	case SIGINT:
		kill(getpid(), SIGINT);
		break;

	case SIGTERM:
		kill(getpid(), SIGTERM);
		break;

	default:
		break;
	}
}
#endif

void init_sig_handling() {
	#ifndef WINDOWS
	sigstate = NO_SIGNAL;
	
	atexit(sig_die);
	
	struct sigaction sa;
	sa.sa_handler = &sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	
	sigaction(SIGINT, &sa, &default_sigint_handler);
	sigaction(SIGTERM, &sa, &default_sigterm_handler);
	#endif
}
