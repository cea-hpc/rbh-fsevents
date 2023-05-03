#!/usr/bin/env bash

# This file is part of rbh-fsevents.
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

################################################################################
#                                  UTILITIES                                   #
################################################################################

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

__rbh_fsevents=$(PATH="$PWD:$PATH" which rbh-fsevents)
rbh_fsevents()
{
    "$__rbh_fsevents" "$@"
}

__mongo=$(which mongosh || which mongo)
mongo()
{
    "$__mongo" --quiet "$@"
}


################################################################################
#                                DATABASE UTILS                                #
################################################################################


update_database()
{
    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"
}

find_attribute()
{
    old_IFS=$IFS
    IFS=','
    local output="$*"
    IFS=$old_IFS
    local res=$(mongo $testdb --eval "db.entries.count({$output})")
    [[ "$res" == "1" ]] && return 0 ||
        error "No entry found with filter '$output'"
}

################################################################################
#                                 LUSTRE UTILS                                 #
################################################################################

hsm_archive_file()
{
    local file="$1"

    lfs hsm_archive "$file"

    while ! lfs hsm_state "$file" | grep "archive_id:"; do
        sleep 0.5
    done
}

hsm_remove_file()
{
    local file="$1"

    lfs hsm_remove "$file"

    while lfs hsm_state "$file" | grep "archived"; do
        sleep 0.5
    done
}

get_hsm_state()
{
    # retrieve the hsm status which is in-between parentheses
    local state=$(lfs hsm_state "$1" | cut -d '(' -f2 | cut -d ')' -f1)
    # the hsm status is written in hexa, so remove the first two characters (0x)
    state=$(echo "${state:2}")
    # bc only understands uppercase hexadecimals, but hsm state only returns
    # lowercase, so we use some bashism to uppercase the whole string, and then
    # convert the hexa to decimal
    echo "ibase=16; ${state^^}" | bc
}

start_changelogs()
{
    userid=$(lctl --device "$LUSTRE_MDT" changelog_register | cut -d "'" -f2)
}

clear_changelogs()
{
    for i in $(seq 1 ${userid:2}); do
        lfs changelog_clear "$LUSTRE_MDT" "cl$i" 0
    done
}

################################################################################
#                                 POSIX UTILS                                  #
################################################################################

mountless_path()
{
    local entry="$1"

    # Get the fullpath of the file, but remove the starting mountpoint as it is
    # not kept in the database (but keep a '/')
    local path="$(realpath $entry)"
    echo "/${path//$LUSTRE_DIR/}"
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
        echo "ibase=$3; ${stat^^}" | bc
    fi
}

verify_statx()
{
    local entry="$1"
    local raw_mode="$(statx +%f "$entry" 16)"
    local type=$((raw_mode & 00170000))
    local mode=$((raw_mode & ~00170000))

    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'$(mountless_path "$entry")'"'
    find_attribute '"statx.atime.sec":NumberLong('$(statx +%X "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.atime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.sec":NumberLong('$(statx +%Z "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.mtime.sec":NumberLong('$(statx +%Y "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.mtime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.btime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.blksize":'$(statx +%o "$entry") \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.blocks":NumberLong('$(statx +%b "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.nlink":'$(statx +%h "$entry") \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ino":NumberLong("'$(statx +%i "$entry")'")' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.gid":'$(statx +%g "$entry") '"ns.name":"'$entry'"'
    find_attribute '"statx.uid":'$(statx +%u "$entry") '"ns.name":"'$entry'"'
    find_attribute '"statx.size":NumberLong('$(statx +%s "$entry")')' \
                   '"ns.name":"'$entry'"'

    find_attribute '"statx.type":'$type '"ns.name":"'$entry'"'
    find_attribute '"statx.mode":'$mode '"ns.name":"'$entry'"'
}

################################################################################
#                                  TEST UTILS                                  #
################################################################################

setup()
{
    # Create a test directory and `cd` into it
    testdir=$PWD/$SUITE-$test
    mkdir "$testdir"
    cd "$testdir"

    # Create test database's name
    testdb=$SUITE-$test
    clear_changelogs
}

teardown()
{
    mongo "$testdb" --eval "db.dropDatabase()" >/dev/null
    rm -rf "$testdir"
    clear_changelogs
}

error()
{
    echo "$*"
    exit 1
}

run_tests()
{
    local fail=0

    for test in "$@"; do
        (set -e; trap -- teardown EXIT; setup; "$test")
        if !(($?)); then
            echo "$test: ✔"
        else
            echo "$test: ✖"
            fail=1
        fi
    done

    return $fail
}