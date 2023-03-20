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

source_link="/etc/hosts"

creat_entry()
{
    ln -s $source_link $1
}

creat_filled_entry()
{
    ln -s $source_link $1
}

test_creat_symlink()
{
    test_creat_entry
}

test_creat_two_symlinks()
{
    test_creat_two_entries
}

test_creat_symlink_check_statx_attr()
{
    test_creat_entry_check_statx_attr
}

test_creat_symlink_check_symlink()
{
    local entry="test_entry"
    creat_entry "$entry"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    if [[ $entries -ne 2 ]]; then
        error "There should be only two entries in the database"
    fi

    find_attribute "\"ns.name\":\"$entry\""
    find_attribute "\"symlink\":\"$source_link\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry\""
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_create_inode.bash

declare -a tests=(test_creat_symlink test_creat_two_symlinks
                  test_creat_symlink_check_statx_attr
                  test_creat_symlink_check_symlink)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
