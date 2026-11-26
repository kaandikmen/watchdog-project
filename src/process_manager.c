/*
 * process_manager.c - Student 2: Child Process Management
 *   - fork() + execv() ile worker baslatma
 *   - SIGCHLD + waitpid() ile sonlanan cocuklari toplama
 *   - normal / beklenmedik sonlanma ayrimi
 *   - en fazla MAX_RESTARTS kez yeniden baslatma
 *   - setitimer() + SIGALRM ile periyodik saglik kontrolu
 *
 * Handler'lar sadece volatile sig_atomic_t bayrak set eder (async-signal-safe);
 * asil is pm_handle_events() icinde, ana akista yapilir.
 */
#define _POSIX_C_SOURCE 200809L
#include "process_manager.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Her worker'in calisma parametreleri: omur (sn, 0 = sonsuz) ve cikis kodu.
 * workers/worker.c bunlari argv[1], argv[2] olarak alir. */
typedef struct {
    const char *life;
    const char *code;
} worker_cfg_t;

static const worker_cfg_t cfg[NUM_WORKERS] = {
    { "0", "0" },   /* worker 0: surekli calisir                          */
    { "0", "0" },   /* worker 1: surekli calisir                          */
    { "3", "0" },   /* worker 2: 3 sn sonra exit(0) -> normal, restart yok */
    { "1", "1" },   /* worker 3: 1 sn sonra exit(1) -> 3 restart, sonra limit */
};

static worker_info_t workers[NUM_WORKERS];
static sigset_t      child_mask;            /* exec oncesi geri yuklenecek maske */
static int           restarts_enabled = 1;

static volatile sig_atomic_t g_child_exited = 0;
static volatile sig_atomic_t g_alarm        = 0;

static void on_sigchld(int signo) { (void)signo; g_child_exited = 1; }
static void on_sigalrm(int signo) { (void)signo; g_alarm = 1; }

static int install_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_sigchld;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;  /* sadece sonlanmada haber ver */
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        return -1;

    sa.sa_handler = on_sigalrm;
    sa.sa_flags   = SA_RESTART;
    return sigaction(SIGALRM, &sa, NULL);
}

static void set_timer(int sec)
{
    struct itimerval it;
    memset(&it, 0, sizeof it);
    it.it_interval.tv_sec = sec;
    it.it_value.tv_sec    = sec;
    if (setitimer(ITIMER_REAL, &it, NULL) == -1)
        logger_log("setitimer failed: %s", strerror(errno));
}

static int spawn(worker_info_t *w)
{
    pid_t pid = fork();

    if (pid == -1) {
        logger_log("fork failed for worker %d: %s", w->id, strerror(errno));
        return -1;
    }

    if (pid == 0) {
        char *const argv[] = { WORKER_PATH, (char *)cfg[w->id].life,
                               (char *)cfg[w->id].code, NULL };
        /* Ayri process group: terminaldeki Ctrl+C sadece watchdog'a gider,
         * cocuklari watchdog kontrollu sekilde kapatir. */
        setpgid(0, 0);
        /* Bloklu sinyal maskesi exec'ten sonra da kalir; orijinali geri yukle. */
        sigprocmask(SIG_SETMASK, &child_mask, NULL);
        execv(WORKER_PATH, argv);
        _exit(127);                             /* exec basarisiz */
    }

    setpgid(pid, pid);                          /* parent tarafinda da (yaris onlemi) */
    w->pid   = pid;
    w->state = STATE_RUNNING;
    logger_log("Started worker %d (pid=%d, life=%ss, exit code=%s)",
               w->id, (int)pid, cfg[w->id].life, cfg[w->id].code);
    return 0;
}

int pm_start_all(const sigset_t *orig_mask)
{
    int i, started = 0;

    child_mask = *orig_mask;
    if (install_handlers() == -1) {
        logger_log("sigaction(SIGCHLD/SIGALRM) failed: %s", strerror(errno));
        return -1;
    }

    for (i = 0; i < NUM_WORKERS; i++) {
        workers[i].id            = i;
        workers[i].pid           = 0;
        workers[i].restart_count = 0;
        if (spawn(&workers[i]) == 0)
            started++;
    }
    if (started < 2)
        return -1;

    set_timer(HEALTH_CHECK_SEC);
    return 0;
}

