.. This file is part of rbh-fsevents
   Copyright (C) 2020 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

#######################
Deduplication algorithm
#######################

Context
=======

Filesystems under heavy use can absorb several billion metadata updates in a
single day, sometimes millions in as little as a minute.

Fortunately, most of these updates can be coalesced into a lot less fsevents_.

For example, consider the following program:

.. code:: python

    from os import fdatasync

    def update(path, value):
        with open(path, 'a') as stream:
            stream.write(str(value))
            stream.flush()
            fdatasync(stream.fileno())

    def my_complex_algorithm(step, *args, **kwargs):
        # Some complex algorithm which computes a very important
        # metric you want to commit on disk as soon as possible
        return 0 # TODO

    if __name__ == '__main__':
        for step in range(1 << 30):
            update('my_important_metric', my_complex_algorithm(step))

If run, this program will issue a little over a billion ``open()`` and
``close()`` system calls, which, on a filesystem such as Lustre, will generate
just as many metadata updates.

Of course, the program would much better be written:

.. code:: python

    from os import fdatasync

    def my_complex_algorithm(step, *args, **kwargs):
        # Still some complex algorithm which computes a very important
        # metric you want to commit on disk as soon as possible
        return 0 # TODO

    if __name__ == '__main__':
        with open('my_important_metric', 'w') as stream:
            for step in range(1 << 30):
                stream.write(str(my_complex_algorithm(step)))
                stream.flush()
                fdatasync(stream.fileno())

But sometimes, it is not, and we have to deal with it.

Now, we have a billion filesystem events on our hands, but do we care equally as
much about each of them? Not really. For a system such as RobinHood, seeing all
those events, or just the last one, is just as informative.

Therefore, ``rbh-fsevents`` should be able to coalesce and deduplicate fsevents
on the fly as much as possible.

.. _fsevents: https://github.com/cea-hpc/librobinhood/blob/main/doc/internals.rst#fsevent

Requirements
============

Controlled divergence
---------------------

To be useful, a RobinHood backend should be as up-to-date as possible.

There should be a configurable upper limit to how much ``rbh-fsevents`` can
delay the processing of filesystem events for the sake of deduplicating them.

Catching up
-----------

In some situations, the previously described maximum delay could lead RobinHood
to never catching up with a large backlog of fsevents.

For example, if ``rbh-fsevents`` is not run for a minute, and the configured
maximum delay is set to a few seconds, it will lead to ``rbh-fsevents``
processing fsevents one at a time. This would make it unlikely that
``rbh-fsevents`` ever catches up with the event source.

There should be a configurable lower limit to how many fsevents should at least
be processed in one pass. Of course, if less events are available than this
limit, ``rbh-fsevents`` should go ahead and process the events immediately.

Memory constraints
------------------

There should be a configurable upper limit to how much memory ``rbh-fsevents``
is allowed to use.

Efficiency
----------

As long as every other constraint is met, there should be no limit to how many
fsevents can be deduplicated at once. This allows ``rbh-fsevents`` to be as
efficient as possible.

Acknowledgment
--------------

To ensure no event is lost, most, if not all event sources supported by
``rbh-fsevents`` will provide a transactional mean to acknowledge the processing
of a batch of fsevents.

The deduplication algorithm must allow determining which fsevents have been
processed durably and which fsevents have not.

Solution
========

Deduplicating a batch of fsevents
---------------------------------

This solution makes use of a batch size parameter, given as argument to the
call, to retrieve a number of events from the source equal to that value, then
deduplicate all of them. With this solution, depending on the batch size,
invokations of ``rbh-fsevents`` are required at regular intervals, and it is up
to the users to determine this interval.

This solution ensures a regular processing of the events, so the mirror can be
updated regularly and is more likely to be up-to-date, which checks the
controlled divergence constraint.

However, if that batch size is lower than the amount of events genereated during
a call to ``rbh-fsevents``, it means we may never be able to catch up to the
source, making this constraint hard to satisfy.

Regularly processing fsevents using this batch size also ensures a somewhat
fixed memory usage. While some special cases may occur (generally from extended
attributes utilisation), in general, the memory usage of a call can be
approximated beforehand. Therefore, this solution ensures the memory constrait
is respected.

A specified size to manage batch of events however conflicts with the efficiency
constrait, as only a certain amount of events will be deduplicated at once,
which can be an issue if the last fsevent of a batch and first fsevent of the
following batch concern the same inode. In this case, they will not be
deduplicated properly, which may lead to unnecessary processing of fsevents, and
ultimately, a loss of time.

Pooling the fsevents
--------------------

This solution consists in creating a pool of fsevents that are continually being
deduplicated with new events, and given to the sink when a constraint is met.
This requires the implementation of multiple new arguments corresponding to the
constraints to ensure a regular sink of the events. When one of these constraint
is met, a certain amount of fsevents in the pool (lower than the size of the
pool) may be synced and removed from the pool, starting from the first fsevents
managed or from the oldest one.

To ensure the fsevents are eventually synced, at least one of these arguments
should be given to the call. On the other hand, since we manage a pool of
fsevents that is regularly synced, ``rbh-fsevents`` may run as long as there
fsevents obtained from the source, meaning less monitoring is required from the
user.

Regarding the controlled divergence constraint, it can be valuated by using a
time argument, given to the call to ``rbh-fsevents`` or hard-coded, to ensure
regular syncing of fsevents.

Similarly, to comply with the catching up constraint, an argument corresponding
to the amount of fsevents deduplicated before syncing may be given. It could be
used as the size of the pool of fsevents, and be the main parameter to determine
how many fsevents should be taken out of the pool when it reaches its maximum
size (for instance, we could sync 1/4th of the value).

To the satisfy the memory constraint, another argument could be given to the
call, and it would require an accurate following of the memory usage of the
program, as a value kept accross the deduplication process. When that memory
usage reaches the given limit, a certain amount of fsevents are removed from the
pool, so that the memory usage goes back to under a threshold related to the
given parameter.

For the efficiency constraint, using a pool of fsevents ensures there is no
limit to the number of fsevents managed at the same time, meaning theoretically,
the call to ``rbh-fsevents`` could manage as many events as there are in the
source, as long as a constraint is not satisfied.

Practical implementation concerns
=================================

For these two solutions, there are two pratical implementation issues that
should be kept in mind. For each of these issues, a potential solution is given,
but not elaborated on.

These issues are:
 - fsevents may concern the same inode, but be of different types, which means
 keeping a single fsevent to encompass all fsevents about the same inode is not
 possible, as the meaning of certains fields differ depending on the type. For
 instance, a ``RBH_FET_LINK`` event can only fill in namespace xattrs values,
 and thus cannot be used to sync inode xattrs. For this issue, a solution could
 be to keep a list of fsevents regarding an inode, and fsevents of the same type
 could be grouped into one.
 - fsevents can be deduplicated multiple times per run of ``rbh-fsevents``,
 which mean each deduplication will require multiple memory allocations, which
 can greatly diminish the performances of the deduplication process. To counter
 this problem, a certain amount of ``struct rbh_sstack`` can be used for the
 whole process, and each deduplication could borrow from them to manage
 fsevents.
