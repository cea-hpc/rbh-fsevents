# SPDX-License-Identifer: LGPL-3.0-or-later

project(
    'rbh-fsevents',
    'c',
    version: '0.1.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

# GNU extensions
add_project_arguments(['-D_DEFAULT_SOURCE', '-D_GNU_SOURCE',], language: 'c')

# Configuration checks
conf_data = configuration_data()

## Functions
cc = meson.get_compiler('c')
have_statx = cc.has_function('statx', args: '-D_GNU_SOURCE',
                             prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STATX', have_statx)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: conf_data)
add_project_arguments(['-DHAVE_CONFIG_H',], language: 'c')

# Dependencies
extra_sources = []

librobinhood = dependency('robinhood', version: '>=0.1.0')
miniyaml = dependency('miniyaml', version: '>=0.1.0')
liblustre = cc.find_library('lustreapi', disabler: true, required: false)
if liblustre.found()
    extra_sources += 'src/sources/lustre.c'
    add_project_arguments(['-DHAVE_LUSTRE',], language: 'c')
endif

includes = include_directories('.', 'include')

executable(
    'rbh-fsevents',
    sources: [
        'rbh-fsevents.c',
        'src/deduplicator.c',
        'src/enricher.c',
        'src/serialization.c',
        'src/sources/file.c',
        'src/sinks/backend.c',
        'src/sinks/file.c',
    ] + extra_sources,
    include_directories: includes,
    dependencies: [librobinhood, miniyaml, liblustre],
    install: true,
)

specfile = configuration_data()
specfile.set('version', meson.project_version())
configure_file(
    input: 'rbh-fsevents.spec.in',
    output: 'rbh-fsevents.spec',
    configuration: specfile,
)

meson.add_dist_script('utils/add_specfile_to_dist.sh')
custom_target('rpm',
    output: 'rbh-fsevents-' + meson.project_version() + '.rpm',
    command: [
        '../utils/make_rpm.sh', meson.current_build_dir(), meson.project_version()
    ],
)
