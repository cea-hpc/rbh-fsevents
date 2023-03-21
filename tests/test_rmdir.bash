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

rm_entry()
{
    rm -rf $1
}

test_rmdir_same_batch()
{
    test_rm_same_batch
}

test_rmdir_different_batch()
{
    test_rm_different_batch
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_rm_inode.bash

declare -a tests=(test_rmdir_same_batch test_rmdir_different_batch)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
