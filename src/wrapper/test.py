#!/usr/bin/env python
from subprocess import Popen, PIPE, STDOUT

def run_expecting(cmd, expected):
    p = Popen(cmd, stdout=PIPE, stderr=STDOUT)
    actual, _ = p.communicate()
    actual = actual.rstrip("\n")
    print "%s %s:" %(cmd[0], " ".join(map(repr, cmd[1:]))),
    if actual == expected:
        print "ok."
    else:
        print "error: expected %r, got %r" %(expected, actual)

def test_xquotestr():
    expect = lambda i, e: run_expecting(["bin/test-xquotestr", i], e)
    expect("foo", 'foo')
    expect("bar baz", '"bar baz"')
    expect("bar\nbaz", '"bar\\nbaz"')
    expect("\\", '"\\\\"')
    expect("x"*1024, "vcslog-wrapper: xquotestr failed (not enough space in output for xxxxxxxxx...)")

if __name__ == "__main__":
    test_xquotestr()
