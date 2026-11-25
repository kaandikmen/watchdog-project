#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "signal_manager.h"

volatile sig_atomic_t g_shutdown_requested = 0;
volatile sig_atomic_t g_status_requested = 0;

static void handle_shutdown(int signo)
{
    (void)signo;
    g_shutdown_requested = 1;
}

static void handle_status(int signo)
{
    (void)signo;
    g_status_requested = 1;
}

static int install(int signo, void (*handler)(int))
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    return sigaction(signo, &sa, NULL);
}

int install_signal_handlers(void)
{
    if (install(SIGINT, handle_shutdown) == -1)
        return -1;
    if (install(SIGTERM, handle_shutdown) == -1)
        return -1;
    if (install(SIGUSR1, handle_status) == -1)
        return -1;
    return 0;
}
