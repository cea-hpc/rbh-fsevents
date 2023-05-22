#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

mdt_count=$(lfs mdts | wc -l)
if [[ $mdt_count -lt 2 ]]; then
    exit 77
fi

################################################################################
#                                    TESTS                                     #
################################################################################

test_migrate()
{
    local entry="test_entry"
    mkdir $entry
    lfs migrate -m 1 $entry
    lfs migrate -m 0 $entry

    invoke_rbh-fsevents

    lfs changelog lustre-MDT0000
    mongo "$testdb" --eval "db.entries.find()"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    verify_statx $entry

    mongo "$testdb" --eval "db.entries.find()"
    exit 1
    find_attribute '"mdt_idx": [0]' '"mdt_count": 1' '"ns.name":"'$entry'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_migrate)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
