# SPDX-License-Identifier: MIT

cplus_test_programs = [
    'clock',
    'event',
    'flags',
    'heap',
    'init',
    'mutex',
    'observable',
    'poll',
    'proxy',
    'rwlock',
    'sched',
    'sem',
    'socket',
    'thread',
    'timer',
    'xbuf',
]

foreach t : cplus_test_programs
    x = executable(t, t + '.cc',
    cpp_args : [ '-Wno-ignored-qualifiers', '-Wno-missing-field-initializers' ],
    dependencies : libevl_dep
)
endforeach
