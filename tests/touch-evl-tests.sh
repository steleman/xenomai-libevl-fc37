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
  touch -acm ${line}
done < ${TESTLIST}

rm -f ${TESTLIST}

