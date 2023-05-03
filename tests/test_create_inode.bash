#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_create_entry()
{
    local entry="test_entry"
    create_entry "$entry"

    update_database

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    verify_statx "$entry"
}

test_create_two_entries()
{
    local entry1="test_entry1"
    local entry2="test_entry2"
    create_entry "$entry1"

    update_database

    clear_changelogs
    create_filled_entry "$entry2"

    update_database

    verify_statx "$entry1"
    verify_statx "$entry2"
}
