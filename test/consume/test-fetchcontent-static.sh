#!/usr/bin/env bash

here=$(pwd)
thisscript=$(basename $0)
workdir=$here/xdata/$thisscript
argumentumgit="file://$(realpath ../../../argumentum)"
builddir=out/build

if [ -d $workdir ]; then
   rm -rf $workdir
fi

if [ ! -d $workdir ]; then
   mkdir -p $workdir
fi

create_test_dir() {
   cd $here
   cp .gitignore $workdir/
   sed -e "s#@THIS_REPO@#$argumentumgit#" CMakeLists-fetchcontent.txt.in > $workdir/CMakeLists.txt
   cp -r src $workdir/

   cd $workdir
   mkdir -p $builddir
}

configure() {
   cd $workdir
   # local debug=--debug-output
   cmake -S . -B $builddir -D ARGUMENTUM_BUILD_STATIC_LIBS=ON $debug
}

build() {
   cd $workdir
   # local debug=--debug-output
   cmake --build $builddir $debug
}

create_test_dir
configure
build

