vcslog logs vcs interactions!
=============================

The title says it all, really... ``vcslog`` logs interactions with version
control systems.

Currently all command-line version control systems are supported.

Overview
--------

There are two parts to ``vcslog``: the main script and the wrapper.  The main
script performs management-type functions, while the wrapper wraps each
invocation of the VCS, logging information such as the command, exit status and
runtime.

Setup
-----

1. Extract/clone the entire ``vcslog`` distribution to ``~/.vcslog`` [*]_ ::

    $ git clone https://github.com/wolever/vcslog.git ~/.vcslog

#. Add ``~/.vcslog/bin/`` to the *front* of ``$PATH`` ::

    $ echo 'export PATH="~/.vcslog/bin/:$PATH' >> ~/.profile

#. Run ``~/.vcslog/vcslog setup`` to build the wrapper script,
   ``vcslog-wrapper``, install the appropriate symlinks, etc. ::

    $ ~/.vcslog/vcslog setup
    building vcslog-wrapper...
    gcc -Wall -g -c xquotestr.c -o bin/xquotestr.o
    gcc -Wall -g bin/xquotestr.o wrapper.c -o bin/vcslog-wrapper
    creating vcs symlinks: hg git svn cvs p4 ok.
    creating vcslog symlink... ok.
    creating log directory... ok.
    checking $PATH... ok.

#. Test it! ::

    $ git stuff
    git: 'stuff' is not a git command. See 'git --help'.
    $ vcslog dump
    cmd,duration,end_time,exit_status,filename,start_time,system_time,user_time,vcslog_version
    git stuff,0.00673794746399,1294479674.55,0,/Users/wolever/.vcslog/logs/vcslog-git-23165,1294479674.55,2080928.20809,1048960.02316,vcslog-wrapper-0.01

.. [*] The ``VCSLOG_HOME`` environment variable can be used to override this
       default.


FAQ
---

Why would I use this?
    For *science*!

    The data collected by ``vcslog`` could help vcs authors learn about how
    people use their systems, address common problems, etc.

    It's also pretty cool.

How can I submit my data?
    Uuhh... Stay tuned! Eventually there will be a simple way to submit your
    data.

