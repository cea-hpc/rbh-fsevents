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

create_entry()
{
    # test_create_two_entries and test_create_entry_check_statx_attr will only
    # check for the symlink itself, and not other entries, so creating two
    # entries here is not an issue
    touch "$1.tmp"
    ln -s "$1.tmp" "$1"
}

create_filled_entry()
{
    echo "test" > "$1.tmp"
    ln -s "$1.tmp" "$1"
}

test_create_symlink()
{
    local entry="test_entry"
    create_entry "$entry"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    find_attribute "\"ns.name\":\"$entry.tmp\""
    find_attribute "\"ns.name\":\"$entry\"" "\"symlink\":\"$entry.tmp\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry\""
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_create_inode.bash

declare -a tests=(test_create_symlink test_create_two_entries
                  test_create_entry_check_statx_attr)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}