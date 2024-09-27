#!/bin/bash

for file in \
  clock.cc \
  event.cc \
  flags.cc \
  heap.cc \
  init.cc \
  mutex.cc \
  observable.cc \
  poll.cc \
  proxy.cc \
  rwlock.cc \
  sched.cc \
  sem.cc \
  socket.cc \
  thread.cc \
  timer.cc \
  xbuf.cc
do
  if [ ! -f "${file}.orig" ] ; then
    cp -fp ${file} "${file}.orig"
  fi
done

