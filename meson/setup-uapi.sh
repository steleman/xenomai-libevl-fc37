#! /bin/sh

# Expand anything in the UAPI path meson did not (e.g. ~).
UAPI=$(eval echo $UAPI)

link_dir() {
    if test \! -d $1; then
	echo "meson: path given to -Duapi does not look right ($1 is missing)"
	exit 1
    fi
    mkdir -p $OUTPUT_DIR/$2
    ln -sf $1 $OUTPUT_DIR/$2
}

if [ -r $UAPI/Kbuild ] ; then
    if [ -f /etc/fedora-release ] ; then
      link_dir $UAPI/evl .
      link_dir $UAPI/asm-generic .
    else
      link_dir $UAPI/include/uapi/evl .
      link_dir $UAPI/arch/$ARCH/include/uapi/asm/evl asm
      link_dir $UAPI/include/uapi/asm-generic .
    fi
else
    if [ -f /etc/fedora-release ] ; then
      link_dir $UAPI/evl .
      link_dir $UAPI/asm-generic .
    else
      DEB_HOST_ARCH=$ARCH
      if [ "$DEB_HOST_ARCH" = "x86" ]; then
          DEB_HOST_ARCH=amd64
      fi
      DEB_HOST_MULTIARCH=$( \
          dpkg-architecture -q DEB_HOST_MULTIARCH -a $DEB_HOST_ARCH 2>/dev/null)
      link_dir $UAPI/evl .
      if [ -d $UAPI/$DEB_HOST_MULTIARCH/asm/evl ]; then
          link_dir $UAPI/$DEB_HOST_MULTIARCH/asm/evl asm
      else
          link_dir $UAPI/asm/evl asm
      fi
      link_dir $UAPI/asm-generic .
    fi
fi
