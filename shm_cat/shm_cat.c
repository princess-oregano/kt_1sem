#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>

const char *PROGRAM_NAME = "cat";

enum buf_t {
        RD_BUF = 0,
        WR_BUF = 1,
};

// BUF_SIZE is low to detect bugs.
const int BUF_SIZE = 4;

static int
error(char* format, ...)
{
        fprintf(stderr, "%s: ", PROGRAM_NAME);

	va_list args = {};
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	return 1;
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

ssize_t my_write(int fd, const char *buffer, ssize_t buf_size)
{
        ssize_t nwrote = 0;
        ssize_t n = 0;

        while (nwrote < buf_size) {
                n = write(fd, buffer, buf_size - nwrote);
                if (n <= 0) {
                        return n;
                }

                nwrote += n;
        }

        return nwrote;
}

static int
writer(int rd_id, int wr_id, char *buf[2], ssize_t *nread_wr, ssize_t *nread_rd)
{
        while (1) {
                sem_lock(rd_id);
                ssize_t nwrote = write(STDOUT_FILENO, buf[RD_BUF], *nread_rd);
                if (nwrote < 0) {
                        perror("my_cat: could not write");
                        return -1;
                }
                sem_unlock(rd_id);

                sem_lock(wr_id);
                nwrote = write(STDOUT_FILENO, buf[WR_BUF], *nread_wr);
                if (nwrote < 0) {
                        perror("my_cat: could not write");
                        return -1;
                }

                sem_unlock(wr_id);
        }
        
        return 0;
}

static int
reader(int rd_id, int wr_id, char *buf[2], ssize_t *nread_wr, ssize_t *nread_rd, int fd_in)
{
        while(1) {
                sem_lock(rd_id);
                *nread_rd = read(fd_in, buf[RD_BUF], BUF_SIZE);
                if (*nread_rd <= 0)
                        break;

                
                sem_unlock(rd_id);

                sem_lock(wr_id);
                *nread_wr = read(fd_in, buf[WR_BUF], BUF_SIZE);
                if (*nread_wr <= 0)
                        break;

                sem_unlock(wr_id);
        }

        return 0;
}

int
main(int argc, char *argv[])
{
        int retval = 0;

        // Get semid's for managing memory.
        int rd_semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
        if (rd_semid == -1)
                return error("cannot get semid in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);
        int wr_semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
        if (wr_semid == -1)
                return error("cannot get semid in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);

        // Get shmid's.
        int rd_shmid = shmget(IPC_PRIVATE, BUF_SIZE, IPC_CREAT | 0666);
        if (rd_shmid == -1)
                return error("cannot shmget in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);
        int wr_shmid = shmget(IPC_PRIVATE, BUF_SIZE, IPC_CREAT | 0666);
        if (wr_shmid == -1)
                return error("cannot shmget in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);

        // Get memory pointers.
        char *rd_buf = (char *) shmat(rd_shmid, NULL, 0);
        if (rd_buf == (char *) -1)
                return error("cannot shmat in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);
        char *wr_buf = (char *) shmat(wr_shmid, NULL, 0);
        if (wr_buf == (char *) -1)
                return error("cannot shmat in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);

        char *buf[2] = {rd_buf, wr_buf};

        // Memory for nread.
        int nread_wr_shmid = shmget(IPC_PRIVATE, sizeof(ssize_t), IPC_CREAT | 0666);
        if (nread_wr_shmid == -1)
                return error("cannot shmget in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);
        ssize_t *nread_wr = (ssize_t *) shmat(nread_wr_shmid, NULL, 0);
        if (nread_wr == (ssize_t *) -1)
                return error("cannot shmat in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);

        int nread_rd_shmid = shmget(IPC_PRIVATE, sizeof(ssize_t), IPC_CREAT | 0666);
        if (nread_rd_shmid == -1)
                return error("cannot shmget in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);
        ssize_t *nread_rd = (ssize_t *) shmat(nread_rd_shmid, NULL, 0);
        if (nread_rd == (ssize_t *) -1)
                return error("cannot shmat in %s(%d)\n", __PRETTY_FUNCTION__, __LINE__);

        // Fork: child -> writer(consumer), parent -> reader(producer).
        if (fork() == 0) {
                retval = writer(rd_semid, wr_semid, buf, nread_wr, nread_rd);
                return retval;
        }

        sem_unlock(rd_semid);
        sem_unlock(wr_semid);
        retval = reader(rd_semid, wr_semid, buf, nread_wr, nread_rd, STDIN_FILENO);

        wait(NULL);

        shmdt(rd_buf);
        shmdt(wr_buf);
        shmdt(nread_wr);

        shmctl(rd_shmid, IPC_RMID, 0);
        shmctl(wr_shmid, IPC_RMID, 0);
        shmctl(nread_wr_shmid, IPC_RMID, 0);
        
        semctl(rd_semid, IPC_RMID, 0);
        semctl(wr_semid, IPC_RMID, 0);

        return retval;
}
