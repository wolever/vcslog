#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOGLEVEL(level, target_level) (level <= target_level)
#define LOG_DEBUG 0
#define LOG_INFO 10
#define LOG_WARNING 20

#define VERSION "vcslog-wrapper-0.01"

struct opts {
    int o_loglevel;
    const char *o_datadir;
    const char *o_logdir;
    const char *o_session_log;
    FILE *o_session_log_file;
    const char *o_realpath;
    const char *o_execname;
    int o_argc;
    char **o_argv;
};

const char *xgetenv(const char *name) {
    const char *result = getenv(name);
    if (!result) {
        fprintf(stderr, "vcslog-wrapper: getenv(\"%s\") failed\n", name);
        exit(1);
    }
    return result;
}

char *xasprintf(const char *format, ...) {
    va_list ap;
    char *result;
    int ret;
    va_start(ap, format);
    ret = vasprintf(&result, format, ap);
    va_end(ap);
    if (ret < 0) {
        fprintf(stderr, "vcslog-wrapper: asprintf failed\n");
        exit(1);
    }
    return result;
}

char *xstrdup(const char *str) {
    char *result;

    result = strdup(str);
    if (!result) {
        perror("vcslog-wrapper: strdup");
        exit(1);
    }
    return result;
}

void *xmalloc(size_t size) {
    void *result;
    result = malloc(size);
    if (!result) {
        perror("vcslog-wrapper: malloc");
        exit(1);
    }
    return result;
}

void xgettimeofday(struct timeval *tv) {
    int ret = gettimeofday(tv, NULL);
    if (ret < 0) {
        perror("vcslog-wrapper: gettimeofday");
        exit(1);
    }
}

void debug_printopts(struct opts *opts) {
    int arg;
    printf("o_loglevel: %d\n", opts->o_loglevel);
    printf("o_datadir: %s\n", opts->o_datadir);
    printf("o_logdir: %s\n", opts->o_logdir);
    printf("o_argc: %d\n", opts->o_argc);
    printf("o_argv:");
    for (arg = 0; arg < opts->o_argc; arg += 1)
        printf(" %s", opts->o_argv[arg]);
    printf("\n");
    printf("o_realpath: %s\n", opts->o_realpath);
    printf("o_execname: %s\n", opts->o_execname);
    printf("o_session_log: %s\n", opts->o_session_log);
    printf("o_session_log_file: %p\n", opts->o_session_log_file);
}

void log_vcs(struct opts *opts, char *format, ...) {
    FILE *logfile;
    int ret;
    va_list ap;

    logfile = opts->o_session_log_file;
    if (!logfile) {
        logfile = opts->o_session_log_file = fopen(opts->o_session_log, "a");
        if (!logfile) {
            fprintf(stderr, "vcslog-wrapper error: could not open session "
                            "log %s\nDid you run 'vcslog setup'?\n",
                            opts->o_session_log);
            exit(1);
        }
    }

    va_start(ap, format);
    ret = vfprintf(logfile, format, ap);
    if (ret < 0) {
        fprintf(stderr, "vcslog-wrapper: vfprintf failed\n");
        exit(1);
    }
    va_end(ap);
}

char *find_real_executable(struct opts *opts) {
    char *sys_path = xstrdup(xgetenv("PATH"));
    char *_old_sys_path = sys_path;
    char *path_part;
    char tmp_path[PATH_MAX + 1] = "\0";
    char _real_path[PATH_MAX + 1] = "\0";
    char *real_path = NULL;
    int res;

    while ((path_part = strsep(&sys_path, ":")) != NULL) {
        res = snprintf(tmp_path, PATH_MAX, "%s/%s",
                       path_part, opts->o_execname);
        if (res < 0) {
            fprintf(stderr, "vcslog-wrapper: snprintf failed\n");
            exit(1);
        }
        tmp_path[PATH_MAX] = '\0';

        real_path = realpath(tmp_path, _real_path);
        if (!real_path)
            continue;

        if (strcmp(real_path, opts->o_realpath) == 0) {
            real_path = NULL;
            continue;
        }

        break;
    }

    if (!real_path) {
        fprintf(stderr, "vcslog-wrapper: could not find %s in $PATH\n",
                opts->o_execname);
        exit(1);
    }

    free(_old_sys_path);
    return xstrdup(real_path);
}
        
void run_wrapped(struct opts *opts) {
    int arg;

    char *torun;
    torun = find_real_executable(opts);

    struct timeval start;
    xgettimeofday(&start);

    log_vcs(opts, "start: %s %d.%06d\n", VERSION, start.tv_sec, start.tv_usec);
    log_vcs(opts, "cmd: %s", opts->o_execname);
    for (arg = 0; arg < opts->o_argc; arg += 1)
        log_vcs(opts, " %s", opts->o_argv[arg]);
    log_vcs(opts, "\n");

    char **new_argv;
    new_argv = xmalloc(sizeof(char *) * (opts->o_argc + 2));
    new_argv[0] = torun;
    for (arg = 0; arg < opts->o_argc; arg += 1)
        new_argv[arg+1] = opts->o_argv[arg];
    new_argv[arg+1] = NULL;

    int pid;
    if ((pid = fork()) == 0) {
        if (execv(torun, new_argv) < 0) {
            perror("vcslog-wrapper: execv");
            exit(1);
        }
        // we should never get here
    }

    struct rusage rusage;
    int status;
    wait3(&status, 0, &rusage);

    struct timeval end;
    xgettimeofday(&end);
    int exitcode = WEXITSTATUS(status);
    log_vcs(opts, "end: %d.%06d s:%d ut:%d.%06d st:%d.%06d\n",
            end.tv_sec, end.tv_usec,
            exitcode,
            rusage.ru_utime.tv_sec, rusage.ru_utime.tv_usec,
            rusage.ru_stime.tv_sec, rusage.ru_stime.tv_usec);
}

void setup_opts(struct opts *opts, int argc, char **argv) {
    opts->o_loglevel = getenv("VCSLOG_DEBUG")? LOG_DEBUG : LOG_INFO;
    opts->o_datadir = xasprintf("%s/.vcslog", xgetenv("HOME"));
    opts->o_logdir = xasprintf("%s/logs", opts->o_datadir);
    opts->o_execname = xstrdup(basename(argv[0]));
    opts->o_realpath = realpath(argv[0], NULL);
    if (!opts->o_realpath) {
        opts->o_realpath = "";
        opts->o_realpath = find_real_executable(opts);
    }
    opts->o_argc = argc - 1;
    opts->o_argv = argv + 1;
    opts->o_session_log = xasprintf("%s/vcslog-%s-%d", opts->o_logdir, 
                                    opts->o_execname, getpid());
    opts->o_session_log_file = NULL;
}

int main(int argc, char **argv) {
    struct opts opts;
    setup_opts(&opts, argc, argv);
    if (LOGLEVEL(opts.o_loglevel, LOG_DEBUG))
        debug_printopts(&opts);
    run_wrapped(&opts);
    return 0;
}
