#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_setxattr()
{
    local entry="test_file"
    touch "$entry"

    update_database

    clear_changelogs
    setfattr -n user.test -v 42 "$entry"

    update_database

    mongo $testdb --eval "db.entries.find()"

    verify_statx $entry
    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"'$entry'"'
}

test_setxattr_remove()
{
    local entry="test_file"
    touch "$entry"

    update_database

    clear_changelogs
    setfattr -n user.test -v 42 "$entry"

    update_database

    verify_statx $entry
    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"test_file"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"test_file"'

    clear_changelogs
    setfattr --remove="user.test" "$entry"

    update_database

    verify_statx $entry
    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"test_file"'
    find_attribute '"xattrs.user.test":{$exists: false}' '"ns.name":"test_file"'
}

test_check_last_setxattr_is_inserted()
{
    local entry="test_file"
    touch "$entry"

    update_database

    clear_changelogs
    setfattr -n user.test -v 42 "$entry"
    setfattr -n user.blob -v 43 "$entry"

    update_database

    verify_statx $entry
    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"test_file"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"test_file"'
    find_attribute '"xattrs.user.blob":{$exists: true}' '"ns.name":"test_file"'
}

test_setxattr_replace()
{
    local entry="test_file"
    touch "$entry"

    update_database

    clear_changelogs
    setfattr -n user.test -v 42 "$entry"

    update_database

    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"test_file"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"test_file"'
    local old_value=$(mongo "$testdb" --eval \
        'db.entries.find({"xattrs.user.test":{$exists: true}},
                         {_id: 0, "xattrs.user.test": 1})')

    clear_changelogs
    setfattr -n user.test -v 43 "test_file"

    update_database

    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"test_file"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"test_file"'
    local new_value=$(mongo "$testdb" --eval \
        'db.entries.find({"xattrs.user.test":{$exists: true}},
                         {_id: 0, "xattrs.user.test": 1})')

    if [ "$old_value" == "$new_value" ]; then
        error "xattrs values should be different after an update"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_setxattr test_setxattr_remove
                  test_check_last_setxattr_is_inserted test_setxattr_replace)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}