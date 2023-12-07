#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>

typedef struct {
        int  n_files;
        char **files;
} args_t;

static int
error(char* format, ...)
{
        fprintf(stderr, "sigcat: ");

	va_list args = {};
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	return 1;
}

volatile sig_atomic_t bit;

void
reader_handler(int signo)
{
}

int
reader(pid_t pid, int fd)
{
        sigset_t empty_set;
        sigemptyset(&empty_set);

        struct sigaction sa;
        sa.sa_handler = reader_handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGUSR1, &sa, NULL) != 0)
                return error(strerror(errno));
        if (sigaction(SIGUSR2, &sa, NULL) != 0)
                return error(strerror(errno));

        static char buf[4096];
        size_t n_read = 0;
        do {
                n_read = read(fd, buf, 4096);
                if (n_read == -1)
                        return error(strerror(errno));

                for (int i = 0; i < n_read; i++) {
                        char byte = buf[i];
                        for (int j = 0; j != 8; j++) {
                                if (byte >> j & 1)
                                        kill(pid, SIGUSR2);
                                else
                                        kill(pid, SIGUSR1);

                                sigsuspend(&empty_set);
                        }
                }
        } while (n_read != 0);

        return 0;
}

void
writer_handler(int signo)
{
        if (signo == SIGUSR2)
                bit = 1;
        else 
                bit = 0;
}

int
writer(pid_t pid, int fd)
{
        sigset_t empty_set;
        sigemptyset(&empty_set);

        struct sigaction sa;
        sa.sa_handler = writer_handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGUSR1, &sa, NULL) != 0)
                return error(strerror(errno));
        if (sigaction(SIGUSR2, &sa, NULL) != 0)
                return error(strerror(errno));

        while (true) {
                char byte = 0;
                for (int i = 0; i < 8; i++) {
                        sigsuspend(&empty_set);
                        byte |= ((unsigned char)bit << i);
                        kill(pid, SIGUSR1);
                }

                write(fd, &byte, 1);
        }

        return 0;
}

int
cat(int fd)
{
        sigset_t block_set, old_set;

        sigemptyset(&block_set);
        sigaddset(&block_set, SIGUSR1);
        sigaddset(&block_set, SIGUSR2);

        if (sigprocmask(SIG_BLOCK, &block_set, &old_set))
                return error(strerror(errno));

        pid_t reader_pid = getpid();
        pid_t writer_pid = fork();
        if (writer_pid == 0)
                return writer(reader_pid, STDOUT_FILENO);

        reader(writer_pid, fd);
        kill(writer_pid, SIGTERM);

        if (sigprocmask(SIG_BLOCK, &old_set, NULL))
                return error(strerror(errno));

        return 0;
}

int
main(int argc, char *argv[])
{
        int retval = 0;
        
        // Parse args.
        args_t args = {.n_files = argc - 1, .files = argv + 1};

        // Cat.
        if (args.n_files == 0) {
                retval = cat(STDIN_FILENO);
        } else {
                for (int i = 0; i < args.n_files; i++) {
                        int fd = open(args.files[i], O_RDONLY); 
                        if (fd == -1) {
                                retval |= error(strerror(errno));
                                continue;
                        }

                        if (cat(fd) != 0) {
                                retval |= error(strerror(errno));
                                continue;
                        }

                        close(fd);
                }
        }

        return retval;
}

