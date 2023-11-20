#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <string.h>

const char* MONTH[12] = {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec",
};

typedef struct {
        // Main data.
        int n_files;
        char **filename;
        // Flags.
        bool all;
        bool one;
        bool help;
        bool longfmt;
        bool recursive;
        bool group;
} args_t;

const struct option long_options[] = {
        {"all", no_argument, 0, 'a'},
        {"help", no_argument, 0, 'h'},
        {"long", no_argument, 0, 'l'},
        {"recursive", no_argument, 0, 'R'},
        {"ommit_group", no_argument, 0, 'o'},
        {0, 0, 0, 0}
};

static int
error(char* format, ...)
{
        fprintf(stderr, "ls: ");

        va_list args = {};
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);

        return 1;
}

static void
print_help()
{
        printf("Supported options: -a, -o, -h, -l, -R.\n");
}

static void
option_switch(int opt, args_t *args)
{
        switch (opt) {
                case 'o':
                        args->group = true;
                        break;
                case 'a':
                        args->all = true;
                        break;
                case 'l':
                        args->longfmt = true;
                        break;
                case 'R':
                        args->recursive = true;
                        break;
                case 'h':
                        print_help();
                        exit(0);
                case '?':
                default: 
                        assert(0 && "The option in not implemented yet.");
        }
}

static int 
parse(int argc, char *argv[], args_t *args)
{
        while(1) {
                int option_index = 0;
                int c = getopt_long(argc, argv, "+aholR", 
                                    long_options, &option_index);
                if (c == -1)
                        break;

                option_switch(c, args);
        }

        args->n_files = argc - optind;
        args->filename = &argv[optind];

        return 0;
}

static int
filter_hidden(const struct dirent* ent)
{
        if (ent->d_name[0] == '.')
                return 0;

        return 1;
}

static int
print_link(char *path)
{
        int retval = 0;

        printf(" -> ");

        char *buf = (char *) calloc(pathconf(".", _PC_PATH_MAX), sizeof(char)); 
        if (buf == NULL) {
                retval = error("%s\n", strerror(errno));
                return retval;
        }

        int len = readlink(path, buf, 1024);
        if (len == -1) {
                free(buf);
                retval = error("%s\n", strerror(errno));
                return retval;
        }

        printf("%s", buf);

        free(buf);

        return 0;
}

static void
print_long_fmt(args_t *args, char *path, char *filename, struct stat *stats)
{
        switch(stats->st_mode & S_IFMT) {
                case S_IFDIR: 
                        printf("d");
                        break;
                case S_IFREG: 
                        printf("-");
                        break;
                case S_IFLNK: 
                        printf("l");
                        break;
                default:
                        printf("?");
                        break;
        }
                
        printf(stats->st_mode & S_IRUSR ? "r" : "-");
        printf(stats->st_mode & S_IWUSR ? "w" : "-");
        printf(stats->st_mode & S_IXUSR ? "x" : "-");

        printf(stats->st_mode & S_IRGRP ? "r" : "-");
        printf(stats->st_mode & S_IWGRP ? "w" : "-");
        printf(stats->st_mode & S_IXGRP ? "x" : "-");

        printf(stats->st_mode & S_IROTH ? "r" : "-");
        printf(stats->st_mode & S_IWOTH ? "w" : "-");
        printf(stats->st_mode & S_IXOTH ? "x" : "-");

        printf(" %lu", stats->st_nlink);

        struct passwd* user_info = getpwuid(stats->st_uid);
        if (user_info)
                printf(" %s", user_info->pw_name);
        else
                printf(" %u", stats->st_uid);

        if (!args->group) {
                struct group* group_info = getgrgid(stats->st_gid);
                if (group_info)
                        printf(" %s", group_info->gr_name);
                else
                        printf(" %u", stats->st_gid);
        }

        printf(" %ld", stats->st_size);

        struct tm* curtime = localtime(&stats->st_mtim.tv_sec);
        printf(" %s %2d %02d:%02d",
                        MONTH[curtime->tm_mon], curtime->tm_mday,
                        curtime->tm_hour, curtime->tm_min);

        printf(" %s", filename);

        if ((stats->st_mode & S_IFMT) == S_IFLNK) {
                print_link(path);
        }

        printf("\n");
}

