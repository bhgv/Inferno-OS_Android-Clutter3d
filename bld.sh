#!/bin/sh

source android-env.sh

cd inferno-os
./bld.sh
cd ..

rm -r app/bin app/libs app/obj

make inferno

