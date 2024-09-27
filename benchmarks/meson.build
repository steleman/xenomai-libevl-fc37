# SPDX-License-Identifier: MIT

latmon_incdir = include_directories('zephyr/latmon/src')

executable('latmus',
    'latmus.c',
    install: true,
    include_directories : latmon_incdir,
    dependencies: libevl_dep
)

executable('hectic',
    'hectic.c',
    install: true,
    dependencies: libevl_dep
)
