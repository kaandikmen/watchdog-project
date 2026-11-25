#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

#include <signal.h>

extern volatile sig_atomic_t g_shutdown_requested;
extern volatile sig_atomic_t g_status_requested;

int install_signal_handlers(void);

#endif
