#! /bin/sh

cd $MESON_INSTALL_DESTDIR_PREFIX
if test -d lib64 -a \! -d lib; then
    ln -sf lib64 lib
fi

exit 0
