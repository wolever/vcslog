import os

import argparse

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

    def setup(self):
        """ Sets up the environment, making sure all directories and such are
            in place. """

        print "building vcslog-wrapper..."
        wrapper_path = self.path("src", "wrapper")
        if not os.path.exists(wrapper_path):
            print "ERROR: %s missing" %(wrapper_path, )
            print "(for now, all the vcslog code must live at ~/.vcslog/)"
            return 1
        system("cd '%s'; make" %(wrapper_path, ))

        print "creating vcs symlinks:",
        for vcs in ["hg", "git", "svn", "cvs", "p4"]:
            print vcs,
            symlink_path = self.path("bin", vcs)
            if not os.path.exists(symlink_path):
                os.symlink("vcslog-wrapper", symlink_path)
        print "ok."

        print "creating log directory...",
        logpath = self.path("logs")
        if not os.path.exists(logpath):
            os.mkdir(logpath)
        print "ok."

        print "checking $PATH...",
        abs_bin_path = os.path.abspath(self.path("bin"))
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


def setup(env):
    return env.setup()


def main():
    env_base = os.environ.get("VCSLOG_HOME", os.path.expanduser("~/.vcslog/"))
    env = Environment(env_base)
    return setup(env)
