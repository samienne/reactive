#!/bin/sh

/bin/sh config.sh || exit 1

cd debug
ninja
cd ..

