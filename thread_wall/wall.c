#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>

const int TILES_IN_ROW = 10;
const int N_TILES = 30;

//---------------- MISC -----------------
static int
error(char* format, ...)
{
        fprintf(stderr, "wall: ");

	va_list args = {};
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	return 1;
}

//----------------------------------------

typedef struct {
        int f_bricks;
        int s_bricks;

        pthread_mutex_t mutex;
        pthread_cond_t f_cond;
        pthread_cond_t s_cond;
} monitor_t;

static int
mon_ctor(monitor_t *mon)
{
        mon->f_bricks = 0;
        mon->s_bricks = TILES_IN_ROW;

        pthread_mutex_init(&mon->mutex, NULL);
        pthread_cond_init(&mon->f_cond, NULL);
        pthread_cond_init(&mon->s_cond, NULL);

        return 0;
}

static int
mon_dtor(monitor_t *mon)
{
        mon->f_bricks = -1;
        mon->s_bricks = -1;

        pthread_mutex_destroy(&mon->mutex);
        pthread_cond_destroy(&mon->f_cond);
        pthread_cond_destroy(&mon->s_cond);

        return 0;
}

static void *
son(void *arg)
{
        int ret = 0;
        monitor_t *mon = (monitor_t *) arg;

        fprintf(stderr, "Son started working.\n");
        ret = pthread_mutex_lock(&mon->mutex);
        if (ret != 0) {
                error("%s", strerror(ret));
                return mon;
        }

        while (mon->s_bricks < N_TILES + TILES_IN_ROW) {
                if (mon->f_bricks < mon->s_bricks - TILES_IN_ROW + 1) {
                        ret = pthread_cond_wait(&mon->s_cond, &mon->mutex);
                        if (ret != 0) {
                                error("%s", strerror(ret));
                                return mon;
                        }
                }

                mon->s_bricks++;
                fprintf(stderr, "son: %d\n", mon->s_bricks - TILES_IN_ROW);

                ret = pthread_cond_signal(&mon->f_cond);
                if (ret != 0) {
                        error("%s", strerror(ret));
                        return mon;
                }

        }

        ret = pthread_mutex_unlock(&mon->mutex);
        if (ret != 0) {
                error("%s", strerror(ret));
                return mon;
        }

        fprintf(stderr, "Son has put all the tiles.\n");

        return NULL;
}

static void *
father(void *arg)
{
        int ret = 0;
        monitor_t *mon = (monitor_t *) arg;

        fprintf(stderr, "Father started working.\n");
        ret = pthread_mutex_lock(&mon->mutex);
        if (ret != 0) {
                error("%s", strerror(ret));
                return mon;
        }

        while (mon->f_bricks < N_TILES) {
                if (mon->s_bricks < mon->f_bricks + 1) {
                        ret = pthread_cond_wait(&mon->f_cond, &mon->mutex);
                        if (ret != 0) {
                                error("%s", strerror(ret));
                                return mon;
                        }
                }

                mon->f_bricks++;
                fprintf(stderr, "father: %d\n", mon->f_bricks);

                ret = pthread_cond_signal(&mon->s_cond);
                if (ret != 0) {
                        error("%s", strerror(ret));
                        return mon;
                }

        }
        
        ret = pthread_mutex_unlock(&mon->mutex);
        if (ret != 0) {
                error("%s", strerror(ret));
                return mon;
        }

        fprintf(stderr, "Father has put all the tiles.\n");

        return NULL;
}

int
main()
{
        monitor_t mon = {0};
        mon_ctor(&mon);

        pthread_t f_thread = 0;
        int ret = pthread_create(&f_thread, NULL, father, &mon);
        if (ret != 0) {
                error("%s", strerror(ret));
        }

        pthread_t s_thread = 0;
        ret = pthread_create(&s_thread, NULL, son, &mon);
        if (ret != 0) {
                error("%s", strerror(ret));
        }

        void *retval = NULL;
        ret = pthread_join(f_thread, &retval);
        if (ret != 0) {
                error("%s", strerror(ret));
                return 1;
        }
        if (retval != NULL) {
                error("father() exited with error.\n");
                return 1;
        }

        ret = pthread_join(s_thread, &retval);
        if (ret != 0) {
                error("%s", strerror(ret));
                return 1;
        }
        if (retval != NULL) {
                error("son() exited with error.\n");
                return 1;
        }

        mon_dtor(&mon);

        return 0;
}
