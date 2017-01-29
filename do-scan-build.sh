#!/bin/sh
rm -rf build-scan-build
mkdir build-scan-build
cd build-scan-build

scan-build meson ..
scan-build ninja

