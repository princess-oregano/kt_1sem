#include <asm-generic/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/sem.h>

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

static int
error(char* format, ...)
{
        fprintf(stderr, "boat: ");

	va_list args = {};
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	return 1;
}

int
boat(int semid, long boat_cap, long pass_cap, long n_cycles)
{
        int ret = 0;

        fprintf(stderr, "Boat moored to the shore!\n");

        for (int i = 0; i < n_cycles; i++) {
                fprintf(stderr, "Boat is ready for passengers.\n");
                // Open pass in.
                ret = semctl(semid, SEM_IN, SETVAL, pass_cap);
                if (ret == -1)
                        return error(strerror(errno));
                // Open boat.
                ret = semctl(semid, SEM_BOAT, SETVAL, boat_cap);
                if (ret == -1)
                        return error(strerror(errno));

                // Then passengers will begin to enter boat: 
                // no more than pass_cap passengers will enter 
                // pass, and no more than boat_cap will enter
                // the boat. As soon as boat is full pass will be locked.

                // Wait for passengers to enter the boat and free pass.
                struct sembuf op[2] = {
                        {.sem_num = SEM_BOAT, .sem_op = 0},
                        {.sem_num = SEM_IN, .sem_op = -pass_cap}
                };
                ret = semop(semid, op, 2);
                if (ret == -1)
                        return error(strerror(errno));

                // Pass is locked due to semop.

                fprintf(stderr, "Boat is full! Closing pass.\n");

                fprintf(stderr, "Leaving.\n");
                sleep(1);

                fprintf(stderr, "Boat is at dock again. Passengers out.\n");

                // Load pass cap.
                ret = semctl(semid, SEM_OUT, SETVAL, pass_cap);
                if (ret == -1)
                        return error(strerror(errno));

                // Load boat cap.
                ret = semctl(semid, SEM_BOAT, SETVAL, boat_cap);
                if (ret == -1)
                        return error(strerror(errno));

                // Wait for passengers to return from boat and free pass.
                struct sembuf op_2[2] = {
                        {.sem_num = SEM_BOAT, .sem_op = 0},
                        {.sem_num = SEM_OUT, .sem_op = -pass_cap},
                };
                ret = semop(semid, op_2, 2);
                if (ret == -1)
                        return error(strerror(errno));
        }

        ret = semctl(semid, 0, IPC_RMID);
        if (ret == -1)
                return error(strerror(errno));

        fprintf(stderr, "Trip is over!\n");

        return 0;
}

int 
passenger(int semid, const long n)
{       
        while (1) {
                int retval = 0;

                // Passenger enters pass and 
                // buys a ticket(reserves place in the boat). 
                struct sembuf in_boat_op[2] = {
                        {.sem_num = SEM_IN, .sem_op = -1},
                        {.sem_num = SEM_BOAT, .sem_op = -1}
                };

                retval = semop(semid, in_boat_op, 2);
                if (retval != 0) {
                        if (errno == EIDRM)
                                return 0;
                        else 
                                return error(strerror(errno));
                }

                fprintf(stderr, "#%ld entered pass.\n", n);

                // Passenger enters boat and leaves pass.
                struct sembuf in_op = {.sem_num = SEM_IN, .sem_op = 1};
                retval = semop(semid, &in_op, 1);
                if (retval != 0) {
                        if (errno == EIDRM)
                                return 0;
                        else 
                                return error(strerror(errno));
                }

                fprintf(stderr, "#%ld entered boat.\n", n);

                // Off boat.
                // Not on boat, so on pass.
                struct sembuf out_boat_op[2] = {
                        {.sem_num = SEM_BOAT, .sem_op = -1},
                        {.sem_num = SEM_OUT, .sem_op = -1},
                };
                retval = semop(semid, out_boat_op, 2);
                if (retval != 0) {
                        if (errno == EIDRM)
                                return 0;
                        else 
                                return error(strerror(errno));
                }

                fprintf(stderr, "#%ld leaving boat.\n", n);

                struct sembuf out_op = {.sem_num = SEM_OUT, .sem_op = 1};
                retval = semop(semid, &out_op, 1);
                if (retval != 0) {
                        if (errno == EIDRM)
                                return 0;
                        else 
                                return error(strerror(errno));
                }

                fprintf(stderr, "#%ld leaving pass.\n", n);
        }

        return 0;
}

int
main(int argc, char *argv[])
{
        long n_passengers = atol(argv[1]);
        long pass_cap     = atol(argv[2]);
        long boat_cap     = atol(argv[3]);
        long n_cycles     = atol(argv[4]);

        if (boat_cap > n_passengers)
                return error("boat capacity > number of passengers");

        int semid = semget(IPC_PRIVATE, 3, IPC_CREAT | 0666);
        if (semid == -1)
                return error(strerror(errno));

        if (fork() == 0) {
                return boat(semid, boat_cap, pass_cap, n_cycles);
        }

        for (long i = 1; i <= n_passengers; i++) {
                if (fork() == 0) {
                        return passenger(semid, i); 
                }
        }

        for (long i = 0; i < n_passengers; i++)
                wait(NULL);

        wait(NULL);

        return 0;
}

