#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

creat_entry()
{
    mkdir $1
}

creat_filled_entry()
{
    mkdir -p $1/tmp_test_dir
}

test_creat_dir()
{
    test_creat_entry
}

test_creat_two_dirs()
{
    test_creat_two_entries
}

test_creat_dir_check_statx_attr()
{
    test_creat_entry_check_statx_attr
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_create_inode.bash

declare -a tests=(test_creat_dir test_creat_two_dirs
                  test_creat_dir_check_statx_attr)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
