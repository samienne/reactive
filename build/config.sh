#!/bin/sh

dir=debug

if [ ! -d "$dir" ]; then
    echo Configuring build in `pwd`/$dir
    mkdir -p $dir
    cd $dir
    CXX=g++-6 ../meson/meson.py ../..
    cd ..
fi

