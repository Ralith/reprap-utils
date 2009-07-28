#ifndef _HANDLESIGS_H_
#define _HANDLESIGS_H_

#define NO_SIGNAL -1

#ifndef WINDOWS
extern int sigstate;
#endif

void init_sig_handling();

#endif
