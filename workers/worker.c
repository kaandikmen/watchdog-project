/* Test amacli basit worker.
 * Kullanim: ./worker [omur_sn] [cikis_kodu]
 *   omur_sn = 0 veya yok -> sonsuza kadar calisir
 *   cikis_kodu            -> bittiginde dondurulecek kod (varsayilan 0)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int life = (argc > 1) ? atoi(argv[1]) : 0;
    int code = (argc > 2) ? atoi(argv[2]) : 0;
    int i;

    printf("worker pid=%d basladi (omur=%d, kod=%d)\n",
           (int)getpid(), life, code);
    fflush(stdout);

    for (i = 0; life == 0 || i < life; i++)
        sleep(1);

    return code;
}
