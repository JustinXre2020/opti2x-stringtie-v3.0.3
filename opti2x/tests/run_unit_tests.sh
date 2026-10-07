#!/usr/bin/env bash
# Builds and runs the StringTie unit/differential tests with the release compile flags.
# Usage: run_unit_tests.sh [test_name ...]   (default: all)
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
BUILD=${ST_TEST_BUILD_DIR:-$HERE/build}
CXX=${CXX:-g++}
# htslib build to link against (StringTie's own htslib dir works once built)
HTSLIB=${HTSLIB:-$SRC/htslib}
CXXFLAGS="-O3 -DNDEBUG -I$SRC -I$SRC/gclib -I$HTSLIB -fsigned-char -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE \
-std=c++11 -fno-strict-aliasing -fno-exceptions -fno-rtti"
ALL_TESTS="test_bpcovPrefix_diff test_fgetline_diff test_samThreads_diff"
TESTS=${*:-$ALL_TESTS}
mkdir -p "$BUILD"

# gclib objects the tests link against
for o in GBase GStr GSam; do
  "$CXX" $CXXFLAGS -w -c "$SRC/gclib/$o.cpp" -o "$BUILD/$o.o"
done
HTSLIBS="$HTSLIB/libhts.a $HTSLIB/xlibs/lib/libbz2.a $HTSLIB/xlibs/lib/liblzma.a $HTSLIB/xlibs/lib/libdeflate.a -lz -lm -pthread"

rc=0
for t in $TESTS; do
  "$CXX" $CXXFLAGS "$HERE/$t.cpp" "$BUILD/GSam.o" "$BUILD/GStr.o" "$BUILD/GBase.o" $HTSLIBS -o "$BUILD/$t"
  if "$BUILD/$t"; then echo "PASS $t"; else echo "FAIL $t"; rc=1; fi
done
exit $rc
