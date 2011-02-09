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

#include "xquotestr.h"

#define LOGLEVEL(opts, target_level) (opts->o_loglevel <= target_level)
#define LOG_DEBUG 0
#define LOG_INFO 10
#define LOG_WARNING 20

// Log messages prefixed with LOG_NP will not be given the level prefix.
#define LOG_NP "\1"

#define VERSION "vcslog-wrapper-0.02"

struct opts {
    int o_loglevel;
    const char *o_datadir;
    const char *o_logdir;
    const char *o_session_log;
    FILE *o_session_log_file;
    char o_realpath[PATH_MAX+1];
    const char *o_execname;
    int o_argc;
    char **o_argv;
};

void debug(struct opts *opts, const char *format, ...) {
    if (!LOGLEVEL(opts, LOG_DEBUG))
        return;

    va_list ap;
    va_start(ap, format);
    if (format[0] == LOG_NP[0]) {
        format += 1;
    } else {
        fprintf(stderr, "DEBUG: ");
    }
    vfprintf(stderr, format, ap);
    va_end(ap);
}

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
    if (!LOGLEVEL(opts, LOG_DEBUG))
        return;

    int arg;
    debug(opts, "o_loglevel: %d\n", opts->o_loglevel);
    debug(opts, "o_datadir: %s\n", opts->o_datadir);
    debug(opts, "o_logdir: %s\n", opts->o_logdir);
    debug(opts, "o_argc: %d\n", opts->o_argc);
    debug(opts, "o_argv:");
    for (arg = 0; arg < opts->o_argc; arg += 1)
        debug(opts, LOG_NP " %s", opts->o_argv[arg]);
    debug(opts, LOG_NP "\n");
    debug(opts, "o_realpath: %s\n", opts->o_realpath);
    debug(opts, "o_execname: %s\n", opts->o_execname);
    debug(opts, "o_session_log: %s\n", opts->o_session_log);
    debug(opts, "o_session_log_file: %p\n", opts->o_session_log_file);
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

void xfind_real_executable(struct opts *opts, char *output) {
    // 'output' must be at least PATH_MAX + 1
    char *sys_path = xstrdup(xgetenv("PATH"));
    char *_old_sys_path = sys_path;
    char *path_part;
    char tmp_path[PATH_MAX + 1] = "\0";
    char *real_path;
    int res;

    while ((path_part = strsep(&sys_path, ":")) != NULL) {
        res = snprintf(tmp_path, PATH_MAX, "%s/%s",
                       path_part, opts->o_execname);
        if (res < 0) {
            fprintf(stderr, "vcslog-wrapper: snprintf failed\n");
            exit(1);
        }
        tmp_path[PATH_MAX] = '\0';

        real_path = realpath(tmp_path, output);
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
}

void run_wrapped(struct opts *opts) {
    int arg;

    char torun[PATH_MAX + 1];
    xfind_real_executable(opts, torun);

    struct timeval start;
    xgettimeofday(&start);

    char quoted_arg[1024*4];
    log_vcs(opts, "start: %s %d.%06d\n", VERSION, start.tv_sec, start.tv_usec);
    log_vcs(opts, "cmd: %s", xquotestr(opts->o_execname, quoted_arg,
                                       sizeof(quoted_arg)));
    for (arg = 0; arg < opts->o_argc; arg += 1)
        log_vcs(opts, " %s", xquotestr(opts->o_argv[arg], quoted_arg,
                                       sizeof(quoted_arg)));
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
    const char *datadir = getenv("VCSLOG_HOME");
    if (datadir == NULL)
        datadir = xasprintf("%s/.vcslog", xgetenv("HOME"));
    opts->o_datadir = datadir;
    opts->o_logdir = xasprintf("%s/logs", opts->o_datadir);
    opts->o_execname = xstrdup(basename(argv[0]));
    if (!realpath(argv[0], opts->o_realpath)) {
        char temp_path[PATH_MAX + 1];
        xfind_real_executable(opts, temp_path);
        strcpy(opts->o_realpath, temp_path);
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
    debug(&opts, VERSION " starting...\n");
    debug_printopts(&opts);
    run_wrapped(&opts);
    return 0;
}
