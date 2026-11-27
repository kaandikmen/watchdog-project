/* Test amacli basit worker.
 * Kullanim: ./worker [omur_sn] [cikis_kodu]
 *   omur_sn = 0 veya yok -> sonsuza kadar calisir
 *   cikis_kodu            -> bittiginde dondurulecek kod (varsayilan 0)
 * SIGUSR2 alirsa temiz sekilde exit(0) yapar (normal termination testi icin).
 * SIGTERM / SIGKILL varsayilan davranisla süreci sonlandirir.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t quit_requested = 0;

static void on_usr2(int sig)
{
    (void)sig;
    quit_requested = 1;
}

int main(int argc, char **argv)
{
    int life = (argc > 1) ? atoi(argv[1]) : 0;
    int code = (argc > 2) ? atoi(argv[2]) : 0;
    struct sigaction sa;
    int i;

    sa.sa_handler = on_usr2;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

    printf("worker pid=%d basladi (omur=%d, kod=%d)\n",
           (int)getpid(), life, code);
    fflush(stdout);

    for (i = 0; !quit_requested && (life == 0 || i < life); i++)
        sleep(1);

    return quit_requested ? 0 : code;
}
