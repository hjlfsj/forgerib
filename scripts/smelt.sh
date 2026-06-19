#!/bin/bash
./build/bin/crush_vme $1 -c config_14O.toml
#./build/bin/sift -x $1 -v $1
./build/bin/coke -x $(($1+1)) -v $1 -c config_14O.toml
./build/bin/smelt -x $(($1+1)) -v $1 -t t1 -c config_14O.toml t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac tafd tafcsi t1csiu t1csid t1du t1dd t1sd t1su beam
#./build/bin/smelt -x $1 -v $1 -t main t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac beam



