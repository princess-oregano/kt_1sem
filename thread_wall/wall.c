#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
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

        int ret = 0; 

        ret = pthread_mutex_init(&mon->mutex, NULL);
        if (ret != 0)
                return error(strerror(errno));

        ret = pthread_cond_init(&mon->f_cond, NULL);
        if (ret != 0) {
                pthread_mutex_destroy(&mon->mutex);
                return error(strerror(errno));
        }

        ret = pthread_cond_init(&mon->s_cond, NULL);
        if (ret != 0) {
                pthread_mutex_destroy(&mon->mutex);
                pthread_cond_destroy(&mon->f_cond);
                return error(strerror(errno));
        }

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
                error(strerror(ret));
                return mon;
        }

        while (mon->s_bricks < N_TILES + TILES_IN_ROW) {
                if (mon->f_bricks < mon->s_bricks - TILES_IN_ROW + 1) {
                        ret = pthread_cond_wait(&mon->s_cond, &mon->mutex);
                        if (ret != 0) {
                                error(strerror(ret));
                                return mon;
                        }
                }

                mon->s_bricks++;
                fprintf(stderr, "son: %d\n", mon->s_bricks - TILES_IN_ROW);

                ret = pthread_cond_signal(&mon->f_cond);
                if (ret != 0) {
                        error(strerror(ret));
                        return mon;
                }

        }

        ret = pthread_mutex_unlock(&mon->mutex);
        if (ret != 0) {
                error(strerror(ret));
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
                error(strerror(ret));
                return mon;
        }

        while (mon->f_bricks < N_TILES) {
                if (mon->s_bricks < mon->f_bricks + 1) {
                        ret = pthread_cond_wait(&mon->f_cond, &mon->mutex);
                        if (ret != 0) {
                                error(strerror(ret));
                                return mon;
                        }
                }

                mon->f_bricks++;
                fprintf(stderr, "father: %d\n", mon->f_bricks);

                ret = pthread_cond_signal(&mon->s_cond);
                if (ret != 0) {
                        error(strerror(ret));
                        return mon;
                }

        }
        
        ret = pthread_mutex_unlock(&mon->mutex);
        if (ret != 0) {
                error(strerror(ret));
                return mon;
        }

        fprintf(stderr, "Father has put all the tiles.\n");

        return NULL;
}

int
main()
{
        int ret = 0;

        monitor_t mon = {0};
        ret = mon_ctor(&mon);
        if (ret != 0)
                return ret;

        pthread_t f_thread = 0;
        ret = pthread_create(&f_thread, NULL, father, &mon);
        if (ret != 0) {
                error(strerror(ret));
                return 1;
        }

        pthread_t s_thread = 0;
        ret = pthread_create(&s_thread, NULL, son, &mon);
        if (ret != 0) {
                error(strerror(ret));
                return 1;
        }

        void *retval = NULL;
        ret = pthread_join(f_thread, &retval);
        if (ret != 0) {
                error(strerror(ret));
                return 1;
        }
        if (retval != NULL) {
                error("father() exited with error.\n");
                return 1;
        }

        ret = pthread_join(s_thread, &retval);
        if (ret != 0) {
                error(strerror(ret));
                return 1;
        }
        if (retval != NULL) {
                error("son() exited with error.\n");
                return 1;
        }

        mon_dtor(&mon);

        return 0;
}
