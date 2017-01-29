#!/bin/sh

dir=tsan

if [ ! -d "$dir" ]; then
    echo Configuring build in `pwd`/$dir
    mkdir -p $dir
    cd $dir
    CXX=g++-6 ../meson/meson.py ../..
    ../meson/mesonconf.py -Db_sanitize=thread -Ddefault_library=static -Dcpp_link_args=-static-libtsan -Dcpp_args="-D__SANITIZE_THREAD__"
    cd ..
fi