static worker_info_t *find_running(pid_t pid)
{
    int i;
    for (i = 0; i < NUM_WORKERS; i++)
        if (workers[i].pid == pid && workers[i].state == STATE_RUNNING)
            return &workers[i];
    return NULL;
}

/* waitpid()'in dondurdugu status'u yorumla, gerekirse yeniden baslat. */
static void handle_exit(worker_info_t *w, int status)
{
    pid_t old = w->pid;
    w->pid = 0;

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        w->state = STATE_EXITED_NORMAL;
        logger_log("Worker %d (pid=%d) exited normally (code 0) - not restarted",
                   w->id, (int)old);
        return;
    }

    if (WIFEXITED(status)) {
        w->state = STATE_EXITED_ERROR;
        logger_log("Worker %d (pid=%d) exited with error code %d",
                   w->id, (int)old, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        w->state = STATE_KILLED_BY_SIGNAL;
        logger_log("Worker %d (pid=%d) killed by signal %d",
                   w->id, (int)old, WTERMSIG(status));
    }

    if (!restarts_enabled)                      /* kapanis sirasinda: restart yok */
        return;

    if (w->restart_count >= MAX_RESTARTS) {
        w->state = STATE_RESTART_LIMIT_REACHED;
        logger_log("Worker %d reached restart limit (%d) - giving up",
                   w->id, MAX_RESTARTS);
        return;
    }

    w->restart_count++;
    w->state = STATE_RESTARTING;
    logger_log("Restarting worker %d (attempt %d/%d)",
               w->id, w->restart_count, MAX_RESTARTS);
    if (spawn(w) == -1)
        w->state = STATE_RESTART_LIMIT_REACHED;
}

int pm_reap_nonblocking(void)
{
    int status, i, running = 0;
    pid_t pid;

    /* Birden fazla SIGCHLD tek sinyale birlesebilir: hepsini topla. */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        worker_info_t *w = find_running(pid);
        if (w != NULL)
            handle_exit(w, status);
        else
            logger_log("Reaped unknown child pid=%d", (int)pid);
    }
    if (pid == -1 && errno != ECHILD)
        logger_log("waitpid failed: %s", strerror(errno));

    for (i = 0; i < NUM_WORKERS; i++)
        if (workers[i].state == STATE_RUNNING)
            running++;
    return running;
}

static void health_check(void)
{
    int i, alive = 0;

    for (i = 0; i < NUM_WORKERS; i++) {
        if (workers[i].state != STATE_RUNNING)
            continue;
        if (kill(workers[i].pid, 0) == 0)       /* sinyal 0: sadece varlik kontrolu */
            alive++;
        else
            logger_log("Health check: worker %d (pid=%d) not reachable: %s",
                       i, (int)workers[i].pid, strerror(errno));
    }
    logger_log("Health check: %d/%d workers running", alive, NUM_WORKERS);
}

void pm_handle_events(void)
{
    if (g_child_exited) {
        g_child_exited = 0;
        pm_reap_nonblocking();
    }
    if (g_alarm) {
        g_alarm = 0;
        pm_reap_nonblocking();                  /* kacirilmis SIGCHLD'ye karsi guvenlik */
        health_check();
    }
}

void pm_stop_restarts(void)
{
    restarts_enabled = 0;
    set_timer(0);                               /* periyodik kontrolu durdur */
}

int pm_signal_all(int signo)
{
    int i, sent = 0;

    for (i = 0; i < NUM_WORKERS; i++) {
        if (workers[i].state != STATE_RUNNING)
            continue;
        if (kill(workers[i].pid, signo) == 0) {
            logger_log("Sent signal %d to worker %d (pid=%d)",
                       signo, i, (int)workers[i].pid);
            sent++;
        } else {
            logger_log("kill(%d, %d) failed: %s",
                       (int)workers[i].pid, signo, strerror(errno));
        }
    }
    return sent;
}

const worker_info_t *pm_get_workers(int *count)
{
    *count = NUM_WORKERS;
    return workers;
}
