#!/bin/sh

dir=ubsan

if [ ! -d "$dir" ]; then
    echo Configuring build in `pwd`/$dir
    mkdir -p $dir
    cd $dir
    CXX=g++-6 ../meson/meson.py ../..
    ../meson/mesonconf.py -Db_sanitize=undefined -Ddefault_library=static -Dcpp_link_args=-static-libubsan
    cd ..
fi

