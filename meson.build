# SPDX-License-Identifier: MIT

project('libevl', [ 'c', 'cpp' ],
  version : '0.47',
  license : 'MIT',
  meson_version : '>= 0.58.0',
  default_options : [ 'warning_level=3',
  		      'werror=true',
                      'c_std=gnu11',
                      'cpp_std=gnu++17',
                      'buildtype=debugoptimized' ])

libevl_scripts = meson.project_source_root() / 'meson'
libevl_version = meson.project_version()

cc = meson.get_compiler('c')
common_flags = [
  '-pipe',
  '-pthread',
  '-Wstrict-aliasing',
  '-Wno-unused-parameter',
  '-Wno-pedantic',
  '-D_GNU_SOURCE',
  '-U_FORTIFY_SOURCE',
]

if not get_option('buildtype').startswith('debug')
   common_flags += '-D_FORTIFY_SOURCE=2'
endif

# Figure out whether the compiler supports -Wshadow=local (GCC >= 7.x)
if cc.compiles('void foo() { }', args : '-Wshadow=local', name : 'shadow')
  common_flags += '-Wshadow=local'
endif

cc_flags = common_flags + [
  '-Wstrict-prototypes',
  '-Wmissing-prototypes',
]
add_project_arguments(cc.get_supported_arguments(cc_flags), language: 'c')

cxx = meson.get_compiler('cpp')
cxx_flags = common_flags

add_project_arguments(cxx.get_supported_arguments(cxx_flags), language: 'cpp')

pthread_dep = dependency('threads')

# Do some architecture name remapping
libevl_arch = host_machine.cpu_family()
if libevl_arch == 'x86_64'
   libevl_arch = 'x86'
endif

if libevl_arch == 'aarch64'
   libevl_arch = 'arm64'
endif

post_install = find_program('post-install.sh',
    dirs : libevl_scripts,
    required : true
)

# DSO version should NOT follow the semantic versioning scheme
# otherwise used for the package. It should be of the form
# <current>.<revision>.<age>, following libtool scheme:
#
# increment <current>, set <revision> to 0, if any interfaces have been added, removed, or changed.
# increment <revision> if the code changed since the last release.
# increment <age> if any interfaces have been added only.
# set <age> to 0 if any interfaces have been removed or changed.
#
# <age> must be less than or equal to <current>

libevl_dso_version = '4.0.0'

libexec_evl = get_option('libexecdir') / 'evl'

meson.add_install_script(post_install)

subdir('include')
subdir('lib')
subdir('benchmarks')
subdir('utils')
subdir('tests')
subdir('tidbits')
