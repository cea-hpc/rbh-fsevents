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

# Remove this line when the Lustre enricher is available
exit 77

################################################################################
#                                    TESTS                                     #
################################################################################

test_migrate()
{
    local entry="test_entry"
    lfs setdirstripe -i 1 $entry
    lfs migrate -m 0 $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    #TODO: verify the MDT is the correct one when the Lustre enricher is added
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
