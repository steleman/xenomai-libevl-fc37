#!/bin/bash

export HERE="`pwd`"
export SRCDIR="${HERE}/xenomai-libevl-fc37"
export BUILDDIR="${HERE}/xenomai-libevl-fc37-build"
export PREFIX="/usr/local"
export UAPI_KERNEL_SRC="6.5.13-300.xenomai.fc37.x86_64"
export UAPI_HEADERS="/usr/src/kernels/${UAPI_KERNEL_SRC}/include/uapi"

if [ ! -f /etc/fedora-release ] ; then
  echo "This is not Fedora."
  exit 1
fi

if [ ! -d ${SRCDIR} ] ; then
  echo "You are in the wrong directory."
  exit 1
fi

if [ -d ${BUILDDIR} ] ; then
  echo "Build directory ${BUILDDIR} already exists."
  echo "Please cleanup and try again."
  exit 1
fi

mkdir -p ${BUILDDIR}
cd ${BUILDDIR}

meson setup \
  -Duapi=${UAPI_HEADERS} \
  -Dbuildtype=release \
  -Dprefix=${PREFIX} \
  ${BUILDDIR} ${SRCDIR}
ret=$?

if [ ${ret} -ne 0 ] ; then
  echo "meson setup step failed."
  exit 1
fi

meson compile
ret=$?

if [ ${ret} -ne 0 ] ; then
  echo "meson compile step failed."
  exit 1
fi

echo "compile OK."

