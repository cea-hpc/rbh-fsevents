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

test_creat_close_simple()
{
    touch "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    if [[ $entries -ne 1 ]]; then
        error "There should be only one entry in the database"
    fi

    find_attribute "\"ns.name\":\"test_file\""
    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
}

test_creat_close_duo()
{
    touch "test_file1"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file1\""

    lfs changelog_clear "$LUSTRE_MDT" "$userid" 0
    touch "test_file2"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.name\":\"test_file1\""
    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file1\""
    find_attribute "\"ns.name\":\"test_file2\""
    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file2\""
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

test_creat_close_attrs()
{
    echo "blob" > "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.name\":\"test_file\""
    find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
    find_attribute "\"statx.atime.sec\":NumberLong($(statx +%X 'test_file'))"
    find_attribute "\"statx.atime.nsec\":0"
    find_attribute "\"statx.ctime.sec\":NumberLong($(statx +%Z 'test_file'))"
    find_attribute "\"statx.ctime.nsec\":0"
    find_attribute "\"statx.mtime.sec\":NumberLong($(statx +%Y 'test_file'))"
    find_attribute "\"statx.mtime.nsec\":0"
    find_attribute "\"statx.btime.nsec\":0"
    find_attribute "\"statx.blksize\":$(statx +%o 'test_file')"
    find_attribute "\"statx.blocks\":NumberLong($(statx +%b 'test_file'))"
    find_attribute "\"statx.nlink\":$(statx +%h 'test_file')"
    find_attribute "\"statx.ino\":NumberLong(\"$(statx +%i 'test_file')\")"
    find_attribute "\"statx.gid\":$(statx +%g 'test_file')"
    find_attribute "\"statx.uid\":$(statx +%u 'test_file')"
    find_attribute "\"statx.size\":NumberLong($(statx +%s 'test_file'))"

    # TODO: I do not how to retrieve these values: btime (stat doesn't give what
    # is recorded in the db, maybe a bug), device numbers, rdev numbers, mode
    # and type

    #find_attribute "\"statx.btime.sec\":NumberLong($(statx +%W 'test_file'))"
    #find_attribute "\"statx.dev.major\":$(statx +%d 'test_file')"
    #find_attribute "\"statx.dev.minor\":$(statx +%d 'test_file')"
    #find_attribute "\"statx.rdev.major\":$(statx +%d 'test_file')"
    #find_attribute "\"statx.rdev.minor\":$(statx +%d 'test_file')"
    #find_attribute "\"statx.mode\":$(statx +%a 'test_file' hexa)"
    #find_attribute "\"statx.type\":$(statx +%F 'test_file')"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_creat_close_simple test_creat_close_duo
                  test_creat_close_attrs)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid=$(lctl --device "$LUSTRE_MDT" changelog_register | cut -d "'" -f2)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
