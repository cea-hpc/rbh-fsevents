#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if ! lctl get_param mdt.*.hsm_control | grep "enabled"; then
    echo "At least 1 MDT needs to have HSM control enabled" >&2
    exit 77
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

# There is no test for releases and restores because these events both trigger a
# layout changelog, and release also trigger a truncate changelog, so this test
# will have to be enriched when these two types of changelogs will be managed

test_hsm()
{
    local entry="$1"
    local state="$2"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    # Since an archived copy of $entry still exists, the DB should contain two
    # entries
    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    local id=$(lfs hsm_state "$entry" | cut -d ':' -f3)
    find_attribute '"ns.name":"'$entry'"' \
                   '"ns.xattrs.hsm_archive_id":'$id \
                   '"ns.xattrs.hsm_state":'$(get_hsm_state "$entry")
}

test_hsm_archive()
{
    local entry="test_entry"
    touch $entry
    hsm_archive_file $entry

    test_hsm $entry "released"
}

test_hsm_remove()
{
    local entry="test_entry"
    touch $entry
    hsm_archive_file $entry
    hsm_remove_file $entry

    test_hsm $entry "none"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_hsm_archive test_hsm_remove)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
