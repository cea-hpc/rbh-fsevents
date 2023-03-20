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
    touch $1.tmp
    ln $1.tmp $1
}

creat_filled_entry()
{
    echo "test" > $1.tmp
    ln $1.tmp $1
}

test_creat_hardlink()
{
    local entry="test_entry"
    touch $entry.tmp
    ln $entry.tmp $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(ls -l | wc -l)
    count=$((count - 1))
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    find_attribute "\"ns.name\":\"$entry.tmp\"" "\"ns.name\":\"$entry\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry.tmp\"" \
    #                "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry\""
}

test_creat_two_hardlinks()
{
    test_creat_two_entries
}

test_creat_hardlink_check_statx_attr()
{
    test_creat_entry_check_statx_attr
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_create_inode.bash

declare -a tests=(test_creat_hardlink test_creat_two_hardlinks
                  test_creat_hardlink_check_statx_attr)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
