#!/bin/bash
set -e

if [[ -z "$OSAF_TEST_WORKDIR" ]]; then
    export OSAF_TEST_WORKDIR=$HOME/osaf_test_workdir
fi

GIT=$(which git)
if [[ -z "$GIT" ]]; then
    echo "*** ERROR: The git command not found"
    echo "           Please run: sudo apt-get install git"
    exit 1
fi
revision=$("$GIT" rev-parse HEAD)

prg=$(basename "$0")
dir=$(dirname "$0"); dir=$(cd "$dir"; pwd)
no_of_processors=$(find /sys/devices/system/cpu -name 'cpu[0-9]*' -print | wc -l)
if [[ "$no_of_processors" -le 0 ]]; then
    no_of_processors=1
fi

test -d "$OSAF_TEST_WORKDIR" || mkdir -p "$OSAF_TEST_WORKDIR"
test -d "$OSAF_TEST_WORKDIR/good_revisions" || mkdir -p "$OSAF_TEST_WORKDIR/good_revisions"

if [[ ! -f "$OSAF_TEST_WORKDIR/googletest/googlemock/lib/libgmock.la" ||
      ! -f "$OSAF_TEST_WORKDIR/googletest/googletest/lib/libgtest.la" ]]; then
    cd "$OSAF_TEST_WORKDIR"

    # remove googletest if it is too old
    if [ ! -f "$OSAF_TEST_WORKDIR/googletest/configure.ac" ]; then
      rm -rf "$OSAF_TEST_WORKDIR/googletest"
    fi

    if [ ! -d "$OSAF_TEST_WORKDIR/googletest" ]; then
      "$GIT" clone https://github.com/google/googletest.git
    fi

    cd "$OSAF_TEST_WORKDIR/googletest"
    git checkout `git tag | grep "release" | tail -n 1`
    autoreconf -vi
    ./configure --with-pthreads
    make -j "$no_of_processors"
fi
export GTEST_DIR="$OSAF_TEST_WORKDIR/googletest/googletest"
export GMOCK_DIR="$OSAF_TEST_WORKDIR/googletest/googlemock"

rm -rf "$OSAF_TEST_WORKDIR/repo" "$OSAF_TEST_WORKDIR/srcdir" "$OSAF_TEST_WORKDIR/objdir"
mkdir "$OSAF_TEST_WORKDIR/repo" "$OSAF_TEST_WORKDIR/srcdir" "$OSAF_TEST_WORKDIR/objdir"
cd "$OSAF_TEST_WORKDIR/repo"
"$GIT" clone "$dir" "$revision"

cd "$revision"
./bootstrap.sh
./configure
make dist
tarball=$(ls -1d opensaf-*.tar.gz)
test -n "$tarball"

cd "$OSAF_TEST_WORKDIR/srcdir"
tar zxf "$OSAF_TEST_WORKDIR/repo/$revision/$tarball"
srcsubdir=$(ls -1d opensaf-*)
test -n "$srcsubdir"

cd "$OSAF_TEST_WORKDIR/objdir"
"$OSAF_TEST_WORKDIR/srcdir/$srcsubdir/configure" CFLAGS="-O3" CXXFLAGS="-O3" --enable-tipc --enable-tests --enable-imm-pbe --enable-ntf-imcn --enable-python
make -j "$no_of_processors"
make -j "$no_of_processors" install DESTDIR="$OSAF_TEST_WORKDIR/objdir/tmpinstall"
make -j "$no_of_processors" check
touch "$OSAF_TEST_WORKDIR/good_revisions/$revision"
echo "Tests PASSED"
exit 0
