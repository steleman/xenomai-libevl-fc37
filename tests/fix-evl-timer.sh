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

  echo "processing ${line} ..."
  sed -i 's^#include <evl/timer.h>^^g' ${line}
done < ${TESTLIST}

rm -f ${TESTLIST}

