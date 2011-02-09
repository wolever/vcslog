import os
import sys
import glob
import logging as log

import argparse

__version__ = "0.02"

def system(cmd):
    ret = os.system(cmd)
    if ret != 0:
        raise Exception("Command failed: %s" %(cmd, ))

class Environment(object):
    def __init__(self, basedir):
        self.basedir = basedir

    def path(self, component, *rest):
        assert component in ("src", "bin", "logs"), \
                "unknown component: %s" %(component, )
        return os.path.join(self.basedir, component, *rest)


def setup_cmd(args, env):
    """ Sets up the environment, making sure all directories and such are
        in place. """

    print "building vcslog-wrapper..."
    wrapper_path = env.path("src", "wrapper")
    if not os.path.exists(wrapper_path):
        print "ERROR: %s missing" %(wrapper_path, )
        print "(all vcslog code must live at $VCSLOG_HOME " \
              "(default: ~/.vcslog/))"
        return 1
    system("cd '%s'; make" %(wrapper_path, ))

    print "creating vcs symlinks:",
    wrapper_path = env.path("src", "wrapper", "bin", "vcslog-wrapper")
    for vcs in ["hg", "git", "svn", "cvs", "p4"]:
        print vcs,
        symlink_path = env.path("bin", vcs)
        if not os.path.exists(symlink_path):
            os.symlink(wrapper_path, symlink_path)
    print "ok."

    print "creating vcslog symlink...",
    symlink_path = env.path("bin", "vcslog")
    if not os.path.exists(symlink_path):
        os.symlink("../vcslog", symlink_path)
    print "ok."

    print "creating log directory...",
    logpath = env.path("logs")
    if not os.path.exists(logpath):
        os.mkdir(logpath)
    print "ok."

    print "checking $PATH...",
    abs_bin_path = os.path.abspath(env.path("bin"))
    for path_part in os.environ["PATH"].split(":"):
        path_part = os.path.expanduser(path_part)
        path_part = os.path.abspath(path_part)
        if path_part == abs_bin_path:
            print "ok."
            break
    else:
        print "WARNING: %s not found in $PATH" %(abs_bin_path, )
        print "Add it to your path then re-run setup to verify."
        return 1

    return 0


def _parse_logfile(lines, filename):
    extra_values = {
        "s": ("exit_status", int),
        "ut": ("user_time", float),
        "st": ("system_time", float),
    }
    _iterlines = enumerate(iter(lines))
    _lastlineno = [-1]

    def nextline(prefix, expect_eof=False):
        lineno, line = next(_iterlines, ("EOF", None))
        _lastlineno[0] = lineno

        if line is None and expect_eof:
            return None

        if line is None or not line.startswith(prefix):
            raise Exception("unexpected line %r at %s:%s"
                            %(line, filename, lineno))

        return line[len(prefix):].strip()

    try:
        while True:
            result = {
                "filename": filename,
            }
            line = nextline("start:", True)
            if line is None:
                break
            version, start_time = line.split()
            result["vcslog_version"] = version
            result["start_time"] = float(start_time)
            result["cmd"] = nextline("cmd:")
            end_time, stuff = nextline("end:").split(None, 1)
            result["end_time"] = float(end_time)
            result["duration"] = result["end_time"] - result["start_time"]
            for (prefix, value) in [ x.split(":") for x in stuff.split() ]:
                result_name, type = extra_values.get(prefix, (None, None))
                if result_name is None:
                    log.warning("ignoring unknown prefix: %r" %(prefix, ))
                    continue
                result[result_name] = type(value)
            yield result
    except Exception, e:
        log.exception("error while processing %s:%s; skipping the rest of file"
                      %(filename, _lastlineno[0]))


class LogEmitter(object):
    def __init__(self, output_stream):
        self.output_stream = output_stream
        self.fields = None

    def set_fields(self, fields):
        self.fields = list(fields)

    def emit(self, log_entry):
        if self.fields is None:
            self.set_fields(sorted(log_entry.keys()))
        self._emit([ log_entry[field] for field in self.fields ])

    def _emit(self, entry_items):
        raise Exception("override this!")


class CVSLogEmitter(LogEmitter):
    def __init__(self, output_stream):
        import csv
        super(CVSLogEmitter, self).__init__(output_stream)
        self.writer = csv.writer(output_stream)

    def set_fields(self, fields):
        super(CVSLogEmitter, self).set_fields(fields)
        self.writer.writerow(self.fields)

    def _emit(self, entry_items):
        self.writer.writerow(entry_items)


def dump_cmd(args, env):
    emitter = CVSLogEmitter(sys.stdout)
    for logfile in glob.iglob(env.path("logs", "*")):
        with open(logfile) as f:
            for entry in _parse_logfile(f, logfile):
                emitter.emit(entry)

def arg_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", action="version",
                        version="vcslog %s" %(__version__, ))
    subparsers = parser.add_subparsers()
    setup = subparsers.add_parser("setup", help="initializes $VCSLOG_HOME, "
                                  "building the wrapper script and installing "
                                  "symlinks.")
    setup.set_defaults(func=setup_cmd)

    dump = subparsers.add_parser("dump", help="dumps existing vcslog data")
    dump.set_defaults(func=dump_cmd)

    return parser

def main():
    parser = arg_parser()
    args = parser.parse_args()
    env_base = os.environ.get("VCSLOG_HOME", os.path.expanduser("~/.vcslog/"))
    env = Environment(env_base)
    return args.func(args, env)
