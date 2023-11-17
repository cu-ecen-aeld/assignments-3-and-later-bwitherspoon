#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
TOOLCHAIN_SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    git am ${FINDER_APP_DIR}/0001-scripts-dtc-Remove-redundant-YYLOC-global-declaratio.patch

    make -j ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE mrproper
    make ARCH=$ARCH defconfig
    make -j ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE Image
fi

echo "Adding the Image in outdir"

install ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/{bin,dev,etc,home,lib,lib64,proc,sys,sbin,tmp,var,usr}
mkdir -p ${OUTDIR}/rootfs/{usr/bin,usr/lib,usr/sbin,var/log}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}

else
    cd busybox
fi

# Make and install busybox
make distclean
make defconfig
make -j ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE CONFIG_PREFIX=$OUTDIR/rootfs install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# Add library dependencies to rootfs
cp ${TOOLCHAIN_SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/ld-linux-aarch64.so.1
cp ${TOOLCHAIN_SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/libm.so.6
cp ${TOOLCHAIN_SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/libresolv.so.2
cp ${TOOLCHAIN_SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/libc.so.6

# Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# Clean and build the writer utility
pushd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=$CROSS_COMPILE
popd

# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
install ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home
install ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
install -d ${OUTDIR}/rootfs/home/conf
install -m 644 -t ${OUTDIR}/rootfs/home/conf ${FINDER_APP_DIR}/conf/assignment.txt ${FINDER_APP_DIR}/conf/username.txt
install ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
install ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home

# Create initramfs.cpio.gz
pushd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root | gzip > ${OUTDIR}/initramfs.cpio.gz
popd
