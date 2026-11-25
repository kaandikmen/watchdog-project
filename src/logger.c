#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *log_fp = NULL;

int logger_init(const char *path)
{
    log_fp = fopen(path, "a");
    if (log_fp == NULL) {
        perror("logger_init: fopen");
        return -1;
    }
    setvbuf(log_fp, NULL, _IOLBF, 0);   /* her satirda flush */
    return 0;
}

void logger_close(void)
{
    if (log_fp != NULL) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

void logger_log(const char *fmt, ...)
{
    char ts[32];
    time_t now;
    struct tm tm_buf;
    va_list ap;

    if (log_fp == NULL)
        return;

    now = time(NULL);
    localtime_r(&now, &tm_buf);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm_buf);

    fprintf(log_fp, "[%s] ", ts);
    va_start(ap, fmt);
    vfprintf(log_fp, fmt, ap);
    va_end(ap);
    fputc('\n', log_fp);
}

const char *logger_state_name(worker_state_t s)
{
    switch (s) {
    case STATE_RUNNING:               return "RUNNING";
    case STATE_EXITED_NORMAL:         return "EXITED_NORMAL";
    case STATE_EXITED_ERROR:          return "EXITED_ERROR";
    case STATE_KILLED_BY_SIGNAL:      return "KILLED_BY_SIGNAL";
    case STATE_RESTARTING:            return "RESTARTING";
    case STATE_RESTART_LIMIT_REACHED: return "RESTART_LIMIT_REACHED";
    }
    return "UNKNOWN";
}

void logger_report_status(const worker_info_t *workers, int count)
{
    int i;

    logger_log("=== STATUS REPORT (SIGUSR1) ===");
    for (i = 0; i < count; i++) {
        logger_log("worker %d | pid=%d | state=%s | restarts=%d",
                   workers[i].id, (int)workers[i].pid,
                   logger_state_name(workers[i].state),
                   workers[i].restart_count);
    }
    logger_log("=== END OF REPORT ===");
}
