#!/bin/bash

export HERE="`pwd`"
export TESTLIST="/tmp/libevl-testlist.$$"

cat /dev/null > ${TESTLIST}
ls -1 *.c >> ${TESTLIST}

while read -r line
do
  if [ ! -f ${line} ] ; then
    echo "File ${line} does not exist."
    exit 1
  fi

  if [ ! -f "${line}.orig" ] ; then
    echo "copying ${line} ..."
    cp -fp ${line} "${line}.orig"
  fi

  echo "processing ${line} ..."
  sed -i 's^#include <evl/evl.h>^#include <evl/atomic.h>\n#include <evl/evl.h>\n^g' ${line}
  sed -i 's^#include <evl/clock.h>^#include <evl/clock.h>\n#include <evl/clock-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/mutex.h>^#include <evl/mutex.h>\n#include <evl/mutex-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/observable.h>^#include <evl/observable.h>\n#include <evl/observable-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/poll.h>^#include <evl/poll.h>\n#include <evl/poll-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/proxy.h>^#include <evl/proxy.h>\n#include <evl/proxy-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/sched.h>^#include <evl/sched.h>\n#include <evl/sched-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/syscall.h>^#include <evl/syscall.h>\n#include <evl/syscall-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/thread.h>^#include <evl/thread.h>\n#include <evl/thread-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/timer.h>^#include <evl/timer.h>\n#include <evl/timer-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/xbuf.h>^#include <evl/xbuf.h>\n#include <evl/xbuf-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/poll.h>^#include <evl/poll.h>\n#include <evl/poll-evl.h>\n^g' ${line}
  sed -i 's^#include <evl/net/socket.h>^#include <evl/net/socket.h>\n#include <evl/net/socket-evl.h>\n^g' ${line}
done < ${TESTLIST}

rm -f ${TESTLIST}

