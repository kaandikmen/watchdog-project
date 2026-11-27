#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "signal_manager.h"
#include "process_manager.h"
#include "logger.h"

#define SHUTDOWN_TIMEOUT_MS 5000
#define POLL_INTERVAL_MS 100

static void graceful_shutdown(void)
{
    struct timespec ts = {0, POLL_INTERVAL_MS * 1000000L};
    int waited_ms = 0;

    logger_log("SHUTDOWN initiated, sending SIGTERM to workers");
    pm_stop_restarts();
    pm_signal_all(SIGTERM);

    while (pm_reap_nonblocking() > 0 && waited_ms < SHUTDOWN_TIMEOUT_MS)
    {
        nanosleep(&ts, NULL);
        waited_ms += POLL_INTERVAL_MS;
    }

    if (pm_reap_nonblocking() > 0)
    {
        logger_log("SHUTDOWN timeout, sending SIGKILL to remaining workers");
        pm_signal_all(SIGKILL);
        while (pm_reap_nonblocking() > 0)
            nanosleep(&ts, NULL);
    }

    logger_log("SHUTDOWN complete");
}

int main(void)
{
    sigset_t block_set, orig_set;

    if (logger_init("watchdog.log") == -1)
    {
        perror("logger_init");
        return EXIT_FAILURE;
    }
    if (install_signal_handlers() == -1)
    {
        perror("sigaction");
        logger_close();
        return EXIT_FAILURE;
    }

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGINT);
    sigaddset(&block_set, SIGTERM);
    sigaddset(&block_set, SIGUSR1);
    sigaddset(&block_set, SIGCHLD);
    sigaddset(&block_set, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &block_set, &orig_set) == -1)
    {
        perror("sigprocmask");
        logger_close();
        return EXIT_FAILURE;
    }

    logger_log("watchdog started pid=%d", (int)getpid());
    if (pm_start_all(&orig_set) == -1)
    {
        logger_log("failed to start worker processes");
        logger_close();
        return EXIT_FAILURE;
    }

    while (!g_shutdown_requested)
    {
        sigsuspend(&orig_set);

        /* Once cocuklari topla, sonra raporla: rapor guncel durumu gostersin. */
        pm_handle_events();

        if (g_status_requested)
        {
            int n;
            const worker_info_t *w;
            g_status_requested = 0;
            w = pm_get_workers(&n);
            logger_report_status(w, n);
        }
    }

    graceful_shutdown();
    logger_close();
    return EXIT_SUCCESS;
}
