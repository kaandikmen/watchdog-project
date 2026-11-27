/* SAHTE watchdog: yalnizca tests/test_scenarios.sh'i gercek watchdog
 * hazir olmadan denemek icin. Teslim edilen proje DEGILDIR. */
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"

#define NWORKERS     2
#define MAX_RESTARTS 3

static volatile sig_atomic_t got_chld, got_usr1, got_stop;
static worker_info_t w[NWORKERS];

static void h_chld(int s) { (void)s; got_chld = 1; }
static void h_usr1(int s) { (void)s; got_usr1 = 1; }
static void h_stop(int s) { (void)s; got_stop = 1; }

static void install(int sig, void (*h)(int))
{
    struct sigaction sa;
    sa.sa_handler = h;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, NULL);
}

static void nap(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static int spawn(int i)
{
    pid_t p = fork();
    if (p < 0) return -1;
    if (p == 0) {
        execl("./worker", "worker", (char *)NULL);
        _exit(127);
    }
    w[i].pid = p;
    w[i].state = STATE_RUNNING;
    logger_log("STARTED worker %d pid=%d", w[i].id, (int)p);
    return 0;
}

static void reap(int stopping)
{
    int st, i;
    pid_t p;

    while ((p = waitpid(-1, &st, WNOHANG)) > 0) {
        for (i = 0; i < NWORKERS && w[i].pid != p; i++) ;
        if (i == NWORKERS) continue;

        if (WIFEXITED(st) && WEXITSTATUS(st) == 0) {
            w[i].state = STATE_EXITED_NORMAL;
            logger_log("EXITED NORMALLY worker %d pid=%d code=0", w[i].id, (int)p);
            continue;                       /* normal cikis: restart yok */
        }
        if (WIFEXITED(st)) {
            w[i].state = STATE_EXITED_ERROR;
            logger_log("TERMINATED worker %d pid=%d error exit code=%d",
                       w[i].id, (int)p, WEXITSTATUS(st));
        } else {
            w[i].state = STATE_KILLED_BY_SIGNAL;
            logger_log("KILLED BY SIGNAL worker %d pid=%d signal=%d",
                       w[i].id, (int)p, WTERMSIG(st));
        }
        if (stopping) continue;
        if (w[i].restart_count < MAX_RESTARTS) {
            w[i].restart_count++;
            logger_log("RESTART worker %d attempt %d/%d",
                       w[i].id, w[i].restart_count, MAX_RESTARTS);
            spawn(i);
        } else {
            w[i].state = STATE_RESTART_LIMIT_REACHED;
            logger_log("RESTART LIMIT reached for worker %d, giving up", w[i].id);
        }
    }
}

int main(void)
{
    int i, waited;

    if (logger_init(LOG_FILE_DEFAULT) != 0) return 1;
    install(SIGCHLD, h_chld);
    install(SIGUSR1, h_usr1);
    install(SIGINT,  h_stop);
    install(SIGTERM, h_stop);

    logger_log("watchdog (MOCK) started pid=%d", (int)getpid());
    for (i = 0; i < NWORKERS; i++) {
        w[i].id = i + 1;
        spawn(i);
    }

    while (!got_stop) {
        if (got_chld) { got_chld = 0; reap(0); }
        if (got_usr1) { got_usr1 = 0; logger_report_status(w, NWORKERS); }
        nap(100);
    }

    logger_log("SHUTDOWN initiated, sending SIGTERM to workers");
    for (i = 0; i < NWORKERS; i++)
        if (w[i].state == STATE_RUNNING) kill(w[i].pid, SIGTERM);

    for (waited = 0; waited < 50; waited++) {   /* 5 sn timeout */
        reap(1);
        for (i = 0; i < NWORKERS && w[i].state != STATE_RUNNING; i++) ;
        if (i == NWORKERS) break;
        nap(100);
    }
    for (i = 0; i < NWORKERS; i++)
        if (w[i].state == STATE_RUNNING) {
            logger_log("worker %d did not stop, sending SIGKILL", w[i].id);
            kill(w[i].pid, SIGKILL);
        }
    nap(200);
    reap(1);

    logger_log("SHUTDOWN complete");
    logger_close();
    return 0;
}
