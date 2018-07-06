# Copyright © 2012, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU Lesser General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin St 
# - Fifth Floor, Boston, MA 02110-1301 USA


set -ex

# download Android NDK and create standalone toolchain
#if ! test -d android-ndk-r8c; then
#  wget http://dl.google.com/android/ndk/android-ndk-r8c-linux-x86.tar.bz2
#  tar jxf android-ndk-r8c-linux-x86.tar.bz2
#  rm android-ndk-r8c-linux-x86.tar.bz2
#  pushd android-ndk-r8c

if ! test -d android-ndk-r10e; then
  wget http://dl.google.com/android/ndk/android-ndk-r10e-linux-x86.bin
  7z x android-ndk-r10e-linux-x86.bin
#  rm android-ndk-r10d-linux-x86.tar.bz2
  pushd android-ndk-r10e
  ./build/tools/make-standalone-toolchain.sh --arch=arm --platform=android-14 --install-dir=../toolchain
#  ./build/tools/make-standalone-toolchain.sh --arch=arm --platform=android-14 --toolchain=arm-linux-androideabi-4.9 --install-dir=../toolchain
  popd
fi

JHB_PREFIX=$PWD/jhbuild/.local

# set up patched version of jhbuild
if ! test -d jhbuild; then
  git clone git://git.gnome.org/jhbuild
  pushd jhbuild
#  patch -p1 -i ../modulesets/patches/jhbuild/disable-clean-la-files.patch
  ./autogen.sh --prefix=$JHB_PREFIX
  make
  make install
  popd
fi

mkdir -p $JHB_PREFIX/share/aclocal
cp /usr/share/aclocal/gtk-doc.m4 $JHB_PREFIX/share/aclocal/

source ./android-env.sh

# start the build
$JHB_PREFIX/bin/jhbuild -f jhbuildrc-android build -q mx libsoup

# download Android SDK
if ! test -d android-sdk-linux; then
  wget http://dl.google.com/android/android-sdk_r21-linux.tgz
  tar zxf android-sdk_r21-linux.tgz
  rm android-sdk_r21-linux.tgz
fi

echo $PATH

#android update sdk -u --filter platform-tool,tool
#android update sdk --no-ui --all --filter build-tools-19.1.0
##android update sdk --no-ui --all --filter build-tools-26.0.2
#android update sdk -u --filter android-14
android update sdk -u --filter android-17
##android list sdk
##android update sdk -a -u -t 1
