#ifndef LOGGER_H
#define LOGGER_H

#include <sys/types.h>

#define LOG_FILE_DEFAULT "watchdog.log"

/* Bir worker'in olabilecegi durumlar (Student 2 de bunu kullanir) */
typedef enum {
    STATE_RUNNING,
    STATE_EXITED_NORMAL,        /* exit(0) ile temiz cikis */
    STATE_EXITED_ERROR,         /* exit(!=0) ile hatali cikis */
    STATE_KILLED_BY_SIGNAL,     /* bir sinyalle olduruldu */
    STATE_RESTARTING,
    STATE_RESTART_LIMIT_REACHED
} worker_state_t;

/* Her worker icin tutulan bilgi (Student 2 bu yapiyi doldurur) */
typedef struct {
    int            id;
    pid_t          pid;
    worker_state_t state;
    int            restart_count;
} worker_info_t;

/* Log dosyasini acar (append modu). Basarida 0, hatada -1 doner. */
int  logger_init(const char *path);

/* Log dosyasini kapatir. */
void logger_close(void);

/* Zaman damgali log satiri yazar. printf formatinda kullanilir.
 * UYARI: Signal handler icinden CAGIRMAYIN (async-signal-safe degil). */
void logger_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* SIGUSR1 raporu: tum worker'larin PID ve durumunu yazar.
 * Student 1'in ana dongusu, bayrak set olunca bunu cagirir. */
void logger_report_status(const worker_info_t *workers, int count);

/* Durum enum'unu yaziya cevirir. */
const char *logger_state_name(worker_state_t s);

#endif
