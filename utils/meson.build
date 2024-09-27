# SPDX-License-Identifier: MIT

executable('evl',
    'evl.c',
    install: true,
    dependencies: libevl_dep
)

executable('evl-ps',
    'evl-ps.c',
    include_directories : libevl_incdirs,
    install: true,
    install_dir : libexec_evl,
)

executable('evl-check',
    'evl-check.c',
    install: true,
    install_dir : libexec_evl,
)

helper_scripts = [
	'evl-gdb',
	'evl-help',
	'evl-start',
	'evl-stop',
	'evl-test',
	'evl-trace',
	'trace.evl',
	'trace.irq',
	'trace.timer',
	'kconf-checklist.evl',
]

install_data(helper_scripts,
	install_dir : libexec_evl,
	install_mode : 'r-xr-x---'
)
