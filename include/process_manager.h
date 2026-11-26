#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <signal.h>
#include "logger.h"     /* worker_info_t, worker_state_t */

#define NUM_WORKERS          4
#define MAX_RESTARTS         3
#define HEALTH_CHECK_SEC     5
#define WORKER_PATH          "./worker"

/* SIGCHLD ve SIGALRM handler'larini kurar, worker'lari fork()+execv() ile
 * baslatir ve periyodik kontrol icin setitimer()'i kurar.
 * orig_mask: cocuklarda exec oncesi geri yuklenecek sinyal maskesi.
 * Basarida 0, en az iki worker baslatilamazsa -1 doner. */
int  pm_start_all(const sigset_t *orig_mask);

/* Ana dongu her uyandiginda cagirir: SIGCHLD geldiyse cocuklari toplar
 * (gerekirse yeniden baslatir), SIGALRM geldiyse saglik kontrolu yapar. */
void pm_handle_events(void);

/* Kapanis basladi: yeniden baslatma yok, zamanlayici durur. */
void pm_stop_restarts(void);

/* Calisan tum cocuklara sinyal gonderir; gonderilen sayi doner. */
int  pm_signal_all(int signo);

/* Bitmis cocuklari WNOHANG ile toplar; hala calisan cocuk sayisini doner. */
int  pm_reap_nonblocking(void);

/* SIGUSR1 raporu icin worker tablosu. */
const worker_info_t *pm_get_workers(int *count);

#endif
