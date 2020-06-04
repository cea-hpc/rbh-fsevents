# This file is part of rbh-changelog
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Quentin Bouget <quentin.bouget@cea.fr>

project(
    'rbh-changelog',
    'c',
    version: '0.0.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

# GNU extensions
add_project_arguments(['-D_GNU_SOURCE',], language: 'c')

librobinhood = dependency('robinhood', version: '>=0.0.0')

includes = include_directories('.', 'include')

executable(
    'rbh-fsevents',
    sources: [
        'rbh-fsevents.c',
        'src/readers/stdin.c',
    ],
    include_directories: includes,
    dependencies: [librobinhood],
    install: true,
)
