# This file is part of the RobinHood Library
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = []

liblustre = dependency('lustre', disabler: true, required: false)

if liblustre.found()
    integration_tests += ['test_creat_close']
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e)
endforeach
