#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/sem.h>

// TODO: write everything through 'array' of semid's

enum SEM {
        SEM_IN,
        SEM_OUT,
        SEM_BOAT,
};

union semun {
        int val;
        struct semid_ds *buf;
        unsigned short int *array;
        struct seminfo *__buf;
} arg;

//! Color ANSI escape codes.
const char DEFAULT_COLOR[] = "\x1b[0m";
const char RED[]           = "\x1B[31m";
const char GREEN[]         = "\x1b[32m";
const char MAGENTA[]       = "\x1b[95m";
const char CYAN[]          = "\x1B[36m";

void print_wcolor(FILE *stream, const char *color, const char *format, ...)
{
        va_list args = {};

        fprintf(stream, "%s", color);

        va_start(args, format);
        vfprintf(stream, format, args);
        va_end(args);

        fprintf(stream, "%s", DEFAULT_COLOR);
}

static void
event(char* format, ...)
{
	va_list args = {};

	va_start(args, format);
	print_wcolor(stderr, DEFAULT_COLOR, format, args);
	va_end(args);
}

static int
sem_lock(int semid)
{
        struct sembuf op = {.sem_num = 0, .sem_op = -1};
        return semop(semid, &op, 1);
}

static int
sem_unlock(int semid)
{
        struct sembuf op = {.sem_num = 0, .sem_op = 1};
        return semop(semid, &op, 1);
}

int
boat(int semid, long boat_cap, long pass_cap, long n_cycles)
{
        event("Boat moored to the shore!\n");

        for (int i = 0; i < n_cycles; i++) {
                arg.val = 0;
                semctl(semid, SEM_OUT, SETVAL, arg);

                fprintf(stderr, "Boat is ready for passengers.\n");
                // Open pass in.
                arg.val = pass_cap;
                semctl(semid, SEM_IN, SETVAL, arg);
                // Open boat.
                arg.val = boat_cap;
                semctl(semid, SEM_BOAT, SETVAL, arg);

                // Then passengers will begin to enter boat: 
                // no more than pass_cap passengers will enter 
                // pass, and no more than boat_cap will enter
                // the boat. As soon as boat is full pass will be locked.

                // Wait for passengers.
                struct sembuf op[2] = {
                        {.sem_num = SEM_BOAT, .sem_op = 0},
                        {.sem_num = SEM_IN, .sem_op = 0}
                };
                semop(semid, op, 1);

                arg.val = 0;
                semctl(semid, SEM_IN, SETVAL, arg);
                fprintf(stderr, "Boat is full! Closing pass.\n");

                fprintf(stderr, "Leaving.\n");
                sleep(1);

                fprintf(stderr, "Boat is at dock again. Passengers out.\n");

                // Load pass cap.
                arg.val = pass_cap;
                semctl(semid, SEM_OUT, SETVAL, arg);
                // Load boat cap.
                arg.val = boat_cap;
                semctl(semid, SEM_BOAT, SETVAL, arg);

                // Wait for passengers to return from boat.
                struct sembuf op_2[2] = {
                        {.sem_num = SEM_BOAT, .sem_op = 0},
                        {.sem_num = SEM_OUT, .sem_op = -pass_cap},
                };
                semop(semid, op_2, 2);
        }

        semctl(semid, SEM_IN, IPC_RMID);
        semctl(semid, SEM_OUT, IPC_RMID);
        semctl(semid, SEM_BOAT, IPC_RMID);
        fprintf(stderr, "Trip is over!\n");

        return 0;
}

int 
passenger(int semid, const long n)
{       
        // Passenger enters pass and 
        // buys a ticket(reserves place in the boat). 
        struct sembuf op[2] = {
                {.sem_num = SEM_IN, .sem_op = -1},
                {.sem_num = SEM_BOAT, .sem_op = -1}
        };

        // Passenger enters boat and leaves pass.
        semop(semid, op, 2);
        fprintf(stderr, "#%ld entered pass.\n", n);

        // On boat, so not on pass.
        struct sembuf pass_boat_op[2] = {
                {.sem_num = SEM_IN,   .sem_op = 1},
                {.sem_num = SEM_BOAT, .sem_op = -1}
        };
        semop(semid, pass_boat_op, 1);
        fprintf(stderr, "#%ld entered boat.\n", n);

        // Wait for boat is full. When boat_semid is 
        // locked until trip is over. 
        struct sembuf op_wait = {.sem_num = SEM_BOAT, .sem_op = 0};
        semop(semid, &op_wait, 1);

        // Off boat.
        // Not on boat, so on pass.
        struct sembuf in_boat[2] = {
                {.sem_num = SEM_BOAT, .sem_op = -1},
                {.sem_num = SEM_OUT, .sem_op = -1},
        };
        semop(semid, in_boat, 2);
        fprintf(stderr, "#%ld leaving boat.\n", n);

        struct sembuf op_2 = {.sem_num = SEM_OUT, .sem_op = 1};
        semop(semid, &op_2, 1);
        /*sem_unlock(out_semid);*/
        fprintf(stderr, "#%ld leaving pass.\n", n);

        return 0;
}

int
main(int argc, char *argv[])
{
        long n_passengers = atol(argv[1]);
        long pass_cap     = atol(argv[2]);
        long boat_cap     = atol(argv[3]);
        long n_cycles     = atol(argv[4]);

        int semid = semget(IPC_PRIVATE, 3, IPC_CREAT | 0666);

        if (fork() == 0) {
                int retval = boat(semid, boat_cap, pass_cap, n_cycles);

                return retval;
        }

        for (long i = 1; i <= n_passengers; i++) {
                if (fork() == 0) {
                        return passenger(semid, i); 
                }
        }

        for (long i = 0; i < n_passengers; i++)
                wait(NULL);

        wait(NULL);

        semctl(semid, SEM_IN, IPC_RMID);
        semctl(semid, SEM_OUT, IPC_RMID);
        semctl(semid, SEM_BOAT, IPC_RMID);

        return 0;
}
