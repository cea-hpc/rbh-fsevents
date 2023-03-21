#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_rm_same_batch()
{
    local entry="test_entry"
    creat_entry $entry
    rm_entry $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    # The only entry in the database should be the mount point, as the created
    # entry was immediately deleted and should have been ignored
    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(ls -l | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi
}

test_rm_different_batch()
{
    local entry="test_entry"
    creat_entry $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs
    rm_entry $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    # There should be two entries, with one having no links available
    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    if [[ $entries -ne 2 ]]; then
        error "There should be only 2 entries in the database"
    fi

    find_attribute '"ns": { $exists : true }' '"ns": { $size : 0 }'
}
