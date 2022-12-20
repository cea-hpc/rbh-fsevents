#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
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
    touch "test_file"

    # XXX: should we use rbh-fsevents and suppose CREAT/CLOSE are managed, or
    # rbh-sync and suppose it is available on the system ?
    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs
    setfattr -n user.test -v 42 "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.name\":\"test_file\""
    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
    find_attribute "\"xattrs.user\":{\$exists: true}"
    find_attribute "\"xattrs.user.test\":{\$exists: true}"
}

statx()
{
    local format="$1"
    local file="$2"

    local stat=$(stat -c="$format" "$file")
    if [ -z $3 ]; then
        echo "${stat:2}"
    else
        stat=${stat:2}
        echo "ibase=16; ${stat^^}" | bc
    fi
}

test_setxattr_statx()
{
    touch "test_file"

    # XXX: should we use rbh-fsevents and suppose CREAT/CLOSE are managed, or
    # rbh-sync and suppose it is available on the system ?
    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs
    setfattr -n user.test -v 42 "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
    find_attribute "\"statx.ctime.sec\":NumberLong($(statx +%Z 'test_file'))"
    find_attribute "\"statx.ctime.nsec\":0"
}

test_setxattr_remove()
{
    touch "test_file"

    # XXX: should we use rbh-fsevents and suppose CREAT/CLOSE are managed, or
    # rbh-sync and suppose it is available on the system ?
    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs
    setfattr -n user.test -v 42 "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
    find_attribute "\"xattrs.user\":{\$exists: true}"
    find_attribute "\"xattrs.user.test\":{\$exists: true}"

    clear_changelogs
    setfattr --remove="user.test" "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

        lfs changelog lustre-MDT0000
        mongo $testdb --eval "db.entries.find()"
    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
    find_attribute "\"xattrs.user\":{\$exists: true}"
    find_attribute "\"xattrs.user.test\":{\$exists: false}"
}

test_setxattr_multiple()
{
    touch "test_file"

    # XXX: should we use rbh-fsevents and suppose CREAT/CLOSE are managed, or
    # rbh-sync and suppose it is available on the system ?
    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs
    setfattr -n user.test -v 42 "test_file"
    setfattr -n user.blob -v 43 "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.name\":\"test_file\""
    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
    find_attribute "\"xattrs.user\":{\$exists: true}"
    find_attribute "\"xattrs.user.test\":{\$exists: true}"
    find_attribute "\"xattrs.user.blob\":{\$exists: true}"
}

test_setxattr_replace()
{
    touch "test_file"

    # XXX: should we use rbh-fsevents and suppose CREAT/CLOSE are managed, or
    # rbh-sync and suppose it is available on the system ?
    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs
    setfattr -n user.test -v 42 "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"xattrs.user\":{\$exists: true}"
    find_attribute "\"xattrs.user.test\":{\$exists: true}"
    local old_value=$(mongo "$testdb" --eval \
        "db.entries.find({\"xattrs.user.test\":{\$exists: true}},
                         {_id: 0, \"xattrs.user.test\": 1})")

    clear_changelogs
    setfattr -n user.test -v 43 "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"xattrs.user\":{\$exists: true}"
    find_attribute "\"xattrs.user.test\":{\$exists: true}"
    local new_value=$(mongo "$testdb" --eval \
        "db.entries.find({\"xattrs.user.test\":{\$exists: true}},
                         {_id: 0, \"xattrs.user.test\": 1})")

    if [ "$old_value" == "$new_value" ]; then
        error "xattrs values should be different after an update"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_setxattr test_setxattr_statx test_setxattr_remove
                  test_setxattr_multiple test_setxattr_replace)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid=$(lctl --device "$LUSTRE_MDT" changelog_register | cut -d "'" -f2)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
