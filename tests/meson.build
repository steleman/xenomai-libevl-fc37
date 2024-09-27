# SPDX-License-Identifier: MIT

libtest = static_library('test_helpers', 'helpers.c',
    include_directories : [ libevl_incdirs ],
)
libtest_dep = declare_dependency(link_with : libtest)


test_programs = [
    'basic-xbuf',
    'clock-timer-periodic',
    'clone-fork-exec',
    'detach-self',
    'duplicate-element',
    'element-visibility',
    'fpu-preload',
    'fpu-stress',
    'heap-torture',
    'mapfd',
    'monitor-deadlock',
    'monitor-deboost-stress',
    'monitor-event',
    'monitor-event-targeted',
    'monitor-event-untrack',
    'monitor-flags',
    'monitor-flags-broadcast',
    'monitor-flags-inband',
    'monitor-pi',
    'monitor-pi-deadlock',
    'monitor-pi-deboost',
    'monitor-pi-stress',
    'monitor-pp-dynamic',
    'monitor-pp-lazy',
    'monitor-pp-lower',
    'monitor-pp-nested',
    'monitor-pp-pi',
    'monitor-pp-raise',
    'monitor-pp-tryenter',
    'monitor-pp-weak',
    'monitor-steal',
    'monitor-trylock',
    'monitor-wait-multiple',
    'monitor-wait-requeue',
    'observable-hm',
    'observable-inband',
    'observable-onchange',
    'observable-oob',
    'observable-race',
    'observable-thread',
    'observable-unicast',
    'poll-close',
    'poll-flags',
    'poll-many',
    'poll-multiple',
    'poll-nested',
    'poll-observable-inband',
    'poll-observable-oob',
    'poll-sem',
    'poll-xbuf',
    'proxy-echo',
    'proxy-eventfd',
    'proxy-pipe',
    'proxy-poll',
    'rwlock-read',
    'rwlock-write',
    'sched-quota-accuracy',
    'sched-tp-accuracy',
    'sched-tp-overrun',
    'sem-close-unblock',
    'sem-flush',
    'sem-timedwait',
    'sem-wait',
    'simple-clone',
    'stax-lock',
    'stax-warn',
    'thread-mode-bits',
]

foreach t : test_programs
    x = executable(t, t + '.c',
    install : true,
    install_dir : 'tests',
    dependencies : [ libtest_dep, libevl_dep ]
)
endforeach

test_programs_with_m = [
    'fault',
]

m_dep = cc.find_library('m', required : true)
foreach t : test_programs_with_m
    x = executable(t, t + '.c',
    c_args : '-Wno-array-bounds',
    install : true,
    install_dir : 'tests',
    dependencies : [ libtest_dep, libevl_dep, m_dep ]
)
endforeach

test_programs_with_atomic = [
    'ring-spray',
]

atomic_dep = cc.find_library('atomic', required : true)
foreach t : test_programs_with_atomic
    x = executable(t, t + '.c',
    install : true,
    install_dir : 'tests',
    dependencies : [ libtest_dep, libevl_dep, atomic_dep ]
)
endforeach

subdir('compile-tests')
