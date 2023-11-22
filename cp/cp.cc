#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <dirent.h>
#include <sys/stat.h>
#include "cp.h"

static bool verbose = false;
static bool interactive = true;
static bool force = false;
static bool preserve = false; 
static bool help = false;

static void
print_help()
{
        printf("Usage: ./cp [OPTION]... SOURCE... DIRECTORY\n");
}

static ssize_t 
my_write(int fd, const char *buffer, ssize_t buf_size)
{
        assert(buffer);

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
file_translator(int fd_in, int fd_out) 
{
        char buffer[BUF_SIZE];
        ssize_t nread = 0;

        while(true) {
                nread = read(fd_in, buffer, BUF_SIZE);
                if (nread <= 0)
                        break;

                ssize_t nwrote = my_write(fd_out, buffer, nread);
                if (nwrote < 0) {
                        perror("file_translator: could not write");
                        return 1;
                }
        }

        if (nread < 0) {
                perror("file_translator: could not read");
                return 1;
        }

        return 0;
}

static void 
option_switch(int opt)
{
        switch (opt) {
                case 'v':
                        verbose = true;
                        break;
                case 'f':
                        force = true;
                        break;
                case 'h':
                        print_help();
                        help = true; 
                        // FIXME: exit is BAD.
                        exit(0);
                        break;
                case 'i':
                        interactive = true;
                        break;
                case 'p':
                        preserve = true;
                        break;
                default: 
                        assert(0 && "The option in not implemented yet.");
        }
}

// TODO: return fd_out
static bool
overwrite(const char *file, mode_t mode) 
{
        assert(file);

        bool ret_val = false;
        int fd_out = open(file, O_WRONLY | O_CREAT | O_EXCL, mode);
        if (errno == EEXIST) {
                printf("cp: overwrite '%s'? ", file);

                char c;

                if (getchar() == 'y')
                        ret_val = true;

                while ((c = getchar()) != '\n' && c != EOF) ;

                if (verbose)
                        printf("skipped '%s'\n", file);

                return ret_val;
        } else {
                perror("overwrite(): ");
                exit(1);
        }

        close(fd_out);

        return true;
}

int 
parse(int argc, char *argv[])
{
        while(true) {
                int option_index = 0;
                int c = getopt_long(argc, argv, "fivph", 
                                    long_options, &option_index);
                if (c == -1)
                        break;

                option_switch(c);
        }

        return 0;
}


int
cp(const char *src, const char *dest)
{
        assert(src);
        assert(dest);

        mode_t mode;
        if (preserve) {
                struct stat fs; 
                int r = stat(src, &fs);
                if (r == -1) {
                        perror("cp(): ");
                        exit(1);
                }

                mode = fs.st_mode;
        } else {
                mode = S_IRUSR | S_IWUSR;
        }

        if (interactive) {
                if(!overwrite(dest, mode)) 
                        return 1;
        }

        if (verbose)
                printf("'%s' -> '%s'\n", src, dest);

        int fd_in = open(src, O_RDONLY);
        if (fd_in == -1) {
                perror("cp(): ");
                exit(1);
        }

        int fd_out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, mode); 

        if ((errno != EEXIST) && force) {
                if (errno == EACCES) {
                        // TODO: add more interface.
                        chmod(src, S_IWUSR | S_IRUSR);
                }
                if (remove(dest)) {
                        perror("cp(): ");
                        exit(1);
                }

                fd_out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, mode); 
        }

        if (fd_out == -1) {
                perror("cp(): ");
                exit(1);
        }

        int err = file_translator(fd_in, fd_out);

        close(fd_in);
        close(fd_out);

        return err;
}

bool
check_dir(const char *dir)
{
        DIR* dir_ptr = opendir(dir);
        if (dir_ptr) {
            closedir(dir_ptr);
            return true;
        } else if (ENOENT == errno) {
                return false;
        } else {
                perror("check_dir: ");
                exit(1);
        }
}

char *
name_wdir(const char *file, const char *dir)
{
        char *new_name = (char *) calloc(strlen(file) + strlen(dir) + 1, sizeof(char));

        sprintf(new_name, "%s/%s", dir, file);

        return new_name;
}

