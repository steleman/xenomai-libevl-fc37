# SPDX-License-Identifier: MIT

uapi_opt = get_option('uapi')
o_dir = meson.current_build_dir()
uapi_env = environment()
uapi_env.set('ARCH', libevl_arch)
uapi_env.set('UAPI', uapi_opt)
uapi_env.set('OUTPUT_DIR', o_dir)
run_command(libevl_scripts / 'setup-uapi.sh',
    check : true,
    env : uapi_env
)

arch_incdir = '../lib/arch' / libevl_arch / 'include'
libevl_incdirs = include_directories(
    '.',
    arch_incdir,
)

libevl_headers = [
    'evl/atomic.h',
    'evl/clock-evl.h',
    'evl/compat.h',
    'evl/compiler.h',
    'evl/event.h',
    'evl/evl.h',
    'evl/flags.h',
    'evl/heap.h',
    'evl/list.h',
    'evl/mutex-evl.h',
    'evl/observable-evl.h',
    'evl/poll-evl.h',
    'evl/proxy-evl.h',
    'evl/ring_ptr.h',
    'evl/rwlock.h',
    'evl/sched-evl.h',
    'evl/sem.h',
    'evl/syscall-evl.h',
    'evl/sys.h',
    'evl/thread-evl.h',
    'evl/timer-evl.h',
    'evl/xbuf-evl.h',
]

install_headers(
    libevl_headers,
    subdir : 'evl'
)

libevl_net_headers = [
    'evl/net/socket-evl.h',
]

install_headers(
    libevl_net_headers,
    subdir : 'evl/net'
)
