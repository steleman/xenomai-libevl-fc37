# SPDX-License-Identifier: MIT

libevl_sources = [
    'clock.c',
    'event.c',
    'flags.c',
    'heap.c',
    'init.c',
    'mutex.c',
    'observable.c',
    'parse_vdso.c',
    'poll.c',
    'proxy.c',
    'rwlock.c',
    'sched.c',
    'sem.c',
    'socket.c',
    'sys.c',
    'syscall.c',
    'thread.c',
    'timer.c',
    'version.c',
    'xbuf.c',
]

stamp_generator = find_program('generate-git-stamp.sh',
    dirs : libevl_scripts,
    required : true
)

gitdir = meson.project_source_root() / '.git'
git_stamp_h = custom_target(
    'git_stamp.h',
    output : 'git_stamp.h',
    build_always_stale : true,
    command : [ stamp_generator, gitdir , '@OUTPUT@' ],
)
libevl_sources += git_stamp_h

libevl = both_libraries('evl', libevl_sources,
     include_directories : [ libevl_incdirs ],
     c_args : [ '-DLIBVERSION="' + libevl_version + '"' ],
     install : true,
     version : libevl_dso_version,
     dependencies : [ pthread_dep ],
)

libevl_dep = declare_dependency(link_with : libevl,
  include_directories : [ libevl_incdirs ],
  dependencies : pthread_dep,
)

pkg = import('pkgconfig')
pkg.generate(libevl,
    description : 'API library and utilities for EVL',
)