static int
print_file(args_t *args, char *path, char *filename, struct stat *stats)
{
        if (args->longfmt) {
                print_long_fmt(args, path, filename, stats);
        } else {
                printf("%s ", filename);
        }

        return 0;
}

static int
print_dir(args_t *args, char *filename_buf, struct stat *stats)
{
        if (args->n_files > 1 || args->recursive)
                printf("%s:\n", filename_buf);
        if (args->longfmt)
                printf("total %ld\n", stats->st_blocks);

        size_t filename_size = strlen(filename_buf);
        struct dirent **entlist  = NULL;
        struct stat   *statlist  = NULL;

        int ret = 0;

        int (*filter)(const struct dirent*) = NULL;
        if (!args->all) {
                filter = &filter_hidden;
        }

        int n_ent = scandir(filename_buf, &entlist, filter, alphasort);
        if (n_ent == -1) {
                ret = error("%s\n", strerror(errno));
                goto cleanup;
        }

        statlist = (struct stat*) malloc((size_t) n_ent * sizeof(struct stat));
        if (!statlist) {
                ret = error("%s\n", strerror(errno));
                goto cleanup;
        }

        for (int i = 0; i < n_ent; i++) {
                sprintf(filename_buf + filename_size, "/%s", entlist[i]->d_name);
                if (lstat(filename_buf, &statlist[i]) == -1)
                {
                        ret = error("cannot stat %s: %s\n", entlist[i]->d_name, strerror(errno));
                        goto cleanup;
                }
                filename_buf[filename_size] = '\0';
        }

        for (int i = 0; i < n_ent; i++) {
                sprintf(filename_buf + filename_size, "/%s", entlist[i]->d_name);
                ret |= print_file(args, filename_buf, entlist[i]->d_name, &statlist[i]);
                filename_buf[filename_size] = '\0';
        }

        printf("\n");

        if (args->recursive) {
                for (int i = 0; i < n_ent; i++) {
                        if (S_ISDIR(statlist[i].st_mode) &&
                                        (strcmp(entlist[i]->d_name, ".") != 0) &&
                                        (strcmp(entlist[i]->d_name, "..") != 0)) {
                                sprintf(filename_buf + filename_size, "/%s", entlist[i]->d_name);
                                ret |= print_dir(args, filename_buf, &statlist[i]);
                                filename_buf[filename_size] = '\0';
                        }
                }
        }

cleanup:
        if (entlist)
        {
                for (int i = 0; i < n_ent; i++)
                        free(entlist[i]);

                free(entlist);
        }

        if (statlist)
                free(statlist);

        return ret;
}

static int
print_entry(args_t *args, char *filename)
{
        struct stat stats;
        if (lstat(filename, &stats) == -1)
                return error("cannot stat %s: %s\n", filename, strerror(errno));

        if (S_ISDIR(stats.st_mode))
                return print_dir(args, filename, &stats);
        else
                return print_file(args, filename, filename, &stats);

        assert(0 && "unreachable\n");
}

int 
main(int argc, char *argv[])
{
        args_t args = {0}; 
        parse(argc, argv, &args);

        long filename_size = pathconf(".", _PC_PATH_MAX);
        if (filename_size == -1) {
                if (errno)
                    return error("%s\n", strerror(errno));
                else
                    filename_size = FILENAME_MAX;
        }

        char* filename_buf = (char*) malloc((size_t) filename_size * sizeof(char));
        if (!filename_buf)
                return error("allocation failed: %s\n", strerror(errno));

        for (int i = 0; i < args.n_files; i++) {
                sprintf(filename_buf, "%s", args.filename[i]);
                print_entry(&args, filename_buf);
        }

        free(filename_buf);

        return 0;
}

